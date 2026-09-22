#ifndef APP_UI_H
#define APP_UI_H

#include <stdint.h>
#include <stdbool.h>

/* 应用层: 启动 benchmark demo + 周期性统计 (串口) */
void ui_init(void);

/* loop 中每帧调用: 传入本帧处理耗时 (us), 内部按 1s 窗口上报 CPU/RAM */
void ui_stats_tick(uint32_t frame_work_us);

/* CPM 性能监视器 UI 接管主屏后调用 (active=true):
 * ui_panel 的控件已被 cpm_ui_init 的 lv_obj_clean 释放, 暂停其 tick
 * (FPS/RAM 标签 + 按键分发), 避免悬空指针. perf 串口上报不受影响. */
void ui_set_cpm_ui_active(bool active);

#endif /* APP_UI_H */
