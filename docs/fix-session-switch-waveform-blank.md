# Bug Fix: Waveform Blank After Switching Back to Virtual-Demo Session

## Reproduction Steps

1. Open DSView with the virtual-demo session, click **Start** → waveforms display normally.
2. Click **+** to create a new session (automatically switches to the new session).
3. Switch back to the original virtual-demo session → **waveforms are blank (not rendered)**.

---

## Root Cause

### Chain of events

When the new session (session 1) is initialized, it calls:

```
set_default_device()
  → ds_active_device(v_demo_handle)
    → open_device_instance()
      → hw_dev_open()
        → load_virtual_device_session()
          → sr_dev_probes_free(sdi)   ← frees every sr_channel* struct
          → allocates brand-new sr_channel structs
```

Both sessions share the **same** `sr_dev_inst*` (the single virtual-demo device). Opening the device for session 1 causes libsigrok to call `sr_dev_probes_free()`, which `g_free()`s every `sr_channel` object.

Session 0's `LogicSignal` objects each hold a `sr_channel *_probe` pointer that was valid when the signal was created. After `sr_dev_probes_free()` runs, all of those pointers point to **freed memory** (dangling pointers).

### Why the screen goes blank

`paintSignals()` in `viewport.cpp` iterates all traces and calls `t->enabled()` before painting each one:

```cpp
for (auto t : traces) {
    if (t->enabled()) {          // reads _probe->enabled from freed memory
        logic_signal->paint_mid_align_sample(...);
    }
}
```

`Signal::enabled()` returns `_probe->enabled`. Reading from freed memory is undefined behavior; in practice it returns `0` (false) for all signals. Every signal is treated as disabled, so `paintSignals()` renders nothing — blank screen.

---

## Fix

### 1. `DSView/pv/view/signal.h`

Made `_probe` non-const and added a `set_probe()` setter so the pointer can be refreshed after a device reopen:

```cpp
// Added:
inline void set_probe(sr_channel *probe) { _probe = probe; }

// Changed:
sr_channel *_probe;   // was: sr_channel *const _probe;
```

### 2. `DSView/pv/sigsession.h`

Declared the new helper method:

```cpp
void refresh_signal_probes();
```

### 3. `DSView/pv/sigsession.cpp`

Implemented `refresh_signal_probes()` — walks the device's new channel list and patches each signal's `_probe` to the freshly-allocated `sr_channel` with the matching index:

```cpp
void SigSession::refresh_signal_probes()
{
    const GSList *channels = _device_agent.get_channels();
    for (auto sig : _signals) {
        int target_index = sig->get_index();
        for (const GSList *l = channels; l; l = l->next) {
            sr_channel *probe = (sr_channel *)l->data;
            if (probe->index == target_index) {
                sig->set_probe(probe);
                break;
            }
        }
    }
}
```

Also updated `rebind_device()` to call it immediately after the device is reopened:

```cpp
if (ds_active_device(handle) == SR_OK) {
    _device_agent.update();
    refresh_signal_probes();   // ← NEW
}
```

### 4. `DSView/pv/mainwindow.cpp` — `switch_to_session()`

Previously `rebind_device()` was only called when the session had **no** data. Changed it to **always** call `rebind_device()` when `saved_handle != NULL_HANDLE`, so probes are refreshed before any rendering attempt:

```cpp
// Before:
if (_session->have_view_data()) {
    /* render */
} else {
    _session->rebind_device(newItem.saved_handle);
}

// After:
_session->rebind_device(newItem.saved_handle);   // always refresh probes first
if (_session->have_view_data()) {
    /* render */
}
```

---

## Summary

| | Detail |
|---|---|
| **Symptom** | Waveforms blank after switching back to a session that has captured data |
| **Root cause** | `sr_dev_probes_free()` frees all `sr_channel` structs when another session reopens the shared virtual-demo device; existing `Signal::_probe` pointers become dangling |
| **Failure point** | `Signal::enabled()` reads `_probe->enabled` from freed memory → returns false → `paintSignals()` skips every signal |
| **Fix** | After reopening the device, walk the new channel list and patch each signal's `_probe` to the newly allocated `sr_channel` with the same index |
| **Files changed** | `view/signal.h`, `sigsession.h`, `sigsession.cpp`, `mainwindow.cpp` |
