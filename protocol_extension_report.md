# CPM 协议扩展报告（PC_HUD_Cyber）

> 日期：2026-09-21
> 目标：扩展 PC 端采集数据，使 HUD 显示更多系统信息（频率 + 网络）
> 帧格式不变：`[0x5A][0xA5][CMD][VAL]`（4 字节定长，无 CRC）
> 应答格式不变：收到 `5A A5 CMD VAL` → 回 `5A A5 CMD FF`

---

## 1. 新增 / 扩展 CMD 列表

| CMD | 含义 | 单位 / 换算 | 状态 |
|-----|------|-------------|------|
| 0x01 | CPU 温度 | °C | 原有 |
| 0x02 | CPU 利用率 | % | 原有 |
| 0x03 | RAM 利用率 | % | 原有 |
| 0x04 | RAM 已用 | GB | 原有 |
| 0x05 | RAM 总量 | GB | 原有 |
| 0x06 | GPU 温度 | °C | 原有 |
| 0x07 | GPU 利用率 | % | 原有 |
| 0x08 | GPU 显存利用率 | % | 原有 |
| 0x09 | GPU 显存已用 | GB | 原有 |
| 0x0A | GPU 显存总量 | GB | 原有 |
| **0x0B** | **CPU 频率** | **100 MHz/VAL（36 = 3600 MHz）** | **新增** |
| **0x0C** | **GPU 频率** | **100 MHz/VAL（24 = 2400 MHz）** | **新增** |
| **0x0D** | RAM 已用 | GB（**别名 → 0x04**，复用同字段） | **新增（别名）** |
| **0x0E** | RAM 总量 | GB（**别名 → 0x05**） | **新增（别名）** |
| **0x0F** | 显存已用 | GB（**别名 → 0x09**） | **新增（别名）** |
| **0x10** | 显存总量 | GB（**别名 → 0x10**→0x0A） | **新增（别名）** |
| **0x11** | 网络下载速度 | **10 MB/s/VAL（12 = 120 MB/s）** | **新增** |
| **0x12** | 网络上传速度 | **10 MB/s/VAL（2 = 20 MB/s）** | **新增** |
| 0xFF | 握手 | 收 `5A A5 FF 01` → 回 `5A A5 FF 10` | 原有 |

> 协议冲突检查：原工程已用到 0x01–0x0A + 0xFF，新增 0x0B–0x12 无冲突。
> 0x0D–0x10 采用“别名映射”策略：CH32 端直接写入与 0x04/0x05/0x09/0x0A 相同的字段，
> 不重复占用内存，兼容未来不同版本的 PC 上位机。

---

## 2. PC 端修改文件（`_migration_analysis\cpm`）

| 文件 | 改动 |
|------|------|
| **`net.h`**（新建） | `Net` 类声明：`sample()` 返回下载速度（10MB/s/VAL）、`getUpSpeed()` 返回缓存上传速度；成员 `lastIn/lastOut/lastTick/firstSample/upVal` |
| **`net.cpp`**（新建） | `GetIfTable` 差分采集：遍历物理网卡（跳过 loopback / 无 MAC 伪接口），累加 `dwInOctets/dwOutOctets`，按 `dOctets / (dtMs × 10000)` 换算成 10MB/s/VAL，int64 差分防 32 位 wrap |
| `cpu.h` / `cpu.cpp` | 加 `getCpuFreq()` → 调 `Wmi::getCpuFreqForWmi()` |
| `wmi.h` / `wmi.cpp` | 加 `getCpuFreqForWmi()`：查询 `Win32_Processor.CurrentClockSpeed`（kHz），局部 enumerator 避免污染成员，返回 MHz |
| `gpu.h` / `gpu.cpp` | 加 `gpuFreq` 成员 + `nvmlDeviceGetClockInfo`（3 参，`NVML_CLOCK_GRAPHICS`）函数指针；`upDate()` 里取 GPU 核心频率（MHz）；加 `getGpuFreq()` |
| `hardware.h` | `CpuInfo`/`GpuInfo` 加 `freq` 字段；新增 `NetInfo{down,up}`；`Hardware` 加 `getNetInfo()` + `Net* net` 成员 |
| `hardware.cpp` | 构造函数 new `Net`；`getCpuInfo/getGpuInfo` 填 freq；新增 `getNetInfo()` |
| `computer_performance_monitor.cpp` | 加 `/test` 固定数据模式（命令行解析）；刷新循环加网络采集 + 发送 0x0B/0x0C/0x11/0x12 + 0x0D–0x10 别名 |
| `computer_performance_monitor.vcxproj` | 加 `net.cpp`（ClCompile）+ `net.h`（ClInclude）；4 个配置加 `/utf-8` 编译选项 |

