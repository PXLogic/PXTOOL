# FX2LAFW Firmware Resources and Smoke Gate Design

Date: 2026-07-11
Branch: `upgrade-libsigrok`

## Goal

Make the upstream-compatible `fx2lafw` lifecycle path honest and operable by defining how firmware resources are discovered, packaged, checked, and manually smoke-tested before DSView moves on to acquisition.

## Context

The current `upstream-fx2lafw` driver can:

- scan supported fx2lafw VID/PID profiles
- create DSView USB devices tagged `DS_DEVICE_SOURCE_UPSTREAM_COMPAT`
- detect devices that already expose USB strings `sigrok` / `fx2lafw`
- open, claim interface 0, validate firmware major version, and close firmware-loaded devices
- attempt bootloader-state firmware upload only when the expected firmware file exists in DSView's firmware resource directory

The current repository does not contain `fx2lafw-*.fw` firmware files in `PXTOOL/res`, and the local upstream reference at `/Users/yuanji/Desktop/project/libsigrok` also does not contain them. DSView's CMake packaging copies `PXTOOL/res` into `share/PXTOOL/res`, and `SigSession::init()` passes that directory into libsigrok via `ds_set_firmware_resource_dir()`.

## Scope

This slice will add a resource contract and smoke-test gate for fx2lafw firmware.

It will include:

- a manifest listing every firmware filename referenced by the selected `fx2lafw` profile table
- tests that keep the manifest in sync with the driver's profile table
- a clear runtime diagnostic when a bootloader-state device is found but the required firmware file is not bundled
- documentation for where firmware files must be placed when the project owner supplies licensed firmware resources
- a manual smoke-test checklist for bootloader upload, re-enumeration, open, and close
- inventory updates that clearly distinguish firmware-loaded support from bootloader upload support

It will not include:

- adding unknown-origin firmware binaries
- downloading firmware at build time
- changing the app UI
- acquisition start/stop
- USB bulk transfers
- trigger setup
- DSView datafeed conversion

## Resource Contract

Firmware files used by this driver must live under DSView's existing firmware resource directory. The default packaged path is:

```text
PXTOOL/res/fx2lafw/<firmware-name>.fw
```

At runtime this resolves under:

```text
<DS_RES_PATH>/fx2lafw/<firmware-name>.fw
```

The driver should first check the subdirectory form above. If the project later decides to keep the files flat in `PXTOOL/res`, that can be added as a compatibility fallback, but this slice should prefer the subdirectory to avoid mixing upstream firmware names with DreamSourceLab firmware files.

The manifest should list the filenames currently referenced by `supported_fx2[]`:

- `fx2lafw-cwav-usbeeax.fw`
- `fx2lafw-cwav-usbeedx.fw`
- `fx2lafw-cwav-usbeesx.fw`
- `fx2lafw-cwav-usbeezx.fw`
- `fx2lafw-saleae-logic.fw`
- `fx2lafw-cypress-fx2.fw`
- `fx2lafw-braintechnology-usb-lps.fw`
- `fx2lafw-sigrok-fx2-8ch.fw`
- `fx2lafw-sigrok-fx2-16ch.fw`
- `fx2lafw-usb-c-grok.fw`

The manifest is not a license grant. It is an operational contract: if the project owner supplies appropriately licensed firmware files with these names, DSView knows where to package and find them.

## Runtime Behavior

For a firmware-loaded device, behavior should remain unchanged: scan creates an inactive device, `dev_open()` opens and validates it, and `dev_close()` releases it.

For a bootloader-state device:

- if the firmware file exists under `<DS_RES_PATH>/fx2lafw/`, the current upload path may proceed
- if the firmware file is missing, scan should skip that bootloader device and log a diagnostic that includes the exact expected path
- the diagnostic should avoid implying that the hardware is unsupported; it should say the firmware resource is missing

This makes the current partial state explicit and prevents DSView from silently failing in packaged builds.

## Tests

Hardware-independent tests should cover:

- `fx2lafw_firmware_path()` returns the subdirectory path
- every selected `fx2lafw` profile has a firmware filename present in the manifest
- every manifest entry maps to one selected profile
- missing resource directory still returns `SR_ERR_FIRMWARE_NOT_EXIST`
- the existing no-hardware scan test still passes

Manual tests should cover real hardware:

- firmware-loaded device appears, opens, and closes
- bootloader-state device logs the expected missing-firmware path when firmware is absent
- after licensed firmware files are added under `PXTOOL/res/fx2lafw`, bootloader-state device uploads firmware, re-enumerates on the same bus, opens, and closes

## Acceptance Criteria

- no unknown-origin firmware binaries are added
- default builds remain unaffected when `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` is off
- upstream fx2lafw builds compile with `DSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON`
- `DSView-test` passes with upstream demo and fx2lafw flags enabled
- `DSView` builds with the same flags enabled
- documentation tells users exactly where firmware resources must be placed
- inventory states that bootloader upload is gated on supplied firmware resources and hardware smoke testing
- no runtime path depends on `/Users/yuanji/Desktop/project/libsigrok`
