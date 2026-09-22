#ifndef APP_HUD_GRAPH_H
#define APP_HUD_GRAPH_H

#include "lvgl.h"
#include "hud_history.h"

/* ============================================================
 * hud_graph — 实时历史曲线 (lv_chart, 示波器滚动)
 *
 * 单一 chart 对象长期存在 (本页存活期间不删不重建, 切页时随 screen clean 销毁)。
 * 模式: LV_CHART_UPDATE_MODE_SHIFT — 新数据入右, 旧数据整体左移 (示波器滚动)。
 *   (CIRCULAR 只原地覆盖不左移, 填满后像"停在第一屏" — 本修复的核心)
 * 数据流: 500ms tick → 读 CPM_Data → hud_history_push() → lv_chart_set_next_value()。
 *   禁止 set_all_value 每帧刷新 (整屏重绘); 仅初始化时用一次。
 * 不改 cpm_serial / PC 协议 / CPM_Data。
 * ============================================================ */

#ifdef __cplusplus
extern "C" {
#endif

/* 在 parent (当前 screen) 下创建 chart: 60 点, Y 0-100, CPU/GPU 双线,
 * SHIFT 滚动, 所有点初始化为当前值 (第一屏平铺, 随后 SHIFT 滚动)。 */
void hud_graph_create(lv_obj_t * parent);

/* 每 loop 调用 (仅 GRAPH 页): 内部 500ms 门控,
 * 到点则 hud_history_push + lv_chart_set_next_value (右入, 自动左移)。非阻塞。 */
void hud_graph_tick(void);

/* 切离本页时调用: 置空内部指针 (防悬空); 对象本身已由 screen clean 销毁。 */
void hud_graph_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_HUD_GRAPH_H */
