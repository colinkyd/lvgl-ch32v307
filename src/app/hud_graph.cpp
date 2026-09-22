/* ============================================================
 * hud_graph.cpp — 实时历史曲线 (lv_chart, 示波器滚动)
 *
 * SHIFT 模式: 新点入右, 旧点整体左移。单一 chart 对象长期存在。
 * 500ms 门控采样 (非阻塞), 数据流:
 *   500ms tick → CPM_Data.cpu_load/gpu_load → hud_history_push()
 *              → lv_chart_set_next_value() (LVGL 自动 shift 左移)
 * 不改 cpm_serial / PC 协议 / CPM_Data。
 * ============================================================ */
#include "hud_graph.h"
#include "cpm_serial.h"
#include "../bsp/board.h"   /* millis(), W */

/* 颜色: CPU 青 / GPU 品红 (与 HUD 主题一致, 一色一条, 无图片) */
#define GRAPH_C_CPU  0x00e5ff
#define GRAPH_C_GPU  0xff2d95

static lv_obj_t           *s_chart;
static lv_chart_series_t  *s_ser_cpu;
static lv_chart_series_t  *s_ser_gpu;
static uint32_t            s_last_tick;   /* 上次采样时间 (500ms 门控) */

void hud_graph_create(lv_obj_t *parent) {
  s_chart = lv_chart_create(parent);
  lv_obj_set_size(s_chart, LCD_WIDTH, 88);
  lv_obj_align(s_chart, LV_ALIGN_TOP_MID, 0, 40);
  lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(s_chart, HUD_HISTORY_LEN);
  lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_div_line_count(s_chart, 2, 0);
  /* 示波器滚动: SHIFT (非 CIRCULAR — CIRCULAR 填满后不左移) */
  lv_chart_set_update_mode(s_chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_obj_set_style_bg_color(s_chart, lv_color_hex(0x0a111f), 0);
  lv_obj_set_style_border_width(s_chart, 0, 0);   /* 去白框 */
  lv_obj_set_style_pad_all(s_chart, 0, 0);
  lv_obj_set_style_line_width(s_chart, 2, LV_PART_MAIN);   /* 折线宽 */
  lv_obj_set_style_size(s_chart, 2, LV_PART_INDICATOR);    /* 数据点 */

  s_ser_cpu = lv_chart_add_series(s_chart, lv_color_hex(GRAPH_C_CPU), LV_CHART_AXIS_PRIMARY_Y);
  s_ser_gpu = lv_chart_add_series(s_chart, lv_color_hex(GRAPH_C_GPU), LV_CHART_AXIS_PRIMARY_Y);

  /* 初始化: 所有点平铺当前值 (第一屏, 随后 SHIFT 右入左移) */
  const CPM_Data *d = cpm_serial_data();
  uint16_t cpu0 = d ? d->cpu_load : 0;
  uint16_t gpu0 = d ? d->gpu_load : 0;
  lv_chart_set_all_value(s_chart, s_ser_cpu, cpu0);   /* 仅初始化用一次, 非每帧 */
  lv_chart_set_all_value(s_chart, s_ser_gpu, gpu0);

  s_last_tick = millis();
}

void hud_graph_tick(void) {
  if (!s_chart || !s_ser_cpu || !s_ser_gpu) return;
  uint32_t now = millis();
  if (now - s_last_tick < 500) return;
  s_last_tick = now;

  const CPM_Data *d = cpm_serial_data();
  if (!d) return;

  hud_history_push();                       /* 历史环形缓存记录 */
  lv_chart_set_next_value(s_chart, s_ser_cpu, d->cpu_load);  /* 右入, SHIFT 左移 */
  lv_chart_set_next_value(s_chart, s_ser_gpu, d->gpu_load);
}

void hud_graph_destroy(void) {
  /* chart 对象随 screen clean 销毁, 此处仅清指针防悬空 */
  s_chart   = NULL;
  s_ser_cpu = NULL;
  s_ser_gpu = NULL;
}
