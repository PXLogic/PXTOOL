# DevMode Compact Width Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep every current-mode icon/title group horizontally centered in the visible DevMode overlay.

**Architecture:** The current control has a stable compact width of 150 px, already sufficient for the icon and the longest compact label. Its dropdown popup continues to calculate its own width from the full labels, so no menu terminology is truncated. This prevents a full menu title from making the overlay child wider than the visible waveform margin.

**Tech Stack:** C++17, Qt Widgets, Boost.Test, native PXTOOL demo.

---

### Task 1: Decouple current-button width from menu labels

**Files:**
- Modify: `PXTOOL/pv/view/devmode.cpp:355-360`

- [ ] **Step 1: Establish the visual regression baseline**

In the native demo, select Analog, Oscilloscope, and Mixed Signal modes. Observe that each icon/title group is centered against a width derived from full menu labels and is clipped or appears shifted right in the visible overlay.

- [ ] **Step 2: Implement the compact width cap**

Replace `sync_mode_button_width()` with:

```cpp
void DevMode::sync_mode_button_width()
{
    _mode_btn->setFixedWidth(150);
}
```

Do not change the popup-width calculation in `set_device()`.

- [ ] **Step 3: Build and run MSO regression tests**

Run:

```bash
cmake --build build.tests --target DSView DSView-test -j2
./build.macOS/DSView-test --run_test=mso_mode --log_level=message
git diff --check
```

Expected: all commands exit 0.

- [ ] **Step 4: Verify actual geometry**

Start the native demo and switch among Logic Analyzer, Data Acquisition, Oscilloscope, and Mixed Signal. Confirm every icon/title group and chevron is horizontally centered in the visible 150 px control; confirm the selector popup still displays `Mixed Signal Oscilloscope` in full.

- [ ] **Step 5: Commit**

```bash
git add PXTOOL/pv/view/devmode.cpp
git add -f docs/superpowers/plans/2026-07-28-devmode-compact-width.md
git commit -m "fix: center compact mode button content"
```

## Self-review

- Spec coverage: button geometry becomes independent from full menu title length, while popup labels are unchanged.
- Placeholder scan: code, verification, and manual geometry checks are explicit.
- Type consistency: the existing `sync_mode_button_width()` keeps its signature and call sites.
