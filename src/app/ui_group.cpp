#include "ui_group.h"
#include "../bsp/board.h"

extern "C" {
#include <lvgl.h>
}

/*
 * main_group: Button -> Slider -> Checkbox -> List(item1..3)
 * 顺序即 UP/DOWN 焦点移动顺序 (LV_KEY_NEXT/PREV, indev 层映射)。
 * 160x128 固定布局: 顶栏焦点显示 + 4 行控件 (22px 行)。
 */

static lv_group_t *main_group = NULL;
static lv_obj_t   *focus_label = NULL;   /* 屏幕顶部: 当前 focus 对象名 */
static lv_obj_t   *ui_btn  = NULL;
static lv_obj_t   *ui_slider = NULL;
static lv_obj_t   *ui_check = NULL;
static lv_obj_t   *ui_list = NULL;
static lv_obj_t   *ui_list_items[3] = {NULL, NULL, NULL};

static lv_obj_t   *btn_name = NULL;      /* button 上的文字 (点击计数用) */

static const char *obj_name(const lv_obj_t *o) {
  if (o == NULL)             return "NONE";
  if (o == ui_btn)           return "BUTTON";
  if (o == ui_slider)        return "SLIDER";
  if (o == ui_check)         return "CHECKBOX";
  for (int i = 0; i < 3; i++)
    if (o == ui_list_items[i]) {
      static char buf[16];
      lv_snprintf(buf, sizeof(buf), "LIST_ITEM%d", i + 1);
      return buf;
    }
  return "OTHER";
}

/* ---- 事件日志 (串口): 证明 LVGL keypad 真正分发到了对象 ---- */

/* button: ENTER 点击 -> 计数 +1 */
static void btn_event_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    static uint32_t clicks = 0;
    clicks++;
    lv_label_set_text_fmt(btn_name, "Btn %lu", (unsigned long)clicks);
    Serial.printf("[ui] BUTTON clicked (#%lu)\r\n", (unsigned long)clicks);
  }
}

/* slider: 值变化 (LV_KEY_LEFT/RIGHT 内建 +/-1) */
static void slider_event_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    int32_t v = lv_slider_get_value(ui_slider);
    Serial.printf("[ui] SLIDER value=%d\r\n", (int)v);
  }
}

/* checkbox: 值变化 (LV_KEY_ENTER 或 LV_KEY_LEFT/RIGHT/UP/DOWN 切换) */
static void check_event_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    bool on = lv_obj_has_state(ui_check, LV_STATE_CHECKED);
    Serial.printf("[ui] CHECKBOX %s\r\n", on ? "ON" : "OFF");
  }
}

/* list item: ENTER 点击 */
static void list_item_event_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *item = lv_event_get_target(e);
    Serial.printf("[ui] LIST item clicked (%s)\r\n", obj_name(item));
  }
}

/* 焦点变化: 打串口 (每次焦点移动) */
static void focused_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_target(e);
  Serial.printf("[ui] focus -> %s (0x%08lx)\r\n",
                obj_name(obj), (unsigned long)(uintptr_t)obj);
}

