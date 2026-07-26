# PXTOOL Data Acquisition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Deliver a capability-driven Data Acquisition page that captures standard analog data, renders it, persists/exports it, and creates protocol-decoder-ready derived logic traces.

**Architecture:** Keep the existing DevMode -> SigSession -> DeviceAgent mode-switch path. Extend AnalogSnapshot into a validated typed store for legacy ADC and standard libsigrok float packets. A new AnalogToLogic owns regenerated packed LogicSnapshot data and is exposed to protocol decoders only as a derived trace.

**Tech Stack:** C++17, Qt Widgets, GLib/libsigrok, libsigrokdecode, Boost.Test, CMake.

---

## File Structure

- PXTOOL/pv/deviceagent.{h,cpp}: derive the displayed modes from actual channel types.
- PXTOOL/pv/data/analogsnapshot.{h,cpp}: validate, retain, and read typed analog samples.
- PXTOOL/pv/data/analogtologic.{h,cpp}: threshold/Schmitt conversion into a derived LogicSnapshot.
- PXTOOL/pv/view/analogsignal.{h,cpp}: auto-range, V/div, grid, and conversion controls.
- PXTOOL/pv/view/analogtologicsignal.{h,cpp}: display the derived logic trace.
- PXTOOL/pv/sigsession.{h,cpp}: own derived traces, normalize analog packets, and resolve derived decoder inputs.
- PXTOOL/pv/dialogs/decoderoptionsdlg.cpp and PXTOOL/pv/data/decoderstack.cpp: select and decode derived logic.
- PXTOOL/pv/storesession.{h,cpp}: retain analog metadata/configuration and export typed samples.
- PXTOOL/test/test_analogsnapshot.cpp and PXTOOL/test/test_analogtologic.cpp: focused new tests.
- PXTOOL/test/CMakeLists.txt: compile test and production sources.

### Task 1: Derive Available Device Modes

**Files:**
- Modify: PXTOOL/pv/deviceagent.h
- Modify: PXTOOL/pv/deviceagent.cpp
- Modify: PXTOOL/test/test_deviceagent_capability.cpp

- [x] **Step 1: Write the failing capability test**

    BOOST_AUTO_TEST_CASE(device_modes_include_analog_only_when_analog_channels_exist)
    {
        DeviceAgent agent;
        agent.bind_custom_device(make_device({SR_CHANNEL_LOGIC}), DEV_TYPE_DEMO,
                                 LOGIC, "test", "", "test", 1'000'000, 1024);
        BOOST_CHECK(!mode_list_contains(agent.get_device_mode_list(), ANALOG));

        agent.bind_custom_device(make_device({SR_CHANNEL_ANALOG}), DEV_TYPE_DEMO,
                                 ANALOG, "test", "", "test", 1'000'000, 1024);
        BOOST_CHECK(mode_list_contains(agent.get_device_mode_list(), ANALOG));
        BOOST_CHECK(!mode_list_contains(agent.get_device_mode_list(), LOGIC));
    }

- [x] **Step 2: Run the test and verify it fails**

Run: cmake --build build --target DSView-test && ./PXTOOL/test/DSView-test --run_test=device_modes_include_analog_only_when_analog_channels_exist

Expected: FAIL because imported devices advertise only their preset mode.

- [x] **Step 3: Implement the cached capability list**

Add GSList *_mode_list_cache and rebuild_mode_list(). Scan get_channels() and append static mode entries in this order:

    if (has_logic && (has_analog || has_dso))
        append(MSO);
    if (has_logic)
        append(LOGIC);
    if (has_analog)
        append(ANALOG);
    if (has_dso)
        append(DSO);

Free/rebuild the cache on bind and release. Return the cache for imported devices, retaining native driver lists for native devices.

- [x] **Step 4: Run the capability test**

Run: cmake --build build --target DSView-test && ./PXTOOL/test/DSView-test --run_test=device_modes_include_analog_only_when_analog_channels_exist

Expected: PASS.

- [x] **Step 5: Commit**

    git add PXTOOL/pv/deviceagent.h PXTOOL/pv/deviceagent.cpp PXTOOL/test/test_deviceagent_capability.cpp
    git commit -m "feat: derive PXTOOL device modes from channels"

### Task 2: Store Standard Float and Legacy Integer Analog Samples

**Files:**
- Modify: PXTOOL/pv/data/analogsnapshot.h
- Modify: PXTOOL/pv/data/analogsnapshot.cpp
- Create: PXTOOL/test/test_analogsnapshot.cpp
- Modify: PXTOOL/test/CMakeLists.txt

