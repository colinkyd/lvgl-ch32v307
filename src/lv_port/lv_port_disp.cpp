#include "lv_port_disp.h"
#include "../bsp/board.h"
#include "../bsp/lcd_st7735.h"

extern "C" {
#include <lvgl.h>
}

static uint8_t lv_buf[LCD_WIDTH * LV_BUF_LINES * 2];   /* RGB565 部分缓冲 */

static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  lcd_flush(area->x1, area->y1,
            area->x2 - area->x1 + 1, area->y2 - area->y1 + 1,
            (const uint16_t *)color_p);
  lv_disp_flush_ready(drv);
}

void lv_port_disp_init(void) {
  lv_disp_draw_buf_init(&draw_buf, lv_buf, NULL, LCD_WIDTH * LV_BUF_LINES);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = LCD_WIDTH;
  disp_drv.ver_res  = LCD_HEIGHT;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.flush_cb = flush_cb;

  lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
  lv_disp_set_bg_color(disp, lv_color_hex(0x000000));
}
