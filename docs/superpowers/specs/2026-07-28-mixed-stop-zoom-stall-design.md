# Mixed Signal Stop Zoom Stall Design

## Problem

In mixed signal mode, the viewport can block for several seconds when the
user zooms immediately after stopping a capture. The GUI paint path calls
`SigSession::check_update()`, which previously waited for `_data_mutex` while
the data-feed callback completed a final analog packet.

## Design

`SigSession::check_update()` will attempt to acquire `_data_mutex` without
waiting. If a data-feed callback owns the mutex, the current paint event skips
the update check and returns so Qt can continue processing interaction and the
next paint event can retry.

The mixed logic rendering path will keep its cached pixmap valid for the
current device-pixel ratio and invalidate it when realtime data arrives. This
keeps zoom and post-stop redraws correct without changing capture semantics.

## Scope

Only `SigSession` update locking and `Viewport` mixed-render cache handling are
in scope. Capture lifecycle, packet processing, and unrelated rendering modes
remain unchanged.

## Verification

Build the affected DSView targets and run the existing test suite. Manually
verify: select mixed signal demo mode, start capture, wait for waveforms, stop,
then repeatedly zoom immediately. Zoom input must remain responsive while the
final data packet completes, and both logic and analog traces must redraw.
