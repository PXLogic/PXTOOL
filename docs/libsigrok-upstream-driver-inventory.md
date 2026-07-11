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

Start with `fx2lafw` if test hardware and firmware are available. Otherwise use
`hantek-4032l` as the first real import only if that device is on hand. Keep
`rigol-ds` for a later SCPI-focused slice because it pulls in a larger shared
transport surface.