- [x] **Step 1: Write failing typed-storage tests**

    BOOST_AUTO_TEST_CASE(analog_snapshot_keeps_float_channels_and_extrema)
    {
        AnalogSnapshot snapshot;
        auto packet = makeAnalogPacket({{"A0", 0}, {"A1", 1}},
                                       {1.F, 10.F, 2.F, 20.F}, 2, 1'000'000,
                                       SR_MQ_VOLTAGE, SR_UNIT_VOLT);
        BOOST_REQUIRE(snapshot.first_payload(packet.analog, 8, packet.channels));
        BOOST_CHECK_CLOSE(snapshot.sample_as_double(0, 1), 2.0, 0.001);
        BOOST_CHECK_EQUAL(snapshot.sample_as_double(1, 0), 10.0);
        BOOST_CHECK_CLOSE(snapshot.channel_min(0), 1.0, 0.001);
        BOOST_CHECK_CLOSE(snapshot.channel_max(1), 20.0, 0.001);
    }

Add one legacy unsigned-8 append/ring-order case and one null encoding/meaning/data case that must return false without mutating the snapshot.

- [x] **Step 2: Run the tests and verify they fail**

Run: cmake --build build --target DSView-test && ./PXTOOL/test/DSView-test --run_test=analog_snapshot_*

Expected: compile failure because the typed accessors and boolean ingestion results do not exist.

- [x] **Step 3: Implement validated typed ingestion**

Make first_payload() and append_payload() return bool. Add explicit encoding kind, source channel order, per-channel minimum/maximum, and raw backing storage. Standard packets require encoding, meaning, meaning->channels, data, a 1/2/4/8 byte unit, and supported integer/float encoding. Legacy packets retain unit_bits, unit_pitch, and probes.

Expose:

    bool is_float() const;
    double sample_as_double(uint32_t channel_order, uint64_t sample_index) const;
    double channel_min(uint32_t channel_order) const;
    double channel_max(uint32_t channel_order) const;
    uint32_t channel_count() const;

Translate logical indexes through _ring_sample_count, update extrema during append, and retain raw accessors for StoreSession.

- [x] **Step 4: Register and run the tests**

Add test_analogsnapshot.cpp and analogsnapshot.cpp to DSView-test. Run: cmake --build build --target DSView-test && ./PXTOOL/test/DSView-test --run_test=analog_snapshot_*

Expected: PASS.

- [x] **Step 5: Commit**

    git add PXTOOL/pv/data/analogsnapshot.h PXTOOL/pv/data/analogsnapshot.cpp PXTOOL/test/test_analogsnapshot.cpp PXTOOL/test/CMakeLists.txt
    git commit -m "feat: support typed analog snapshot samples"

### Task 3: Normalize Packets and Render Analog Samples

**Files:**
- Modify: PXTOOL/pv/sigsession.cpp
- Modify: PXTOOL/pv/view/analogsignal.h
- Modify: PXTOOL/pv/view/analogsignal.cpp
- Modify: PXTOOL/test/test_analogpacketadapter.cpp

- [x] **Step 1: Write a failing standard-packet session test**

Feed makeAnalogPacket() through the public analog feed seam. Assert float metadata and channel mapping survive. Feed a packet with null encoding and assert Pkt_data_err/session_error is reported while the prior snapshot samples remain unchanged.

- [x] **Step 2: Run it and verify it fails**

Run: cmake --build build --target DSView-test && ./PXTOOL/test/DSView-test --run_test=analog_session_*

Expected: FAIL because feed_in_analog() rewrites standard fields as legacy unit_bits data.

- [x] **Step 3: Normalize without guessing**

Replace the compatibility copy with a helper accepting either complete standard metadata or complete legacy metadata. Invalid packets set Pkt_data_err, invoke session_error(), and never mutate AnalogSnapshot. Honor false from both snapshot ingestion methods.

- [x] **Step 4: Add range-driven rendering controls**

Add bool _auto_range, double _volts_per_div, and display bounds to AnalogSignal. Auto mode derives bounds from channel_min/max and expands a constant signal by one unit. Manual mode uses NumSpanY * _volts_per_div above/below zero. Float rendering reads sample_as_double(); legacy ADC rendering remains unchanged. Add an analog trace popup with auto-range and V/div controls whose updates repaint only.

- [x] **Step 5: Run targeted tests and commit**

