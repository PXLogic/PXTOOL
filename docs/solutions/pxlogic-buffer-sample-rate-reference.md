# PX-Logic Buffer / 采样率 / 深度 参考说明

本文档说明 DSView 顶部工具栏 **Buffer** 下拉列表的生成方式，以及 **PX-Logic U3 channel 32** 设备在不同通道模式、采样率下的预期结果，便于手工测试对比。

---

## 1. 相关代码位置

| 层级 | 文件 | 说明 |
|------|------|------|
| UI | `DSView/pv/toolbars/samplingbar.cpp` | `update_sample_count_selector()`：根据 `hw_depth`、采样率、模式动态生成 Buffer 时间列表 |
| UI 常量 | `DSView/pv/toolbars/samplingbar.h` | `LogicMaxSWDepth64 = SR_GB(16)`（64 位逻辑分析仪软件深度上限） |
| 驱动 | `libsigrok4DSL/hardware/pxlogic/pxlogic.c` | `config_get(SR_CONF_HW_DEPTH)`：返回每通道可用采样点数 |
| 设备能力 | `libsigrok4DSL/hardware/pxlogic/pxlogic.h` | `supported_PX[]` 中 `dev_caps.hw_depth`；`channel_modes[]` 中通道数、`unit_bits`、最大采样率 |
| 采样率表 | `libsigrok4DSL/hardware/pxlogic/pxlogic.h` | `samplerates[]` 全局表；`adjust_samplerate()` 按通道模式裁剪可用范围 |

---

## 2. 核心概念

### 2.1 `hw_depth`（驱动返回给 UI 的值）

在 **Buffer 模式**（`OP_BUFFER`）下，驱动通过 `SR_CONF_HW_DEPTH` 返回的是 **每个启用通道** 的采样点数（samples per channel），不是总 bit 数：

```
hw_depth (per channel) = profile.dev_caps.hw_depth
                       / channel_modes[ch_mode].unit_bits
                       / en_ch_num
```

- `profile.dev_caps.hw_depth`：设备 profile 中的物理深度（单位与 `SR_Gn` 等宏一致，见下节）
- `unit_bits`：每采样点占用的 bit 数（PX-Logic 各通道模式目前均为 **1**）
- `en_ch_num`：当前 **已启用** 的逻辑通道数（`en_ch_num(sdi)`，非模式名义通道数）

**PX-Logic U3 channel 32** 的 profile 中：

```c
hw_depth = SR_Gn(4)   // = 4 × 10^9
```

### 2.2 Buffer 下拉列表显示的是「时间」

UI 不维护固定档位表，而是根据 **最大可采集时间** 从大到小按 1-2-5 规律递减生成一串 `duration`（秒），再用 `sr_time_string()` 格式化为 `500.00 ms`、`2.00 us` 等。

### 2.3 最大 Buffer 时间（Logic 模式）

设当前采样率为 `samplerate`（Hz），`SR_SEC(1) = 1e9`（纳秒单位下的 1 秒常量，与 `duration` 计算配套使用）：

```cpp
hw_duration = hw_depth / (samplerate * (1.0 / SR_SEC(1)));
```

等价于：

```
duration_max = hw_depth / samplerate   （单位：秒）
```

**分支：**

| 条件 | 用于生成列表的最大 `duration` |
|------|--------------------------------|
| Buffer 模式，无 RLE | `hw_duration` |
| Stream 模式 | `sw_depth / samplerate` |
| 支持 RLE | `min(hw_depth × 1024, sw_depth) / samplerate` |
| DSO | 使用 `SR_CONF_MAX_TIMEBASE` |

64 位 macOS / x86_64 下 Logic 的 `sw_depth`：

```cpp
sw_depth = LogicMaxSWDepth64 = SR_GB(16)   // 16 × 1024^3 samples
```

---

## 3. PX-Logic U3 channel 32 设备参数

### 3.1 物理深度（所有 U3 32 路 profile 一致）

| 字段 | 值 |
|------|-----|
| `dev_caps.hw_depth` | `SR_Gn(4)` = **4,000,000,000** |
| 默认通道模式（当前代码） | `BUFFER_LOGIC1000x32` |

### 3.2 Buffer 通道模式（`channel_modes[]`）

