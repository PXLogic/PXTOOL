# Libsigrok Upstream Driver Inventory

Date: 2026-07-11

## Rules

- `/Users/yuanji/Desktop/project/libsigrok` is a reference source only.
- Copied upstream source files must use the DSView/PXTOOL file header style.
- Drivers are enabled through an allowlist.
- The first real upstream driver should produce waveform data.
- Keep DSView's existing `ds_*` API facade until each imported driver path is proven.

## Candidate: fx2lafw

Purpose: USB logic analyzer class.

Verified upstream files:

- `src/hardware/fx2lafw/api.c`
- `src/hardware/fx2lafw/protocol.c`
- `src/hardware/fx2lafw/protocol.h`

Likely shared dependencies:

- `src/usb.c`
- libusb
- EZ-USB firmware loading helpers and firmware files for selected profiles

Risks:

- firmware renumeration flow
- async USB transfer lifecycle
- upstream session and trigger struct differences
- mapping upstream logic packets into DSView's `ds_data_forward()` path

Decision: recommended as the first real USB logic analyzer after the POC driver.

## Selected Slice: fx2lafw Scan-Only

Status: implemented behind `DSVIEW_ENABLE_UPSTREAM_FX2LAFW`.

Scope included:

- profile lookup for the selected upstream `fx2lafw` VID/PID table
- DSView-compatible config contract for samplerate, limit samples, valid channels, and channel enable state
- default-enabled logic channels named `D0` through `D7` or `D15`
- libusb scan path that creates inactive DSView USB devices tagged as `DS_DEVICE_SOURCE_UPSTREAM_COMPAT`

Scope intentionally deferred:

- firmware upload and USB renumeration
- opening and claiming device interfaces
- asynchronous acquisition transfers
- conversion into DSView's live logic data path

Follow-up direction: keep the next slice small by adding open/close and firmware handling first, then add acquisition only after that lifecycle is covered by tests and manual hardware checks.

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

## Candidate: rigol-ds

Purpose: SCPI oscilloscope class.

Verified upstream files:

- `src/hardware/rigol-ds/api.c`
- `src/hardware/rigol-ds/protocol.c`
- `src/hardware/rigol-ds/protocol.h`

Likely shared dependencies:

- `src/scpi.h`
- `src/scpi/scpi.c`
- one or more SCPI transports: serial, TCP, USBTMC, VISA, VXI
- libserialport for serial mode
- libusb for USBTMC mode

Risks:

- SCPI stack import size
- choosing one transport first without over-importing all transports
- analog waveform mapping into DSView's DSO path

Decision: second driver class after the USB logic analyzer path is stable.

## Candidate: hantek-4032l

Purpose: USB logic analyzer class.

Verified upstream files:

- `src/hardware/hantek-4032l/api.c`
- `src/hardware/hantek-4032l/protocol.c`
- `src/hardware/hantek-4032l/protocol.h`

Likely shared dependencies:

- `src/usb.c`
- libusb

Risks:

- device availability for manual smoke tests
- trigger and samplerate config mapping differences
- async transfer lifecycle versus DSView's current session callbacks

Decision: useful alternate first real USB logic analyzer if hardware is available.

## Recommended Next Step

Continue `fx2lafw` after the scan-only slice. Add firmware/open lifecycle next
if test hardware and firmware are available. Keep `rigol-ds` for a later
SCPI-focused slice because it pulls in a larger shared transport surface.