### 单位换算（PC → CH32 的 8-bit VAL）
- CPU/GPU 频率：`VAL = MHz / 100`，上限 255（= 25.5 GHz）
- 网络：`VAL = dOctets / (dtMs × 10000)`，上限 255（= 2.55 GB/s）

### /test 固定测试数据（用于验证协议，与任务要求一致）
| 项 | VAL | 显示 |
|----|-----|------|
| CPU_FREQ (0x0B) | 36 | 3600 MHz |
| GPU_FREQ (0x0C) | 24 | 2400 MHz |
| NET_DOWN (0x11) | 12 | ↓ 120 MB/s |
| NET_UP (0x12) | 2 | ↑ 20 MB/s |

启动方式：`computer_performance_monitor.exe /test`

### PC 构建结果
- 工具链：MSBuild 2022 BuildTools，`Release | x64`
- 输出：`x64\Release\computer_performance_monitor.exe`
- **构建成功（exit 0）**
- 关键修复：原工程源文件是 GBK 无 BOM，新增 UTF-8 中文注释导致字节混码（`C2001 常量中有换行` / `Wmi 类解析失败`）。统一转为 UTF-8 with BOM + 工程加 `/utf-8` 后消除。
- 网络模块坑：SDK 10.0.18362 的 `iphlpapi.h` 未声明 `GetIfTable2`/`MIB_IFTABLE2`，改回经典 `GetIfTable`（`MIB_IFTABLE`，数组成员名 `table` 小写）。

---

## 3. CH32 端修改文件（`LVGL_Demo\src\app`）

| 文件 | 改动 |
|------|------|
| `cpm_serial.h` | `CPM_Data` 加 `cpu_freq / gpu_freq / net_down / net_up`；加 CMD 常量 0x0B/0x0C/0x0D/0x0E/0x0F/0x10/0x11/0x12；注释补单位 |
| `cpm_serial.cpp` | `cpm_dispatch` 加 0x0B/0x0C/0x11/0x12 分支 + 0x0D/0x0E/0x0F/0x10 别名分支（复用现有字段） |
| `hud_page.h` | 加 `PAGE_DETAIL` 页，`HUD_PAGE_COUNT` 5 → 6 |
| `hud_page.cpp` | 新增 `build_page_detail()`（频率 + 网络显示）；CPU 页 FREQ 行接真实 `cpu_freq`；update 去抖改为按页 4 字段 `s_last_key[4]` 快照（避免无关字段触发重绘）；页脚 `P%d/6` |

### PAGE_DETAIL 显示内容
```
SYS DETAIL
CPU: 3600MHz
GPU: 2400MHz
NET:  ↓ 120MB/s  ↑ 20MB/s
P6/6 B4
```

### CH32 构建结果（Flash / RAM 增量）
| 项 | 基线 | 扩展后 | 增量 | 目标 | 达标 |
|----|------|--------|------|------|------|
| Flash | 214896 B (81%) | 215460 B (82%) | **+564 B** | < 15 KB | ✅ |
| RAM | 13552 B (20%) | 13556 B (20%) | **+4 B** | < 5 KB | ✅ |

---

## 4. 测试数据 / 验证

### 协议测试脚本（已扩展）
`LVGL_Demo\cpm_protocol_test.ps1` 帧列表从 0x01–0x0A（10 CMD）扩到 **0x01–0x12（18 CMD）**，3 轮共 54 帧：

