# PXTOOL Data Acquisition Design

## Goal

Implement a Data Acquisition page in PXTOOL for devices that expose
`SR_CHANNEL_ANALOG`. The page is selected from the existing top-left device
mode selector alongside Logic Analyzer and Oscilloscope. It must support raw
analog capture, display, persistence, export, and conversion of analog inputs
to derived logic inputs for protocol decoders.

The design uses PXView's device-mode and hardware-control model as the
integration base. It borrows the proven PulseView concepts for analog display
and analog-to-logic conversion, but does not import PulseView's separate
per-channel session architecture.

## User Flow

1. Device discovery reads its actual channel list.
2. The top-left selector shows Data Acquisition only when an
   `SR_CHANNEL_ANALOG` channel is available.
3. Selecting Data Acquisition stops any active capture, saves the current
   session state, requests `SR_CONF_DEVICE_MODE = ANALOG`, then rebuilds the
   visible channel model and analog controls.
4. The user configures available sample-rate, depth, and device/channel
   settings, then starts capture.
5. Analog waveforms are shown in the trace view. A channel can be set to
   automatic range or configured with a manual V/div scale.
6. A channel can optionally produce a derived logic trace using either a
   threshold or Schmitt-trigger conversion. Protocol decoders use only this
   derived trace, while the original analog trace remains available.

## Architecture

### Mode and Device Boundaries

- `DevMode` owns only the selector UI and emits a request to change mode.
- `SigSession` owns the mode-switch transaction: stop capture, retain session
  configuration, change device mode, rebuild channels and view data, and
  report failures without leaving partially initialized traces.
- `DeviceAgent` is the sole source of supported modes. It derives the list
  from exposed `SR_CHANNEL_LOGIC`, `SR_CHANNEL_ANALOG`, and `SR_CHANNEL_DSO`
  channel types and owns driver configuration access.
- `SamplingBar` and device options present only controls supported by the
  selected device and the active ANALOG mode. Logic-only trigger and loop
  controls are hidden in this mode.

### Analog Data Path

The capture pipeline is:

```
device -> SR_DF_ANALOG -> AnalogSnapshot -> AnalogSignal
```

`AnalogSnapshot` remains PXTOOL's raw-sample owner. It must preserve channel
mapping, units, encoding information, sample range, and envelope data. It
accepts standard libsigrok floating-point analog packets as well as the
integer ADC packets required by PX hardware. `AnalogSignal` renders raw data
with channel-local V/div, vertical grid, auto-ranging, hover measurement, and
zoom-dependent envelope rendering.

Raw analog samples are immutable from the perspective of derived processing.

### Analog-to-Logic Conversion

For each analog input, the UI can select:

- no conversion;
- single-threshold conversion; or
- Schmitt-trigger conversion with low and high thresholds.

The conversion creates a separate `AnalogToLogicSignal` backed by the selected
analog source and conversion configuration. It is invalidated and regenerated
when its source data or configuration changes. It does not modify the analog
snapshot.

The pipeline becomes:

```
AnalogSnapshot -> AnalogSignal
               -> derived logic signal -> protocol decoder -> decode traces
```

Only the derived logic signal is offered as a protocol-decoder input. The raw
analog trace, derived logic trace, and decoder output can coexist and be
shown or hidden independently.

## Persistence and Export

Session persistence stores raw analog data, channel metadata, and conversion
configuration. Derived logic samples are regenerated after load rather than
serialized as duplicate capture data. Export writes analog data using analog
packets and preserves correct type and channel metadata. Mixed captures retain
both analog and logic data rather than coercing one into the other.

## Failure Handling

- Do not offer ANALOG mode to devices without analog channels.
- If the driver rejects a mode change, leave the application in a usable
  previous mode and show the failure.
- Validate each analog packet's sample count, unit size, channel list, and
  interleaving before storing it.
- Treat unknown or incomplete encoding metadata as a capture error; do not
  guess whether bytes are integer or float samples.
- On capture restart, device/file close, and mode change, release analog
  snapshots, derived logic caches, decode tasks, and view objects in ownership
  order so that stale data cannot enter a new session.

## Verification

Unit tests cover integer and float packet decoding, multi-channel interleaved
data, bounded/ring storage, range/envelope calculation, threshold conversion,
Schmitt conversion, and conversion-cache invalidation.

Session tests cover start/stop, ANALOG mode switching, analog save/load,
analog export, and the absence of the selector option on logic-only devices.

UI integration tests cover selector-driven toolbar and trace refresh plus
simultaneous display of analog, derived logic, and decoder traces.

Hardware acceptance uses a device with `SR_CHANNEL_ANALOG` to verify
sampling-rate and depth controls, acquisition, V/div, auto-ranging, and
analog-to-logic protocol decoding.

## Scope Boundaries

The first implementation presents separate logic and analog pages through the
existing selector. A future MSO page may display logic and analog signals
together, but it is explicitly outside this design.
