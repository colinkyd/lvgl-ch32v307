// LVGL 8.3.11 on CH32V307EVT-R1 + 1.8" ST7735S TFT shield (160x128)
//
// 可维护工程结构:
//   src/
//     main.cpp            入口: setup()/loop() 编排 (独立编译为 sketch 主对象)
//     bsp/  board.{h,cpp} 集中 GPIO + 屏幕参数 (LCD_WIDTH/HEIGHT/OFFSET/ROTATION)
//           lcd_st7735.{h,cpp}  TFT 驱动封装 (Adafruit_ST7735)
//     lv_port/  lv_port_disp.{h,cpp}   画缓冲 + 显示驱动 + flush
//               lv_port_indev.{h,cpp}  五向摇杆输入 (keypad)
//               lv_port_tick.{h,cpp}   1ms tick (CH32 SysTick -> lv_tick_inc)
//     app/  ui.{h,cpp}    benchmark demo 启动 + CPU/RAM 统计
//
// 本 .ino 仅为 arduino-cli 识别 sketch 的入口占位 (setup/loop 在 src/main.cpp,
// 若在此 include main.cpp 会与其独立编译的 .o 重复定义)。

#include <Arduino.h>
