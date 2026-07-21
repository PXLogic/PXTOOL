# Task 7 Report: Standard Analog Outputs

## Status

Complete. Intended commit: `feat: add standard analog export support`.

## Delivered

- Added `pv::data::AnalogPacket`, which owns float samples, standard analog
  encoding/meaning/spec metadata, samplerate metadata, GSList nodes, and any
  synthesized channels. Copying is disabled and moves rebind every internal
  pointer. Borrowed device channels retain pointer identity for upstream WAV
  channel mapping and remain valid for each synchronous module send.
- Added strict validation for empty channels, zero samples/samplerate,
  non-interleaved sample counts, oversized sample counts, duplicate/invalid
  channels, and invalid unsigned 8-bit conversion parameters.
- Added the unsigned 8-bit DSView snapshot conversion boundary. It selects
  enabled channels in device order and converts raw values to interleaved
  floats with the same zero-offset/range formula used by DSView's analog CSV
  path.
- Directly imported upstream `output/analog.c` and `output/wav.c`, replaced
  their headers with the DSView/PXTOOL GPL notice, and registered both after
  srzip and before the logic additions. Analog is extensionless; WAV declares
  `.wav`.
- Added the direct key-info and little-endian write helpers required by those
  upstream modules. The analog algorithm is byte-for-byte upstream after its
  header. WAV differs only for DSView logging and the existing local
  `sr_config_get()` channel-group argument.
- Added production and test build sources. The ranked capability result is
  now the required 14-format order.
- Connected eligible 8-bit AnalogSnapshot exports to standard float packets.
  Standard analog outputs receive HEADER, samplerate META, ANALOG, and END.
  Existing CSV and other legacy analog paths are unchanged.

## Conservative Rejection

`StoreSession::export_start()` rejects `analog` and `wav` before starting the
export thread or opening/truncating the destination when any of these apply:

- The active snapshot is logic or DSO rather than analog.
- The analog snapshot is not 8-bit.
- Samplerate or reference range is unavailable.
- No enabled analog channel exists.
- An enabled channel lacks captured data, a source-channel mapping, a finite
  voltage range, or the `V` unit.
- WAV samplerate/channel count cannot fit its 32-bit byte-rate or 16-bit
  block-align fields.

No waveform import loading was implemented.

## TDD Evidence

The first production build was observed RED because
`analogpacketadapter.h` did not exist. After the owner and modules were added,
the focused fixtures became GREEN. A second RED cycle added the raw-sample
conversion test; the build failed because
`convertUnsigned8AnalogSamples()` did not exist, then passed after the
conversion boundary was implemented.

The fixtures cover packet pointer ownership and move rebinding, channel order
and identity, samplerate metadata, invalid counts, raw-value conversion,
module discovery, options, extension policy, binary-safe temporary file
writes, analog text output, and WAV RIFF/WAVE structure.

## Verification

All commands exited 0 on 2026-07-20:

```text
cmake --build . --target DSView-test -j4
./build.macOS/DSView-test '--run_test=analog_adapter_*'
./build.macOS/DSView-test --run_test=io_migration_output_fixtures
./build.macOS/DSView-test --run_test=enumerates_final_export_format_manifest
./build.macOS/DSView-test --run_test=formatcapability
./build.macOS/DSView-test
cmake --build . --target DSView -j4
git diff --check
```

Results: 5 focused adapter cases, 9 output fixture cases, the final 14-format
manifest, 8 capability cases, and all 85 DSView-test cases passed. Both
targets built successfully. Production compilation still reports existing
legacy/Qt warnings; no warning became an error.

## Review Notes

- Source comparison confirmed upstream format algorithms were preserved.
- WAV packet channels are the same device-channel pointers collected during
  module initialization; the fixture exercises this identity requirement.
- Unsupported DSO and non-convertible analog snapshots cannot reach ordinary
  file creation.
- `DSView-test` does not link the complete StoreSession/SigSession/snapshot
  graph, so the no-file rejection behavior is build- and path-reviewed here;
  the approved plan retains a direct StoreSession failure fixture for Task 11.

## Follow-up Review Fix: WAV Option Lifetime

Commit: `fix: reset wav output default option after cleanup`.

Review finding addressed on 2026-07-21:

- `libsigrok/output/wav.c` now mirrors `analog.c`: cleanup only unrefs the
  module-global `options[0].def` when present and immediately sets it to
  `NULL`. Repeated WAV output creation, options queries, and exports no longer
  reuse a freed `GVariant`.
- Added `wav_output_recreates_options_after_cleanup`. The RED run aborted in
  GLib's GVariant type-info check after freeing one WAV output and querying WAV
  options again. After the cleanup fix, the regression passes and performs a
  second WAV export.
- Strengthened the WAV fixture to verify RIFF/WAVE structure, `fmt ` chunk
  size, IEEE-float format code `3`, channel count, sample rate, byte rate,
  block alignment, bits per sample, extension size, `data` chunk marker/size,
  and the emitted interleaved float payload.
- The pre-file-open StoreSession rejection gap remains documented above; this
  follow-up stays scoped to the reviewed module-global option lifetime and WAV
  fixture coverage.

Verification commands exited 0:

```text
cmake --build . --target DSView-test -j4
./build.macOS/DSView-test --run_test=io_migration_output_fixtures/wav_output_recreates_options_after_cleanup
./build.macOS/DSView-test --run_test=io_migration_output_fixtures
./build.macOS/DSView-test '--run_test=analog_adapter_*'
./build.macOS/DSView-test --run_test=enumerates_final_export_format_manifest
./build.macOS/DSView-test --run_test=formatcapability
./build.macOS/DSView-test
cmake --build . --target DSView -j4
```

Results: 1 focused WAV lifetime regression, 10 output fixture cases, 5 analog
adapter cases, the final manifest, 8 format capability cases, and all 86
DSView-test cases passed. The DSView target built successfully.
