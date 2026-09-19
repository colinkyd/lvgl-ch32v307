#include "board.h"
#include <SPI.h>

/* 五向摇杆 ADC 五档判定阈值 (右/上/中/左/下 边界), 仅内部使用 */
static const uint16_t joy_adc_map[5] = {78, 183, 403, 667, 950};

void board_init(void) {
  /* SPI2 重绑到 PB15/PB14/PB13 */
  SPI.setMOSI(PIN_SPI_MOSI);
  SPI.setMISO(PIN_SPI_MISO);
  SPI.setSCLK(PIN_SPI_SCK);
  pinMode(PIN_SPI_MISO, INPUT);

  Serial.begin(115200);
  analogReadResolution(10);
}

uint8_t board_joystick_read(void) {
  uint16_t v = analogRead(PIN_JOY_ADC);
  for (int i = 0; i < 5; i++) {
    if (v < joy_adc_map[i]) return (uint8_t)i;
  }
  return 5; /* 无方向 */
}
