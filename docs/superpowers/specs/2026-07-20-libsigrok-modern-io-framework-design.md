# Modern libsigrok Input/Output Framework Migration Design

## Status

This design supersedes the implementation direction in
`2026-07-20-libsigrok-import-export-design.md`. The already delivered menu
enumeration remains useful, but generic file loading is deferred until every
target export format is available and verified.

## Goal

Move DSView's libsigrok input and output integration to the current upstream
framework model while preserving DSView-native `*.dsl` behavior. First provide
all PulseView-visible export formats plus DSView's existing export formats.
Only after export is complete and verified will DSView load external input
formats into waveforms. The migrated analog data model must also be usable by
future ADC acquisition and display work.

## Scope and Ordering

This project has four ordered delivery stages. A later stage must not begin
until the prior stage's automated and manual verification passes.

1. Migrate the input/output framework and its required shared data model to
   the current upstream contracts. Keep the visible import behavior unchanged
   in this stage.
2. Complete and verify the export feature. Preserve current DSView export
   formats, then add every PulseView-visible upstream output module.
3. Connect the migrated input framework to DSView's waveform session for the
   three formats already shown by the menu: VCD, WAV, and raw binary.
4. Build ADC acquisition and display on the standard analog datafeed model.
   ADC hardware implementation is explicitly outside this project's code
   scope, but the framework migration must leave a documented, tested packet
   boundary for it.

The migration targets the input/output portion of libsigrok and its direct
data-model dependencies. It does not replace DSView's hardware drivers,
DSView-native session archive format, device management, or DSView-specific
DSO packet behavior with upstream equivalents.

## Current Baseline

DSView currently compiles a forked, older output ABI. `StoreSession` manually
constructs `struct sr_output`, calls a module's `init` and `receive` callbacks
directly, and writes all results through a text stream. It registers four
formats:

- `csv`
- `vcd`
- `gnuplot`
- `srzip`

The local input ABI exposes `init(filename)` and `loadfile(filename)`, with
registered modules `vcd`, `wav`, and `binary`. The current menu lists them,
but non-DSL import is intentionally a placeholder and does not replace the
active session.

The upstream libsigrok checkout at
`/Users/yuanji/Desktop/project/libsigrok` uses the current streaming
input/output model:

- Input: create an input instance with typed options, submit byte buffers, and
  finish the input stream.
- Output: create an output instance with `sr_output_new`, submit datafeed
  packets with `sr_output_send`, and release it with `sr_output_free`.
- Options: module-defined `sr_option` descriptors with typed default values
  and optional enumerations.
- Analog packets: data, encoding, meaning, enabled channels, measurement
  quantity, and unit are carried together rather than inferred from a
  DSView-specific raw buffer.

## Architecture

### Upstream-Compatible I/O Layer

Import current upstream input/output framework sources and their required
direct helper APIs into DSView's `libsigrok` tree. Preserve the upstream
public contracts for input and output callers inside DSView. Do not retain a
second permanent old/new I/O API.

Where DSView-specific types are required, place narrowly scoped adapters at
the DSView boundary rather than changing imported module behavior. Every
imported upstream source file must use the DSView/PXTOOL copyright and GPL
header style required by `AGENTS.md`.

The build must compile only one registration list for input modules and one
registration list for output modules. CMake source lists, registration lists,
and the format capability enumeration are treated as one contract and tested
together.

### Output Adapter

Replace manual callback invocation in `StoreSession` with the standard output
lifecycle:

1. Resolve the selected `sr_output_module`.
2. Collect validated typed options.
3. Create an instance with `sr_output_new`.
4. Write each returned `GString` with a binary-safe `QFile` write.
5. Send metadata and logic, analog, or DSO-adapted packets with
   `sr_output_send`.
6. Send the final end packet, then free the instance with `sr_output_free`
   on every success, cancellation, and error path.

The export adapter owns file creation and cleanup. Modules that use internal
I/O, such as `srzip`, retain their module-specific behavior. Ordinary output
modules never receive a text stream.

Export file filters and default suffixes use each module's declared extension
list, not the module ID. A module with no declared extension uses an
extensionless save choice and must not silently append its ID as a suffix.

### Format Availability and Menu Order

The format capability layer derives menus exclusively from compiled
`sr_output_list()` and `sr_input_list()` registrations. It provides each
format's ID, description, extension list, options, and export data
compatibility.

The Export menu order is fixed:

1. Existing DSView formats: CSV, VCD, Gnuplot, srzip.
2. Upstream additions: ASCII analog, ASCII art, raw binary, 0/1 digits,
   ChronoVu LA8, hexadecimal digits, null output, OpenBench Logic Sniffer,
   WAV, and WaveDrom.

The menu must not advertise a format that is not compiled and registered.
The original `Export Data...` action remains available as the general export
dialog. Selecting a format shortcut preselects that output format in the
same export flow.

`*.dsl` remains a native DSView `Save...` format and is never represented as
an `sr_output_module`.

### Unified Options Dialog

Add one reusable DSView dialog for input and output module options. It
renders current `sr_option` descriptors:

- Boolean values use a checkbox.
- Signed and unsigned integer values use range-checked numeric inputs.
- Floating-point values use decimal numeric inputs.
- Strings use text inputs.
- Options with an allowed value list use a combo box.

The dialog initializes each control from the module default and returns a
typed option map. It is shown only when the selected module declares options.
DSView-owned internal options, such as the snapshot data type used by the
export adapter, are not shown.

