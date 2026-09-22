#ifndef APP_HUD_PAGE_H
#define APP_HUD_PAGE_H

#include <stdint.h>

/* ============================================================
 * hud_page — 多页面 HUD 框架 (CH32V307 + ST7735 128x160 竖屏 + LVGL 8.3.11)
 *
 * 4 页: MAIN(总览) / CPU(详情) / GPU(详情) / SYSTEM(系统)
 * 页面切换 = lv_obj_clean 重建当前页 (方案: 任一时刻仅 1 页对象常驻,
 *   RAM 峰值 = 单页对象, 比 4 页全建再隐藏/显示 更省 RAM)。
 * 切换仅在按键触发时发生 (非每 loop), 重建开销可忽略。
 *
 * 数据源 cpm_serial_data() (CPM_Data, 不改); hud_page_update() 每 loop 调用,
 *   memcmp 去抖, 数值不变不重绘。
 * 字体: Montserrat 12/14 (ASCII 标签) + HarmonyOS_2bit 14px (数字/℃)。
 * ============================================================ */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PAGE_MAIN = 0,
  PAGE_CPU,
  PAGE_GPU,
  PAGE_SYSTEM
} HUD_PAGE;

#define HUD_PAGE_COUNT 4

/* 初始化: 建 PAGE_MAIN + 底部页脚 (需在 lv_init + disp 驱动 + cpm_serial_init 后) */
void hud_page_init(void);

/* 切页: 仅当 page != 当前页时 clean + 重建. 非阻塞. */
void hud_page_switch(HUD_PAGE page);

/* 当前页 (供页脚/输入层用) */
HUD_PAGE hud_page_current(void);

/* 每 loop 调用: 按当前页刷新数据 (memcmp 去抖, 无变化不重绘) */
void hud_page_update(void);

/* 亮度档位 (0..8, LEFT/RIGHT 预留调节, 页脚显示) */
uint8_t hud_brightness(void);
void hud_brightness_up(void);
void hud_brightness_down(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_HUD_PAGE_H */
