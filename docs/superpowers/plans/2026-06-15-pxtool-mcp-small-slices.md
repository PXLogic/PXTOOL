# PXTOOL MCP Small Slices Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish PXTOOL MCP support in small, compile-verifiable slices.

**Architecture:** Keep PXView MCP transport and tool surface, but adapt the service layer to PXTOOL's single active `SigSession`. Add dispatcher, sidebar UI, and optional web client only after the service layer compiles.

**Tech Stack:** C++17, Qt Widgets/Network, nlohmann/json, CMake, PXTOOL sidebar dock patterns.

---

## Task A: Service Layer Compile Slice

**Files:**
- Modify: `PXTOOL/pv/api/session_service.*`
- Modify: `PXTOOL/pv/api/app_service.*`
- Modify: `PXTOOL/pv/appcontrol.*`
- Modify: `PXTOOL/pv/mainwindow.cpp`
- Modify: `CMakeLists.txt`

- [ ] Replace PXView-only calls with PXTOOL equivalents or `ConfigNotSupported`.
- [ ] Keep MCP startup deferred until `rpc_dispatcher.*` exists.
- [ ] Build until failures are outside service-layer adaptation.
- [ ] Commit.

## Task B: Dispatcher Slice

**Files:**
- Create: `PXTOOL/pv/api/rpc_dispatcher.*`
- Modify: `PXTOOL/pv/appcontrol.cpp`
- Modify: `CMakeLists.txt`

- [ ] Copy PXView dispatcher.
- [ ] Rename product metadata to PXTOOL / `pxtool`.
- [ ] Keep the 17 MCP tools.
- [ ] Remove Task A dispatcher guard if no longer needed.
- [ ] Build and commit.

## Task C: Sidebar MCP Panel Slice

**Files:**
- Create: `PXTOOL/pv/dock/mcpcontroldock.*`
- Modify: `PXTOOL/pv/dock/sidebar.*`
- Modify: `PXTOOL/themes/dark.qss`
- Modify: `PXTOOL/themes/light.qss`
- Modify: `PXTOOL/PXTOOL.qrc`
- Create: `PXTOOL/icons/sidebar/workflow.svg`
- Modify: `CMakeLists.txt`

- [ ] Add MCP tab between Filter and Log.
- [ ] Match PXTOOL Device Options / Decode Protocol title and group styles.
- [ ] Use `pxtool` commands.
- [ ] Build and commit.

## Task D: Web Client Slice

**Files:**
- Create/modify: `web/**`
- Modify: `CMakeLists.txt`

- [ ] Copy PXView web client.
- [ ] Rename storage keys and visible text from PXView to PXTOOL.
- [ ] Add optional `webui` target and runtime staging.
- [ ] Build web if npm exists and commit.

## Task E: End-to-End Verification Slice

- [ ] Build `DSView`.
- [ ] Launch PXTOOL if executable is not locked.
- [ ] Verify MCP initialize, tools/list, get_devices.
- [ ] Run `git diff --check` and final review.
