#include "ui_panel.h"
#include "../../bsp/board.h"
#include "../../bsp/key_adc.h"   /* 边沿事件模型: key_adc_event / key_adc_hold_ms */
#include <Arduino.h>   /* millis() */

/*
 * CH32V307 TFT Control Panel
 *
 * 布局 (160x128):
 *   主菜单:  title "CH32V307" (y1) + 3 状态行 (y18/31/44) + "Menu:" (y57)
 *            + 3 菜单项按钮 (y70/89/108, 18px 高)
 *   子页面:  "< Title" + "Value:" + slider(0-100) + bar 镜像 + hint + Back
 *
 * 五向键 (对照 PC_HUD_Cyber: 边沿事件 + 手动分发, 不靠 LVGL indev):
 *   主菜单  UP/DOWN 移动菜单焦点, SELECT 进子页
 *   子页面  UP/DOWN 在 slider/Back 间移焦点, LEFT/RIGHT 调 slider, SELECT 激活
 *   LVGL group 仅用于焦点高亮 (LV_STATE_FOCUSED 黄框), 焦点切换全手动
 *   (lv_group_focus_obj), 激活走 lv_event_send(CLICKED)。
 */

/* ---- 颜色/样式常量 ---- */
static const uint32_t CLR_ACCENT = 0xFFE066;   /* 黄色: 焦点/高亮 */
static const uint32_t CLR_TITLE  = 0xFFFFFF;   /* 白色标题 */
static const uint32_t CLR_TEXT   = 0xD0D0D0;   /* 浅灰正文 */
static const uint32_t CLR_VAL    = 0x66FF66;   /* 绿色数值 */

/* ---- 主菜单对象 ---- */
static lv_group_t *menu_group    = NULL;
static lv_obj_t   *menu_items[3] = {NULL, NULL, NULL};
static lv_obj_t   *fps_label  = NULL;   /* "FPS: xxxx"  */
static lv_obj_t   *ram_label  = NULL;   /* "RAM: xx/64KB" */

/* ---- 子页面状态 ---- */
static lv_group_t *sub_group   = NULL;
static lv_obj_t   *sub_root    = NULL;    /* 子页面容器 (back 时 lv_obj_del) */
static lv_obj_t   *sub_slider  = NULL;
static lv_obj_t   *sub_back    = NULL;
static lv_obj_t   *sub_bar     = NULL;
static lv_obj_t   *sub_val_lbl = NULL;

/* 焦点黄框样式 (菜单项 / 子页面控件共用) */
static lv_style_t foc_style;
static bool foc_style_ready = false;
static void foc_style_init(void) {
  if (foc_style_ready) return;
  foc_style_ready = true;
  lv_style_init(&foc_style);
  lv_style_set_border_width(&foc_style, 2);
  lv_style_set_border_color(&foc_style, lv_color_hex(CLR_ACCENT));
}

/* ---- FPS 自测 (ui_panel_tick 每 loop 调一次, 1s 窗口 = loop 率 = 有效帧率) ---- */
static uint32_t fps_cnt = 0, fps_win_start = 0, fps_last = 0;
static uint32_t fps_now(void) {
  uint32_t now = millis();
  uint32_t win = now - fps_win_start;
  if (win >= 1000) { fps_last = fps_cnt; fps_cnt = 0; fps_win_start = now; }
  return fps_last;
}

/* 当前 RAM 占用 KB (堆已用) */
static uint32_t ram_used_kb(void) {
  void *hb = _sbrk(0);
  return ((uint32_t)hb - (uint32_t)_end) / 1024;
}

/* ---- indev group 切换 (主菜单 <-> 子页面) ----
 * 注: indev 现已哑火 (总返回 RELEASED), 焦点全手动; 此函数仅保持
 * LVGL indev 的 group 指针指向活跃 group (无害), 便于 LVGL 内部一致性。 */
static void indev_set_group(lv_group_t *g) {
  lv_indev_t *joy = lv_indev_get_next(NULL);
  if (joy != NULL) lv_indev_set_group(joy, g);
}

