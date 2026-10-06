#include "stm32f4xx_hal.h"

void SysTick_Handler(void)
{
    HAL_IncTick();
}

int main(void)
{
    HAL_Init();

    /* Use the internal 16 MHz oscillator for this initial test. */
    RCC_OscInitTypeDef oscillator = {0};
    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    oscillator.HSIState = RCC_HSI_ON;
    oscillator.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    oscillator.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK)
    {
        while (1) {}
    }

    RCC_ClkInitTypeDef clock = {0};
    clock.ClockType = RCC_CLOCKTYPE_SYSCLK |
                      RCC_CLOCKTYPE_HCLK |
                      RCC_CLOCKTYPE_PCLK1 |
                      RCC_CLOCKTYPE_PCLK2;
    clock.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clock.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clock.APB1CLKDivider = RCC_HCLK_DIV1;
    clock.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_0) != HAL_OK)
    {
        while (1) {}
    }

    __HAL_RCC_GPIOF_CLK_ENABLE();

    GPIO_InitTypeDef led = {0};
    led.Pin = GPIO_PIN_12;
    led.Mode = GPIO_MODE_OUTPUT_PP;
    led.Pull = GPIO_NOPULL;
    led.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOF, &led);

    while (1)
    {
        HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_12);
        HAL_Delay(500);
    }
}