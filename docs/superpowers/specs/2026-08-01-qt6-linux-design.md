# PXTOOL Qt6-Only Migration Design

Date: 2026-08-01
Branch: `upgrade-to-qt6-linux`

## Objective

Make PXTOOL Qt6-only on every supported desktop platform, with particular
focus on the Linux build and packaging path. Linux must configure and link
against Qt6 even when a stale Qt5 CMake cache or tool entry exists. Generated
artifacts must not contain Qt5 runtime dependencies.

Project history documents such as `NEWS25`, `NEWS31`, `INSTALL`, and the
theme README are outside the migration scope and will remain unchanged.

## Current State

- The top-level CMake file still probes Qt5 on non-Windows/non-macOS systems
  and conditionally selects Qt5 or Qt6.
- Translation, MOC, UIC, and RCC generation still have Qt5 branches.
- C++ sources retain compatibility branches for Qt5 APIs including high-DPI
  attributes, text encoding, font metrics, wheel events, regular expressions,
  QVariant metatypes, native events, screenshots, and enter events.
- `PXTOOL/test/CMakeLists.txt` still conditionally uses Qt5 resources.
- `scripts/linux/package-linux.sh` declares Qt5 runtime dependencies.
- Existing Linux CMake caches in `build/` and `build-tests/` resolve Qt5.
- Windows and macOS already build with Qt6, but Windows scripts contain
  Qt5-specific cleanup and detection paths that should become Qt6-only or
  generic Qt artifact validation.
- The development machine has Qt6 6.10.2 installed alongside Qt5 5.15.18.
  `qmake` and `lrelease` currently resolve through `qtchooser` to Qt5, while
  `qmake6` and `/usr/lib/qt6/bin/lrelease` resolve to Qt6.

## Design

### 1. Qt6-only CMake configuration

Replace the current Qt5/Qt6 selection block with one required Qt6 lookup:

- Find Qt6 `Core`, `Gui`, `Widgets`, `Svg`, and `Network`.
- Add Qt6 `DBus` on macOS, preserving the existing platform behavior.
- Remove `QT_VERSION_FORCE`, all `Qt5Core_FOUND` checks, Qt5 imported
  targets, Qt5 include/definition variables, and the Qt5 fallback error text.
- Use only Qt6 targets in `QT_LIBRARIES`.
- Use only `Qt6::LinguistTools`, `qt6_add_translation`, `qt6_wrap_cpp`,
  `qt6_wrap_ui`, and `qt6_add_resources`.
- Keep the existing project-wide C++17 and platform-specific build settings.

Update `PXTOOL/test/CMakeLists.txt` to unconditionally use
`qt6_add_resources`.

The CMake configure step should fail clearly when Qt6 is not available and
should report the Qt6 include/package location used for the build.

### 2. Qt6-only C++ sources

Remove Qt version preprocessor branches and retain the current Qt6 code path.
The following API updates are included:

- Use Qt6 high-DPI behavior and rounding policy without Qt5-only attributes.
- Use `QStandardPaths::AppDataLocation` for user data and profiles.
- Use `QStringConverter::Utf8` with `QTextStream::setEncoding`; remove
  `QTextCodec` code.
- Use `QFontMetrics::horizontalAdvance`.
- Use `QWheelEvent::position()` and `angleDelta()`.
- Use `QRegularExpression` and `QRegularExpressionValidator`.
- Use `QVariant::metaType().id()`.
- Use `QStyleOption::initFrom`.
- Use Qt6 screen-grab APIs.
- Use the Qt6 `nativeEvent` signature with `qintptr *`.
- Use `QEnterEvent` for widget enter events.
- Use the Qt6 `QSignalMapper::mappedObject` signal.

Only Qt version compatibility code is removed. Existing operating-system
branches for Windows, Linux, and macOS remain intact, and business logic,
device protocols, resource layout, and UI behavior are not intentionally
changed.

### 3. Linux Qt6 build environment

Add shared shell logic for `scripts/linux/build_and_run.sh` and
`scripts/linux/package-linux.sh` to establish and validate a Qt6 environment:

- Resolve Qt6 using `qmake6 -query`.
- Derive the Qt6 tools directory and Qt6 CMake package directory from that
  query rather than assuming the system `qmake` selector is correct.
- Prepend the Qt6 tools directory to the script-local `PATH`.
- Validate `qmake6`, Qt6 `lrelease`, and `Qt6Config.cmake` before configuring.
- Pass the resolved Qt6 CMake directory to CMake explicitly.

When a script-managed build cache contains stale Qt entries from an older
configuration, remove that generated build directory and configure it again
with Qt6. This ensures stale Qt5 object files cannot be reused. Source files
and user-authored files outside the generated build directory are untouched.

### 4. Packaging and deployment

