#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

/*
 * LVGL 输入端口: 五向摇杆 (LV_INDEV_TYPE_KEYPAD)
 *
 * 数据链路 (均在 LVGL 主循环上下文, 非 ADC 中断):
 *   LVGL read timer
 *     -> joy_read_cb()            [本文件]
 *     -> key_scan()               [bsp/key_adc.cpp: 滑动平均+死区+去抖]
 *     -> data->key (LV_KEY_*)
 *
 * 支持: 释放 / 长按 / 重复 (由 LVGL keypad indev 内建处理)。
 * indev 的 group 由 ui_group_indev_bind() 切到 main_group (焦点 UI 就绪后)。
 */
void lv_port_indev_init(void);

#endif /* LV_PORT_INDEV_H */
