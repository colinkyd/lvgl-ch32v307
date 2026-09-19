#include "lv_port_indev.h"
#include "../bsp/board.h"
#include "../bsp/key_adc.h"
#include <Arduino.h>   /* millis() 边沿时间戳 */

extern "C" {
#include <lvgl.h>
}

/*
 * 五向摇杆 -> LVGL keypad indev
 *
 * 数据链路: LVGL read timer -> joy_read_cb() -> key_scan() -> data->key
 * 全程在 LVGL 主循环上下文执行, ADC 不在中断里读 (满足 "不要在 ADC 中断中调 LVGL API")。
 *
 * 键值映射 (key_to_lv_key):
 *   UP/DOWN  -> LV_KEY_NEXT / LV_KEY_PREV   (LVGL keypad 内建: 只有 NEXT/PREV 移动 group 焦点)
 *   LEFT/RIGHT -> LV_KEY_LEFT / LV_KEY_RIGHT (slider 内建 +/-1; checkbox 内建切换)
 *   SELECT   -> LV_KEY_ENTER                 (点击当前焦点对象)
 *
 * 释放/长按/重复由 LVGL keypad indev 内建处理。
 * indev 的 group 由 ui_group_indev_bind() 切到 main_group (焦点 UI 就绪后)。
 */

/* bsp key_t -> LV_KEY_* (见文件头映射说明) */
static uint32_t key_to_lv_key(key_t k) {
  switch (k) {
    case KEY_UP:     return LV_KEY_NEXT;    /* 焦点后移 */
    case KEY_DOWN:   return LV_KEY_PREV;    /* 焦点前移 */
    case KEY_LEFT:   return LV_KEY_LEFT;
    case KEY_RIGHT:  return LV_KEY_RIGHT;
    case KEY_SELECT: return LV_KEY_ENTER;
    case KEY_NONE:
    default:         return 0;
  }
}

/* key_t 名称 (read_cb 边沿日志用) */
static const char *key_name(key_t k) {
  switch (k) {
    case KEY_UP:     return "UP";
    case KEY_DOWN:   return "DOWN";
    case KEY_LEFT:   return "LEFT";
    case KEY_RIGHT:  return "RIGHT";
    case KEY_SELECT: return "ENTER";
    case KEY_NONE:
    default:         return "NONE";
  }
}

static lv_group_t *joy_group = NULL;   /* 占位 group (init 时先绑, 焦点 UI 就绪后切 main_group) */

/* 边沿事件模型 (对照 PC_HUD_Cyber):
 * LVGL keypad indev 不再参与焦点 —— read_cb 只调 key_adc_poll() 做 30ms 边沿去抖
 * + 日志, 并向 LVGL 返回 RELEASED (哑火)。焦点由 ui_panel 手动分发 (ui_panel_tick
 * 取 key_adc_event() -> handle_key)。
 *
 * 为何不用 LVGL indev: LVGL 8.3 keypad_proc 对 NEXT/PREV 只在"按边沿"处理一次,
 * 配合 bsp 旧多层滤波 (滑动平均+死区+去抖+15ms read timer) 短按被吞/焦点乱跳。
 * PC_HUD 单层 30ms 边沿 + 每 loop 高频轮询 + 手动分发实测可靠, 故照搬。
 *
 * 键值 (bsp key_t): UP/DOWN/LEFT/RIGHT/SELECT, 手动映射到焦点操作 (见 ui_panel)。
 */
static key_t last_log_key = KEY_NONE;

/* read_cb: LVGL 周期性调用 (主循环上下文, ~15ms)。纯哑火 + 边沿日志。
 * 采样/边沿去抖在 ui_panel_tick 的 key_adc_poll() (每 loop, 对照 PC_HUD), 此处不 poll。 */
static void joy_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  (void)drv;

  /* 边沿日志: 打印当前稳定按键 (辅助诊断; current 由 tick 的 poll 更新, 滞后 ~1 loop) */
  key_t cur = key_adc_current();
  if (cur != last_log_key) {
    uint16_t raw = key_adc_read();
    Serial.printf("[indev] %s @%lu raw=%u\r\n",
                  key_name(cur), millis(), raw);
    last_log_key = cur;
  }

  /* 哑火: 不向 LVGL 报键 (焦点全手动, 见 ui_panel::handle_key)。返回 RELEASED */
  data->state = LV_INDEV_STATE_RELEASED;
  data->key   = 0;
}

void lv_port_indev_init(void) {
  /* bsp 驱动初始化: ADC 10bit + 滑动平均缓冲预填 (须在 board_init 的 Serial 之后) */
  key_adc_init();

  /* 占位 group: init 时先绑上 (indev->group 非空), 焦点 UI 就绪后由
   * ui_group_indev_bind() 切到 main_group。 */
  joy_group = lv_group_create();

  /* indev 驱动: keypad */
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type                 = LV_INDEV_TYPE_KEYPAD;
  indev_drv.read_cb              = joy_read_cb;
  indev_drv.long_press_time      = LV_INDEV_DEF_LONG_PRESS_TIME;      /* 400ms  */
  indev_drv.long_press_repeat_time = LV_INDEV_DEF_LONG_PRESS_REP_TIME; /* 100ms  */

  lv_indev_t *joy = lv_indev_drv_register(&indev_drv);
  /* read timer 周期: LVGL 8.3 写死用 LV_INDEV_DEF_READ_PERIOD(30ms) 创建, 非可配字段。
   * 30ms + 旧滤波(AVG_N=16/DEBOUNCE=3) 确认延迟~540ms, 短按全丢 (只能长按走重复)。
   * 降到 15ms, 配合 key_adc_config 的 AVG_N=8/DEBOUNCE=2, 确认延迟~150ms。 */
  lv_timer_set_period(joy->driver->read_timer, 15);
  lv_indev_set_group(joy, joy_group);

  Serial.printf("[indev] keypad ready: long_press=%dms rep=%dms (UP=NEXT DOWN=PREV L/R=LEFT/RIGHT SEL=ENTER)\r\n",
                (int)indev_drv.long_press_time, (int)indev_drv.long_press_repeat_time);
}
