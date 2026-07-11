# fx2lafw Hardware Smoke Test

Date: 2026-07-11

## Preconditions

- Build with `DSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON`.
- Use firmware-loaded fx2lafw-compatible hardware for open/close validation.
- For bootloader-state validation, place licensed firmware files under
  `PXTOOL/res/fx2lafw/` before building or launching DSView.
- Do not proceed to acquisition testing until open/close passes.

## Firmware-Loaded Device

1. Start DSView from the build produced by:
   `script/macos/build_and_run.sh`
2. Confirm the fx2lafw-compatible device appears in the device list.
3. Activate the device.
4. Confirm open succeeds and the device remains stable.
5. Switch away from the device or close DSView.
6. Confirm close does not hang or leave the device unavailable to a second scan.

## Bootloader-State Device

1. Remove `PXTOOL/res/fx2lafw/<firmware>.fw` and start DSView.
2. Confirm scan logs the exact missing firmware path.
3. Add the licensed matching firmware file under `PXTOOL/res/fx2lafw/`.
4. Rebuild or ensure the app bundle contains `share/PXTOOL/res/fx2lafw/<firmware>.fw`.
5. Start DSView and scan again.
6. Confirm firmware upload is attempted.
7. Confirm the device re-enumerates on the same USB bus.
8. Activate the device.
9. Confirm open, firmware version validation, and close succeed.

## Stop Conditions

- Missing firmware path is wrong.
- Upload succeeds but re-enumeration opens a different same-VID/PID device.
- Open succeeds but close hangs or leaves the handle claimed.
- Any DSLogic, DSCope, PXLogic, demo, file, decode, or session behavior regresses.
