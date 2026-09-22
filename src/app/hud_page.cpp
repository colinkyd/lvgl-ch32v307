/* ============================================================
 * hud_page.cpp — 多页面 HUD 框架 (CH32V307 + ST7735 128x160 竖屏 + LVGL 8.3.11)
 *
 * 4 页: MAIN(总览) / CPU(详情) / GPU(详情) / SYSTEM(系统)
 * 方案: lv_obj_clean + 重建当前页 (任一时刻仅 1 页常驻, RAM 峰值最小)。
 * 切换仅在按键触发 (非每 loop), 重建开销 ~200 个 LV 对象 < 5ms。
 *
 * 数据源 cpm_serial_data() (CPM_Data); hud_page_update() 每 loop,
 *   memcmp 去抖, 数值不变不重绘。
 *
 * 字体: Montserrat 12 (ASCII) + HarmonyOS_2bit 14px (数字/℃/中文)。
 * 视觉风格: 与 cpm_ui (Cyber HUD v1) 一致 — 科技青蓝 + 四角装饰 + 面板边框。
 * ============================================================ */
#include "hud_page.h"
#include "cpm_serial.h"
#include "cpm_ui.h"       /* PAGE_MAIN 复用 cpm_ui_init/cpm_ui_update */
#include "ui.h"           /* ui_set_cpm_ui_active */
#include "hud_history.h"  /* 性能历史环形缓存 (GRAPH 页) */
#include "hud_graph.h"    /* 实时曲线 lv_chart 封装 (SHIFT 滚动) */
#include "../bsp/board.h"
#include <string.h>

extern "C" {
#include <lvgl.h>
}

extern const lv_font_t HarmonyOS_2bit;   /* src/app/HarmonyOS_2bit.c (14px) */

/* ---- 主题色 (与 cpm_ui 一致) ---- */
#define C_BG      0x060a14
#define C_CPU     0x00e5ff
#define C_GPU     0xff2d95
#define C_RAM     0x00ff9d
#define C_GMEM    0xffea00
#define C_TEXT    0xd6ecff
#define C_DIM     0x5a7a99
#define C_SYS     0xaaaaaa

#define W 128
#define H 160

/* ---- bar 颜色: 0=绿 → 100=红 (RGB565 as 0xRRGGBB for lv_color_hex) ---- */
static uint32_t calc_color(uint8_t rate) {
  if (rate > 100) rate = 100;
  uint8_t r = (255 * rate) / 100;
  uint8_t g = 255 - r;
  uint8_t r5 = r >> 3, g5 = g >> 3, b5 = r5;
  return (uint32_t)((uint32_t)r5 << 11 | (uint32_t)g5 << 5 | (uint32_t)b5);
}

/* ---- 辅助: 水平/竖直短线 ---- */
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

/* 四角 L 形装饰 */
static void draw_corners(lv_obj_t *parent, int x0, int y0, int w, int h,
                         uint32_t color, int len) {
  int opa = LV_OPA_90;
  line_h(parent, x0, y0, len, color, opa);
  line_v(parent, x0, y0, len, color, opa);
  line_h(parent, x0 + w - len, y0, len, color, opa);
  line_v(parent, x0 + w, y0, len, color, opa);
  line_h(parent, x0, y0 + h, len, color, opa);
  line_v(parent, x0, y0 + h - len, len, color, opa);
  line_h(parent, x0 + w - len, y0 + h, len, color, opa);
  line_v(parent, x0 + w, y0 + h - len, len, color, opa);
}

/* ---- 详情页通用行: 标签(左) + 大数字(中) + 单位(右) + 可选 bar ---- */
typedef struct {
  lv_obj_t *val;   /* 大数字 label */
  lv_obj_t *bar;   /* 可选 bar (NULL 如果无) */
} metric_row_t;