In stage 2 the dialog is used for export modules. In stage 3 the same dialog
is used by input modules, including raw binary's channel-count and sample-rate
options.

### Standard Analog Boundary

The migration introduces a DSView adapter that creates a standard
`sr_datafeed_analog` packet for data entering or leaving the generic I/O
layer. The adapter supplies:

- The ordered enabled analog channels.
- A defined numeric encoding for the sample buffer.
- Measurement quantity and unit when the capture provides them.
- Sampling-rate metadata.

Existing DSView DSO packets remain supported by the live-view code. For
generic output, DSO data is either converted by the adapter to standard analog
packets where the selected module supports analog data, or rejected before
file creation with a clear compatibility error. No output module may interpret
a DSView DSO payload as an upstream analog payload without an explicit
adapter.

This boundary is the future ADC integration contract: an ADC acquisition
driver must emit the same standard analog packet and metadata shape consumed
by the session, display, and file I/O paths.

## Export Formats

The target registered output IDs after stage 2 are:

- Existing DSView: `csv`, `vcd`, `gnuplot`, `srzip`.
- Upstream: `analog`, `ascii`, `binary`, `bits`, `chronovu-la8`, `hex`,
  `null`, `ols`, `wav`, `wavedrom`.

Compatibility rules:

- Logic: `ascii`, `binary`, `bits`, `chronovu-la8`, `hex`, `ols`, `vcd`,
  `wavedrom`, and the logic mode of CSV.
- Analog: `analog`, `wav`, and the analog mode of CSV.
- `null`: accepts any supported data type, completes successfully, and
  intentionally writes no data payload.
- `gnuplot` and `srzip`: preserve their existing DSView behavior and remain
  available in the menu.

If a chosen format cannot represent the active session data, DSView must show
the format description and a precise reason before creating or truncating the
destination file.

## Input Staging

No external file is loaded into the waveform session in stages 1 or 2. The
current `Open...` and `Import > Open DSView Data...` paths continue to use
the native DSL loader.

Stage 3 enables only the existing visible import formats:

- VCD: logic channels and source samplerate.
- WAV: analog channels, source samplerate, and standard analog packet
  metadata.
- Raw binary: logic channels and sample rate supplied through the unified
  options dialog.

Import failure must leave the current DSView session unchanged. The input
adapter must create and validate the input device before switching the active
session, then stream data into the existing datafeed handling path.

## Error Handling and Resource Ownership

- Invalid options are rejected before input or output instance creation.
- Failure to create a module instance leaves the current session and existing
  destination file unchanged.
- On export cancellation or error, output instances are freed and partially
  written ordinary files are removed unless the user explicitly selected an
  overwrite-preserving behavior already used by DSView.
- Format IDs and source paths are logged for diagnostics; user messages use
  the human-readable format description.
- Internal-I/O modules manage their own files but still report errors through
  the common export result path.

## Verification Strategy

### Registration and Build Contract

Add automated tests that compare the expected format manifest with:

1. The input/output registration lists.
2. The CMake compilation source lists.
3. The runtime result of `sr_input_list()` and `sr_output_list()`.
4. The menu capability ordering.

The test fails if a registered module is absent from the build, a compiled
module is not registered, or the menu exposes an unregistered ID.

### Output Fixtures

Use one deterministic logic fixture and one deterministic analog fixture.
For every target output format:

- Verify output instance creation with default options.
- Export into a temporary directory.
- Verify the expected extension or extensionless policy.
- Verify the file is non-empty except for `null`.
- For text outputs, verify stable identifying header/content.
- For binary outputs, verify a documented signature, fixed header field, or
  minimum structural length.
- Verify cleanup after cancellation and after a module-reported error.

Run baseline fixture checks for CSV, VCD, Gnuplot, and srzip before and after
the framework migration to detect regressions.

### Input Fixtures

When stage 3 begins, use VCD, WAV, and raw binary fixtures. Assert emitted
datafeed type, enabled channel count, samplerate metadata, and sample count.
Exported VCD, WAV, and raw binary fixtures are then re-imported to provide
round-trip coverage.

### Analog Contract Tests

Create test packets with multiple analog channels, a samplerate, explicit
encoding, quantity, and unit. Verify that:

- The adapter preserves channel order and sample count.
- The analog ASCII and WAV outputs accept the packet.
- Invalid DSO-to-analog adaptation is rejected without writing a file.

### End-to-End and Manual Checks

For each stage:

- Build `DSView-test`.
- Run focused I/O tests.
- Build the `DSView` target.
- Run `scripts/macOS/build_and_run.sh`.
- Verify menus in both the main File menu and the toolbar File menu.
- Verify native `.dsl` save/open and a hardware/virtual-device session still
  start normally.

## Acceptance Criteria

- DSView has one current upstream-compatible input/output framework, not a
  permanent dual API.
- Existing native DSL open/save behavior remains unchanged.
- All fourteen target output IDs are compiled, registered, visible in the
  required order, and fixture-verified.
- Every ordinary output format writes through a binary-safe file path.
- Export options are shown from module descriptors without per-format UI
  dialogs.
- Generic import remains disabled until all export acceptance criteria pass.
- VCD, WAV, and raw binary import then load valid files into a waveform
  session using the migrated streaming input API.
- Standard analog packets are verified independently of ADC hardware and are
  suitable as the ADC integration boundary.