#### Linux

Update `scripts/linux/package-linux.sh` so its Debian control file depends on
Qt6 runtime libraries. Use Debian dependency alternatives where necessary for
the core library name, allowing the current Ubuntu 26.04 `libqt6core6t64` and
older distributions' `libqt6core6` package. The other required components are
Qt6 GUI, widgets, network, and SVG runtime packages.

After installation into the staging root and before `dpkg-deb --build`:

- Verify the staged executable exists.
- Inspect its ELF `NEEDED` entries.
- Require Qt6 runtime dependencies.
- Reject any Qt runtime dependency that is not Qt6.

The script must stop before producing a package if these checks fail.

#### Windows and macOS

Keep the existing Qt6 deployment tools and paths. Remove Qt5-specific
operations from active build/deployment scripts:

- Windows cleanup uses a Qt6 whitelist or generic Qt artifact scan rather than
  treating Qt5 as a package to copy or deploy.
- Windows deployment continues to require `windeployqt6.exe`, Qt6 plugins,
  and Qt6 imports.
- macOS packaging verifies that `macdeployqt` is the Qt6 tool before bundling
  frameworks.

All deployment scripts must only install, copy, or accept Qt6 runtime files.

### 5. Machine toolchain migration

On the current Ubuntu 26.04 machine:

1. Confirm the required Qt6 development, SVG, and translation/tool packages
   are installed.
2. Remove installed Qt5 development packages, tools, and runtime libraries,
   including packages pulled in solely by the Qt5 toolchain. Resolve the
   installed package list explicitly from `dpkg-query`, covering the Qt5
   package families (`libqt5*`, `qt5*`, `qtbase5*`, `qttools5*`,
   `qttranslations5*`, `qtwayland5*`, Qt5 QML modules, and Qt5-named tools).
3. Remove or disable the Qt5 `qtchooser` default selection. Use `qmake6` and
   the Qt6 tool directory explicitly; no build script may rely on the old
   `qmake` or `lrelease` default selector.
4. Verify that Qt5 packages are no longer installed and that `qmake6`, Qt6
   `lrelease`, and the Qt6 CMake package are usable.

Qt6 packages are installed before Qt5 removal so the build environment does
not lose its required development tools during the transition. The removal
operation is first run with `apt-get -s` and reviewed as an exact package
list. Only the resolved Qt5 package list and dependencies identified as
Qt5-only are removed; an unrestricted `apt autoremove` is not used.

## Error Handling

- CMake fails at configure time if Qt6 components are missing.
- Linux scripts fail if `qmake6`, Qt6 `lrelease`, or the Qt6 CMake package
  cannot be resolved.
- Linux scripts rebuild from a clean generated cache when a stale Qt cache is
  detected.
- Packaging fails if the staged executable is missing, if its dynamic
  dependencies are not Qt6-only, or if the Debian control file cannot be
  created.
- Windows and macOS deployment scripts fail when the required Qt6 deployment
  tool or Qt6 runtime files are unavailable.

No script silently falls back to a system Qt selector or to a different Qt
major version.

## Verification Plan

### Static checks

Search active source, CMake, test CMake, and build/deployment scripts for
Qt5 imports, targets, version guards, headers, and package dependencies.
Historical release/install/theme documentation is excluded from this scan.

### Build checks

- Configure a clean Qt6 Linux build.
- Build PXTOOL and the C decoder targets.
- Configure and build the test target with Qt6.
- Run the enabled CTest suite.
- Confirm the executable reports Qt6 at runtime where the existing diagnostic
  output exposes the Qt version.

### Package checks

- Run `scripts/linux/package-linux.sh`.
- Inspect the generated Debian control metadata for Qt6-only dependencies.
- Inspect the staged and packaged executable ELF dependencies for Qt6-only
  Qt linkage.
- Confirm the expected web UI, resources, Python decoders, and C decoders
  remain present.

### System checks

- Confirm no installed package matching the Qt5 toolchain/runtime family
  remains.
- Confirm `qmake6 -v` reports Qt6.
- Confirm Qt6 `lrelease -version` reports Qt6.
- Confirm CMake resolves `Qt6Config.cmake` and no Qt5 cache entries remain in
  the script-managed build trees.

## Scope Boundaries

Included:

- Qt6-only C++ source compatibility cleanup.
- Top-level and test CMake changes.
- Linux build and Debian packaging scripts.
- Active Windows/macOS deployment script cleanup needed to enforce Qt6-only
  artifacts.
- Current machine Qt5 removal and Qt6 toolchain validation.

Excluded:

- Historical release notes and installation/theme documentation.
- Unrelated CMake or source refactoring.
- Changes to device behavior, decoder behavior, resource contents, or UI
  design unrelated to Qt6 API compatibility.
