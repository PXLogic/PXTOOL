# Channel Tint Background Design

## Goal

Add the PXView-style channel tint background to DSView's waveform area. Each
row-oriented channel in the waveform should get a subtle background tint derived
from the same trace color used by the channel header swatch and arrow label.

The tint is a visual grouping aid only. It must not change signal data,
decoding, triggering, measurement, scrolling, or channel layout behavior.

## Current Behavior

Channel header colors are stored on each `Trace` as `_colour` and exposed through
`Trace::get_colour()`. `Trace::paint_label()` uses that color for the left header
swatch and right arrow label.

The waveform background is painted from `Viewport::paintEvent()` by iterating
visible traces and calling `Trace::paint_back()` or a subclass override. Current
background painting uses the Qt palette foreground/background for midlines,
grids, and scope-style graph areas. It does not apply a row tint based on the
trace color.

## Proposed Approach

Add a single helper in `Viewport` that paints a trace row tint before the trace's
existing `paint_back()` call. The helper should:

- Run only for `TIME_VIEW`.
- Skip disabled traces, traces with no rows, and invalid trace colors.
- Use `Trace::get_colour()` as the tint source.
- Compute a row rectangle from the trace vertical center and total height,
  expanded slightly by `View::SignalMargin` so adjacent row spacing is tinted in
  the same style as PXView.
- Clip the row rectangle to the viewport bounds.
- Fill the rectangle with a low-alpha color, with separate alpha values for
  light and dark themes.

The existing trace-specific `paint_back()`, `paint_mid()`, and `paint_fore()`
methods remain responsible for grid lines, waveform drawing, decode annotations,
cursor overlays, and controls.

## Scope

Included trace types:

- Logic channels.
- Decoder rows.
- Group/protocol rows.
- Other row-oriented traces shown in `TIME_VIEW` with `rows_size() > 0`.

Excluded trace types:

- FFT/Spectrum independent graph views.
- Lissajous independent graph view.
- DSO-specific full graph background.

These excluded views are not row-oriented in the PXView screenshot and already
have their own graph backgrounds where tinting could reduce contrast.

## Paint Order

`Viewport::paintEvent()` should keep the current broad order:

1. Resolve foreground/background colors from the Qt palette.
2. Get traces for the active viewport type.
3. For each trace, paint the channel tint if applicable.
4. Call the trace's existing `paint_back()`.
5. Paint signals/decodes via `paintSignals()`.
6. Paint trace foreground overlays via `paint_fore()`.
7. Paint cursors, selection, dividers, and measurement overlays as today.

This makes the tint sit below grids, waveforms, annotations, and labels.

## Theme Behavior

The tint should be subtle in both themes:

- Light theme: use a low alpha so the background reads as a pale color wash.
- Dark theme: use a slightly higher alpha so the hue remains visible on dark
  backgrounds.

The exact alpha values can be tuned during visual verification. They should
favor readability over color intensity.

## Error Handling

If a trace has no valid color, no rows, invalid layout geometry, or is outside
the visible viewport, the tint helper should do nothing. Painting should continue
with the existing `paint_back()` behavior.

## Testing

Verification should include:

- Build the Qt application or at least compile the touched sources.
- Inspect the changed paint path to confirm the tint is below existing trace
  graphics.
- Run or screenshot the app when practical and compare against the PXView
  reference in light and dark themes.
- Confirm waveform lines, decode annotations, grid lines, dividers, and header
  labels remain readable.
