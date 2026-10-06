#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* USER CODE BEGIN Includes */
#include "spark_lcd.h"
#include "spark_audio.h"
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

static uint8_t currentSong = 0;
static uint8_t pendingSong = 0;
static volatile uint8_t selectionActive = 0;
static TickType_t selectionStartTick = 0;
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
    audioTestResult = SparkAudio_TestTone(&hi2c2);
}
/* USER CODE END 2 */

    lcdMutexHandle = xSemaphoreCreateMutex();

    if (lcdMutexHandle == NULL)
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

void StartDisplayTask(void *argument)
{
    /* USER CODE BEGIN 5 */
    (void)argument;

    const char message[] =
        "AudioPlayer: ready\r\n"
        "B1: enter selection / confirm\r\n"
        "Hold B2-B4 for binary song choice 1-8.\r\n"
        "Confirm within 5 seconds.\r\n"
        "Potentiometer: volume setting.\r\n";

    if (xSemaphoreTake(lcdMutexHandle, portMAX_DELAY) == pdTRUE)
    {
        SparkLCD_Fill(0x0000);

        SparkLCD_Text(16, 24, "STM32 AUDIO PLAYER",
                      0xFFFF, 0x0000);

        SparkLCD_Text(
            16, 64,
            codecDetected ? "Codec: detected" : "Codec: not detected",
            codecDetected ? 0x07E0 : 0xF800,
            0x0000);

        SparkLCD_Text(
            16, 88,
            audioTestResult == HAL_OK
                ? "Audio test: sent"
                : "Audio test: failed",
            audioTestResult == HAL_OK ? 0x07E0 : 0xF800,
            0x0000);

        SparkLCD_Text(16, 104, "Song: 1",
                      0xFFFF, 0x0000);

        SparkLCD_Text(16, 136, "Status: Ready",
                      0xFFFF, 0x0000);

        xSemaphoreGive(lcdMutexHandle);
    }

    HAL_UART_Transmit(&huart1,
                      (uint8_t *)message,
                      sizeof(message) - 1,
                      100);

    for (;;)
    {
        uint8_t selecting = selectionActive;

        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_5,
                          selecting ? GPIO_PIN_RESET : GPIO_PIN_SET);

        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6,
                          selecting ? GPIO_PIN_SET : GPIO_PIN_RESET);

        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7, GPIO_PIN_RESET);

        vTaskDelay(pdMS_TO_TICKS(20));
    }
    /* USER CODE END 5 */
}

void StartButtonTask(void *argument)
{
    /* USER CODE BEGIN StartButtonTask */
    (void)argument;

    uint8_t lastRaw = 0;
    uint8_t stableButtons = 0;
    uint8_t displayChanged = 1;

    TickType_t lastChangeTick = xTaskGetTickCount();

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();
        uint8_t rawButtons = 0;
        uint32_t inputs = GPIOE->IDR;

        /* Active-low buttons: pressed = 1. */
        for (uint8_t i = 0; i < 4; i++)
        {
            if ((inputs & (GPIO_PIN_2 << i)) == 0)
            {
                rawButtons |= (uint8_t)(1U << i);
            }
        }

        if (rawButtons != lastRaw)
        {
            lastRaw = rawButtons;
            lastChangeTick = now;
        }

        /* Cancel an unconfirmed choice after five seconds. */
        if (selectionActive &&
            (TickType_t)(now - selectionStartTick) >=
                pdMS_TO_TICKS(5000))
        {
            selectionActive = 0;
            pendingSong = currentSong;
            displayChanged = 1;
        }

        /* Accept changes after 40 ms of stable input. */
        if (rawButtons != stableButtons &&
            (TickType_t)(now - lastChangeTick) >=
                pdMS_TO_TICKS(40))
        {
            uint8_t newlyPressed =
                (uint8_t)(rawButtons & (uint8_t)~stableButtons);

            stableButtons = rawButtons;

            if (newlyPressed & 0x01U)
            {
                if (!selectionActive)
                {
                    selectionStartTick = now;
                    pendingSong =
                        (uint8_t)((stableButtons >> 1) & 0x07U);
                    selectionActive = 1;
                }
                else
                {
                    pendingSong =
                        (uint8_t)((stableButtons >> 1) & 0x07U);

                    currentSong = pendingSong;
                    selectionActive = 0;
                }

                displayChanged = 1;
            }
            else if (selectionActive)
            {
                pendingSong =
                    (uint8_t)((stableButtons >> 1) & 0x07U);

                displayChanged = 1;
            }
        }

        if (displayChanged)
        {
            char songText[] = "Song: 1             ";
            char choiceText[] = "Choice: 1           ";

            songText[6] = (char)('1' + currentSong);
            choiceText[8] = (char)('1' + pendingSong);

            if (xSemaphoreTake(lcdMutexHandle, portMAX_DELAY) == pdTRUE)
            {
                SparkLCD_Text(16, 104, songText,
                              0xFFFF, 0x0000);

                SparkLCD_Text(
                    16, 136,
                    selectionActive
                        ? "Status: Selecting   "
                        : "Status: Ready       ",
                    0xFFFF, 0x0000);

                SparkLCD_Text(
                    16, 184,
                    selectionActive
                        ? choiceText
                        : "                    ",
                    0xFFFF, 0x0000);

                xSemaphoreGive(lcdMutexHandle);
            }

            displayChanged = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
    /* USER CODE END StartButtonTask */
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
                    char text[] = "Volume:   0%";

                    text[8] = percent >= 100U ? '1' : ' ';
                    text[9] = percent >= 10U
                              ? (char)('0' + (percent / 10U) % 10U)
                              : ' ';
                    text[10] = (char)('0' + percent % 10U);

                    if (xSemaphoreTake(lcdMutexHandle,
                                       portMAX_DELAY) == pdTRUE)
                    {
                        SparkLCD_Text(16, 160, text,
                                      0xFFFF, 0x0000);

                        xSemaphoreGive(lcdMutexHandle);
                    }

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