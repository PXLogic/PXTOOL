# PXTOOL

PXTOOL is a cross-platform Qt desktop application for signal capture,
visualization, measurement, and protocol decoding workflows.

The codebase includes the application sources under `PXTOOL/`, the bundled
sigrok core under `libsigrok/`, and the bundled protocol decoder runtime and
Python decoders under `libsigrokdecode/`.

## Features

- Device connection and capture control
- Digital and analog waveform visualization
- Measurement, search, export, and session workflows
- Protocol decoding based on the bundled sigrok decoder stack
- Packaging support for Windows, Linux, and macOS

## Build Notes

On macOS, the local app bundle is generated at:

```text
build.macOS/PXTOOL.app
```

For macOS development builds, use:

```bash
bash scripts/macOS/build_and_run.sh
```

For a distributable macOS app bundle or DMG, use:

```bash
bash scripts/macOS/package-macos.sh
```

Pass `--no-dmg` to skip DMG creation, or `--skip-build` to package an existing
build.

Non-build maintenance helpers, such as translation utilities and icon
regeneration, live under `scripts/misc/`.

## Project Layout

- `PXTOOL/` - Qt application sources and resources
- `libsigrok/` - bundled sigrok core
- `libsigrokdecode/` - bundled protocol decoder runtime and Python decoders
- `pv/cdecoders/` - bundled C decoder examples/modules
- `scripts/` - platform build, run, and packaging helpers

## License

This project is licensed under the GNU General Public License, version 3 or
later. Some individual source files may carry GPLv2+ or GPLv3+ notices. See the
source files and [LICENSE](LICENSE) for details.
