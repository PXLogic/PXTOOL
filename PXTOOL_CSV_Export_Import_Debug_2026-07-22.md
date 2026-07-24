# PXTOOL CSV Export/Import 波形不一致调试记录

日期：2026-07-22

## 背景

使用 Upstream Compat Demo 采集逻辑波形后，通过 DSView/PXTOOL 导出 CSV，再将该 CSV 导入回 PXTOOL。最初表现为导入失败或波形未正确显示；随后导入能够完成，但顶部工具栏的 sample rate、buffer 显示不正确；最后剩余问题是 export 前后的波形形状不一致，看起来导入后的波形采样数量或时间密度变少。

用户提供的关键 CSV 文件包括：

- `Testing/Upstream-Compat-Demo-la-260722-102151.csv`
- `Testing/Upstream-Compat-Demo-la-260722-181641.csv`
- `Testing/Upstream-Compat-Demo-la-260722-182746.csv`
- `Testing/Upstream-Compat-Demo-la-260722-184410.csv`

最终用于确认的最新文件 `Upstream-Compat-Demo-la-260722-184410.csv` 内容特征：

- Header: `Sample rate: 1 MHz`
- Header: `Sample count: 1024 Samples`
- 数据行数：1024
- 时间列从 `0` 到 `0.001023`，步进为 1 us
- 8 个逻辑通道按 `00000000` / `11111111` 交替

这说明最后一轮问题已经不是 CSV 文件少写数据，而是导入后内部波形数据解释不正确。

## 调试方法

本次按 systematic debugging 思路处理：

1. 先确认每个边界的输入输出，不直接猜修复。
2. 对采集、session feed、export、import、viewport paint 增加或查看日志。
3. 每次只锁定一个假设，用最小测试验证。
4. 对最终 root cause 增加回归测试。

关键日志路径：

```text
~/Library/Application Support/DreamSourceLab/PXTOOL/PXTOOL.log
```

## 数据流证据

### 采集端

Upstream Compat Demo 修复后，driver 按配置的 sample limit 发出数据，而不是固定 128 samples：

```text
upstream-demo: Acquisition start: samplerate=1000000 limit_samples=1024 unitsize=1
SigSession::feed_in_logic ... len=1024 ... samplelimit=1024
```

### Export 端

StoreSession 导出时能看到 snapshot 和 ring 内都是 1024 samples：

```text
StoreSession::export_exec logic meta ... snapshot_samples=1024 ring_samples=1024
rows_to_send=1024
```

导出的 CSV 也有 1024 行数据：

```text
Sample rate: 1 MHz
Sample count: 1024 Samples
```

### Import 端

导入 DSView CSV 时检测到 header 中的 sample rate、sample count、channel count：

```text
DSView CSV detection ... samplelimit=1024
SigSession::feed_in_logic ... len=1024 ... samplelimit=1024
```

最后修复后，CSV input 会额外输出 cross-data 转换日志：

```text
CSV logic flush: rows=1024 channels=8 sample_unit=1 cross_bytes=1024.
```

### View 端

Viewport paint 日志确认导入后 UI 层拿到的是 8 个逻辑通道、1 MHz、1024 samples：

```text
Viewport::paintSignals logic summary: ... traces=8 enabled=8 logic=8 drawn=8 samplerate=1000000 samplelimit=1024 have_data=1
```

## Root Cause

最终 root cause 是 CSV import 输出的逻辑数据布局与 PXTOOL 波形存储期望的数据布局不一致。

具体来说：

- `libsigrok/input/csv.c` 原始 CSV input 按 sample-interleaved layout 输出逻辑数据。也就是每一行 CSV 样本被打包成一个 sample byte，例如 8 通道交替波形会形成 `0x00, 0xff, 0x00, 0xff...`。
- PXTOOL 的 `LogicSnapshot::append_cross_payload()` 期望的是 `LA_CROSS_DATA` 布局。该布局不是逐样本排列，而是按 64 samples 为一组、每个通道单独一段 bitstream。
- 当 sample-interleaved bytes 被当作 `LA_CROSS_DATA` 解释时，时间轴和通道数据会错位，视觉上就表现为波形脉冲变宽、密度不对，虽然 sample count 看起来已经正确。

因此最后的问题不是 export 少写样本，也不是 toolbar 显示问题，而是 import 数据进入 LogicSnapshot 前的二进制布局错误。

## 修复内容

### 1. Upstream Demo sample limit

