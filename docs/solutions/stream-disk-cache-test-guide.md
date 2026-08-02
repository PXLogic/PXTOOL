# Stream 磁盘缓存测试指南

本文档用于验证 Stream 模式下的磁盘缓存 / disk-spill 功能。

## Demo Device 是否可以用于测试

Demo Device 可以用于本次功能测试。

原因：

- Demo Device 虽然是虚拟设备，但仍然走正常的数据采集回调路径。
- 在 `SigSession::action_start_capture()` 中，Demo Device 和文件设备都会被当作逻辑 Stream 数据源处理：
  `else if (_device_agent.is_demo() || _device_agent.is_file()) _is_stream_mode = true;`
- 因此，在启用 Disk Cache 后，Demo Device 的 Logic 采集可以验证 `SpillManager` 创建、临时文件创建、block 写盘、读回和删除。

限制：

- Demo Device 不能验证真实 USB 吞吐、真实硬件背压、真实设备长时间稳定性。
- Demo Device 可以验证功能路径，但性能和压力测试后续仍建议用真实硬件补测。
- 当前 UI 的 `RAM hot window` 最小值是 `1 GB`，如果采集数据没有超过 1GB 热窗口，日志中 `spilled=0` 是正常的。

## Log Options 和日志文件路径

`Log Options` 弹窗入口：

```text
Help -> Log Options
```

代码位置：

```text
DSView/pv/toolbars/logobar.cpp
LogoBar::on_action_setting_log()
```

弹窗包含：

- `Log Level`
- `Save To File`
- `Append mode`
- `Open`
- `Clear`

日志文件路径由 `DSView/pv/log.cpp` 中的 `get_dsv_log_path()` 决定。

Windows 下路径为：

```text
GetUserDataDir() + "/DSView.log"
```

`GetUserDataDir()` 使用 `QStandardPaths::AppDataLocation`。因为 `main.cpp` 中设置了：

```text
OrganizationName = DreamSourceLab
ApplicationName  = PXTOOL
```

所以 Windows 下通常是：

```text
%APPDATA%\DreamSourceLab\PXTOOL\DSView.log
```

如果不确定实际路径，可以用 PowerShell 查找：

```powershell
Get-ChildItem "$env:APPDATA" -Recurse -Filter DSView.log |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 5 FullName,LastWriteTime
```

也可以从 UI 打开：

```text
Help -> Log Options -> Open
```

## 推荐启动方式

从命令行启动并开启文件日志：

```powershell
cd D:\ap\DSView\build.dir
.\PXTOOL.exe --storelog -l 4
```

实时查看磁盘缓存相关日志：

```powershell
Get-Content "$env:APPDATA\DreamSourceLab\PXTOOL\DSView.log" -Wait |
    Select-String "SpillMgr|Malloc_err|Data_overflow|Short read|Short write"
```

观察临时 spill 文件：

```powershell
Get-ChildItem $env:TEMP -Filter "dsview_spill_*" |
    Sort-Object LastWriteTime -Descending |
    Select-Object Name,Length,LastWriteTime
```

## 测试 1：关闭磁盘缓存，确认旧行为不变

目标：确认 Disk Cache 关闭时，新代码不会影响原有采集逻辑。

步骤：

1. 启动 `PXTOOL.exe --storelog -l 4`。
2. 选择 Demo Device。
3. 使用 Logic 模式。
4. 打开 `File -> Config... -> Disk Cache Settings...`。
5. 确认 `Enable disk cache` 未勾选。
6. 执行一次普通采集。
7. 停止采集。
8. 拖动、缩放、搜索，并保存 `.dsv` 文件。

预期：

- 不崩溃。
- 波形显示正常。
- 保存和重新打开正常。
- `%TEMP%` 下没有残留 `dsview_spill_*` 文件。
- 日志中不应出现 `Spill file created`。

检查命令：

```powershell
Select-String "$env:APPDATA\DreamSourceLab\PXTOOL\DSView.log" `
    -Pattern "SpillMgr|Malloc_err|Data_overflow"
