# PXTOOL Enhanced Analog Demo Design

## Goal

Upgrade the built-in PXTOOL demo device's ANALOG mode so that the data
acquisition page can be exercised without hardware using configurable,
standard libsigrok floating-point analog samples.

## Scope

- Keep the existing demo driver's LOGIC and DSO behaviour unchanged.
- Replace the current ANALOG-mode-only two-channel 8-bit replay/random path
  with five generated analog channels by default.
- Emit complete `sr_datafeed_analog` packets with encoding, meaning, and
  channel metadata. Samples are interleaved `float` voltage values.
- Configure each analog channel independently with pattern, amplitude, and
  offset. Supported patterns are sine, square, triangle, sawtooth, and random.
- Retain existing `.demo` analog replay as a fallback when a demo capture is
  explicitly selected. Generated data is the default for a new demo device.

## Architecture

The change is confined to `libsigrok/hardware/demo`.

`scan()` creates five `SR_CHANNEL_ANALOG` channels in ANALOG mode. Each channel
has a generator state containing phase, selected pattern, amplitude, and
offset. Acquisition advances phase from the configured sample rate and writes
interleaved float voltage samples into a reusable packet buffer.

The driver initializes `sr_analog_encoding`, `sr_analog_meaning`, and
`sr_analog_spec` once per generated packet. The packet contains only enabled
analog channels in stable channel-index order. PXTOOL's existing
`AnalogSnapshot` therefore receives the same standard float packet shape used
by imported analog data.

When replay mode is explicitly selected, the existing byte-oriented replay
path continues to feed its legacy analog packets. No generated data is mixed
with replay data in one acquisition.

## Configuration

The demo driver advertises the existing per-channel analog configuration keys:

- `SR_CONF_PATTERN_MODE`: `sine`, `square`, `triangle`, `sawtooth`, `random`.
- `SR_CONF_AMPLITUDE`: voltage peak amplitude, default 1.0 V.
- `SR_CONF_OFFSET`: voltage DC offset, default 0.0 V.
- `SR_CONF_PROBE_EN`: include or exclude the channel from generated packets.

The default sample rate is 1 MHz, and generated packets are bounded to a
small, fixed time slice so streaming and buffered capture remain responsive.

## Error Handling

- Invalid pattern names or non-finite amplitude/offset values are rejected.
- A packet is not emitted when no analog channels are enabled.
- Allocation or standard-packet initialization failures terminate acquisition
  through the existing demo error path.
- Replay failures retain the existing fallback/error semantics; they do not
  silently switch a selected replay capture to generated data.

## Tests

- Extend demo-driver tests to assert five analog channels and standard packet
  metadata.
- Assert per-channel waveforms stay within offset +/- amplitude and use the
  configured channel ordering.
- Assert disabled channels are omitted from the standard meaning channel list.
- Run existing analog snapshot, analog-to-logic, format integration, and full
  CTest suites.

## Scope Boundaries

- No real hardware driver changes.
- No simultaneous logic/analog MSO demo page in this change.
- No changes to PXTOOL's analog rendering or decoder conversion APIs.

## Verification

- `cmake --build build.tests --target DSView DSView-test DSView-format-integration-test -j2`: passed.
- `ctest --test-dir build.tests --output-on-failure`: passed, 2/2 tests.
- `./build.macOS/DSView-test '--run_test=upstream_demo/*analog*' --log_level=message`:
  passed three native-demo tests for five channels, standard float packets,
  waveform configuration, and channel enablement.
- `./build.macOS/DSView-format-integration-test --log_level=message`: passed.
- `bash scripts/macOS/build_and_run.sh`: passed; the app bundle was rebuilt,
  decoder resources staged, and PXTOOL launched.
- Computer Use visual check: passed for rendering. The top-left control shows
  the current `Logic Analyzer` mode with the enlarged text-and-icon affordance.
  The local macOS accessibility tree does not expose the overlaid DevMode child
  or its popup actions, so automated activation of the three menu entries could
  not be recorded; the control now has stable object/accessibility metadata and
  remains a manual UI follow-up.
