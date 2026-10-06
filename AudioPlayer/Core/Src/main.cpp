#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* USER CODE BEGIN Includes */
#include "spark_lcd.h"
#include "spark_audio.h"
#include "spark_songs.h"
#include <stdio.h>
#include <string.h>
#include "timers.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
static HAL_StatusTypeDef audioTestResult = HAL_ERROR;
I2C_HandleTypeDef hi2c2;
static uint8_t codecDetected = 0;
UART_HandleTypeDef huart1;
ADC_HandleTypeDef hadc1;

TaskHandle_t DisplayTaskHandle;
TaskHandle_t ButtonTaskHandle;
TaskHandle_t VolumeTaskHandle;
SemaphoreHandle_t lcdMutexHandle;

static volatile uint32_t playRequest = 0;
static volatile uint8_t volumePercent = 50;
static uint8_t currentSong = 0;
static uint8_t pendingSong = 0;
static volatile uint8_t selectionActive = 0;
static TickType_t selectionStartTick = 0;
static TimerHandle_t selectionTimer = NULL;
/* USER CODE END PV */

/* USER CODE BEGIN PFP */
void SystemClock_Config(void);
static void MX_I2C2_Init(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ADC1_Init(void);

void update_lcd_leds_thread(void *argument);
void polling_buttons(void *argument);
void adjust_volume(void *argument);
/* USER CODE END PFP */

static void SelectionTimeoutCallback(TimerHandle_t timer)
{
    (void)timer;

    /* The time check prevents an older timeout from
       cancelling a newly opened selection window. */
    if (selectionActive &&
        (TickType_t)(xTaskGetTickCount() - selectionStartTick)
            >= pdMS_TO_TICKS(5000))
    {
        selectionActive = 0;
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();

   /* USER CODE BEGIN 2 */
MX_USART1_UART_Init();
MX_ADC1_Init();
MX_I2C2_Init();

SparkLCD_Init();
SparkLCD_Fill(0x0000);

/* HAL expects the 7-bit address shifted left once. */
codecDetected =
    (HAL_I2C_IsDeviceReady(&hi2c2, (0x10U << 1), 3, 100)
     == HAL_OK);

     if (codecDetected)
{
    audioTestResult = SparkAudio_Init(&hi2c2);
}
/* USER CODE END 2 */

    lcdMutexHandle = xSemaphoreCreateMutex();

    if (lcdMutexHandle == NULL)
    {
        Error_Handler();
    }
selectionTimer = xTimerCreate(
    "SelectionTimeout",
    pdMS_TO_TICKS(5000),
    pdFALSE,  /* One-shot timer. */
    NULL,
    SelectionTimeoutCallback
);

if (selectionTimer == NULL)
{
    Error_Handler();
}
    if (xTaskCreate(update_lcd_leds_thread, "DisplayTask", 512,
                    NULL, 1, &DisplayTaskHandle) != pdPASS)
    {
        Error_Handler();
    }

    if (xTaskCreate(polling_buttons, "ButtonTask", 256,
                    NULL, 1, &ButtonTaskHandle) != pdPASS)
    {
        Error_Handler();
    }

    if (xTaskCreate(adjust_volume, "VolumeTask", 256,
                    NULL, 1, &VolumeTaskHandle) != pdPASS)
    {
        Error_Handler();
    }

    vTaskStartScheduler();

    /* The scheduler returns only if it could not start. */
    Error_Handler();

    while (1)
    {
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clocks = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    oscillator.HSIState = RCC_HSI_ON;
    oscillator.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    oscillator.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK)
    {
        Error_Handler();
    }

    clocks.ClockType = RCC_CLOCKTYPE_HCLK |
                       RCC_CLOCKTYPE_SYSCLK |
                       RCC_CLOCKTYPE_PCLK1 |
                       RCC_CLOCKTYPE_PCLK2;

    clocks.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clocks.APB1CLKDivider = RCC_HCLK_DIV1;
    clocks.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    /* USER CODE BEGIN MX_GPIO_Init_2 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /* Buttons: PE2-PE5, with external 10 kOhm pull-ups. */
    GPIO_InitTypeDef buttons = {0};
    buttons.Pin = GPIO_PIN_2 | GPIO_PIN_3 |
                  GPIO_PIN_4 | GPIO_PIN_5;
    buttons.Mode = GPIO_MODE_INPUT;
    buttons.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOE, &buttons);

    /* Built-in UP button: PC5, active-low. */
__HAL_RCC_GPIOC_CLK_ENABLE();

GPIO_InitTypeDef userButton = {0};
userButton.Pin = GPIO_PIN_5;
userButton.Mode = GPIO_MODE_INPUT;
userButton.Pull = GPIO_PULLUP;
HAL_GPIO_Init(GPIOC, &userButton);

    /* Common-cathode RGB: PF5 red, PF6 green, PF7 blue. */
    HAL_GPIO_WritePin(GPIOF,
                      GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
                      GPIO_PIN_RESET);

    GPIO_InitTypeDef leds = {0};
    leds.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    leds.Mode = GPIO_MODE_OUTPUT_PP;
    leds.Pull = GPIO_NOPULL;
    leds.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOF, &leds);
    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void MX_USART1_UART_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_ADC1_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    ADC_ChannelConfTypeDef channel = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    /* Potentiometer wiper: PA0, ADC1 channel 0. */
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfDiscConversion = 0;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    channel.Channel = ADC_CHANNEL_0;
    channel.Rank = 1;
    channel.SamplingTime = ADC_SAMPLETIME_144CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &channel) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_I2C2_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_I2C2_CLK_ENABLE();

    /* Spark-1 codec bus: PF1 SCL, PF0 SDA. */
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_AF_OD;
    gpio.Pull = GPIO_NOPULL; /* Board has external pull-ups. */
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init(GPIOF, &gpio);

    hi2c2.Instance = I2C2;
    hi2c2.Init.ClockSpeed = 100000;
    hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c2) != HAL_OK)
    {
        Error_Handler();
    }
}
/* USER CODE END 4 */

