#include "lcd_st7735.h"
#include "board.h"
#include <Adafruit_ST7735.h>

static Adafruit_ST7735 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

void lcd_init(void) {
  tft.initR(INITR_GREENTAB);
  tft.setRotation(LCD_ROTATION);
  tft.setSPISpeed(TFT_SPI_SPEED);
  tft.fillScreen(ST7735_BLACK);
}

void lcd_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *px) {
  tft.startWrite();
  tft.setAddrWindow(x + LCD_OFFSET_X, y + LCD_OFFSET_Y, w, h);
  tft.writePixels((uint16_t *)px, w * h);
  tft.endWrite();
}
