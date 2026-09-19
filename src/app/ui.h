#ifndef APP_UI_H
#define APP_UI_H

#include <stdint.h>

/* 应用层: 启动 benchmark demo + 周期性统计 (串口) */
void ui_init(void);

/* loop 中每帧调用: 传入本帧处理耗时 (us), 内部按 1s 窗口上报 CPU/RAM */
void ui_stats_tick(uint32_t frame_work_us);

#endif /* APP_UI_H */
