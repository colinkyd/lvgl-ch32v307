#ifndef APP_HUD_HISTORY_H
#define APP_HUD_HISTORY_H

#include <stdint.h>

/* ============================================================
 * hud_history — 性能历史数据环形缓存 (CH32V307 + LVGL 8.3.11)
 *
 * 缓存 4 条 60 点序列: CPU load / GPU load / CPU temp / GPU temp。
 * 500ms 采样一次 (非阻塞, 调用方在 loop 里用 millis 门控)。
 * 环形写入: 新数据覆盖最旧点, 索引从 0 递增到 59 后回绕。
 *
 * 数据源: cpm_serial_data() (CPM_Data)。本模块不改 PC 通信协议。
 * ============================================================ */

#ifdef __cplusplus
extern "C" {
#endif

#define HUD_HISTORY_LEN 60   /* 采样点个数 (60 * 500ms = 30s 窗口) */

typedef struct {
  uint8_t cpu_load[HUD_HISTORY_LEN];
  uint8_t gpu_load[HUD_HISTORY_LEN];
  uint8_t cpu_temp[HUD_HISTORY_LEN];
  uint8_t gpu_temp[HUD_HISTORY_LEN];
} HUD_HISTORY;

/* 初始化: 清空 + 索引归零 */
void hud_history_init(void);

/* 取历史缓冲 (只读) */
const HUD_HISTORY *hud_history_get(void);

/* 采样一次: 从 cpm_serial_data() 抓当前值写入最旧点 (环形覆盖)。
 * 非阻塞: 仅 4 次赋值 + 索引递增, 无分配。
 * @return 当前写入索引 (0..59) */
uint8_t hud_history_push(void);

/* 最新一帧的值 (用于底部"当前: CPU xx% GPU xx%"显示) */
void hud_history_latest(uint8_t *cpu_load, uint8_t *gpu_load);

#ifdef __cplusplus
}
#endif

#endif /* APP_HUD_HISTORY_H */
