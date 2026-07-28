# PXTOOL MSO Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a real MSO mode that captures and displays logic and analog channels together in PXTOOL.

**Architecture:** Add `MSO` to the libsigrok contract, implement simultaneous native-demo feeds, then centralize MSO policy in PXTOOL session and view layers. Reuse the existing LogicSnapshot, AnalogSnapshot, LogicSignal, and AnalogSignal types.

**Tech Stack:** C/C++17, Qt Widgets, GLib, libsigrok datafeed API, Boost.Test, CMake.

---

## File Structure

- `libsigrok/libsigrok.h`: public mode value.
- `libsigrok/libsigrok-internal.h`: descriptors indexed by work-mode value.
- `libsigrok/hardware/demo/demo.[ch]`: mixed demo channels, feeds, and completion.
- `PXTOOL/pv/deviceagent.cpp`: capability-derived mode list.
- `PXTOOL/pv/sigsession.[ch]`: channel and logic-capability policy.
- `PXTOOL/pv/view/devmode.cpp`: selector label and icon.
- `PXTOOL/pv/view/view.[ch]`: logic-only UI behavior under MSO.
- `PXTOOL/test/test_deviceagent_capability.cpp`, `test_upstream_demo.cpp`, and new `test_mso_mode.cpp`: regression coverage.

### Task 1: Add the MSO mode contract

**Files:**
- Modify: `libsigrok/libsigrok.h:645-651`
- Modify: `libsigrok/libsigrok-internal.h:338-344`
- Modify: `PXTOOL/pv/deviceagent.cpp:56-85`
- Modify: `PXTOOL/test/test_deviceagent_capability.cpp:122-145`

- [ ] **Step 1: Write the failing capability test**

```cpp
BOOST_AUTO_TEST_CASE(imported_mixed_device_lists_mso_first)
{
    sr_dev_inst *sdi = sr_dev_inst_new(MSO, SR_ST_INACTIVE, "DSView", "Mixed", "0.1");
    BOOST_REQUIRE(sdi != nullptr);
    BOOST_REQUIRE(sr_channel_new(sdi, 0, SR_CHANNEL_LOGIC, TRUE, "D0"));
    BOOST_REQUIRE(sr_channel_new(sdi, 1, SR_CHANNEL_ANALOG, TRUE, "A0"));
    DeviceAgent agent;
    agent.bind_custom_device(sdi, DEV_TYPE_DEMO, MSO, "Mixed", "", "mixed", 1000000, 1024);
    const GSList *modes = agent.get_device_mode_list();
    BOOST_REQUIRE(modes != nullptr);
    BOOST_CHECK_EQUAL(static_cast<const sr_dev_mode *>(modes->data)->mode, MSO);
    agent.release();
}
```

- [ ] **Step 2: Verify the test fails before implementation**

Run: `cmake --build build.macOS --target DSView-test && build.macOS/PXTOOL/test/DSView-test --run_test=deviceagent_capability/imported_mixed_device_lists_mso_first`

Expected: build failure because `MSO` is not declared.

- [ ] **Step 3: Add the enum, descriptor, and DeviceAgent capability item**

```c
enum OPERATION_MODE { LOGIC = 0, DSO = 1, ANALOG = 2, MSO = 3, UNKNOWN_DSL_MODE = 99 };
```

```c
static const struct sr_dev_mode sr_mode_list[] = {
    [LOGIC] = {LOGIC, "Logic Analyzer", "la"},
    [DSO] = {DSO, "Oscilloscope", "osc"},
    [ANALOG] = {ANALOG, "Data Acquisition", "daq"},
    [MSO] = {MSO, "Mixed Signal Oscilloscope", "mso"},
};
```

```cpp
if (has_logic && has_analog)
    append(MSO, "Mixed Signal Oscilloscope", "mso");
```

The MSO `append()` call must precede independent LOGIC, ANALOG, and DSO entries. Do not add it when either required type is absent.

- [ ] **Step 4: Run focused and full DeviceAgent tests**

Run: `cmake --build build.macOS --target DSView-test && build.macOS/PXTOOL/test/DSView-test --run_test=deviceagent_capability`

Expected: all `deviceagent_capability` cases pass.

- [ ] **Step 5: Commit**

```bash
git add libsigrok/libsigrok.h libsigrok/libsigrok-internal.h PXTOOL/pv/deviceagent.cpp PXTOOL/test/test_deviceagent_capability.cpp
git commit -m "feat: define MSO device mode"
```

### Task 2: Add simultaneous MSO output to the native demo

**Files:**
- Modify: `libsigrok/hardware/demo/demo.h:190-260`
- Modify: `libsigrok/hardware/demo/demo.c:715-730,1040-1055,1420-1600,2496-2610`
- Modify: `PXTOOL/test/test_upstream_demo.cpp:46-293`

- [ ] **Step 1: Write the failing dual-feed test**