Run: cmake --build build --target DSView-test && ./PXTOOL/test/DSView-test --run_test=analog_*

Expected: PASS.

    git add PXTOOL/pv/sigsession.cpp PXTOOL/pv/view/analogsignal.h PXTOOL/pv/view/analogsignal.cpp PXTOOL/test/test_analogpacketadapter.cpp
    git commit -m "feat: render standard analog captures"

### Task 4: Build Derived Analog-to-Logic Traces

**Files:**
- Create: PXTOOL/pv/data/analogtologic.h
- Create: PXTOOL/pv/data/analogtologic.cpp
- Create: PXTOOL/pv/view/analogtologicsignal.h
- Create: PXTOOL/pv/view/analogtologicsignal.cpp
- Modify: PXTOOL/pv/sigsession.h
- Modify: PXTOOL/pv/sigsession.cpp
- Create: PXTOOL/test/test_analogtologic.cpp
- Modify: PXTOOL/test/CMakeLists.txt

- [x] **Step 1: Write failing conversion tests**

    BOOST_AUTO_TEST_CASE(analog_to_logic_threshold_uses_high_as_one)
    {
        BOOST_CHECK_EQUAL(bits(convert_threshold({-1.0, 0.0, 0.1, -0.1}, 0.0)), "0110");
    }

    BOOST_AUTO_TEST_CASE(analog_to_logic_schmitt_keeps_state_between_thresholds)
    {
        BOOST_CHECK_EQUAL(bits(convert_schmitt({-1.0, 0.6, 0.0, -0.6, 0.0},
                                              -0.5, 0.5)), "01100");
    }

- [x] **Step 2: Run them and verify they fail**

Run: cmake --build build --target DSView-test && ./PXTOOL/test/DSView-test --run_test=analog_to_logic_*

Expected: compile failure because no converter exists.

- [x] **Step 3: Implement AnalogToLogic**

Define Mode { Disabled, Threshold, Schmitt } and Config { Mode mode; double threshold; double low; double high; }. rebuild() reads AnalogSnapshot::sample_as_double(), packs 0/1 values into an owned LogicSnapshot, and records source sample count/config generation. Threshold is high for value >= threshold. Schmitt sets high at >= high, low at <= low, and holds state between values. Reject low >= high.

- [x] **Step 4: Own and invalidate derived traces in SigSession**

Add owned derived trace/converter maps keyed by analog channel. Create a trace only when conversion is enabled. Destroy/rebuild derived data before decoder notification on new analog samples, capture reset, device/file close, mode switch, or configuration change. Assign indexes from DerivedLogicIndexBase + analog channel index; never add these channels to DeviceAgent.

- [x] **Step 5: Add popup controls, build, test, and commit**

Add conversion mode and threshold widgets to AnalogSignal's popup and call a SigSession conversion slot. Add all new sources to DSView-test.

Run: cmake --build build --target DSView-test && ./PXTOOL/test/DSView-test --run_test=analog_to_logic_*

Expected: PASS.

    git add PXTOOL/pv/data/analogtologic.h PXTOOL/pv/data/analogtologic.cpp PXTOOL/pv/view/analogtologicsignal.h PXTOOL/pv/view/analogtologicsignal.cpp PXTOOL/pv/sigsession.h PXTOOL/pv/sigsession.cpp PXTOOL/test/test_analogtologic.cpp PXTOOL/test/CMakeLists.txt
    git commit -m "feat: derive logic traces from analog signals"

### Task 5: Bind Derived Logic to Protocol Decoders

**Files:**
- Modify: PXTOOL/pv/dialogs/decoderoptionsdlg.cpp
- Modify: PXTOOL/pv/data/decoderstack.cpp
- Modify: PXTOOL/pv/sigsession.{h,cpp}
- Modify: PXTOOL/test/test_analogtologic.cpp

- [ ] **Step 1: Write the failing decoder-input test**

Create an enabled analog conversion. Assert SigSession::decoder_input_signals() contains its synthetic index and display name but never the raw analog trace. Disable conversion and assert it disappears.

- [ ] **Step 2: Run it and verify it fails**

Run: cmake --build build --target DSView-test && ./PXTOOL/test/DSView-test --run_test=decoder_inputs_include_derived_logic

Expected: compile failure because the decoder input API does not exist.

- [x] **Step 3: Query and present decoder inputs**

