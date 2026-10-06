#ifndef SPARK_AUDIO_H
#define SPARK_AUDIO_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

HAL_StatusTypeDef SparkAudio_Init(I2C_HandleTypeDef *codecBus);
HAL_StatusTypeDef SparkAudio_PlaySong(uint8_t index);

void SparkAudio_GetPositionMillis(uint32_t *position, uint32_t *length);
void SparkAudio_SetVolume(uint8_t percent);
void SparkAudio_TogglePause(void);

uint8_t SparkAudio_IsPlaying(void);
uint8_t SparkAudio_IsPaused(void);
uint8_t SparkAudio_HasError(void);

#ifdef __cplusplus
}
#endif

#endif /* SPARK_AUDIO_H */