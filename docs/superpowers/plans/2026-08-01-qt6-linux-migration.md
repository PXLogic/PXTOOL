# PXTOOL Qt6-Only Linux Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove Qt5 from PXTOOL's active source and build/deployment paths, make every desktop build Qt6-only, produce a Qt6-only Linux package, and switch the current machine to the Qt6 toolchain.

**Architecture:** CMake will require Qt6 directly and expose only Qt6 targets and generation helpers. A shared Linux shell helper will resolve Qt6 from `qmake6`, clean legacy CMake caches, and validate ELF dependencies; the Linux build and package scripts will source it. Existing Qt6 branches in the C++ code will become unconditional while operating-system branches remain unchanged.

**Tech Stack:** CMake, Qt 6.10, C++17, Bash, Debian `dpkg-deb`/`readelf`, MSYS2 MinGW64, macOS `macdeployqt`, CTest.

**Status (2026-08-02):** All tasks are complete. Active-source hygiene, functional testing, Qt6 toolchain checks, and Linux package validation pass. The Ubuntu host has no installed Qt5-family packages or `qtchooser`.

---

## File Map

Create:

- `scripts/linux/qt6_env.sh` - shared Qt6 discovery, build-cache cleanup, and ELF dependency validation functions.

Modify:

- `CMakeLists.txt` - require Qt6 components and use only Qt6 generation helpers.
- `PXTOOL/test/CMakeLists.txt` - use Qt6 resource generation unconditionally.
- `PXTOOL/main.cpp` - remove Qt5 high-DPI guards and obsolete attributes.
- `PXTOOL/pv/config/appconfig.cpp` - use Qt6 application data paths unconditionally.
- `PXTOOL/pv/utility/encoding.cpp` - use Qt6 UTF-8 stream encoding only.
- `PXTOOL/pv/utility/path.cpp` - remove the Qt5-specific comment from active source.
- `PXTOOL/pv/mainframe.h`, `PXTOOL/pv/mainframe.cpp` - use the Qt6 native-event signature.
- `PXTOOL/pv/mainwindow.cpp` - retain only Qt6 screenshot branches.
- `PXTOOL/pv/toolbars/titlebar.cpp` - remove the Qt5 version condition from Linux system move support.
- `PXTOOL/pv/dock/deviceoptionsdock.cpp`, `PXTOOL/pv/dock/measuredock.cpp`, `PXTOOL/pv/dialogs/deviceoptions.cpp` - use Qt6 font metrics only.
- `PXTOOL/pv/dock/keywordlineedit.cpp` - use Qt6 wheel delta only.
- `PXTOOL/pv/dock/triggerdock.cpp` - use Qt6 regular-expression headers only.
- `PXTOOL/pv/view/viewstatus.cpp` - use `QStyleOption::initFrom` only.
- `PXTOOL/pv/data/iooptions.cpp` - use `QVariant::metaType().id()` only.
- `PXTOOL/pv/view/header.cpp`, `PXTOOL/pv/view/viewport.cpp` - use Qt6 wheel position and angle APIs only.
- `PXTOOL/pv/view/edge_nav_button.h`, `PXTOOL/pv/view/edge_nav_button.cpp` - use `QEnterEvent` only.
- `PXTOOL/pv/widgets/decodermenu.cpp` - use `mappedObject(QObject*)` only.
- `scripts/linux/build_and_run.sh`, `scripts/linux/package-linux.sh` - source the Qt6 helper and pass/verify Qt6 paths and dependencies.
- `scripts/windows/build_script.sh`, `scripts/windows/deploy_script.sh`, `scripts/windows/FULL_BUILD.bat`, `scripts/windows/prepare_qt6_msys2.sh` - remove Qt5-specific operations and retain Qt6-only deployment checks.
- `scripts/macOS/package-macos.sh` - validate the selected `macdeployqt` tool is Qt6.

No project documentation files are modified. The design and plan documents are process artifacts under `docs/` and are intentionally force-added because this repository ignores that directory.

## Task 1: Add the shared Linux Qt6 environment helper

**Files:**

- Create: `scripts/linux/qt6_env.sh`

- [x] **Step 1: Create the helper with explicit Qt6 discovery and validation.**

Add this complete implementation:

