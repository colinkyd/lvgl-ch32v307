#ifndef BSP_LCD_ST7735_H
#define BSP_LCD_ST7735_H

#include <Arduino.h>

/*
 * ST7735S TFT 驱动封装 (Adafruit_ST7735 库)
 * 引脚/尺寸/旋转角全部取自 board.h, 本文件不硬编码
 */
void lcd_init(void);

/* 刷新矩形区域 (RGB565, 小端), x/y 为屏内坐标 (已含偏移) */
void lcd_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *px);

#endif /* BSP_LCD_ST7735_H */
