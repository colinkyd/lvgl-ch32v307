#ifndef APP_UI_GROUP_H
#define APP_UI_GROUP_H

/*
 * LVGL keypad focus 系统 (五向键驱动 UI 焦点, 无触摸)
 *
 * main_group 成员: Button -> Slider -> Checkbox -> List(item1..3)
 *   UP/DOWN   : 切换焦点 (indev 层映射为 LV_KEY_NEXT/PREV)
 *   ENTER     : 点击当前焦点对象
 *   LEFT/RIGHT: 调整 Slider (LVGL slider 内建 LV_KEY_LEFT/RIGHT +/-1)
 *   屏幕顶部显示当前 focus 对象名
 */

/* benchmark finished_cb 签名 void(void): benchmark 跑完清屏后异步构建焦点 UI */
void ui_group_init(void);

/* loop 每帧调用: 刷新焦点显示 (label + 串口日志) */
void ui_group_tick(void);

/* 把 keypad indev 的 group 切到 main_group (ui_group_init 之后调用) */
void ui_group_indev_bind(void);

/* 当前焦点对象名 (供 lv_port_indev probe 回调查询, 串口日志用) */
const char *ui_group_focus_name(void);

#endif /* APP_UI_GROUP_H */
