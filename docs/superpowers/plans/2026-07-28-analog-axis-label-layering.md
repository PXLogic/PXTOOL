# Analog Axis Label Layering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep analog voltage-axis labels visible above dense waveforms in MSO and analog views.

**Architecture:** `AnalogSignal::paint_back()` remains responsible for background grid lines and tick marks. A small helper will draw only the voltage text during `paint_fore()`, which `Viewport::doPaint()` calls after `paint_mid()` has drawn the waveform. This preserves existing waveform clipping and leaves logic and DSO paths unchanged.

**Tech Stack:** C++17, Qt `QPainter`, Boost test build targets, native PXTOOL demo.

---

### Task 1: Split analog grid and label layers

**Files:**
- Modify: `PXTOOL/pv/view/analogsignal.h:134-150`
- Modify: `PXTOOL/pv/view/analogsignal.cpp:403-530`

- [ ] **Step 1: Establish the visual regression baseline**

Run the native demo in MSO mode with a dense analog trace. Observe that the values at the left/right edges, such as `5.00V`, `0.00V`, and `-5.00V`, are painted before the waveform and become obscured.

- [ ] **Step 2: Add the focused label painter declaration**

Add this private method beside the existing paint methods in `AnalogSignal`:

```cpp
void paint_axis_labels(QPainter &p, int left, int right, QColor fore);
```

- [ ] **Step 3: Move only voltage text into the foreground layer**

Keep each `drawLine()` call in `paint_back()`. Replace its left/right `drawText()` calls with `paint_axis_labels(p, left, right, fore)`, called from `paint_fore()` after the zero reference line. The helper must reproduce the existing label positions and values: top, midpoint, and bottom divisions on both edges.

- [ ] **Step 4: Build and run focused regression coverage**

Run:

```bash
cmake --build build.tests --target DSView DSView-test -j2
./build.macOS/DSView-test --run_test=mso_mode --log_level=message
```

Expected: build exits 0 and all MSO mode checks pass.

- [ ] **Step 5: Verify the rendered layer ordering manually**

In the native demo, select **Mixed Signal Oscilloscope**, start a capture, and confirm the analog labels remain readable over a dense waveform. Confirm the same labels still appear at both viewport edges and DSO controls remain absent.

- [ ] **Step 6: Commit**

```bash
git add PXTOOL/pv/view/analogsignal.h PXTOOL/pv/view/analogsignal.cpp \
        docs/superpowers/plans/2026-07-28-analog-axis-label-layering.md
git commit -m "fix: draw analog axis labels above waveforms"
```

## Self-review

- Spec coverage: Task 1 separates grid/ticks from labels, draws labels after waveform data, and verifies MSO isolation.
- Placeholder scan: no implementation placeholders remain.
- Type consistency: `paint_axis_labels()` uses the same `QPainter`, bounds, and foreground color as the existing `paint_back()` / `paint_fore()` signatures.
