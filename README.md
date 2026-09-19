# LVGL Benchmark Demo — CH32V307 + ST7735

CH32V307EVT-R1 + Arduino Core + 1.8" ST7735S TFT shield (160×128) 上的 LVGL 8.3.11
Benchmark Demo。可维护的工程结构 (src/ 分 bsp / lv_port / app 三层)，LVGL 源码未改。

**稳定里程碑: tag `lvgl-benchmark-ok`** (commit `0594844`, "CH32V307 LVGL benchmark demo working")

## 硬件与依赖

| 项 | 值 |
|---|---|
| 开发板 | CH32V307EVT-R1 (256KB Flash / 64KB RAM, 144MHz) |
| 串口 | COM9, 115200 |
| 下载器 | WCH-LinkRV + OpenOCD |
| TFT | 1.8" ST7735S shield, 160×128, SPI, rotation=3, 偏移 X=26 / Y=1 |
| 输入 | 五向摇杆 ADC (PA3) |
| Arduino Core | WCH:ch32v 1.0.4, 板选 `WCH:ch32v:CH32V30x_EVT` |
| LVGL | 8.3.11 (未改源码, lv_conf 在库内) |
| TFT 库 | Adafruit_ST7735_and_ST7789_Library + Adafruit_GFX + Adafruit_BusIO |
| 工具链 | arduino-cli (C:\Users\Administrator\Tools\arduino-cli) |

## 工程结构

```
LVGL_Demo/
├── LVGL_Demo.ino            # 入口占位 (Arduino 要求 .ino 存在)
├── wiring_private.h         # Adafruit 库 shim, 必须留 sketch 根目录
├── build_upload.ps1         # arduino-cli 编译
├── upload_no_verify.ps1     # WCH-LinkRV + OpenOCD 烧录
├── NOTES.md                 # 工程结构与编译细节
└── src/
    ├── main.cpp             # setup()/loop() 编排
    ├── bsp/
    │   ├── board.h/.cpp     # 集中: 屏幕参数 + GPIO + 摇杆 ADC
    │   └── lcd_st7735.h/.cpp# Adafruit_ST7735 封装 (lcd_init / lcd_flush)
    ├── lv_port/
    │   ├── lv_port_disp.*   # 画缓冲 + disp_drv + flush_cb
    │   ├── lv_port_indev.*  # 五向摇杆 keypad indev
    │   └── lv_port_tick.*   # millis → lv_tick 增量
    └── app/
        └── ui.h/.cpp        # benchmark 启动 + CPU/RAM 串口统计
```

关键设计点 (详见 NOTES.md):

- LVGL 库侧仅 10 个 `bm_*.c` 包装 TU (demos/ 不被 arduino-cli 编译) +
  `bm_benchmark.c` 里 `#define` alias 三张大图到 indexed16 省 ~94KB Flash
- 屏幕参数与全部 GPIO 集中在 `src/bsp/board.h`
- TFT 驱动不在 main.cpp，在 `src/bsp/lcd_st7735.cpp`
- `LV_MEM_CUSTOM=1`, `LV_MEM_SIZE=36000`; `LV_DEF_REFR_PERIOD=8` (~125fps 上限)

## 构建与烧录

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build_upload.ps1
# 编译 + arduino-cli upload 走 COM9 (会失败, 串口被 OpenOCD 占用属正常)
powershell -NoProfile -ExecutionPolicy Bypass -File upload_no_verify.ps1
# WCH-LinkRV + OpenOCD 烧录 build/LVGL_Demo.ino.elf, EXIT=0 即成功
```

## 编译结果 (本里程碑)

- Flash: 222820 B (84%) / 262144 B
- RAM: 8164 B (12%) / 65536 B
- 0 error, 0 undefined reference, 8 个源文件全部链接进 ELF

## 已验证功能

- CH32V307 启动 / Arduino Core 运行
- SPI 正常, ST7735 显示正常
- LVGL 初始化 / flush callback / tick 正常
- Benchmark Demo 正常运行: 串口上报 `weighted fps: 12444`, `Opa.speed: 999%`