```
0x01,45  0x02,23  0x03,45  0x04,8   0x05,16
0x06,52  0x07,67  0x08,71  0x09,9   0x0A,16
0x0B,36  0x0C,24  0x0D,8   0x0E,16  0x0F,9
0x10,16  0x11,12  0x12,2
```
判定标准：收到 `5A A5 CMD FF` 即 OK（CMD 回显 + 0xFF 尾）。

### 当前验证状态
| 步骤 | 状态 | 说明 |
|------|------|------|
| PC exe 构建 | ✅ 成功 | Release x64，exit 0 |
| CH32 编译 | ✅ 成功 | Flash 215460 / RAM 13556，达标 |
| CH32 烧录 | ✅ **已烧录** | `upload_only.ps1`（arduino-cli 路径）报 `missing close-brace`（Tcl 转义问题）；改用 `upload_no_verify.ps1`（手动 openocd program）**成功**：`Programming Finished / EXIT 0` |
| 全 CMD 协议测试 | ✅ **54/54 = 100%** | 0x01–0x12 全 CMD × 3 轮全 OK（2026-09-22 18:13），含新增 0x0B/0x0C/0x0D/0x0E/0x0F/0x10/0x11/0x12 |
| PC exe /test 运行 | ✅ 运行中 | pid=20676，已稳定 10 s，持续发固定测试数据 |
| LVGL 显示验证 | ⏳ 待看屏 | 按 5 向键切到第 6 页（P6/6），应显示 CPU: 3600MHz / GPU: 2400MHz / NET: ↓120MB/s ↑20MB/s |

---

## 5. 下一步建议

1. **看屏确认**（当前最后一步）：PC exe `/test` 已运行，按 5 向键切到第 6 页（页脚 `P6/6`），确认显示 `CPU: 3600MHz / GPU: 2400MHz / NET: ↓120MB/s ↑20MB/s`。
2. **实机跑正常模式**（去掉 `/test`）：验证真实 CPU/GPU 频率、网络速度是否合理（频率非 0、网络在有流量时非 0）。
3. **网络采集精度**：`GetIfTable` 计数器 32 位，高负载下可能 wrap（已用 int64 差分缓解，但单接口 > 4 GB 累计仍会回绕）；若需长期精度可升级 SDK 后改用 `GetIfTable2`（64 位计数）。
4. **烧录脚本**：arduino-cli 上传路径当前报 `missing close-brace`，稳定路径是 `upload_no_verify.ps1`（手动 openocd program + wlink_reset_resume）。
5. **可选扩展**：当前网络只显示当前速度。后续可在历史曲线页把 `net_down` 也加入 `hud_history` 采样（需评估 RAM，历史缓冲每路约 64 B）。
6. **单位说明写进 UI**：频率/网络是“近似量”（100 MHz / 10 MB/s 步进），如需更精确可考虑双字节 VAL 或独立多字节帧（会破坏 4 字节定长约定，需协议版本化，暂不建议）。

---

## 附：文件清单速查

**PC 端**（`D:\administrator\Desktop\Hermes_Workshop\_migration_analysis\cpm`）
- 新建：`net.h` `net.cpp`
- 修改：`cpu.h/.cpp` `wmi.h/.cpp` `gpu.h/.cpp` `hardware.h/.cpp` `computer_performance_monitor.cpp` `computer_performance_monitor.vcxproj`

**CH32 端**（`D:\administrator\Desktop\Hermes_Workshop\CH32V307\LVGL_Demo\src\app`）
- 修改：`cpm_serial.h/.cpp` `hud_page.h/.cpp`

**脚本**（`...\CH32V307\LVGL_Demo`）
- `build_upload.ps1`（编译 + 烧录）
- `upload_only.ps1`（仅烧录）
- `cpm_protocol_test.ps1`（协议测试，已扩到 0x01–0x12）
- `cpm_protocol_test.txt`（测试输出）

**备份**：`...\CH32V307\backup_protocol_ext_20260922_164245\pc_cpm_src`（PC 原始 GBK 源 + 资源）
