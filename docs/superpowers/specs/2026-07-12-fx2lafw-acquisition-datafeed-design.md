# FX2LAFW Acquisition Datafeed Design

Date: 2026-07-12
Branch: `upgrade-libsigrok`

## Goal

Extend the build-gated `upstream-fx2lafw` driver from scan/open/firmware lifecycle to real logic acquisition that feeds DSView's existing waveform data path.

This is the next compatibility milestone toward closing the gap with upstream `libsigrok`: one upstream-derived USB logic analyzer must scan, open, start acquisition, stream logic packets through DSView, stop safely, and leave DSView-native devices unchanged.

## Current State

Implemented:

- `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` build gate.
- `fx2lafw` profile table, config list/get/set, and default enabled `D0..D7` or `D0..D15` channels.
- libusb scan path that tags devices as `DS_DEVICE_SOURCE_UPSTREAM_COMPAT`.
- firmware-loaded open/close lifecycle.
- bootloader firmware upload path when licensed firmware resources are present.
- `PXTOOL/res/fx2lafw/manifest.txt` and smoke-test documentation.

Still missing:

- `dev_acquisition_start` and `dev_acquisition_stop` callbacks for `fx2lafw_driver_info`.
- FX2 `CMD_START` control transfer.
- async bulk transfers from endpoint `2 | LIBUSB_ENDPOINT_IN`.
- event-loop integration with DSView's current `sr_session_run()` / `sr_session_source_add()` model.
- conversion of received bytes into `SR_DF_LOGIC` packets sent with `ds_data_forward()`.
- safe stop/cancel/free behavior.
- hardware smoke checklist for acquisition.

## Chosen Approach

Implement acquisition in the existing DSView-adapted `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`, using upstream `src/hardware/fx2lafw/protocol.c` as the reference but adapting it to DSView's local session and packet APIs.

The first acquisition slice will support logic-only acquisition. It will not import upstream analog/MSO handling, soft trigger logic, or generalized upstream USB helper infrastructure yet.

Rationale:

- `fx2lafw` is the first real upstream-derived waveform device path already present in DSView.
- DSView's current session loop already supports pollfd sources through `sr_session_source_add()`.
- DSView already consumes `SR_DF_HEADER`, `SR_DF_LOGIC`, and `SR_DF_END` packets through `ds_data_forward()`.
- A small `fx2lafw`-local implementation proves the acquisition/datafeed boundary before extracting shared helpers for other upstream USB drivers.

## Alternatives Considered

### Import upstream `protocol.c` nearly whole

Pros:

- Fastest apparent route to upstream parity for `fx2lafw`.
- Keeps upstream algorithms recognizable.

Cons:

- Pulls in upstream session APIs such as `sr_session_send()`, `usb_source_add()`, trigger helpers, channel groups, analog packet structures, and upstream `dev_context`.
- Increases migration surface before DSView's compatibility boundary is proven.

Decision: defer. Use upstream protocol code as a reference, not a direct wholesale import.

### Build generic upstream USB acquisition helpers first

Pros:

- Better long-term architecture for many USB drivers.
- Could reduce duplication when adding `hantek-4032l` and other devices.

Cons:

- Premature abstraction before one real DSView upstream acquisition path works.
- Risks refactoring session and USB source behavior without hardware proof.

Decision: defer until `fx2lafw` acquisition works end to end.

### Logic-only `fx2lafw` acquisition first

Pros:

- Smallest slice that proves upstream device data can reach DSView waveforms.
- Hardware-independent unit tests can cover command calculation, buffer sizing, packet conversion, and lifecycle edges.
- Leaves analog/MSO and trigger support for later slices.

Cons:

- Does not yet expose every upstream `fx2lafw` capability.
- Needs a later extraction pass before adding multiple similar USB drivers.

Decision: use this approach.

## Scope

### Included