```

## 测试 2：开启磁盘缓存，确认 SpillManager 创建

目标：确认 UI 设置生效，并且采集开始时会创建 `SpillManager`。

步骤：

1. 打开 `File -> Config... -> Disk Cache Settings...`。
2. 勾选 `Enable disk cache`。
3. 设置 `RAM hot window = 1 GB`。
4. 设置 `Disk total depth = 64 GB` 或更大。
5. `Cache directory` 留空使用默认路径，或设置为一个确认可写的目录。
6. 点击 OK。
7. 确认采样栏 Buffer 标签类似：

```text
RAM: 1 GB / Disk: 64 GB
```

8. 使用 Demo Device 的 Logic 模式开始采集。

预期日志：

```text
[SpillMgr] Spill file created:
[SpillMgr] init channels=...
```

通过标准：

- 出现 16 个 channel 对应的 spill 临时文件创建日志，说明 Demo Device 的 Logic 采集已经进入磁盘缓存路径。
- 如果采集量很小，`spilled=0` 仍然是正常结果，只说明还没有超过 RAM hot window。

检查命令：

```powershell
Select-String "$env:APPDATA\DreamSourceLab\PXTOOL\DSView.log" `
    -Pattern "\[SpillMgr\]"
```

## 测试 2 日志样例分析

下面这种日志说明测试 2 已通过：

```text
[SpillMgr] Spill file created: ... ch0.bin
...
[SpillMgr] Spill file created: ... ch15.bin
[SpillMgr] init channels=16 ... ram_limit=1024MB disk_limit=65536MB
[SpillMgr] Session end: spilled=0 blocks written=0MB ram_usage=32MB limit=1024MB
```

含义：

- `channels=16`：Demo Device 当前启用了 16 路 Logic 通道。
- `ram_limit=1024MB`：Disk Cache 设置中的 `RAM hot window = 1 GB` 已生效。
- `disk_limit=65536MB`：Disk Cache 设置中的 `Disk total depth = 64 GB` 已生效。
- `ram_usage=32MB`：本次采集只分配了约 32MB leaf block RAM。
- `spilled=0`：没有超过 1GB RAM hot window，所以没有真正写盘。
- `Deleted spill file`：采集结束后临时文件已被清理。

如果采样率是 `1 MHz`，采集深度约 `1,000,448 samples`，日志中 `spilled=0` 是符合预期的。

## 测试 3：用 Demo Device 触发真实落盘

目标：让采集数据超过 `RAM hot window`，确认 `spilled > 0`。

建议配置：

- Disk Cache：开启。
- RAM hot window：`1 GB`。
- Disk total depth：`64 GB` 或更大。
- Device：Demo Device。
- Mode：Logic。
- Channel mode：优先使用 16 channels。
- Sample rate：尽量调到较高值，例如 `125 MHz` 或 Demo 当前可选的最高值。
- Capture depth / duration：设置到足够大，确保超过 1GB 热窗口。

估算：

- 当前 16 通道 Demo 在 1 次 leaf block 分配中约占用 32MB RAM。
- 需要超过 1GB，至少要产生 33 组左右的 leaf block。
- 如果采样率只有 `1 MHz`，触发 spill 可能需要数分钟。
- 如果采样率提高到 `125 MHz`，触发 spill 会快很多。

步骤：

1. 保持 Disk Cache 开启。
2. 设置 `RAM hot window = 1 GB`。
3. 使用 Demo Device 的 Logic 模式。
4. 选择高采样率和大采集深度。
5. 开始采集并等待直到数据量超过 1GB 热窗口。
6. 采集中观察 `%TEMP%`：

```powershell
Get-ChildItem $env:TEMP -Filter "dsview_spill_*" |
    Sort-Object LastWriteTime -Descending |
    Select-Object Name,Length,LastWriteTime
```

7. 停止采集。

预期：

- 采集中 `dsview_spill_*_chN.bin` 文件大小开始增长。
- 结束日志中 `spilled` 大于 0。
- 采集结束后临时文件被删除。

通过标准：

- `spilled > 0`
- 没有 `Malloc_err`
- 没有 `Data_overflow`
- 没有 `Short read`
- 没有 `Short write`

检查命令：

```powershell
Select-String "$env:APPDATA\DreamSourceLab\PXTOOL\DSView.log" `
    -Pattern "Spill file created|Session end|Deleted spill file|Malloc_err|Data_overflow|Short read|Short write"
