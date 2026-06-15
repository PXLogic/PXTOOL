# PXTOOL MCP 功能测试指南

本文档用于测试 PXTOOL 的 MCP Server、Sidebar MCP 面板、MCP HTTP 接口和 Web Console。

## 1. 测试环境准备

在仓库根目录执行：

```powershell
cd D:\ap\DSView
git branch --show-current
git status --short
```

预期：

- 当前分支为 `add-mcp-server`
- `git status --short` 没有未提交的非预期修改

## 2. 构建 PXTOOL

当前 Windows 构建目录使用 MSYS2 CMake，执行：

```powershell
& 'C:/msys64/mingw64/bin/cmake.exe' --build build.windows --target DSView -j 2
```

预期：

- 命令退出码为 `0`
- 输出包含 `Built target DSView`
- 生成或更新 `build.windows\PXTOOL.exe`

## 3. 构建并部署 Web Console

如果本机安装了 `npm`，执行：

```powershell
& 'C:/msys64/mingw64/bin/cmake.exe' --build build.windows --target webui
& 'C:/msys64/mingw64/bin/cmake.exe' -DSRC='D:/ap/DSView/web/dist' -DDST='D:/ap/DSView/build.windows/webui' -P 'D:/ap/DSView/CMake/CopyOptionalDir.cmake'
Test-Path build.windows\webui\index.html
```

预期：

- `webui` 构建成功
- `Test-Path` 返回 `True`

说明：

- `npm audit` 可能报告依赖漏洞；这不影响 MCP 功能测试，但需要记录在测试报告中。

## 4. 启动 PXTOOL

先确认没有旧进程和端口占用：

```powershell
Get-Process PXTOOL -ErrorAction SilentlyContinue
Get-NetTCPConnection -LocalPort 10110 -ErrorAction SilentlyContinue
```

如果存在旧 PXTOOL 进程，可以先关闭应用窗口，或测试环境中执行：

```powershell
Get-Process PXTOOL -ErrorAction SilentlyContinue | Stop-Process -Force
```

启动：

```powershell
Start-Process -FilePath (Resolve-Path .\build.windows\PXTOOL.exe) -WindowStyle Hidden
Start-Sleep -Seconds 5
Get-Process PXTOOL -ErrorAction SilentlyContinue
```

预期：

- 能看到 `PXTOOL.exe` 进程
- 应用没有立即退出

## 5. Sidebar MCP 面板测试

打开 PXTOOL UI 后，在右侧 sidebar 测试：

1. 确认 tab 顺序为：

   `Trigger / Decode / Measure / Search / Options / Filter / MCP / Log`

2. 点击 `MCP` tab。

3. 预期面板标题为 `MCP Server`。

4. 预期 UI 风格与 `Device Options`、`Decode Protocol`、`Glitch Filter` 面板一致：

   - header 高度、字体、标题颜色一致
   - 内容使用 QGroupBox 分组
   - 字体随 PXTOOL 全局字体设置变化

5. 面板应包含：

   - `Web Console`
   - `Connect AI Tool`
   - server 状态和地址
   - Claude Code / Codex / OpenCode 连接命令
   - `Copy` 按钮
   - `Restart MCP Service` 按钮

6. 点击 `Copy`，粘贴到文本框中确认内容。

   Codex 命令预期类似：

   ```powershell
   codex mcp add --url http://127.0.0.1:10110 pxtool
   ```

7. 点击 `Restart MCP Service`，面板状态应保持或恢复为 `Running`。

## 6. Web Root 测试

执行：

```powershell
Invoke-WebRequest -Uri http://127.0.0.1:10110/ -UseBasicParsing
```

预期：

- HTTP status 为 `200`
- 返回内容是 Web Console HTML

如果未构建或未部署 `webui`，可能返回 `404`；这种情况下需要先执行第 3 节。

## 7. MCP initialize 测试

执行：

```powershell
$body = '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}'
Invoke-RestMethod -Uri http://127.0.0.1:10110/mcp -Method Post -Body $body -ContentType 'application/json' | ConvertTo-Json -Depth 10
```