#define METRIC_ROWS 4
static metric_row_t s_rows[METRIC_ROWS];
static lv_obj_t *s_title_lbl;   /* 页面标题 */
static uint8_t  s_row_count = 0;
static uint32_t s_row_colors[METRIC_ROWS];  /* 每行主题色 (bar 用) */

/* 建一行 metric (y = 行起始 y)。有 bar 则 bar 在 y+22, 无 bar 则紧凑。 */
static void add_metric_row(const char *label, int y, uint32_t color, bool with_bar) {
  lv_obj_t *scr = lv_scr_act();
  s_row_count++;

  /* 标签 (Montserrat 12) — 左上对齐 */
  lv_obj_t *lbl = lv_label_create(scr);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, y);

  /* 大数字 (HarmonyOS 14px) — 右上对齐 (右边界留 8px), 与左标签绝不水平重叠 */
  s_rows[s_row_count - 1].val = lv_label_create(scr);
  lv_obj_set_style_text_font(s_rows[s_row_count - 1].val, &HarmonyOS_2bit, 0);
  lv_obj_set_style_text_color(s_rows[s_row_count - 1].val, lv_color_hex(C_TEXT), 0);
  lv_label_set_text(s_rows[s_row_count - 1].val, "0");
  lv_obj_align(s_rows[s_row_count - 1].val, LV_ALIGN_TOP_RIGHT, -8, y);

  /* bar (可选) */
  if (with_bar) {
    s_rows[s_row_count - 1].bar = lv_bar_create(scr);
    lv_obj_set_size(s_rows[s_row_count - 1].bar, 110, 6);
    lv_obj_set_pos(s_rows[s_row_count - 1].bar, 8, y + 20);
    lv_bar_set_range(s_rows[s_row_count - 1].bar, 0, 100);
    lv_bar_set_value(s_rows[s_row_count - 1].bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_rows[s_row_count - 1].bar, lv_color_hex(0x101a2b), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_rows[s_row_count - 1].bar, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_rows[s_row_count - 1].bar, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_rows[s_row_count - 1].bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_rows[s_row_count - 1].bar, 0, LV_PART_INDICATOR);
  } else {
    s_rows[s_row_count - 1].bar = NULL;
  }
  s_row_colors[s_row_count - 1] = color;
}

/* ---- 页面构建 ---- */

/* PAGE_CPU: CPU 详情 */
static void build_page_cpu(void) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  s_row_count = 0;

  /* 标题栏 */
  line_h(scr, 0, 2, W, C_CPU, LV_OPA_COVER);
  s_title_lbl = lv_label_create(scr);
  lv_label_set_text(s_title_lbl, "CPU DETAIL");
  lv_obj_set_style_text_color(s_title_lbl, lv_color_hex(C_CPU), 0);
  lv_obj_align(s_title_lbl, LV_ALIGN_TOP_MID, 0, 5);

  /* 4 行 metric: CPU TEMP / CPU LOAD / RAM LOAD / CPU FREQ(预留) */
  add_metric_row("CPU TEMP",  26, C_CPU,  false);
  add_metric_row("CPU LOAD",  58, C_CPU,  true);
  add_metric_row("RAM LOAD",  90, C_RAM,  true);
  add_metric_row("CPU FREQ", 122, C_DIM,  false);   /* 预留: 显示 "----" */

  /* 底部页脚 */
  line_h(scr, 0, H - 4, W, C_DIM, LV_OPA_40);
}

/* PAGE_GPU: GPU 详情 */
static void build_page_gpu(void) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  s_row_count = 0;

  line_h(scr, 0, 2, W, C_GPU, LV_OPA_COVER);
  s_title_lbl = lv_label_create(scr);
  lv_label_set_text(s_title_lbl, "GPU DETAIL");
  lv_obj_set_style_text_color(s_title_lbl, lv_color_hex(C_GPU), 0);
  lv_obj_align(s_title_lbl, LV_ALIGN_TOP_MID, 0, 5);

  add_metric_row("GPU TEMP",  26, C_GPU,  false);
  add_metric_row("GPU LOAD",  58, C_GPU,  true);
  add_metric_row("GPU MEM",   90, C_GMEM, true);
  add_metric_row("MEM SIZE", 122, C_DIM,  false);   /* 预留: 显存容量 GB */

  line_h(scr, 0, H - 4, W, C_DIM, LV_OPA_40);
}

