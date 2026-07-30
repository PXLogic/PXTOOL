# Windows Qt 6.11.0 Migration Implementation Plan

> For agentic workers: REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Make every Windows build, deployment artifact, release ZIP, and MSYS2 package use Qt 6.11.0 with no Qt5 residue, while preserving Linux Qt5/Qt6 selection.

**Architecture:** Windows and macOS require Qt6. Linux retains the current Qt5-first/Qt6-fallback path. A small Windows Shell COM wrapper replaces QtWinExtras; Windows deployment uses Qt6 windeployqt and residue checks.

**Tech Stack:** CMake, MSYS2 MinGW64, Qt 6.11.0, Windows Shell COM ITaskbarList3, Bash, PowerShell, Boost.Test.

---

## File structure

| File | Responsibility |
| --- | --- |
| CMakeLists.txt | Platform Qt selection, Windows-only sources, COM link libraries. |
| PXTOOL/pv/wintaskbarprogress.h/.cpp | Testable owner of ITaskbarList3. |
| PXTOOL/pv/mainframe.h/.cpp | Existing capture-progress slot delegates to wrapper. |
| PXTOOL/pv/mainwindow.cpp, winnativewidget.cpp | Qt6 screenshot and Qt5-header removal. |
| PXTOOL/test/test_wintaskbarprogress.cpp | Windows-only normalization test. |
| scripts/windows/prepare_qt6_msys2.sh | Install Qt6 and explicitly purge Qt5. |
| Windows build/deploy scripts | Qt6-only build, deployment, and residue validation. |

### Task 1: Add a tested native taskbar-progress wrapper

**Files:**

- Create: PXTOOL/pv/wintaskbarprogress.h
- Create: PXTOOL/pv/wintaskbarprogress.cpp
- Create: PXTOOL/test/test_wintaskbarprogress.cpp
- Modify: CMakeLists.txt
- Modify: PXTOOL/test/CMakeLists.txt

- [ ] **Step 1: Write the failing test**

Create the test with this body:

    #include <boost/test/unit_test.hpp>
    #ifdef _WIN32
    #include "../pv/wintaskbarprogress.h"
    BOOST_AUTO_TEST_CASE(taskbar_progress_clamps_to_shell_range) {
        BOOST_CHECK_EQUAL(pv::WinTaskbarProgress::normalizedValue(-1), 0);
        BOOST_CHECK_EQUAL(pv::WinTaskbarProgress::normalizedValue(42), 42);
        BOOST_CHECK_EQUAL(pv::WinTaskbarProgress::normalizedValue(101), 100);
    }
    #endif

Append the test and the new source to DSView-test only within if(WIN32).

- [ ] **Step 2: Verify it fails**

Run: cmake -S . -B build.windows -G "MinGW Makefiles" -DENABLE_TESTS=ON; cmake --build build.windows --target DSView-test -j1

Expected: the compiler reports that wintaskbarprogress.h does not exist.

- [ ] **Step 3: Implement the wrapper**

Header contract:

    #ifdef _WIN32
    struct ITaskbarList3;
    namespace pv {
    class WinTaskbarProgress final {
    public:
        ~WinTaskbarProgress();
        WinTaskbarProgress(const WinTaskbarProgress&) = delete;
        static int normalizedValue(int value);
        void attach(HWND window);
        void setProgress(int value);
    private:
        HWND window_ = nullptr;
        ITaskbarList3* taskbar_ = nullptr;
        bool owns_com_initialization_ = false;
    };
    }
    #endif

Source requirements: include objbase.h, shobjidl.h, and algorithm; return std::clamp(value, 0, 100); use CoInitializeEx with COINIT_APARTMENTTHREADED and allow RPC_E_CHANGED_MODE; create CLSID_TaskbarList as ITaskbarList3 and require HrInit; use TBPF_NOPROGRESS at zero and TBPF_NORMAL plus SetProgressValue(window, value, 100) otherwise; clear, release, and conditionally CoUninitialize in the destructor.

- [ ] **Step 4: Link and pass the test**

Append the new cpp to the Windows DSView_SOURCES list and append -lole32 and -luuid to DSVIEW_LINK_LIBS under WIN32.

Run: cmake --build build.windows --target DSView-test -j1; ctest --test-dir build.windows -R "^DSView-test$" --output-on-failure

Expected: DSView-test passes.