```bash
#!/usr/bin/env bash

qt6_init() {
    local qmake_bin="${QT6_QMAKE_BIN:-}"
    if [ -z "${qmake_bin}" ]; then
        qmake_bin="$(command -v qmake6 2>/dev/null || true)"
    fi
    if [ -z "${qmake_bin}" ] || [ ! -x "${qmake_bin}" ]; then
        echo "ERROR: qmake6 is required to configure PXTOOL with Qt6." >&2
        return 1
    fi

    local qt_version
    qt_version="$(${qmake_bin} -query QT_VERSION 2>/dev/null || true)"
    case "${qt_version}" in
        6.*) ;;
        *)
            echo "ERROR: qmake6 resolved to an unexpected Qt version: ${qt_version:-unknown}" >&2
            return 1
            ;;
    esac

    local qt6_bin_dir
    local qt6_lib_dir
    qt6_bin_dir="$(${qmake_bin} -query QT_INSTALL_BINS 2>/dev/null || true)"
    qt6_lib_dir="$(${qmake_bin} -query QT_INSTALL_LIBS 2>/dev/null || true)"
    if [ -z "${qt6_bin_dir}" ] || [ -z "${qt6_lib_dir}" ]; then
        echo "ERROR: qmake6 did not report Qt6 tool and library directories." >&2
        return 1
    fi

    local qt6_cmake_dir="${qt6_lib_dir}/cmake/Qt6"
    if [ ! -f "${qt6_cmake_dir}/Qt6Config.cmake" ]; then
        echo "ERROR: Qt6 CMake package not found: ${qt6_cmake_dir}/Qt6Config.cmake" >&2
        return 1
    fi
    if [ ! -x "${qt6_bin_dir}/lrelease" ]; then
        echo "ERROR: Qt6 lrelease not found: ${qt6_bin_dir}/lrelease" >&2
        return 1
    fi
    if ! "${qt6_bin_dir}/lrelease" -version 2>&1 | grep -Eq 'version 6(\.|$)'; then
        echo "ERROR: lrelease is not a Qt6 tool: ${qt6_bin_dir}/lrelease" >&2
        return 1
    fi

    export QT6_QMAKE_BIN="${qmake_bin}"
    export QT6_BIN_DIR="${qt6_bin_dir}"
    export QT6_CMAKE_DIR="${qt6_cmake_dir}"
    export PATH="${QT6_BIN_DIR}:${PATH}"

    echo "Qt6 version: ${qt_version}"
    echo "Qt6 tools: ${QT6_BIN_DIR}"
    echo "Qt6 CMake: ${QT6_CMAKE_DIR}"
}

qt6_prepare_build_dir() {
    local build_dir="$1"
    local cache_file="${build_dir}/CMakeCache.txt"
    if [ ! -f "${cache_file}" ]; then
        return 0
    fi

    local non_qt6_refs
    non_qt6_refs="$(grep -Eio 'qt[0-9]+' "${cache_file}" | sort -fu | grep -Eiv '^qt6$' || true)"
    if [ -n "${non_qt6_refs}" ]; then
        echo "Legacy Qt cache entries found in ${cache_file}: ${non_qt6_refs//$'\n'/, }"
        echo "Removing the generated build directory before the Qt6 configure."
        cmake -E remove_directory "${build_dir}"
    fi
}

qt6_verify_elf_dependencies() {
    local binary="$1"
    if [ ! -x "${binary}" ]; then
        echo "ERROR: Qt6 ELF validation target is missing or not executable: ${binary}" >&2
        return 1
    fi
    if ! command -v readelf >/dev/null 2>&1; then
        echo "ERROR: readelf is required for Qt6 ELF validation." >&2
        return 1
    fi

    local needed
    needed="$(readelf -d "${binary}" | sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p')"
    local qt_needed
    qt_needed="$(printf '%s\n' "${needed}" | grep -Ei '^libqt[0-9]+' || true)"
    if [ -z "${qt_needed}" ]; then
        echo "ERROR: executable has no Qt6 runtime dependency: ${binary}" >&2
        return 1
    fi

    local non_qt6
    non_qt6="$(printf '%s\n' "${qt_needed}" | grep -Eiv '^libqt6' || true)"
    if [ -n "${non_qt6}" ]; then
        echo "ERROR: executable contains a non-Qt6 runtime dependency:" >&2
        printf '  %s\n' ${non_qt6} >&2
        return 1
    fi

    local module
    for module in Core Gui Widgets Network Svg; do
        if ! printf '%s\n' "${qt_needed}" | grep -Eqi "^libqt6${module}"; then
            echo "ERROR: executable is missing direct Qt6${module} linkage: ${binary}" >&2
            return 1
        fi
    done
}
```

- [x] **Step 2: Run shell syntax and a real Qt6 discovery smoke test.**

Run:

```bash
bash -n scripts/linux/qt6_env.sh
source scripts/linux/qt6_env.sh
qt6_init
test "${QT6_QMAKE_BIN}" = "$(command -v qmake6)"
test -f "${QT6_CMAKE_DIR}/Qt6Config.cmake"
test -x "${QT6_BIN_DIR}/lrelease"
```

Expected: `qt6_init` prints version `6.10.2`, the Qt6 tools directory, and the Qt6 CMake directory; all tests exit with status 0.

