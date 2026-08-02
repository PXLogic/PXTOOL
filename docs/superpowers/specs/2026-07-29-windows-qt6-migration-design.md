# Windows Qt 6.11.0 Migration Design

## Status and scope

This design migrates the Windows build, deployment package, and MSYS2 development environment from Qt 5 to the current MSYS2 Qt 6.11.0 packages.

Linux is explicitly out of scope. Its existing Qt5-first, Qt6-fallback behavior and its Qt5-related package documentation remain unchanged. macOS already builds with Qt6 and remains unchanged except for sharing the common Qt6 CMake branch where practical.

The completed migration must leave no Qt5 dependency in the Windows CMake configuration, build scripts, deployment directory, release ZIP, or installed MSYS2 `mingw-w64-x86_64-qt5-*` packages.

## Architecture

### Platform-specific Qt selection

`CMakeLists.txt` will have explicit platform paths:

- Windows and macOS require Qt6. Windows requires Core, Gui, Widgets, Svg, Network, and LinguistTools; macOS retains DBus.
- Linux retains the current Qt5-first / Qt6-fallback discovery and its `QT_VERSION_FORCE` compatibility behavior.
- The Qt target list remains the common input to application and test linkage. The Windows list contains only `Qt6::*` targets and never `Qt5::WinExtras`.

Qt5-specific translation, MOC, UI, and resource wrapper calls remain only inside the Linux compatibility path. Windows takes the Qt6 wrapper path unconditionally.

### Windows-native taskbar progress

Qt5's `QtWinExtras` is not supplied by the MSYS2 Qt6 repository, so `QWinTaskbarButton` and `QWinTaskbarProgress` cannot remain.

A small Windows-only wrapper will use `ITaskbarList3` from the Windows shell COM API:

- Initialize once after `MainFrame` owns a native window handle.
- For a positive progress value, call `SetProgressState(..., TBPF_NORMAL)` and `SetProgressValue(..., value, 100)`.
- For zero or no progress, call `SetProgressState(..., TBPF_NOPROGRESS)`.
- Treat COM initialization or taskbar API failures as non-fatal; application capture must continue without taskbar decoration.
- Release the COM interface during shutdown.

The wrapper preserves the existing `MainFrame::setTaskbarProgress(int)` signal/slot contract, so capture code remains unchanged. CMake links the Windows COM libraries required by MinGW.

### Qt6 source compatibility

Windows-only sources are compiled with Qt6 and must not include removed Qt5 APIs:

- Remove unused `QDesktopWidget` includes from the native-window and main-frame sources.
- Replace the Qt6 Windows screenshot branch's invalid `QApplication::desktop` use with the relevant `QScreen` (falling back to the primary screen) and `grabWindow(0, x, y, width, height)`.
- Retain existing Qt6 adaptations for wheel events, font metrics, regular expressions, string encoding, `QVariant`, enter events, and checked geometry. They already select correct Qt6 APIs.

## Toolchain and scripts

### MSYS2 packages

The supported Windows toolchain is MSYS2 MinGW64 with:

- `mingw-w64-x86_64-qt6-base`
- `mingw-w64-x86_64-qt6-svg`
- `mingw-w64-x86_64-qt6-tools`

The migration provides an explicit preparation/purge workflow. It first installs the Qt6 packages, presents the exact installed `mingw-w64-x86_64-qt5-*` package list, and only then removes that list with `pacman -Rns`. Dependency conflicts stop the operation rather than removing unrelated packages. Qt5 removal is performed only after an initial Qt6 build and test pass, followed by the final clean verification build.

### Build configuration

`scripts/windows/build_script.sh` configures CMake against the MinGW64 Qt6 prefix and checks that the resolved toolchain is Qt6. It uses `lrelease-qt6` (with only a verified Qt6 generic-name fallback) to compile translations. Its package-install guidance names the Qt6 tools package.

A stale Qt5 CMake cache triggers reconfiguration. The full rebuild entry point also removes prior Qt plugin and Qt DLL deployment artifacts so that a release cannot inherit files from a previous Qt5 build.

