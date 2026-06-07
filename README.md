![DreamSourceLab Logo](DSView/icons/dsl_logo.svg)

# PXTOOL

PXTOOL is the current product name for this DreamSourceLab desktop application. It provides a Qt-based GUI for DreamSourceLab instruments, including logic analyzers, oscilloscopes, and related capture/analysis workflows.

The project is based on the [sigrok](https://sigrok.org) ecosystem and continues to use a number of historical `DSView` names internally for source directories, build targets, and resource paths. On GitHub, this repository now documents the application as **PXTOOL** to match the current product branding used by the app itself.

## Features

- Device control and data acquisition for DreamSourceLab instruments
- Signal visualization and measurement
- Protocol decoding workflows built on the sigrok stack
- Cross-platform Qt application with packaging support for Windows, Linux, and macOS

## Build Notes

The CMake project still uses `project(DSView)` internally, while the packaged application title is `PXTOOL`.

Typical local build flow:

```bash
cmake -S . -B build
cmake --build build
```

## Project Status

PXTOOL is actively developed from the long-running DSView codebase. Some repository paths and filenames intentionally keep their historical names for compatibility, but the user-facing application title has been updated to `PXTOOL`.

## Useful Links

- [DreamSourceLab](https://www.dreamsourcelab.com)
- [sigrok](https://sigrok.org)

## License

This project is licensed under the GNU General Public License, version 3 or later.

Some individual source files may carry GPLv2+ or GPLv3+ notices, but the program as a whole is distributed under GPLv3+ terms. See the source files and [LICENSE](LICENSE) for details.