- [x] **Step 3: Commit the helper independently.**

```bash
git add scripts/linux/qt6_env.sh
git commit -m "build: add shared Linux Qt6 environment checks"
```

## Task 2: Make CMake and test resources Qt6-only

**Files:**

- Modify: `CMakeLists.txt:186-244,724-751`
- Modify: `PXTOOL/test/CMakeLists.txt:20-30`

- [x] **Step 1: Replace the Qt selection block with one required Qt6 component lookup.**

Replace the complete block beginning at `#= Qt5 or Qt6` and ending at the Qt availability error with:

```cmake
#===============================================================================
#= Qt6
#-------------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Network)

message("----- Qt6:")
message(STATUS "\t includes:" ${Qt6Core_INCLUDE_DIRS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt6Widgets_EXECUTABLE_COMPILE_FLAGS}")
set(QT_INCLUDE_DIRS
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Gui_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}
    ${Qt6Svg_INCLUDE_DIRS}
    ${Qt6Network_INCLUDE_DIRS}
)
set(QT_LIBRARIES Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Network)
add_definitions(${Qt6Gui_DEFINITIONS} ${Qt6Widgets_DEFINITIONS} ${Qt6Network_DEFINITIONS})

if(APPLE)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    list(APPEND QT_LIBRARIES Qt6::DBus)
endif()
```

This removes `QT_VERSION_FORCE`, all Qt5 discovery variables, the Qt5 target list, and the dual-version fatal error. Keep the existing include-directory and target-linking structure below this block.

- [x] **Step 2: Make translation and Qt code generation unconditional Qt6.**

Replace the conditional generation blocks with:

```cmake
# Qt Linguist: compile zh_CN.ts -> zh_CN.qm during build
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)

set(DSVIEW_TS_FILES
    PXTOOL/languages/zh_CN.ts
    PXTOOL/languages/zh_TW.ts
)
qt6_add_translation(DSVIEW_QM_FILES ${DSVIEW_TS_FILES})

qt6_wrap_cpp(DSView_HEADERS_MOC ${DSView_HEADERS})
qt6_wrap_ui(DSView_FORMS_HEADERS ${DSView_FORMS})
qt6_add_resources(DSView_RESOURCES_RCC ${DSView_RESOURCES})
```

- [x] **Step 3: Make test resource generation unconditional Qt6.**

Replace lines 24-30 of `PXTOOL/test/CMakeLists.txt` with:

```cmake
qt6_add_resources(DSVIEW_TEST_RESOURCES ${DSVIEW_TEST_QRC})
```

- [x] **Step 4: Configure a minimal clean Qt6 tree before compiling.**

Run:

```bash
QT6_CMAKE_DIR="$(qmake6 -query QT_INSTALL_LIBS)/cmake/Qt6"
cmake -S . -B build.qt6-config \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DQt6_DIR="${QT6_CMAKE_DIR}" \
  -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON
```

Expected: configure succeeds, prints the Qt6 section, and does not create any `Qt5*` CMake package entries in `build.qt6-config/CMakeCache.txt`.

- [x] **Step 5: Commit the CMake-only conversion.**

```bash
git add CMakeLists.txt PXTOOL/test/CMakeLists.txt
git commit -m "build: require Qt6 in CMake"
```

## Task 3: Remove Qt5 source compatibility branches

**Files:**

- Modify: `PXTOOL/main.cpp`
- Modify: `PXTOOL/pv/config/appconfig.cpp`
- Modify: `PXTOOL/pv/utility/encoding.cpp`
- Modify: `PXTOOL/pv/utility/path.cpp`
- Modify: `PXTOOL/pv/mainframe.h`, `PXTOOL/pv/mainframe.cpp`
- Modify: `PXTOOL/pv/mainwindow.cpp`
- Modify: `PXTOOL/pv/toolbars/titlebar.cpp`

- [x] **Step 1: Simplify startup high-DPI setup.**

In `PXTOOL/main.cpp`, remove the `bHighScale` block, the temporary `QApplication`, and the `AA_DisableHighDpiScaling`, `AA_EnableHighDpiScaling`, and `AA_UseHighDpiPixmaps` calls. Keep the fractional scale policy as an unconditional Qt6 call before constructing the final application:

```cpp
// Pass fractional scale factors through as-is so non-integer DPI screens
// (e.g. 1.5x) are not rounded, which would cause blurry text on secondary
// monitors whose scale factor differs from the primary display.
QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
    Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
```

- [x] **Step 2: Remove Qt version checks from application paths and encoding.**

Replace both `GetUserDataDir()` and `GetProfileDir()` bodies in `appconfig.cpp` with:

```cpp
return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
```

Make `encoding.cpp` contain the Qt6-only implementation:

