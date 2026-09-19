#ifndef APP_UI_PANEL_H
#define APP_UI_PANEL_H

#include <stdint.h>

extern "C" {
#include <lvgl.h>
}

/*
 * CH32V307 TFT Control Panel — 主菜单 + 3 子页面 (160x128, Montserrat 12/14)
 *
 * 主菜单布局:
 *   顶部   CH32V307 (title, font14 加粗感)
 *   状态   SPI: 36MHz / FPS: xxxx / RAM: xx KB (3 行, font12)
 *   菜单   Menu:  > Display / > Input / > System (3 行, font12, 焦点黄框高亮)
 *
 * 子页面 (Display/Input/System 共用模板):
 *   标题   < Title
 *   参数   slider 0-100 (LV_KEY_LEFT/RIGHT 内建 +/-1) + bar 镜像 + value 显示
 *   返回   Back 按钮 (LV_KEY_ENTER 点击)
 *
 * 五向键 (复用 lv_port_indev 的 LV_KEY 映射):
 *   UP/DOWN   焦点移动 (LV_KEY_NEXT/PREV, LVGL keypad 内建)
 *   ENTER     点击当前焦点 (LV_KEY_ENTER, 菜单项进子页 / Back 回主菜单)
 *   LEFT/RIGHT 调整 slider (LV_KEY_LEFT/RIGHT, slider 内建 +/-1)
 *
 * 内存: 主菜单对象常驻; 子页面进时 lv_obj_create、回时 lv_obj_clean,
 *   不常驻 -> 主菜单 RAM 不增长。
 */

/* 建主菜单 (title/status/menu items) + menu_group + 初始焦点, 并绑定 indev */
void ui_panel_init(void);

/* 建子页面 (在 parent 上): title + slider + bar + value label + Back
 * 进子页面时调用; 返回主菜单时由内部 back_cb lv_obj_clean(parent) 回收 */
void ui_panel_subpage_build(lv_obj_t *parent, const char *title);

/* loop 每帧调用: 刷新 FPS / RAM 标签 (500ms 节流, 读 perf 缓存 + _sbrk) */
void ui_panel_tick(void);

#endif /* APP_UI_PANEL_H */
