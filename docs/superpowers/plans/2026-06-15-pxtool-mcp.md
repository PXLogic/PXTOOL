# PXTOOL MCP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add PXView-equivalent MCP support to PXTOOL, exposed through a PXTOOL-styled `MCP` sidebar tab.

**Architecture:** Reuse PXView's MCP protocol and tool surface, but adapt the service layer to PXTOOL's existing `AppControl`, `SigSession`, `DeviceAgent`, `dock::SideBar`, and current session/view model. Avoid PXView's `SlidingDrawer`, `widgets::SideBar`, `SessionDocument`, and `TabContext`.

**Tech Stack:** C++17, Qt5/Qt6 Widgets + Network, nlohmann/json, CMake, existing PXTOOL/PXView Qt patterns.

---

## File Structure

- Create `PXTOOL/pv/api/types.h`: API enums, structs, `Result<T>`.
- Create `PXTOOL/pv/api/transport.h`: JSON-RPC request/response interfaces.
- Create `PXTOOL/pv/api/mcp_transport.h` and `.cpp`: HTTP/MCP server and static webui serving.
- Create `PXTOOL/pv/api/iapp_service.h` and `isession_service.h`: service interfaces.
- Create `PXTOOL/pv/api/app_service.h` and `.cpp`: active PXTOOL session facade.
- Create `PXTOOL/pv/api/session_service.h` and `.cpp`: adapter from MCP tools to PXTOOL session/device/decoder operations.
- Create `PXTOOL/pv/api/rpc_dispatcher.h` and `.cpp`: MCP `initialize`, `tools/list`, `tools/call`, and tool handlers.
- Create `PXTOOL/pv/dock/mcpcontroldock.h` and `.cpp`: right-sidebar MCP control panel.
- Modify `PXTOOL/pv/appcontrol.h` and `.cpp`: own and start/stop MCP service.
- Modify `PXTOOL/pv/mainwindow.cpp`: bind current `view::View` into `AppService` after sidebar/view setup.
- Modify `PXTOOL/pv/dock/sidebar.h` and `.cpp`: add `MCP` tab and panel wrapper.
- Modify `PXTOOL/themes/dark.qss` and `PXTOOL/themes/light.qss`: scoped MCP panel styles.
- Modify `PXTOOL/PXTOOL.qrc`: register MCP sidebar icon.
- Create `PXTOOL/icons/sidebar/workflow.svg`: sidebar icon.
- Copy/adapt `web/` from `../PXVIEW/web/`: PXTOOL web client.
- Modify `CMakeLists.txt`: add new sources, headers, Qt Network, nlohmann/json, optional webui targets, and raise the C++ standard from C++14 to C++17 so copied PXView API code using nested namespaces compiles without a broad syntax rewrite.

## Task 1: Copy API Contracts and MCP Transport