```cpp
#include "encoding.h"

#include <QTextStream>
#include <QStringConverter>

namespace pv {
namespace encoding {

void init()
{
}

void set_utf8(QTextStream &stream)
{
    stream.setEncoding(QStringConverter::Utf8);
}

} // namespace encoding
} // namespace pv
```

Rewrite the Qt-version-specific comment in `path.cpp` without naming a Qt major/minor version; preserve the direct UTF-8 behavior.

- [x] **Step 3: Use the Qt6 native event signature consistently.**

In `mainframe.h`, replace the whole conditional declaration with:

```cpp
bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
```

In `mainframe.cpp`, replace the conditional definition with:

```cpp
bool MainFrame::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef _WIN32
    if (_parentNativeWidget != NULL) {
        MSG *msg = static_cast<MSG *>(message);
        HWND hwnd = _parentNativeWidget->Handle();

        switch (msg->message) {
        case WM_NCMOUSEMOVE:
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONUP:
        case WM_NCLBUTTONDBLCLK:
        case WM_NCHITTEST:
            *result = static_cast<qintptr>(SendMessageW(
                hwnd, msg->message, msg->wParam, msg->lParam));
            return true;
        }
    }
#endif

    return QWidget::nativeEvent(eventType, message, result);
}
```

- [x] **Step 4: Keep only Qt6 screenshot implementations.**

In `mainwindow.cpp`, remove the Qt5 alternatives under the Windows and macOS branches. Preserve the existing Qt6 statements:

```cpp
#ifdef _WIN32
    QScreen *screen = QGuiApplication::screenAt(QPoint(x, y));
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QPixmap pixmap = screen ? screen->grabWindow(0, x, y, w, h) : QPixmap();
#elif __APPLE__
    x += MainFrame::Margin;
    y += MainFrame::Margin;
    w -= MainFrame::Margin * 2;
    h -= MainFrame::Margin * 2;
    QPixmap pixmap = QGuiApplication::primaryScreen()->grabWindow(winId(), x, y, w, h);
#else
    QPixmap pixmap = QGuiApplication::primaryScreen()->grabWindow(winId());
#endif
```

- [x] **Step 5: Remove the Linux Qt version guard from title-bar movement.**

Change `#if defined(Q_OS_LINUX) && QT_VERSION >= ...` to `#ifdef Q_OS_LINUX` and remove its matching Qt-only conditional lines. Keep `startSystemMove()` and the fallback drag path unchanged. If the Qt6 compiler reports the deprecated `globalPos()` call under the project warning policy, replace it with `event->globalPosition().toPoint()` without changing the stored coordinate semantics.

- [x] **Step 6: Build the affected source group with Qt6.**

Run:

```bash
cmake --build build.qt6-config --target DSView --parallel 2
```

Expected: compilation reaches the linker without Qt5 headers, `QTextCodec`, or Qt5 native-event signature errors. Fix only Qt6 compile errors exposed by this source group before proceeding.

- [x] **Step 7: Commit the startup and platform source conversion.**

```bash
git add PXTOOL/main.cpp PXTOOL/pv/config/appconfig.cpp \
  PXTOOL/pv/utility/encoding.cpp PXTOOL/pv/utility/path.cpp \
  PXTOOL/pv/mainframe.h PXTOOL/pv/mainframe.cpp \
  PXTOOL/pv/mainwindow.cpp PXTOOL/pv/toolbars/titlebar.cpp
git commit -m "refactor: remove Qt5 source compatibility paths"
```

## Task 4: Remove Qt5 widget and event API branches

**Files:**

- Modify: `PXTOOL/pv/ui/xtoolbutton.cpp`
- Modify: `PXTOOL/pv/dock/deviceoptionsdock.cpp`
- Modify: `PXTOOL/pv/dock/measuredock.cpp`
- Modify: `PXTOOL/pv/dialogs/deviceoptions.cpp`
- Modify: `PXTOOL/pv/dock/keywordlineedit.cpp`
- Modify: `PXTOOL/pv/dock/triggerdock.cpp`
- Modify: `PXTOOL/pv/view/viewstatus.cpp`
- Modify: `PXTOOL/pv/data/iooptions.cpp`
- Modify: `PXTOOL/pv/view/header.cpp`
- Modify: `PXTOOL/pv/view/viewport.cpp`
- Modify: `PXTOOL/pv/view/edge_nav_button.h`, `PXTOOL/pv/view/edge_nav_button.cpp`
- Modify: `PXTOOL/pv/widgets/decodermenu.cpp`

- [x] **Step 1: Replace all Qt5 font metric branches.**

In `xtoolbutton.cpp`, `deviceoptionsdock.cpp`, `measuredock.cpp`, and `dialogs/deviceoptions.cpp`, remove each Qt version conditional and retain the Qt6 expression. The resulting expressions must be:

