# Task 6 Report: Upstream Logic Output Formats

## Status

Complete. Source commit: `feat: add upstream logic output formats`.

## Delivered

- Directly imported the current upstream `ascii`, `binary`, `bits`,
  `chronovu-la8`, `hex`, `ols`, and `wavedrom` output modules.
- Replaced every imported source header with the DSView/PXTOOL GPL header
  required by `AGENTS.md`.
- Preserved upstream module IDs, descriptions, extensions, options, packet
  lifecycle, and format algorithms. The only source adaptations are existing
  DSView core API boundaries: the channel-group argument to `sr_config_get()`,
  `PACKAGE_VERSION` for the absent upstream version accessor, and inclusion of
  the DSView log macro header for ChronoVu warnings.
- Registered the seven modules immediately after `srzip` in the requested
  order, while retaining the existing CSV, VCD, gnuplot, srzip, and null
  modules.
- Added every new source to both the production `libsigrok_SOURCES` list and
  the `DSView-test` target.
- Added a table-driven four-channel fixture covering module discovery, module
  extensions, output file creation, binary NUL preservation, required output
  prefixes, and end-of-stream rendering for `ols` and `wavedrom`.

## TDD Evidence

Before the output sources and registrations were added, the focused test was
observed RED with the fully qualified Boost path:

```text
./build.macOS/DSView-test \
  --run_test=io_migration_output_fixtures/exports_logic_formats

fatal error: Missing output module: ascii
```

After the direct imports and the minimal API adaptations, both
`exports_logic_formats` and the pre-existing
`binary_output_preserves_nul_bytes` fixture are GREEN.

## Verification

Passed:

```text
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=io_migration_output_fixtures
./build.macOS/DSView-test --run_test=formatcapability
cmake --build . --target DSView
git diff --check
```

The full `DSView-test` execution ran 78 cases and has one intentional failure:

```text
enumerates_final_export_format_manifest
actual.size() == expected.size() failed: 12 != 14
```

The registered/ranked list contains every Task 6 logic output. The two absent
entries are exactly `analog` and `wav`, which belong to Task 7; no input
loading or analog export behavior was added here.

## Review

The source comparison against `/Users/yuanji/Desktop/project/libsigrok/src/output`
shows each imported module differs only in its required DSView header and the
three API-boundary adjustments documented above. Registration order, module
extensions, and production/test source-list parity were checked directly.
No legacy output framework or old/new compatibility wrapper was introduced.

## Follow-up: StoreSession Header Packet

Review found that `StoreSession::export_exec()` started standard output streams
at `SR_DF_META`, so output modules that render bytes from `SR_DF_HEADER` did
not receive their initial packet during real exports. ChronoVu LA8 depends on
that packet for its one-byte samplerate divcount header.

Added a focused ChronoVu fixture using a 50 MHz test samplerate. The fixture was
observed RED before the fix:

```text
./build.macOS/DSView-test \
  --run_test=io_migration_output_fixtures/chronovu_output_requires_header_and_preserves_logic_bytes

critical check exported.size() == 1 + 4 + source.size() failed [8 != 9]
```

The fix sends `SR_DF_HEADER` through `sr_output_send()` and writes any returned
bytes before metadata, logic data, and `SR_DF_END`. The ChronoVu output module
algorithm was not changed. The fixture now asserts that 50 MHz emits divcount
`0x01`, and that the original logic bytes survive in order after the trigger
record.

Follow-up verification passed:

```text
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=io_migration_output_fixtures
./build.macOS/DSView-test --run_test=formatcapability
cmake --build . --target DSView
```
