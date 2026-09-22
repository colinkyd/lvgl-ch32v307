/* ============================================================
 * cpm_ui.cpp — CPM 性能监视器 UI (160x128 紧凑 4 行布局)
 *
 * 移植自 RP2040 cpm_rp2040/rtos_task.c 的 UI 创建 + 数据刷新逻辑:
 *   - 保留: lv_label_set_text_fmt / lv_bar_set_value / calc_color
 *   - 删除: ST7789 寄存器 / PIO / DMA / FreeRTOS 队列 (改用 cpm_serial 超循环)
 *   - 字体: HarmonyOS_2bit (14px, 2bpp, 本目录)
 *
 * 布局 (对齐 RP2040 目标样式, 4 内容行, 160x128):
 *   行1  CPU温度+利用率  -> "CPU45C60%"   + bar(cpu_load)
 *   行2  RAM利用率        -> "RAM50%"       + bar(ram_load)
 *   行3  GPU温度+利用率   -> "GPU45C60%"   + bar(gpu_load)
 *   行4  GPU显存利用率    -> "GPU内存45%"   + bar(gpu_mem_load)
 *
 * 字符集约束 (HarmonyOS_2bit 实际含):
 *   ASCII: 空格 A B C G M P R U 0-9 % / :
 *   CJK:   ℃ 内 利 存 度 率 用 （ ） ：
 *   缺: V T D E S L O H K F Q W X Y Z N I J; 中文 显 能 主 频 速 等.
 *   => "VRAM" 的 V 缺 -> 用 "GPU内存" 替代.
 *   行文本用无空格短格式, 实测宽度: CPU45C60%=144 / RAM50%=96 /
 *   GPU45C60%=144 / GPU内存45%=152 px, 均 <= 152px 可用宽, 不换行.
 *   3 位温度 (100C) 会超 8px -> LV_LABEL_LONG_SCROLL 兜底横滚.
 * ============================================================ */
#include "cpm_ui.h"
#include "cpm_serial.h"
#include "ui.h"            /* ui_set_cpm_ui_active */
#include "../bsp/board.h"
#include <string.h>   /* memcmp */

extern "C" {
#include <lvgl.h>
}

extern const lv_font_t HarmonyOS_2bit;   /* src/app/HarmonyOS_2bit.c */

/* ---- 布局常量 (160x128) ---- */
#define PAD         4
#define ROW_W       (LCD_WIDTH - 2 * PAD)   /* 152 */
#define LABEL_H     15
#define BAR_H       10
#define ROW_GAP     5
#define ROW_H       (LABEL_H + BAR_H + ROW_GAP)   /* 30 */
#define TOP         8
#define N_ROWS      4
#define ROW_X       PAD

/* 中文字体测试模式: 1 = update 不跟随数据, 4 行轮播显示
 * "CPU温度" / "GPU利用率" / "RAM" (bar 用固定演示值), 用于人工核对缺字/乱码.
 * 正式数据展示时改回 0. */
#ifndef CPM_UI_TEST
#define CPM_UI_TEST 0
#endif

/* bar 颜色: 0=绿 -> 100=红. 移植自 RP2040 calc_color.
 * 返回 0xRRGGBB (调用方用 lv_color_hex 转 RGB565).
 * g 随 rate 从 255 降到 0 (原版 g += (0-g)/100*rate);
 * 若 g 恒 255, 全段都是绿/黄, 看不出红端. */
static uint32_t calc_color(uint8_t rate) {
  uint8_t r = 0, g = 255, b = 0;
  r += (255 - r) / 100 * rate;
  g += (0 - g) / 100 * rate;
  b += (0 - b) / 100 * rate;
  return (uint32_t)((uint32_t)r << 16 | (uint32_t)g << 8 | b);
}

/* 行句柄: label (名称+数值, 单行) + bar */
typedef struct {
  lv_obj_t *lbl;
  lv_obj_t *bar;
} cpm_row_t;

static cpm_row_t s_rows[N_ROWS];

void cpm_ui_init(void) {
  lv_obj_clean(lv_scr_act());

  /* 暂停 ui_panel_tick (其控件已被 lv_obj_clean 释放), 保留 perf 串口上报 */
  ui_set_cpm_ui_active(true);

  /* 全屏深色背景 */
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x1b1b1b), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

  for (int i = 0; i < N_ROWS; i++) {
    int y = TOP + i * ROW_H;

    /* 行文本 label: 单行, 超宽横滚 (不换行溢到 bar) */
    lv_obj_t *lbl = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(lbl, &HarmonyOS_2bit, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL);
    lv_obj_set_size(lbl, ROW_W, LABEL_H);
    lv_obj_set_pos(lbl, ROW_X, y);
    s_rows[i].lbl = lbl;

    /* bar: 量程 0-100, 满宽 */
    lv_obj_t *bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(bar, ROW_W, BAR_H);
    lv_obj_set_pos(bar, ROW_X, y + LABEL_H);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x262626), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x303030), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    s_rows[i].bar = bar;
  }

  /* 初始文本: 测试模式直接放 4 个中文字体测试串 (首帧即显示);
   * 数据模式放纯名称占位 (等首帧数据覆盖). 只用字体内已有字符. */