```cpp
fm.horizontalAdvance(text)
enable_all->fontMetrics().horizontalAdvance(enable_all->text())
disable_all->fontMetrics().horizontalAdvance(disable_all->text())
fm.horizontalAdvance(str)
fm.horizontalAdvance("############")
enable_all_probes->fontMetrics().horizontalAdvance(enable_all_probes->text())
disable_all_probes->fontMetrics().horizontalAdvance(disable_all_probes->text())
```

Keep each file's existing padding and width calculations exactly as written.

- [x] **Step 2: Use Qt6 wheel APIs in the numeric editor and waveform views.**

In `keywordlineedit.cpp`, replace the conditional with:

```cpp
if (event->angleDelta().y() > 0) {
    v++;
} else {
    v--;
}
```

In `header.cpp` and `viewport.cpp`, remove the old `event->x()`, `event->delta()`, `event->orientation()`, and `event->pos()` branch. Retain the current Qt6 calculation that reads `event->position()` and `event->angleDelta()`, including horizontal-versus-vertical selection and the existing macOS wheel behavior. Remove only the Qt version preprocessor wrappers around that calculation and the old branch.

- [x] **Step 3: Remove Qt5-only headers and API alternatives.**

Apply these exact Qt6-only forms:

```cpp
// triggerdock.cpp
#include <QRegularExpression>
#include <QRegularExpressionValidator>

// viewstatus.cpp
QStyleOption opt;
opt.initFrom(this);

// iooptions.cpp
int variantMetaTypeId(const QVariant &value)
{
    return value.metaType().id();
}

// decodermenu.cpp
connect(&_mapper, SIGNAL(mappedObject(QObject*)), this, SLOT(on_action(QObject*)));
```

Remove `QRegExp`, `QRegExpValidator`, `QStyleOption::init`, and `QVariant::userType()` alternatives. Do not add a new wrapper abstraction.

- [x] **Step 4: Make `EdgeNavButton` Qt6-only.**

In the header, include `<QEnterEvent>` unconditionally and declare:

```cpp
void enterEvent(QEnterEvent *event) override;
```

In the implementation, define:

```cpp
void EdgeNavButton::enterEvent(QEnterEvent *event)
{
    (void)event;
    _hovered = true;
    update();
}
```

- [x] **Step 5: Build and run the existing test target.**

Reconfigure the tree with tests enabled, then build and run it:

```bash
cmake -S . -B build.qt6-config \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DENABLE_TESTS=ON \
  -DQt6_DIR="$(qmake6 -query QT_INSTALL_LIBS)/cmake/Qt6" \
  -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON
cmake --build build.qt6-config --target DSView-test --parallel 2
ctest --test-dir build.qt6-config --output-on-failure
```

Expected: the Qt6 test executable builds and all registered tests pass. A failure caused by a removed Qt5 API is fixed in the corresponding file only; no Qt5 compatibility branch is reintroduced.

- [x] **Step 6: Commit the widget and event conversion.**

```bash
git add PXTOOL/pv/ui/xtoolbutton.cpp \
  PXTOOL/pv/dock/deviceoptionsdock.cpp PXTOOL/pv/dock/measuredock.cpp \
  PXTOOL/pv/dialogs/deviceoptions.cpp PXTOOL/pv/dock/keywordlineedit.cpp \
  PXTOOL/pv/dock/triggerdock.cpp PXTOOL/pv/view/viewstatus.cpp \
  PXTOOL/pv/data/iooptions.cpp PXTOOL/pv/view/header.cpp \
  PXTOOL/pv/view/viewport.cpp PXTOOL/pv/view/edge_nav_button.h \
  PXTOOL/pv/view/edge_nav_button.cpp PXTOOL/pv/widgets/decodermenu.cpp
git commit -m "refactor: use Qt6 widget and event APIs"
```

## Task 5: Integrate Qt6 discovery into Linux build and package scripts

**Files:**

- Modify: `scripts/linux/build_and_run.sh`
- Modify: `scripts/linux/package-linux.sh`

- [x] **Step 1: Source and initialize the Qt6 helper in both scripts.**

Immediately after each script computes `ROOT_DIR`, add:

```bash
# shellcheck source=scripts/linux/qt6_env.sh
source "${ROOT_DIR}/scripts/linux/qt6_env.sh"
qt6_init
```

Immediately before each script chooses a generator or calls `cmake`, add:

```bash
qt6_prepare_build_dir "${BUILD_DIR}"
```

This ensures a stale generated cache is removed before the new configure.

- [x] **Step 2: Pass the resolved Qt6 CMake package directory.**

Append this argument to both `CMAKE_ARGS` arrays:

