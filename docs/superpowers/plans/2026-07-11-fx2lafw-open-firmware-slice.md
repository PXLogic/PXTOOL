# FX2LAFW Open Firmware Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the build-gated `upstream-fx2lafw` driver from scan-only discovery to safe firmware-aware open/close.

**Architecture:** Keep the current DSView CMake build and the existing one-file `upstream-fx2lafw` driver. Add small testable helpers for firmware path construction, firmware-state tracking, USB open/version checks, and handle cleanup; leave acquisition and async transfers for a later slice.

**Tech Stack:** C/C++17 style already used by DSView, CMake, GLib/GVariant/GSList, libusb, Boost.Test, existing vendored `libsigrok`, local upstream reference at `/Users/yuanji/Desktop/project/libsigrok`.

## Global Constraints

- Do not directly replace DSView's entire `libsigrok/` directory with upstream in one step.
- Do not rewrite the PXTOOL UI as part of this slice.
- Do not change DSLogic, DSCope, PXLogic, demo, file, decode, or session behavior.
- Keep `/Users/yuanji/Desktop/project/libsigrok` as a development reference source only; DSView must not depend on that path at build or runtime.
- When importing upstream libsigrok source into DSView, replace the upstream file header with the DSView/PXTOOL copyright and GPL notice style used by nearby files.
- Preserve the current in-source CMake build model for this branch.
- Keep the `fx2lafw` path disabled by default behind `DSVIEW_ENABLE_UPSTREAM_FX2LAFW`.
- This slice must not implement acquisition start/stop, USB bulk transfer submission, trigger setup, or `ds_data_forward()` conversion.
- Devices found by this path must remain tagged `DS_DEVICE_SOURCE_UPSTREAM_COMPAT`.

---

## File Structure

- Modify `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`: add DSView-private constants and helper declarations used by tests.
- Modify `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`: add firmware path helper, firmware-state detection, firmware upload, open/close, and firmware version command.
- Modify `PXTOOL/test/test_upstream_fx2lafw.cpp`: add hardware-independent tests for firmware path joining, missing firmware resource path, close-without-open behavior, and lifecycle wiring.
- Modify `PXTOOL/test/CMakeLists.txt`: compile `hardware/common/ezusb.c` into `DSView-test` when `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` is enabled.
- Modify `docs/libsigrok-upstream-driver-inventory.md`: record that the next fx2lafw slice is open/firmware lifecycle, not acquisition.

## Task 1: Firmware Path Helper

**Files:**
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`
- Modify: `PXTOOL/test/test_upstream_fx2lafw.cpp`
- Modify: `PXTOOL/test/CMakeLists.txt`

**Interfaces:**
- Consumes: `DS_RES_PATH`, `struct fx2lafw_profile::firmware`, GLib allocation.
- Produces:
  - `SR_PRIV int fx2lafw_firmware_path(const struct fx2lafw_profile *profile, char **path);`

- [ ] **Step 1: Add failing firmware path tests**

Add this test-only definition after the `extern "C"` include block in `PXTOOL/test/test_upstream_fx2lafw.cpp`. `DSView-test` does not link `lib_main.c`, while the real application already gets this symbol from `lib_main.c`:

```cpp
#ifdef HAVE_UPSTREAM_FX2LAFW
extern "C" {
char DS_RES_PATH[500];
}
#endif
```

Append these test cases before `BOOST_AUTO_TEST_SUITE_END()`:

```cpp
BOOST_AUTO_TEST_CASE(firmware_path_requires_resource_directory)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    DS_RES_PATH[0] = '\0';
    char *path = nullptr;
    BOOST_CHECK_EQUAL(fx2lafw_firmware_path(profile, &path),
        SR_ERR_FIRMWARE_NOT_EXIST);
    BOOST_CHECK(path == nullptr);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(firmware_path_joins_resource_directory)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    g_strlcpy(DS_RES_PATH, "/tmp/dsview-fw", sizeof(DS_RES_PATH));
    char *path = nullptr;
    BOOST_REQUIRE_EQUAL(fx2lafw_firmware_path(profile, &path), SR_OK);
    BOOST_REQUIRE(path != nullptr);
    BOOST_CHECK_EQUAL(path, "/tmp/dsview-fw/fx2lafw-saleae-logic.fw");
    g_free(path);

    g_strlcpy(DS_RES_PATH, "/tmp/dsview-fw/", sizeof(DS_RES_PATH));
    path = nullptr;
    BOOST_REQUIRE_EQUAL(fx2lafw_firmware_path(profile, &path), SR_OK);
    BOOST_REQUIRE(path != nullptr);
    BOOST_CHECK_EQUAL(path, "/tmp/dsview-fw/fx2lafw-saleae-logic.fw");
    g_free(path);
    DS_RES_PATH[0] = '\0';
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}
```

- [ ] **Step 2: Declare the helper**

Add this declaration to `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h` after `fx2lafw_profile_channel_count()`:

```c
SR_PRIV int fx2lafw_firmware_path(const struct fx2lafw_profile *profile,
	char **path);