/* ---- 菜单项: ENTER 点击 -> 进子页面 (由 handle_key 的 lv_event_send 触发) ---- */
static void menu_item_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_obj_t *item = lv_event_get_target(e);
  for (int i = 0; i < 3; i++) {
    if (item == menu_items[i]) {
      const char *names[3] = {"Display", "Input", "System"};
      Serial.printf("[ui] enter subpage: %s\r\n", names[i]);
      ui_panel_subpage_build(lv_scr_act(), names[i]);
      return;
    }
  }
}

/* 菜单项焦点变化 (串口日志, 由 lv_group_focus_obj 触发) */
static void menu_focus_cb(lv_event_t *e) {
  lv_obj_t *o = lv_event_get_target(e);
  for (int i = 0; i < 3; i++)
    if (o == menu_items[i]) Serial.printf("[ui] menu focus -> item%d\r\n", i);
}

/* ---- 子页面: slider 值变化 -> 同步 bar + value 标签 ---- */
static void sub_slider_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  int32_t v = lv_slider_get_value(sub_slider);
  lv_bar_set_value(sub_bar, v, LV_ANIM_OFF);
  lv_label_set_text_fmt(sub_val_lbl, "Value: %d", (int)v);
}

/* ---- 子页面: Back 按钮 ENTER 点击 -> 回主菜单 (由 handle_key 触发) ---- */
static void sub_back_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  Serial.printf("[ui] back -> main menu\r\n");
  lv_obj_del(sub_root);          /* 回收子页面全部对象 (释放 RAM) */
  sub_root = sub_slider = sub_back = sub_bar = sub_val_lbl = NULL;
  indev_set_group(menu_group);   /* 键路由切回主菜单 */
  lv_group_focus_obj(menu_items[0]);
}

/* ================= 手动按键分发 (对照 PC_HUD handle_key) =================
 * 每 loop 取一个边沿事件 (key_adc_event), 按当前页面 + 焦点手动驱动。
 * 主菜单: UP/DOWN 移菜单焦点, SELECT 激活 (进子页)
 * 子页面: UP/DOWN 在 slider/Back 间移焦点, LEFT/RIGHT 调 slider, SELECT 激活 */
static void handle_key(key_t k) {
  if (sub_root == NULL) {
    /* ---------- 主菜单 ---------- */
    switch (k) {
      case KEY_UP:
      case KEY_DOWN: {
        int cur = -1;
        for (int i = 0; i < 3; i++)
          if (menu_items[i] == lv_group_get_focused(menu_group)) cur = i;
        int nxt = (cur < 0) ? 0 : (k == KEY_UP ? (cur + 2) % 3 : (cur + 1) % 3);
        lv_group_focus_obj(menu_items[nxt]);   /* LVGL 自动更新 LV_STATE_FOCUSED 黄框 */
        break;
      }
      case KEY_SELECT: {
        lv_obj_t *f = lv_group_get_focused(menu_group);
        if (f != NULL) lv_event_send(f, LV_EVENT_CLICKED, NULL);  /* menu_item_cb 进子页 */
        break;
      }
      default: break;   /* LEFT/RIGHT 主菜单无动作 */
    }
  } else {
    /* ---------- 子页面 ---------- */
    switch (k) {
      case KEY_UP:
      case KEY_DOWN: {
        lv_obj_t *f = lv_group_get_focused(sub_group);
        lv_group_focus_obj((f == sub_slider) ? sub_back : sub_slider);
        break;
      }
      case KEY_LEFT:
      case KEY_RIGHT: {
        int32_t v = lv_slider_get_value(sub_slider) + (k == KEY_RIGHT ? 1 : -1);
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        lv_slider_set_value(sub_slider, v, LV_ANIM_OFF);   /* 触发 sub_slider_cb 同步 */
        break;
      }
      case KEY_SELECT: {
        lv_obj_t *f = lv_group_get_focused(sub_group);
        if (f != NULL) lv_event_send(f, LV_EVENT_CLICKED, NULL);  /* Back 的 cb 回主菜单 */
        break;
      }
      default: break;
    }
  }
}

