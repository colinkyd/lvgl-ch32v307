#include <Arduino.h>

#include "bsp/board.h"
#include "bsp/lcd_st7735.h"
#include "lv_port/lv_port_disp.h"
#include "lv_port/lv_port_indev.h"
#include "lv_port/lv_port_tick.h"
#include "app/ui.h"

extern "C" {
#include <lvgl.h>
}

void setup(void) {
  board_init();      // SPI 重绑 / Serial / ADC
  lcd_init();        // ST7735 初始化 (rotation/speed/fill)
  lv_init();         // LVGL 内核
  lv_port_disp_init();   // 画缓冲 + 显示驱动
  lv_port_indev_init();  // 摇杆输入
  ui_init();         // benchmark demo + 统计

  delay(200);        // 让串口/外设稳定
}

void loop(void) {
  uint32_t t0 = millis();          /* millis 分辨率 (WCH micros 短窗口噪声大) */

  perf_on_frame();           /* 主循环率参考 */
  lv_port_tick_task();   // 硬件 1ms tick 增量
  lv_timer_handler();    // 非阻塞 LVGL 主处理 (动画/timer/刷新, 含同步 flush)

  ui_stats_tick(millis() - t0); // 本帧 LVGL 忙时间 (ms) -> CPU 占比 + perf 上报
}
