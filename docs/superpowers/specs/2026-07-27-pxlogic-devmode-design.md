# PXLogic DevMode Support

## Problem

PXLogic is detected and selected successfully, but its driver does not register
the `dev_mode_list` callback. `DevMode` therefore receives an empty mode list
from `ds_get_actived_device_mode_list()` and cannot populate its mode selector.

## Scope

Add the missing PXLogic driver callback. PXLogic supports only logic analysis,
so the callback returns one `sr_dev_mode` entry for `LOGIC`.

Add a focused regression test that verifies the registered PXLogic driver
exposes a non-null mode-list callback and that its list contains only `LOGIC`.

## Out Of Scope

The saved hardware profile's `Operation Mode` setting is intentionally kept.
It selects the buffer or stream acquisition method and is separate from the UI
work mode (`DeviceMode`).

## Verification

The new test must fail before the callback is added and pass after it is
registered. The relevant existing test targets will also be run after the
change.