修复 Upstream Compat Demo driver，使用配置的 `limit_samples` 作为实际采集长度，避免固定只输出 128 samples。

相关文件：

- `libsigrok/hardware/upstream-demo/upstream_demo.c`

### 2. CSV export sample count

导出 header 中的 sample count 使用精确整数样本数，避免 header 和实际数据行数不一致。

相关文件：

- `libsigrok/output/csv.c`
- `PXTOOL/pv/storesession.cpp`

### 3. DSView CSV import metadata

导入 DSView/PXTOOL 自己导出的 CSV 时，先解析 header：

- `Sample rate`
- `Sample count`
- `Time(s), D0, D1, ...` 中的逻辑通道数量

然后将这些值应用到 import options 和 session，使工具栏顶部 sample rate、buffer 能显示 CSV 对应值，并在导入数据场景下处于 disabled 状态。

相关文件：

- `PXTOOL/pv/data/inputimporter.h`
- `PXTOOL/pv/data/inputimporter.cpp`
- `PXTOOL/pv/mainwindow.cpp`
- `PXTOOL/pv/toolbars/filebar.cpp`

### 4. DSView CSV cross-data 转换

在 `libsigrok/input/csv.c` 增加内部 option：

```text
dsview_cross_data
```

当 PXTOOL 检测到 DSView CSV export 格式时，设置：

```cpp
options.set(QStringLiteral("dsview_cross_data"), QVariant(true));
```

CSV input 在 flush logic samples 时，将逐行 CSV 样本转换为 `LA_CROSS_DATA`：

```text
sample-interleaved:
  sample0, sample1, sample2, ...

LA_CROSS_DATA:
  block0 channel0 bits, block0 channel1 bits, ...
```

对 8 通道、1024 samples 的交替波形，转换后应得到连续的 `0xaa` bit pattern。这样 `LogicSnapshot::append_cross_payload()` 就能按 DSView 原有路径正确显示波形。

相关文件：

- `libsigrok/input/csv.c`
- `PXTOOL/pv/data/inputimporter.h`

## 新增/调整的日志

重点新增日志：

```text
CSV logic flush: rows=%zu channels=%zu sample_unit=%zu cross_bytes=%zu.
```

它用于确认：

- CSV 解析得到多少数据行
- 逻辑通道数量
- 单个 sample 的字节数
- 转换后的 `LA_CROSS_DATA` 字节数

对于 `Upstream-Compat-Demo-la-260722-184410.csv`，期望日志：

```text
CSV logic flush: rows=1024 channels=8 sample_unit=1 cross_bytes=1024.
```

## 回归测试

新增和调整了 CSV import 聚焦测试，覆盖以下行为：

- 普通 CSV input 仍按原路径工作。
- CSV input 会报告 total sample count。
- DSView CSV header 能解析 sample rate、sample count、channel count。
- DSView CSV import plan 会设置 `samplerate`、`logic_channels` 和 `dsview_cross_data`。
- 128 samples 的 DSView export fixture 导入后样本数和通道数正确。
- 1024 samples / 8 channels / 全通道交替波形导入后，样本数仍为 1024，转换后的 cross-data 前缀为 `0xaa`。

相关测试文件：

- `PXTOOL/test/test_input_fixtures.cpp`
- `PXTOOL/test/test_datafeed_stub.cpp`
- `PXTOOL/test/test_upstream_io_stubs.c`

最终验证命令：

```bash
cmake --build build.tests --target DSView -j4
build.macOS/DSView-test --run_test=csv_input_streams_logic_packets,csv_input_reports_total_sample_count,csv_import_plan_applies_to_csv_input_option_types,csv_import_streams_logic_as_cross_data,csv_import_detects_dsview_export_channel_count,csv_import_plan_streams_dsview_export_file,csv_import_streams_expected_dsview_export_waveform_bytes --log_level=test_suite
```

验证结果：

```text
Built target DSView
Running 7 test cases...
*** No errors detected
```

## 最终结论

这次问题分成两个阶段：

1. 早期确实存在样本数和 metadata 传播问题，包括 demo driver 固定输出 128 samples、CSV header/import metadata 没有完整保留。
2. 最后一轮 export/import 波形形状不一致的 root cause 是 CSV import 的 logic payload layout 错误。

最终修复是在 DSView CSV import 路径中保留 metadata，并将逐样本 CSV 数据转换为 PXTOOL 内部波形存储期望的 `LA_CROSS_DATA` 布局。用户重新 export/import 对比后，波形显示已经一致。
