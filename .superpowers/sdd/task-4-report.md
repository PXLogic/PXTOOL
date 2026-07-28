# Task 4 Report: MSO selector, view behavior, and persistence

## Implemented

- Added `DevMode::display_name_for_mode(int)` with stable labels for Logic,
  Analog, DSO, and MSO. The selector now uses that single source for both the
  popup and the current mode label. MSO keeps the oscilloscope icon.
- Added the `mso_selector_name_is_available` unit test.
- Added `View::is_logic_rendering_mode()` and applied it to logic-oriented
  interactions: cursor selection/rendering, logic edge snapping and measuring,
  logic zoom, row resizing/reordering, signal-height presets, RLE reporting,
  and realtime auto-scale/auto-scroll.
- Preserved AnalogSignal traces in the MSO time-trace layout. Analog signals
  continue through their analog scale path; DSO-only paths remain guarded by
  `mode == DSO`.
- Extended session saving to accept the exact MSO pair (logic + analog), write
  both chunk families into one archive, and preserve typed probe metadata.
  Existing single-type and DSO save paths retain their formats.
- Extended the virtual session loader to rebuild typed MSO logic and analog
  channels from the new metadata, allowing a restored session to retain both
  channel kinds.

## Verification

- `cmake --build build.tests --target DSView-test DSView DSView-format-integration-test -j2`
  passed.
- `build.macOS/DSView-test` passed: 159 test cases.
- `build.macOS/DSView-format-integration-test` passed: 9 test cases.
- `git diff --check` passed.

## Notes

- The build emits pre-existing compiler warnings in unrelated UI and legacy
  libsigrok code; no new build errors were introduced.
- Automated coverage verifies the selector label and existing MSO mode
  predicates. The format integration target exercises the real persistence
  runtime, but an end-to-end captured MSO fixture was not available in its
  current test harness; manual GUI validation should capture and reopen an
  MSO recording before release.
