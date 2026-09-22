# CPM PC 上位机连接测试报告 — CH32V307

> 完整数据链验证: PC 性能采集 → COM 串口 → 5A A5 CMD VAL 协议 → CH32V307 → CPM_Data → LVGL UI
> 日期: 2026-09-22
> 前置: cpm_serial 通信层 + cpm_ui 界面层 + ST7735 显示 + 色彩/方向修正 均已完成

## 1. 本次改动

| 文件 | 改动 |
|---|---|
| `src/app/cpm_ui.cpp` | `CPM_UI_TEST` 1 → **0** (切正式数据模式, 不再显示静态测试串, 跟随 CPM_Data 刷新) |
| `src/app/cpm_serial.cpp` | `cpm_reply()` 加 `#if CPM_DEBUG` 门控的 **TX ACK 日志** `[cpm] tx ACK CMD=0xNN` (与已有 RX 日志 `rx CMD=/VALUE=` 配套, 正式 CPM_DEBUG=0 时不输出, 不污染协议) |

正式模式配置: `CPM_UI_TEST=0` / `CPM_DEBUG=0` / `LCD_COLOR_TEST=0` / `LCD_MADCTL=0x60`。

## 2. 编译结果

| 构建 | Flash (262144 B) | RAM (65536 B) | 结果 |
|---|---|---|---|
| 正式 (CPM_DEBUG=0) | 201496 B (76%) | 15764 B (24%) | EXIT=0, 已烧录 COM9 |
| 调试 (CPM_DEBUG=1) | 205456 B (78%) | 15772 B (24%) | EXIT=0 (仅验证日志代码, 未烧录) |

## 3. 串口协议验证 (cpm_protocol_test.ps1, COM9, 9600 8N1)

完全复刻 PC 上位机 `writeData` 串行模式: 发 4 字节 → 阻塞读 4 字节 (500ms 超时)。握手 + 3 轮 × 10 帧 (CMD 0x01~0x0A)。

```
[HS] 5A,A5,FF,01 -> 5A,A5,FF,10  OK
[R1 c01] -> 5A,A5,01,FF   11ms  OK
[R1 c02..c0A] -> 全部 OK   144-145ms
[R2 c01..c0A] -> 全部 OK   11-16ms
[R3 c01..c0A] -> 全部 OK   11ms
==== SUMMARY: 30 / 30 frames OK (100%) ====
```

**结果: 30/30 (100%)**。
- 握手 `5A A5 FF 01 → 5A A5 FF 10` OK
- 每帧 `5A A5 CMD VAL → 5A A5 CMD FF` 应答全部正确
- **稳态 ACK 延迟 11-16ms** (R2/R3), 远低于 PC 500ms 超时
- R1 首轮 144-145ms: CH32 刚进数据模式做首次全屏重绘 (LVGL SPI flush ~145ms 阻塞主循环), ACK 等到本轮 loop 回到 `cpm_serial_poll` 才发出; 145ms 仍 < 500ms, 不超时。R2/R3 数据稳定后回到 11ms。

## 4. PC 上位机测试结果 (computer_performance_monitor.exe)

启动 exe (PID 3472), 自动探测 COM 口 + 握手 + 持续发送 (600ms/轮, 10 帧/轮)。截图 `pc_app_screenshot.png` 确认:

| 项 | PC 端读数 | 对应 CMD | CH32 应显示 |
|---|---|---|---|
| 设备连接状态 | **已连接** | 握手 | — |
| 处理器使用率 | 9% | 0x02 | CPU 行 bar=9% (绿) |
| 处理器温度 | 40°C | 0x01 | CPU 行 `CPU40C9%` |
| 内存占用率 | 49% (32/64GB) | 0x03 (0x04/0x05 GB) | RAM 行 `RAM49%` bar=49% (黄) |
| 显卡使用率 | 84% | 0x07 | GPU 行 bar=84% (红) |
| 显卡核心温度 | 73°C | 0x06 | GPU 行 `GPU73C84%` |
| 显存占用率 | 93% (22/23GB) | 0x08 (0x09/0x0A GB) | GPU内存 行 `GPU内存93%` bar=93% (红) |

运行日志: "成功程序启动 / [初始化通信串口] / 成功初始化通信串口完成 / [初始化硬件性能读取模块]"。

**结论**: PC 自动找到 COM 口 + 握手成功 + 持续发送, 全链路 (采集→串口→协议→CH32→CPM_Data) 打通。

