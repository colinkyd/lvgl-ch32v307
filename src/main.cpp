#include <Arduino.h>

#include "bsp/board.h"
#include "bsp/lcd_st7735.h"
#include "lv_port/lv_port_disp.h"
#include "lv_port/lv_port_indev.h"
#include "lv_port/lv_port_tick.h"
#include "app/ui.h"
#include "app/cpm_serial.h"
#include "app/cpm_ui.h"

extern "C" {
#include <lvgl.h>
}

void setup(void) {
  board_init();      // SPI 重绑 / Serial / ADC
  lcd_init();        // ST7735 初始化 (rotation/speed/fill)
  lv_init();         // LVGL 内核
  lv_port_disp_init();   // 画缓冲 + 显示驱动
  lv_port_indev_init();  // 摇杆输入 (keypad indev, 占位 group)
  ui_init();             // benchmark -> finished_cb 建焦点 UI + 切 indev group

  delay(200);        // 让串口/外设稳定
  cpm_serial_init(); // CPM 协议口: 9600 8N1 (在 ui_init 的 115200 banner 之后切速)
  cpm_ui_init();     // CPM 性能监视器 UI: 替换控制板为主屏 (160x128 紧凑 5 行)
}

void loop(void) {
  uint32_t t0 = millis();          /* millis 分辨率 (WCH micros 短窗口噪声大) */

  perf_on_frame();           /* 主循环率参考 */
  cpm_serial_poll();    // CPM 协议: 批量收帧 + 应答 (在 LVGL 重绘前, 应答及时)
  cpm_ui_update();      // CPM UI: 数值变化时刷新 label/bar (无变化不重绘)
  lv_port_tick_task();   // 硬件 1ms tick 增量
  lv_timer_handler();    // 非阻塞 LVGL 主处理 (动画/timer/刷新, 含同步 flush)

  ui_stats_tick(millis() - t0); // 本帧 LVGL 忙时间 (ms) -> CPU 占比 + perf 上报
}