| 模式 ID | 名称 | 名义通道数 `num` | `unit_bits` | 最大采样率 |
|---------|------|------------------|-------------|------------|
| `BUFFER_LOGIC250x32` | Use 32 Channels (Max 250MHz) | 32 | 1 | 250 MHz |
| `BUFFER_LOGIC500x16` | Use 16 Channels (Max 500MHz) | 16 | 1 | 500 MHz |
| `BUFFER_LOGIC1000x8` | Use 8 Channels (Max 1000MHz) | 8 | 1 | 1 GHz |
| `BUFFER_LOGIC1000x32` | Use 32 Channels (Max 1000MHz) | 32 | 1 | 1 GHz |

**注意：** `BUFFER_LOGIC250x32` 与 `BUFFER_LOGIC1000x32` 在 **32 路全启用** 时，每通道 `hw_depth` 相同（均为 125M samples），区别是 **可用采样率上限** 不同。

### 3.3 每通道深度（启用通道数 = 模式名义通道数时）

```
depth_per_ch = 4,000,000,000 / unit_bits / ch_num
```

| 通道模式 | ch_num | depth_per_ch (samples) |
|----------|--------|-------------------------|
| 32 路（250M / 1000M） | 32 | **125,000,000** |
| 16 路 | 16 | **250,000,000** |
| 8 路 | 8 | **500,000,000** |

若用户只启用部分通道，`en_ch_num` 变小，则 `hw_depth`（每通道）会 **变大**（总内存按启用通道平分）。

---

## 4. 最大 Buffer 时间速查表（Buffer 模式）

公式：`T_max = depth_per_ch / samplerate`

### 4.1 32 通道（125M samples/ch）

| 采样率 | T_max |
|--------|-------|
| 1 GHz | **125 ms** |
| 800 MHz | 156.25 ms |
| 500 MHz | **250 ms** |
| 400 MHz | 312.5 ms |
| 250 MHz | **500 ms** |
| 200 MHz | 625 ms |
| 125 MHz | **1 s** |
| 100 MHz | 1.25 s |
| 50 MHz | 2.5 s |
| 25 MHz | 5 s |
| 10 MHz | 12.5 s |
| 1 MHz | 125 s |
| 100 kHz | 1250 s |
| 2 kHz | 62500 s |

### 4.2 16 通道（250M samples/ch）

| 采样率 | T_max |
|--------|-------|
| 1 GHz | —（该模式最大 500 MHz） |
| 500 MHz | **500 ms** |
| 250 MHz | **1 s** |
| 125 MHz | **2 s** |
| 100 MHz | 2.5 s |
| 50 MHz | 5 s |
| 10 MHz | 25 s |
| 1 MHz | 250 s |

### 4.3 8 通道（500M samples/ch）

| 采样率 | T_max |
|--------|-------|
| 1 GHz | **500 ms** |
| 500 MHz | **1 s** |
| 250 MHz | **2 s** |
| 125 MHz | **4 s** |
| 100 MHz | 5 s |
| 50 MHz | 10 s |
| 10 MHz | 50 s |
| 1 MHz | 500 s |

---

## 5. Stream 模式最大 Buffer 时间（64 位）

`duration_max = SR_GB(16) / samplerate`（与通道数无关，仅受软件内存上限约束；实际还受 stream 通道模式最大采样率限制）。

| Stream 通道模式 | 最大采样率 | T_max (64-bit) |
|-----------------|------------|----------------|
| STREAM_LOGIC50x32 | 50 MHz | ≈ 343 s |
| STREAM_LOGIC125x16 | 125 MHz | ≈ 137 s |
| STREAM_LOGIC250x8 | 250 MHz | ≈ 68.7 s |
| STREAM_LOGIC500x4 | 500 MHz | ≈ 34.4 s |
| STREAM_LOGIC1000x2 | 1 GHz | ≈ 17.2 s |

---

## 6. Buffer 列表生成算法

实现：`SamplingBar::update_sample_count_selector()`（`samplingbar.cpp`）

### 6.1 起始值

从上一节得到的 `duration`（最大时间）开始。

### 6.2 递减规则（1-2-5 步进）

根据当前 `duration` 选择时间单位 `unit`（天 / 时 / 分 / 秒），再按下列顺序缩小：

