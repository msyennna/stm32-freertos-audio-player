/*
 * Spark-1 ES8388 audio bring-up.
 * Codec register sequence adapted from the supplied RT-Thread driver.
 * Copyright (c) 2006-2021, RT-Thread Development Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include "spark_audio.h"

#define CODEC_ADDRESS (0x10U << 1)

static I2S_HandleTypeDef audioI2S;
static uint16_t tone[64];

static HAL_StatusTypeDef codec_write(
    I2C_HandleTypeDef *bus, uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(
        bus, CODEC_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT,
        &value, 1, 100);
}

static HAL_StatusTypeDef configure_i2s(void)
{
    GPIO_InitTypeDef gpio = {0};
    RCC_PeriphCLKInitTypeDef clocks = {0};

    /*
     * This test expects the current HSI 16 MHz system clock,
     * with the main PLL disabled.
     */
    if ((RCC->CR & RCC_CR_PLLON) != 0U)
    {
        return HAL_ERROR;
    }

    /* HSI / 16 = 1 MHz input to PLLI2S. */
    MODIFY_REG(RCC->PLLCFGR,
               RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLSRC,
               16U);

    /* 1 MHz * 256 / 5 = 51.2 MHz I2S clock. */
    clocks.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    clocks.PLLI2S.PLLI2SN = 256;
    clocks.PLLI2S.PLLI2SR = 5;

    if (HAL_RCCEx_PeriphCLKConfig(&clocks) != HAL_OK)
    {
        return HAL_ERROR;
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI3_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF6_SPI3;

    /* PB3: bit clock, PB5: audio data. */
    gpio.Pin = GPIO_PIN_3 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* PA15: left/right channel clock. */
    gpio.Pin = GPIO_PIN_15;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* PC7: codec master clock. */
    gpio.Pin = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOC, &gpio);

    audioI2S.Instance = SPI3;
    audioI2S.Init.Mode = I2S_MODE_MASTER_TX;
    audioI2S.Init.Standard = I2S_STANDARD_PHILIPS;
    audioI2S.Init.DataFormat = I2S_DATAFORMAT_16B;
    audioI2S.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
    audioI2S.Init.AudioFreq = I2S_AUDIOFREQ_8K;
    audioI2S.Init.CPOL = I2S_CPOL_LOW;
    audioI2S.Init.ClockSource = I2S_CLOCK_PLL;
    audioI2S.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;

    return HAL_I2S_Init(&audioI2S);
}

HAL_StatusTypeDef SparkAudio_TestTone(I2C_HandleTypeDef *codecBus)
{
    static const uint8_t settings[][2] = {
        {0x19, 0x04}, /* Mute DAC during setup. */
        {0x01, 0x50},
        {0x02, 0x00},
        {0x08, 0x00}, /* Codec is I2S slave. */
        {0x04, 0xC0},
        {0x00, 0x12},
        {0x17, 0x18}, /* 16-bit Philips I2S. */
        {0x18, 0x02}, /* MCLK / sample rate = 256. */
        {0x26, 0x00},
        {0x27, 0x9C}, /* Left DAC to left output. */
        {0x2A, 0x9C}, /* Right DAC to right output. */
        {0x2B, 0x80},
        {0x2D, 0x00},
       {0x1A, 0x18}, /* Left attenuation: 12 dB. */
        {0x1B, 0x18}, /* Right attenuation: 12 dB. */
        {0x03, 0xFF}, /* ADC powered down. */
        {0x2E, 0x1E},
        {0x2F, 0x1E},
        {0x04, 0x3C}  /* Enable DAC and outputs. */
    };

    if (HAL_I2C_IsDeviceReady(
            codecBus, CODEC_ADDRESS, 3, 100) != HAL_OK)
    {
        return HAL_ERROR;
    }

    for (uint32_t i = 0;
         i < sizeof(settings) / sizeof(settings[0]);
         i++)
    {
        if (codec_write(codecBus, settings[i][0],
                        settings[i][1]) != HAL_OK)
        {
            /* Attempt to leave the codec muted. */
            codec_write(codecBus, 0x19, 0x04);
            return HAL_ERROR;
        }
    }

    if (configure_i2s() != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* 32 stereo frames: a quiet 250 Hz triangle at 8 kHz. */
    for (uint32_t i = 0; i < 32; i++)
    {
        int32_t sample;

        if (i < 16)
        {
            sample = -4096 + (int32_t)i * 512;
        }
        else
        {
            sample = 4096 - (int32_t)(i - 16) * 512;
        }

        tone[2 * i] = (uint16_t)(int16_t)sample;
        tone[2 * i + 1] = (uint16_t)(int16_t)sample;
    }

    /* Start clocks with a silent frame before unmuting. */
    uint16_t silence[2] = {0, 0};

    if (HAL_I2S_Transmit(&audioI2S, silence, 2, 100) != HAL_OK)
    {
        __HAL_I2S_DISABLE(&audioI2S);
        return HAL_ERROR;
    }

    HAL_Delay(100);

    if (codec_write(codecBus, 0x19, 0x00) != HAL_OK)
    {
        __HAL_I2S_DISABLE(&audioI2S);
        return HAL_ERROR;
    }

    HAL_StatusTypeDef result = HAL_OK;

    /* Approximately one second; polling is only for this startup test. */
    for (uint32_t block = 0; block < 250; block++)
    {
        result = HAL_I2S_Transmit(&audioI2S, tone, 64, 100);

        if (result != HAL_OK)
        {
            break;
        }
    }

    HAL_StatusTypeDef muteResult =
        codec_write(codecBus, 0x19, 0x04);

    HAL_I2S_Transmit(&audioI2S, silence, 2, 100);
    __HAL_I2S_DISABLE(&audioI2S);

    return result == HAL_OK ? muteResult : result;
}