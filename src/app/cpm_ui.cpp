/* ============================================================
 * cpm_ui.cpp — Cyber HUD 性能监视器 UI (CH32V307 + ST7735 128x160 竖屏 + LVGL 8.3.11)
 *
 * 竖屏分层 HUD:
 *   顶部   : SYSTEM MONITOR 标题栏 (Montserrat 12, 青) + 顶/底分隔线
 *   中部上 : CPU 面板  标题行(CPU + 温度) + 大数字% (HarmonyOS 14) + 进度条
 *   中部下 : GPU 面板  标题行(GPU + 温度) + 大数字% (HarmonyOS 14) + 进度条
 *   底部   : RAM / GPU MEM 次级区 (Montserrat 12 数值 + 细 bar)
 *
 * HUD 元素: 面板边框 + 四角 L 形线条装饰 + 标题栏 + 分隔线 + 大数字突出.
 * 无图片, 无 gif, 动画仅 lv_bar 平滑过渡 (LV_ANIM_ON).
 *
 * 字体 (不新建字体):
 *   - Montserrat 12 (LV_FONT_DEFAULT, 全 ASCII) : 标题/标签/底部数值
 *   - HarmonyOS_2bit 14px : 大数字% / 温度℃ (字库含 0-9 % ℃ 内 存 度 率 利 用)
 *
 * 数据源 cpm_serial_data() (CPM_Data); cpm_ui_update() 每 loop 调用,
 *   memcmp 去抖, 数值不变不重绘 -> 流畅.
 * ============================================================ */
#include "cpm_ui.h"
#include "cpm_serial.h"
#include "ui.h"            /* ui_set_cpm_ui_active */
#include "../bsp/board.h"
#include <string.h>

extern "C" {
#include <lvgl.h>
}

extern const lv_font_t HarmonyOS_2bit;   /* src/app/HarmonyOS_2bit.c (14px) */

/* ---- 主题色 (科技青蓝) ---- */
#define C_BG      0x060a14   /* 深蓝黑背景 */
#define C_CPU     0x00e5ff   /* CPU: 青 */
#define C_GPU     0xff2d95   /* GPU: 品红 */
#define C_RAM     0x00ff9d   /* RAM: 绿 */
#define C_GMEM    0xffea00   /* GPU MEM: 黄 */
#define C_TEXT    0xd6ecff   /* 主文字 */
#define C_DIM     0x5a7a99   /* 次要文字/线 */

/* ---- 布局常量 (128 x 160 竖屏) ---- */
#define W         128
#define H         160
#define FX        3                    /* 面板左边距 */
#define FW        (W - 2 * FX)         /* 面板宽 122 */
#define INX       9                    /* 面板内左边距 */
#define INW       (FW - 12)            /* 面板内宽 110 */

/* 顶部标题栏 */
#define TOP_Y     3
#define TOP_H     13
/* CPU 面板 y=20..79 (60) */
#define CPU_Y     20
#define CPU_H     60
/* GPU 面板 y=83..142 (60) */
#define GPU_Y     83
#define GPU_H     60
/* 底部 y=146..158 (13) */
#define BOT_Y     146

/* ---- bar 颜色: 0=绿 -> 100=红 (RGB565, lv_color_hex 期望 0xRRGGBB) ---- */
static uint32_t calc_color(uint8_t rate) {
  if (rate > 100) rate = 100;
  uint8_t r = (255 * rate) / 100;        /* 0->255 */
  uint8_t g = 255 - r;                   /* 255->0 */
  uint8_t r5 = r >> 3, g5 = g >> 3, b5 = r5; /* 红端蓝也升 */
  return (uint32_t)((uint32_t)r5 << 11 | (uint32_t)g5 << 5 | (uint32_t)b5);
}

/* ---- 主面板句柄 ---- */
typedef struct {
  lv_obj_t *box;       /* 面板容器 (边框) */
  lv_obj_t *title;     /* 面板标题 CPU/GPU */
  lv_obj_t *temp;      /* 温度 */
  lv_obj_t *big;       /* 大数字 % (HarmonyOS) */
  lv_obj_t *bar;       /* 进度条 */
} hud_panel_t;

static hud_panel_t s_cpu, s_gpu;
static lv_obj_t *s_sys_title;
static lv_obj_t *s_ram_lbl, *s_ram_bar;
static lv_obj_t *s_gm_lbl,  *s_gm_bar;

/* 画一条水平/竖直短线 (HUD 线条装饰). w/h 之一为 0 表示退化. */
static lv_obj_t *line_h(lv_obj_t *parent, int x, int y, int w, uint32_t color, int opa) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, w, 1);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, opa, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
static lv_obj_t *line_v(lv_obj_t *parent, int x, int y, int h, uint32_t color, int opa) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, 1, h);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, opa, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

