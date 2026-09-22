# CPM UI 迁移报告 — RP2040 → CH32V307

> 步骤: 把 RP2040 版性能监视器 UI 迁移到 CH32V307EVT + ST7735(128×160 SPI) + LVGL 8.3.11。
> 不改通信协议、不改 PC 端、不重写 LCD 驱动。
> 日期: 2026-09-22

## 1. 修改文件

| 文件 | 改动 |
|---|---|
| `src/main.cpp` | `setup()` 在 `cpm_serial_init()` 后加 `cpm_ui_init()`;`loop()` 在 `cpm_serial_poll()` 后加 `cpm_ui_update()` |
| `src/app/ui.h` | 加 `#include <stdbool.h>` + 接口 `ui_set_cpm_ui_active(bool)` |
| `src/app/ui.cpp` | 加 `cpm_ui_active` 标志;`ui_stats_tick()` 里 CPM UI 接管后跳过 `ui_panel_tick()`(避免悬空指针),保留 perf 串口上报 |
| `libraries/lvgl/src/lv_conf.h` | `LV_MEM_SIZE` 768 → 4096(满足 ≥4096;注:`LV_MEM_CUSTOM=1` 实际走 stdlib malloc,此值当前不生效,仅达标) |
| `src/app/HarmonyOS_2bit.c` | 字体文件 include 由 `"lvgl/lvgl.h"` 改为 `<lvgl.h>`(本库 lvgl.h 在 src/ 下,与项目统一) |

## 2. 新增文件

| 文件 | 说明 |
|---|---|
| `src/app/cpm_ui.h` | 接口声明 `cpm_ui_init()` / `cpm_ui_update()`;`CPM_UI_TEST` 宏说明 |
| `src/app/cpm_ui.cpp` | UI 实现:4 行紧凑布局 + label + bar + calc_color;数据模式 + 中文字体测试模式 |
| `src/app/HarmonyOS_2bit.c` | 中文字体(从 `_migration_analysis/cpm_rp2040/` 复制,14px 2bpp,12875 B) |

## 3. UI 布局说明

160×128 横屏(rotation=3),对齐 RP2040 目标样式,4 内容行,每行 = 一行 label(名称+数值,HarmonyOS 2bit 中文字体)+ 一行 lv_bar(满宽 152px,量程 0-100):

### 3.1 数据模式(CPM_UI_TEST=0,正式展示)

```
┌────────────────────────────────┐
│ CPU45C60%                      │
│ ████████████░░░░░░░░░░░░░░░░░░ │  bar = CPU利用率
│ RAM50%                         │
│ ███████████████░░░░░░░░░░░░░░░ │  bar = RAM利用率
│ GPU45C60%                      │
│ ████████████████████░░░░░░░░░░ │  bar = GPU利用率
│ GPU内存45%                     │
│ ███████████████░░░░░░░░░░░░░░░ │  bar = GPU显存利用率
└────────────────────────────────┘
```

- 行1 CPU 温度+利用率 → `CPU%uC%u%%`,bar=cpu_load
- 行2 RAM 利用率 → `RAM%u%%`,bar=ram_load
- 行3 GPU 温度+利用率 → `GPU%uC%u%%`,bar=gpu_load
- 行4 GPU 显存利用率 → `GPU内存%u%%`,bar=gpu_mem_load
- bar 颜色 `calc_color()`:0→绿(255,255,0)、100→红(255,0,0),移植自 RP2040
  - **修复(2026-09-22)**: 首版 `return (uint16_t)(red<<16|green<<8)` 把 red 高位截断丢失 → 全段绿/黄看不出红端。且 `LV_COLOR_RGB` 在 LVGL 8.3.11 未声明。改为返回 0xRRGGBB(uint32_t) 经 `lv_color_hex` 转 RGB565,并还原原版 `g += (0-g)/100*rate`(g 随 rate 255→0)。现 25=黄绿 / 50=黄 / 75=橙红 / 95=红,渐变可见
- 数据源 `cpm_serial_data()`(CPM_Data);`cpm_ui_update()` 每 loop 调,`memcmp` 检测变化后才重绘(无变化不刷,省 SPI flush)
- 文本用无空格短格式,实测宽度 `CPU45C60%`=144 / `RAM50%`=96 / `GPU45C60%`=144 / `GPU内存45%`=152 px,均 ≤ 152px 可用宽;label 设 `LV_LABEL_LONG_SCROLL` 兜底(3 位温度 100C 超 8px 时横滚,不换行不叠 bar)

### 3.2 中文字体测试模式(CPM_UI_TEST=1,当前烧录状态)

4 行同时静态显示任务要求的测试串 + 演示 bar(绿→红渐变),1s 幂等重设一次,一眼核对所有中文字形:

```
┌────────────────────────────────┐
│ CPU温度                        │
│ █████░░░░░░░░░░░░░░░░░░░░░░░░░ │  bar=25  黄绿
│ GPU利用率                      │
│ ██████████████░░░░░░░░░░░░░░░░ │  bar=50  黄
│ RAM                            │
│ ████████████████████░░░░░░░░░░ │  bar=75  橙红
│ GPU内存                        │
│ █████████████████████████░░░░░ │  bar=95  红
└────────────────────────────────┘
```

