# HUD Graph 报告 — 实时历史曲线页 (PAGE_GRAPH)

日期: 2026-09-22
分支: `feature/adc-lvgl-input` (colinkyd/lvgl-ch32v307)
基线: Page v1 (4 页: MAIN/CPU/GPU/SYSTEM, commit 183ee97, Flash 212992 B / RAM 13288 B)

---

## 1. 本步骤目标

在 4 页框架基础上增加 **PAGE_GRAPH 实时历史曲线页**：
- lv_chart 双线显示 CPU/GPU 使用率（0-100，60 点滚动）
- 60 点历史缓存，500ms 采样一次（非阻塞）
- 顶部 PERFORMANCE GRAPH / 中间折线 / 底部当前值

**约束（已遵守）**：不改 PC 通信协议、不改 `cpm_serial.cpp`、不改 `CPM_Data`，只加历史缓存 + Chart 页。

---

## 2. 新增文件

| 文件 | 职责 |
|---|---|
| `src/app/hud_history.h` | `HUD_HISTORY`（4×60 点）+ init/sample/get/latest 接口 |
| `src/app/hud_history.cpp` | 环形缓冲实现，500ms 采样写入最旧点 |

修改文件：
| 文件 | 改动 |
|---|---|
| `src/app/hud_page.h` | `HUD_PAGE` 加 `PAGE_GRAPH`，`HUD_PAGE_COUNT` 4→5 |
| `src/app/hud_page.cpp` | 建 `build_page_graph`（lv_chart 双线 + 图例 + 当前值）、500ms 采样接入 update、页脚 `P%d/5` |

`cpm_serial.cpp` / 协议 / `CPM_Data` **未改**。

---

## 3. 数据缓存（第 2、3 点）

```c
#define HUD_HISTORY_LEN 60   /* 60 * 500ms = 30s 窗口 */
typedef struct {
  uint8_t cpu_load[60];
  uint8_t gpu_load[60];
  uint8_t cpu_temp[60];
  uint8_t gpu_temp[60];
} HUD_HISTORY;
```

- 静态 `HUD_HISTORY`（非堆分配），**240 字节** + 1 字节索引
- `hud_history_sample()` 环形覆盖最旧点（`s_idx = (s_idx+1) % 60`），仅 4 次赋值，无分配 → **非阻塞**
- 500ms 门控：`hud_page_update()` 里 `millis()` 判断，`now - s_sample_ms >= 500` 才采样 → **不每 loop 保存**
- 历史缓冲**全局持续记录**（不论当前在哪页），切到 GRAPH 页即有 30s 数据

---

## 4. LVGL Chart（第 4、5、6 点）

使用 `lv_chart`（本库为 8.3.11 增强版，API：`set_update_mode` + `set_next_value` + `set_div_line_count`）：

```
lv_chart_set_type(s_graph, LV_CHART_TYPE_LINE);
lv_chart_set_point_count(s_graph, 60);
lv_chart_set_range(s_graph, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
lv_chart_set_div_line_count(s_graph, 2, 0);
lv_chart_set_update_mode(s_graph, LV_CHART_UPDATE_MODE_CIRCULAR);  /* 环形滚动 */
ser_cpu = lv_chart_add_series(..., C_CPU青, PRIMARY_Y);
ser_gpu = lv_chart_add_series(..., C_GPU品红, PRIMARY_Y);
```

- **滚动**：`LV_CHART_UPDATE_MODE_CIRCULAR`，chart 内部环形滚动，采样时只需 `lv_chart_set_next_value(ser, val)`，**无需手动搬 60 点**，开销极小
- **颜色**：CPU 青 `0x00e5ff`、GPU 品红 `0xff2d95`，一色一条，**无图片**
- 无动画（chart 无 anim，直接刷新）

PAGE_GRAPH 布局（128×160）：
```
┌────────────────┐
│ PERFORMANCE GRAPH │  ← 标题 (Montserrat 12)
│  CPU / GPU LOAD   │  ← 图例
│  ■CPU  ■GPU       │  ← 色块 + 标签
┌────────────────┐
│                │
│   ╱╲  CPU 青   │
│  ╱  ╲╱╲        │  ← chart 128x100
│ ╱╲╱   ╱╲ GPU品红│
│  0        100   │
└────────────────┘
│  CPU 45% GPU 30%  │  ← 底部当前值 (每帧刷新)
│   P5/5 B4          │  ← 页脚
└────────────────┘
```

---

## 5. 输入集成（第 8 点）

5 页循环（`hud_input` 的 `% HUD_PAGE_COUNT` 自动适配）：
```
MAIN → CPU → GPU → SYSTEM → GRAPH → MAIN
```
UP/DOWN 循环切，CENTER 回 MAIN。无需改 `hud_input.cpp`。

---

## 6. 内存限制（第 7 点）

| 指标 | Page v1 (4 页) | Graph (5 页) | 变化 | 目标 |
|---|---|---|---|---|
| **Flash** | 212992 B (81%) | **214772 B (81%)** | **+1780 B** | <10KB ✅ |
| **RAM** | 13288 B (20%) | **13552 B (20%)** | **+264 B** | <8KB ✅ |

- **Flash +1.78 KB**：lv_chart 双线 + build_page_graph + 图例对象，远小于 10KB 上限
- **RAM +264 B**：`HUD_HISTORY` 240 B（静态）+ series 指针 + 采样计时器，远小于 8KB 上限
- chart 用 **CIRCULAR 模式**（内部环形，无每采样 memcpy 60 点）+ **无动画**，运行期开销极低

---

## 7. 编译验证

- `compile_check.ps1` **EXIT=0**，无 error
- Flash 214772 B (81%) / RAM 13552 B (20%)（均达标）
- 编译期修正 1 处：`build_page_graph` 在 `build_footer` 定义前调用 → 加静态前向声明

---

## 8. 运行确认

| 项 | 结果 | 依据 |
|---|---|---|
| 曲线滚动正常 | 待屏幕确认 | CIRCULAR 模式，500ms 新点入队，旧点左移 |
| PC 数据继续刷新 | ✅ | `cpm_protocol_test.ps1` **30/30 帧 OK**，ACK 11ms 稳定 |
| 页面切换正常 | 待屏幕确认 | 5 页循环，CENTER 回 MAIN |
| 无 LVGL 卡顿 | 待屏幕确认 | chart 无动画，500ms 才更新一次，稳态只改当前值 label |
| LVGL 无泄漏 | ✅（设计保证） | 稳态无对象创建/销毁，切页才 clean+重建 |

已烧录正式版 + 重启上位机，屏幕现在应是 PAGE_MAIN。

---

## 9. 待用户屏幕确认

1. 连续按 DOWN 5 次：MAIN→CPU→GPU→SYSTEM→**GRAPH**，能进曲线页
2. GRAPH 页：CPU 青线 / GPU 品红线 是否随 PC 负载变化滚动
3. 底部 `CPU xx% GPU xx%` 是否实时刷新
4. 顶部 PERFORMANCE GRAPH + 图例是否清晰不重叠
5. 整屏方向（MADCTL=0x00）是否仍正确

## 10. 后续（本步未做，待指令）

- temp 缓存已记录但 GRAPH 页只显示 load（按需求）；后续可加 CPU/GPU TEMP 曲线（再 +2 series）
- chart 可加 Y 轴刻度标签（当前仅 2 条横线，无文字刻度）
- 500ms 采样间隔可调（改 `hud_page_update` 的 `>= 500` 常量）
