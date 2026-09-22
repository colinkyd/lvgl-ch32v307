# Cyber HUD UI v1 — CH32V307 性能监视器

> 将普通 4 行性能监视器升级为科技风 HUD（对齐参考图，小屏重设计非缩放）。
> 日期: 2026-09-22
> 约束: 只改 `cpm_ui.cpp` / `cpm_ui.h`；不动 cpm_serial / 协议 / CPM_Data / ST7735 驱动。

## 1. 修改文件
| 文件 | 改动 |
|---|---|
| `src/app/cpm_ui.cpp` | 整体重写：4 行紧凑布局 → Header + 2×2 面板 + Footer 的 HUD 结构 |
| `src/app/cpm_ui.h` | 注释更新为 HUD 结构 + 字体策略说明（接口不变） |

## 2. UI 结构（160×128 横屏，物理 128×160 经 setRotation(3)）

```
Screen (黑底 0x060612)
├─ Header
│   ├─ 左  CH32V307        (Montserrat 12, 灰蓝)
│   ├─ 中  SYSTEM MONITOR  (Montserrat 12, 亮白)
│   └─ 右  ● PC OK         (绿点 lv_obj + 绿字)
├─ 分隔线 (lv_line, 青色 70% opa)
├─ CPU Panel   左上 (青 0x00BFFF)  大数字 "40C 9%"  + bar
├─ GPU Panel   右上 (绿 0x39FF6A)  大数字 "73C 84%" + bar
├─ RAM Panel   左下 (黄 0xFFD23F)  大数字 "49%"     + bar
├─ GPU MEM     右下 (紫 0xC44BD4)  大数字 "93%"     + bar
└─ Footer: PERF  REALTIME  STABLE (Montserrat 12, 灰蓝)
```

- 每面板 = 主题色 1px 边框容器 + 标题 label + 大数字 label + lv_bar(0~100)。
- 仅用 `lv_obj / lv_label / lv_bar`（分隔线用 1px 高 lv_obj，比 lv_line 更省）。
- 无图片背景 / 无大字体资源 / 无 GIF 动画。
- 动态效果只有 **bar 平滑过渡**（`lv_bar_set_value(..., LV_ANIM_ON)`）；大数字/label 仅在数值变化时重设（`memcmp` 去抖）。

### 字体策略（不生成新字体，用现有两种）
- **ASCII 文本/标题/状态**：Montserrat 12（`LV_FONT_DEFAULT`，全 ASCII，已链接进 Flash）。
- **大数字（温度+百分比）**：HarmonyOS_2bit 14px（14px 更大，且 `0-9 C % 空格` 全在字库）。
- 缺字记录：HarmonyOS_2bit 14px 仅 33 字形，缺 `S Y T E O N I H V K L F Q W X` 等大写 → 标题/状态一律走 Montserrat 12；大数字只含 `0-9 C % 空格`，全部命中，无缺字。

## 3. Flash 变化
| 版本 | Flash | RAM |
|---|---|---|
| 之前 4 行版 | 201488 B (76.9%) | 15764 B (24.0%) |
| **Cyber HUD v1** | **202344 B (77.2%)** | **15812 B (24.1%)** |
| 变化 | **+856 B** | **+48 B** |

增量极小：Montserrat 12 与 HarmonyOS 14px 均已链接，delta 只是新增的 Header/Footer/4 面板边框容器对象。

## 4. 编译验证
`compile_check.ps1` → EXIT=0
- Flash: `202344 bytes (77%)`（上限 262144）
- RAM: `15812 bytes (24%)`（上限 65536，余 49724）

## 5. 屏幕效果（待用户确认）
烧录 COM9 成功（OCD EXIT=0），PC 上位机已启动持续发数据。预期屏幕：
- 顶部 `SYSTEM MONITOR`，左 `CH32V307`，右绿点 `PC OK`，下方青色分隔线。
- 左上 CPU 青框大数字（当前 PC 实测 ~40C 9%），右上 GPU 绿框（~73C 84%）。
- 左下 RAM 黄框（~49%），右下 GPU MEM 紫框（~93%）。
- 底部 `PERF REALTIME STABLE`。
- PC 数据变化时：大数字跳变 + bar 平滑过渡。

请确认：1) 四面板布局/边框/颜色是否符合参考图；2) 数据是否实时更新；3) 大数字是否清晰、bar 是否平滑。

## 6. 后续优化建议
1. **缺字补齐**：若要标题用更醒目大字，可给 HarmonyOS 字体补 `S Y T E O N I H V K` 等字形（需生成新字体，违背本任务约束，故暂用 Montserrat 12）。
2. **霓虹发光**：参考图有 glow，可用 `lv_obj_set_style_shadow`（当前 `LV_USE_SHADOW=0`，需开启，耗 Flash）或对大数字叠一层同色 2px 偏移副本模拟。
3. **角标装饰**：参考图面板是 corner-bracket 风格，当前用整框 1px 线；可改 4 角 L 形装饰（4 条短 lv_obj）更接近原图。
4. **CPU/GPU 利用率语义**：参考图 GPU 大数字是利用率 84%，当前 GPU 大数字同时含温度 `73C 84%`（信息更全，符合任务要求"温度+利用率"）。
5. **清理 benchmark**：上一任务分析显示 LVGL `bm_*.c`（1.4KB 源码）已被 `--gc-sections` 回收占 0 Flash，删它不省 Flash；真正大头是 LVGL extra 控件全套（animimg/calendar/chart/colorwheel/led/menu/meter/msgbox/span/spinbox/spinner/tabview/win 全开）。若后续要省 Flash，优先在 `lv_conf.h` 关掉 CPM 用不到的 extra 控件。