/* RGB565 palette: dark navy, cyan, lavender and warm amber. */
static const uint16_t UI_BG = 0x10A4;
static const uint16_t UI_CARD = 0x1947;
static const uint16_t UI_LINE = 0x294A;
static const uint16_t UI_CYAN = 0x05FF;
static const uint16_t UI_PURPLE = 0xA3FF;
static const uint16_t UI_WHITE = 0xEF7D;
static const uint16_t UI_MUTED = 0x8495;
static const uint16_t UI_AMBER = 0xFD48;
static const uint16_t UI_RED = 0xF9A6;

struct PlayerView {
    uint8_t song, choice, selecting, state, volume, secondsLeft;
    uint32_t position, length, animation;
};

static void ui_line(uint16_t x, uint16_t y, const char *text,
                    uint8_t columns, uint16_t color, uint16_t background)
{
    char line[27];
    if (columns > 26U) columns = 26U;
    memset(line, ' ', columns);
    line[columns] = 0;
    if (text) {
        size_t size = strlen(text);
        if (size > columns) size = columns;
        memcpy(line, text, size);
    }
    SparkLCD_Text(x, y, line, color, background);
}

static void ui_clean_name(char *out, const char *source, size_t capacity)
{
    if (!source) source = "Unknown";
    while (*source == ' ' || *source == '-') ++source;
    snprintf(out, capacity, "%s", source);
    size_t length = strlen(out);
    while (length && (out[length-1] == ' ' || out[length-1] == '-'))
        out[--length] = 0;
}

static void ui_cover(void)
{
    SparkLCD_RoundRect(12, 40, 216, 76, 8, UI_CARD);
    SparkLCD_RoundRect(20, 48, 64, 64, 6, UI_PURPLE);
    /* Music note drawn geometrically; no new font or image assets. */
    SparkLCD_Rect(38, 59, 4, 34, UI_WHITE);
    SparkLCD_Rect(57, 55, 4, 33, UI_WHITE);
    SparkLCD_Rect(40, 55, 21, 5, UI_WHITE);
    SparkLCD_Circle(34, 94, 7, UI_WHITE);
    SparkLCD_Circle(53, 89, 7, UI_WHITE);
    ui_line(100, 48, "NOW PLAYING", 14, UI_MUTED, UI_CARD);
}

