# PXLOGIC Stream 磁盘缓存深度截断修复记录

日期：2026-07-30

## 问题现象

PXLOGIC 处于 Stream 模式时，设备持续发送逻辑数据。启用磁盘缓存后运行十余秒，停止采集并将波形缩放到最小时，界面仍只显示约 5 ms 的几个 PWM 方波，而不是整个采集期间的数据。

测试配置：

- PX-Logic U3 channel 32，Stream 模式。
- 已启用 2 个逻辑通道。
- 采样率：50 MHz。
- Buffer：5 ms。
- Disk Cache：启用，RAM 热区 32 MB，磁盘上限 24 GB。

## 关键日志

```text
SamplingBar::update_view_status ... op_mode(valid=1,val=1) ... helper_stream=1
[SpillMgr] init channels=2 ... ram_limit=32MB disk_limit=24576MB
LogicSnapshot::first_payload begin len=131072 total=250880 spill=1
LogicSnapshot::append_cross_payload end samples=250880 ram=4260880 pending=0
...
[SpillMgr] Session end: spilled=0 blocks written=0MB ram_usage=2MB limit=32MB
```

其中 `total=250880` 与 `50 MHz * 约 5 ms` 对应。第一个 USB 数据包已超过该容量，快照被立即填满；后续包仍到达 `append_cross_payload()`，但在数据写入前因达到 `_total_sample_count` 而返回。

## 根因

`SigSession::capture_init()` 原本只对“未启用磁盘缓存”的 Stream 采集扩大快照容量：

```cpp
if (_is_stream_mode && !_disk_cache_settings.enabled)
    sample_limit = max(sample_limit, one_megabyte_window);
```

启用磁盘缓存时，`sample_limit` 仍使用 UI Buffer 的 5 ms 值，并传给 `LogicSnapshot::first_payload()` 作为 `_total_sample_count`。

`SpillManager` 的职责是将已分配的 leaf block 从 RAM 换出到磁盘；它不会扩展 `LogicSnapshot` 的总样本容量。因此数据在达到 32 MB RAM 换出阈值以前，已经被 5 ms 容量上限丢弃，最终显示 `spilled=0`。

## 修复

对具有有限磁盘上限的 Stream 采集，使用磁盘上限换算快照总样本数：

```text
sample_limit = disk_limit_bytes * 8 / enabled_logic_channel_count
```

`capture_init()` 将该容量与设备当前 sample limit 取较大值。对 24 GB、2 通道配置，快照容量为：

```text
24 * 1024^3 * 8 / 2 = 103079215104 samples
```

RAM 热区仍是独立的换出阈值，达到 32 MB 后由 `SpillManager` 异步写入临时 spill 文件。未启用磁盘缓存时，原有的 1 MiB 环形窗口逻辑保持不变。

涉及文件：

- `PXTOOL/pv/sigsession.cpp`
- `PXTOOL/pv/utility/diskcachesettings.h`
- `PXTOOL/test/test_diskcachesettings.cpp`

## 验证标准

修复后的相同配置应满足：

```text
LogicSnapshot::first_payload ... total=103079215104 spill=1
[SpillMgr] First spill write: ...
[SpillMgr] Session end: spilled=<non-zero> ...
```

停止采集后将波形缩放到最小时，时间轴应覆盖实际采集时长，而不是固定在约 5 ms。测试同时确认 `spilled`、`written`、`stalls`，并检查日志中没有 `Malloc_err`、`Data_overflow`、`Short read` 或 `Short write`。

## 限制

磁盘上限设为 `Unlimited` 时没有可预分配的有限快照容量；当前修复仅对配置了非零磁盘上限的磁盘缓存生效。支持真正不限深度的 stream 采集需要让 `LogicSnapshot` 的根节点索引按需扩展，属于独立的结构性改造。
