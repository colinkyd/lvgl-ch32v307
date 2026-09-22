#ifndef APP_CPM_UI_H
#define APP_CPM_UI_H

/* ============================================================
 * cpm_ui — Cyber HUD 性能监视器 UI (CH32V307 + ST7735 128x160 竖屏 + LVGL 8.3.11)
 *
 * 科技风 HUD (竖屏分层): 顶部 SYSTEM MONITOR + 分隔线, 中部两个主要面板
 *   (CPU/GPU: 标题栏[名+温] + 大数字% + bar + 四角线条装饰), 底部次区域
 *   (RAM/GPU MEM: 标题+数值% 单行 + 细 bar), 底部装饰线 + PERF REALTIME STABLE.
 *
 * 字体策略 (不生成新字体, 用现有两种):
 *   - ASCII 标题/状态/温度/底部数值: Montserrat 12 (LV_FONT_DEFAULT, 已链接, 全 ASCII)
 *   - 主区域大数字 利用率%: HarmonyOS_2bit 14px (14px 更大更突出, 数字/% 全在字库)
 *
 * 数据源: cpm_serial_data() (CPM_Data). cpm_ui_update() 每 loop 调用,
 *   数值变化时刷新 label/bar (memcmp 去抖, 无变化不重绘 -> 流畅). 动态效果仅 bar 平滑过渡.
 * ============================================================ */

#ifdef __cplusplus
extern "C" {
#endif

/* 创建 Cyber HUD UI (替换当前屏幕内容). 需在 lv_init + disp 驱动注册后调用. */
void cpm_ui_init(void);

/* loop 中每帧调用: 读 CPM_Data, 数值变化时刷新大数字 + bar 目标. 非阻塞.
 * bar 用 LV_ANIM_ON 平滑过渡, label 仅在数值变化时重设 (无变化不重绘). */
void cpm_ui_update(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CPM_UI_H */