static void ui_transport(uint8_t state)
{
    SparkLCD_Circle(32, 201, 16, state == 4 ? UI_RED : UI_CYAN);
    if (state == 1) {
        SparkLCD_Rect(25, 193, 5, 16, UI_BG);
        SparkLCD_Rect(34, 193, 5, 16, UI_BG);
    } else {
        for (int16_t dy = -8; dy <= 8; ++dy) {
            uint16_t width = (uint16_t)(12 - (dy < 0 ? -dy : dy));
            SparkLCD_Rect(28, (uint16_t)(201 + dy), width, 1, UI_BG);
        }
    }
}

static void ui_wave(uint32_t frame, uint8_t state)
{
    static const uint8_t heights[9] = {5, 12, 17, 8, 14, 6, 19, 10, 15};
    SparkLCD_Rect(100, 87, 112, 22, UI_CARD);
    for (uint8_t i = 0; i < 9; ++i) {
        uint8_t height = state == 1 ? heights[(i + frame) % 9U] : 4U;
        SparkLCD_RoundRect(102 + i * 12U, 108 - height, 6, height, 2,
                          i % 2U ? UI_PURPLE : UI_CYAN);
    }
}

static void ui_time(uint32_t milliseconds, char *out, size_t capacity)
{
    uint32_t seconds = milliseconds / 1000U;
    snprintf(out, capacity, "%02lu:%02lu",
             (unsigned long)(seconds / 60U), (unsigned long)(seconds % 60U));
}

static void ui_initialize(void)
{
    SparkLCD_Fill(UI_BG);
    SparkLCD_Text(16, 12, "SPARK", UI_CYAN, UI_BG);
    SparkLCD_Text(72, 12, "PLAYER", UI_WHITE, UI_BG);
    SparkLCD_Rect(16, 33, 208, 1, UI_LINE);
    SparkLCD_Text(60, 188, "VOLUME", UI_MUTED, UI_BG);
    SparkLCD_Text(16, 224, "UP: pause / B1: select", UI_MUTED, UI_BG);
}