/* PAGE_SYSTEM: 系统信息 (静态, 不刷新数据) */
static void build_page_system(void) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  s_row_count = 0;

  line_h(scr, 0, 2, W, C_SYS, LV_OPA_COVER);
  s_title_lbl = lv_label_create(scr);
  lv_label_set_text(s_title_lbl, "SYSTEM");
  lv_obj_set_style_text_color(s_title_lbl, lv_color_hex(C_SYS), 0);
  lv_obj_align(s_title_lbl, LV_ALIGN_TOP_MID, 0, 5);

  /* 3 行静态信息 (用 Montserrat 12, 非 HarmonyOS) */
  struct { const char *label; const char *value; int y; } items[] = {
    { "DEVICE",  "CH32V307", 30 },
    { "COMMS",   "OK",       70 },
    { "VERSION", "HUD V1",  110 },
  };
  for (int i = 0; i < 3; i++) {
    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, items[i].label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(C_SYS), 0);
    lv_obj_set_pos(lbl, 8, items[i].y);

    lv_obj_t *val = lv_label_create(scr);
    lv_label_set_text(val, items[i].value);
    lv_obj_set_style_text_color(val, lv_color_hex(C_TEXT), 0);
    lv_obj_set_pos(val, 60, items[i].y);
  }

  /* 装饰: 四角 */
  draw_corners(scr, 4, 10, W - 8, H - 20, C_SYS, 6);

  line_h(scr, 0, H - 4, W, C_DIM, LV_OPA_40);
}

/* build_footer 定义在后部, 供 build_page_graph 调用前可见 */
static void build_footer(void);

/* ---- PAGE_GRAPH: 实时历史曲线 (chart 封装在 hud_graph, 此页只搭布局) ---- */
static lv_obj_t   *s_graph_cur; /* 底部 "CPU xx% GPU xx%" 标签 */

static void graph_update_current(void) {
  if (!s_graph_cur) return;
  uint8_t cl, gl;
  hud_history_latest(&cl, &gl);
  lv_label_set_text_fmt(s_graph_cur, "CPU %d%% GPU %d%%", (int)cl, (int)gl);
}