- Add acquisition state to `struct fx2lafw_context`.
- Add helper APIs for testable acquisition calculations:
  - whether a profile/channel configuration uses 8-bit or 16-bit samples
  - start-command flags and sample delay
  - transfer buffer size
  - transfer count
  - timeout
  - enabled-channel mask
- Implement `CMD_START` as a vendor OUT control transfer.
- Submit async libusb bulk transfers on endpoint `2 | LIBUSB_ENDPOINT_IN`.
- Integrate libusb event handling into DSView's `sr_session_run()` using `sr_session_source_add()`.
- Convert completed transfer bytes into `SR_DF_LOGIC` packets:
  - `logic.length = received byte length`
  - `logic.unitsize = 1` for 8-channel captures
  - `logic.unitsize = 2` for 16-channel captures
  - `logic.format = LA_CROSS_DATA`
- Send `SR_DF_HEADER` before data and `SR_DF_END` exactly once on completion.
- Respect `SR_CONF_LIMIT_SAMPLES` when nonzero.
- Stop acquisition by canceling submitted transfers and cleaning all buffers/transfers.
- Add tests for hardware-independent acquisition helpers and datafeed adapter behavior.
- Update acquisition smoke-test documentation.
- Update the upstream driver inventory to mark firmware resources complete and acquisition as the active next slice.

### Excluded

- Upstream soft trigger support.
- DSView advanced trigger mapping.
- Analog/MSO support for CWAV USBee AX analog channel.
- Multi-frame capture semantics beyond a single DSView acquisition run.
- Generic upstream USB helper extraction.
- Adding `hantek-4032l`, `rigol-ds`, or any additional upstream driver.
- Distributing `fx2lafw-*.fw` binaries.
- Replacing DSView's `libsigrok/` tree with upstream.
- Rewriting PXTOOL UI or session architecture.

## Acquisition Design

### State

Extend `struct fx2lafw_context` with DSView-local acquisition state:

- `gboolean acquisition_running`
- `gboolean acquisition_aborted`
- `gboolean sample_wide`
- `uint64_t sent_samples`
- `unsigned int submitted_transfers`
- `unsigned int num_transfers`
- `unsigned int empty_transfer_count`
- `struct libusb_transfer **transfers`
- `gintptr event_source`
- `gboolean event_source_added`
- `gboolean end_sent`

The existing `samplerate`, `limit_samples`, `profile`, and `firmware_loaded` fields remain the config source of truth.

### Start Command

Use the upstream command format:

```c
struct fx2lafw_cmd_start_acquisition {
	uint8_t flags;
	uint8_t sample_delay_h;
	uint8_t sample_delay_l;
};
```

Constants:

- `FX2LAFW_CMD_START 0xb1`
- `FX2LAFW_CMD_START_FLAGS_SAMPLE_8BIT (0 << 5)`
- `FX2LAFW_CMD_START_FLAGS_SAMPLE_16BIT (1 << 5)`
- `FX2LAFW_CMD_START_FLAGS_CLK_30MHZ (0 << 6)`
- `FX2LAFW_CMD_START_FLAGS_CLK_48MHZ (1 << 6)`
- `FX2LAFW_MAX_16BIT_SAMPLE_RATE SR_MHZ(12)`
- `FX2LAFW_MAX_SAMPLE_DELAY (6 * 256)`

Delay selection follows upstream:

1. Prefer 48MHz when `48MHz % samplerate == 0` and computed delay is valid.
2. Fall back to 30MHz when `30MHz % samplerate == 0`.
3. Reject unsupported samplerates.
4. Reject 16-bit sampling above 12MHz.

### Sample Width

Logic-only width is derived from enabled channels:

- 8-bit mode when no enabled channel has index greater than 7.
- 16-bit mode when any enabled channel has index 8 through 15.

This keeps existing 16-channel profiles usable while avoiding analog/MSO handling.

### Transfer Sizing

Use upstream sizing rules:

