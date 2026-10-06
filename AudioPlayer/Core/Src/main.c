/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "spark_lcd.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* Definitions for DisplayTask */
TaskHandle_t DisplayTaskHandle;
TaskHandle_t ButtonTaskHandle;
TaskHandle_t VolumeTaskHandle;
SemaphoreHandle_t lcdMutexHandle;
/* USER CODE BEGIN PV */

UART_HandleTypeDef huart1;

static uint8_t currentSong = 0;
static uint8_t pendingSong = 0;
static uint8_t selectionActive = 0;
static TickType_t selectionStartTick = 0;

/* USER CODE END PV */
/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void StartDisplayTask(void *argument);
void StartButtonTask(void *argument);
void StartVolumeTask(void *argument);

/* USER CODE BEGIN PFP */
static void MX_USART1_UART_Init(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
MX_GPIO_Init();
/* USER CODE BEGIN 2 */
MX_USART1_UART_Init();
SparkLCD_Init();
SparkLCD_Fill(0x0000);
/* USER CODE END 2 */

  /* Init scheduler */
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

/* Reaching here means the scheduler could not start. */
Error_Handler();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
__HAL_RCC_GPIOE_CLK_ENABLE();

GPIO_InitTypeDef buttons = {0};
buttons.Pin = GPIO_PIN_2 | GPIO_PIN_3 |
              GPIO_PIN_4 | GPIO_PIN_5;
buttons.Mode = GPIO_MODE_INPUT;
buttons.Pull = GPIO_NOPULL; /* Your external 10 kΩ pull-ups */
HAL_GPIO_Init(GPIOE, &buttons);
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void MX_USART1_UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDisplayTask */
/**
  * @brief  Function implementing the DisplayTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDisplayTask */
void StartDisplayTask(void *argument)
{
    /* USER CODE BEGIN 5 */
    const char message[] = "AudioPlayer: LCD initialized\r\n";

    if (xSemaphoreTake(lcdMutexHandle, portMAX_DELAY) == pdTRUE)
    {
        SparkLCD_Fill(0x0000);

        SparkLCD_Text(16, 24, "STM32 AUDIO PLAYER",
                      0xFFFF, 0x0000);

        SparkLCD_Text(16, 64, "LCD text ready",
                      0x07E0, 0x0000);

        SparkLCD_Text(16, 104, "Song: --",
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
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartButtonTask */
/**
* @brief Function implementing the ButtonTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartButtonTask */
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

        /* Active-low inputs: pressed = 1. */
        uint32_t inputs = GPIOE->IDR;

        for (uint8_t i = 0; i < 4; i++)
        {
            if ((inputs & (GPIO_PIN_2 << i)) == 0)
            {
                rawButtons |= (uint8_t)(1U << i);
            }
        }

        /* Require the reading to remain unchanged for 40 ms. */
        if (rawButtons != lastRaw)
        {
            lastRaw = rawButtons;
            lastChangeTick = now;
        }

        /* Expire selection before accepting a late confirmation. */
        if (selectionActive &&
            (TickType_t)(now - selectionStartTick) >=
                pdMS_TO_TICKS(5000))
        {
            selectionActive = 0;
            pendingSong = currentSong;
            displayChanged = 1;
        }

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
                    selectionActive = 1;
                    selectionStartTick = now;

                    /* B2, B3, B4 represent bits 0, 1, 2. */
                    pendingSong =
                        (uint8_t)((stableButtons >> 1) & 0x07U);
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

/* USER CODE BEGIN Header_StartVolumeTask */
/**
* @brief Function implementing the VolumeTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartVolumeTask */
void StartVolumeTask(void *argument)
{
  /* USER CODE BEGIN StartVolumeTask */
  /* Infinite loop */
  for(;;)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  /* USER CODE END StartVolumeTask */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