static void ui_render(const PlayerView &view)
{
    static uint8_t oldSong = 255, oldChoice = 255, oldSelecting = 255;
    static uint8_t oldState = 255, oldVolume = 255, oldCountdown = 255;
    static uint32_t oldSeconds = UINT32_MAX, oldLength = UINT32_MAX;
    static uint32_t oldBar = UINT32_MAX, oldAnimation = UINT32_MAX;
    char text[27];

    if (view.song != oldSong) {
        SparkSong song;
        if (SparkSongs_Get(view.song, &song)) {
            ui_clean_name(text, song.title, sizeof(text));
            ui_line(16, 120, text, 26, UI_WHITE, UI_BG);
            ui_clean_name(text, song.subtitle, sizeof(text));
            ui_line(16, 139, text, 26, UI_MUTED, UI_BG);
        }
        snprintf(text, sizeof(text), "%02u/08", (unsigned)view.song + 1U);
        ui_line(184, 12, text, 5, UI_CYAN, UI_BG);
        oldSong = view.song;
    }

    bool panelChanged = view.selecting != oldSelecting;
    if (panelChanged) {
        if (view.selecting)
            SparkLCD_RoundRect(12, 40, 216, 76, 8, UI_CARD);
        else ui_cover();
        oldSelecting = view.selecting;
    }
    if (view.selecting) {
        if (panelChanged || view.choice != oldChoice) {
            SparkSong choice;
            snprintf(text, sizeof(text), "CONFIRM SONG %02u", (unsigned)view.choice + 1U);
            ui_line(24, 48, text, 23, UI_AMBER, UI_CARD);
            if (SparkSongs_Get(view.choice, &choice)) {
                ui_clean_name(text, choice.title, sizeof(text));
                ui_line(24, 70, text, 23, UI_WHITE, UI_CARD);
            }
            oldChoice = view.choice;
        }
        if (panelChanged || view.secondsLeft != oldCountdown) {
            snprintf(text, sizeof(text), "B1: confirm        %us", (unsigned)view.secondsLeft);
            ui_line(24, 92, text, 23, UI_AMBER, UI_CARD);
            SparkLCD_Rect(24, 110, 192, 3, UI_LINE);
            SparkLCD_Rect(24, 110, 192U * view.secondsLeft / 5U, 3, UI_AMBER);
            oldCountdown = view.secondsLeft;
        }
    } else {
        if (panelChanged || view.state != oldState) {
            static const char *const labels[] = {"READY", "PLAYING", "PAUSED", "FINISHED", "AUDIO ERROR"};
            ui_line(100, 68, labels[view.state], 14,
                    view.state == 4 ? UI_RED : UI_CYAN, UI_CARD);
        }
        if (panelChanged || view.state != oldState ||
            (view.state == 1 && view.animation != oldAnimation)) {
            ui_wave(view.animation, view.state);
            oldAnimation = view.animation;
        }
    }
    if (view.state != oldState) {
        ui_transport(view.state);
        oldState = view.state;
    }

    uint32_t seconds = view.position / 1000U;
    if (seconds != oldSeconds || view.length != oldLength) {
        ui_time(view.position, text, sizeof(text));
        ui_line(16, 158, text, 6, UI_MUTED, UI_BG);
        ui_time(view.length, text, sizeof(text));
        ui_line(184, 158, text, 5, UI_MUTED, UI_BG);
        oldSeconds = seconds;
        oldLength = view.length;
    }
    uint32_t bar = view.length ? (uint32_t)((uint64_t)view.position * 208U / view.length) : 0;
    if (bar > 208U) bar = 208U;
    if (bar != oldBar) {
        SparkLCD_Rect(16, 177, 208, 4, UI_LINE);
        SparkLCD_Rect(16, 177, (uint16_t)bar, 4, UI_CYAN);
        oldBar = bar;
    }
    if (view.volume != oldVolume) {
        snprintf(text, sizeof(text), "%3u%%", (unsigned)view.volume);
        ui_line(184, 188, text, 5, UI_WHITE, UI_BG);
        SparkLCD_Rect(60, 211, 164, 6, UI_LINE);
        SparkLCD_Rect(60, 211, 164U * view.volume / 100U, 6, UI_CYAN);
        oldVolume = view.volume;
    }
}

