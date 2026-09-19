#ifndef BSP_PERF_CONFIG_H
#define BSP_PERF_CONFIG_H
/* Best config from 2026-09-19 benchmark sweep: 40-line buffer + SPI 36MHz
   -> Weighted FPS 12463 (highest of all 7 groups). See performance_report.md */
#define PERF_BUF_LINES   40
#define PERF_SPI_SPEED   36000000UL
#define PERF_TEST_TAG    "OPT"
#endif
