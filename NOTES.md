# LVGL_Demo 可维护工程结构 (整理完成)

## 工程树
```
LVGL_Demo/
├── LVGL_Demo.ino            # 入口占位 (Arduino 要求 .ino 存在); 仅 #include <Arduino.h>
├── wiring_private.h         # Adafruit 库 shim, 必须留 sketch 根目录 (include path)
├── build_upload.ps1         # arduino-cli 编译脚本
├── upload_no_verify.ps1     # WCH-LinkRV + OpenOCD 烧录脚本
├── NOTES.md                 # 本文件
└── src/
    ├── main.cpp             # 入口: setup()/loop() 编排
    ├── bsp/
    │   ├── board.h          # 集中: 屏幕参数 + GPIO + 内存布局 + 板级 API
    │   ├── board.cpp        # SPI 重绑 / 引脚初始化 / 摇杆 ADC
    │   ├── lcd_st7735.h     # lcd_init / lcd_flush
    │   └── lcd_st7735.cpp   # Adafruit_ST7735 实例 + 刷新 (参数取自 board.h)
    ├── lv_port/
    │   ├── lv_port_disp.h/.cpp    # 画缓冲 + disp_drv + flush_cb
    │   ├── lv_port_indev.h/.cpp   # 五向摇杆 keypad indev
    │   └── lv_port_tick.h/.cpp    # millis→lv_tick 增量
    └── app/
        ├── ui.h             # ui_init / ui_stats_tick
        └── ui.cpp           # benchmark 启动 + CPU/RAM 统计上报
```

## 设计要点
1. **LVGL 源码未改**: 全部逻辑在 `src/` 内; LVGL 库侧仅 `src/` 下有 10 个 `bm_*.c`
   包装 TU (benchmark 主文件 + 9 个 asset 独立 .c, 因 demos/ 不被 arduino-cli 编译),
   以及 `bm_benchmark.c` 里 `#define` alias 三张大图到 indexed16 省 ~94KB Flash。
2. **TFT 驱动不在 main.cpp**: 在 `bsp/lcd_st7735.cpp`; main.cpp 只调 `lcd_init()`。
3. **GPIO 集中**: 全部在 `bsp/board.h` (PIN_SPI_*/PIN_TFT_*/PIN_JOY_ADC)。
4. **屏幕参数集中**: `bsp/board.h` 的 LCD_WIDTH/LCD_HEIGHT/LCD_OFFSET_X/LCD_OFFSET_Y/LCD_ROTATION。
5. **Benchmark 保留**: `app/ui.cpp` 调 `lv_demo_benchmark()`。

## 编译结果 (arduino-cli, WCH:ch32v:CH32V30x_EVT, COM9)
- 编译: 0 error, 0 undefined reference
- Flash: 222820 B (84%) / 262144 B  (整理前 222776 B, +44 B = 统计函数参数化开销)
- RAM:   8164 B (12%) / 65536 B    (与整理前持平)
- 上传: OpenOCD EXIT=0, 256KB Flash / 64KB RAM 写入成功
- 符号确认: setup@0x77a, loop@0x796, ui_stats_tick@0x482, board_init/lcd_init/
  lcd_flush/lv_port_{disp,indev,tick}_init 全部链接进 ELF

## 验证命令
```
build_upload.ps1    # 编译
upload_no_verify.ps1 # 烧录 COM9 (WCH-LinkRV + OpenOCD)
```
