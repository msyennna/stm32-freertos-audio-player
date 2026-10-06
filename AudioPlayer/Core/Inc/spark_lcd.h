#ifndef SPARK_LCD_H
#define SPARK_LCD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SparkLCD_Init(void);
void SparkLCD_Fill(uint16_t color);

#ifdef __cplusplus
}
#endif

#endif