**Files:**
- Create: `PXTOOL/pv/api/types.h`
- Create: `PXTOOL/pv/api/transport.h`
- Create: `PXTOOL/pv/api/mcp_transport.h`
- Create: `PXTOOL/pv/api/mcp_transport.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Copy reference API contract files**

Copy these files from PXView:

```powershell
New-Item -ItemType Directory -Force PXTOOL\pv\api
Copy-Item ..\PXVIEW\PXView\pv\api\types.h PXTOOL\pv\api\types.h
Copy-Item ..\PXVIEW\PXView\pv\api\transport.h PXTOOL\pv\api\transport.h
Copy-Item ..\PXVIEW\PXView\pv\api\mcp_transport.h PXTOOL\pv\api\mcp_transport.h
Copy-Item ..\PXVIEW\PXView\pv\api\mcp_transport.cpp PXTOOL\pv\api\mcp_transport.cpp
```

- [ ] **Step 2: Make transport source ASCII-clean where edited**

In `PXTOOL/pv/api/mcp_transport.cpp`, replace any mojibake comments copied from PXView with ASCII comments only. Do not change behavior.

- [ ] **Step 3: Add build dependencies**

In `CMakeLists.txt`, add `nlohmann_json` lookup after Qt setup:

```cmake
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()
```

Add Qt Network for both Qt5 and Qt6 paths:

```cmake
find_package(Qt5Network REQUIRED)
set(QT_LIBRARIES ${QT_LIBRARIES} Qt5::Network)
```

```cmake
find_package(Qt6Network REQUIRED)
set(QT_LIBRARIES ${QT_LIBRARIES} Qt6::Network)
```

Add API files to `DSView_SOURCES` and `DSView_HEADERS`:

```cmake
PXTOOL/pv/api/mcp_transport.cpp
```

```cmake
PXTOOL/pv/api/types.h
PXTOOL/pv/api/transport.h
PXTOOL/pv/api/mcp_transport.h
```

Link JSON:

```cmake
target_link_libraries(${PROJECT_NAME} ${DSVIEW_LINK_LIBS} nlohmann_json::nlohmann_json)
```

Raise the C++ standard line:

```cmake
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
```

- [ ] **Step 4: Verify configure reaches dependency resolution**

Run:

```powershell
cmake -S . -B build.mcp-plan -G Ninja -DQT_VERSION_FORCE=6
```

Expected: configure either succeeds or fails only for missing local third-party dependencies already required by the project. It must not fail because Qt Network or nlohmann/json is missing from CMake logic.

## Task 2: App and Session Service Layer

**Files:**
- Create: `PXTOOL/pv/api/iapp_service.h`
- Create: `PXTOOL/pv/api/isession_service.h`
- Create: `PXTOOL/pv/api/app_service.h`
- Create: `PXTOOL/pv/api/app_service.cpp`
- Create: `PXTOOL/pv/api/session_service.h`
- Create: `PXTOOL/pv/api/session_service.cpp`
- Modify: `PXTOOL/pv/appcontrol.h`
- Modify: `PXTOOL/pv/appcontrol.cpp`
- Modify: `PXTOOL/pv/mainwindow.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Copy service interfaces**

Copy:

```powershell
Copy-Item ..\PXVIEW\PXView\pv\api\iapp_service.h PXTOOL\pv\api\iapp_service.h
Copy-Item ..\PXVIEW\PXView\pv\api\isession_service.h PXTOOL\pv\api\isession_service.h
Copy-Item ..\PXVIEW\PXView\pv\api\app_service.h PXTOOL\pv\api\app_service.h
Copy-Item ..\PXVIEW\PXView\pv\api\app_service.cpp PXTOOL\pv\api\app_service.cpp
Copy-Item ..\PXVIEW\PXView\pv\api\session_service.h PXTOOL\pv\api\session_service.h
Copy-Item ..\PXVIEW\PXView\pv\api\session_service.cpp PXTOOL\pv\api\session_service.cpp
```

- [ ] **Step 2: Adapt logging and includes**

In copied API `.cpp` files:

- replace `pxv_info`, `pxv_warn`, `pxv_err` with `dsv_info`, `dsv_warn`, `dsv_err`
- remove includes for PXView-only files that do not exist in PXTOOL
- keep includes for existing PXTOOL equivalents: `sigsession.h`, `deviceagent.h`, `storesession.h`, `view/view.h`, decoder model/stack files

- [ ] **Step 3: Remove PXView-only session/document behavior**

In `app_service.cpp`:

- `initialize()` registers `AppControl::GetSession()` as session id `0`.
- `create_session()` returns the existing active session id unless `file_path` is non-empty, in which case it calls `SigSession::set_file(QString::fromStdString(file_path))` and returns the active id.
- `set_new_tab_callback()` may remain as a no-op setter for compatibility.

In `session_service.cpp`:

- remove use of `SessionDocument`, `TabContext`, and `SessionManager`
- keep operations bound to `_session`, `_device`, and optional `_view`
- where PXTOOL lacks a PXView-only helper, return `Result<void>::Fail(ErrorCode::ConfigNotSupported, "...")` rather than adding new architecture

