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

/* ================= 性能统计 (1s 窗口) ================= */
static uint32_t perf_frames = 0;          /* loop/lv_timer_handler 调用次数 */
static uint32_t perf_flushes = 0;         /* lcd_flush 次数 */
static uint64_t perf_flush_total_ms = 0;  /* flush 累计耗时 (ms) */
static uint32_t perf_flush_max_ms = 0;    /* 单次 flush 最大耗时 (ms) */
static uint32_t perf_win_start = 0;

void perf_init(void) {
  perf_frames = perf_flushes = 0;
  perf_flush_total_ms = 0;
  perf_flush_max_ms = 0;
  perf_win_start = millis();
}

void perf_on_frame(void) { perf_frames++; }

void perf_on_flush(uint32_t ms) {
  perf_flushes++;
  perf_flush_total_ms += ms;
  if (ms > perf_flush_max_ms) perf_flush_max_ms = ms;
}

/* SPI 实际落地频率: SPI2 源 = PCLK1 = 72MHz, 分频表 /2.. /256.
   spi_init 选 "能达到的不低于请求" 的最快档 (源/2=36M 为硬件上限). */
static uint32_t spi_actual_hz(void) {
  const uint32_t src = 72000000UL;
  uint32_t f = PERF_SPI_SPEED;
  if (f >= src / 2)   return src / 2;
  if (f >= src / 4)   return src / 4;
  if (f >= src / 8)   return src / 8;
  if (f >= src / 16)  return src / 16;
  if (f >= src / 32)  return src / 32;
  if (f >= src / 64)  return src / 64;
  if (f >= src / 128) return src / 128;
  return src / 256;
}

void perf_report(uint32_t now_ms, uint32_t cpu_pct, uint32_t lv_ram_kb, uint32_t free_ram_kb) {
  uint32_t win = now_ms - perf_win_start;
  if (win < 1000) return;

  uint32_t loop_rate  = perf_frames * 1000UL / win;
  uint32_t avg_flush  = perf_flushes ? (uint32_t)(perf_flush_total_ms / perf_flushes) : 0;

#if CPM_DBG
  Serial.printf("[%02lu:%02lu] %s CPU=%lu%% loop/s=%lu flush/s=%lu flush_avg=%lums flush_max=%lums SPI=%luMHz LV_RAM=%luKB free=%luKB\r\n",
                (unsigned long)(now_ms / 60000), (unsigned long)((now_ms % 60000) / 1000),
                PERF_TEST_TAG,
                (unsigned long)cpu_pct, (unsigned long)loop_rate, (unsigned long)perf_flushes,
                (unsigned long)avg_flush, (unsigned long)perf_flush_max_ms,
                (unsigned long)(spi_actual_hz() / 1000000UL),
                (unsigned long)lv_ram_kb, (unsigned long)free_ram_kb);
#endif

  perf_frames = perf_flushes = 0;
  perf_flush_total_ms = 0;
  perf_flush_max_ms = 0;
  perf_win_start = now_ms;
}
