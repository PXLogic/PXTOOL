# Libsigrok Upstream Compatibility Design

Date: 2026-07-11
Branch: `upgrade-libsigrok`

## Goal

Bring DSView closer to upstream `sigrokproject/libsigrok` so DSView can support more hardware devices, while preserving the existing DSLogic, DSCope, PXLogic, decode, session, and UI behavior.

The first implementation target is not "enable every upstream device". The first target is a safe compatibility path that can run existing DSView devices unchanged and can add a small allowlist of upstream logic-analyzer or oscilloscope drivers.

## Current State

DSView currently vendors a heavily modified and trimmed `libsigrok` tree:

- Local hardware drivers are limited to `DSL`, `pxlogic`, `demo`, and `common`.
- Local decode support is already broad: `libsigrokdecode/decoders` contains 222 decoder directories.
- PXTOOL UI talks through DSView-specific APIs such as `ds_get_device_list()`, `ds_active_device()`, `ds_start_collect()`, `ds_stop_collect()`, and `ds_get_actived_device_config()`.
- The UI and device layer depend on DSView-specific concepts including `ds_device_handle`, active-device global state, custom `SR_CONF_*` keys, hardware modes, stream mode, advanced trigger capability, and DS-specific channel/session behavior.

The local upstream reference is `/Users/yuanji/Desktop/project/libsigrok`:

- Upstream hardware drivers live in `src/hardware`.
- The current upstream tree has 87 hardware driver directories.
- Upstream drivers depend on shared infrastructure such as `usb.c`, `serial*.c`, `scpi`, `modbus`, `ezusb`, `std.c`, and optional libraries such as libserialport, hidapi, libftdi, libusb, libzip, and minilzo.
- Upstream uses standard `sr_*` APIs and a different core model from DSView's local `ds_*` facade.

Conclusion: DSView is far from upstream in the hardware layer, but not primarily in the decode layer.

## Non-Goals

- Do not directly replace DSView's entire `libsigrok/` directory with upstream in one step.
- Do not rewrite the PXTOOL UI as part of the first migration.
- Do not promise full support for all 87 upstream hardware drivers in the first phase.
- Do not remove DSView-specific `SR_CONF_*` keys until equivalent capability-based behavior exists.
- Do not change DSLogic, DSCope, or PXLogic behavior as a side effect of adding upstream drivers.

## Recommended Approach

Use a staged compatibility architecture:

```text
PXTOOL UI
  |
DeviceAgent / SigSession / existing docks and toolbars
  |
DSView ds_* compatibility API
  |
Device manager + config mapper + capability mapper
  |
Upstream-style libsigrok core
  |
DSView native drivers       Upstream allowlisted drivers
DSL / pxlogic / demo        fx2lafw / hantek / rigol / ...
```

The key decision is to keep `ds_*` as the stable frontend API during the migration. Internally, `ds_*` becomes a facade over a device manager that can host both DSView-native drivers and upstream-style drivers.

## Architecture

### 1. DSView Compatibility Facade

Keep the public DSView API that PXTOOL already uses:

- `ds_get_device_list()`
- `ds_active_device()`
- `ds_get_actived_device_info()`
- `ds_get_actived_device_config()`
- `ds_get_actived_device_config_list()`
- `ds_start_collect()`
- `ds_stop_collect()`
- channel enable/name APIs

The facade should hide whether a device came from a DSView-native driver or an upstream driver. This keeps UI churn small and provides a stable checkpoint after each internal change.

### 2. Unified Device Manager

Introduce an internal device manager model behind the facade. It should track:

- device handle
- source kind: DSView-native, upstream, file, or demo
- driver pointer
- device instance pointer
- current active state
- device capabilities
- scan/open/acquire lifecycle state

The manager should preserve DSView's current "active device" behavior, but avoid baking DSLogic-specific assumptions into the new upstream path.

### 3. Driver Registry

Keep DSView-native drivers registered explicitly:

- `DSLogic_driver_info`
- `DSCope_driver_info`
- `px_driver_test_info`
- local demo/session drivers

Add an upstream-driver registry with an allowlist. The first allowlist should be small and biased toward waveform-producing devices:

- `fx2lafw` or another simple logic analyzer driver
- one Hantek logic/oscilloscope class driver if dependencies are manageable
- one SCPI oscilloscope class such as `rigol-ds` after SCPI helpers are available
- upstream `demo` or a test-like driver only if useful for CI-style validation

The allowlist keeps build, packaging, and runtime behavior controllable.

### 4. Config and Capability Mapper

This is the most important compatibility piece.

DSView currently uses many local `SR_CONF_*` keys. Some overlap conceptually with upstream keys and some are DSView-specific. The mapper should classify every key used by PXTOOL:

- standard upstream key with matching semantics
- standard upstream key with type or unit differences
- DSView-only key supported only by native drivers
- unsupported key for a given upstream device

The UI should increasingly ask "does this device support this capability?" instead of checking driver names. Examples:

- show samplerate controls only if `SR_CONF_SAMPLERATE` is listed in device options
- show sample-limit controls only if `SR_CONF_LIMIT_SAMPLES` is supported
- show advanced trigger only if the device explicitly reports DSView advanced trigger support
- show stream/disk-cache controls only for DSView-native devices that support them
- avoid treating DMMs, power supplies, and electronic loads as waveform devices

### 5. Datafeed Adapter