- [ ] **Step 4: Wire AppControl ownership**

In `PXTOOL/pv/appcontrol.h`, forward declare API classes and add:

```cpp
namespace pv { namespace api {
class IAppService;
class AppService;
class RpcDispatcher;
class McpTransport;
} }
```

Add public methods:

```cpp
pv::api::IAppService* GetAppService();
pv::api::McpTransport* get_mcp_transport() { return _mcp_transport; }
```

Add private members:

```cpp
pv::api::AppService* _app_service = nullptr;
pv::api::RpcDispatcher* _rpc_dispatcher = nullptr;
pv::api::McpTransport* _mcp_transport = nullptr;
```

In `appcontrol.cpp`, include:

```cpp
#include "api/iapp_service.h"
#include "api/app_service.h"
#include "api/rpc_dispatcher.h"
#include "api/mcp_transport.h"
```

Update `Start()`:

```cpp
_session->Open();
_app_service = new pv::api::AppService(this);
_app_service->initialize();
_rpc_dispatcher = new pv::api::RpcDispatcher(_app_service);
_mcp_transport = new pv::api::McpTransport(_rpc_dispatcher, 10110);
_mcp_transport->start();
return true;
```

Update `Stop()`:

```cpp
if (_mcp_transport) { _mcp_transport->stop(); delete _mcp_transport; _mcp_transport = nullptr; }
if (_rpc_dispatcher) { delete _rpc_dispatcher; _rpc_dispatcher = nullptr; }
if (_app_service) { _app_service->shutdown(); delete _app_service; _app_service = nullptr; }
_session->Close();
```

Add `GetAppService()` implementation:

```cpp
pv::api::IAppService* AppControl::GetAppService()
{
    return _app_service;
}
```

- [ ] **Step 5: Bind the visible view**

In `PXTOOL/pv/mainwindow.cpp`, include `api/app_service.h`. After `_sidebar_widget` and `_view` are created, add:

```cpp
auto *app_svc = AppControl::Instance()->GetAppService();
if (auto *concrete = dynamic_cast<pv::api::AppService*>(app_svc)) {
    concrete->set_active_view(_view);
}
```

Add `set_active_view(view::View*)` to `AppService`, forwarding it to the active `SessionService`.

- [ ] **Step 6: Add service files to CMake**

Add:

```cmake
PXTOOL/pv/api/app_service.cpp
PXTOOL/pv/api/session_service.cpp
```

and headers:

```cmake
PXTOOL/pv/api/iapp_service.h
PXTOOL/pv/api/isession_service.h
PXTOOL/pv/api/app_service.h
PXTOOL/pv/api/session_service.h
```

- [ ] **Step 7: Verify compile errors are only service adaptation errors, then fix**

Run:

```powershell
cmake --build build.windows --target DSView -j 2
```

Expected: first run may expose missing PXTOOL adaptation symbols. Fix all compile errors in the new API service files without changing unrelated UI behavior.

## Task 3: MCP Dispatcher and Tool Surface

**Files:**
- Create: `PXTOOL/pv/api/rpc_dispatcher.h`
- Create: `PXTOOL/pv/api/rpc_dispatcher.cpp`
- Modify: `CMakeLists.txt`
- Modify: `PXTOOL/pv/api/session_service.cpp`

- [ ] **Step 1: Copy dispatcher**

Copy:

```powershell
Copy-Item ..\PXVIEW\PXView\pv\api\rpc_dispatcher.h PXTOOL\pv\api\rpc_dispatcher.h
Copy-Item ..\PXVIEW\PXView\pv\api\rpc_dispatcher.cpp PXTOOL\pv\api\rpc_dispatcher.cpp
```

- [ ] **Step 2: Rename product metadata**

In `rpc_dispatcher.cpp`:

- server name: `PXTOOL MCP Server`
- server/client product strings: `pxtool`
- file extension descriptions should mention PXTOOL where user-visible

