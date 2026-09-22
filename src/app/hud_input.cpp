/* ============================================================
 * hud_input.cpp — 五向键输入层 (复用 bsp/key_adc 边沿事件模型)
 *
 * 每 loop: key_adc_poll() -> key_adc_event() -> 映射 HUD 动作。
 * 非阻塞: key_adc 内部已 5 连采中值 + 死区 + 30ms 去抖。
 * 不重复采样: key_adc_poll 只在 key_adc.cpp 里做一次。
 *
 * 按键映射:
 *   UP     -> 上一页面 (循环, PAGE_MAIN 的上一页 = PAGE_SYSTEM)
 *   DOWN   -> 下一页面 (循环, PAGE_SYSTEM 的下一页 = PAGE_MAIN)
 *   LEFT   -> 减少亮度 (预留)
 *   RIGHT  -> 增加亮度 (预留)
 *   CENTER -> 返回 PAGE_MAIN
 * ============================================================ */
#include "hud_input.h"
#include "hud_page.h"
#include "../bsp/key_adc.h"
#include <Arduino.h>

void hud_input_init(void) {
  /* key_adc_init 设 ADC 分辨率 (须在 board_init 后)。
   * board_init 已调 analogReadResolution(10), 但 key_adc_init 还会
   * 预填充滑动平均缓冲 + 重置去抖状态, 必须调。 */
  key_adc_init();
}

void hud_input_poll(void) {
  key_adc_poll();               /* 采样 + 30ms 边沿去抖 */
  key_t k = key_adc_event();    /* 取走一个边沿 (无则 KEY_NONE) */
  if (k == KEY_NONE) return;

  switch (k) {
    case KEY_UP:
      hud_page_switch((HUD_PAGE)((hud_page_current() + HUD_PAGE_COUNT - 1) % HUD_PAGE_COUNT));
      break;
    case KEY_DOWN:
      hud_page_switch((HUD_PAGE)((hud_page_current() + 1) % HUD_PAGE_COUNT));
      break;
    case KEY_LEFT:
      hud_brightness_down();
      break;
    case KEY_RIGHT:
      hud_brightness_up();
      break;
    case KEY_SELECT:
      hud_page_switch(PAGE_MAIN);   /* CENTER = 返回主页 */
      break;
    default:
      break;
  }
}
