# fx2lafw Firmware Resources Final Fix Report

Date: 2026-07-11
Worktree: `/Users/yuanji/Desktop/project/DSView`
Branch: `upgrade-libsigrok`

## Summary

Implemented the final review fixes for the fx2lafw firmware resources slice:

- Clarified `docs/fx2lafw-hardware-smoke-test.md` so the active CMake cache is explicitly configured with `DSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON` before using `scripts/macOS/build_and_run.sh`.
- Corrected the script path from `script/macos/build_and_run.sh` to `scripts/macOS/build_and_run.sh`.
- Documented removing the stale firmware file from the active macOS app bundle path, or cleaning that bundle resource directory, during the missing-resource smoke-test phase.
- Strengthened `PXTOOL/test/test_upstream_fx2lafw.cpp` so `firmware_manifest_matches_profiles` preserves raw manifest/profile counts, rejects duplicate manifest entries, rejects duplicate profile firmware names, and still compares the unique filename sets.

## Files Changed

- `PXTOOL/test/test_upstream_fx2lafw.cpp`
- `docs/fx2lafw-hardware-smoke-test.md`

No firmware binaries were added.
No dependency on `/Users/yuanji/Desktop/project/libsigrok` was introduced.
No acquisition/start/stop/USB transfer/trigger/datafeed behavior was implemented.

## RED Evidence

Temporary setup: appended a duplicate `fx2lafw-saleae-logic.fw` line to `PXTOOL/res/fx2lafw/manifest.txt`, then restored the manifest afterward.

Command:

```sh
./build.macOS/DSView-test --run_test=upstream_fx2lafw/firmware_manifest_matches_profiles
```

Output:

```text
Running 1 test case...
/Users/yuanji/Desktop/project/DSView/PXTOOL/test/test_upstream_fx2lafw.cpp:224: error: in "upstream_fx2lafw/firmware_manifest_matches_profiles": duplicate firmware manifest entry: fx2lafw-saleae-logic.fw

*** 3 failures are detected in the test module "Master Test Suite"
/Users/yuanji/Desktop/project/DSView/PXTOOL/test/test_upstream_fx2lafw.cpp:238: error: in "upstream_fx2lafw/firmware_manifest_matches_profiles": check manifest_files.size() == profile_files.size() has failed [11 != 10]
/Users/yuanji/Desktop/project/DSView/PXTOOL/test/test_upstream_fx2lafw.cpp:239: error: in "upstream_fx2lafw/firmware_manifest_matches_profiles": check unique_manifest_files.size() == manifest_files.size() has failed [10 != 11]
```

Exit code: `201`

## GREEN Evidence

Configure command used before building:

```sh
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
```

Result: exit code `0`.

Command:

```sh
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Output:

```text
[100%] Built target DSView-test
```

Exit code: `0`

Command:

```sh
./build.macOS/DSView-test --run_test=upstream_fx2lafw/firmware_manifest_matches_profiles
```

Output:

```text
Running 1 test case...

*** No errors detected
```

Exit code: `0`

Command:

```sh
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Output:

```text
Running 11 test cases...

*** No errors detected
```

Exit code: `0`

## Self-Review

- The doc now names the real macOS script path and makes the `DSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON` cache requirement explicit before launch.
- The bootloader missing-resource phase now accounts for stale bundle resources at `build.macOS/PXTOOL.app/Contents/Resources/share/PXTOOL/res/fx2lafw/<firmware>.fw`.
- The test no longer relies only on `std::set` values. It keeps raw vectors for manifest/profile filenames, checks raw counts, and asserts uniqueness for both sources before comparing unique collections.
- The temporary duplicate manifest line used for RED was restored; final `git status` only showed the intended test and docs changes before report creation.