## 5. 屏幕显示 (CH32, CPM_UI_TEST=0 数据模式)

PC 持续发数据, CH32 屏幕应显示 (bar 颜色 calc_color 绿→红):

```
CPU40C9%        bar 9%   绿
RAM49%          bar 49%  黄
GPU73C84%       bar 84%  红
GPU内存93%       bar 93%  红
```

**待用户屏幕确认** (PC 程序仍在运行, 数据实时刷新中):
- [ ] 1. CPU 行: 温度 40C + 利用率 bar 随 PC 变化
- [ ] 2. RAM 行: 利用率 bar 随 PC 变化
- [ ] 3. GPU 行: 温度 73C + 利用率 bar 随 PC 变化
- [ ] 4. GPU内存 行: 利用率 bar 随 PC 变化
- [ ] 5. bar 颜色绿→红渐变可见 (9%/49%/84%/93% 四档)
- [ ] 6. 无卡顿 (LVGL 刷新流畅, 不卡死)

## 6. 问题排查 (重点 4 项)

| 检查项 | 结果 |
|---|---|
| 串口是否被 debug 输出污染 | 否 — 正式 CPM_DEBUG=0, 协议口只走 5A A5 帧; 调试日志全部 `#if CPM_DEBUG` 门控, 同口输出但正式时编译剔除 |
| PC 是否 500ms 内收到 ACK | 是 — 稳态 11-16ms, 首轮 145ms (全屏重绘), 均 < 500ms |
| 数据是否更新 | 是 — PC 600ms/轮发 10 帧, CH32 CPM_Data 持续刷新 (屏幕待确认) |
| LVGL 是否卡顿 | 待屏幕确认 (首轮重绘 145ms 为正常全屏 flush, 非卡死) |

## 7. 调试日志用法 (CPM_DEBUG=1)

调试时 `compile_cpm_debug.ps1` 编译 (`-DCPM_DEBUG=1`, Flash 78%) 烧录, **PC 程序关闭** (否则同口冲突)。串口 9600 看到:
- 收帧: `[cpm] rx CMD=0x02 VALUE=9`
- 应答: `[cpm] tx ACK CMD=0x02`
- 帧计数: `cpm_serial_rx_frames()` / `cpm_serial_resyncs()` 可加打印

## 8. 下一步优化建议

1. **屏幕确认**: 用户确认 §5 六项后, 本步骤完成。若某行 bar 不动 → 查对应 CMD 是否收到 (开 CPM_DEBUG 看 rx 日志)。
2. **首轮 145ms 延迟 (可选)**: 若 PC 偶尔报超时, 可把 `cpm_serial_poll` 的应答提到 loop 最前 (已在 lv_timer_handler 前, 实际是本轮 flush 阻塞导致); 或将 LVGL 改成双缓冲 + 中断内快速 ACK。当前 145ms < 500ms, 非必须。
3. **RAM 已用/总量**: 任务要求显示, 160px 宽度所限当前只显 `RAM49%` (ram_used/ram_total 在 CPM_Data 可用, 可改 `RAM49% 32/64G` 横滚或单独一行)。
4. **PC 程序窗口**: 当前最小化/隐藏 (启动后未置顶), 需手动还原查看; 可加 `SetForegroundWindow` 或开机启动。
5. **GPU 数据依赖**: `nvml.dll` 仅 NVIDIA, 非 NVIDIA 机器 GPU 字段可能为 0 (本机 NVIDIA, GPU 84%/73C/显存93% 正常)。

## 9. 关键文件

| 文件 | 作用 |
|---|---|
| `src/app/cpm_ui.cpp` | UI (CPM_UI_TEST=0 数据模式) |
| `src/app/cpm_serial.cpp` | 协议接收 + ISR + 环形缓冲 + ACK + 调试日志 |
| `src/bsp/lcd_st7735.cpp` | LCD (LCD_MADCTL=0x60 色彩/方向修正, LCD_COLOR_TEST=0) |
| `cpm_protocol_test.ps1` | 协议验证 (复刻 PC 串行模式) |
| `compile_check.ps1` / `compile_cpm_debug.ps1` | 正式/调试构建 |
| `flash_manual.ps1` | 烧录 COM9 |
| `pc_app_screenshot.png` | PC 上位机连接状态截图 |
| `cpm_protocol_test.txt` | 协议测试原始结果 |