```cpp
BOOST_AUTO_TEST_CASE(mso_capture_emits_logic_and_analog_before_one_end)
{
    sr_dev_inst *sdi = create_upstream_demo_device();
    BOOST_REQUIRE(sdi != nullptr);
    BOOST_REQUIRE_EQUAL(upstream_demo_driver_info.config_set(SR_CONF_DEVICE_MODE, g_variant_new_int16(MSO), sdi, nullptr, nullptr), SR_OK);
    BOOST_REQUIRE_EQUAL(upstream_demo_driver_info.config_set(SR_CONF_LIMIT_SAMPLES, g_variant_new_uint64(1024), sdi, nullptr, nullptr), SR_OK);
    test_input_observer_reset();
    BOOST_REQUIRE_EQUAL(upstream_demo_driver_info.dev_acquisition_start(sdi, nullptr), SR_OK);
    test_input_observer_run_until_end();
    BOOST_CHECK_GT(test_input_observer_logic_packets(), 0U);
    BOOST_CHECK_GT(test_input_observer_analog_packets(), 0U);
    BOOST_CHECK_EQUAL(test_input_observer_end_packets(), 1U);
    upstream_demo_driver_info.dev_destroy(sdi);
}
```

- [ ] **Step 2: Verify the test fails**

Run: `cmake --build build.macOS --target DSView-test && build.macOS/PXTOOL/test/DSView-test --run_test=upstream_demo/mso_capture_emits_logic_and_analog_before_one_end`

Expected: failure because the demo currently starts one producer per mode.

- [ ] **Step 3: Add per-feed completion state and one end helper**

```c
gboolean mso_logic_done;
gboolean mso_analog_done;
gboolean mso_end_sent;

static void demo_mso_finish_feed(const struct sr_dev_inst *sdi, gboolean logic)
{
    struct session_vdev *vdev = sdi->priv;
    if (logic) vdev->mso_logic_done = TRUE; else vdev->mso_analog_done = TRUE;
    if (vdev->mso_logic_done && vdev->mso_analog_done && !vdev->mso_end_sent) {
        struct sr_datafeed_packet packet = { .type = SR_DF_END, .status = SR_PKT_OK, .payload = NULL };
        vdev->mso_end_sent = TRUE;
        ds_data_forward(sdi, &packet);
        sr_session_source_remove(-1);
    }
}
```

When `sdi->mode == MSO`, each logic or analog callback calls this helper at its normal limit instead of independently sending `SR_DF_END`. Preserve existing end paths for every other mode.

- [ ] **Step 4: Configure and schedule both producers in MSO**

In `config_set(SR_CONF_DEVICE_MODE)`, retain/create both logic and analog channel groups for `MSO`, set the analog generator path, and set the logic random generator. In `hw_dev_acquisition_start()`, reset all three completion fields and register both sources:

```c
if (sdi->mode == MSO) {
    vdev->mso_logic_done = FALSE;
    vdev->mso_analog_done = FALSE;
    vdev->mso_end_sent = FALSE;
    sr_session_source_add(-1, 0, 0, receive_data_logic, sdi);
    sr_session_source_add(-1, 0, 0, receive_data_analog_generated, sdi);
    return SR_OK;
}
```

Count enabled logic and analog channels separately and return `SR_ERR_ARG` before scheduling when either count is zero. Never use a mixed total as the logic packet divisor.

- [ ] **Step 5: Run the demo suite**

Run: `cmake --build build.macOS --target DSView-test && build.macOS/PXTOOL/test/DSView-test --run_test=upstream_demo`

Expected: all demo tests pass, including one end after both feeds.

- [ ] **Step 6: Commit**

```bash
git add libsigrok/hardware/demo/demo.c libsigrok/hardware/demo/demo.h PXTOOL/test/test_upstream_demo.cpp
git commit -m "feat: emit mixed signal demo captures"
```

### Task 3: Centralize MSO session policy

**Files:**
- Create: `PXTOOL/test/test_mso_mode.cpp`
- Modify: `PXTOOL/test/CMakeLists.txt:32-90`
- Modify: `PXTOOL/pv/sigsession.h:175-240`
- Modify: `PXTOOL/pv/sigsession.cpp:985-1300,2757-2800,3018-3035`

- [ ] **Step 1: Write failing policy tests**

```cpp
BOOST_AUTO_TEST_CASE(mso_is_logic_capable_but_not_dso)
{
    BOOST_CHECK(SigSession::is_logic_capable_mode(MSO));
    BOOST_CHECK(!SigSession::is_dso_mode(MSO));
}

BOOST_AUTO_TEST_CASE(mso_accepts_logic_and_analog_only)
{
    BOOST_CHECK(SigSession::channel_type_visible_in_mode(MSO, SR_CHANNEL_LOGIC));
    BOOST_CHECK(SigSession::channel_type_visible_in_mode(MSO, SR_CHANNEL_ANALOG));
    BOOST_CHECK(!SigSession::channel_type_visible_in_mode(MSO, SR_CHANNEL_DSO));
}
```

- [ ] **Step 2: Add the test source and verify it fails**

Add `test_mso_mode.cpp` to `DSView-test`, then run: `cmake --build build.macOS --target DSView-test && build.macOS/PXTOOL/test/DSView-test --run_test=mso_mode`

Expected: build failure because the policy methods do not exist.

- [ ] **Step 3: Implement the shared policy**