void update_lcd_leds_thread(void *argument)
{
    (void)argument;
    const char message[] =
        "Hold B2-B4 for binary song 1-8, then press B1.\r\n"
        "Release all buttons; press B1 within 5 seconds to confirm.\r\n"
        "Built-in UP button: pause/resume.\r\n"
        "Potentiometer controls playback volume.\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)message, sizeof(message)-1, 100);
    uint32_t handledRequest = 0;
    if (xSemaphoreTake(lcdMutexHandle, portMAX_DELAY) == pdTRUE) {
        ui_initialize();
        xSemaphoreGive(lcdMutexHandle);
    }
    for (;;) {
        if (handledRequest != playRequest) {
            handledRequest = playRequest;
            audioTestResult = SparkAudio_PlaySong(currentSong);
        }
        uint8_t running = SparkAudio_IsPlaying();
        uint8_t pausedNow = SparkAudio_IsPaused();
        uint8_t failed = audioTestResult != HAL_OK || SparkAudio_HasError();
        PlayerView view = {};
        view.song = currentSong;
        view.choice = pendingSong;
        view.selecting = selectionActive;
        view.volume = volumePercent;
        SparkAudio_GetPositionMillis(&view.position, &view.length);
        view.state = failed ? 4 : running ? 1 : pausedNow ? 2 :
            (view.length && view.position >= view.length) ? 3 : 0;
        TickType_t now = xTaskGetTickCount();
        TickType_t elapsedTicks = now - selectionStartTick;
        TickType_t limit = pdMS_TO_TICKS(5000);
        view.secondsLeft = elapsedTicks >= limit ? 0 :
            (uint8_t)((limit - elapsedTicks + pdMS_TO_TICKS(1000) - 1U) /
                       pdMS_TO_TICKS(1000));
        view.animation = now / pdMS_TO_TICKS(250);
        if (xSemaphoreTake(lcdMutexHandle, portMAX_DELAY) == pdTRUE) {
            ui_render(view);
            xSemaphoreGive(lcdMutexHandle);
        }
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_5,
            !view.selecting && !running ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6,
            view.selecting ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7,
            !view.selecting && running ? GPIO_PIN_SET : GPIO_PIN_RESET);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void polling_buttons(void *argument)
{
    (void)argument;
    uint8_t lastRaw = 0, stable = 0;
    TickType_t changed = xTaskGetTickCount();
    uint8_t userLastRaw = 0;
uint8_t userStable = 0;
TickType_t userChanged = xTaskGetTickCount();


    for (;;)
    {
        TickType_t now = xTaskGetTickCount();
        uint8_t userRaw =
    HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5) == GPIO_PIN_RESET;

if (userRaw != userLastRaw)
{
    userLastRaw = userRaw;
    userChanged = now;
}

if (userRaw != userStable &&
    (TickType_t)(now - userChanged) >= pdMS_TO_TICKS(40))
{
    userStable = userRaw;

    if (userStable)
    {
        SparkAudio_TogglePause();
    }
}
        uint8_t raw = (uint8_t)((~GPIOE->IDR >> 2) & 15U);
        if (raw != lastRaw) { lastRaw = raw; changed = now; }
        if (selectionActive && now-selectionStartTick >= pdMS_TO_TICKS(5000))
            selectionActive = 0;
        if (raw != stable && now-changed >= pdMS_TO_TICKS(40))
        {
            uint8_t pressed = (uint8_t)(raw & ~stable);
            stable = raw;
            if (pressed & 1U)
            {
                if (!selectionActive)
                {
                    /* Capture once: releasing the binary buttons must
                     * not change the stored selection. */
                    pendingSong = (stable >> 1) & 7U;
                    selectionStartTick = now;
                    selectionActive = 1;

                    if (xTimerReset(selectionTimer, 0) != pdPASS)
{
    selectionActive = 0;
}
                }
                else
                {
                    currentSong = pendingSong;
selectionActive = 0;

if (xTimerStop(selectionTimer, 0) != pdPASS)
{
    /* Any eventual callback is harmless because
       selectionActive is already zero. */
}

++playRequest;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void adjust_volume(void *argument)
{
    /* USER CODE BEGIN adjust_volume */
    (void)argument;

    uint32_t filteredRaw = 0;
    uint32_t displayedPercent = 101;
    uint8_t initialized = 0;

    for (;;)
    {
        if (HAL_ADC_Start(&hadc1) == HAL_OK)
        {
            if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
            {
                uint32_t raw = HAL_ADC_GetValue(&hadc1);

                if (!initialized)
                {
                    filteredRaw = raw;
                    initialized = 1;
                }
                else
                {
                    /* Smooth readings using a moving average. */
                    filteredRaw =
                        (filteredRaw * 7U + raw + 4U) / 8U;
                }

                uint32_t percent =
                    (filteredRaw * 100U + 2047U) / 4095U;

                /* Make the endpoints easier to reach. */
                if (percent <= 1U)
                {
                    percent = 0;
                }
                else if (percent >= 99U)
                {
                    percent = 100;
                }

                uint32_t difference =
                    percent > displayedPercent
                        ? percent - displayedPercent
                        : displayedPercent - percent;

                /* Ignore small fluctuations in the display. */
                if (displayedPercent == 101U ||
                    difference >= 2U ||
                    (percent == 0U && displayedPercent != 0U) ||
                    (percent == 100U && displayedPercent != 100U))
                {
                    volumePercent = (uint8_t)percent;
                    SparkAudio_SetVolume(volumePercent);
                    displayedPercent = percent;
                }
            }

            HAL_ADC_Stop(&hadc1);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
    /* USER CODE END adjust_volume */
}

extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* USER CODE BEGIN Callback 0 */
    if (htim->Instance == TIM6)
    {
        HAL_IncTick();
    }
    /* USER CODE END Callback 0 */
}

extern "C" void vApplicationIdleHook(void)
{
    HAL_PWR_EnterSLEEPMode(
        PWR_MAINREGULATOR_ON,
        PWR_SLEEPENTRY_WFI
    );
}

void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    __disable_irq();

    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    (void)file;
    (void)line;
    /* USER CODE END 6 */
}
#endif