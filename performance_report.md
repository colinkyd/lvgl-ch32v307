# LVGL Benchmark Performance Report — CH32V307 + ST7735

- Date: 2026-09-19
- Firmware: LVGL 8.3.11 benchmark demo, CH32V307EVT-R1 @120MHz, SPI2 → ST7735 (160×128)
- Authoritative metric: **Weighted FPS** (LVGL official benchmark output, 48 scenes × normal/opa modes, weighted)
- Raw serial data: `serial_B1.txt` … `serial_S48.txt` (each = full ~100s benchmark round + closing summary)
- Toolchain: `perf_test.ps1` (config → compile → flash → 130s capture, with COM9 auto-recovery), `run_batch.ps1` (batch driver)

## 1. Draw buffer sweep (SPI = 36MHz fixed)

| Group | Buffer lines | Buffer bytes | **Weighted FPS** | Opa speed | flush/s | flush avg/max | LV RAM peak | Global RAM |
|-------|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| B2 | 5 | 3840 | 10288 | 108% | 167 | 4/6ms | 13KB | 6588B (10%) |
| B1 | 10 | 7680 | 11656 | 107% | 98 | 8/11ms | 15KB | 8188B (12%) |
| B3 | 20 | 15360 | 11888 | 108% | 62 | 13/21ms | 13KB | 11388B (17%) |
| **B4** | **40** | **30720** | **12463** | **108%** | **44** | **20/41ms** | **8KB** | **17788B (27%)** |

Monotonic increase 5→40 lines (+21%). Blocking SPI flush: larger buffer → fewer flushes/s → smaller idle gaps between frames → higher total throughput. Cost: longer single-flush blocking (41ms max at 40 lines) and LV_MEM pool usage 30KB/36KB (85%).

## 2. SPI frequency sweep (buffer = 10 lines fixed)

| Group | Requested | Actual | **Weighted FPS** | flush/s | flush avg/max |
|-------|:---:|:---:|:---:|:---:|:---:|
| S8 | 8 MHz | 4.5 MHz | 8010 | 73 | 11/14ms |
| S16 | 16 MHz | 9 MHz | 9655 | 86 | 9/12ms |
| S24 | 24 MHz | 18 MHz | 11656 | 98 | 8/11ms |
| S48 | 48 MHz | 36 MHz (capped) | 11656 | 99 | 8/11ms |
| B1 | 36 MHz | 36 MHz | 11656 | 98 | 8/11ms |

- **18 MHz saturates**: 24/36/48 MHz all give identical 11656 — no gain beyond 18 MHz.
- **48 MHz capped to 36 MHz** by hardware (SPI2 source = PCLK1 = 72MHz, fastest divider /2). S48 data byte-identical to B1 confirms the cap.
- 4.5→18 MHz: FPS +45%; at low frequency the SPI transfer time is the dominant bottleneck.

## 3. Best configuration

**`LV_BUF_LINES = 40` + SPI 36 MHz → Weighted FPS 12463 (highest of all groups)**
- Flash 226148 B (86% of 256KB), global RAM 17788 B (27% of 64KB)
- LV_MEM pool: 36KB total, draw buffer occupies 30.7KB (85%)

Trade-offs:
- Full-screen / large-area redraw workloads (all benchmark scenes) → 40 lines is optimal
- Small-dirty-region + high-frequency interaction workloads → 10–20 lines preferred (shorter flush blocking, more LV_MEM headroom), FPS loss ≤ 2%
- Requesting 24 MHz is sufficient in practice (18 MHz already saturates); 36 MHz kept as margin

## 4. Pitfalls recorded

1. **WCH-Link flash drops the CH340 UART (COM9 disappears)**: `perf_test.ps1` auto-recovers via `pnputil /restart-device USB\VID_05C6&PID_90BB`; otherwise physical USB replug.
2. **`Start-Process -PassThru` on PS 5.1**: child stdout not inherited + WaitForExit unreliable → batch uses synchronous `&` invocation under a background process.
3. **`lv_log()` is NOT filtered by `LV_LOG_LEVEL`** (only checks `< NONE`), while the `LV_LOG_WARN` macro is compiled out per level → `LV_LOG_LEVEL=3` kills the "layer context" warn flood without losing the Weighted FPS line.
4. One benchmark round ≈ 100 s (48 scenes × 2 modes × SCENE_TIME 1000 ms + switching); capture window must be ≥ 110 s.
