#ifndef LV_PORT_TICK_H
#define LV_PORT_TICK_H

/*
 * LVGL 时钟端口: 由 CH32 SysTick 驱动的 millis() (硬件 1ms 节拍)
 * 每次 loop 调 lv_port_tick_task() 喂增量
 */
void lv_port_tick_task(void);

#endif /* LV_PORT_TICK_H */
