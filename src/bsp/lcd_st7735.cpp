#include "lcd_st7735.h"
#include "board.h"
#include <Adafruit_ST7735.h>

static Adafruit_ST7735 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

/* 色彩自测: 1 = lcd_init 后依次全屏显示 R/G/B/Y/M/C/W (各 1.5s),
 * 用于人工确认 R/G/B 通道对应关系. 正式使用改回 0. */
#ifndef LCD_COLOR_TEST
#define LCD_COLOR_TEST 0
#endif

/* 方向/色彩 (本屏 green tab).
 * 库 setRotation(3) 对 green tab 发 MADCTL = MX|MV|BGR = 0x68:
 *   该方向位组合在本屏显示正常(改色前用户未抱怨方向),
 *   仅 BGR(0x08) 位导致 R/B 互换.
 * 故最小修法 = 只清 BGR, 保留原方向位: 0x68 & ~0x08 = 0x60 (MX|MV).
 *   注意: 曾误发 0xE0(MY|MX|MV) 多加了 MY 位 -> 上下镜像, 已纠正.
 * 位定义: MX=0x40 左右翻  MY=0x80 上下翻  MV=0x20 行列交换  BGR=0x08
 * 若仍异常, 按上面位定义调 LCD_MADCTL (候选: 0x60 / 0x68 / 0xE0 / 0xA0).
 */
#ifndef LCD_MADCTL
#define LCD_MADCTL 0x60
#endif

/* 在 setRotation 之后重发一次 MADCTL (库只在 setRotation 内重写它,
 * setAddrWindow 不碰, 故调一次即长期有效). */
static void fix_madctl(void) {
  uint8_t madctl = LCD_MADCTL;
  tft.sendCommand(ST77XX_MADCTL, &madctl, 1);
}

void lcd_init(void) {
  tft.initR(INITR_GREENTAB);
  tft.setRotation(LCD_ROTATION);
  tft.setSPISpeed(TFT_SPI_SPEED);
  fix_madctl();
  tft.fillScreen(ST7735_BLACK);

#if LCD_COLOR_TEST
  /* 色彩自测: 红->绿->蓝->黄->品红->青->白, 各 1.5s.
   * 期望: 红=纯红 / 绿=纯绿 / 蓝=纯蓝; 若红蓝互换说明 BGR 位需清/置. */
  static const uint16_t colors[] = {ST7735_RED, ST7735_GREEN, ST7735_BLUE,
                                    ST7735_YELLOW, ST7735_MAGENTA,
                                    ST7735_CYAN, ST7735_WHITE};
  for (uint32_t i = 0; i < 7; i++) {
    tft.fillScreen(colors[i]);
    delay(1500);
  }
#endif
}

void lcd_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *px) {
  /* WCH micros() 基于会被 SysTick 中断清零的 CNT, 短窗口有噪声;
     flush 都 >1ms, 用 millis() 分辨率 (ms) 更稳. */
  uint32_t t0 = millis();
  tft.startWrite();
  tft.setAddrWindow(x + LCD_OFFSET_X, y + LCD_OFFSET_Y, w, h);
  tft.writePixels((uint16_t *)px, w * h);
  tft.endWrite();
  perf_on_flush(millis() - t0);   /* 单位: ms */
}