```

## 测试 4：读取已落盘的历史数据

目标：确认波形渲染、搜索、边沿跳转可以从磁盘读回已 spill 的 block。

步骤：

1. 完成测试 3，并确认 `spilled > 0`。
2. 不要关闭 PXTOOL。
3. 停止采集后，把波形拖到最早的数据区域。
4. 放大、缩小。
5. 执行搜索或边沿跳转。
6. 分别检查开头、中间、结尾区域。

预期：

- 历史波形正常显示。
- 搜索和跳转不崩溃。
- 日志中没有读回错误。

检查命令：

```powershell
Select-String "$env:APPDATA\DreamSourceLab\PXTOOL\DSView.log" `
    -Pattern "load_block|Short read|Malloc_err|Data_overflow|Session end"
```

## 测试 5：落盘后保存并重新打开

目标：确认 `.dsv` 保存路径可以读取已 spill 的 block。

步骤：

1. 完成测试 3，并确认 `spilled > 0`。
2. 停止采集。
3. 执行 `File -> Save...`。
4. 保存 `.dsv` 文件。
5. 重启 PXTOOL，或清除当前采集。
6. 打开刚保存的 `.dsv`。
7. 检查开头、中间、结尾波形。

预期：

- 保存成功。
- 重新打开成功。
- 数据完整。
- 不崩溃。

## 测试 6：启动时清理残留 spill 文件

目标：确认上次异常退出遗留的 `dsview_spill_*` 文件会在启动时被清理。

步骤：

1. 关闭 PXTOOL。
2. 手动创建一个假的残留文件：

```powershell
New-Item -Path "$env:TEMP\dsview_spill_fake_ch0.bin" -ItemType File -Force
```

3. 启动 PXTOOL：

```powershell
D:\ap\DSView\build.dir\PXTOOL.exe --storelog -l 4
```

预期：

- 假文件被删除。
- 日志包含：

```text
[SpillMgr] Cleaned stale file:
```

检查命令：

```powershell
Test-Path "$env:TEMP\dsview_spill_fake_ch0.bin"
Select-String "$env:APPDATA\DreamSourceLab\PXTOOL\DSView.log" `
    -Pattern "Cleaned stale file"
```

## 测试 7：对比关闭和开启磁盘缓存的日志

目标：形成 before / after 对比。

步骤：

1. 关闭 Disk Cache，执行测试 1。
2. 保存日志副本：

```powershell
Copy-Item "$env:APPDATA\DreamSourceLab\PXTOOL\DSView.log" `
    D:\ap\DSView\baseline-no-disk-cache.log -Force
```

3. 开启 Disk Cache，执行测试 3。
4. 保存日志副本：

```powershell
Copy-Item "$env:APPDATA\DreamSourceLab\PXTOOL\DSView.log" `
    D:\ap\DSView\with-disk-cache.log -Force
```

5. 对比关键日志：

```powershell
Select-String D:\ap\DSView\baseline-no-disk-cache.log `
    -Pattern "SpillMgr|Malloc_err|Data_overflow|Session end"

Select-String D:\ap\DSView\with-disk-cache.log `
    -Pattern "SpillMgr|Malloc_err|Data_overflow|Session end"
