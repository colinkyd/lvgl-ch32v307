#include "ui.h"
#include "../bsp/board.h"

extern "C" {
#include <lvgl.h>
extern void lv_demo_benchmark(void);   /* LVGL 库 src/bm_benchmark.c */
}

/* ---- CPU 占用: lv_timer_handler 忙时间占比 (millis 分辨率, 1s 窗口) ---- */
static uint32_t  loop_work_ms = 0;
static uint32_t  cpu_win_start = 0;

/* LV_LOG 回调: 把 benchmark 的 Weighted FPS / 场景日志打到串口 */
static void perf_log_cb(const char *buf) {
  Serial.print(buf);
}

void ui_init(void) {
  lv_log_register_print_cb(perf_log_cb);   /* 抓 benchmark 权威 Weighted FPS */
  lv_demo_benchmark();
  cpu_win_start = millis();
  perf_init();

  Serial.println("=== LVGL 8.3.11 benchmark (CH32V307) perf test ===");
  Serial.printf("tag=%s buf_lines=%d spi_req=%luMHz heap_total=%luKB\r\n",
                PERF_TEST_TAG, LV_BUF_LINES,
                (unsigned long)(PERF_SPI_SPEED / 1000000UL),
                (unsigned long)(HEAP_TOTAL / 1024));
  Serial.println("Perf report every 1s. Weighted FPS = benchmark authority.");
}

void ui_stats_tick(uint32_t frame_work_ms) {
  loop_work_ms += frame_work_ms;
  uint32_t now = millis();

  uint32_t win = now - cpu_win_start;
  if (win >= 1000) {
    uint32_t cpu_pct = (uint32_t)((uint64_t)loop_work_ms * 100UL / win);
    void *hb = _sbrk(0);
    uint32_t lv_ram_kb   = ((uint32_t)hb - (uint32_t)_end) / 1024;
    uint32_t free_ram_kb = (HEAP_END_ADDR - (uint32_t)hb) / 1024;
    perf_report(now, cpu_pct, lv_ram_kb, free_ram_kb);
    loop_work_ms = 0;
    cpu_win_start = now;
  }
}
