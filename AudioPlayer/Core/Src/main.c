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

void StartDisplayTask(void *argument);
void StartButtonTask(void *argument);
void StartVolumeTask(void *argument);
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
    if (xTaskCreate(StartDisplayTask, "DisplayTask", 512,
                    NULL, 1, &DisplayTaskHandle) != pdPASS)
    {
        Error_Handler();
    }

    if (xTaskCreate(StartButtonTask, "ButtonTask", 256,
                    NULL, 1, &ButtonTaskHandle) != pdPASS)
    {
        Error_Handler();
    }

    if (xTaskCreate(StartVolumeTask, "VolumeTask", 256,
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

/* DisplayTask alone writes to the LCD and starts confirmed melodies. */
static void lcd_line(uint16_t y, const char *text)
{
    char line[27];
    memset(line, ' ', 26);
    line[26] = 0;
    size_t length = strlen(text);
    if (length > 26) length = 26;
    memcpy(line, text, length);
    SparkLCD_Text(16, y, line, 0xFFFF, 0x0000);
}
void StartDisplayTask(void *argument)
{
    (void)argument;
    const char message[] =
    "Hold B2-B4 for binary song 1-8, then press B1.\r\n"
    "Release all buttons; press B1 within 5 seconds to confirm.\r\n"
    "Built-in UP button: pause/resume.\r\n"
    "Potentiometer controls playback volume.\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)message, sizeof(message)-1, 100);
    uint32_t handledRequest = 0;
    char oldLines[7][27] = {{0}};
    const uint16_t rows[7] = {24, 56, 80, 104, 128, 160, 192};
    for (;;)
    {
        if (handledRequest != playRequest)
        {
            handledRequest = playRequest;
            audioTestResult = SparkAudio_PlaySong(currentSong);
        }
        uint8_t running = SparkAudio_IsPlaying();
        uint8_t failed = audioTestResult != HAL_OK || SparkAudio_HasError();
        SparkSong song;
        SparkSongs_Get(currentSong, &song);
        char lines[7][27] = {{0}};
        snprintf(lines[0], 27, "STM32 AUDIO PLAYER");
        snprintf(lines[1], 27, "%s", codecDetected ? "Codec: detected" : "Codec: not detected");
        snprintf(lines[2], 27, "Song %u: %.17s", (unsigned)currentSong+1, song.title);
        snprintf(lines[3], 27, "%.26s", song.subtitle);
        snprintf(
    lines[4], 27, "Status: %s",
    failed ? "Audio error" :
    selectionActive ? "Confirm choice" :
    SparkAudio_IsPaused() ? "Paused" :
    running ? "Playing" : "Ready"
);
        snprintf(lines[5], 27, "Volume: %3u%%", (unsigned)volumePercent);
        if (selectionActive)
            snprintf(lines[6], 27, "Choice: %u - press B1", (unsigned)pendingSong+1);
        if (xSemaphoreTake(lcdMutexHandle, portMAX_DELAY) == pdTRUE)
        {
            for (unsigned i=0; i<7; ++i)
                if (strcmp(lines[i], oldLines[i]))
                {
                    lcd_line(rows[i], lines[i]);
                    strcpy(oldLines[i], lines[i]);
                }
            xSemaphoreGive(lcdMutexHandle);
        }
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_5,
            !selectionActive && !running ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6,
            selectionActive ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7,
            !selectionActive && running ? GPIO_PIN_SET : GPIO_PIN_RESET);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void StartButtonTask(void *argument)
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

void StartVolumeTask(void *argument)
{
    /* USER CODE BEGIN StartVolumeTask */
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
    /* USER CODE END StartVolumeTask */
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* USER CODE BEGIN Callback 0 */
    if (htim->Instance == TIM6)
    {
        HAL_IncTick();
    }
    /* USER CODE END Callback 0 */
}

void vApplicationIdleHook(void)
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