Keep tool names unchanged.

- [ ] **Step 3: Keep the 17 PXView MCP tools**

Ensure `get_tool_schemas()` exposes:

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

Ensure `dispatch_mcp_tool()` routes the same 17 names to handlers.

- [ ] **Step 4: Add dispatcher to CMake**

Add:

```cmake
PXTOOL/pv/api/rpc_dispatcher.cpp
PXTOOL/pv/api/rpc_dispatcher.h
```

- [ ] **Step 5: Verify protocol-level behavior compiles**

Run:

```powershell
cmake --build build.windows --target DSView -j 2
```

Expected: target compiles past `rpc_dispatcher.cpp`; any failures are concrete service method mismatches and must be fixed in `session_service.*` or `app_service.*`.

## Task 4: MCP Sidebar Panel

**Files:**
- Create: `PXTOOL/pv/dock/mcpcontroldock.h`
- Create: `PXTOOL/pv/dock/mcpcontroldock.cpp`
- Modify: `PXTOOL/pv/dock/sidebar.h`
- Modify: `PXTOOL/pv/dock/sidebar.cpp`
- Modify: `PXTOOL/themes/dark.qss`
- Modify: `PXTOOL/themes/light.qss`
- Modify: `PXTOOL/PXTOOL.qrc`
- Create: `PXTOOL/icons/sidebar/workflow.svg`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create McpControlDock**

Use PXView `mcpcontroldock` as behavior reference, but implement PXTOOL styling directly:

- inherit `QScrollArea` and `IUiWindow`
- inner widget object name `mcpControlWidget`
- use `QGroupBox` sections: `Web Console`, `Connect AI Tool`
- commands use `pxtool`
- fonts use `AppConfig::Instance().appOptions.fontSize`
- copy button uses clipboard
- restart calls `AppControl::Instance()->get_mcp_transport()`

- [ ] **Step 2: Add sidebar tab enum and members**

In `sidebar.h`, update enum:

```cpp
TabFilter   = 5,
TabMcp      = 6,
TabLogs     = 7,
TabCount    = 8
```

Add:

```cpp
class McpControlDock;
McpControlDock *mcp_widget() { return _mcp_widget; }
McpControlDock *_mcp_widget;
QLabel *_title_mcp = nullptr;
```

- [ ] **Step 3: Add sidebar wrapper and stack page**

In `sidebar.cpp`, include `mcpcontroldock.h`. Create `mcp_wrap` with header bar and `_title_mcp = new QLabel(tr("MCP Server"), ...)`, then `_mcp_widget = new McpControlDock(mcp_wrap)`.

Add page order:

```cpp
_stack->addWidget(filter_wrap);
_stack->addWidget(mcp_wrap);
_stack->addWidget(log_wrap);
```

- [ ] **Step 4: Add icon and label arrays**

Add `:/icons/sidebar/workflow.svg` and `QT_TR_NOOP("MCP")` in both label arrays.

When opening `TabMcp`, call:

```cpp
_mcp_widget->refresh_status();
```

Update `retranslateUi()`, `UpdateFont()`, and `applyTitleStyle()` to include `_title_mcp`.

- [ ] **Step 5: Add scoped QSS**

In both dark and light themes, add QSS matching `deviceOptionsWidget` group style for:

```css
QWidget#mcpControlWidget QGroupBox
QWidget#mcpControlWidget QGroupBox::title
QWidget#mcpControlWidget QLabel
QFrame#mcpCmdFrame
```

- [ ] **Step 6: Add icon resource**

Create a simple 16x16 workflow icon SVG using currentColor-compatible stroke, then add it to `PXTOOL/PXTOOL.qrc`.

- [ ] **Step 7: Add dock files to CMake and build**

Add:

```cmake
PXTOOL/pv/dock/mcpcontroldock.cpp
PXTOOL/pv/dock/mcpcontroldock.h
```

Run:

```powershell
cmake --build build.windows --target DSView -j 2
```