```

- [ ] **Step 3: Verify RED**

Run:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected: FAIL at link time because `fx2lafw_firmware_path` is declared but not implemented.

- [ ] **Step 4: Implement the helper**

Add this function in `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c` after `fx2lafw_profile_channel_count()`:

```c
SR_PRIV int fx2lafw_firmware_path(const struct fx2lafw_profile *profile,
	char **path)
{
	size_t dir_len;

	if (!path)
		return SR_ERR_ARG;
	*path = NULL;

	if (!profile || !profile->firmware)
		return SR_ERR_ARG;
	if (DS_RES_PATH[0] == '\0')
		return SR_ERR_FIRMWARE_NOT_EXIST;

	dir_len = strlen(DS_RES_PATH);
	if (dir_len > 0 && DS_RES_PATH[dir_len - 1] == '/')
		*path = g_strdup_printf("%s%s", DS_RES_PATH, profile->firmware);
	else
		*path = g_strdup_printf("%s/%s", DS_RES_PATH, profile->firmware);

	return *path ? SR_OK : SR_ERR_MALLOC;
}
```

- [ ] **Step 5: Verify GREEN**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: PASS, with 6 upstream fx2lafw test cases.

- [ ] **Step 6: Commit**

```bash
git add PXTOOL/test/test_upstream_fx2lafw.cpp libsigrok/hardware/upstream-fx2lafw/fx2lafw.h libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
git commit -m "feat: add fx2lafw firmware path helper"
```

## Task 2: Firmware State During Scan

**Files:**
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`
- Modify: `PXTOOL/test/test_upstream_fx2lafw.cpp`

**Interfaces:**
- Consumes: `fx2lafw_firmware_path()`, `ezusb_upload_firmware(libusb_device *dev, int configuration, const char *filename)`, `libusb_get_string_descriptor_ascii()`.
- Produces:
  - `FX2LAFW_USB_INTERFACE`
  - `FX2LAFW_USB_CONFIGURATION`
  - `FX2LAFW_UNKNOWN_ADDRESS`
  - `FX2LAFW_MAX_RENUM_DELAY_MS`
  - `SR_PRIV int fx2lafw_has_firmware(const char *manufacturer, const char *product);`
  - device context fields for `fw_updated` and firmware upload state

- [ ] **Step 1: Add failing firmware-state tests**

Append this test case before `BOOST_AUTO_TEST_SUITE_END()`:

```cpp
BOOST_AUTO_TEST_CASE(firmware_state_uses_sigrok_fx2lafw_strings)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    BOOST_CHECK_EQUAL(fx2lafw_has_firmware("sigrok", "fx2lafw"), TRUE);
    BOOST_CHECK_EQUAL(fx2lafw_has_firmware("Saleae", "Logic"), FALSE);
    BOOST_CHECK_EQUAL(fx2lafw_has_firmware(nullptr, "fx2lafw"), FALSE);
    BOOST_CHECK_EQUAL(fx2lafw_has_firmware("sigrok", nullptr), FALSE);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}
```

- [ ] **Step 2: Declare constants and helper**

