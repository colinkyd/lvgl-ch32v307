# LVGL PC Performance HUD — CH32V307 + ST7735

CH32V307EVT-R1 + Arduino Core + 1.8" ST7735S TFT shield (160×128 竖屏, rotation=3) 上的
LVGL 8.3.11 **PC 性能监视 HUD**。通过 CPM 串口协议接收 PC 上位机
(`computer_performance_monitor.exe`) 推送的 CPU / GPU / RAM / 网络数据，
6 页 HUD 实时显示。可维护的工程结构 (src/ 分 bsp / lv_port / app 三层)，LVGL 源码未改。

历史起点: LVGL Benchmark Demo，tag `lvgl-benchmark-ok`。

## 硬件与依赖

| 项 | 值 |
|---|---|
| 开发板 | CH32V307EVT-R1 (256KB Flash / 64KB RAM, 144MHz) |
| 调试/下载 | COM9 虚拟串口 (CPM 协议口 9600 8N1) + WCH-LinkRV + OpenOCD |
| TFT | 1.8" ST7735S shield, 160×128, SPI, rotation=3, 偏移 X=26 / Y=1 |
| 输入 | 五向摇杆 ADC (PA3), 边沿事件模型 (key_adc) |
| Arduino Core | WCH:ch32v 1.0.4, 板选 `WCH:ch32v:CH32V30x_EVT` |
| LVGL | 8.3.11 (未改源码, lv_conf 在库内) |
| 字体 | Montserrat 12 (LV_FONT_DEFAULT, 全 ASCII) + HarmonyOS_2bit 14px (中文/数字, 33 字形) |
| TFT 库 | Adafruit_ST7735_and_ST7789_Library + Adafruit_GFX + Adafruit_BusIO |
| 工具链 | arduino-cli (C:\Users\Administrator\Tools\arduino-cli) |

## HUD 页面 (6 页)

| 页 | 标题 | 内容 |
|---|---|---|
| 1 | MAIN | CPU/GPU 主面板 + RAM/显存底部区 (cpm_ui, 总览) |
| 2 | CPU DETAIL | CPU 温度 / 负载 / RAM 负载 (bar) + CPU 频率 (MHz) |
| 3 | GPU DETAIL | GPU 温度 / 负载 / 显存 (bar) + 显存容量 (GB) |
| 4 | SYSTEM | 静态信息: 设备 / 通信状态 / 版本 |
| 5 | PERFORMANCE GRAPH | 实时性能曲线 (lv_chart, SHIFT 示波器滚动, 500ms 采样) |
| 6 | SYSTEM DETAIL | CPU/GPU 频率 (MHz) + 网络下行 ↓ / 上行 ↑ (MB/s) |

- 切页 = `lv_obj_clean` 重建当前页 (任一时刻仅 1 页对象常驻, 省 RAM)。
- 数据刷新去抖: `hud_page_update()` memcmp 本页显示字段, 无变化不重绘; GRAPH 页 500ms 采样。
- 字体坑: HarmonyOS_2bit 仅 33 字形 (ASCII 只含 0/2/5 和 `!"'-`), 纯 ASCII 数值
  (如 `2900MHz` / `5MB/s`) 的字母必须走 Montserrat 12, 否则 missing glyph 出方框。
  网络行箭头用 LVGL 内置符号 `LV_SYMBOL_UP/DOWN` (U+F077/F078, 在 Montserrat 12 稀疏 cmap 中)。

### 按键 (五向键)

| 键 | 动作 |
|---|---|
| UP / DOWN | 上 / 下一页 (循环) |
| LEFT / RIGHT | 亮度 − / + (页脚显示) |
| CENTER | 返回 MAIN 页 |

## CPM 串口协议

物理层: USART1, **9600 8N1** (在 115200 banner 后切速)。
帧格式: `[0x5A][0xA5][CMD][VAL]` 4 字节定长, 无长度无 CRC。
应答: 收到 `5A A5 CMD VAL` (0x01~0x12) → 回 `5A A5 CMD FF`; 未用 CMD 回 `5A A5 CMD FF`;
握手 `5A A5 FF 01` → 回 `5A A5 FF 10`。

| CMD | 含义 | 单位 |
|---|---|---|
| 0x01 | CPU 温度 | °C |
| 0x02 | CPU 利用率 | % |
| 0x03 | RAM 利用率 | % |
| 0x04 | RAM 已用 | GB (0x0D 别名) |
| 0x05 | RAM 总量 | GB (0x0E 别名) |
| 0x06 | GPU 温度 | °C |
| 0x07 | GPU 利用率 | % |
| 0x08 | GPU 显存利用率 | % |
| 0x09 | GPU 显存已用 | GB (0x0F 别名) |
| 0x0A | GPU 显存总量 | GB (0x10 别名) |
| 0x0B | CPU 频率 | 100MHz/VAL (36=3600MHz) |
| 0x0C | GPU 频率 | 100MHz/VAL (24=2400MHz) |
| 0x11 | 网络下载 | 1MB/s/VAL (12=12MB/s) |
| 0x12 | 网络上传 | 1MB/s/VAL (2=2MB/s) |
| 0xFF | 握手 | — |