Expected: sidebar and MCP dock compile.

## Task 5: Web Client and Runtime Packaging

**Files:**
- Create/modify: `web/**`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Copy web client**

Run:

```powershell
Copy-Item -Recurse ..\PXVIEW\web .\web
```

- [ ] **Step 2: Rename product strings**

Replace product keys and text:

- `pxview-mcp-web` -> `pxtool-mcp-web`
- `PXView MCP` -> `PXTOOL MCP`
- `PXView` -> `PXTOOL` in web UI text and system prompt
- `pxview-mcp-sessions` -> `pxtool-mcp-sessions`
- `pxview-mcp-current-session` -> `pxtool-mcp-current-session`
- `pxview-mcp-settings` -> `pxtool-mcp-settings`

- [ ] **Step 3: Add optional CMake web targets**

Add:

```cmake
find_program(NPM_EXECUTABLE npm)
if(NPM_EXECUTABLE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/index.html
        COMMAND ${NPM_EXECUTABLE} install
        COMMAND ${NPM_EXECUTABLE} run build
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/web
        COMMENT "Building Vite MCP web client..."
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/package.json
    )
    add_custom_target(webui DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/index.html)
endif()
```

Add install/runtime copy logic so built `web/dist` is copied to `webui` beside the executable when present.

- [ ] **Step 4: Verify web build if npm exists**

Run:

```powershell
if (Get-Command npm -ErrorAction SilentlyContinue) { cmake --build build.windows --target webui }
```

Expected: if npm exists, `web/dist/index.html` is produced.

## Task 6: End-to-End Verification and Fixes

**Files:**
- Modify only files touched by Tasks 1-5 when fixing verification failures.

- [ ] **Step 1: Clean build**

Run:

```powershell
cmake --build build.windows --target DSView -j 2
```

Expected: build succeeds. If `PXTOOL.exe` is locking outputs, close the running app and rerun.

- [ ] **Step 2: Launch PXTOOL**

Run:

```powershell
Start-Process -FilePath .\build.windows\PXTOOL.exe -WindowStyle Hidden
Start-Sleep -Seconds 3
```

Expected: process starts and MCP port opens.

- [ ] **Step 3: Verify MCP initialize**

Run:

```powershell
$body = '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}'
Invoke-RestMethod -Uri http://127.0.0.1:10110/mcp -Method Post -Body $body -ContentType 'application/json'
```

Expected: response contains `protocolVersion` and server info naming PXTOOL.

- [ ] **Step 4: Verify tools/list**

Run:

```powershell
$body = '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}'
Invoke-RestMethod -Uri http://127.0.0.1:10110/mcp -Method Post -Body $body -ContentType 'application/json'
```

Expected: response contains all 17 MCP tools listed in Task 3.

- [ ] **Step 5: Verify get_devices**

Run:

```powershell
$body = '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"get_devices","arguments":{}}}'
Invoke-RestMethod -Uri http://127.0.0.1:10110/mcp -Method Post -Body $body -ContentType 'application/json'
```

Expected: response is a successful MCP content result with device JSON text.

- [ ] **Step 6: Verify web root**

Run:

```powershell
Invoke-WebRequest -Uri http://127.0.0.1:10110/ -UseBasicParsing
```

Expected: if `webui` is staged, HTTP 200 with HTML. If not staged, HTTP 404 is acceptable and must be noted.

- [ ] **Step 7: Final diff review**

Run:

```powershell
git status --short
git diff --check
git diff --stat
```

Expected: no whitespace errors; changes are limited to MCP/API/UI/web/CMake files and docs/plans.

## Self-Review

- Spec coverage: Tasks 1-6 cover backend transport, service layer, dispatcher tools, UI sidebar, styling/resources, web client, CMake, and verification.
- Placeholder scan: no placeholder markers remain.
- Type consistency: `McpTransport`, `RpcDispatcher`, `AppService`, `SessionService`, and `McpControlDock` names match the spec and planned CMake entries.
