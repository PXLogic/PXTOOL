# FX2LAFW Firmware Resources Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a clear firmware resource contract and smoke-test gate for the upstream-compatible `fx2lafw` driver without committing unknown-origin firmware binaries.

**Architecture:** Keep DSView's existing firmware resource directory and CMake resource staging. Add a small manifest plus DSView-private helper APIs so tests can keep the driver profile table and resource contract in sync; update runtime diagnostics and docs so bootloader upload is explicitly gated on supplied firmware resources.

**Tech Stack:** C/C++17 style already used by DSView, CMake, GLib/GVariant/GSList, libusb, Boost.Test, existing vendored `libsigrok`, local upstream reference at `/Users/yuanji/Desktop/project/libsigrok`.

## Global Constraints

- Do not directly replace DSView's entire `libsigrok/` directory with upstream in one step.
- Do not rewrite the PXTOOL UI as part of this slice.
- Do not change DSLogic, DSCope, PXLogic, demo, file, decode, or session behavior.
- Keep `/Users/yuanji/Desktop/project/libsigrok` as a development reference source only; DSView must not depend on that path at build or runtime.
- When importing upstream libsigrok source into DSView, replace the upstream file header with the DSView/PXTOOL copyright and GPL notice style used by nearby files.
- Preserve the current in-source CMake build model for this branch.
- Keep the `fx2lafw` path disabled by default behind `DSVIEW_ENABLE_UPSTREAM_FX2LAFW`.
- Do not add unknown-origin `fx2lafw-*.fw` binary firmware files.
- Do not download firmware at build time.
- This slice must not implement acquisition start/stop, USB bulk transfer submission, trigger setup, or `ds_data_forward()` conversion.
- Devices found by this path must remain tagged `DS_DEVICE_SOURCE_UPSTREAM_COMPAT`.

---

## File Structure

- Create `PXTOOL/res/fx2lafw/README.md`: explains where licensed fx2lafw firmware files belong and lists required filenames.
- Create `PXTOOL/res/fx2lafw/manifest.txt`: plain-text manifest of expected firmware filenames, one filename per line.
- Modify `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`: expose DSView-private test helpers for profile count, profile access, and firmware subdirectory.
- Modify `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`: make `fx2lafw_firmware_path()` prefer `<DS_RES_PATH>/fx2lafw/<firmware>`, add profile enumeration helpers, and improve the missing-firmware diagnostic.
- Modify `PXTOOL/test/test_upstream_fx2lafw.cpp`: add manifest sync tests and update path tests to the subdirectory contract.
- Modify `PXTOOL/test/CMakeLists.txt`: define the source-tree manifest path for tests.
- Modify `docs/libsigrok-upstream-driver-inventory.md`: record that bootloader support is gated on externally supplied firmware files and hardware smoke testing.
- Create `docs/fx2lafw-hardware-smoke-test.md`: manual checklist for firmware-loaded and bootloader-state devices.

## Task 1: Firmware Manifest Contract

**Files:**
- Create: `PXTOOL/res/fx2lafw/README.md`
- Create: `PXTOOL/res/fx2lafw/manifest.txt`
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`
- Modify: `PXTOOL/test/test_upstream_fx2lafw.cpp`
- Modify: `PXTOOL/test/CMakeLists.txt`

**Interfaces:**
- Consumes: existing `supported_fx2[]` profile table.
- Produces:
  - `#define FX2LAFW_FIRMWARE_DIR "fx2lafw"`
  - `SR_PRIV size_t fx2lafw_profile_count(void);`
  - `SR_PRIV const struct fx2lafw_profile *fx2lafw_profile_get(size_t index);`
  - CMake test definition `FX2LAFW_MANIFEST_PATH`.

- [ ] **Step 1: Add failing manifest tests**

In `PXTOOL/test/CMakeLists.txt`, add this definition after `target_include_directories(...)`:

```cmake
if(DSVIEW_ENABLE_UPSTREAM_FX2LAFW)
    target_compile_definitions(DSView-test PRIVATE
        FX2LAFW_MANIFEST_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../res/fx2lafw/manifest.txt"
    )
endif()
```

In `PXTOOL/test/test_upstream_fx2lafw.cpp`, add these includes near the existing Boost include:

```cpp
#include <fstream>
#include <set>
#include <string>
```

Append these tests before `BOOST_AUTO_TEST_SUITE_END()`:

```cpp
BOOST_AUTO_TEST_CASE(firmware_manifest_matches_profiles)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    std::ifstream manifest(FX2LAFW_MANIFEST_PATH);
    BOOST_REQUIRE_MESSAGE(manifest.good(), FX2LAFW_MANIFEST_PATH);

    std::set<std::string> manifest_files;
    std::string line;
    while (std::getline(manifest, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        manifest_files.insert(line);
    }

    std::set<std::string> profile_files;
    for (size_t i = 0; i < fx2lafw_profile_count(); i++) {
        const fx2lafw_profile *profile = fx2lafw_profile_get(i);
        BOOST_REQUIRE(profile != nullptr);
        BOOST_REQUIRE(profile->firmware != nullptr);
        profile_files.insert(profile->firmware);
    }

    BOOST_CHECK_EQUAL_COLLECTIONS(
        profile_files.begin(), profile_files.end(),
        manifest_files.begin(), manifest_files.end());
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(firmware_manifest_documents_all_profiles)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    BOOST_CHECK_EQUAL(fx2lafw_profile_count(), 10U);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}
```

- [ ] **Step 2: Declare profile enumeration helpers and firmware dir**

In `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`, add this near the other `FX2LAFW_*` constants:

```c
#define FX2LAFW_FIRMWARE_DIR "fx2lafw"
```

Add these declarations after `fx2lafw_profile_find()`:

```c
SR_PRIV size_t fx2lafw_profile_count(void);
SR_PRIV const struct fx2lafw_profile *fx2lafw_profile_get(size_t index);
```

- [ ] **Step 3: Verify RED**

Run:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected: FAIL because `PXTOOL/res/fx2lafw/manifest.txt` does not exist and/or profile enumeration helpers are declared but not implemented.

- [ ] **Step 4: Create manifest and README**

Create `PXTOOL/res/fx2lafw/manifest.txt`:

```text
fx2lafw-cwav-usbeeax.fw
fx2lafw-cwav-usbeedx.fw
fx2lafw-cwav-usbeesx.fw
fx2lafw-cwav-usbeezx.fw
fx2lafw-saleae-logic.fw
fx2lafw-cypress-fx2.fw
fx2lafw-braintechnology-usb-lps.fw
fx2lafw-sigrok-fx2-8ch.fw
fx2lafw-sigrok-fx2-16ch.fw
fx2lafw-usb-c-grok.fw
```

Create `PXTOOL/res/fx2lafw/README.md`:

```markdown
# fx2lafw Firmware Resources

Place licensed `fx2lafw-*.fw` firmware files in this directory when enabling
bootloader-state FX2 logic analyzers.

This repository intentionally does not add unknown-origin firmware binaries.
`manifest.txt` lists the filenames referenced by DSView's
`upstream-fx2lafw` profile table. A firmware-loaded device can be opened
without these files, but a bootloader-state device needs the matching file
before DSView can upload firmware and wait for re-enumeration.
```

- [ ] **Step 5: Implement profile enumeration helpers**

Add these functions in `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c` after `fx2lafw_profile_find()`:

```c
SR_PRIV size_t fx2lafw_profile_count(void)
{
	size_t count;

	for (count = 0; supported_fx2[count].vid; count++)
		;

	return count;
}

SR_PRIV const struct fx2lafw_profile *fx2lafw_profile_get(size_t index)
{
	if (index >= fx2lafw_profile_count())
		return NULL;

	return &supported_fx2[index];
}
```

- [ ] **Step 6: Verify GREEN**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw/firmware_manifest*
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: PASS. The focused manifest tests and the full upstream fx2lafw suite pass.

- [ ] **Step 7: Commit**

```bash
git add PXTOOL/test/CMakeLists.txt PXTOOL/test/test_upstream_fx2lafw.cpp libsigrok/hardware/upstream-fx2lafw/fx2lafw.h libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
git add -f PXTOOL/res/fx2lafw/README.md PXTOOL/res/fx2lafw/manifest.txt
git commit -m "feat: add fx2lafw firmware manifest"
```

## Task 2: Runtime Firmware Resource Path

**Files:**
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`
- Modify: `PXTOOL/test/test_upstream_fx2lafw.cpp`

**Interfaces:**
- Consumes: `FX2LAFW_FIRMWARE_DIR`, `DS_RES_PATH`, `fx2lafw_firmware_path()`.
- Produces: firmware path format `<DS_RES_PATH>/fx2lafw/<firmware>`.

- [ ] **Step 1: Update failing firmware path expectations**

In `PXTOOL/test/test_upstream_fx2lafw.cpp`, update `firmware_path_joins_resource_directory` expected strings:

```cpp
BOOST_CHECK_EQUAL(path, "/tmp/dsview-fw/fx2lafw/fx2lafw-saleae-logic.fw");
```

Apply that expected value to both checks in the test.

- [ ] **Step 2: Verify RED**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw/firmware_path_joins_resource_directory
```

Expected: FAIL because `fx2lafw_firmware_path()` still returns the flat resource path.

- [ ] **Step 3: Update path construction**

Replace the path construction block in `fx2lafw_firmware_path()`:

```c
dir_len = strlen(DS_RES_PATH);
if (dir_len > 0 && DS_RES_PATH[dir_len - 1] == '/')
	*path = g_strdup_printf("%s%s", DS_RES_PATH, profile->firmware);
else
	*path = g_strdup_printf("%s/%s", DS_RES_PATH, profile->firmware);
```

