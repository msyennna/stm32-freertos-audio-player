#include "spark_songs.h"
#include "../../../reference/song_def.h"
static const Song *const songs[SPARK_SONG_COUNT] = {
    &FUR_ELISE, &CANNON_IN_D, &MINUET_IN_G_MAJOR, &TURKISH_MARCH,
    &NOCTRUNE_IN_E_FLAT, &WALTZ_NO2, &NOCTRUNE_IN_C_SHARP_MAJOR,
    &SYMPHONY_NO40, &SYMPHONY_NO5, &EINE_KLEINE_NACHTAMUSIK
};
extern "C" int SparkSongs_Get(uint8_t index, SparkSong *result)
{
    if (!result || index >= SPARK_SONG_COUNT) return 0;
    const Song *song = songs[index];
    result->title = song->name1.c_str();
    result->subtitle = song->name2.c_str();
    result->notePeriods = song->note;
    result->beats = song->beat;
    result->tempo = song->tempo;
    result->length = (uint16_t)song->length;
    return 1;
}
