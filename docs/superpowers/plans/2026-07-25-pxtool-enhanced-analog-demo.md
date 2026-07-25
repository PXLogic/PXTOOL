# PXTOOL Enhanced Analog Demo Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make PXTOOL's built-in demo ANALOG mode generate five configurable channels of standard floating-point analog data without hardware.

**Architecture:** Extend the existing `libsigrok/hardware/demo` driver rather than creating a second demo device. Keep its legacy `.demo` replay path intact, and introduce a generated-data path selected by default for ANALOG mode. Per-channel generator state owns waveform parameters; packet construction owns only transient interleaved float data and standard libsigrok metadata.

**Tech Stack:** C, GLib, bundled libsigrok API, Boost.Test, CMake.

---

## File Structure

- `libsigrok/hardware/demo/demo.h`: analog generator constants, waveform enum, per-channel state, and helper declarations.
- `libsigrok/hardware/demo/demo.c`: channel creation, config get/set/list, generated float packet assembly, and legacy replay selection.
- `PXTOOL/test/test_upstream_demo.cpp`: driver-facing regression tests for channel count, configuration, and standard analog packets.
- `PXTOOL/test/CMakeLists.txt`: compile the demo driver into the test executable when required.

### Task 1: Define Generated Analog State

**Files:**
- Modify: `libsigrok/hardware/demo/demo.h`
- Modify: `libsigrok/hardware/demo/demo.c`
- Test: `PXTOOL/test/test_upstream_demo.cpp`

- [x] **Step 1: Write failing driver capability tests**

Add a test that scans a demo device with ANALOG mode and asserts five enabled
`SR_CHANNEL_ANALOG` channels. Add a second test that lists `SR_CONF_PATTERN_MODE`
for an analog channel and expects `sine`, `square`, `triangle`, `sawtooth`, and
`random`.

```cpp
BOOST_CHECK_EQUAL(analog_channel_count(sdi), 5);
BOOST_CHECK(pattern_list_contains(channel, "sawtooth"));
```

- [x] **Step 2: Run the tests and verify they fail**

Run: `cmake --build build.tests --target DSView-test && ./build.macOS/DSView-test --run_test=upstream_demo/analog_*`

Expected: FAIL because the current demo creates two analog channels and does
not expose generated waveform names as per-channel configuration.

- [x] **Step 3: Add the state model**

In `demo.h`, define `enum demo_analog_pattern` with the five names and a
`struct demo_analog_generator { enum demo_analog_pattern pattern; double amplitude; double offset; double phase; }`.
Store one generator per analog channel in `session_vdev`. Set defaults to sine,
1.0 V amplitude, 0.0 V offset, and an offset phase per channel.

Change `ANALOG_DEFAULT_NUM_PROBE` and `ANALOG_PROBE_NUM` to 5. In the ANALOG
branch of device creation, initialize one generator and one `SR_CHANNEL_ANALOG`
channel for each index.

- [x] **Step 4: Expose configuration**

Make `config_list`, `config_get`, and `config_set` recognize `SR_CONF_PATTERN_MODE`,
`SR_CONF_AMPLITUDE`, `SR_CONF_OFFSET`, and `SR_CONF_PROBE_EN` for individual
analog channels. Reject an unrecognized pattern and non-finite amplitude or
offset with `SR_ERR_ARG`.

- [x] **Step 5: Run tests and commit**

Run: `cmake --build build.tests --target DSView-test && ./build.macOS/DSView-test --run_test=upstream_demo/analog_*`

Expected: PASS.

```bash
git add libsigrok/hardware/demo/demo.h libsigrok/hardware/demo/demo.c PXTOOL/test/test_upstream_demo.cpp
git commit -m "feat: configure five-channel analog demo"
```

### Task 2: Emit Standard Float Analog Packets

**Files:**
- Modify: `libsigrok/hardware/demo/demo.c`
- Test: `PXTOOL/test/test_upstream_demo.cpp`

- [x] **Step 1: Write failing packet tests**

Add a session callback test that starts ANALOG demo acquisition and captures its
first `SR_DF_ANALOG` packet. Assert `encoding`, `meaning`, and `meaning->channels`
are non-null, `encoding->is_float` is true, `encoding->unitsize == sizeof(float)`,
and the packet contains enabled channels in index order.

```cpp
BOOST_REQUIRE(packet.analog.encoding->is_float);
BOOST_CHECK_EQUAL(packet.analog.encoding->unitsize, sizeof(float));
BOOST_CHECK_EQUAL(g_slist_length(packet.analog.meaning->channels), 5);
```

- [x] **Step 2: Run and verify RED**

