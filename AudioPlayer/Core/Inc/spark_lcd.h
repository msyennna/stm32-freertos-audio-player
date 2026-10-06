#ifndef SPARK_LCD_H
#define SPARK_LCD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SparkLCD_Rect(uint16_t x, uint16_t y, uint16_t width,
                   uint16_t height, uint16_t color);
void SparkLCD_Circle(uint16_t cx, uint16_t cy, uint16_t radius,
                     uint16_t color);
void SparkLCD_RoundRect(uint16_t x, uint16_t y, uint16_t width,
                        uint16_t height, uint16_t radius, uint16_t color);
void SparkLCD_Init(void);
void SparkLCD_Fill(uint16_t color);
void SparkLCD_Text(uint16_t x, uint16_t y, const char *text,
                   uint16_t foreground, uint16_t background);

#ifdef __cplusplus
}
#endif

#endif