- [ ] **Step 5: Commit**

Run: git add CMakeLists.txt PXTOOL/pv/wintaskbarprogress.h PXTOOL/pv/wintaskbarprogress.cpp PXTOOL/test/CMakeLists.txt PXTOOL/test/test_wintaskbarprogress.cpp; git commit -m "feat: replace Windows taskbar progress abstraction"

### Task 2: Replace QtWinExtras and obsolete desktop APIs

**Files:**

- Modify: PXTOOL/pv/mainframe.h
- Modify: PXTOOL/pv/mainframe.cpp
- Modify: PXTOOL/pv/mainwindow.cpp
- Modify: PXTOOL/pv/winnativewidget.cpp

- [ ] **Step 1: Prove the old references exist**

Run: rg -n "QWinTaskbar|QApplication::desktop|#include <QDesktopWidget>" PXTOOL/pv/mainframe.h PXTOOL/pv/mainframe.cpp PXTOOL/pv/mainwindow.cpp PXTOOL/pv/winnativewidget.cpp

Expected: current taskbar, Qt5 header, and invalid screenshot calls are printed.

- [ ] **Step 2: Wire MainFrame to WinTaskbarProgress**

Remove QWinTaskbar includes/members. Forward-declare WinTaskbarProgress under _WIN32 and use one pointer named _taskbar_progress. Include the new header in mainframe.cpp, construct it at the current _taskBtn construction point, and delete it in MainFrame destruction.

In showEvent, attach windowHandle()->winId when windowHandle exists, otherwise winId. In setTaskbarProgress, call _taskbar_progress->setProgress(progress) if the pointer is non-null. Preserve the existing non-Windows branch.

- [ ] **Step 3: Correct Qt6 screenshot capture**

Delete QDesktopWidget includes from these files. Replace the Windows Qt6 screenshot expression with:

    QScreen *screen = QGuiApplication::screenAt(QPoint(x, y));
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QPixmap pixmap = screen ? screen->grabWindow(0, x, y, w, h) : QPixmap();

Do not change macOS or Linux screenshot branches.

- [ ] **Step 4: Verify and commit**

Run: rg -n "QWinTaskbar|QApplication::desktop|#include <QDesktopWidget>" PXTOOL/pv/mainframe.h PXTOOL/pv/mainframe.cpp PXTOOL/pv/mainwindow.cpp PXTOOL/pv/winnativewidget.cpp; cmake --build build.windows --target DSView -j1

Expected: the search is empty and PXTOOL.exe links. Manually verify taskbar progress and screenshot export on a secondary high-DPI screen.

Run: git add PXTOOL/pv/mainframe.h PXTOOL/pv/mainframe.cpp PXTOOL/pv/mainwindow.cpp PXTOOL/pv/winnativewidget.cpp; git commit -m "fix: make Windows shell integration Qt6 compatible"

### Task 3: Require Qt6 on Windows only

**Files:**

- Modify: CMakeLists.txt lines 187-244 and 724-749
- Modify: PXTOOL/test/CMakeLists.txt lines 24-30

- [ ] **Step 1: Record old cache behavior**

Run: grep -E "Qt5|Qt6|QT_VERSION_FORCE" build.windows/CMakeCache.txt || true

Expected: a previous Qt5 Windows cache can be observed.

- [ ] **Step 2: Split CMake selection**

Before the Linux compatibility code, use:

    if(WIN32 OR APPLE)
        find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Network LinguistTools)
        set(QT_LIBRARIES Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Network)
        set(DSVIEW_QT_MAJOR 6)
        if(APPLE)
            find_package(Qt6 REQUIRED COMPONENTS DBus)
            list(APPEND QT_LIBRARIES Qt6::DBus)
        endif()
    else()
        # Keep existing Linux Qt5-first/Qt6-fallback discovery.
    endif()

Set DSVIEW_QT_MAJOR to 5 or 6 in the retained Linux branches. Windows/macOS must not call find_package(Qt5).

Use DSVIEW_QT_MAJOR to select the existing qt5 or qt6 translation, MOC, UI, and resource commands. Apply the same selector to PXTOOL/test/CMakeLists.txt resource generation.

- [ ] **Step 3: Clean configure and test**