```bash
-DQt6_DIR="${QT6_CMAKE_DIR}"
```

Keep the existing build types, upstream demo setting, generator selection, web UI target, runtime staging, and launch behavior.

- [x] **Step 3: Change Linux Debian dependencies to Qt6.**

Replace the `Depends:` line in `package-linux.sh` with:

```text
Depends: libc6, libstdc++6, libqt6core6t64 | libqt6core6, libqt6gui6, libqt6widgets6, libqt6network6, libqt6svg6, libglib2.0-0, libusb-1.0-0, zlib1g, libfftw3-double3
```

This keeps the current Ubuntu 26.04 core package name first and allows the older core package name as a Debian dependency alternative.

- [x] **Step 4: Validate the staged Linux executable before writing the package.**

After `cmake --install` and the existing web UI/C decoder checks, add:

```bash
STAGED_APP="${STAGE_DIR}/usr/bin/PXTOOL"
qt6_verify_elf_dependencies "${STAGED_APP}"
```

The existing `readelf`-based helper must fail before `DEBIAN/control` is written when a Qt6 component is missing or a different Qt major version is present.

- [x] **Step 5: Add Linux shell validation.**

Run:

```bash
bash -n scripts/linux/qt6_env.sh scripts/linux/build_and_run.sh scripts/linux/package-linux.sh
git diff --check
```

Expected: all commands exit 0.

- [x] **Step 6: Commit the Linux script integration.**

```bash
git add scripts/linux/qt6_env.sh scripts/linux/build_and_run.sh scripts/linux/package-linux.sh
git commit -m "build: make Linux scripts Qt6-only"
```

## Task 6: Remove Qt5-specific operations from Windows and macOS deployment scripts

**Files:**

- Modify: `scripts/windows/build_script.sh`
- Modify: `scripts/windows/deploy_script.sh`
- Modify: `scripts/windows/FULL_BUILD.bat`
- Modify: `scripts/windows/prepare_qt6_msys2.sh`
- Modify: `scripts/macOS/package-macos.sh`

- [x] **Step 1: Make the MSYS2 preparation script detect only non-Qt6 packages generically.**

In `prepare_qt6_msys2.sh`:

- Rename the option to `--purge-legacy-qt` and the state variable to `PURGE_LEGACY_QT`.
- Keep the existing Qt6 package installation list.
- Replace the package query with:

```bash
mapfile -t legacy_qt_packages < <(
    pacman -Qq |
    grep -E '^mingw-w64-x86_64-qt[0-9]+-' |
    grep -v '^mingw-w64-x86_64-qt6-' || true
)
```

- Keep the confirmation prompt and `pacman -Rns -- "${legacy_qt_packages[@]}"` removal.
- Change all messages and the final verification to refer to legacy/non-Qt6 packages, without naming an older Qt major version.

- [x] **Step 2: Make the Windows build script reject any non-Qt6 package or cache entry.**

Replace the package check with a `legacy_qt_packages` array using the same two-stage `grep` filter as Step 1. Replace the cache check with:

```bash
legacy_qt_cache_refs="$(grep -Eio 'qt[0-9]+' CMakeCache.txt | sort -fu | grep -Eiv '^qt6$' || true)"
if [ -n "${legacy_qt_cache_refs}" ]; then
    NEED_CMAKE=1
    echo "[Step 1/2] Non-Qt6 cache entries found, re-configuring with Qt6..."
fi
```

Keep `Qt6_DIR`, the Qt6 `lrelease` lookup, and the existing Qt6 package requirement unchanged.

- [x] **Step 3: Make Windows deployment use a Qt6 whitelist.**

In `deploy_script.sh`:

- Change the cleanup glob to `rm -f Qt*.dll Qt*.DLL qt.conf` so every stale Qt DLL is removed before deployment.
- In the `ldd` loop, reject a dependency matching `Qt[0-9]*.dll` unless it matches `Qt6*.dll`.
- Replace the post-deployment scans with generic checks for Qt-named files/imports that are not Qt6, then require a Qt6 import. Use `objdump -p PXTOOL.exe` as the import source.
- Keep `windeployqt6.exe`, Qt6 plugin deployment, resource staging, and existing non-Qt DLL copying.

The resulting script must not copy or accept an unapproved Qt runtime.

- [x] **Step 4: Make the Windows full-build cleanup generic.**

Replace the two version-specific DLL deletion lines in `FULL_BUILD.bat` with:

```bat
del /f /q build.windows\Qt*.dll 2>nul
```

The Qt6 deployment step will restore the required Qt6 DLLs afterward.

- [x] **Step 5: Verify macOS selects Qt6 `macdeployqt`.**

In `package-macos.sh`, resolve the command safely and fail if it is absent:

