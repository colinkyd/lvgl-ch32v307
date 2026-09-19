#include "ui.h"
#include "../bsp/board.h"

extern "C" {
#include <lvgl.h>
extern void lv_demo_benchmark(void);   /* LVGL 库 src/bm_benchmark.c */
}

/* ---- 统计状态 ---- */
static uint64_t  loop_work_us = 0;      /* loop 内处理累计耗时 (us) */
static uint32_t  window_start_ms = 0;

static void report_stats(uint32_t now) {
  uint32_t win_ms = now - window_start_ms;
  if (win_ms < 1000) return;

  uint32_t cpu_pct = (uint32_t)(loop_work_us * 100UL / (win_ms * 1000UL));
  void *hb = _sbrk(0);
  uint32_t lv_ram   = (uint32_t)hb - (uint32_t)_end;
  uint32_t free_ram = HEAP_END_ADDR - (uint32_t)hb;
  uint32_t up_min = now / 60000, up_sec = (now % 60000) / 1000;

  Serial.printf("[%02lu:%02lu] CPU=%lu%% LV_RAM=%luKB free=%luKB\r\n",
                up_min, up_sec,
                (unsigned long)cpu_pct,
                (unsigned long)lv_ram,
                (unsigned long)free_ram);

  loop_work_us = 0;
  window_start_ms = now;
}

void ui_init(void) {
  lv_demo_benchmark();
  window_start_ms = millis();

  Serial.println("=== LVGL 8.3.11 benchmark (CH32V307) ===");
  Serial.printf("heap: start=%p end=%p total=%luKB\r\n",
                (void *)_end, (void *)HEAP_END_ADDR,
                (unsigned long)(HEAP_TOTAL / 1024));
  Serial.println("Stats every 1s on serial.");
}

void ui_stats_tick(uint32_t frame_work_us) {
  loop_work_us += frame_work_us;
  report_stats(millis());
}
