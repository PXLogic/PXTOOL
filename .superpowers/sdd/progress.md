Preflight baseline: complete (commit 86bc970, review clean, controller verified `./build.macOS/DSView-test --run_test=formatcapability` exit 0)
Task 1: complete (commit b9fcb4d, review approved; minor: future Boost suite names differ from the shared `io_migration` name in the plan)
Task 2A: complete (commits 8ab5ee2..26f8b19, review clean)
Task 2B: complete (commits 26f8b19..1a5e0ef, review clean)
Task 3: complete (commit 83b700a, review clean)
Task 4: complete (commits 9fc1dc5 and 0157fbe, lifecycle and
focused regression tests green; binary fixture intentionally RED until Task 6;
StoreSession cancellation cleanup has no direct DSView-test coverage)
Task 5: complete (format capabilities now expose real module extensions and
option availability; Task 6/7 module-extension and final-manifest tests remain
intentionally RED until their modules are registered)
Task 6: complete (registered current upstream logic outputs: ascii,
binary, bits, chronovu-la8, hex, ols, and wavedrom; their fixture and
capability tests are green. The final manifest is intentionally 12/14 until
Task 7 imports analog and wav. Follow-up fixed StoreSession header forwarding
for ChronoVu LA8 exports.)

Task 5: complete (commit 17308b8, review clean)
Task 6: complete (commits af4fa6c..ad9b380, review clean)
Task 7: complete (standard AnalogPacket owner and unsigned 8-bit conversion
boundary added; upstream analog/wav outputs registered; final export manifest
is 14/14; focused fixtures, full 85-test suite, and DSView build are green.
StoreSession rejects unsupported standard analog exports before file creation.)

Task 7: complete (commits 3104378..83e69c8, review clean)
Task 8: complete (export UI now routes all 14 upstream-compatible output
formats in final order, optioned formats use InputOutputOptionsDlg, selected
options are carried into StoreSession, compatibility is validated before
destination selection/opening, full DSView-test suite and macOS build/run are
green; manual menu popup screenshot remains a human verification item.)