Add decoder_input_signals(), returning enabled physical LogicSignal objects plus enabled AnalogToLogicSignal objects. Replace signal_type() == SR_CHANNEL_LOGIC filtering in DecoderOptionsDlg::effectiveSession() and create_probe_selector() with this query while preserving index-based selection.

- [x] **Step 4: Resolve synthetic inputs in DecoderStack**

When DecoderStack resolves a binding index, route indexes >= DerivedLogicIndexBase to SigSession::derived_logic_snapshot(index). Keep physical logic lookup unchanged. A missing/stale derived snapshot must use the existing decoder error path.

- [ ] **Step 5: Run regression tests and commit**

Run: cmake --build build --target DSView-test && ./PXTOOL/test/DSView-test --run_test=decoder_inputs_include_derived_logic,DecoderStackTest/*

Expected: PASS.

    git add PXTOOL/pv/dialogs/decoderoptionsdlg.cpp PXTOOL/pv/data/decoderstack.cpp PXTOOL/pv/sigsession.h PXTOOL/pv/sigsession.cpp PXTOOL/test/test_analogtologic.cpp
    git commit -m "feat: allow protocol decoders on derived analog logic"

### Task 6: Persist and Export Typed Analog Captures

**Files:**
- Modify: PXTOOL/pv/storesession.{h,cpp}
- Modify: PXTOOL/pv/data/analogpacketadapter.cpp
- Modify: PXTOOL/test/test_analogpacketadapter.cpp
- Modify: PXTOOL/test/test_format_integration.cpp

- [ ] **Step 1: Write failing persistence/export tests**

Save/reload a two-channel float snapshot and assert names, float encoding, and samples survive. Export it using the standard analog output and assert it receives float SR_DF_ANALOG data in source order. Assert conversion configuration is saved but packed derived bits are not serialized.

- [ ] **Step 2: Run them and verify they fail**

Run: cmake --build build --target DSView-format-integration-test && ./PXTOOL/test/DSView-format-integration-test --run_test=analog_*

Expected: FAIL because standard export accepts only legacy unsigned-8 snapshots.

- [x] **Step 3: Export typed values and persist conversion configuration**

Build standard analog packets from sample_as_double(). Emit stored floats for float snapshots; retain current reference-range conversion for integers. Persist conversion mode and thresholds per analog channel, recreate configuration after loading raw analog data, then regenerate derived logic.

- [ ] **Step 4: Run tests and commit**

Run: cmake --build build --target DSView-format-integration-test && ./PXTOOL/test/DSView-format-integration-test --run_test=analog_*

Expected: PASS.

    git add PXTOOL/pv/storesession.cpp PXTOOL/pv/storesession.h PXTOOL/pv/data/analogpacketadapter.cpp PXTOOL/test/test_analogpacketadapter.cpp PXTOOL/test/test_format_integration.cpp
    git commit -m "feat: persist and export typed analog captures"

### Task 7: Full Verification

**Files:**
- Modify: docs/superpowers/specs/2026-07-25-pxtool-data-acquisition-design.md

- [x] **Step 1: Build every test executable**

Run: cmake --build build --target DSView-test DSView-format-integration-test srd-c-decoder-lifetime-test

Expected: successful build.

- [x] **Step 2: Run all automated tests**

Run: ctest --test-dir build --output-on-failure

Expected: PASS.

- [ ] **Step 3: Perform hardware acceptance**

Use a device exposing SR_CHANNEL_ANALOG. Confirm that the selector exposes Data Acquisition, switching updates channels/toolbars, capture works, auto-range and V/div work, threshold conversion creates a selectable decoder input, protocol decoding runs, and switching back removes stale derived traces.

- [x] **Step 4: Record and commit verification**

Append a Verification Result section with exact automated results and either the tested hardware/model or Not run: hardware unavailable.

    git add -f docs/superpowers/specs/2026-07-25-pxtool-data-acquisition-design.md
    git commit -m "docs: record data acquisition verification"

## Plan Self-Review

Spec coverage: Tasks 1-3 cover selector visibility, capture, validation, display, grid, V/div, and auto-range. Tasks 4-5 cover derived threshold/Schmitt logic and decoder binding. Task 6 covers persistence/export. Task 7 covers automated and hardware acceptance. MSO remains out of scope.

Placeholder scan: each task names concrete files, behaviors, commands, expected results, and commit paths.

Type consistency: AnalogSnapshot, AnalogToLogic, AnalogToLogicSignal, DerivedLogicIndexBase, decoder_input_signals(), and derived_logic_snapshot() keep the same meaning in every task.