/* ================= 主菜单 ================= */
void ui_panel_init(void) {
  foc_style_init();
  lv_obj_t *scr = lv_scr_act();

  /* 背景黑 (disp 已设, 再确保) */
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

  /* 1) 标题 */
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "CH32V307");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 1);
  lv_obj_set_style_text_font(title, lv_theme_get_font_large(NULL), 0);
  lv_obj_set_style_text_color(title, lv_color_hex(CLR_TITLE), 0);

  /* 2) 状态区 (3 行, font12 默认) */
  lv_obj_t *spi_lbl = lv_label_create(scr);
  lv_label_set_text(spi_lbl, "SPI: 36MHz");
  lv_obj_align(spi_lbl, LV_ALIGN_TOP_LEFT, 4, 18);
  lv_obj_set_style_text_color(spi_lbl, lv_color_hex(CLR_TEXT), 0);

  fps_label = lv_label_create(scr);
  lv_label_set_text(fps_label, "FPS: ---");
  lv_obj_align(fps_label, LV_ALIGN_TOP_LEFT, 4, 31);
  lv_obj_set_style_text_color(fps_label, lv_color_hex(CLR_VAL), 0);

  ram_label = lv_label_create(scr);
  lv_label_set_text(ram_label, "RAM: --/64KB");
  lv_obj_align(ram_label, LV_ALIGN_TOP_LEFT, 4, 44);
  lv_obj_set_style_text_color(ram_label, lv_color_hex(CLR_VAL), 0);

  /* 3) Menu: 标签 */
  lv_obj_t *menu_hdr = lv_label_create(scr);
  lv_label_set_text(menu_hdr, "Menu:");
  lv_obj_align(menu_hdr, LV_ALIGN_TOP_LEFT, 4, 57);
  lv_obj_set_style_text_color(menu_hdr, lv_color_hex(CLR_ACCENT), 0);

  /* 4) 3 个菜单项按钮 */
  const char *names[3] = {"> Display", "> Input", "> System"};
  menu_group = lv_group_create();
  lv_group_set_default(menu_group);
  for (int i = 0; i < 3; i++) {
    int y = 70 + i * 19;
    menu_items[i] = lv_btn_create(scr);
    lv_obj_set_size(menu_items[i], 152, 18);
    lv_obj_align(menu_items[i], LV_ALIGN_TOP_LEFT, 4, y);
    lv_obj_add_style(menu_items[i], &foc_style, LV_STATE_FOCUSED);
    lv_obj_t *lbl = lv_label_create(menu_items[i]);
    lv_label_set_text(lbl, names[i]);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(menu_items[i], menu_item_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(menu_items[i], menu_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_group_add_obj(menu_group, menu_items[i]);
  }

  /* 初始焦点 + indev 绑定 (indev 哑火, 焦点手动) */
  lv_group_focus_obj(menu_items[0]);
  indev_set_group(menu_group);

  /* FPS 窗口起算 */
  fps_cnt = 0;
  fps_win_start = millis();

  Serial.printf("[ui] CH32V307 control panel ready (keypad, manual dispatch)\r\n");
}

/* ================= 子页面 ================= */
void ui_panel_subpage_build(lv_obj_t *parent, const char *title) {
  sub_root = lv_obj_create(parent);
  lv_obj_set_size(sub_root, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_align(sub_root, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_pad_all(sub_root, 0, 0);
  lv_obj_set_style_border_width(sub_root, 0, 0);
  lv_obj_clear_flag(sub_root, LV_OBJ_FLAG_SCROLLABLE);
  /* 不透明底盖住主菜单 (子页面独立视觉层) */
  lv_obj_set_style_bg_color(sub_root, lv_color_hex(0x101018), 0);
  lv_obj_set_style_bg_opa(sub_root, LV_OPA_COVER, 0);

  /* 标题 */
  lv_obj_t *t = lv_label_create(sub_root);
  lv_label_set_text_fmt(t, "< %s", title);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 1);
  lv_obj_set_style_text_font(t, lv_theme_get_font_large(NULL), 0);
  lv_obj_set_style_text_color(t, lv_color_hex(CLR_TITLE), 0);

  /* 数值标签 */
  sub_val_lbl = lv_label_create(sub_root);
  lv_label_set_text(sub_val_lbl, "Value: 50");
  lv_obj_align(sub_val_lbl, LV_ALIGN_TOP_LEFT, 8, 24);
  lv_obj_set_style_text_color(sub_val_lbl, lv_color_hex(CLR_VAL), 0);

  /* slider (LEFT/RIGHT 手动调值, 见 handle_key) */
  sub_slider = lv_slider_create(sub_root);
  lv_obj_set_size(sub_slider, 144, 18);
  lv_obj_align(sub_slider, LV_ALIGN_TOP_MID, 0, 42);
  lv_obj_add_style(sub_slider, &foc_style, LV_STATE_FOCUSED);
  lv_slider_set_value(sub_slider, 50, LV_ANIM_OFF);
  lv_obj_add_event_cb(sub_slider, sub_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

  /* bar 镜像 slider */
  sub_bar = lv_bar_create(sub_root);
  lv_obj_set_size(sub_bar, 144, 8);
  lv_obj_align(sub_bar, LV_ALIGN_TOP_MID, 0, 68);
  lv_bar_set_range(sub_bar, 0, 100);
  lv_bar_set_value(sub_bar, 50, LV_ANIM_OFF);

  /* 提示 */
  lv_obj_t *hint = lv_label_create(sub_root);
  lv_label_set_text(hint, "L/R adjust  U/D focus");
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 82);
  lv_obj_set_style_text_color(hint, lv_color_hex(CLR_TEXT), 0);

  /* Back 按钮 */
  sub_back = lv_btn_create(sub_root);
  lv_obj_set_size(sub_back, 60, 20);
  lv_obj_align(sub_back, LV_ALIGN_BOTTOM_MID, 0, -3);
  lv_obj_add_style(sub_back, &foc_style, LV_STATE_FOCUSED);
  lv_obj_t *back_lbl = lv_label_create(sub_back);
  lv_label_set_text(back_lbl, "Back");
  lv_obj_center(back_lbl);
  lv_obj_add_event_cb(sub_back, sub_back_cb, LV_EVENT_CLICKED, NULL);

  /* 子页面 group: slider + back (焦点高亮用) */
  sub_group = lv_group_create();
  lv_group_add_obj(sub_group, sub_slider);
  lv_group_add_obj(sub_group, sub_back);
  indev_set_group(sub_group);
  lv_group_focus_obj(sub_slider);   /* 初始焦点在 slider */
}

/* ================= tick: 按键分发 (每 loop) + 刷新 FPS/RAM (500ms 节流) ================= */
void ui_panel_tick(void) {
  fps_cnt++;   /* 每 loop 一次 -> 1s 窗口 = 有效帧率 */

  /* 按键分发 (每 loop, 对照 PC_HUD: poll -> event -> handle_key) */
  key_adc_poll();                /* 30ms 边沿去抖, 产生 pending */
  key_t k = key_adc_event();     /* 取走一个边沿 */
  if (k != KEY_NONE) handle_key(k);

  static uint32_t last_upd = 0;
  uint32_t now = millis();
  if (now - last_upd < 500) return;
  last_upd = now;

  if (fps_label != NULL)
    lv_label_set_text_fmt(fps_label, "FPS: %lu", (unsigned long)fps_now());
  if (ram_label != NULL)
    lv_label_set_text_fmt(ram_label, "RAM: %lu/%luKB",
                          (unsigned long)ram_used_kb(),
                          (unsigned long)(HEAP_TOTAL / 1024));
}