Run: rm -rf build.windows/CMakeFiles build.windows/_deps; rm -f build.windows/CMakeCache.txt; cmake -S . -B build.windows -G "MinGW Makefiles" -DENABLE_TESTS=ON -DQt6_DIR=/mingw64/lib/cmake/Qt6; cmake --build build.windows -j1; ctest --test-dir build.windows --output-on-failure

Expected: Windows resolves Qt6 only and all tests pass.

- [ ] **Step 4: Commit**

Run: git add CMakeLists.txt PXTOOL/test/CMakeLists.txt; git commit -m "build: require Qt6 for Windows"

### Task 4: Prepare MSYS2 Qt6, deploy Qt6 only, and purge Qt5

**Files:**

- Create: scripts/windows/prepare_qt6_msys2.sh
- Modify: scripts/windows/build_script.sh
- Modify: scripts/windows/deploy_script.sh
- Modify: scripts/windows/FULL_BUILD.bat

- [ ] **Step 1: Write preparation/purge script**

The script installs exactly:

    pacman -Syu --needed mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-svg mingw-w64-x86_64-qt6-tools

Then obtains exact packages with:

    mapfile -t qt5_packages < <(pacman -Qq | grep '^mingw-w64-x86_64-qt5-' || true)

Default invocation prints the list and exits. Only --purge-qt5 may prompt the user, then run pacman -Rns with that exact array after a y confirmation. It fails if any Qt5 package remains. Invalid arguments exit 2; never use a force-removal option.

- [ ] **Step 2: Verify non-destructive setup**

Run: bash scripts/windows/prepare_qt6_msys2.sh

Expected: Qt6 packages install or are current; Qt5 package names print but remain installed.

- [ ] **Step 3: Enforce Qt6 tooling in build script**

Require /mingw64/lib/cmake/Qt6 and lrelease-qt6.exe; pass -DQt6_DIR=/mingw64/lib/cmake/Qt6; replace lrelease-qt5 candidates and install hints with Qt6 values. A generic lrelease fallback must run lrelease -version and reject non-Qt6 output. Reconfigure stale caches containing Qt5Core_DIR.

- [ ] **Step 4: Replace plugin copying with Qt6 deployment**

Require /mingw64/bin/windeployqt6.exe. Remove only build-directory plugins and Qt5 DLLs, then run:

    "$WINDEPLOYQT" --release --no-translations --no-compiler-runtime ./PXTOOL.exe

After it completes, fail if find reports Qt5*.dll or qt5 plugin paths, if objdump -p PXTOOL.exe finds Qt5, or if it does not find Qt6. Keep current Python, decoders, resources, web UI, icon, and non-Qt MinGW DLL steps.

- [ ] **Step 5: Perform clean verification and explicit purge**

FULL_BUILD.bat removes only build.windows plugins, Qt5 DLLs, Qt6 DLLs, and qt.conf before rebuilding.

Run: bash scripts/windows/build_script.sh; bash scripts/windows/deploy_script.sh; bash scripts/windows/prepare_qt6_msys2.sh --purge-qt5

Expected: deployment has Qt6 only and the purge prints/requires confirmation for the exact list.

Run after purge: scripts/windows/FULL_BUILD.bat; ctest --test-dir build.windows --output-on-failure; find build.windows -type f -iname "Qt5*.dll"; objdump -p build.windows/PXTOOL.exe | grep -E "Qt5|Qt6"; unzip -l PXTOOL-*-win64.zip | grep -i qt5 && exit 1 || true

Expected: tests pass; no Qt5 package, file, import, or ZIP entry remains; executable imports Qt6.

- [ ] **Step 6: Manual release smoke test and commit**

Launch deployed PXTOOL.exe on high-DPI multi-monitor Windows. Verify plugins, translations, resources, Python decoders, web UI, capture taskbar progress, screenshot export, and shutdown. Confirm application log prints Qt:6.11.x.

Run: git add scripts/windows/prepare_qt6_msys2.sh scripts/windows/build_script.sh scripts/windows/deploy_script.sh scripts/windows/FULL_BUILD.bat; git commit -m "build: deploy Windows runtime with Qt6 only"

## Final acceptance

- [ ] Windows CMake resolves Qt6 only; Linux Qt5 references remain inside Linux-only logic.
- [ ] MSYS2 reports no mingw-w64-x86_64-qt5 packages.
- [ ] CTest passes after a clean build with Qt5 uninstalled.
- [ ] The deployed application smoke test passes.
- [ ] Release ZIP contains no Qt5 DLL or plugin path.