static void build_page_graph(void) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  s_row_count = 0;

  /* 标题栏 */
  line_h(scr, 0, 2, W, C_TEXT, LV_OPA_COVER);
  s_title_lbl = lv_label_create(scr);
  lv_label_set_text(s_title_lbl, "PERFORMANCE GRAPH");
  lv_obj_set_style_text_color(s_title_lbl, lv_color_hex(C_TEXT), 0);
  lv_obj_align(s_title_lbl, LV_ALIGN_TOP_MID, 0, 5);

  /* 图例: 色块 + CPU/GPU 标签 (删除 "CPU / GPU LOAD" 文字, 避免与白框/底部重叠) */
  lv_obj_t *sw_cpu = lv_obj_create(scr);
  lv_obj_remove_style_all(sw_cpu);
  lv_obj_set_size(sw_cpu, 8, 8);
  lv_obj_align(sw_cpu, LV_ALIGN_TOP_LEFT, 8, 26);
  lv_obj_set_style_bg_color(sw_cpu, lv_color_hex(C_CPU), 0);
  lv_obj_t *lg_cpu = lv_label_create(scr);
  lv_label_set_text(lg_cpu, "CPU");
  lv_obj_set_style_text_color(lg_cpu, lv_color_hex(C_CPU), 0);
  lv_obj_align(lg_cpu, LV_ALIGN_TOP_LEFT, 20, 25);

  lv_obj_t *sw_gpu = lv_obj_create(scr);
  lv_obj_remove_style_all(sw_gpu);
  lv_obj_set_size(sw_gpu, 8, 8);
  lv_obj_align(sw_gpu, LV_ALIGN_TOP_LEFT, 48, 26);
  lv_obj_set_style_bg_color(sw_gpu, lv_color_hex(C_GPU), 0);
  lv_obj_t *lg_gpu = lv_label_create(scr);
  lv_label_set_text(lg_gpu, "GPU");
  lv_obj_set_style_text_color(lg_gpu, lv_color_hex(C_GPU), 0);
  lv_obj_align(lg_gpu, LV_ALIGN_TOP_LEFT, 60, 25);

  /* chart: hud_graph 封装 (SHIFT 示波器滚动, 60 点, 双线, 初始化平铺当前值) */
  hud_graph_create(scr);

  /* 底部当前值 (下移, 避开 chart 底边 y=128) */
  s_graph_cur = lv_label_create(scr);
  lv_obj_set_style_text_color(s_graph_cur, lv_color_hex(C_TEXT), 0);
  lv_obj_align(s_graph_cur, LV_ALIGN_BOTTOM_MID, 0, -16);
  graph_update_current();

  build_footer();   /* build_footer 定义在后部, 上方前向声明已可见 */
}

/* ---- 页面管理器状态 ---- */
static HUD_PAGE s_current = PAGE_MAIN;
static uint8_t  s_brightness = 4;   /* 0..8, 预留 */
static CPM_Data s_last_data;

/* 底部页脚: 页码 + 亮度 (MAIN 页由 cpm_ui 管, 其他页自建) */
static lv_obj_t *s_footer;
static void update_footer(void) {
  if (!s_footer) return;
  char buf[32];
  snprintf(buf, sizeof(buf), "P%d/5 B%d", (int)(s_current + 1), (int)s_brightness);
  lv_label_set_text(s_footer, buf);
}

static void build_footer(void) {
  s_footer = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_color(s_footer, lv_color_hex(C_DIM), 0);
  lv_obj_align(s_footer, LV_ALIGN_BOTTOM_MID, 0, -3);
  update_footer();
}

/* ---- 公共接口 ---- */

void hud_page_init(void) {
  s_current = PAGE_MAIN;
  s_brightness = 4;
  memset(&s_last_data, 0, sizeof(CPM_Data));

  /* PAGE_MAIN = 复用 cpm_ui (已有完整 Cyber HUD 布局) */
  cpm_ui_init();   /* 内部 lv_obj_clean + 建 HUD + ui_set_cpm_ui_active(true) */

  /* GRAPH 页历史环形缓存 (500ms 采样, 非阻塞) */
  hud_history_init();
}

HUD_PAGE hud_page_current(void) { return s_current; }

uint8_t hud_brightness(void) { return s_brightness; }

void hud_brightness_up(void) {
  if (s_brightness < 8) s_brightness++;
  if (s_footer) update_footer();
}

void hud_brightness_down(void) {
  if (s_brightness > 0) s_brightness--;
  if (s_footer) update_footer();
}

void hud_page_switch(HUD_PAGE page) {
  if (page >= HUD_PAGE_COUNT) page = PAGE_MAIN;
  if (page == s_current) return;   /* 同页不重建 */
  s_current = page;
  memset(&s_last_data, 0, sizeof(CPM_Data));   /* 强制首帧刷新 */

  switch (page) {
    case PAGE_MAIN:
      /* 复用 cpm_ui (内部 clean + 建 + ui_set_cpm_ui_active) */
      cpm_ui_init();
      s_footer = NULL;   /* MAIN 用 cpm_ui 自带底部, 无页脚 (清掉旧页脚指针) */
      break;
    case PAGE_CPU:
      build_page_cpu();
      build_footer();
      break;
    case PAGE_GPU:
      build_page_gpu();
      build_footer();
      break;
    case PAGE_SYSTEM:
      build_page_system();
      build_footer();
      break;
    case PAGE_GRAPH:
      build_page_graph();   /* 内部已 build_footer + 设采样起点 */
      break;
  }
}