```

预期：

- 关闭 Disk Cache 的日志没有 spill 活动。
- 开启 Disk Cache 的日志包含 `Spill file created` 和 `Session end: spilled=...`。
- 开启 Disk Cache 的日志没有 `Malloc_err`、`Data_overflow`、`Short read`、`Short write`。

## 最终通过标准

功能测试通过需要满足：

- Disk Cache 关闭时，旧行为不变。
- Disk Cache 开启时，Demo Device 可以创建 `SpillManager`。
- 大采集时 `spilled > 0`。
- 采集结束后临时 spill 文件被删除。
- 启动时可以清理残留 `dsview_spill_*` 文件。
- 对已落盘历史区域进行浏览、缩放、搜索不崩溃。
- 落盘后保存并重新打开 `.dsv` 正常。
- 日志中没有 `Malloc_err`、`Data_overflow`、`Short read`、`Short write`。

## 实测记录：Demo Device 125MHz / 20s / 32MB 成功落盘

本节记录一次已经通过的端到端测试，用于后续回归对比。

测试时间：

- 2026-06-02

测试目标：

- 验证 Demo Device 在较大采集深度下可以触发真实 disk spill。
- 验证 `32 MB` RAM hot window 测试选项生效。
- 验证采集过程中没有磁盘写入 stall、短读、短写或内存错误。
- 明确区分临时 spill cache 文件和用户手动保存的 `.dsl` / `.dsv` 文件。

测试配置：

- Device：Demo Device。
- Work mode：Logic。
- Capture mode：Single。
- Demo pattern：建议使用 `random`，避免 `.demo` 文件 pattern 受文件长度限制提前结束。
- Sample Rate：`125 MHz`。
- Buffer：`20 s`。
- Disk Cache：开启。
- RAM hot window：`32 MB`。
- Disk total depth：`64 GB`。
- Cache directory：默认临时目录，即 `%TEMP%`。

操作步骤：

1. 启动 PXTOOL，并开启文件日志。
2. 打开 `File -> Config... -> Disk Cache Settings...`。
3. 勾选 `Enable disk cache`。
4. 设置 `RAM hot window = 32 MB`。
5. 设置 `Disk total depth = 64 GB`。
6. 选择 Demo Device。
7. 切换到 Logic。
8. 设置 Demo pattern 为 `random`。
9. 设置 `Sample Rate = 125 MHz`。
10. 设置 `Buffer = 20 s`。
11. 设置采集模式为 `Single`。
12. 点击 Start。
13. 不要手动 Stop，等待采集自动结束。
14. 采集结束后检查日志中的 `SpillMgr` 统计。

本次关键日志：

```text
16:02:18.878  commit_settings: duration=2e+10 sample_rate=125000000 limit_samples=2500000768
16:02:18.878  Start collect.
16:02:18.894  [SpillMgr] init channels=16 session=1780387338894 ram_limit=32MB disk_limit=65536MB dir=C:/Users/ZhaiYuanji/AppData/Local/Temp
16:02:38.905  ------------SR_DF_END packet.
16:02:38.915  [SpillMgr] Session end: spilled=2084 blocks written=4234MB ram_usage=633MB limit=32MB lru_hit=0 lru_miss=0 queue_peak=7 stalls=0
```

日志解读：

- `duration=2e+10`：UI 提交的采集时长为 20 秒，单位为 ns。
- `sample_rate=125000000`：采样率为 `125 MHz`。
- `limit_samples=2500000768`：20 秒采集对应约 25 亿 samples，并经过内部对齐。
- `ram_limit=32MB`：`RAM hot window = 32 MB` 设置已经生效。
- `disk_limit=65536MB`：`Disk total depth = 64 GB` 设置已经生效。
- `spilled=2084 blocks`：已经发生真实落盘。
- `written=4234MB`：本次临时 spill cache 写入约 `4.2 GB`。
- `queue_peak=7`：异步写队列峰值为 7。
- `stalls=0`：采集过程中没有因为磁盘写入而产生 stall。
- 没有出现 `Malloc_err`、`Data_overflow`、`Short read`、`Short write`。

采集时长验证：

```text
Start collect: 16:02:18.878
SR_DF_END:     16:02:38.905
```

实际采集约 20 秒，符合 `125 MHz / 20 s` 测试目标。

临时文件生命周期：

本次采集创建了 16 个通道对应的临时 spill 文件：

```text
C:/Users/ZhaiYuanji/AppData/Local/Temp/dsview_spill_1780387338894_ch0.bin
...
C:/Users/ZhaiYuanji/AppData/Local/Temp/dsview_spill_1780387338894_ch15.bin
```

采集结束后这些临时文件被自动删除：

```text
[SpillMgr] Deleted spill file: ... ch0.bin
...
[SpillMgr] Deleted spill file: ... ch15.bin
```

重要说明：

- `written=4234MB` 表示写入临时 disk cache 的数据量。
- 这些 `dsview_spill_*` 文件不是用户最终保存的数据文件。
- 它们只在采集和内部读回期间使用，采集/session 结束后会自动清理。
- 如果需要永久保存采集结果，需要通过 `File -> Save...` 另存为 `.dsl` / `.dsv` 文件。
- 保存后的文件才可以通过 `File -> Open...` 再次打开。

通过结论：

- Demo Device 可以用于验证 disk cache 功能路径。
- `Single + Demo Device + 125MHz + 20s + 32MB RAM hot window` 可以稳定触发真实落盘。
- 本次测试成功写入约 `4.2 GB` 临时 spill cache。
- 本次测试没有观察到 IO 错误、内存错误或采集 stall。
- 临时 spill 文件在采集结束后被正常清理。

后续建议：

- 对同样配置重复执行 3 次，确认 `spilled`、`written MB`、`stalls=0` 稳定。
- 在采集结束后执行 `File -> Save...`，验证大数据量保存和重新打开。
- 在真实硬件上补测相同路径，以覆盖真实 USB 吞吐和硬件背压场景。