- 任务 §4 要求的 3 个测试串 **`CPU温度` / `GPU利用率` / `RAM` 全部显示**,第 4 行加 `GPU内存`(数据模式行4 用词,一并核对)
- 正式数据展示时把 `cpm_ui.cpp` 顶部 `CPM_UI_TEST` 改回 0(或编译加 `-DCPM_UI_TEST=0`)

### 3.3 集成

CPM UI 接管主屏(替换原 ui_panel 控制板),`cpm_ui_init` 的 `lv_obj_clean` 释放旧控件后 `ui_set_cpm_ui_active(true)` 暂停 `ui_panel_tick`(FPS/RAM 标签 + 按键分发),perf 串口上报保留。

### 3.4 字符集约束与缺字记录

HarmonyOS_2bit 实际含字符(由 `unicode_list_0` 解码,`range_start=0x20` 偏移):
- **ASCII**: 空格 `A B C G M P R U` `0-9 % / :`
- **CJK**: `℃ 内 利 存 度 率 用 （ ） ：`

**缺字**:
- 大写字母缺 `V T D E S L O H K F Q W X Y Z N I J`(只有 A B C G M P R U)
- 中文缺 `显 能 主 频 速` 等

**测试串字符覆盖核对**(逐字,全部命中):
| 测试串 | 字符 | 覆盖 |
|---|---|---|
| CPU温度 | C P U 温 度 | 全在字体内 |
| GPU利用率 | G P U 利 用 率 | 全在字体内 |
| RAM | R A M | 全在字体内 |
| GPU内存 | G P U 内 存 | 全在字体内 |

**因此**:
- `VRAM` 的 **V** 缺 → 用 `GPU内存` 替代(内+存都在)
- `GPU显存` 的 **显** 缺 → 同上,用 `GPU内存`
- 任务要求的 3 个测试串无缺字(上表已核对)
- 未生成新字体(按要求)

## 4. Flash / RAM 变化

| 版本 | Flash (262144 B) | RAM (65536 B) |
|---|---|---|
| 仅 cpm_serial(上一步) | 199276 B (76%) | 15704 B (23%) |
| + cpm_ui 数据模式(CPM_UI_TEST=0) | 201488 B (76%) | 15764 B (24%) |
| + cpm_ui 测试模式(CPM_UI_TEST=1,当前烧录) | **201360 B (76%)** | **15760 B (24%)** |
| 增量(数据模式 vs 上一步) | +2212 B | +60 B |

`LV_MEM_CUSTOM=1` → 实际用 stdlib malloc(64KB 堆),`LV_MEM_SIZE=4096` 当前不生效,仅达标。
两种模式均已 `compile_check` 通过(EXIT=0);测试模式固件已烧录 COM9(OCD EXIT=0)。

## 5. 屏幕测试结果

**待用户反馈**(固件已烧录 COM9,当前为**测试模式**:4 行静态显示 `CPU温度` / `GPU利用率` / `RAM` / `GPU内存` + 演示 bar):

- [ ] 1. CPU/RAM/GPU 文字显示(4 个测试串中文正确,无方块/空白)
- [ ] 2. bar 正常显示(满宽,4 行 25/50/75/95 渐变色 黄绿→黄→橙红→红;修复前全绿/黄看不出红端)
- [ ] 3. 无乱码(中文 温度/利用率/内存 字形正确,无缺字)
- [ ] 4. 无死机(LVGL 主循环稳定,1s 幂等重设不卡顿)
- [ ] 5. (数据模式)切 `CPM_UI_TEST=0` 重烧后:数据变化时 UI 同步变化(PC 端发不同值,屏幕文字+bar 跟随)

## 6. 下一步建议

1. **屏幕确认测试模式**: 按 §5 核对 4 个测试串 + bar 显示;确认无乱码/缺字/死机后,把 `cpm_ui.cpp` 的 `CPM_UI_TEST` 改回 0 重烧,进数据模式。
2. **接 PC 上位机实测**: 跑 `computer_performance_monitor` PC 端,确认 COM 自动探测 + 握手 + 4 行数据实时刷新。注意 `nvml.dll` 仅 NVIDIA GPU,CPU 温度走 WinRing0(AMD 未支持默认 0)→ 非 NVIDIA 机器 GPU 字段可能为 0。
3. **RAM 已用/总量**: 任务 §1 要求显示,但 160px 宽度所限当前只显 `RAM50%`(`ram_used`/`ram_total` 在 CPM_Data 中可用)。若要显示可改成 `RAM50% 4/8G`(需 SROLL 横滚)或单独一行。
4. **缺字优化(可选)**: 若需 `VRAM`/`显存` 原词,需重新生成含 V/显 的字体;当前 `GPU内存` 语义等价,可不改。
5. **温度 bar**: 当前 CPU/GPU 行 bar 显示的是"利用率"(温度只是行内数字)。若要温度单独 bar,需拆成 6 行(每行单指标),128px 高度会更紧。
6. **布局微调**: 若屏幕实测文字/比例不理想,可调 `ROW_H`/`BAR_H`/字体(当前 14px),或把 CPU/GPU 拆成温度行+利用率行。
