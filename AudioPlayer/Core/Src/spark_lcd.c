/*
 * Spark-1 LCD HAL adaptation.
 * Initialization sequence based on the supplied RT-Thread driver:
 * Copyright (c) 2006-2022, RT-Thread Development Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include "main.h"
#include "spark_lcd.h"

#define LCD_COMMAND (*(volatile uint8_t *)0x68000000UL)
#define LCD_DATA    (*(volatile uint8_t *)0x68040000UL)

static SRAM_HandleTypeDef lcd_sram;

static void command(uint8_t value)
{
    LCD_COMMAND = value;
}

static void data(uint8_t value)
{
    LCD_DATA = value;
}

static void configure_bus(void)
{
    GPIO_InitTypeDef gpio = {0};
    FSMC_NORSRAM_TimingTypeDef timing = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_FSMC_CLK_ENABLE();

    /* D2, D3, NOE, NWE, A18, D0, D1 */
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 |
               GPIO_PIN_4 | GPIO_PIN_5 |
               GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_FSMC;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* D4 through D7 */
    gpio.Pin = GPIO_PIN_7 | GPIO_PIN_8 |
               GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOE, &gpio);

    /* NE3 */
    gpio.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOG, &gpio);

    /* Reset and backlight */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOD, &gpio);

    gpio.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOF, &gpio);

    lcd_sram.Instance = FSMC_NORSRAM_DEVICE;
    lcd_sram.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
    lcd_sram.Init.NSBank = FSMC_NORSRAM_BANK3;
    lcd_sram.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
    lcd_sram.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
    lcd_sram.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_8;
    lcd_sram.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
    lcd_sram.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
    lcd_sram.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
    lcd_sram.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
    lcd_sram.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
    lcd_sram.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
    lcd_sram.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;
    lcd_sram.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
    lcd_sram.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
    lcd_sram.Init.PageSize = FSMC_PAGE_SIZE_NONE;

    /* Conservative timings for initial bring-up. */
    timing.AddressSetupTime = 15;
    timing.AddressHoldTime = 1;
    timing.DataSetupTime = 60;
    timing.BusTurnAroundDuration = 1;
    timing.CLKDivision = 2;
    timing.DataLatency = 2;
    timing.AccessMode = FSMC_ACCESS_MODE_A;

    if (HAL_SRAM_Init(&lcd_sram, &timing, NULL) != HAL_OK)
    {
        Error_Handler();
    }
}

void SparkLCD_Init(void)
{
    /* Each entry: command, parameter count, parameters. */
    static const uint8_t sequence[] = {
        0x36, 1, 0x00,
        0x3A, 1, 0x65,
        0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,
        0xB7, 1, 0x35,
        0xBB, 1, 0x37,
        0xC0, 1, 0x2C,
        0xC2, 1, 0x01,
        0xC3, 1, 0x12,
        0xC4, 1, 0x20,
        0xC6, 1, 0x0F,
        0xD0, 2, 0xA4, 0xA1,
        0xE0, 14, 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
                  0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23,
        0xE1, 14, 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
                  0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23,
        0x21, 0
    };

    configure_bus();

    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);
    HAL_Delay(100);

    unsigned int index = 0;
    while (index < sizeof(sequence))
    {
        command(sequence[index++]);
        uint8_t count = sequence[index++];

        while (count--)
        {
            data(sequence[index++]);
        }
    }

    command(0x11); /* Sleep out */
    HAL_Delay(120);
    command(0x29); /* Display on */
}

void SparkLCD_Fill(uint16_t color)
{
    command(0x2A); /* Columns 0–239 */
    data(0); data(0);
    data(0); data(239);

    command(0x2B); /* Rows 0–239 */
    data(0); data(0);
    data(0); data(239);

    command(0x2C);

    for (uint32_t pixel = 0; pixel < 240UL * 240UL; pixel++)
    {
        data((uint8_t)(color >> 8));
        data((uint8_t)color);
    }

    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
}