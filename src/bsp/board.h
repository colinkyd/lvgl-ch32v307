#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include <Arduino.h>

/* ================= 屏幕参数 (集中管理) ================= */
/* 1.8" ST7735S shield, 横屏使用: 160 宽 x 128 高 */
#define LCD_WIDTH     160
#define LCD_HEIGHT    128
#define LCD_OFFSET_X  0
#define LCD_OFFSET_Y  0
#define LCD_ROTATION  3               /* setRotation: case3 -> 160x128 横屏 */

/* 部分刷新缓冲行数 (RGB565, 16bit/px) */
#define LV_BUF_LINES  10

/* ================= SPI / TFT 引脚 (WCH 核心数字号) ================= */
#define PIN_SPI_MOSI  17              /* PB15, SPI2 重绑 */
#define PIN_SPI_MISO  18              /* PB14, SPI2 重绑 */
#define PIN_SPI_SCK   19              /* PB13, SPI2 重绑 */
#define PIN_TFT_CS    16              /* PB12 */
#define PIN_TFT_DC    14              /* PB1  */
#define PIN_TFT_RST   (-1)            /* 接 5V, 无复位脚 */
#define TFT_SPI_SPEED 36000000UL

/* ================= 输入 (五向摇杆 ADC) ================= */
#define PIN_JOY_ADC   A3

/* ================= 内存布局 (链接脚本) ================= */
#define HEAP_END_ADDR 0x2000F800UL    /* 堆顶 (RAM 64KB 边界) */

#ifdef __cplusplus
extern "C" {
#endif
extern char _end[];                   /* 堆起点 (链接脚本符号) */
void * _sbrk(long incr);              /* newlib 堆分配器, _sbrk(0)=当前堆顶 */
#ifdef __cplusplus
}
#endif

#define HEAP_TOTAL (HEAP_END_ADDR - (uint32_t)_end)

/* ================= 板级初始化 ================= */
void board_init(void);

/* 五向摇杆: 返回 0=右 1=上 2=中 3=左 4=下 5=无 */
uint8_t board_joystick_read(void);

#endif /* BSP_BOARD_H */