```bash
MACDEPLOYQT="$(command -v macdeployqt || true)"
if [ -z "${MACDEPLOYQT}" ]; then
  echo "ERROR: Qt6 macdeployqt was not found."
  exit 1
fi
if ! "${MACDEPLOYQT}" -version 2>&1 | grep -Eq 'version 6(\.|$)|macdeployqt 6(\.|$)'; then
  echo "ERROR: macdeployqt is not a Qt6 deployment tool: ${MACDEPLOYQT}"
  exit 1
fi
```

Keep the existing framework bundling and optional-plugin cleanup.

- [x] **Step 6: Check all touched shell scripts and batch edits.**

Run:

```bash
bash -n scripts/windows/build_script.sh scripts/windows/deploy_script.sh \
  scripts/windows/prepare_qt6_msys2.sh scripts/macOS/package-macos.sh
rg -n -i 'qt5|qt 5|qt_version_check\(5|qt5_' \
  CMakeLists.txt PXTOOL scripts \
  --glob '!*.md' --glob '!*.txt' --glob '!*.json' || true
```

Expected: the first command exits 0. The second command prints no active-code matches; documentation files remain outside the scan.

- [x] **Step 7: Commit the cross-platform deployment cleanup.**

```bash
git add scripts/windows/build_script.sh scripts/windows/deploy_script.sh \
  scripts/windows/FULL_BUILD.bat scripts/windows/prepare_qt6_msys2.sh \
  scripts/macOS/package-macos.sh
git commit -m "build: enforce Qt6-only desktop deployment"
```

## Task 7: Switch the current Ubuntu toolchain from Qt5 to Qt6

**Files:**

- System package state only; no repository files.

- [x] **Step 1: Install or refresh the Qt6 development and tooling packages first.**

Run:

```bash
sudo apt-get update
sudo apt-get install --reinstall \
  qt6-base-dev qt6-svg-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools
```

Expected: `qmake6`, Qt6 headers, Qt6 CMake files, Qt6 SVG development files, and Qt6 `lrelease` remain available.

- [x] **Step 2: Resolve and inspect the exact installed legacy package list.**

Run this read-only package inventory:

```bash
mapfile -t legacy_qt_packages < <(
  dpkg-query -W -f='${binary:Package}\t${db:Status-Status}\n' 2>/dev/null |
  awk '$2 == "installed" { print $1 }' |
  grep -Ei '^(libqt5|qt5|qtbase5|qttools5|qttranslations5|qtwayland5|qml-module-qt|qdoc-qt5|qhelpgenerator-qt5|qtattributionsscanner-qt5|qtchooser$)' || true
)
printf '%s\n' "${legacy_qt_packages[@]}"
```

Expected: the list contains only Qt5-family packages and `qtchooser`; no Qt6 package appears.

- [x] **Step 3: Simulate removal and inspect the package solver result.**

Run:

```bash
sudo apt-get -s purge "${legacy_qt_packages[@]}"
```

Proceed only when the removal list contains the intended Qt5 packages and Qt5-only dependencies, does not remove Qt6 packages, and does not remove unrelated manually installed applications.

- [x] **Step 4: Purge the reviewed exact package list.**

Run:

```bash
sudo apt-get purge "${legacy_qt_packages[@]}"
```

Do not run an unrestricted `apt autoremove`. If the simulation identifies an additional package as a Qt5-only dependency, add that exact package to the reviewed command and repeat the simulation before purging it.

- [x] **Step 5: Verify the direct Qt6 tool entry points.**

Run:

```bash
qmake6 -v
QT6_BIN_DIR="$(qmake6 -query QT_INSTALL_BINS)"
"${QT6_BIN_DIR}/lrelease" -version
test -f "$(qmake6 -query QT_INSTALL_LIBS)/cmake/Qt6/Qt6Config.cmake"
```

Expected: both tools report Qt6 6.10.2 and the Qt6 CMake package exists. A generic `qmake` or `lrelease` alias may be absent after removing `qtchooser`; all project scripts use the direct Qt6 paths.

- [x] **Step 6: Commit no system state to Git; record the verification in the final handoff.**

The package transition is machine state, not repository content. Report the installed Qt6 version and the absence of Qt5 packages after the final build verification.

## Task 8: Run clean Qt6 builds, tests, and Linux packaging

**Files:**

- Generated only: `build.qt6`, `build.linux`, and package staging/output directories.

- [x] **Step 1: Run the active-code Qt5 scan and source hygiene checks.**

Run:

```bash
if rg -n -i 'qt5|qt 5|qt_version_check\(5|qt5_|Qt5::|find_package\(Qt5|QTextCodec|QRegExp|QPixmap::grabWidget|event->delta\(\)|AA_(Enable|Disable)HighDpiScaling' \
  CMakeLists.txt PXTOOL scripts \
  --glob '!*.md' --glob '!*.txt' --glob '!*.json'; then
  echo "ERROR: active Qt5 or Qt5-era API reference remains."
  exit 1
fi
git diff --check
```

