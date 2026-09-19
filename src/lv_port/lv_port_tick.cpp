#include "lv_port_tick.h"
#include <Arduino.h>

extern "C" {
#include <lvgl.h>
}

static uint32_t last_ms = 0;

void lv_port_tick_task(void) {
  /* millis() 由 CH32 SysTick 硬件 1kHz 驱动, 增量喂给 LVGL */
  uint32_t now = millis();
  lv_tick_inc(now - last_ms);
  last_ms = now;
}