Add these definitions near the existing `FX2LAFW_DEV_CAPS_16BIT` in `fx2lafw.h`:

```c
#define FX2LAFW_USB_INTERFACE 0
#define FX2LAFW_USB_CONFIGURATION 1
#define FX2LAFW_UNKNOWN_ADDRESS 0xff
#define FX2LAFW_MAX_RENUM_DELAY_MS 3000
```

Add this declaration after `fx2lafw_firmware_path()`:

```c
SR_PRIV int fx2lafw_has_firmware(const char *manufacturer,
	const char *product);
```

- [ ] **Step 3: Verify RED**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected: FAIL at link time because `fx2lafw_has_firmware` is not implemented.

- [ ] **Step 4: Link EZ-USB helper into tests**

Modify the `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` block in `PXTOOL/test/CMakeLists.txt` so it includes `ezusb.c`:

```cmake
if(DSVIEW_ENABLE_UPSTREAM_FX2LAFW)
    target_sources(DSView-test PRIVATE
        ../../libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
        ../../libsigrok/hardware/common/ezusb.c
    )
endif()
```

- [ ] **Step 5: Extend the device context**

Modify `struct fx2lafw_context` in `fx2lafw.c` to include firmware lifecycle fields:

```c
struct fx2lafw_context {
	const struct fx2lafw_profile *profile;
	uint64_t samplerate;
	uint64_t limit_samples;
	gint64 fw_updated;
	gboolean firmware_loaded;
};
```

- [ ] **Step 6: Implement firmware-state helper**

Add this function after `fx2lafw_firmware_path()`:

```c
SR_PRIV int fx2lafw_has_firmware(const char *manufacturer,
	const char *product)
{
	return manufacturer && product &&
		strcmp(manufacturer, "sigrok") == 0 &&
		strcmp(product, "fx2lafw") == 0;
}
```

- [ ] **Step 7: Update device creation**

Change `create_device_from_profile()` signature in `fx2lafw.c` from:

```c
static struct sr_dev_inst *create_device_from_profile(
	const struct fx2lafw_profile *profile, uint8_t bus, uint8_t address)
```

to:

```c
static struct sr_dev_inst *create_device_from_profile(
	const struct fx2lafw_profile *profile, uint8_t bus, uint8_t address,
	int status, gboolean firmware_loaded, gint64 fw_updated)
```

Inside the function, change the `sr_dev_inst_new()` call from:

```c
sdi = sr_dev_inst_new(LOGIC, SR_ST_INACTIVE,
	profile->vendor, profile->model, NULL);
```

to:

```c
sdi = sr_dev_inst_new(LOGIC, status, profile->vendor, profile->model, NULL);
```

After `devc->limit_samples = 0;`, add:

```c
devc->firmware_loaded = firmware_loaded;
devc->fw_updated = fw_updated;
```

- [ ] **Step 8: Update scan to mark firmware state**

In `hw_scan()`, after reading manufacturer/product and after the final profile lookup, replace the existing `create_device_from_profile()` call with:

```c
gboolean has_firmware;
gint64 fw_updated;
uint8_t address;
int status;

has_firmware = fx2lafw_has_firmware(manufacturer, product);
fw_updated = 0;
address = libusb_get_device_address(devlist[i]);
status = SR_ST_INACTIVE;

if (!has_firmware) {
	char *firmware;
	int upload_ret;

	firmware = NULL;
	upload_ret = fx2lafw_firmware_path(profile, &firmware);
	if (upload_ret == SR_OK) {
		upload_ret = ezusb_upload_firmware(devlist[i],
			FX2LAFW_USB_CONFIGURATION, firmware);
		g_free(firmware);
	}
	if (upload_ret == SR_OK) {
		has_firmware = FALSE;
		fw_updated = g_get_monotonic_time();
		address = FX2LAFW_UNKNOWN_ADDRESS;
		status = SR_ST_INITIALIZING;
	} else {
		sr_err("Firmware upload failed for device %d.%d, name %s.",
			libusb_get_bus_number(devlist[i]),
			libusb_get_device_address(devlist[i]),
			profile->firmware ? profile->firmware : "(null)");
		continue;
	}
}

struct sr_dev_inst *sdi = create_device_from_profile(profile,
	libusb_get_bus_number(devlist[i]), address, status, has_firmware,
	fw_updated);
```