### Deployment

`scripts/windows/deploy_script.sh` no longer copies `share/qt5/plugins`. It removes the old `plugins/` deployment tree and Qt5 DLLs, invokes the Qt6 `windeployqt` executable for `PXTOOL.exe`, and retains the existing packaging of non-Qt MinGW DLLs, Python, decoders, resources, icon, and web UI.

Before completion, deployment scans the output for `Qt5*.dll`, Qt5 plugin paths, and Qt5 references. Any match fails deployment. The release ZIP is created only from this validated directory.

## Files and responsibilities

| Area | Planned changes |
| --- | --- |
| `CMakeLists.txt` | Explicit Windows/macOS Qt6 path; Linux compatibility path; remove Windows Qt5/WinExtras linkage; add native taskbar link dependencies. |
| `PXTOOL/pv/mainframe.*` | Replace `QWinTaskbar*` ownership and calls with the native taskbar wrapper. |
| Windows taskbar wrapper files | Encapsulate `ITaskbarList3` lifecycle and progress states behind a focused API. |
| `PXTOOL/pv/winnativewidget.cpp` | Remove obsolete Qt5-only include. |
| `PXTOOL/pv/mainwindow.cpp` | Correct Windows Qt6 screenshot capture. |
| `scripts/windows/build_script.sh` | Enforce Qt6 CMake and translation tooling; invalidate stale Qt5 configuration. |
| `scripts/windows/deploy_script.sh` | Use Qt6 deployment tooling; cleanse and validate deployment output. |
| `scripts/windows/FULL_BUILD.bat` and setup guidance | Provide clean Qt6 rebuild and explicit, reviewable Qt5 package purge. |
| `PXTOOL/test/CMakeLists.txt` | Preserve Linux compatibility while ensuring Windows resource generation resolves through Qt6. |

## Migration sequence

1. Implement the CMake Qt selection split and the Windows-native API replacements.
2. Update Windows build, deployment, and full-clean scripts for MSYS2 Qt6.
3. Install the Qt6 package set alongside Qt5 and perform a clean Qt6 configuration, build, and test run.
4. Validate a deployed application and release directory with the Qt5-residue checks.
5. Inspect and remove the exact MSYS2 Qt5 package set.
6. Delete all Windows build/deployment artifacts, rebuild, test, deploy, package, and repeat residue checks without Qt5 installed.

## Validation and acceptance criteria

Automated checks:

- CMake configuration records Qt6 targets for Windows and contains no resolved Qt5 package path.
- `ENABLE_TESTS=ON` build succeeds and `ctest --output-on-failure` passes.
- Deployment succeeds with Qt6 `windeployqt` and fails if a Qt5 DLL or plugin is present.
- `objdump -p PXTOOL.exe` or equivalent dependency inspection lists Qt6 DLLs and no Qt5 DLLs.
- `pacman -Qq` returns no `mingw-w64-x86_64-qt5-*` packages after the purge.
- The source/build-script residue scan is limited to Windows-scoped files, so Linux's intentionally retained Qt5 compatibility does not create a false failure.

Manual Windows smoke checks:

- Launch from the deployed directory; verify platform plugin loading, translations, resources, Python decoders, and web UI.
- Exercise capture progress and verify taskbar progress appears and clears correctly.
- Verify screenshot export, high-DPI scaling, and multi-monitor behavior.
- Build the final ZIP and inspect its contents for Qt5 DLLs, plugin paths, and strings.

## Risks and mitigations

- **QtWinExtras removal:** use a focused native wrapper and preserve the current slot interface; test both visible and hidden progress transitions.
- **Stale build artifacts:** force fresh CMake configuration for a Qt5 cache and cleanse plugin/DLL output before Qt6 deployment.
- **MSYS2 package dependency conflicts:** display exact removal targets and let `pacman` reject unsafe dependency removals; never remove a broad unverified directory or package group.
- **Linux regression:** keep its current discovery branch and validate it separately when its eventual migration is scoped.
