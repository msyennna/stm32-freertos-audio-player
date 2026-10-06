/*
 * Spark-1 ES8388 melody player with TIM7 note ticker and I2S DMA.
 * Codec register sequence adapted from the supplied RT-Thread driver.
 * Copyright (c) 2006-2021, RT-Thread Development Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include "spark_audio.h"
#include "spark_songs.h"
#include <string.h>

#define CODEC_ADDRESS (0x10U << 1)

static I2S_HandleTypeDef audioI2S;
static DMA_HandleTypeDef audioDMA;
static uint16_t samples[128]; /* 64 stereo frames: two 4 ms halves. */
static TIM_HandleTypeDef noteTimer;
static uint32_t noteMilliseconds[176];
static volatile uint32_t noteTicks;
#define HALF_FRAMES 32U
static uint32_t increments[176], durations[176];
static volatile uint8_t playing, audioFailed;
static volatile uint8_t paused = 0;
static volatile uint16_t amplitude = 2048;
static uint16_t noteCount;
static volatile uint16_t noteIndex;
static volatile uint32_t phase, elapsed;
static uint8_t initialized;

static void fill_half(uint16_t *destination)
{
    for (unsigned frame = 0; frame < HALF_FRAMES; ++frame)
    {
        int32_t value = 0;
        if (playing && !paused && !audioFailed && noteIndex < noteCount)
        {
            uint32_t duration = durations[noteIndex];
            uint32_t increment = increments[noteIndex];
            if (increment && elapsed < duration - duration / 10U)
            {
                uint32_t position = phase >> 16;
                int32_t triangle = position < 32768U
                    ? (int32_t)position * 2 - 32768
                    : 98303 - (int32_t)position * 2;
                value = triangle * (int32_t)amplitude / 32768;
                /* Short attack and release to reduce clicks. */
                uint32_t sounding = duration - duration / 10U;
                uint32_t envelope = elapsed < 32U ? elapsed : 32U;
                if (sounding - elapsed < envelope)
                    envelope = sounding - elapsed;
                value = value * (int32_t)envelope / 32;
                phase += increment;
            }
            /* DMA generates samples; TIM7 decides when notes change. */
            if (elapsed < duration) ++elapsed;
        }
        destination[2U * frame] = (uint16_t)(int16_t)value;
        destination[2U * frame + 1U] = (uint16_t)(int16_t)value;
    }
}

/* Recurring note ticker: one callback every millisecond.
 * No LCD, I2C, delays, floating point or RTOS calls in this callback. */
static void advance_note_tick(void)
{
    if (!playing || paused || audioFailed || noteIndex >= noteCount) return;

    if (++noteTicks >= noteMilliseconds[noteIndex])
    {
        /* DMA has higher interrupt priority. Protect the short transition
         * so its callback cannot observe a partly updated note. */
        uint32_t interruptMask = __get_PRIMASK();
        __disable_irq();
        noteTicks = 0;
        elapsed = phase = 0;
        ++noteIndex;
        if (noteIndex >= noteCount) playing = 0;
        __DMB();
        __set_PRIMASK(interruptMask);
    }
}

void TIM7_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&noteTimer, TIM_FLAG_UPDATE) != RESET &&
        __HAL_TIM_GET_IT_SOURCE(&noteTimer, TIM_IT_UPDATE) != RESET)
    {
        __HAL_TIM_CLEAR_IT(&noteTimer, TIM_IT_UPDATE);
        advance_note_tick();
    }
}

static HAL_StatusTypeDef start_note_ticker(void)
{
    __HAL_RCC_TIM7_CLK_ENABLE();
    uint32_t timerClock = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0U) timerClock *= 2U;
    if (timerClock < 1000000U || timerClock % 1000000U != 0U)
        return HAL_ERROR;

    noteTimer.Instance = TIM7;
    noteTimer.Init.Prescaler = timerClock / 1000000U - 1U;
    noteTimer.Init.CounterMode = TIM_COUNTERMODE_UP;
    noteTimer.Init.Period = 1000U - 1U; /* 1 MHz / 1000 = 1 kHz. */
    noteTimer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    noteTimer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&noteTimer) != HAL_OK) return HAL_ERROR;
    __HAL_TIM_CLEAR_IT(&noteTimer, TIM_IT_UPDATE);
    HAL_NVIC_SetPriority(TIM7_IRQn, 7, 0);
    HAL_NVIC_ClearPendingIRQ(TIM7_IRQn);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);
    HAL_StatusTypeDef result = HAL_TIM_Base_Start_IT(&noteTimer);
    if (result != HAL_OK) HAL_NVIC_DisableIRQ(TIM7_IRQn);
    return result;
}

