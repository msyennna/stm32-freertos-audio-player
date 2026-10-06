#ifndef SPARK_SONGS_H
#define SPARK_SONGS_H
#include <stdint.h>
#define SPARK_SONG_COUNT 10
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const char *title;
    const char *subtitle;
    const float *notePeriods;
    const float *beats;
    float tempo;
    uint16_t length;
} SparkSong;
int SparkSongs_Get(uint8_t index, SparkSong *result);
#ifdef __cplusplus
}
#endif
#endif