with:

```c
dir_len = strlen(DS_RES_PATH);
if (dir_len > 0 && DS_RES_PATH[dir_len - 1] == '/')
	*path = g_strdup_printf("%s%s/%s", DS_RES_PATH,
		FX2LAFW_FIRMWARE_DIR, profile->firmware);
else
	*path = g_strdup_printf("%s/%s/%s", DS_RES_PATH,
		FX2LAFW_FIRMWARE_DIR, profile->firmware);
```

- [ ] **Step 4: Improve missing resource diagnostic**

In `hw_scan()`, replace:

```c
sr_err("Firmware file is not bundled: %s.", firmware);
```

with:

```c
sr_err("fx2lafw firmware resource is missing: %s.", firmware);
```

- [ ] **Step 5: Verify GREEN**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw/firmware_path_joins_resource_directory
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add PXTOOL/test/test_upstream_fx2lafw.cpp libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
git commit -m "fix: use fx2lafw firmware resource directory"
```

## Task 3: Smoke-Test Documentation and Inventory

**Files:**
- Create: `docs/fx2lafw-hardware-smoke-test.md`
- Modify: `docs/libsigrok-upstream-driver-inventory.md`

**Interfaces:**
- Consumes: current firmware resource contract from Tasks 1 and 2.
- Produces: manual smoke-test checklist for firmware-loaded and bootloader-state devices.

- [ ] **Step 1: Create smoke test document**

Create `docs/fx2lafw-hardware-smoke-test.md`:

```markdown
# fx2lafw Hardware Smoke Test

Date: 2026-07-11

## Preconditions

- Build with `DSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON`.
- Use firmware-loaded fx2lafw-compatible hardware for open/close validation.
- For bootloader-state validation, place licensed firmware files under
  `PXTOOL/res/fx2lafw/` before building or launching DSView.
- Do not proceed to acquisition testing until open/close passes.

## Firmware-Loaded Device

1. Start DSView from the build produced by:
   `script/macos/build_and_run.sh`
2. Confirm the fx2lafw-compatible device appears in the device list.
3. Activate the device.
4. Confirm open succeeds and the device remains stable.
5. Switch away from the device or close DSView.
6. Confirm close does not hang or leave the device unavailable to a second scan.

## Bootloader-State Device

1. Remove `PXTOOL/res/fx2lafw/<firmware>.fw` and start DSView.
2. Confirm scan logs the exact missing firmware path.
3. Add the licensed matching firmware file under `PXTOOL/res/fx2lafw/`.
4. Rebuild or ensure the app bundle contains `share/PXTOOL/res/fx2lafw/<firmware>.fw`.
5. Start DSView and scan again.
6. Confirm firmware upload is attempted.
7. Confirm the device re-enumerates on the same USB bus.
8. Activate the device.
9. Confirm open, firmware version validation, and close succeed.

## Stop Conditions

- Missing firmware path is wrong.
- Upload succeeds but re-enumeration opens a different same-VID/PID device.
- Open succeeds but close hangs or leaves the handle claimed.
- Any DSLogic, DSCope, PXLogic, demo, file, decode, or session behavior regresses.
```

- [ ] **Step 2: Update inventory**

In `docs/libsigrok-upstream-driver-inventory.md`, update the `Selected Slice: fx2lafw Open/Firmware Lifecycle` section so `Still deferred` includes only acquisition-related items and hardware evidence, not the resource contract. Use this text:

```markdown
Still deferred:

- adding licensed `fx2lafw-*.fw` binary files, if the project chooses to distribute them
- manual bootloader upload and re-enumeration smoke tests with real hardware
- asynchronous acquisition transfers
- trigger setup
- DSView datafeed conversion
```

- [ ] **Step 3: Verify docs and tests**

Run:

```bash
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add -f docs/fx2lafw-hardware-smoke-test.md docs/libsigrok-upstream-driver-inventory.md
git commit -m "docs: add fx2lafw firmware smoke gate"
```

## Task 4: Final Verification

**Files:**
- No new code files expected.

**Interfaces:**
- Consumes: Tasks 1 through 3.
- Produces: verified branch state.

- [ ] **Step 1: Run full verification**

Run:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test
make DSView -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected: PASS. Existing non-fatal CMake warnings are acceptable only if all commands exit 0.

- [ ] **Step 2: Confirm packaged resource staging includes the directory**

Run:

```bash
test -f build.macOS/PXTOOL.app/Contents/Resources/share/PXTOOL/res/fx2lafw/manifest.txt
```

Expected: PASS after `make DSView` because CMake copies `PXTOOL/res` into the app bundle.

- [ ] **Step 3: Commit final verification note if needed**

If no files changed, do not create a commit. If documentation needed correction during verification, commit only that correction:

```bash
git status --short
```

Expected: clean working tree.