/* 面板四角 L 形装饰 (8 段短线). len = 每段长度. */
static void draw_corners(lv_obj_t *parent, int x0, int y0, int w, int h,
                         uint32_t color, int len) {
  int opa = LV_OPA_90;
  /* 左上 */
  line_h(parent, x0, y0, len, color, opa);
  line_v(parent, x0, y0, len, color, opa);
  /* 右上 */
  line_h(parent, x0 + w - len, y0, len, color, opa);
  line_v(parent, x0 + w, y0, len, color, opa);
  /* 左下 */
  line_h(parent, x0, y0 + h, len, color, opa);
  line_v(parent, x0, y0 + h - len, len, color, opa);
  /* 右下 */
  line_h(parent, x0 + w - len, y0 + h, len, color, opa);
  line_v(parent, x0 + w, y0 + h - len, len, color, opa);
}

/* 建一个主面板 (CPU/GPU). */
static void make_panel(hud_panel_t *p, int py, int ph, uint32_t color) {
  lv_obj_t *scr = lv_scr_act();

  /* 容器边框 */
  p->box = lv_obj_create(scr);
  lv_obj_remove_style_all(p->box);
  lv_obj_set_size(p->box, FW, ph);
  lv_obj_set_pos(p->box, FX, py);
  lv_obj_set_style_border_color(p->box, lv_color_hex(color), 0);
  lv_obj_set_style_border_width(p->box, 1, 0);
  lv_obj_set_style_border_opa(p->box, LV_OPA_40, 0);
  lv_obj_set_style_radius(p->box, 2, 0);
  lv_obj_set_style_bg_color(p->box, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(p->box, LV_OPA_10, 0);
  lv_obj_set_style_pad_all(p->box, 0, 0);
  lv_obj_clear_flag(p->box, LV_OBJ_FLAG_SCROLLABLE);

  draw_corners(scr, FX, py, FW, ph, color, 7);

  /* 标题 (CPU/GPU) — Montserrat 12 */
  p->title = lv_label_create(scr);
  lv_label_set_text(p->title, "");
  lv_obj_set_style_text_font(p->title, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(p->title, lv_color_hex(color), 0);
  lv_obj_set_pos(p->title, INX, py + 4);

  /* 温度 — HarmonyOS (数字 + ℃), 右对齐到面板内右端 */
  p->temp = lv_label_create(scr);
  lv_obj_set_style_text_font(p->temp, &HarmonyOS_2bit, 0);
  lv_obj_set_style_text_color(p->temp, lv_color_hex(C_TEXT), 0);
  lv_obj_align(p->temp, LV_ALIGN_TOP_RIGHT, -(W - INX - INW), py + 4);

  /* 大数字 % — HarmonyOS 14px, 突出 */
  p->big = lv_label_create(scr);
  lv_obj_set_style_text_font(p->big, &HarmonyOS_2bit, 0);
  lv_obj_set_style_text_color(p->big, lv_color_hex(color), 0);
  lv_obj_set_pos(p->big, INX, py + 24);

  /* 进度条 */
  p->bar = lv_bar_create(scr);
  lv_obj_set_size(p->bar, INW, 10);
  lv_obj_set_pos(p->bar, INX, py + ph - 16);
  lv_bar_set_range(p->bar, 0, 100);
  lv_bar_set_value(p->bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(p->bar, lv_color_hex(0x101a2b), LV_PART_MAIN);
  lv_obj_set_style_bg_color(p->bar, lv_color_hex(color), LV_PART_INDICATOR);
  lv_obj_set_style_radius(p->bar, 1, LV_PART_MAIN);
  lv_obj_set_style_pad_all(p->bar, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(p->bar, 0, LV_PART_INDICATOR);
}

void cpm_ui_init(void) {
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

  /* 顶部 SYSTEM MONITOR 标题栏 */
  line_h(lv_scr_act(), 0, TOP_Y, W, C_CPU, LV_OPA_COVER);          /* 顶线 */
  line_h(lv_scr_act(), 0, TOP_Y + TOP_H, W, C_DIM, LV_OPA_60);     /* 分隔线 */
  s_sys_title = lv_label_create(lv_scr_act());
  lv_label_set_text(s_sys_title, "SYSTEM MONITOR");
  lv_obj_set_style_text_font(s_sys_title, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(s_sys_title, lv_color_hex(C_CPU), 0);
  lv_obj_align(s_sys_title, LV_ALIGN_TOP_MID, 0, TOP_Y + 2);

  /* CPU / GPU 主面板 */
  make_panel(&s_cpu, CPU_Y, CPU_H, C_CPU);
  make_panel(&s_gpu, GPU_Y, GPU_H, C_GPU);
  lv_label_set_text(s_cpu.title, "CPU");
  lv_label_set_text(s_gpu.title, "GPU");

  /* 底部 RAM / GPU MEM 次级区 */
  line_h(lv_scr_act(), 0, BOT_Y - 2, W, C_DIM, LV_OPA_40);         /* 底部分隔线 */

  int colw = (W - 8) / 2;                                          /* 每列宽 */
  /* 左列 RAM */
  s_ram_lbl = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(s_ram_lbl, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(s_ram_lbl, lv_color_hex(C_RAM), 0);
  lv_obj_set_pos(s_ram_lbl, 4, BOT_Y);
  s_ram_bar = lv_bar_create(lv_scr_act());
  lv_obj_set_size(s_ram_bar, colw - 6, 2);
  lv_obj_set_pos(s_ram_bar, 4, BOT_Y + 11);
  lv_bar_set_range(s_ram_bar, 0, 100);
  lv_bar_set_value(s_ram_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_ram_bar, lv_color_hex(0x101a2b), LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_ram_bar, lv_color_hex(C_RAM), LV_PART_INDICATOR);
  lv_obj_set_style_radius(s_ram_bar, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_ram_bar, 0, LV_PART_MAIN);

  /* 右列 GPU MEM */
  s_gm_lbl = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(s_gm_lbl, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(s_gm_lbl, lv_color_hex(C_GMEM), 0);
  lv_obj_set_pos(s_gm_lbl, 4 + colw, BOT_Y);
  s_gm_bar = lv_bar_create(lv_scr_act());
  lv_obj_set_size(s_gm_bar, colw - 6, 2);
  lv_obj_set_pos(s_gm_bar, 4 + colw, BOT_Y + 11);
  lv_bar_set_range(s_gm_bar, 0, 100);
  lv_bar_set_value(s_gm_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_gm_bar, lv_color_hex(0x101a2b), LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_gm_bar, lv_color_hex(C_GMEM), LV_PART_INDICATOR);
  lv_obj_set_style_radius(s_gm_bar, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_gm_bar, 0, LV_PART_MAIN);

  /* 初始占位文本 (等首帧数据覆盖) */
  lv_label_set_text_fmt(s_cpu.temp, "%u℃", 0);
  lv_label_set_text_fmt(s_cpu.big, "%u%%", 0);
  lv_label_set_text_fmt(s_gpu.temp, "%u℃", 0);
  lv_label_set_text_fmt(s_gpu.big, "%u%%", 0);
  lv_label_set_text(s_ram_lbl, "RAM 0%");
  lv_label_set_text(s_gm_lbl, "MEM 0%");

  /* 暂停 ui_panel_tick (其控件已被 lv_obj_clean 释放), 保留 perf 串口上报 */
  ui_set_cpm_ui_active(true);
}

void cpm_ui_update(void) {
  const CPM_Data *d = cpm_serial_data();
  if (!d) return;

  static CPM_Data last;
  if (memcmp(d, &last, sizeof(CPM_Data)) == 0) return;   /* 无变化不重绘 */
  last = *d;

  /* CPU 面板 */
  lv_label_set_text_fmt(s_cpu.temp, "%u℃", (int)d->cpu_temp);
  lv_label_set_text_fmt(s_cpu.big, "%u%%", (int)d->cpu_load);
  lv_bar_set_value(s_cpu.bar, d->cpu_load, LV_ANIM_ON);
  lv_obj_set_style_bg_color(s_cpu.bar, lv_color_hex(calc_color(d->cpu_load)), LV_PART_INDICATOR);

  /* GPU 面板 */
  lv_label_set_text_fmt(s_gpu.temp, "%u℃", (int)d->gpu_temp);
  lv_label_set_text_fmt(s_gpu.big, "%u%%", (int)d->gpu_load);
  lv_bar_set_value(s_gpu.bar, d->gpu_load, LV_ANIM_ON);
  lv_obj_set_style_bg_color(s_gpu.bar, lv_color_hex(calc_color(d->gpu_load)), LV_PART_INDICATOR);

  /* 底部 RAM / GPU MEM */
  lv_label_set_text_fmt(s_ram_lbl, "RAM %u%%", (int)d->ram_load);
  lv_bar_set_value(s_ram_bar, d->ram_load, LV_ANIM_ON);
  lv_obj_set_style_bg_color(s_ram_bar, lv_color_hex(calc_color(d->ram_load)), LV_PART_INDICATOR);

  lv_label_set_text_fmt(s_gm_lbl, "MEM %u%%", (int)d->gpu_mem_load);
  lv_bar_set_value(s_gm_bar, d->gpu_mem_load, LV_ANIM_ON);
  lv_obj_set_style_bg_color(s_gm_bar, lv_color_hex(calc_color(d->gpu_mem_load)), LV_PART_INDICATOR);
}