预期：

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "protocolVersion": "2025-03-26",
    "serverInfo": {
      "name": "PXTOOL MCP Server"
    }
  }
}
```

## 8. MCP tools/list 测试

执行：

```powershell
$body = '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}'
$result = Invoke-RestMethod -Uri http://127.0.0.1:10110/mcp -Method Post -Body $body -ContentType 'application/json'
$result.result.tools.name
$result.result.tools.Count
```

预期工具数量为 `17`，名称为：

```text
get_devices
start_capture
stop_capture
wait_capture
load_capture
save_capture
close_capture
add_analyzer
remove_analyzer
list_analyzers
get_analyzer_options
export_raw_data_csv
export_raw_data_binary
export_data_table_csv
get_capture_status
get_channels
get_analyzer_results
```

## 9. get_devices 测试

执行：

```powershell
$body = '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"get_devices","arguments":{}}}'
Invoke-RestMethod -Uri http://127.0.0.1:10110/mcp -Method Post -Body $body -ContentType 'application/json' | ConvertTo-Json -Depth 20
```

预期：

- 返回 `result.content[0].type = "text"`
- `text` 内容是设备 JSON 字符串
- 至少包含 Demo Device，类似：

```json
{
  "display_name": "Demo Device",
  "is_demo": true,
  "is_virtual": true
}
```

## 10. get_capture_status 测试

执行：

```powershell
$body = '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"get_capture_status","arguments":{}}}'
Invoke-RestMethod -Uri http://127.0.0.1:10110/mcp -Method Post -Body $body -ContentType 'application/json' | ConvertTo-Json -Depth 20
```

预期：

- 返回 MCP content result
- JSON text 中包含 capture 状态字段，例如 `state`、`progress`、`triggered`

## 11. get_channels 测试

执行：

```powershell
$body = '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"get_channels","arguments":{}}}'
Invoke-RestMethod -Uri http://127.0.0.1:10110/mcp -Method Post -Body $body -ContentType 'application/json' | ConvertTo-Json -Depth 20
```

预期：

- 返回 MCP content result
- JSON text 中包含通道数组
- 通道对象应包含 `index`、`name`、`type`、`enabled`

## 12. Analyzer 基础测试

列出可用 analyzer：

```powershell
$body = '{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"list_analyzers","arguments":{}}}'
Invoke-RestMethod -Uri http://127.0.0.1:10110/mcp -Method Post -Body $body -ContentType 'application/json' | ConvertTo-Json -Depth 20
```

预期：

- 返回 analyzer/decoder 列表
- 常见 decoder 如 `i2c`、`spi`、`uart` 可能出现在列表中，取决于当前 decoder 环境

查询 analyzer 选项：

```powershell
$body = '{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"get_analyzer_options","arguments":{"analyzerName":"uart"}}}'
Invoke-RestMethod -Uri http://127.0.0.1:10110/mcp -Method Post -Body $body -ContentType 'application/json' | ConvertTo-Json -Depth 20
```

预期：

- 如果 `uart` decoder 存在，返回 channel/options 信息
- 如果不存在，返回明确错误，不应导致 PXTOOL 崩溃

## 13. Capture 冒烟测试

使用 Demo Device 时，可做最小 capture 测试。先从 `get_devices` 返回中取一个 `id`，替换下面的 `<DEVICE_ID>`：

```powershell
$body = '{"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"start_capture","arguments":{"deviceId":"<DEVICE_ID>","logicDeviceConfiguration":{"digitalChannels":[0,1]},"captureConfiguration":{"manualCaptureMode":{"sampleCount":1000}}}}}'
Invoke-RestMethod -Uri http://127.0.0.1:10110/mcp -Method Post -Body $body -ContentType 'application/json' | ConvertTo-Json -Depth 20
```

等待完成：

```powershell
$body = '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"wait_capture","arguments":{"timeoutSeconds":30}}}'
Invoke-RestMethod -Uri http://127.0.0.1:10110/mcp -Method Post -Body $body -ContentType 'application/json' | ConvertTo-Json -Depth 20
```

预期：

- `start_capture` 不崩溃
- `wait_capture` 成功或返回明确超时/状态错误
- 若返回错误，记录错误内容和当时 UI 状态

注意：

- 带 trigger preconfiguration 的复杂 capture 在 PXTOOL 当前适配中可能返回 `ConfigNotSupported`。

## 14. Web Console 功能测试

浏览器打开：

```text
http://127.0.0.1:10110/
```

预期：

- 页面标题/文本使用 PXTOOL，不出现 PXView
- 页面能加载，无空白页
- 浏览器开发者工具 Console 不应出现阻塞功能的 JS 错误

可在 Web Console 中配置模型/API 后发起自然语言请求，例如：

```text
List devices
```

预期：

- Web Console 调用 MCP tool
- 能显示设备列表或明确错误

## 15. 关闭测试进程

测试结束后执行：

```powershell
Get-Process PXTOOL -ErrorAction SilentlyContinue | Stop-Process -Force
```

确认端口释放：

```powershell
Get-NetTCPConnection -LocalPort 10110 -ErrorAction SilentlyContinue
```

预期：

- 没有监听 `10110` 的 PXTOOL 进程

## 16. 已知差异和注意事项

PXTOOL MCP 当前与 PXView MCP tool surface 对齐，暴露同样的 17 个 MCP 工具。

已知差异：

- PXTOOL 只绑定当前 active `SigSession`，没有引入 PXView 的 `SessionDocument`、`TabContext`、`SessionManager` 多文档架构。
- PXTOOL 缺失的 PXView-only 能力会返回 `ConfigNotSupported`，而不是强行迁移架构。
- 目前可能返回 `ConfigNotSupported` 的能力包括：
  - 部分 trigger preconfiguration
  - signal invert
  - 部分保存/导出路径
- `webui` 是可选构建；如果未构建，MCP HTTP 接口仍可工作，但 `/` Web Console 可能不可用。

## 17. 测试报告建议记录

每轮测试建议记录：

- Git commit：

  ```powershell
  git rev-parse --short HEAD
  ```

- 构建命令和结果
- `initialize` 返回的 `serverInfo`
- `tools/list` 的工具数量和名称
- `get_devices` 返回内容
- Sidebar MCP 面板截图
- Web Console 是否可打开
- 任何 `ConfigNotSupported` 的具体请求参数和返回内容
- PXTOOL 是否崩溃、卡死或端口未释放