void DMA1_Stream7_IRQHandler(void) { HAL_DMA_IRQHandler(&audioDMA); }
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *handle)
{
    if (handle == &audioI2S) fill_half(samples);
}
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *handle)
{
    if (handle == &audioI2S) fill_half(samples + 64);
}
void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *handle)
{
    if (handle == &audioI2S) { playing = 0; audioFailed = 1; }
}

void SparkAudio_SetVolume(uint8_t percent)
{
    if (percent > 100U) percent = 100U;
    amplitude = (uint16_t)(32767U * percent / 100U);
}
uint8_t SparkAudio_IsPlaying(void)
{
    return playing && !paused;
}

uint8_t SparkAudio_IsPaused(void)
{
    return playing && paused;
}

void SparkAudio_TogglePause(void)
{
    if (playing && !audioFailed)
    {
        paused = !paused;
    }
}
uint8_t SparkAudio_HasError(void) { return audioFailed; }


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

HAL_StatusTypeDef SparkAudio_Init(I2C_HandleTypeDef *codecBus)
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

    __HAL_RCC_DMA1_CLK_ENABLE();
    audioDMA.Instance = DMA1_Stream7;
    audioDMA.Init.Channel = DMA_CHANNEL_0;
    audioDMA.Init.Direction = DMA_MEMORY_TO_PERIPH;
    audioDMA.Init.PeriphInc = DMA_PINC_DISABLE;
    audioDMA.Init.MemInc = DMA_MINC_ENABLE;
    audioDMA.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    audioDMA.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    audioDMA.Init.Mode = DMA_CIRCULAR;
    audioDMA.Init.Priority = DMA_PRIORITY_HIGH;
    audioDMA.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&audioDMA) != HAL_OK) return HAL_ERROR;
    __HAL_LINKDMA(&audioI2S, hdmatx, audioDMA);
    HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);
    memset(samples, 0, sizeof(samples));
    if (HAL_I2S_Transmit_DMA(&audioI2S, samples, 128) != HAL_OK)
        return HAL_ERROR;
    HAL_Delay(100); /* Only called before the scheduler starts. */
    if (codec_write(codecBus, 0x19, 0x00) != HAL_OK)
    {
        HAL_I2S_DMAStop(&audioI2S);
        return HAL_ERROR;
    }
    if (start_note_ticker() != HAL_OK)
    {
        codec_write(codecBus, 0x19, 0x04);
        HAL_I2S_DMAStop(&audioI2S);
        return HAL_ERROR;
    }
    initialized = 1;
    return HAL_OK;
}

HAL_StatusTypeDef SparkAudio_PlaySong(uint8_t index)
{
    SparkSong song;
    if (!initialized || audioFailed || !SparkSongs_Get(index, &song)
        || !song.length || song.length > 176U || song.tempo <= 0.0f)
        return HAL_ERROR;

    /* The callback sees silence while the next melody is prepared. */
    playing = 0;
    for (uint16_t i = 0; i < song.length; ++i)
    {
        float period = song.notePeriods[i];
        increments[i] = period > 0.0f
            ? (uint32_t)(4294967296.0 / (8.0 * (double)period)) : 0U;
        /* One supplied eighth-note beat (0.125) lasts tempo seconds.
         * Change this factor if the lecturer's missing example uses
         * a different interpretation of tempo. */
        float count = song.beats[i] * song.tempo * 8.0f * 8000.0f;
        durations[i] = count >= 80.0f ? (uint32_t)(count + 0.5f) : 80U;
        noteMilliseconds[i] = (durations[i] + 7U) / 8U;
    }
    uint32_t interruptMask = __get_PRIMASK();
    __disable_irq();
    noteCount = song.length;
    paused = 0;
    noteIndex = 0;
    elapsed = phase = noteTicks = 0;
    __DMB();
    playing = 1;
    __set_PRIMASK(interruptMask);
    return HAL_OK;
}


/* Called by DisplayTask. Reads the note ticker; does not change playback. */
void SparkAudio_GetPositionMillis(uint32_t *position, uint32_t *length)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    uint16_t index = noteIndex;
    uint16_t count = noteCount;
    uint32_t ticks = noteTicks;
    __set_PRIMASK(mask);

    uint32_t total = 0, played = 0;
    for (uint16_t i = 0; i < count; ++i)
    {
        total += noteMilliseconds[i];
        if (i < index) played += noteMilliseconds[i];
    }
    if (index < count) played += ticks;
    if (played > total) played = total;
    if (position) *position = played;
    if (length) *length = total;
}
