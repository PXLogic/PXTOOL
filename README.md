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

## Documentation

- [PXTOOL User Manual (English)](PXTOOL_User_Manual_en.md)
- [PXTOOL 用户手册（中文）](PXTOOL_User_Manual_zh.md)

## Build Notes

Run the platform commands from the PXTOOL repository root.

### Linux (Debian/Ubuntu)

Install the build dependencies:

```bash
sudo apt update
sudo apt install git gcc g++ make cmake pkg-config \
  qt6-base-dev qt6-svg-dev qt6-tools-dev qt6-l10n-tools \
  libglib2.0-dev zlib1g-dev libusb-1.0-0-dev libboost-dev \
  libfftw3-dev python3-dev libudev-dev \
  libgl1-mesa-dev libxkbcommon-dev libvulkan-dev
```

Build and run PXTOOL with:

```bash
bash scripts/linux/build_and_run.sh
```

To create a Debian package instead, run:

```bash
bash scripts/linux/package-linux.sh
```

The Qt6 development packages are required even when the Qt runtime libraries
are already installed. In particular, `qt6-svg-dev` provides the `Qt6Svg` CMake
configuration and `qt6-tools-dev` provides `Qt6LinguistTools`.

### Windows (MSYS2 MinGW64)

Install the Qt6 MinGW64 toolchain and dependencies in MSYS2, then run the
incremental build and deployment command from a Windows command prompt:

```bat
scripts\windows\BUILD.bat
```

For a clean rebuild followed by deployment and release ZIP creation, run:

```bat
scripts\windows\FULL_BUILD.bat
```

### macOS

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
