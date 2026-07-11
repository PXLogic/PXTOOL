# FX2LAFW Open and Firmware Lifecycle Design

Date: 2026-07-11
Branch: `upgrade-libsigrok`

## Goal

Extend the existing build-gated `upstream-fx2lafw` compatibility driver from scan-only discovery to a safe open/close lifecycle that can handle already-firmware-loaded devices and bootloader-state devices that need fx2lafw firmware upload.

## Context

The current DSView slice already adds:

- `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` and `HAVE_UPSTREAM_FX2LAFW`
- an allowlisted `fx2lafw_driver_info`
- upstream-derived profile lookup
- DSView-compatible config list/get/set for samplerate, limit samples, valid channels, and channel enable state
- default-enabled `D0` through `D7` or `D15` channels
- libusb scan that creates inactive DSView USB devices tagged `DS_DEVICE_SOURCE_UPSTREAM_COMPAT`

The next step should prove the hardware lifecycle before acquisition. Firmware upload and USB re-enumeration are the riskiest parts of the upstream `fx2lafw` path, and they should be isolated from asynchronous acquisition transfers.

## Scope

This slice will add:

- firmware-state detection during scan
- DSView resource-path based firmware filename construction
- firmware upload for bootloader-state fx2lafw-compatible devices
- re-enumeration wait in `dev_open()`
- libusb open and interface claim for firmware-loaded devices
- firmware major-version validation
- libusb release/close in `dev_close()`
- unit tests for helper behavior and no-hardware safety

This slice will not add:

- acquisition start/stop
- USB bulk transfer submission
- trigger setup
- analog channel support
- datafeed conversion into `ds_data_forward()`
- support for every upstream fx2lafw profile field

## Architecture

Keep the current one-file `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c` driver for this slice. It is still small enough that splitting would add more friction than clarity. Add private helpers inside that file and expose only test seams needed by `PXTOOL/test/test_upstream_fx2lafw.cpp`.

Use DSView's existing infrastructure where possible:

- `ds_set_firmware_resource_dir()` owns the global firmware resource directory.
- `ezusb_upload_firmware()` performs Cypress EZ-USB firmware upload.
- `sr_usb_dev_inst_new()` stores bus/address and `devhdl`.
- `struct sr_dev_driver.dev_open` and `dev_close` define the lifecycle entry points.

The driver will keep its build gate. When `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` is off, none of this code is compiled into `DSView` or `DSView-test`.

## Device State Flow

Scan has two outcomes for supported VID/PID/profile matches:

1. Firmware-loaded device
   - Detect by USB manufacturer/product strings matching `sigrok` / `fx2lafw`.
   - Create `sr_dev_inst` with `SR_ST_INACTIVE`.
   - Store current bus/address in `sdi->conn`.
   - `dev_open()` can open by bus/address immediately.

2. Bootloader-state device
   - Build the firmware path from DSView's firmware resource directory plus `profile->firmware`.
   - Upload firmware with `ezusb_upload_firmware()`.
   - Create `sr_dev_inst` with `SR_ST_INITIALIZING`.
   - Store bus and address `0xff` to indicate that the post-upload address is not known yet.
   - Store a monotonic timestamp in the device context.
   - `dev_open()` waits for the device to disappear/reappear and tries to open the matching VID/PID/profile until the timeout expires.

The initial timeout should match upstream's intent: sleep 300 ms after upload, then poll for up to 3000 ms in 100 ms intervals.

## Firmware Path Handling

The driver should not hardcode an absolute firmware path and must not depend on `/Users/yuanji/Desktop/project/libsigrok`.

Add a tiny DSView-internal accessor for the firmware resource directory if needed, because `DS_RES_PATH` is currently private to `lib_main.c`. The helper should return a const string view of the directory already set by `ds_set_firmware_resource_dir()`.

Firmware path construction should:

- return `SR_ERR_FIRMWARE_NOT_EXIST` or `SR_ERR` when the resource directory is empty
- join directory and filename with exactly one slash
- reject missing profile or missing firmware filename with `SR_ERR_ARG`
- keep ownership clear: callers free the returned string with `g_free()`

## Open/Close Behavior

`dev_open()` should:

- validate `sdi`, `sdi->priv`, and `sdi->conn`
- for `SR_ST_INITIALIZING`, wait for re-enumeration before opening
- for `SR_ST_INACTIVE`, open immediately
- find the matching libusb device using VID/PID/profile and current bus/address when known
- call `libusb_open()`
- detach kernel driver only on platforms where DSView already does so safely
- claim interface `0`
- read firmware version with vendor command `CMD_GET_FW_VERSION`
- require major version `1`
- set `sdi->status = SR_ST_ACTIVE` on success
- close and clear handles on failure

`dev_close()` should:

- validate `sdi` and `sdi->conn`
- return a predictable error if no handle is open
- release interface `0`
- close `usb->devhdl`
- set `usb->devhdl = NULL`
- set `sdi->status = SR_ST_INACTIVE`

## Test Strategy

Keep tests hardware-independent by default:

- profile and config tests from the scan-only slice keep passing
- helper test: firmware path joins normal directories correctly
- helper test: missing firmware resource directory returns an error
- lifecycle test: `dev_close()` on a scanned but unopened synthetic device returns the expected error and does not crash
- scan test: no options/no hardware still does not crash

Hardware smoke testing remains manual:

- with a supported fx2lafw device already running firmware, scan should show the device and open/close should succeed
- with a bootloader-state device and firmware files in the DSView resource directory, scan should upload firmware, then open should wait for re-enumeration and claim interface 0

## Risks

The largest risk is USB lifecycle leakage. DSView's `sr_usb_dev_inst_free()` currently does not close libusb handles, so the upstream-compat driver must close handles itself in `dev_close()` and on every open failure path.

Another risk is matching the re-enumerated device. Upstream uses physical USB port paths, but DSView's local `sr_dev_inst` does not currently have `connection_id`. This slice should use the narrower matching available in DSView: profile VID/PID plus bus/address when known, and profile string matching when descriptors are readable.

## Acceptance Criteria

- default builds remain unaffected when `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` is off
- upstream fx2lafw builds compile with `DSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON`
- `DSView-test` passes with the upstream demo and fx2lafw flags enabled
- `DSView` builds with the same flags enabled
- no copied upstream file keeps an upstream libsigrok header; DSView/PXTOOL header style is preserved
- no runtime path depends on `/Users/yuanji/Desktop/project/libsigrok`
