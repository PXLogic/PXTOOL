# fx2lafw Hardware Smoke Test

Date: 2026-07-11

## Preconditions

- Build with `DSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON`.
- Configure the active build tree before launching:
  `cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON`
- `scripts/macOS/build_and_run.sh` uses the existing CMake cache; run the
  configure command above first so the fx2lafw gate is enabled.
- Use firmware-loaded fx2lafw-compatible hardware for open/close validation.
- For bootloader-state validation, place licensed firmware files under
  `PXTOOL/res/fx2lafw/` before building or launching DSView.
- Do not proceed to acquisition testing until open/close passes.

## Firmware-Loaded Device

1. Start DSView from the build produced by:
   `scripts/macOS/build_and_run.sh`
2. Confirm the fx2lafw-compatible device appears in the device list.
3. Activate the device.
4. Confirm open succeeds and the device remains stable.
5. Switch away from the device or close DSView.
6. Confirm close does not hang or leave the device unavailable to a second scan.

## Bootloader-State Device

1. Remove `PXTOOL/res/fx2lafw/<firmware>.fw` and the matching active app
   bundle firmware file:
   `build.macOS/PXTOOL.app/Contents/Resources/share/PXTOOL/res/fx2lafw/<firmware>.fw`.
   Alternatively, clean
   `build.macOS/PXTOOL.app/Contents/Resources/share/PXTOOL/res/fx2lafw/`
   before rebuilding or launching.
2. Confirm scan logs the exact missing firmware path.
3. Add the licensed matching firmware file under `PXTOOL/res/fx2lafw/`.
4. Rebuild or ensure the app bundle contains `share/PXTOOL/res/fx2lafw/<firmware>.fw`.
5. Start DSView and scan again.
6. Confirm firmware upload is attempted.
7. Confirm the device re-enumerates on the same USB bus.
8. Activate the device.
9. Confirm open, firmware version validation, and close succeed.

## Acquisition

1. Complete either the firmware-loaded or bootloader-state open/close checklist above.
2. Keep all logic channels enabled for the first run.
3. Set a low samplerate such as `1 MHz`.
4. Set a finite sample limit such as `100000`.
5. Start acquisition.
6. Confirm DSView shows changing logic waveform data.
7. Stop acquisition from the UI.
8. Confirm stop returns promptly and exactly one acquisition end is observed in logs.
9. Start acquisition a second time without restarting DSView.
10. Confirm the second run starts, streams, stops, and closes cleanly.
11. For 16-channel profiles, enable at least one channel above `D7` and repeat
    a finite acquisition to exercise 16-bit transfer units.
12. During a controlled test only, unplug during acquisition and confirm DSView
    does not hang and the device can be scanned again after reconnect.

## Stop Conditions

- Missing firmware path is wrong.
- Upload succeeds but re-enumeration opens a different same-VID/PID device.
- Open succeeds but close hangs or leaves the handle claimed.
- Acquisition start succeeds but no `SR_DF_LOGIC` data reaches the waveform view.
- Stop does not return promptly.
- A second acquisition run fails after the first run stopped cleanly.
- Unplug during acquisition hangs the session thread.
- Any DSLogic, DSCope, PXLogic, demo, file, decode, or session behavior regresses.