接收走 USART1 RX 中断 + 软件环形缓冲 (WCH core 默认纯轮询, LVGL SPI flush 阻塞
~18ms 会 overrun 掉 9600 字节 → 整帧丢)。字节错位自动重同步。
PC 上位机: `_migration_analysis/cpm/computer_performance_monitor.exe`,
带 `/test` 参数发固定测试数据; 协议测试 `cpm_protocol_test.txt` (CMD 0x01~0x12, 54/54 通过)。

## 工程结构

```
LVGL_Demo/
├── LVGL_Demo.ino            # 入口占位 (Arduino 要求 .ino 存在)
├── wiring_private.h         # Adafruit 库 shim, 必须留 sketch 根目录
├── build_upload.ps1         # arduino-cli 编译 (+ upload 走 COM9, 串口被 OpenOCD 占用会失败属正常)
├── upload_no_verify.ps1     # WCH-LinkRV + OpenOCD 手动烧录 (arduino-cli upload 报 missing close-brace 时用)
├── NOTES.md                 # 工程结构与编译细节
├── protocol_extension_report.md  # CMD 0x0B~0x12 扩展 + 分帧测试报告
├── cpm_protocol_test.txt    # 协议测试脚本 (CMD 0x01~0x12)
└── src/
    ├── main.cpp             # setup()/loop() 编排
    ├── bsp/
    │   ├── board.h/.cpp     # 集中: 屏幕参数 + GPIO + 串口
    │   ├── lcd_st7735.h/.cpp# Adafruit_ST7735 封装 (lcd_init / lcd_flush)
    │   └── key_adc.*        # 五向摇杆 ADC: 5 连采中值 + 死区 + 30ms 去抖 + 边沿事件
    ├── lv_port/
    │   ├── lv_port_disp.*   # 画缓冲 + disp_drv + flush_cb
    │   ├── lv_port_indev.*  # 摇杆 keypad indev
    │   └── lv_port_tick.*   # millis → lv_tick 增量
    └── app/
        ├── cpm_serial.*     # CPM 协议: RX 中断 + 环形缓冲 + 帧状态机 + 应答 (CMD 0x01~0x12)
        ├── cpm_ui.*         # MAIN 页 (总览面板)
        ├── hud_page.*       # 6 页框架: 建页/切页/刷新去抖/亮度档位
        ├── hud_input.*      # 五向键 → 切页/亮度 动作映射
        ├── hud_graph.*      # GRAPH 页: lv_chart SHIFT 滚动曲线
        ├── hud_history.*    # 曲线历史缓冲
        ├── HarmonyOS_2bit.c # 14px 2bit 中文字体 (33 字形)
        ├── ui_group.*       # indev group 管理
        └── ui/ui_panel.*    # 控制面板 UI
```

关键设计点 (详见 NOTES.md):

- LVGL 库侧仅 10 个 `bm_*.c` 包装 TU (demos/ 不被 arduino-cli 编译) +
  `bm_benchmark.c` 里 `#define` alias 三张大图到 indexed16 省 ~94KB Flash
- 屏幕参数与全部 GPIO 集中在 `src/bsp/board.h`
- `LV_MEM_CUSTOM=1`, `LV_MEM_SIZE=36000`; `LV_DEF_REFR_PERIOD=8` (~125fps 上限)
- 主循环顺序: cpm_serial_poll (收帧+应答) → hud_input_poll (按键) → hud_page_update (数据) →
  lv_port_tick_task → lv_timer_handler

## 构建与烧录

```powershell
# 编译 (EXIT=0 且生成 build/LVGL_Demo.ino.elf 即成功; E 盘 index 警告无害)
powershell -NoProfile -ExecutionPolicy Bypass -File build_upload.ps1
# WCH-LinkRV + OpenOCD 烧录 build/LVGL_Demo.ino.elf, "Programming Finished" + EXIT=0 即成功
powershell -NoProfile -ExecutionPolicy Bypass -File upload_no_verify.ps1
```

## 当前编译结果 (master @ dc0665f)

- Flash: 215516 B (82%) / 262144 B
- RAM: 13556 B (20%) / 65536 B
- 0 error

## 已验证功能

- CH32V307 启动 / Arduino Core / SPI / ST7735 显示 / LVGL flush / tick 正常
- CPM 协议全 CMD (0x01~0x12) 54/54 测试通过, 含分帧 / 错位重同步
- 6 页切换正常; GRAPH 页示波器滚动正常; 详情页去抖无冗余重绘
- PC 上位机非测试模式: CPU 频率实测 (MSR APERF/MPERF > rdtsc > WMI 三层), 网络 GetIfTable 差分
- 第 6 页: 频率/网络数据 + 箭头符号显示, 无 missing glyph 方框
