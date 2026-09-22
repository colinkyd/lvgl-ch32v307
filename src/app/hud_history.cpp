/* ============================================================
 * hud_history.cpp — 性能历史数据环形缓存
 *
 * 4 条 60 点序列, 500ms 采样一次 (调用方门控), 环形覆盖最旧点。
 * 数据源 cpm_serial_data() (CPM_Data), 不改通信协议。
 * ============================================================ */
#include "hud_history.h"
#include "cpm_serial.h"
#include <string.h>

static HUD_HISTORY s_hist;
static uint8_t     s_idx = 0;      /* 当前写入索引 0..59 (环形) */

void hud_history_init(void) {
  memset(&s_hist, 0, sizeof(HUD_HISTORY));
  s_idx = 0;
}

const HUD_HISTORY *hud_history_get(void) {
  return &s_hist;
}

uint8_t hud_history_push(void) {
  const CPM_Data *d = cpm_serial_data();
  if (d) {
    s_hist.cpu_load[s_idx] = d->cpu_load;
    s_hist.gpu_load[s_idx] = d->gpu_load;
    s_hist.cpu_temp[s_idx] = d->cpu_temp;
    s_hist.gpu_temp[s_idx] = d->gpu_temp;
  }
  /* 环形: 0..59 后回绕 (覆盖最旧) */
  s_idx = (s_idx + 1) % HUD_HISTORY_LEN;
  return s_idx;
}

void hud_history_latest(uint8_t *cpu_load, uint8_t *gpu_load) {
  const CPM_Data *d = cpm_serial_data();
  if (d) {
    if (cpu_load) *cpu_load = d->cpu_load;
    if (gpu_load) *gpu_load = d->gpu_load;
  } else {
    if (cpu_load) *cpu_load = 0;
    if (gpu_load) *gpu_load = 0;
  }
}