- `bytes_per_ms = samplerate / 1000`
- buffer holds about 10ms of data
- buffer size is rounded up to a 512-byte multiple
- total queued transfer size represents about 500ms
- cap simultaneous transfers at 32
- timeout is total queued transfer time plus 25 percent

The helper functions must be testable without libusb hardware.

### Event Loop

DSView's `sr_session_run()` polls sources registered through `sr_session_source_add()`. The `fx2lafw` driver should register one source that periodically calls `libusb_handle_events_timeout()` with a zero timeval for the driver's libusb context.

The event source may use a dummy poll object when no platform pollfd integration is needed. It must be removed when acquisition finishes or stops.

### Datafeed

On acquisition start:

1. verify `sdi->status == SR_ST_ACTIVE`
2. initialize acquisition counters
3. compute sample width and transfer sizing
4. add the session event source
5. allocate and submit transfers
6. send `SR_DF_HEADER`
7. send `CMD_START`

On transfer completion:

1. ignore/free canceled transfers after abort
2. handle `LIBUSB_TRANSFER_NO_DEVICE` by aborting acquisition
3. tolerate timeout transfers when bytes were received
4. count empty/error transfers and abort after `MAX_EMPTY_TRANSFERS`
5. send only up to `limit_samples` when a limit is configured
6. send `SR_DF_LOGIC` through `ds_data_forward()`
7. cancel remaining transfers once sample limit is reached

On finish:

1. free transfer buffers and transfer structs
2. remove the session event source
3. send `SR_DF_END` once
4. clear running state

## Test Strategy

Hardware-independent tests first:

- start-command calculation for common valid samplerates
- invalid samplerate rejection
- 16-bit samplerate limit rejection
- 8-bit versus 16-bit sample-width detection from enabled channels
- transfer buffer size, count, and timeout calculations
- datafeed helper sends `SR_DF_LOGIC` with correct `length`, `unitsize`, `format`, and pointer
- stop without a running acquisition is safe
- driver exposes `dev_acquisition_start` and `dev_acquisition_stop`

Hardware smoke tests second:

- firmware-loaded device starts acquisition and shows logic waveform data
- stop button cancels transfers and emits one end packet
- unplug during acquisition does not hang
- bootloader-state device still uploads firmware before acquisition

Full verification:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test
make DSView -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

## Risks

### DSView session loop differs from upstream

Mitigation: adapt to `sr_session_source_add()` and keep event-loop logic local to `fx2lafw` for this slice.

### Async transfer cleanup can double-free or hang

Mitigation: track submitted transfers explicitly; send end once; make stop idempotent; test stop-without-running and finish cleanup helpers.

### Packet format mismatch with DSView waveform path

Mitigation: mirror `upstream-demo` and native logic packet conventions: `SR_DF_LOGIC`, `LA_CROSS_DATA`, and unitsize 1 or 2.

### Hardware may be unavailable

Mitigation: keep implementation gated and unit-tested; document hardware smoke tests separately. Do not claim hardware support until smoke evidence exists.

## Success Criteria

- `fx2lafw_driver_info` exposes acquisition start/stop callbacks behind `DSVIEW_ENABLE_UPSTREAM_FX2LAFW`.
- Hardware-independent tests pass for acquisition calculations and datafeed packet construction.
- DSView builds and tests pass with upstream demo and fx2lafw gates enabled.
- With real firmware-loaded fx2lafw hardware, DSView can start and stop acquisition without hanging and can display logic waveform data.
- Existing DSLogic, DSCope, PXLogic, demo, file, decode, and session behavior are not intentionally changed.

## Follow-Up After This Slice

1. Add trigger support for `fx2lafw`.
2. Decide whether to distribute licensed `fx2lafw-*.fw` binaries.
3. Extract shared upstream USB acquisition helpers after one real device is proven.
4. Add a second USB logic analyzer driver such as `hantek-4032l`.
5. Add SCPI oscilloscope support, starting with `rigol-ds`, after SCPI helper scope is planned.
