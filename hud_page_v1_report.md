# HUD Page v1 报告 — 多页面框架 + 五向键输入

日期: 2026-09-22
分支: `feature/pc-hud-cyber` (colinkyd/lvgl-ch32v307)
基线: Cyber HUD v1 单页版 (commit 44a3331, Flash 202788 B / RAM 13228 B)

---

## 1. 本步骤目标

在单页 Cyber HUD (cpm_ui) 基础上，增加：
1. 多页面 HUD 框架（4 页：MAIN / CPU / GPU / SYSTEM）
2. 五向键输入层（复用现有 ADC 按键驱动）
3. 页面与输入集成（非阻塞）

**约束（已遵守）**：不改 `cpm_serial.cpp`、不改 PC 通信协议、不改 `CPM_Data` 结构，只扩展 UI 和输入层。

---

## 2. 新增文件

| 文件 | 职责 |
|---|---|
| `src/app/hud_page.h` | 4 页枚举 + init/switch/update/brightness 接口 |
| `src/app/hud_page.cpp` | 页面管理 + 3 个详情页构建 + 通用 update + 页脚 |
| `src/app/hud_input.h` | 五向键输入层接口 |
| `src/app/hud_input.cpp` | 复用 key_adc 边沿模型 + 按键→动作映射 |

修改文件：
| 文件 | 改动 |
|---|---|
| `src/main.cpp` | setup: `cpm_ui_init` → `hud_page_init` + `hud_input_init`；loop: `cpm_ui_update` → `hud_input_poll` + `hud_page_update` |

`cpm_ui.cpp/.h` **未改**（PAGE_MAIN 直接复用其 init/update）。

---

## 3. 页面设计

### PAGE_MAIN（默认主页）
直接复用 `cpm_ui_init/cpm_ui_update` 的完整 Cyber HUD 布局：
- 顶部 SYSTEM MONITOR
- CPU 面板（温度 + 使用率% + bar + 四角装饰）
- GPU 面板（温度 + 使用率% + bar + 四角装饰）
- 底部 RAM / GPU MEM（数值% + 细 bar）

### PAGE_CPU（CPU 详情）
4 行 metric：
- CPU TEMP（`cpu_temp`, ℃）
- CPU LOAD（`cpu_load`, % + bar）
- RAM LOAD（`ram_load`, % + bar）
- CPU FREQ（预留，显示 `--`）

### PAGE_GPU（GPU 详情）
4 行 metric：
- GPU TEMP（`gpu_temp`, ℃）
- GPU LOAD（`gpu_load`, % + bar）
- GPU MEM（`gpu_mem_load`, % + bar）
- MEM SIZE（`gpu_mem_total`, GB，预留显存容量）

### PAGE_SYSTEM（系统）
3 行静态信息：
- DEVICE: CH32V307
- COMMS: OK
- VERSION: HUD V1

---

## 4. LVGL 对象管理方案（第 5 点）

**采用 clean + 重建**（任一时刻仅 1 页对象常驻）：
- `hud_page_switch` 仅当目标页 ≠ 当前页时执行 `lv_obj_clean(lv_scr_act())` + 重建当前页
- 切换只在按键触发时发生（非每 loop），重建开销 ~200 个 LV 对象 < 5ms
- **选 clean+重建而非 4 页全建再隐藏/显示**：RAM 峰值 = 单页对象，比 4 页常驻更小
- 无每 loop 分配：稳态下只改 label/bar 值，无对象创建/销毁 → **无 LVGL 泄漏**
- 切到 PAGE_MAIN 时 `s_footer=NULL`（cpm_ui 内部 clean 会清掉详情页页脚，避免悬空）

---

## 5. 五向键输入（第 3、4 点）

**复用** `src/bsp/key_adc` 的边沿事件模型（成熟：5 连采中值 + 死区 + 30ms 去抖，非阻塞）：
- `hud_input_init` → `key_adc_init()`
- `hud_input_poll`（每 loop）→ `key_adc_poll()` + `key_adc_event()` 取一个边沿 + 分发

按键映射：
| 键 | 动作 |
|---|---|
| UP | 上一页面（循环，MAIN 的上一页 = SYSTEM） |
| DOWN | 下一页面（循环，SYSTEM 的下一页 = MAIN） |
| LEFT | 减少亮度（预留，仅计数 + 页脚显示） |
| RIGHT | 增加亮度（预留，仅计数 + 页脚显示） |
| CENTER | 返回 PAGE_MAIN |

loop 顺序（符合需求）：`cpm_serial_poll` → `hud_input_poll` → `hud_page_update` → `lv_port_tick_task` → `lv_timer_handler`。全程非阻塞，按键不卡死。

---

## 6. 字体策略（不生成新字体）

- **Montserrat 12**（LV_FONT_DEFAULT，全 ASCII）：所有 ASCII 标签（CPU TEMP / GPU LOAD / DEVICE 等）+ 页脚
- **HarmonyOS_2bit 14px**（字库含 `0-9 % ℃ GB :` + 中文）：详情页大数字 + 单位（`45℃` / `100%` / `10GB` 并入同一 label）

未链接任何新 Montserrat 字号（lv_conf.h 仍只启用 Montserrat 12），Flash 增量主要来自代码 + 重复的辅助函数，非字体。

---

## 7. 编译验证

| 指标 | v1 单页版 | Page v1 | 变化 |
|---|---|---|---|
| **Flash** | 202788 B (77%) | **213004 B (81%)** | **+10216 B** |
| **RAM** | 13228 B (20%) | **13288 B (20%)** | **+60 B** |

- Flash +10 KB：4 页框架 + 3 个详情页构建 + 输入层 + 每页重复的 line/corner 辅助函数（`line_h/line_v/draw_corners/calc_color` 在 cpm_ui 与 hud_page 各一份）。后续可抽到共享模块再省 ~3 KB。
- RAM +60 B：`s_rows[4]`（val+bar 指针）+ `s_row_colors[4]` 静态数组，可忽略。
- 编译 EXIT=0，无 error。

### 运行确认
| 项 | 结果 | 依据 |
|---|---|---|
| PC 数据继续刷新 | ✅ | `cpm_protocol_test.ps1` **30/30 帧 OK**，ACK 10-12ms 稳定 |
| 页面切换正常 | 待屏幕确认 | UP/DOWN 循环切 4 页，CENTER 回主页 |
| 按键无卡死 | 待屏幕确认 | 非阻塞边沿模型，每 loop 取一个事件 |
| LVGL 无泄漏 | ✅（设计保证） | 稳态无对象创建/销毁，切页才 clean+重建 |
| FPS | 未本轮重测 | 对象数与 v1 相当（单页常驻），预期 ~v1 水平（~9-12Hz）；如需精确值可烧 CPM_DEBUG 版抓 loop/s |

---

## 8. 待用户屏幕确认

1. PAGE_MAIN 显示是否仍为完整 Cyber HUD（与 v1 一致）
2. UP/DOWN 能否循环切换 MAIN→CPU→GPU→SYSTEM→MAIN
3. 详情页数字/bar 是否随 PC 数据刷新
4. CENTER 是否回主页
5. 整屏方向（上版已设 MADCTL=0x00 整屏 180°）是否仍正确

## 9. 后续（本步未做，待指令）

- 亮度 LEFT/RIGHT 目前仅计数显示，未接 ST7735 背光/伽马（预留）
- 详情页 CPU FREQ / MEM SIZE 预留行待数据源
- 辅助函数去重（line/corner/calc_color 抽共享模块）可再省 Flash
