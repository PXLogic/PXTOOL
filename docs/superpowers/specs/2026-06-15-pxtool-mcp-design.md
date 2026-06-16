# PXTOOL MCP Design

## Goal

Add the existing PXView MCP capability to PXTOOL on the current `add-mcp-server` branch, exposed through a new right-sidebar `MCP` tab whose visual style matches PXTOOL's existing `Device Options` and `Decode Protocol` panels.

The feature should let MCP clients connect to PXTOOL at `http://127.0.0.1:10110`, list tools, control the active capture session, manage analyzers, export data, and open the optional web console served from the built web client.

## Source Reference

PXView contains the reference implementation in the sibling checkout:

- `../PXVIEW/PXView/pv/api/`
- `../PXVIEW/PXView/pv/dock/mcpcontroldock.{h,cpp}`
- `../PXVIEW/web/`
- `../PXVIEW/doc/MCP与Web客户端使用指南.md`
- MCP CMake/web install blocks in `../PXVIEW/CMakeLists.txt`

PXTOOL currently has no MCP or `pv/api` layer. PXTOOL already has the target UI shell in:

- `PXTOOL/pv/dock/sidebar.{h,cpp}`
- `PXTOOL/pv/dock/deviceoptionsdock.{h,cpp}`
- `PXTOOL/pv/dock/protocoldock.{h,cpp}`
- `PXTOOL/themes/dark.qss`
- `PXTOOL/themes/light.qss`
- `PXTOOL/PXTOOL.qrc`

## Scope

### In Scope

- Add a PXTOOL API layer adapted from PXView:
  - data types and `Result<T>`
  - JSON-RPC transport structures
  - MCP HTTP/Streamable HTTP transport
  - app/session service facade for the active PXTOOL session
  - MCP tool schema and dispatcher
- Start the MCP server automatically from `AppControl::Start()` and stop it in `AppControl::Stop()`.
- Add `AppControl::GetAppService()` and `AppControl::get_mcp_transport()` for UI and service integration.
- Add a new `McpControlDock` widget under `PXTOOL/pv/dock/`.
- Add a new `MCP` tab to PXTOOL's existing right sidebar.
- Use `pxtool` as the MCP client/server registration name in commands and server metadata.
- Serve static web UI files from `<app dir>/webui/` when they exist.
- Add optional CMake targets to build/copy the web UI, following PXView's `webui` and `install-webui` pattern.
- Copy/adapt PXView web client sources under a PXTOOL-local `web/` directory and update visible/product strings from PXView to PXTOOL where relevant.
- Verify with CMake/build and direct MCP requests.

### Out Of Scope

- Do not migrate PXView's `SlidingDrawer`, `widgets::SideBar`, `SessionDocument`, `TabContext`, or PXView main-window architecture.
- Do not replace PXTOOL's existing `dock::SideBar`.
- Do not redesign PXTOOL's existing panels.
- Do not change the default MCP port unless a later product requirement asks for it.
- Do not implement cloud LLM integration inside the desktop app; the web client may keep its own configurable API settings.

## User Experience

PXTOOL's right sidebar gains an `MCP` tab. The tab appears between `Filter` and `Log`:

`Trigger / Decode / Measure / Search / Options / Filter / MCP / Log`

Clicking the tab expands the same sidebar panel used by the other tabs. The panel header follows the current `SideBar` pattern:

- wrapper widget with zero margins
- header bar with `12, 8, 12, 8` margins
- title label styled and font-scaled by `SideBar::applyTitleStyle()` and `SideBar::UpdateFont()`
- title text: `MCP Server`

The panel body is a scrollable/control widget styled like `DeviceOptionsDock`:

- top-level object name: `mcpControlWidget`
- content margins and spacing consistent with `DeviceOptionsDock`
- `QGroupBox` sections for:
  - `Web Console`
  - `Connect AI Tool`
- normal labels use the current app font size from `AppConfig::Instance().appOptions.fontSize`
- buttons reuse existing QPushButton theme styling; no custom card-like visual language
- command snippets are selectable and have a `Copy` button