Run: `cmake --build build.tests --target DSView-test && ./build.macOS/DSView-test --run_test=upstream_demo/generated_analog_packet_is_standard_float`

Expected: FAIL because the old path sends a legacy byte packet.

- [x] **Step 3: Implement generated packet assembly**

Add a helper in `demo.c` that gathers enabled analog channels, computes a bounded
packet sample count, and fills an interleaved `float` buffer. Generate each
sample from its channel state using normalized phase:

```c
value = offset + amplitude * waveform_value(pattern, phase);
phase = fmod(phase + frequency / samplerate, 1.0);
```

Initialize a local `sr_analog_encoding`, `sr_analog_meaning`, and
`sr_analog_spec` with `sr_analog_init`, set float encoding fields, and send an
`SR_DF_ANALOG` packet. Free the temporary channel list and buffer after the
session callback returns.

- [x] **Step 4: Keep legacy replay isolated**

Route explicitly selected `.demo` captures to the existing `receive_data_analog`
replay logic. Route the default ANALOG mode to the new generator. Do not combine
generated and replay bytes in an acquisition.

- [x] **Step 5: Run tests and commit**

Run: `cmake --build build.tests --target DSView-test && ./build.macOS/DSView-test --run_test=upstream_demo/generated_analog_packet_is_standard_float`

Expected: PASS.

```bash
git add libsigrok/hardware/demo/demo.c PXTOOL/test/test_upstream_demo.cpp
git commit -m "feat: emit standard float analog demo packets"
```

### Task 3: Verify Waveforms and Channel Enablement

**Files:**
- Modify: `PXTOOL/test/test_upstream_demo.cpp`
- Modify: `libsigrok/hardware/demo/demo.c`

- [x] **Step 1: Write failing waveform tests**

Configure channel 0 to square with amplitude 2.0 and offset 1.0, disable
channel 1, then capture one packet. Assert all channel-0 values are within
[-1.0, 3.0], values reach both square levels, and the packet's meaning list
omits channel 1.

```cpp
BOOST_CHECK(value >= -1.0F && value <= 3.0F);
BOOST_CHECK_EQUAL(g_slist_length(packet.analog.meaning->channels), 4);
```

- [x] **Step 2: Run and verify RED**

Run: `cmake --build build.tests --target DSView-test && ./build.macOS/DSView-test --run_test=upstream_demo/analog_waveform_*`

Expected: FAIL until the generated path honors per-channel settings.

- [x] **Step 3: Correct generator/config integration**

Use the per-channel generator state and `sr_channel::enabled` state when
assembling each packet. Ensure every generated waveform stays in [-1, 1] before
amplitude/offset transformation. Random samples must also use that normalized
range.

- [x] **Step 4: Run regression tests and commit**

Run: `cmake --build build.tests --target DSView-test && ./build.macOS/DSView-test --run_test=upstream_demo/analog_*`

Expected: PASS.

```bash
git add libsigrok/hardware/demo/demo.c PXTOOL/test/test_upstream_demo.cpp
git commit -m "test: cover analog demo waveforms"
```

### Task 4: End-to-End Verification

**Files:**
- Modify: `docs/superpowers/specs/2026-07-25-pxtool-enhanced-analog-demo-design.md`

- [x] **Step 1: Build relevant targets**

Run: `cmake --build build.tests --target DSView DSView-test DSView-format-integration-test -j2`

Expected: successful build.

- [x] **Step 2: Run automated suites**

Run: `ctest --test-dir build.tests --output-on-failure && ./build.macOS/DSView-format-integration-test --log_level=message`

Expected: PASS.

- [x] **Step 3: Run demo acceptance**

Launch `build.macOS/PXTOOL.app/Contents/MacOS/PXTOOL`, select Demo -> ANALOG,
confirm five traces are present, use the analog trace context menu to select a
waveform and threshold conversion, and confirm the derived logic trace is
available in the decoder input selector.

- [x] **Step 4: Record verification and commit**

Append exact automated results and either `Demo acceptance: passed` or the
unavailable condition to the design spec.

```bash
git add -f docs/superpowers/specs/2026-07-25-pxtool-enhanced-analog-demo-design.md
git commit -m "docs: verify enhanced analog demo"
```

## Plan Self-Review

Spec coverage: Tasks 1-3 implement five channels, standard float packets,
per-channel waveform configuration, channel enablement, and legacy replay
separation. Task 4 covers build, automated testing, and the in-app demo
acceptance flow.

Placeholder scan: every task names files, concrete assertions, commands, and
commit boundaries. No unspecified error behavior is left in the plan.

Type consistency: `demo_analog_pattern`, `demo_analog_generator`, and the
existing `sr_datafeed_analog` packet are used consistently across tasks.