- [ ] **Step 9: Verify GREEN**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: PASS. On machines without bootloader-state hardware, scan still returns zero or firmware-loaded devices without crashing.

- [ ] **Step 10: Commit**

```bash
git add PXTOOL/test/CMakeLists.txt PXTOOL/test/test_upstream_fx2lafw.cpp libsigrok/hardware/upstream-fx2lafw/fx2lafw.h libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
git commit -m "feat: track fx2lafw firmware state"
```

## Task 3: Open Firmware-Loaded Devices Safely

**Files:**
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`
- Modify: `PXTOOL/test/test_upstream_fx2lafw.cpp`

**Interfaces:**
- Consumes: `struct sr_usb_dev_inst`, `fx2lafw_driver_info.dev_open`, `CMD_GET_FW_VERSION` upstream semantics.
- Produces:
  - `FX2LAFW_REQUIRED_VERSION_MAJOR`
  - `FX2LAFW_CMD_GET_FW_VERSION`
  - `FX2LAFW_USB_TIMEOUT_MS`
  - `dev_open` implementation registered in `fx2lafw_driver_info`

- [ ] **Step 1: Add failing open lifecycle wiring test**

Append this test case before `BOOST_AUTO_TEST_SUITE_END()`:

```cpp
BOOST_AUTO_TEST_CASE(driver_exposes_open_lifecycle)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    BOOST_REQUIRE(fx2lafw_driver_info.dev_open != nullptr);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}
```

- [ ] **Step 2: Add protocol constants**

Add these definitions to `fx2lafw.h` near the other fx2lafw constants:

```c
#define FX2LAFW_REQUIRED_VERSION_MAJOR 1
#define FX2LAFW_CMD_GET_FW_VERSION 0xb0
#define FX2LAFW_USB_TIMEOUT_MS 100
```

- [ ] **Step 3: Verify RED**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw/driver_exposes_open_lifecycle
```

Expected: FAIL because `fx2lafw_driver_info.dev_open` is still null.

- [ ] **Step 4: Add firmware version command helper**

Add these packed structs and helper after `struct fx2lafw_driver_context` in `fx2lafw.c`:

```c
#pragma pack(push, 1)
struct fx2lafw_version_info {
	uint8_t major;
	uint8_t minor;
};
#pragma pack(pop)

static int command_get_fw_version(libusb_device_handle *devhdl,
	struct fx2lafw_version_info *version)
{
	int ret;

	if (!devhdl || !version)
		return SR_ERR_ARG;

	ret = libusb_control_transfer(devhdl,
		LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
		FX2LAFW_CMD_GET_FW_VERSION, 0x0000, 0x0000,
		(unsigned char *)version, sizeof(*version),
		FX2LAFW_USB_TIMEOUT_MS);
	if (ret < 0) {
		sr_err("Unable to get fx2lafw firmware version: %s.",
			libusb_error_name(ret));
		return SR_ERR;
	}

	return SR_OK;
}
```

- [ ] **Step 5: Add handle cleanup helper**

Add this helper before `hw_scan()`:

```c
static void close_usb_handle(struct sr_dev_inst *sdi)
{
	struct sr_usb_dev_inst *usb;

	if (!sdi || !sdi->conn)
		return;

	usb = sdi->conn;
	if (!usb->devhdl)
		return;

	libusb_release_interface(usb->devhdl, FX2LAFW_USB_INTERFACE);
	libusb_close(usb->devhdl);
	usb->devhdl = NULL;
}
```

- [ ] **Step 6: Add device matching/open helper**

Add this helper before `config_get()`:

