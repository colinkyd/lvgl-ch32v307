#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include <Arduino.h>
#include <stdint.h>
#include "perf_config.h"

/* 全工程 debug 打印门控 (与 CPM 协议口联动):
 *   0 = 正式: 协议口 (COM9, 9600) 只走 5A A5 帧, 不混入文本
 *   1 = 调试: perf/[ui]/[indev] 等日志同口打印 (PC 上位机须未运行)
 * cpm_serial.h 的 CPM_DEBUG=1 会自动把它置 1. */
#ifndef CPM_DBG
#define CPM_DBG 0
#endif

/* ===== 屏幕参数 (集中管理) =====
 * 竖屏 (portrait) 128x160: 库 setRotation(0) 内部坐标 width=128/height=160/
 * xstart=2/ystart=1, 硬件 MADCTL=0xC0 (MX|MY, 见 lcd_st7735.cpp fix_madctl).
 * 坐标与朝向配对, 不回绕. 横屏是 160x128 (case3, MADCTL=0x60). */
#define LCD_WIDTH     128
#define LCD_HEIGHT    160
#define LCD_OFFSET_X  0
#define LCD_OFFSET_Y  0
#define LCD_ROTATION  0               /* case0: 竖屏 128x160, _xstart=2/_ystart=1 */

/* LVGL draw buffer 行数 (部分缓冲, 160x N 行 RGB565) */
#define LV_BUF_LINES  PERF_BUF_LINES

/* ================= SPI / TFT 引脚 (WCH 核心数字号) ================= */
#define PIN_SPI_MOSI  17              /* PB15, SPI2 重绑 */
#define PIN_SPI_MISO  18              /* PB14, SPI2 重绑 */
#define PIN_SPI_SCK   19              /* PB13, SPI2 重绑 */
#define PIN_TFT_CS    16              /* PB12 */
#define PIN_TFT_DC    14              /* PB1  */
#define PIN_TFT_RST   (-1)            /* 接 5V, 无复位脚 */
#define TFT_SPI_SPEED PERF_SPI_SPEED /* 请求频率; 实际由 SPI 分频表落地 (源 PCLK1=72MHz) */

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

/* 性能测试: 帧率/FPS/flush 时间统计, 1s 窗口串口上报 */
void perf_init(void);
void perf_on_frame(void);                       /* loop 每帧调用 */
void perf_on_flush(uint32_t us);               /* lcd_flush 耗时记录 */
void perf_report(uint32_t now_ms, uint32_t cpu_pct, uint32_t lv_ram_kb, uint32_t free_ram_kb);

#endif /* BSP_BOARD_H */
