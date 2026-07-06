# Channel Tint Background Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add PXView-style per-channel tint backgrounds to DSView's time-domain waveform rows.

**Architecture:** Put the tint calculation in a small helper under `PXTOOL/pv/view/` so color and row rectangle behavior can be unit tested without constructing a full `Viewport`. Call the helper from `Viewport::paintEvent()` immediately before each trace's existing `paint_back()` call.

**Tech Stack:** C++/Qt painting, `QColor`, `QRect`, existing Boost unit test executable `DSView-test`.

---

### Task 1: Add Tested Tint Helper

**Files:**
- Create: `PXTOOL/pv/view/channeltint.h`
- Create: `PXTOOL/pv/view/channeltint.cpp`
- Create: `PXTOOL/test/test_channeltint.cpp`
- Modify: `PXTOOL/test/CMakeLists.txt`

- [ ] **Step 1: Write the failing tests**

Create `PXTOOL/test/test_channeltint.cpp` with tests for light/dark alpha, invalid colors, disabled traces, missing rows, and clipped row rectangles.

- [ ] **Step 2: Add the test file to the test target**

Modify `PXTOOL/test/CMakeLists.txt` so `test_channeltint.cpp` is compiled into `DSView-test`.

- [ ] **Step 3: Run the test target and verify RED**

Run: `cmake --build build.macOS --target DSView-test`

Expected: build fails because `PXTOOL/pv/view/channeltint.h` does not exist.

- [ ] **Step 4: Implement the helper**

Create `PXTOOL/pv/view/channeltint.h` and `PXTOOL/pv/view/channeltint.cpp` with pure helper functions:

- `QColor channel_tint_color(QColor trace_colour, QColor back, bool enabled, int rows);`
- `QRect channel_tint_rect(int viewport_width, int viewport_height, int center_y, int total_height, int margin);`

- [ ] **Step 5: Run helper tests and verify GREEN**

Run: `cmake --build build.macOS --target DSView-test && ./build.macOS/PXTOOL/test/DSView-test --run_test=channel_tint`

Expected: the channel tint tests pass.

### Task 2: Wire Tint Painting Into Viewport

**Files:**
- Modify: `PXTOOL/pv/view/viewport.h`
- Modify: `PXTOOL/pv/view/viewport.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Declare a private paint helper**

Add `void paintChannelTint(QPainter &p, Trace *trace, QColor back);` to `Viewport`.

- [ ] **Step 2: Call it before existing background painting**

In `Viewport::paintEvent()`, call `paintChannelTint(p, t, back);` before `t->paint_back(...)` inside the trace background loop.

- [ ] **Step 3: Implement the paint helper**

Use `channel_tint_color()` and `channel_tint_rect()` to fill a clipped row rectangle for applicable `TIME_VIEW` traces. Skip non-time views and traces with no valid tint color.

- [ ] **Step 4: Add helper source to the app build**

Add `PXTOOL/pv/view/channeltint.cpp` and `PXTOOL/pv/view/channeltint.h` to the relevant `DSView_SOURCES` and `DSView_HEADERS` lists in `CMakeLists.txt`.

- [ ] **Step 5: Build and verify**

Run: `cmake --build build.macOS --target PXTOOL`

Expected: the app target builds successfully.

### Task 3: Final Verification

**Files:**
- Inspect: `PXTOOL/pv/view/viewport.cpp`
- Inspect: `PXTOOL/pv/view/channeltint.cpp`

- [ ] **Step 1: Run unit tests**

Run: `./build.macOS/PXTOOL/test/DSView-test --run_test=channel_tint`

Expected: all `channel_tint` tests pass.

- [ ] **Step 2: Review the diff**

Run: `git diff -- PXTOOL/pv/view/channeltint.h PXTOOL/pv/view/channeltint.cpp PXTOOL/pv/view/viewport.h PXTOOL/pv/view/viewport.cpp PXTOOL/test/test_channeltint.cpp PXTOOL/test/CMakeLists.txt CMakeLists.txt`

Expected: only the helper, tests, build wiring, and viewport paint hook changed.

- [ ] **Step 3: Commit implementation**

Commit message: `feat: add channel tint waveform backgrounds`