Expected: the scan prints no matches and `git diff --check` exits 0. Historical docs can still contain their original Qt5 text.

- [x] **Step 2: Configure a clean Qt6 test build.**

Run:

```bash
cmake -S . -B build.qt6 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DENABLE_TESTS=ON \
  -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON \
  -DQt6_DIR="$(qmake6 -query QT_INSTALL_LIBS)/cmake/Qt6"
```

Expected: configuration succeeds with Qt6 and `rg -n -i 'qt[0-9]+' build.qt6/CMakeCache.txt | grep -v -i 'qt6'` prints no lines.

- [x] **Step 3: Build and test all configured targets.**

Run:

```bash
cmake --build build.qt6 --parallel "$(nproc 2>/dev/null || echo 4)"
ctest --test-dir build.qt6 --output-on-failure
```

Expected: the main executable, test executable, C decoders, and web UI dependencies build successfully; CTest reports all tests passed.

- [x] **Step 4: Build the Linux package through the production script.**

Run:

```bash
bash scripts/linux/package-linux.sh
```

Expected: the script configures with the helper's Qt6 CMake directory, builds, stages resources, validates ELF dependencies, and creates `build.linux/package/pxtool_1.0.0_<arch>.deb`.

- [x] **Step 5: Inspect package metadata and ELF linkage.**

Run:

```bash
PACKAGE_PATH="$(find build.linux/package -maxdepth 1 -type f -name 'pxtool_*.deb' -print -quit)"
test -n "${PACKAGE_PATH}"
dpkg-deb -f "${PACKAGE_PATH}" Depends
readelf -d build.linux/package-root/usr/bin/PXTOOL | sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p' | grep -Ei '^libqt'
```

Expected: `Depends` contains only Qt6 package names/alternatives; the ELF output contains `libQt6Core`, `libQt6Gui`, `libQt6Widgets`, `libQt6Network`, and `libQt6Svg`, with no other Qt major version.

- [x] **Step 6: Verify packaged resources remain complete.**

Run:

```bash
test -f build.linux/package-root/usr/bin/webui/index.html
test -d build.linux/package-root/usr/share/libsigrokdecode/decoders/c_decoders
test -f build.linux/package-root/usr/share/PXTOOL/logo.svg
```

Expected: all three checks pass and the existing web UI, C decoder, and application logo remain installed.

- [x] **Step 7: Verify final repository and machine state.**

Run:

```bash
dpkg-query -W -f='${binary:Package}\t${db:Status-Status}\n' 2>/dev/null |
  awk '$2 == "installed" { print $1 }' |
  grep -Ei '^(libqt5|qt5|qtbase5|qttools5|qttranslations5|qtwayland5|qml-module-qt|qdoc-qt5|qhelpgenerator-qt5|qtattributionsscanner-qt5|qtchooser$)' && exit 1 || true
qmake6 -v
"$(qmake6 -query QT_INSTALL_BINS)/lrelease" -version
git status --short
```

Expected: the package scan prints no installed Qt5-family package, Qt6 tools report 6.10.2, and Git shows only the intended implementation changes plus the committed process documents.

- [x] **Step 8: Commit the verified implementation.**

```bash
git add CMakeLists.txt PXTOOL scripts/linux scripts/windows scripts/macOS
git commit -m "feat: migrate Linux builds to Qt6"
```

## Plan Self-Review

**Execution alignment (2026-08-02):** The implementation, product-level verification, and host cleanup are complete. The host package scan produced no Qt5-family or `qtchooser` entries, while `qmake6` and `lrelease` both reported Qt 6.10.2. No `apt autoremove` was used.

- Spec coverage: Qt6-only CMake and tests are covered by Task 2; source API cleanup by Tasks 3-4; Linux tool discovery, stale-cache handling, package dependencies, and ELF validation by Tasks 1 and 5; Windows/macOS deployment enforcement by Task 6; machine package migration by Task 7; full build, CTest, package, resource, and system verification by Task 8.
- Completeness scan: this plan contains no unresolved markers or deferred implementation step; every code change has a target file and concrete replacement or command.
- Type/signature consistency: the Qt6 native-event declaration and definition both use `qintptr *`; the helper exports `QT6_QMAKE_BIN`, `QT6_BIN_DIR`, and `QT6_CMAKE_DIR`, and both Linux scripts consume `QT6_CMAKE_DIR`; the ELF helper validates the same five components declared by the CMake target list and package metadata.
- Scope check: historical documentation is excluded, while active CMake, source, and deployment paths are covered. No unrelated product behavior or UI refactor is included.