The first upstream devices should produce data compatible with DSView's existing waveform path:

- `SR_DF_LOGIC`
- `SR_DF_ANALOG`
- `SR_DF_TRIGGER`
- `SR_DF_END`
- relevant metadata such as samplerate

If upstream packet structures differ, add a narrow adapter at the session/datafeed boundary rather than modifying the UI data model first.

### 6. Build Integration

DSView currently compiles vendored `libsigrok` sources directly from CMake. Upstream libsigrok uses autotools and includes more source modules and optional dependencies.

The first phase should keep DSView's CMake build and add only the upstream source modules required by the first allowlisted drivers. The build should expose feature flags for upstream driver groups and fail clearly when a required dependency is missing.

The implementation should not rely on `/Users/yuanji/Desktop/project/libsigrok` at runtime. That path is a reference source during development, not an application dependency.

### 7. Imported Source File Headers

When copying source files from upstream libsigrok into DSView, normalize each copied file's header to the DSView/PXTOOL copyright and license style used by nearby files. Do not paste upstream libsigrok headers verbatim into the DSView tree. Keep the resulting header consistent with the target directory's existing GPL notice style.

## Migration Phases

### Phase 0: Baseline Inventory

Produce a local compatibility table:

- DSView `SR_CONF_*` keys used by PXTOOL
- DSView `struct sr_dev_driver` and `struct sr_dev_inst` fields
- upstream equivalents or missing fields
- source files required by candidate upstream drivers
- dependency list per candidate driver
- header normalization requirements for any source files copied from upstream

Exit criteria:

- no code behavior changes
- clear allowlist for Phase 1
- known API and struct deltas documented

### Phase 1: Compatibility Boundary

Refactor behind the existing `ds_*` API so DSView-native devices still behave the same but the internals can represent multiple driver source kinds.

Exit criteria:

- DSLogic/DSCope/PXLogic still scan, open, configure, acquire, stop, and release
- demo/file sessions still work
- no UI-visible behavior changes expected

### Phase 2: Upstream Core Helpers

Bring in the minimal upstream core/helper modules needed for the first allowlisted driver set.

Candidate helpers include:

- `usb.c`
- `ezusb.c`
- `serial*.c` as needed
- `std.c`
- `scpi` helpers if an SCPI oscilloscope is selected
- `modbus` helpers only if a selected driver requires them

Exit criteria:

- DSView builds with the helper set enabled
- no selected helper changes DSView-native device behavior
- dependencies are detected and reported through CMake

### Phase 3: First Upstream Driver

Add one simple upstream waveform-producing driver through the allowlist and adapter.

Exit criteria:

- the driver appears in device scan results when the dependency/device is present
- open/close works
- basic config list/get/set works for supported keys
- acquisition sends data into DSView's existing waveform path
- unsupported DSView-only controls are hidden or disabled

### Phase 4: Expand Driver Classes

Add additional drivers in small batches by class:

- USB logic analyzers
- USB oscilloscopes
- SCPI oscilloscopes
- later: non-waveform instruments if DSView gains suitable views

Exit criteria per batch:

- build remains deterministic
- device capability reporting is correct
- unsupported UI paths do not appear
- DSView-native regression suite still passes

## Testing Strategy

Testing should scale with the risk of each phase:

- Build tests for default DSView build and upstream-driver-enabled build.
- Unit tests for config/capability mapper behavior.
- Device-manager tests for active-device switching, remove/re-scan, and handle stability.
- Regression tests for DSView-native DSLogic/DSCope/PXLogic paths where hardware-independent coverage is possible.
- Session/datafeed tests using demo or recorded input to verify `SR_DF_LOGIC`, `SR_DF_ANALOG`, trigger, and end packets.
- Manual hardware smoke tests for each newly allowlisted upstream driver.

## Risks and Mitigations

Risk: upstream and DSView structs have diverged.

Mitigation: isolate struct translation in the compatibility layer. Avoid exposing upstream-only structs directly to PXTOOL until the UI is ready.

Risk: upstream drivers require many optional dependencies.

Mitigation: use an allowlist and dependency gates. Add only the helpers and libraries required by selected drivers.

Risk: UI assumes every hardware device is DSLogic-like.

Mitigation: introduce explicit capabilities and hide unsupported controls. Keep DSView-private features limited to native drivers.

Risk: DSView-native behavior regresses.

Mitigation: keep native drivers registered and exercised throughout the migration. Do not rewrite DS/PX drivers during early phases.

Risk: many upstream devices are not waveform devices.

Mitigation: first support waveform-producing devices only. Treat DMMs, power supplies, loads, and signal generators as later UI work.

## Success Criteria

The migration is successful when:

- existing DSView devices continue to work without user-visible regressions
- DSView can enable at least one upstream waveform-producing driver through an allowlist
- that upstream device can scan, open, configure, acquire, stop, and render data in the existing UI
- unsupported controls are not shown for devices that do not support them
- adding the next upstream driver is mostly a driver/dependency/mapper task, not a UI rewrite

## Recommended First Implementation Slice

The first implementation plan should target:

1. inventory and compatibility table
2. internal device source-kind model behind `ds_*`
3. capability mapper for the controls PXTOOL already queries
4. allowlist mechanism in CMake
5. one upstream waveform driver proof of concept

This gives DSView a controlled bridge toward upstream while protecting the current DSLogic, DSCope, PXLogic, decode, and session workflows.