1. 若 `duration > 5 × 10^floor × unit` → 设为 `5 × …`
2. 否则若 `> 2 × …` → 设为 `2 × …`
3. 否则若 `> 1 × …` → 设为 `1 × …`
4. 否则 → `× 0.5` 或特殊边界（小时/分钟等）

典型在秒级以下表现为：**… → 500ms → 200ms → 100ms → 50ms → 20ms → 10ms → 5ms → 2ms → 1ms → 500µs → …**

### 6.3 停止条件（最小项）

Logic 模式下循环继续条件：

```cpp
not_last = (duration / SR_SEC(1) * samplerate >= SR_KB(1));
```

即要求：

```
duration × samplerate ≥ 1024   （samples）
```

**最小 Buffer 时间** 为满足上式的、由 1-2-5 序列递减能到达的 **最大** 那一档（列表最后一项）。

常用近似（Logic）：

| 采样率 | 理论下限 1024/sr | 列表最小档（实测规律） |
|--------|------------------|------------------------|
| 1 GHz | 1.024 µs | **2 µs**（1 µs × 1G = 1000 < 1024，不入选） |
| 500 MHz | 2.048 µs | **5 µs** |
| 250 MHz | 4.096 µs | **5 µs** |
| 100 MHz | 10.24 µs | **20 µs** |
| 10 MHz | 102.4 µs | **200 µs** |
| 1 MHz | 1.024 ms | **2 ms** |
| 100 kHz | 10.24 ms | **20 ms** |
| 10 kHz | 102.4 ms | **200 ms** |
| 2 kHz | 512 ms | **1 s** |

---

## 7. 已验证示例：8 通道 @ 1 GHz

**配置：**

- 设备：PX-Logic U3 channel 32  
- Device Options：Use 8 Channels (Max 1000MHz)（`BUFFER_LOGIC1000x8`）  
- Operation Mode：Buffer  
- Sample Rate：1 GHz  

**计算：**

```
depth_per_ch = 4e9 / 1 / 8 = 500,000,000 samples
T_max = 500e6 / 1e9 = 0.5 s = 500 ms
```

**预期 Buffer 列表（17 项，自上而下）：**

```
500.00 ms
200.00 ms
100.00 ms
 50.00 ms
 20.00 ms
 10.00 ms
  5.00 ms
  2.00 ms
  1.00 ms
500.00 us
200.00 us
100.00 us
 50.00 us
 20.00 us
 10.00 us
  5.00 us
  2.00 us    ← 最小项（无 1.00 us）
```

与 UI 截图一致。

---

## 8. 采样率列表（PX-Logic 全局表）

`pxlogic.h` 中 `samplerates[]` 共 33 档（2 kHz～1 GHz）。  
实际下拉内容由 `adjust_samplerate()` 按当前 `ch_mode` 的 `min_samplerate` / `max_samplerate` 裁剪后，再由 `config_list(SR_CONF_SAMPLERATE)` 返回。

切换 **Channel Mode** 或 **Operation Mode** 后，应触发 `SamplingBar::update_sample_rate_list()` 与 `update_sample_count_selector()` 刷新（见 `sig_channel_mode_changed` 等改动）。

---

## 9. 测试检查清单

1. **设备**：PX-Logic U3 channel 32，Buffer 模式。  
2. **通道模式**：分别测 32 / 16 / 8 路 Buffer 模式，核对 §4 中 `T_max`。  
3. **采样率**：在允许范围内选 1 GHz、500 MHz、250 MHz、10 MHz、1 MHz 等，核对第一项与 §6.3 最小项。  
4. **全列表**：对照 §6.2 的 1-2-5 序列，项数与中间档位应连续递减。  
5. **Stream**：切换到 Stream 模式，最大时间应接近 §5（且不超过该 stream 模式最大采样率）。  
6. **部分启用通道**：禁用部分通道后，`hw_depth` 增大，最大 Buffer 时间应变长。  

---

## 10. 修订记录

| 日期 | 说明 |
|------|------|
| 2026-05-23 | 初版：PX-Logic U3 Buffer 计算、深度表、列表算法；修正 1 GHz 最小档为 2 µs（非 1 µs） |