void ui_group_init(void) {
  main_group = lv_group_create();
  lv_group_set_default(main_group);

  lv_obj_t *scr = lv_scr_act();

  /* 顶部焦点显示栏 */
  focus_label = lv_label_create(scr);
  lv_label_set_text(focus_label, "FOCUS: -");
  lv_obj_align(focus_label, LV_ALIGN_TOP_MID, 0, 1);
  lv_obj_set_style_text_font(focus_label, lv_theme_get_font_small(NULL), 0);
  lv_obj_set_style_text_color(focus_label, lv_color_hex(0xFFE066), 0);

  /* 1) Button (y=18) */
  ui_btn = lv_btn_create(scr);
  lv_obj_set_size(ui_btn, 96, 20);
  lv_obj_align(ui_btn, LV_ALIGN_TOP_LEFT, 6, 18);
  btn_name = lv_label_create(ui_btn);
  lv_label_set_text(btn_name, "Btn 0");
  lv_obj_center(btn_name);
  /* 焦点态高亮: 2px 黄框 */
  static lv_style_t btn_foc;
  lv_style_init(&btn_foc);
  lv_style_set_border_width(&btn_foc, 2);
  lv_style_set_border_color(&btn_foc, lv_color_hex(0xFFE066));
  lv_obj_add_style(ui_btn, &btn_foc, LV_STATE_FOCUSED);
  lv_obj_add_event_cb(ui_btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_btn, focused_cb, LV_EVENT_FOCUSED, NULL);
  lv_group_add_obj(main_group, ui_btn);

  /* 2) Slider (y=42, LV_KEY_LEFT/RIGHT 内建 +/-1) */
  ui_slider = lv_slider_create(scr);
  lv_obj_set_size(ui_slider, 148, 18);
  lv_obj_align(ui_slider, LV_ALIGN_TOP_MID, 0, 42);
  lv_slider_set_value(ui_slider, 50, LV_ANIM_OFF);
  lv_obj_add_event_cb(ui_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(ui_slider, focused_cb, LV_EVENT_FOCUSED, NULL);
  lv_group_add_obj(main_group, ui_slider);

  /* 3) Checkbox (y=64; LV_KEY_ENTER 切换, LV_KEY_LEFT/DOWN=OFF, RIGHT/UP=ON) */
  ui_check = lv_checkbox_create(scr);
  lv_checkbox_set_text(ui_check, "Check");
  lv_obj_align(ui_check, LV_ALIGN_TOP_LEFT, 6, 64);
  lv_obj_add_event_cb(ui_check, check_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(ui_check, focused_cb, LV_EVENT_FOCUSED, NULL);
  lv_group_add_obj(main_group, ui_check);

  /* 4) List + 3 items (y=88..120, 固定 3 行不滚动; item 可点击) */
  ui_list = lv_list_create(scr);
  lv_obj_set_size(ui_list, 148, 40);
  lv_obj_align(ui_list, LV_ALIGN_TOP_MID, 0, 86);
  /* 保留 SCROLLABLE: 3 个 item 聚焦时 LVGL 自动 scroll_to_view (内建) */
  for (int i = 0; i < 3; i++) {
    char txt[8];
    lv_snprintf(txt, sizeof(txt), "Item%d", i + 1);
    ui_list_items[i] = lv_list_add_btn(ui_list, NULL, txt);
    lv_obj_add_event_cb(ui_list_items[i], list_item_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_list_items[i], focused_cb, LV_EVENT_FOCUSED, NULL);
    lv_group_add_obj(main_group, ui_list_items[i]);
  }

  /* 初始焦点 */
  lv_group_focus_obj(ui_btn);

  Serial.printf("[ui] group ready: btn/slider/check/list(3) in main_group, default set\r\n");
}

/* keypad indev 原先绑定 joy_group; 焦点 UI 就绪后切到 main_group,
 * 这样 indev_keypad_proc 的 i->group 就是 main_group, 键才会分发到控件。 */
void ui_group_indev_bind(void) {
  lv_indev_t *joy = lv_indev_get_next(NULL);
  if (joy == NULL) {
    Serial.printf("[ui] WARN: no indev found\r\n");
    return;
  }
  lv_indev_set_group(joy, main_group);
  Serial.printf("[ui] indev bound to main_group\r\n");
}

void ui_group_tick(void) {
  if (main_group == NULL || focus_label == NULL) return;
  lv_obj_t *f = lv_group_get_focused(main_group);
  lv_label_set_text_fmt(focus_label, "FOCUS: %s", obj_name(f));
}

/* 供 lv_port_indev.cpp probe 回调查询当前焦点 (串口日志用) */
const char *ui_group_focus_name(void) {
  if (main_group == NULL) return "NONE";
  return obj_name(lv_group_get_focused(main_group));
}