```c
static int open_matching_device(struct sr_dev_inst *sdi)
{
	struct fx2lafw_driver_context *drvc;
	struct fx2lafw_context *devc;
	struct sr_usb_dev_inst *usb;
	libusb_device **devlist;
	ssize_t device_count;
	int ret;

	if (!sdi || !sdi->priv || !sdi->conn)
		return SR_ERR_ARG;

	drvc = fx2lafw_driver_info.priv;
	devc = sdi->priv;
	usb = sdi->conn;
	if (!drvc || !drvc->libusb_ctx || !devc->profile)
		return SR_ERR;

	device_count = libusb_get_device_list(drvc->libusb_ctx, &devlist);
	if (device_count < 0)
		return SR_ERR;

	ret = SR_ERR;
	for (ssize_t i = 0; i < device_count; i++) {
		struct libusb_device_descriptor desc;
		libusb_device_handle *handle;
		uint8_t bus;
		uint8_t address;

		if (libusb_get_device_descriptor(devlist[i], &desc) < 0)
			continue;
		if (desc.idVendor != devc->profile->vid ||
				desc.idProduct != devc->profile->pid)
			continue;

		bus = libusb_get_bus_number(devlist[i]);
		address = libusb_get_device_address(devlist[i]);
		if (usb->address != FX2LAFW_UNKNOWN_ADDRESS &&
				(usb->bus != bus || usb->address != address))
			continue;

		if (libusb_open(devlist[i], &handle) != 0)
			continue;

		usb->bus = bus;
		usb->address = address;
		usb->devhdl = handle;
		ret = SR_OK;
		break;
	}

	libusb_free_device_list(devlist, 1);
	return ret;
}
```

- [ ] **Step 7: Add `hw_dev_open()`**

Add this function before `fx2lafw_driver_info`:

```c
static int hw_dev_open(struct sr_dev_inst *sdi)
{
	struct fx2lafw_context *devc;
	struct sr_usb_dev_inst *usb;
	struct fx2lafw_version_info version;
	int ret;

	if (!sdi || !sdi->priv || !sdi->conn)
		return SR_ERR_ARG;

	devc = sdi->priv;
	usb = sdi->conn;

	if (usb->devhdl)
		return SR_OK;

	if (sdi->status == SR_ST_INITIALIZING && devc->fw_updated > 0) {
		gint64 waited_ms;

		g_usleep(300 * 1000);
		waited_ms = 0;
		while (waited_ms < FX2LAFW_MAX_RENUM_DELAY_MS) {
			ret = open_matching_device(sdi);
			if (ret == SR_OK)
				break;
			g_usleep(100 * 1000);
			waited_ms = (g_get_monotonic_time() - devc->fw_updated) / 1000;
		}
		if (!usb->devhdl)
			return SR_ERR;
	} else {
		ret = open_matching_device(sdi);
		if (ret != SR_OK)
			return ret;
	}

#if !defined(__APPLE__)
	if (libusb_kernel_driver_active(usb->devhdl, FX2LAFW_USB_INTERFACE) == 1) {
		ret = libusb_detach_kernel_driver(usb->devhdl,
			FX2LAFW_USB_INTERFACE);
		if (ret < 0) {
			close_usb_handle(sdi);
			return SR_ERR;
		}
	}
#endif

	ret = libusb_claim_interface(usb->devhdl, FX2LAFW_USB_INTERFACE);
	if (ret != 0) {
		close_usb_handle(sdi);
		return SR_ERR;
	}

	ret = command_get_fw_version(usb->devhdl, &version);
	if (ret != SR_OK) {
		close_usb_handle(sdi);
		return ret;
	}

	if (version.major != FX2LAFW_REQUIRED_VERSION_MAJOR) {
		close_usb_handle(sdi);
		return SR_ERR_DEVICE_FIRMWARE_VERSION_LOW;
	}

	devc->firmware_loaded = TRUE;
	sdi->status = SR_ST_ACTIVE;
	return SR_OK;
}
```

- [ ] **Step 8: Register `dev_open`**

In `fx2lafw_driver_info`, add:

```c
	.dev_open = hw_dev_open,
```

Do not add `dev_close` yet; Task 4 adds and tests close behavior separately.

- [ ] **Step 9: Verify GREEN**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: PASS for the focused upstream fx2lafw suite.

