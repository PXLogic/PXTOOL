# libsigrok Import/Export Format Support Design

## Goal

DSView should keep its current native `*.dsl` open/save behavior while adding PulseView-style import and export menus backed by libsigrok input and output modules. The user-visible target is a toolbar drop-down similar to PulseView: native DSView actions first, then `Import ...` or `Export ...` entries for supported formats.

## Current State

The `main` branch and the current `upgrade-libsigrok` branch both register the same DSView-local libsigrok formats:

- Input modules: `vcd`, `wav`, and fallback `binary`.
- Output modules: `csv`, `vcd`, `gnuplot`, and `srzip`.

The current branch has not yet synced the broader upstream libsigrok input/output module set into DSView. DSView also does not currently expose a general libsigrok import menu: `PXTOOL/pv/toolbars/filebar.cpp` opens only `DSView Data (*.dsl)`, and `ds_device_from_file()` delegates to `sr_new_virtual_device()`, which parses DSView session zip/header data rather than generic libsigrok input modules.

PulseView is different: it uses `context->input_formats()` and `context->output_formats()` from libsigrokcxx to populate its menus. The PulseView source tree does not carry the format implementation itself; the supported format list comes from the linked libsigrok runtime.

## Scope

This work covers the DSView user workflow and the DSView-local libsigrok integration needed for it:

- Add PulseView-style open/save drop-down entries for import/export formats.
- Preserve existing `*.dsl`, session setup, save, save-as, and selected-range workflows.
- Add an internal DSView format capability layer so UI code does not directly own format enumeration, filtering, or file dialog filter generation.
- Add a libsigrok import bridge for non-`*.dsl` files.
- Keep the first implementation slice focused on formats already present in this repository, then add upstream modules in follow-up slices.

This work does not replace DSView's existing native session file format, nor does it attempt to make every upstream libsigrok format fully configurable in the first slice.

## Architecture

Introduce a small DSView-owned format capability layer, tentatively under `PXTOOL/pv/data/` or an equivalent nearby location following local conventions. It will expose:

- Native open/save descriptors for DSView-owned actions such as `*.dsl` and session setup.
- Import format descriptors derived from `sr_input_list()`.
- Export format descriptors derived from `sr_output_list()`.
- Filtering helpers for the active work mode and channel data types.
- File dialog filter strings and stable menu labels.

The UI layer will consume descriptors and emit selected native actions or selected libsigrok format IDs. The data layer will route those IDs to the existing native path or the new libsigrok import/export bridges.

## Import Design

Native `*.dsl` files continue to use the current path:

1. `FileBar` or `MainWindow` selects a DSView data file.
2. `ds_device_from_file()` creates a virtual DSView file device.
3. `SigSession` switches to that device.

Generic files use a new path:

1. The user selects a specific `sr_input_format` menu entry, or chooses a file from a combined import dialog.
2. DSView resolves the format by ID, validates that the selected file matches or is intentionally accepted by that module.
3. DSView calls the module's input entry points and collects the produced libsigrok datafeed packets.
4. DSView converts logic and analog packets into a DSView-displayable file-backed or memory-backed session representation.
5. DSView switches the active session to the imported data.

The first implementation slice should support only the formats already present in this repository: `binary`, `vcd`, and `wav`. If a format requires options and DSView does not yet have a parameter dialog for it, the bridge uses the module defaults only when those defaults are safe and predictable. Otherwise the descriptor hides the format until its option UI exists.

## Export Design

Export remains based on libsigrok output modules, but through the new capability layer:

1. The save button drop-down keeps native save actions first.
2. The menu then lists export descriptors derived from `sr_output_list()`.
3. Descriptors are filtered by active data type. Logic-only formats appear for logic sessions. Analog-capable formats appear only when the output module and DSView packet generation support them.
4. Existing StoreSession packet generation sends data to the selected `sr_output_module`.

The first implementation slice should preserve the current working set: `csv`, `vcd`, `gnuplot`, and `srzip`. Extra upstream output modules can be added after a compile and fixture-backed verification pass.

## UI Design

The open button menu should follow this order:

1. `Open...`
2. `Open Session Setup From File...`
3. Separator.
4. `Import <format description>...` entries.

The save button menu should follow this order:

1. `Save...`
2. `Save As...`
3. `Save Selected Range As...`
4. `Save Session Setup...`
5. Separator.
6. `Export <format description>...` entries.

Menu entries should be generated from descriptors so new verified formats appear without custom UI code. Translation strings should follow existing DSView translation patterns.

## Upstream Module Sync

After the first slice validates the architecture, sync upstream input/output modules in small groups.

Candidate input modules from upstream libsigrok:

- `binary`
- `chronovu-la8`
- `csv`
- `logicport`
- `null`
- `protocoldata`
- `raw_analog`
- `saleae`
- `stf`
- `trace32_ad`
- `vcd`
- `wav`
- `isf`

Candidate output modules from upstream libsigrok:

- `analog`
- `ascii`
- `binary`
- `bits`
- `chronovu-la8`
- `csv`
- `hex`
- `null`
- `ols`
- `vcd`
- `wav`
- `wavedrom`
- DSView-local or already present: `gnuplot`, `srzip`

When importing upstream libsigrok source into DSView, replace the upstream file header with the DSView/PXTOOL copyright and GPL notice style used by nearby files.

## Error Handling

If a file cannot be recognized, DSView should show a concise error such as `Unsupported or unrecognized file format`.

If a selected format cannot import the chosen file, DSView should keep the current session unchanged and report the failing format description.

If an output module is not compatible with the active data type, it should not be shown in the menu.

If an output module fails while writing, DSView should keep the current error path used by `StoreSession`, but include the output format ID in logs.

## Testing

Add focused tests for:

- Format descriptor enumeration from current `sr_input_list()` and `sr_output_list()`.
- Open menu descriptor ordering: native actions, separator, import entries.
- Save menu descriptor ordering: native actions, separator, export entries.
- `*.dsl` routing continues to use the existing native path.
- `vcd`, `wav`, and `binary` import descriptors resolve by ID.
- Existing `csv`, `vcd`, `gnuplot`, and `srzip` export descriptors resolve by ID.
- A CMake or unit-level sanity check that every registered module has a compiled source file.

Fixture import/export tests should be added per format as the bridge and upstream modules are enabled.

## Implementation Slices

1. Add the format capability layer and tests for descriptor enumeration and filtering.
2. Update file toolbar menus to use descriptors while preserving existing native actions.
3. Add the import bridge for current in-repo input modules.
4. Route current export UI through descriptors and preserve current output behavior.
5. Sync additional upstream output modules in small groups.
6. Sync additional upstream input modules in small groups.
7. Add option dialogs for formats whose defaults are not sufficient.

## Acceptance Criteria

- DSView still opens existing `*.dsl` files through the current native path.
- The open button drop-down lists native actions followed by verified import formats.
- The save button drop-down lists native actions followed by verified export formats.
- The first slice exposes and verifies current in-repo formats without claiming unverified upstream formats.
- Additional upstream formats are added only when they compile, are registered, and have at least a minimal verification path.
- Unsupported formats are hidden or rejected before they can corrupt or replace the current session.