Displayed connection commands use `pxtool`:

```bash
claude mcp add --transport http pxtool http://127.0.0.1:10110
codex mcp add --url http://127.0.0.1:10110 pxtool
opencode --mcp http://127.0.0.1:10110
```

The panel shows status:

- Running: `127.0.0.1:10110`
- Stopped: `-`

Buttons:

- `Open MCP Web Console`: opens `http://127.0.0.1:10110/`
- `Restart MCP Service`: calls `McpTransport::stop()` then `start()` and refreshes status
- `Copy`: copies the command in that row

## MCP Behavior

PXTOOL implements the same MCP protocol surface as PXView:

- JSON-RPC 2.0 over HTTP
- MCP protocol version `2025-03-26`
- Streamable HTTP transport
- static GET file serving for the web UI
- SSE progress/result stream for `wait_capture`

The server metadata should identify PXTOOL, not PXView.

The tool list targets the documented PXView MCP tool set:

- `get_devices`
- `get_channels`
- `start_capture`
- `stop_capture`
- `wait_capture`
- `get_capture_status`
- `list_analyzers`
- `get_analyzer_options`
- `add_analyzer`
- `remove_analyzer`
- `get_analyzer_results`
- `export_raw_data_csv`
- `export_raw_data_binary`
- `export_data_table_csv`
- `load_capture`
- `save_capture`
- `close_capture`

If the reference implementation exposes more schemas than the documented list above, PXTOOL should keep the PXView-compatible MCP surface needed by the web client and existing tool flows, but the acceptance path focuses on the documented tools above.

## Architecture

### Backend

Create `PXTOOL/pv/api/` with the following files, adapted from PXView:

- `types.h`: shared enums, service structs, `Result<T>`.
- `transport.h`: JSON-RPC request/response and transport interfaces.
- `mcp_transport.{h,cpp}`: local HTTP server on `127.0.0.1:10110`, CORS, JSON-RPC parsing, MCP response wrapping, static file serving, SSE `wait_capture`.
- `iapp_service.h`: app-level service interface.
- `isession_service.h`: session-level service interface.
- `app_service.{h,cpp}`: wraps PXTOOL's active `SigSession`.
- `session_service.{h,cpp}`: adapts PXTOOL `SigSession`, `DeviceAgent`, decoder, waveform, measurement, cursor, glitch-filter, and file/export operations to the service interface.
- `rpc_dispatcher.{h,cpp}`: MCP initialization, tool schemas, and tool calls.

The API layer must depend on PXTOOL's existing classes rather than adding PXView-only session/document concepts. In particular:

- Treat PXTOOL's current active `SigSession` as the active MCP session.
- `create_session()` returns the registered current session unless file loading or a device switch is explicitly needed.
- Avoid calling UI-heavy device/session transitions from MCP unless the corresponding PXTOOL method already supports it safely.
- Bind a `view::View*` to `SessionService` when available so cursor and zoom operations can act on the visible session.

### App Lifecycle

`AppControl` owns:

- `pv::api::AppService *_app_service`
- `pv::api::RpcDispatcher *_rpc_dispatcher`
- `pv::api::McpTransport *_mcp_transport`

`AppControl::Start()`:

1. Opens the existing `SigSession`.
2. Creates and initializes `AppService`.
3. Creates `RpcDispatcher`.
4. Creates `McpTransport` on port `10110`.
5. Starts the transport.

`AppControl::Stop()`:

1. Stops and deletes `McpTransport`.
2. Deletes `RpcDispatcher`.
3. Shuts down and deletes `AppService`.
4. Closes the existing `SigSession`.

### Sidebar Integration

Modify `PXTOOL/pv/dock/sidebar.h`:

- forward declare `McpControlDock`
- insert `TabMcp` before `TabLogs`
- increment `TabCount`
- add `McpControlDock *mcp_widget()`
- add `_mcp_widget`
- add `_title_mcp`

Modify `PXTOOL/pv/dock/sidebar.cpp`:

- include `mcpcontroldock.h`
- build an MCP wrapper with a header matching the existing tab wrappers
- add it to `_stack`
- add icon path and label entries
- update retranslation, title font list, title style list
- call `_mcp_widget->refresh_status()` when opening the tab

Use a sidebar icon resource. Prefer reusing a simple existing icon if one is visually acceptable; otherwise add a new `PXTOOL/icons/sidebar/workflow.svg` and register it in `PXTOOL/PXTOOL.qrc`.

### Styling

Add only narrowly scoped QSS for MCP-specific pieces:

- `QWidget#mcpControlWidget QGroupBox`
- `QWidget#mcpControlWidget QGroupBox::title`
- `QWidget#mcpControlWidget QLabel`
- `QFrame#mcpCmdFrame`

Reuse the existing colors and spacing of `deviceOptionsWidget` where practical. Do not create a separate visual language.

### Build

Update root `CMakeLists.txt`:

- add `PXTOOL/pv/api/*.cpp` to `DSView_SOURCES`
- add API headers and `PXTOOL/pv/dock/mcpcontroldock.h` to `DSView_HEADERS`
- add `PXTOOL/pv/dock/mcpcontroldock.cpp` to `DSView_SOURCES`
- find and link `nlohmann_json::nlohmann_json`
- ensure Qt network/socket dependencies required by `QTcpServer`/`QTcpSocket` are found and linked for Qt5 and Qt6 builds
- add optional `webui` and `install-webui` targets if `npm` is available
- install/copy `web/dist` into `bin/webui` when present

PXTOOL currently supports Qt5 or Qt6, so the CMake changes must preserve both paths.

### Web Client

Copy PXView `web/` to PXTOOL `web/`.

Update product-specific text:

- package name: `pxtool-mcp-web`
- browser title: `PXTOOL MCP`
- local storage keys from `pxview-*` to `pxtool-*`
- system prompt and visible strings from PXView to PXTOOL

The web client should still default to the MCP server at `http://127.0.0.1:10110`.

## Error Handling

- If port `10110` is unavailable, `McpTransport::start()` returns false and the MCP panel shows `Stopped`.
- Restart should tolerate a stopped transport.
- Static web requests must reject paths containing `..`.
- Missing `webui` files should return `404`, while the desktop panel still remains usable for external MCP clients.
- MCP notifications without `id` should return HTTP 204.
- Tool errors should be returned as MCP content with `isError: true`, matching PXView.

## Testing

Minimum verification:

1. CMake configure succeeds for the current build setup.
2. The PXTOOL target compiles at least through the modified sources.
3. Run PXTOOL and verify the MCP tab appears in the sidebar.
4. Open the MCP tab and verify:
   - title and fonts match existing panel headers
   - status is shown
   - commands use `pxtool`
   - restart refreshes status
5. Use direct HTTP checks:

```bash
curl -X POST http://127.0.0.1:10110/mcp -H "Content-Type: application/json" -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-03-26\",\"capabilities\":{},\"clientInfo\":{\"name\":\"test\",\"version\":\"1.0\"}}}"
curl -X POST http://127.0.0.1:10110/mcp -H "Content-Type: application/json" -d "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\",\"params\":{}}"
curl -X POST http://127.0.0.1:10110/mcp -H "Content-Type: application/json" -d "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"get_devices\",\"arguments\":{}}}"
```

6. If web UI is built, visit `http://127.0.0.1:10110/` and verify it loads.

## Acceptance Criteria

- PXTOOL has a right-sidebar `MCP` tab that expands a functioning MCP control panel.
- The MCP panel visually matches PXTOOL's existing panel style.
- MCP service starts with the application and listens on `127.0.0.1:10110`.
- MCP `initialize`, `tools/list`, and `get_devices` work.
- Tool/server metadata and UI commands use `pxtool`.
- CMake includes the new backend, UI, resources, and optional web client targets.
- No PXView-only main-window, SlidingDrawer, or SessionDocument architecture is introduced into PXTOOL.