- [ ] **Step 10: Commit**

```bash
git add PXTOOL/test/test_upstream_fx2lafw.cpp libsigrok/hardware/upstream-fx2lafw/fx2lafw.h libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
git commit -m "feat: open fx2lafw upstream devices"
```

## Task 4: Close Handles and Finalize Lifecycle

**Files:**
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`
- Modify: `PXTOOL/test/test_upstream_fx2lafw.cpp`
- Modify: `docs/libsigrok-upstream-driver-inventory.md`

**Interfaces:**
- Consumes: `close_usb_handle()`, `fx2lafw_driver_info.dev_close`.
- Produces: `dev_close` implementation registered in `fx2lafw_driver_info`.

- [ ] **Step 1: Add close-without-open test**

Append this test case before `BOOST_AUTO_TEST_SUITE_END()`:

```cpp
BOOST_AUTO_TEST_CASE(close_without_open_returns_error)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = sr_dev_inst_new(LOGIC, SR_ST_INACTIVE,
        profile->vendor, profile->model, nullptr);
    BOOST_REQUIRE(sdi != nullptr);
    sdi->driver = &fx2lafw_driver_info;
    sdi->conn = sr_usb_dev_inst_new(1, 2);

    BOOST_REQUIRE(fx2lafw_driver_info.dev_close != nullptr);
    BOOST_CHECK_EQUAL(fx2lafw_driver_info.dev_close(sdi), SR_ERR_BUG);
    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}
```

- [ ] **Step 2: Verify RED**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw/close_without_open_returns_error
```

Expected: FAIL because `dev_close` is not registered.

- [ ] **Step 3: Add `hw_dev_close()`**

Add this function after `hw_dev_open()`:

```c
static int hw_dev_close(struct sr_dev_inst *sdi)
{
	struct sr_usb_dev_inst *usb;

	if (!sdi || !sdi->conn)
		return SR_ERR_ARG;

	usb = sdi->conn;
	if (!usb->devhdl)
		return SR_ERR_BUG;

	close_usb_handle(sdi);
	sdi->status = SR_ST_INACTIVE;
	return SR_OK;
}
```

- [ ] **Step 4: Register `dev_close`**

In `fx2lafw_driver_info`, add:

```c
	.dev_close = hw_dev_close,
```

- [ ] **Step 5: Update inventory documentation**

In `docs/libsigrok-upstream-driver-inventory.md`, update the `Selected Slice: fx2lafw Scan-Only` section by adding this section after it:

```markdown
## Next Slice: fx2lafw Open/Firmware Lifecycle

Status: planned for implementation behind `DSVIEW_ENABLE_UPSTREAM_FX2LAFW`.

Scope:

- detect firmware-loaded devices during scan
- upload fx2lafw firmware for bootloader-state devices using DSView's firmware resource directory
- wait for re-enumeration in `dev_open()`
- open, claim interface 0, validate firmware major version, and close handles safely

Still deferred:

- asynchronous acquisition transfers
- trigger setup
- DSView datafeed conversion
```

- [ ] **Step 6: Verify full upstream fx2lafw tests**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: PASS, with all upstream fx2lafw tests passing.

- [ ] **Step 7: Verify full build and tests**

Run:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test
make DSView -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected: PASS. Existing warnings are acceptable only if the commands exit 0 and no new fatal errors appear.

- [ ] **Step 8: Commit**

```bash
git add PXTOOL/test/test_upstream_fx2lafw.cpp libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
git add -f docs/libsigrok-upstream-driver-inventory.md
git commit -m "feat: close fx2lafw upstream devices"
```

## Manual Hardware Smoke Test

After the implementation plan is complete, use real fx2lafw-compatible hardware if available:

```bash
script/macos/build_and_run.sh
```

Expected manual behavior:

- current DSView demo device behavior remains available
- `fx2lafw` appears only when `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` is enabled and matching hardware is present
- firmware-loaded devices can be opened and closed without hanging
- bootloader-state devices upload firmware from DSView's configured resource directory, re-enumerate, then open
- acquisition is still unavailable for this upstream driver until the next slice