#if CPM_UI_TEST
  lv_label_set_text(s_rows[0].lbl, "CPU温度");
  lv_label_set_text(s_rows[1].lbl, "GPU利用率");
  lv_label_set_text(s_rows[2].lbl, "RAM");
  lv_label_set_text(s_rows[3].lbl, "GPU内存");
  static const uint8_t init_bar[N_ROWS] = {25, 50, 75, 95};
  for (int i = 0; i < N_ROWS; i++) {
    lv_bar_set_value(s_rows[i].bar, init_bar[i], LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_rows[i].bar, lv_color_hex(calc_color(init_bar[i])), LV_PART_INDICATOR);
  }
#else
  lv_label_set_text(s_rows[0].lbl, "CPU");
  lv_label_set_text(s_rows[1].lbl, "RAM");
  lv_label_set_text(s_rows[2].lbl, "GPU");
  lv_label_set_text(s_rows[3].lbl, "GPU内存");
#endif
}

void cpm_ui_update(void) {
#if CPM_UI_TEST
  /* ---- 中文字体测试模式: 不跟随数据, 每帧轮播 4 行测试串 ----
   * 任务要求显示: CPU温度 / GPU利用率 / RAM.
   * 字符覆盖核对 (HarmonyOS_2bit unicode_list_0, range_start=0x20):
   *   CPU温度 = C P U 温 度   -> 全部在字体内
   *   GPU利用率 = G P U 利 用 率 -> 全部在字体内
   *   RAM       = R A M       -> 全部在字体内
   * 3 个测试串无缺字; 缺字记录见 cpm_ui_migration_report.md §3. */
  /* 4 行同时静态显示 (不轮播): 每行一个测试串 + 演示 bar,
   * 一眼核对所有中文字形 + calc_color 渐变. 1s 重设一次文本 (幂等, 防外部改动). */
  static uint32_t last_ms = 0;
  static const char *test_str[N_ROWS] = {"CPU温度", "GPU利用率", "RAM", "GPU内存"};
  static const uint8_t test_bar[N_ROWS] = {25, 50, 75, 95};
  if (millis() - last_ms >= 1000) {
    last_ms = millis();
    for (int i = 0; i < N_ROWS; i++) {
      lv_label_set_text(s_rows[i].lbl, test_str[i]);
      lv_bar_set_value(s_rows[i].bar, test_bar[i], LV_ANIM_OFF);
      lv_obj_set_style_bg_color(s_rows[i].bar, lv_color_hex(calc_color(test_bar[i])), LV_PART_INDICATOR);
    }
  }
#else
  const CPM_Data *d = cpm_serial_data();
  if (!d) return;

  static CPM_Data last;
  /* 数值无变化 -> 不重绘 (保持流畅, 省 SPI flush) */
  if (memcmp(d, &last, sizeof(CPM_Data)) == 0) return;
  last = *d;

  /* 行1: CPU 温度 + 利用率. bar = cpu_load */
  lv_label_set_text_fmt(s_rows[0].lbl, "CPU%uC%u%%",
                        (int)d->cpu_temp, (int)d->cpu_load);
  lv_bar_set_value(s_rows[0].bar, d->cpu_load, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_rows[0].bar, lv_color_hex(calc_color(d->cpu_load)), LV_PART_INDICATOR);

  /* 行2: RAM 利用率. bar = ram_load. (ram_used/ram_total 在 CPM_Data, 宽度所限暂不显示) */
  lv_label_set_text_fmt(s_rows[1].lbl, "RAM%u%%", (int)d->ram_load);
  lv_bar_set_value(s_rows[1].bar, d->ram_load, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_rows[1].bar, lv_color_hex(calc_color(d->ram_load)), LV_PART_INDICATOR);

  /* 行3: GPU 温度 + 利用率. bar = gpu_load */
  lv_label_set_text_fmt(s_rows[2].lbl, "GPU%uC%u%%",
                        (int)d->gpu_temp, (int)d->gpu_load);
  lv_bar_set_value(s_rows[2].bar, d->gpu_load, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_rows[2].bar, lv_color_hex(calc_color(d->gpu_load)), LV_PART_INDICATOR);

  /* 行4: GPU 显存利用率 (显存; 用"内存"避"显"缺字). bar = gpu_mem_load */
  lv_label_set_text_fmt(s_rows[3].lbl, "GPU内存%u%%", (int)d->gpu_mem_load);
  lv_bar_set_value(s_rows[3].bar, d->gpu_mem_load, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_rows[3].bar, lv_color_hex(calc_color(d->gpu_mem_load)), LV_PART_INDICATOR);
#endif /* CPM_UI_TEST */
}
