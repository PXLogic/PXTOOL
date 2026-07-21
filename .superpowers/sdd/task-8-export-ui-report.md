# Task 8 Export UI Report

## Scope

Implemented export UI completion for the modern libsigrok output manifest.
The UI now exposes the final 14 export formats in this order:

```text
csv, vcd, gnuplot, srzip, analog, ascii, binary,
bits, chronovu-la8, hex, null, ols, wav, wavedrom
```

The existing four DSView formats stay first. FileBar and the main File menu
both enumerate `exportFormats()` and emit or route format IDs only.

## Implementation

- Added menu helpers and compatibility metadata in `pv::data::formatcapability`.
- Added user-option routing for `analog`, `ascii`, `bits`, `hex`, and `wav`
  through `InputOutputOptionsDlg`.
- Kept CSV `type` and srzip `filename` as internal export parameters.
- Passed the selected format ID and selected option values through
  `MainWindow` -> `StoreProgress` -> `StoreSession`.
- Cleared temporary selected options after each handoff/transaction.
- Fixed `IoOptions` so copied selected options survive
  `sr_output_options_free()` by deep-copying type signatures and enum values.
- Added `StoreSession::validateExportFormat()` and call sites before export
  destination selection/opening:
  - logic-only outputs reject analog data.
  - analog-only outputs reject logic data.
  - DSO export is rejected for every format except `null`, with a
    human-readable format description.
  - `null` accepts any active data type.

No import waveform loading was implemented.

## Tests

Fresh verification performed:

```bash
git diff --check
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=formatcapability
./build.macOS/DSView-test --run_test=io_migration_options/selected_output_options_survive_descriptor_cleanup
./build.macOS/DSView-test --run_test=io_migration_output_fixtures
./build.macOS/DSView-test '--run_test=analog_adapter_*'
QT_QPA_PLATFORM=offscreen ./build.macOS/DSView-test --report_level=short
cmake --build . --target DSView -j4
bash scripts/macOS/build_and_run.sh
```

Results:

- `git diff --check`: clean.
- Focused format/options/output/analog tests: no errors.
- Full suite: 91/91 test cases passed, 704/704 assertions passed.
- `DSView` target built successfully.
- `scripts/macOS/build_and_run.sh` completed successfully and launched the
  worktree app bundle:
  `build.macOS/PXTOOL.app/Contents/MacOS/PXTOOL`.

Build notes:

- Existing compiler warnings remain in `storeprogress` around missing
  `override` and signed/unsigned comparisons.
- CMake still reports the existing missing `WrapVulkanHeaders` warning.
- npm reports existing `allow-scripts` warnings for `esbuild` and `fsevents`.

## Review

Spec review:

- Both export menus enumerate the final 14-format manifest.
- Existing four formats remain first.
- FileBar emits format IDs only.
- MainWindow owns option dialogs and export start.
- StoreProgress and StoreSession receive the selected ID/options.
- Compatibility validation happens before `MakeExportFile(true)` and before
  the export file is opened/truncated.
- Import waveform loading remains out of scope.

Code-quality review:

- No upstream source files were imported in this task, so no header rewrite was
  needed.
- Selected option values are isolated by format ID to avoid leaking options
  across formats.
- The compatibility rules are centralized in `formatcapability`, while
  StoreSession keeps the snapshot/device-specific analog validation.

## Manual UI Expectations

- File toolbar menu and top File menu should show `Export Data...`, then:
  CSV, VCD, Gnuplot, srzip, analog, ASCII, binary, bits, ChronoVu LA8, hex,
  null, OLS, WAV, and WaveDrom.
- ASCII, bits, hex, analog, and WAV should open the options dialog first.
- CSV, VCD, Gnuplot, srzip, binary, ChronoVu LA8, null, OLS, and WaveDrom
  should proceed directly to the export dialog/progress path.
- `.dsl` Open/Save remains unchanged.