```cpp
static bool is_logic_capable_mode(int mode) { return mode == LOGIC || mode == MSO; }
static bool is_dso_mode(int mode) { return mode == DSO; }

bool SigSession::channel_type_visible_in_mode(int mode, int type)
{
    if (mode == LOGIC) return type == SR_CHANNEL_LOGIC;
    if (mode == ANALOG) return type == SR_CHANNEL_ANALOG;
    if (mode == DSO) return type == SR_CHANNEL_DSO;
    if (mode == MSO) return type == SR_CHANNEL_LOGIC || type == SR_CHANNEL_ANALOG;
    return false;
}
```

- [ ] **Step 4: Apply the policy to `init_signals()`, `reload()`, and `switch_work_mode()`**

Replace each mode-specific filtering group with `if (!channel_type_visible_in_mode(mode, probe->type)) continue;`. In `switch_work_mode()`, synchronize `probe->enabled` with the same predicate through the existing device-channel enable API. Use `is_logic_capable_mode()` for stream mode, decoder cleanup, and `get_ring_sample_count()`.

- [ ] **Step 5: Run policy and full unit tests**

Run: `cmake --build build.macOS --target DSView-test && build.macOS/PXTOOL/test/DSView-test --run_test=mso_mode && build.macOS/PXTOOL/test/DSView-test`

Expected: both commands pass.

- [ ] **Step 6: Commit**

```bash
git add PXTOOL/pv/sigsession.h PXTOOL/pv/sigsession.cpp PXTOOL/test/CMakeLists.txt PXTOOL/test/test_mso_mode.cpp
git commit -m "feat: configure mixed signal sessions"
```

### Task 4: Update selector, view behavior, and persistence

**Files:**
- Modify: `PXTOOL/pv/view/devmode.[ch]:35-290`
- Modify: `PXTOOL/pv/view/view.[ch]:230-325,500-575,825-900,1680-1740`
- Modify: `PXTOOL/test/test_mso_mode.cpp`
- Modify: `PXTOOL/test/test_format_integration.cpp:325-480` when persistence coverage is missing

- [ ] **Step 1: Add a failing selector assertion**

```cpp
BOOST_AUTO_TEST_CASE(mso_selector_name_is_available)
{
    BOOST_CHECK_EQUAL(DevMode::display_name_for_mode(MSO), QStringLiteral("Mixed Signal Oscilloscope"));
}
```

- [ ] **Step 2: Implement selector and view predicates**

Declare `static QString display_name_for_mode(int mode);` in `devmode.h`, add `{MSO, "osc.svg"}` to `dev_mode_name_list`, and define the method to return the four exact labels. Add `View::is_logic_rendering_mode()` returning `mode == LOGIC || mode == MSO`; use it only for logic tools: cursor list, RLE status, auto-scale, logic layout/order, signal height preset, and waveform copy menu. Preserve existing DSO guards and AnalogSignal rendering.

- [ ] **Step 3: Add mixed persistence coverage**

```cpp
BOOST_AUTO_TEST_CASE(mso_session_round_trip_preserves_channel_types)
{
    auto session = make_mso_test_session();
    const QString path = temporary_session_path("mso-roundtrip.pxs");
    BOOST_REQUIRE(save_session(session.get(), path));
    auto restored = load_session(path);
    BOOST_REQUIRE(restored);
    BOOST_CHECK_EQUAL(restored->get_ch_num(SR_CHANNEL_LOGIC), 1);
    BOOST_CHECK_EQUAL(restored->get_ch_num(SR_CHANNEL_ANALOG), 1);
}
```

If it fails, change `PXTOOL/pv/storesession.cpp` only where it selects one signal type from the first signal: explicitly collect and serialize logic and analog metadata/chunks. Do not change DSO serialization.

- [ ] **Step 4: Run all test targets and manual GUI verification**

Run: `cmake --build build.macOS --target DSView DSView-test DSView-format-integration-test && build.macOS/PXTOOL/test/DSView-test && build.macOS/PXTOOL/test/DSView-format-integration-test`

Expected: both test binaries pass.

Manual check: native demo -> select **Mixed Signal Oscilloscope** -> see digital and analog tracks -> capture -> decode `D0` -> move cursor over `A0` -> confirm no DSO controls.

- [ ] **Step 5: Commit**

```bash
git add PXTOOL/pv/view/devmode.cpp PXTOOL/pv/view/devmode.h PXTOOL/pv/view/view.cpp PXTOOL/pv/view/view.h PXTOOL/pv/storesession.cpp PXTOOL/test/test_mso_mode.cpp PXTOOL/test/test_format_integration.cpp
git commit -m "feat: display and persist mixed signal sessions"
```

## Plan Self-Review

- Spec coverage: Tasks 1-4 cover capability gating, concurrent feeds, coordinated completion, session construction, logic interaction, selector UI, and persistence.
- Placeholder scan: every implementation step identifies exact files, behavior, tests, and commands.
- Type consistency: the plan uses `MSO`, `SigSession::is_logic_capable_mode`, `SigSession::is_dso_mode`, `SigSession::channel_type_visible_in_mode`, and `View::is_logic_rendering_mode` consistently.
