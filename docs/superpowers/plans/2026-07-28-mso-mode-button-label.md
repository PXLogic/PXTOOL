# MSO Mode Button Label Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Display `Mixed Signal` in the compact current-mode button while retaining `Mixed Signal Oscilloscope` in the mode menu and accessibility text.

**Architecture:** Keep `DevMode::display_name_for_mode()` as the full, menu-facing label source. Add a distinct static helper for the current button title, used exclusively by the custom `DevModeComboBox::paintEvent()`; this prevents layout constraints from altering the selection menu's terminology.

**Tech Stack:** C++17, Qt Widgets, Boost.Test.

---

### Task 1: Give MSO its compact current-button title

**Files:**
- Modify: `PXTOOL/test/test_mso_mode.cpp:42-48`
- Modify: `PXTOOL/pv/view/devmode.h:68-85`
- Modify: `PXTOOL/pv/view/devmode.cpp:84-100`

- [ ] **Step 1: Write the failing unit test**

Add this test to the existing `mso_mode` suite:

```cpp
BOOST_AUTO_TEST_CASE(mso_button_label_is_compact)
{
    BOOST_CHECK_EQUAL(pv::view::DevMode::button_label_for_mode(MSO).toStdString(),
                      std::string("Mixed Signal"));
}
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```bash
cmake --build build.tests --target DSView-test -j2
./build.macOS/DSView-test --run_test=mso_mode/mso_button_label_is_compact --log_level=message
```

Expected: compilation fails because `button_label_for_mode` is not yet declared.

- [ ] **Step 3: Implement the minimal label split**

Declare and define this static helper in `DevMode`:

```cpp
static QString button_label_for_mode(int mode)
{
    return mode == MSO ? QStringLiteral("Mixed Signal")
                       : display_name_for_mode(mode);
}
```

In `DevModeComboBox::paintEvent()`, replace `currentText()` with:

```cpp
const int mode = itemData(currentIndex()).toInt();
const QString text = DevMode::button_label_for_mode(mode);
```

Keep `set_device()` using `display_name_for_mode()` for menu items, and keep `sync_mode_button()` using the full label for tooltip and accessibility text.

- [ ] **Step 4: Run regression checks**

Run:

```bash
cmake --build build.tests --target DSView DSView-test -j2
./build.macOS/DSView-test --run_test=mso_mode --log_level=message
git diff --check
```

Expected: all commands exit 0.

- [ ] **Step 5: Verify the native button**

Start the built PXTOOL application in MSO mode. Confirm the purple current-mode control renders the complete `Mixed Signal` title, while opening the selector still shows `Mixed Signal Oscilloscope`.

- [ ] **Step 6: Commit**

```bash
git add PXTOOL/pv/view/devmode.h PXTOOL/pv/view/devmode.cpp \
        PXTOOL/test/test_mso_mode.cpp
git add -f docs/superpowers/plans/2026-07-28-mso-mode-button-label.md
git commit -m "fix: shorten mixed mode button label"
```

## Self-review

- Spec coverage: the plan separates the compact button label from full menu/accessibility wording.
- Placeholder scan: every code and verification step has an exact command or code block.
- Type consistency: `button_label_for_mode(int)` accepts the same mode type as `display_name_for_mode(int)` and is callable from the combo painter and Boost test.
