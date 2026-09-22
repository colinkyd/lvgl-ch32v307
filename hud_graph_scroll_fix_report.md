# HUD Graph 滚动修复报告 (PAGE_GRAPH 示波器式滚动)

日期: 2026-09-22
分支: `feature/adc-lvgl-input` (colinkyd/lvgl-ch32v307)
基线: Graph v1 (CIRCULAR 模式, Flash 214712 B / RAM 13552 B)

---

## 1. 问题现象

PAGE_GRAPH 页面：
- 第一屏数据从左向右逐点绘制
- 填满 60 点后曲线不再变化（停在"第一屏"）
- 缺少示波器式持续左移滚动

## 2. 根因（源码级定位）

本库 LVGL `lv_chart.c`：

- `lv_chart_set_next_value()`（line 538）两种模式写入逻辑**相同**：
  `ser->y_points[ser->start_point] = value; ser->start_point = (start_point+1) % point_cnt;`
- **区别在绘制 x 起点**：`lv_chart_get_x_start_point()`（line 267）
  - `LV_CHART_UPDATE_MODE_CIRCULAR` → 返回 `0`：绘制时 `id = (0 + id) % 60`，即**原地覆盖**最旧槽位，x 坐标不动 → 填满后视觉上"停在第一屏"
  - `LV_CHART_UPDATE_MODE_SHIFT` → 返回 `ser->start_point`：绘制时 `id = (start_point + id) % 60`，整条曲线**相对 start_point 重新映射 x** → 新点出现在最右，旧点整体左移

Graph v1 用了 `LV_CHART_UPDATE_MODE_CIRCULAR` → 原地覆盖，不左移。

## 3. 修复方案（按需求 6 点逐条落实）

| 需求 | 落实 |
|---|---|
| 1. 用 `LV_CHART_UPDATE_MODE_SHIFT` | `hud_graph_create()` 中 `lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT)` |
| 2. 禁 `set_all_value` 每帧 | `set_all_value` 仅在 `hud_graph_create()` 初始化调用一次；每 500ms 只调 `lv_chart_set_next_value()` |
| 3. point_count=60 + 初始化 | `lv_chart_set_point_count(60)`；初始化所有点平铺当前 CPU/GPU 值（无数据则 0） |
| 4. 数据流 | `500ms tick → CPM_Data.cpu_load/gpu_load → hud_history_push() → lv_chart_set_next_value() → LVGL 自动 shift` |
| 5. 单 chart 长期存在 | chart 在 `hud_graph_create()` 建一次，本页存活期间不删不重建不清 series；切离页随 screen `lv_obj_clean` 销毁 + `hud_graph_destroy()` 清指针 |
| 6. 验证 | 见 §6 |

**未改**：`cpm_serial.cpp`、PC 协议、`CPM_Data`（仅读取 `d->cpu_load/d->gpu_load`）。

## 4. 文件改动

| 文件 | 改动 |
|---|---|
| `src/app/hud_graph.h`（新） | `hud_graph_create(parent)` / `hud_graph_tick()` / `hud_graph_destroy()` 接口 |
| `src/app/hud_graph.cpp`（新） | chart 封装：SHIFT 模式、60 点、CPU 青/GPU 品红双线、`set_all_value` 初始化、500ms 门控 `hud_graph_tick`（内部 `hud_history_push` + 2×`lv_chart_set_next_value`） |
| `src/app/hud_history.h/.cpp` | `hud_history_sample()` 改名 `hud_history_push()`（对齐需求数据流命名） |
| `src/app/hud_page.cpp` | 删除内联 chart 代码（静态 `s_graph/s_ser_cpu/s_ser_gpu/s_sample_ms`），`build_page_graph` 改调 `hud_graph_create(scr)`；`hud_page_update` 的 GRAPH 分支改调 `hud_graph_tick()` + `graph_update_current()` |

布局不变：标题 / 图例（色块+CPU/GPU）/ chart 128×88（y=40）/ 底部 `CPU xx% GPU xx%`（y≈130，避开 chart 底边）/ 页脚 P5/5。

## 5. 性能（需求 6.4：RAM 增加 <5KB）

| 指标 | Graph v1 (CIRCULAR) | Scroll fix (SHIFT) | 变化 | 目标 |
|---|---|---|---|---|
| **Flash** | 214712 B (81%) | **214896 B (81%)** | **+184 B** | — |
| **RAM** | 13552 B (20%) | **13552 B (20%)** | **+0 B** | <5KB ✅ |

- 单 chart 对象长期存在，稳态无对象创建/销毁 → 无 LVGL 内存泄漏
- SHIFT 模式 `set_next_value` 只 `invalidate_point` 两个点（新点 + 旧起点），非整屏重绘（对比 `set_all_value` 的 `lv_chart_refresh` 整对象 invalidate）
- 500ms 门控，非每 loop 采样

## 6. 验证

| 项 | 结果 |
|---|---|
| 编译 | `EXIT=0`，无 error（Flash 214896 B / RAM 13552 B） |
| 烧录 | `flash_manual.ps1` → `Programming Finished` / `OCD EXIT = 0` |
| PC 数据刷新 | `cpm_protocol_test.ps1` **30/30 帧 OK（100%）**（烧录后首测 0/30 为 MCU 复位暂态，等 6s 重跑全过 — 历史多次相同规律） |
| 上位机 | 已重启 PID=2364 |
| 曲线持续右入左移 / 最新在右 / 不停第一屏 / 5 分钟无卡顿 | 待屏幕确认（SHIFT 源码级保证 + 协议 30/30 保证数据流） |

## 7. 待用户屏幕确认

1. 进 GRAPH 页，等 ≥30s（60 点填满），观察曲线是否持续向左滚动、最右侧始终为最新数据
2. 运行 5 分钟：无卡顿、无停帧、无 LVGL 内存增长迹象（切走再切回无异常）
3. 底部 `CPU xx% GPU xx%` 实时刷新正常

## 8. 已知边界

- SHIFT 模式每 tick 重绘整条线（`invalidate_point` 两点 + 线重绘），88px 高 × 128px 宽下开销可忽略（500ms 才一次）
- 切离 GRAPH 页期间不采样（`hud_graph_tick` 只在 GRAPH 页被调）；历史缓存 `hud_history` 也只在 tick 时 push，与 chart 同步，无数据丢失问题（chart 随页销毁本就无数据）
- 若后续要"切走期间继续记录、切回看到 30s 历史"，需把 `hud_history_push` 提到全页面全局 tick（当前按需求只动 chart 相关代码，未扩此行为）
