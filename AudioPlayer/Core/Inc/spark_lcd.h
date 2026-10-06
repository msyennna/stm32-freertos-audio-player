#ifndef SPARK_LCD_H
#define SPARK_LCD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SparkLCD_Init(void);
void SparkLCD_Fill(uint16_t color);
void SparkLCD_Text(uint16_t x, uint16_t y, const char *text,
                   uint16_t foreground, uint16_t background);

#ifdef __cplusplus
}
#endif

#endif