void hud_page_update(void) {
  const CPM_Data *d = cpm_serial_data();
  if (!d) return;

  /* PAGE_MAIN: 委托 cpm_ui_update (内部 memcmp 去抖) */
  if (s_current == PAGE_MAIN) {
    cpm_ui_update();
    return;
  }

  /* PAGE_GRAPH: 500ms 采样一次 (hud_graph_tick 内部门控, 历史缓冲全局持续记录) */
  if (s_current == PAGE_GRAPH) {
    hud_graph_tick();   /* 到点则 hud_history_push + lv_chart_set_next_value (SHIFT 左移) */
    graph_update_current();   /* 底部当前值每帧刷新 */
    return;
  }

  /* 详情页: 自己的 memcmp 去抖 */
  if (memcmp(d, &s_last_data, sizeof(CPM_Data)) == 0) return;
  s_last_data = *d;

  if (s_current == PAGE_CPU) {
    if (s_row_count >= 4) {
      /* row0: CPU TEMP */
      lv_label_set_text_fmt(s_rows[0].val, "%u℃", (int)d->cpu_temp);
      /* row1: CPU LOAD */
      lv_label_set_text_fmt(s_rows[1].val, "%u%%", (int)d->cpu_load);
      if (s_rows[1].bar) {
        lv_bar_set_value(s_rows[1].bar, d->cpu_load, LV_ANIM_ON);
        lv_obj_set_style_bg_color(s_rows[1].bar, lv_color_hex(calc_color(d->cpu_load)), LV_PART_INDICATOR);
      }
      /* row2: RAM LOAD */
      lv_label_set_text_fmt(s_rows[2].val, "%u%%", (int)d->ram_load);
      if (s_rows[2].bar) {
        lv_bar_set_value(s_rows[2].bar, d->ram_load, LV_ANIM_ON);
        lv_obj_set_style_bg_color(s_rows[2].bar, lv_color_hex(calc_color(d->ram_load)), LV_PART_INDICATOR);
      }
      /* row3: CPU FREQ (预留, 显示 "--") */
      lv_label_set_text(s_rows[3].val, "--");
    }
  }
  else if (s_current == PAGE_GPU) {
    if (s_row_count >= 4) {
      /* row0: GPU TEMP */
      lv_label_set_text_fmt(s_rows[0].val, "%u℃", (int)d->gpu_temp);
      /* row1: GPU LOAD */
      lv_label_set_text_fmt(s_rows[1].val, "%u%%", (int)d->gpu_load);
      if (s_rows[1].bar) {
        lv_bar_set_value(s_rows[1].bar, d->gpu_load, LV_ANIM_ON);
        lv_obj_set_style_bg_color(s_rows[1].bar, lv_color_hex(calc_color(d->gpu_load)), LV_PART_INDICATOR);
      }
      /* row2: GPU MEM */
      lv_label_set_text_fmt(s_rows[2].val, "%u%%", (int)d->gpu_mem_load);
      if (s_rows[2].bar) {
        lv_bar_set_value(s_rows[2].bar, d->gpu_mem_load, LV_ANIM_ON);
        lv_obj_set_style_bg_color(s_rows[2].bar, lv_color_hex(calc_color(d->gpu_mem_load)), LV_PART_INDICATOR);
      }
      /* row3: MEM SIZE (GB, 从 gpu_mem_total) */
      lv_label_set_text_fmt(s_rows[3].val, "%uGB", (int)d->gpu_mem_total);
    }
  }
  /* PAGE_SYSTEM: 静态, 不刷新 */
}
