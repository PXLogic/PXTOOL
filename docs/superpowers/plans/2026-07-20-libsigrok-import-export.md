# libsigrok Import/Export Format Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add PulseView-style DSView import/export menu support backed by libsigrok format descriptors while preserving native `*.dsl` workflows.

**Architecture:** Add a small format capability layer that enumerates `sr_input_list()` and `sr_output_list()` into stable C++ descriptors. Update `FileBar` to build native actions plus generated import/export menus from those descriptors. Keep the first implementation slice scoped to current in-repo modules, with a verified bridge point for a separate upstream-format sync plan.

**Tech Stack:** C++/Qt, DSView/PXTOOL, DSView-local libsigrok C API, Boost.Test, CMake.

---

## File Structure

- Create `PXTOOL/pv/data/formatcapability.h`: C++ descriptor types and public enumeration/filter helper declarations.
- Create `PXTOOL/pv/data/formatcapability.cpp`: implementation that reads `sr_input_list()` and `sr_output_list()` and produces menu/file-dialog descriptors.
- Create `PXTOOL/test/test_formatcapability.cpp`: Boost.Test coverage for descriptor enumeration, ordering, and current module IDs.
- Modify `CMakeLists.txt`: add the new capability source/header to the app build.
- Modify `PXTOOL/test/CMakeLists.txt`: add `test_formatcapability.cpp`, `formatcapability.cpp`, and the minimal libsigrok input/output module sources needed by the test binary.
- Modify `PXTOOL/pv/toolbars/filebar.h`: add import/export menu members and slots/signals for format-specific actions.
- Modify `PXTOOL/pv/toolbars/filebar.cpp`: generate PulseView-style open/save menu entries from descriptors.
- Modify `PXTOOL/pv/mainwindow.h` and `PXTOOL/pv/mainwindow.cpp`: receive format-specific import requests and route native `*.dsl` unchanged.
- Modify `PXTOOL/pv/storesession.h` and `PXTOOL/pv/storesession.cpp`: select output format by ID rather than relying only on file filter text.
- Modify `libsigrok/input/input.c` and `libsigrok/output/output.c` only if descriptor tests expose registration inconsistencies.

## Scope Notes

This plan implements the first working slice. It exposes descriptors and menus for formats already compiled in the current repository:

- Input: `vcd`, `wav`, `binary`
- Output: `csv`, `vcd`, `gnuplot`, `srzip`

Full upstream sync for `csv` input, Saleae, ChronoVu, STF, Trace32, ASCII/Bits/Hex/OLS/WaveDrom output, and option dialogs is outside this first implementation plan. Create a separate plan for those modules after this menu and descriptor foundation is verified.

### Task 1: Format Capability Descriptors

**Files:**
- Create: `PXTOOL/pv/data/formatcapability.h`
- Create: `PXTOOL/pv/data/formatcapability.cpp`
- Test: `PXTOOL/test/test_formatcapability.cpp`
- Modify: `PXTOOL/test/CMakeLists.txt`

- [ ] **Step 1: Write the failing descriptor tests**

Create `PXTOOL/test/test_formatcapability.cpp` with:

```cpp
/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <boost/test/unit_test.hpp>

#include <algorithm>

#include "../pv/data/formatcapability.h"

using pv::data::FormatCapability;
using pv::data::FormatKind;

namespace {

bool contains_id(const QVector<FormatCapability> &formats, const QString &id)
{
    return std::any_of(formats.begin(), formats.end(),
        [&](const FormatCapability &format) { return format.id == id; });
}

FormatCapability find_id(const QVector<FormatCapability> &formats, const QString &id)
{
    auto it = std::find_if(formats.begin(), formats.end(),
        [&](const FormatCapability &format) { return format.id == id; });
    BOOST_REQUIRE(it != formats.end());
    return *it;
}

} // namespace

BOOST_AUTO_TEST_SUITE(formatcapability)

BOOST_AUTO_TEST_CASE(enumerates_current_input_formats)
{
    const QVector<FormatCapability> inputs = pv::data::importFormats();

    BOOST_CHECK(contains_id(inputs, "vcd"));
    BOOST_CHECK(contains_id(inputs, "wav"));
    BOOST_CHECK(contains_id(inputs, "binary"));

    const FormatCapability vcd = find_id(inputs, "vcd");
    BOOST_CHECK_EQUAL(vcd.kind, FormatKind::Import);
    BOOST_CHECK_EQUAL(vcd.description, "Value Change Dump data");
    BOOST_CHECK(vcd.dialogFilter.contains("*.vcd"));
    BOOST_CHECK(vcd.menuText.contains("Value Change Dump data"));
}

BOOST_AUTO_TEST_CASE(enumerates_current_output_formats)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();

    BOOST_CHECK(contains_id(outputs, "csv"));
    BOOST_CHECK(contains_id(outputs, "vcd"));
    BOOST_CHECK(contains_id(outputs, "gnuplot"));
    BOOST_CHECK(contains_id(outputs, "srzip"));

    const FormatCapability csv = find_id(outputs, "csv");
    BOOST_CHECK_EQUAL(csv.kind, FormatKind::Export);
    BOOST_CHECK_EQUAL(csv.description, "Comma-separated values");
    BOOST_CHECK(csv.dialogFilter.contains("*.csv"));
    BOOST_CHECK(csv.menuText.contains("Comma-separated values"));
}

BOOST_AUTO_TEST_CASE(native_open_filter_precedes_import_filters)
{
    const QString filter = pv::data::openDialogFilter();

    BOOST_CHECK(filter.startsWith("DSView Data (*.dsl)"));
    BOOST_CHECK(filter.contains("Value Change Dump data (*.vcd)"));
    BOOST_CHECK(filter.contains("Microsoft WAV file format data (*.wav)"));
}

BOOST_AUTO_TEST_SUITE_END()
```

- [ ] **Step 2: Register the failing test source**

Modify `PXTOOL/test/CMakeLists.txt` so the `DSView-test` source list includes:

```cmake
    test_formatcapability.cpp
    ../pv/data/formatcapability.cpp
    ../../libsigrok/input/input.c
    ../../libsigrok/input/in_binary.c
    ../../libsigrok/input/in_vcd.c
    ../../libsigrok/input/in_wav.c
    ../../libsigrok/output/output.c
    ../../libsigrok/output/csv.c
    ../../libsigrok/output/gnuplot.c
    ../../libsigrok/output/srzip.c
    ../../libsigrok/output/vcd.c
```

Add these entries inside the existing `add_executable(DSView-test ...)` block.

- [ ] **Step 3: Run the test to verify it fails**

Run:

```bash
cmake --build Testing --target DSView-test
```

Expected: FAIL because `PXTOOL/pv/data/formatcapability.h` does not exist.

- [ ] **Step 4: Add the public descriptor API**

Create `PXTOOL/pv/data/formatcapability.h` with:

```cpp
/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef DSVIEW_PV_DATA_FORMATCAPABILITY_H
#define DSVIEW_PV_DATA_FORMATCAPABILITY_H

#include <QString>
#include <QVector>

namespace pv {
namespace data {

enum class FormatKind {
    Import,
    Export
};

struct FormatCapability {
    FormatKind kind;
    QString id;
    QString description;
    QString dialogFilter;
    QString menuText;
};

QVector<FormatCapability> importFormats();
QVector<FormatCapability> exportFormats();
QString openDialogFilter();
QString saveDialogFilter(const QVector<FormatCapability> &formats);
const FormatCapability *findFormatById(const QVector<FormatCapability> &formats,
                                       const QString &id);

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_FORMATCAPABILITY_H
```

- [ ] **Step 5: Add the descriptor implementation**

Create `PXTOOL/pv/data/formatcapability.cpp` with:

```cpp
/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "formatcapability.h"

extern "C" {
#include "libsigrok/libsigrok.h"
}

namespace pv {
namespace data {

namespace {

QString inputDescription(const sr_input_format *format)
{
    return QString::fromUtf8(format->description ? format->description : format->id);
}

QString outputDescription(const sr_output_module *module)
{
    return QString::fromUtf8(module->desc ? module->desc : module->id);
}

QString extensionFromId(const char *id)
{
    if (!id || !*id)
        return "*";
    return QString("*.%1").arg(QString::fromUtf8(id));
}

FormatCapability makeImportCapability(const sr_input_format *format)
{
    const QString id = QString::fromUtf8(format->id);
    const QString description = inputDescription(format);
    const QString extension = extensionFromId(format->id);

    FormatCapability capability;
    capability.kind = FormatKind::Import;
    capability.id = id;
    capability.description = description;
    capability.dialogFilter = QString("%1 (%2)").arg(description, extension);
    capability.menuText = QString("Import %1...").arg(description);
    return capability;
}

FormatCapability makeExportCapability(const sr_output_module *module)
{
    const QString id = QString::fromUtf8(module->id);
    const QString description = outputDescription(module);
    const QString extension = extensionFromId(module->id);

    FormatCapability capability;
    capability.kind = FormatKind::Export;
    capability.id = id;
    capability.description = description;
    capability.dialogFilter = QString("%1 (%2)").arg(description, extension);
    capability.menuText = QString("Export %1...").arg(description);
    return capability;
}

} // namespace

QVector<FormatCapability> importFormats()
{
    QVector<FormatCapability> formats;
    sr_input_format **modules = sr_input_list();
    for (int i = 0; modules && modules[i]; i++) {
        if (!modules[i]->id)
            continue;
        formats.push_back(makeImportCapability(modules[i]));
    }
    return formats;
}

QVector<FormatCapability> exportFormats()
{
    QVector<FormatCapability> formats;
    const sr_output_module **modules = sr_output_list();
    for (int i = 0; modules && modules[i]; i++) {
        if (!modules[i]->id)
            continue;
        formats.push_back(makeExportCapability(modules[i]));
    }
    return formats;
}

QString openDialogFilter()
{
    QStringList filters;
    filters << "DSView Data (*.dsl)";
    const QVector<FormatCapability> imports = importFormats();
    for (const FormatCapability &format : imports)
        filters << format.dialogFilter;
    return filters.join(";;");
}

QString saveDialogFilter(const QVector<FormatCapability> &formats)
{
    QStringList filters;
    for (const FormatCapability &format : formats)
        filters << format.dialogFilter;
    return filters.join(";;");
}

const FormatCapability *findFormatById(const QVector<FormatCapability> &formats,
                                       const QString &id)
{
    for (const FormatCapability &format : formats) {
        if (format.id == id)
            return &format;
    }
    return nullptr;
}

} // namespace data
} // namespace pv
```

- [ ] **Step 6: Add missing Qt include if the compiler asks for it**

If Step 7 reports `QStringList` is an incomplete type, add this include to `PXTOOL/pv/data/formatcapability.cpp`:

```cpp
#include <QStringList>
```

- [ ] **Step 7: Run descriptor tests**

Run:

```bash
cmake --build Testing --target DSView-test
./Testing/PXTOOL/test/DSView-test --run_test=formatcapability
```

Expected: PASS for all `formatcapability` test cases.

- [ ] **Step 8: Commit descriptor layer**

Run:

```bash
git add PXTOOL/pv/data/formatcapability.h PXTOOL/pv/data/formatcapability.cpp PXTOOL/test/test_formatcapability.cpp PXTOOL/test/CMakeLists.txt
git commit -m "feat: enumerate libsigrok format capabilities"
```

### Task 2: App Build Wiring

**Files:**
- Modify: `CMakeLists.txt`
- Test: app build target configured in `Testing`

- [ ] **Step 1: Write the expected source-list change**

Modify the `DSView_SOURCES` list in `CMakeLists.txt` to include:

```cmake
    PXTOOL/pv/data/formatcapability.cpp
```

Modify the `DSView_HEADERS` list in `CMakeLists.txt` to include:

```cmake
    PXTOOL/pv/data/formatcapability.h
```

- [ ] **Step 2: Build to verify app target still compiles**

Run:

```bash
cmake --build Testing --target DSView
```

Expected: PASS. If the target name differs in this checkout, run `cmake --build Testing --target help | rg "DSView|PXTOOL"` and build the app target listed there.

- [ ] **Step 3: Commit app wiring**

Run:

```bash
git add CMakeLists.txt
git commit -m "build: include format capability layer"
```

### Task 3: PulseView-Style Import Menu Entries

**Files:**
- Modify: `PXTOOL/pv/toolbars/filebar.h`
- Modify: `PXTOOL/pv/toolbars/filebar.cpp`
- Test: `PXTOOL/test/test_formatcapability.cpp`

- [ ] **Step 1: Add menu ordering tests**

Append this test case to `PXTOOL/test/test_formatcapability.cpp` before `BOOST_AUTO_TEST_SUITE_END()`:

```cpp
BOOST_AUTO_TEST_CASE(import_menu_labels_are_stable)
{
    const QVector<FormatCapability> inputs = pv::data::importFormats();

    const FormatCapability vcd = find_id(inputs, "vcd");
    const FormatCapability wav = find_id(inputs, "wav");

    BOOST_CHECK_EQUAL(vcd.menuText, "Import Value Change Dump data...");
    BOOST_CHECK_EQUAL(wav.menuText, "Import Microsoft WAV file format data...");
}
```

- [ ] **Step 2: Run the test to verify current labels**

Run:

```bash
cmake --build Testing --target DSView-test
./Testing/PXTOOL/test/DSView-test --run_test=formatcapability/import_menu_labels_are_stable
```

Expected: PASS after Task 1. This locks descriptor labels before UI consumes them.

- [ ] **Step 3: Add import members and signal**

Modify `PXTOOL/pv/toolbars/filebar.h`.

Add `#include <QMap>` below the existing Qt includes:

```cpp
#include <QMap>
```

Add this signal after `void sig_load_file(QString);`:

```cpp
    void sig_import_file(QString format_id, QString file_name);
```

Add this private slot after `void on_actionOpen_triggered();`:

```cpp
    void on_import_format_triggered();
```

Add these private members after `_action_open`:

```cpp
    QMenu   *_menu_import;
    QMap<QAction *, QString> _import_format_ids;
```

- [ ] **Step 4: Build the import menu in the constructor**

Modify `PXTOOL/pv/toolbars/filebar.cpp`.

Add this include:

```cpp
#include "../data/formatcapability.h"
```

After `_action_open->setObjectName(QString::fromUtf8("actionOpen"));`, add:

```cpp
    _menu_import = new QMenu(this);
    _menu_import->setObjectName(QString::fromUtf8("menuImport"));
```

Replace the current `_menu` population block:

```cpp
    _menu->addMenu(_menu_session);
    _menu->addAction(_action_open);
    _menu->addAction(_action_save);
    _menu->addAction(_action_export);
    _menu->addAction(_action_capture);
```

with:

```cpp
    _menu->addMenu(_menu_session);
    _menu->addAction(_action_open);
    _menu->addMenu(_menu_import);
    _menu->addAction(_action_save);
    _menu->addAction(_action_export);
    _menu->addAction(_action_capture);
```

After the existing `_action_capture` connection, add:

```cpp
    const QVector<pv::data::FormatCapability> import_formats = pv::data::importFormats();
    for (const pv::data::FormatCapability &format : import_formats) {
        QAction *action = _menu_import->addAction(format.menuText);
        action->setData(format.id);
        _import_format_ids.insert(action, format.id);
        connect(action, SIGNAL(triggered()), this, SLOT(on_import_format_triggered()));
    }
```

- [ ] **Step 5: Translate the import menu title**

In `FileBar::retranslateUi()`, add:

```cpp
    _menu_import->setTitle(tr("&Import"));
```

Place it after `_action_open->setText(tr("&Open..."));`.

- [ ] **Step 6: Style the import menu**

In `FileBar::reStyle()`, add:

```cpp
    _menu_import->setIcon(QIcon(iconPath+"/open.svg"));
```

Place it after `_action_open->setIcon(QIcon(iconPath+"/open.svg"));`.

- [ ] **Step 7: Implement format-specific import slot**

Add this method to `PXTOOL/pv/toolbars/filebar.cpp` after `FileBar::on_actionOpen_triggered()`:

```cpp
void FileBar::on_import_format_triggered()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;

    const QString format_id = action->data().toString();
    const QVector<pv::data::FormatCapability> formats = pv::data::importFormats();
    const pv::data::FormatCapability *format = pv::data::findFormatById(formats, format_id);
    if (!format)
        return;

    AppConfig &app = AppConfig::Instance();
    const QString file_name = QFileDialog::getOpenFileName(
        this,
        tr("Import File"),
        app.userHistory.openDir,
        format->dialogFilter);

    if (file_name.isEmpty())
        return;

    const QString dir = path::GetDirectoryName(file_name);
    if (dir != app.userHistory.openDir) {
        app.userHistory.openDir = dir;
        app.SaveHistory();
    }

    sig_import_file(format_id, file_name);
}
```

- [ ] **Step 8: Build to verify Qt signal/slot wiring**

Run:

```bash
cmake --build Testing --target DSView
```

Expected: PASS with no missing slot or moc errors.

- [ ] **Step 9: Commit import menu**

Run:

```bash
git add PXTOOL/pv/toolbars/filebar.h PXTOOL/pv/toolbars/filebar.cpp PXTOOL/test/test_formatcapability.cpp
git commit -m "feat: add libsigrok import menu entries"
```

### Task 4: Route Format-Specific Imports Without Breaking `*.dsl`

**Files:**
- Modify: `PXTOOL/pv/mainwindow.h`
- Modify: `PXTOOL/pv/mainwindow.cpp`
- Modify: `PXTOOL/pv/toolbars/filebar.cpp`
- Test: build target

- [ ] **Step 1: Add a MainWindow slot**

Modify `PXTOOL/pv/mainwindow.h` and add this slot next to `on_load_file(QString file_name)`:

```cpp
    void on_import_file(QString format_id, QString file_name);
```

- [ ] **Step 2: Connect FileBar import signal**

In `PXTOOL/pv/mainwindow.cpp`, near the existing `sig_load_file` connection, add:

```cpp
        connect(_file_bar, SIGNAL(sig_import_file(QString, QString)),
                this, SLOT(on_import_file(QString, QString)));
```

- [ ] **Step 3: Implement the first routing behavior**

Add this method near `MainWindow::on_load_file(QString file_name)`:

```cpp
    void MainWindow::on_import_file(QString format_id, QString file_name)
    {
        dsv_info("Import data: format=%s file=%s",
                 format_id.toUtf8().constData(),
                 file_name.toUtf8().constData());

        QString strMsg(tr("Import format is listed but not connected yet: "));
        strMsg += format_id;
        strMsg += "\n";
        strMsg += file_name;
        MsgBox::Show(strMsg);
    }
```

This preserves `*.dsl` behavior while giving format-specific menu entries a non-destructive, visible route.

- [ ] **Step 4: Build to verify routing compiles**

Run:

```bash
cmake --build Testing --target DSView
```

Expected: PASS.

- [ ] **Step 5: Commit import routing**

Run:

```bash
git add PXTOOL/pv/mainwindow.h PXTOOL/pv/mainwindow.cpp PXTOOL/pv/toolbars/filebar.cpp
git commit -m "feat: route libsigrok import selections"
```

### Task 5: Export Descriptor Selection

**Files:**
- Modify: `PXTOOL/pv/storesession.h`
- Modify: `PXTOOL/pv/storesession.cpp`
- Modify: `PXTOOL/pv/dialogs/storeprogress.h`
- Modify: `PXTOOL/pv/dialogs/storeprogress.cpp`
- Test: `PXTOOL/test/test_formatcapability.cpp`

- [ ] **Step 1: Add output label tests**

Append this test case to `PXTOOL/test/test_formatcapability.cpp` before `BOOST_AUTO_TEST_SUITE_END()`:

```cpp
BOOST_AUTO_TEST_CASE(export_menu_labels_are_stable)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();

    BOOST_CHECK_EQUAL(find_id(outputs, "csv").menuText,
                      "Export Comma-separated values...");
    BOOST_CHECK_EQUAL(find_id(outputs, "vcd").menuText,
                      "Export Value Change Dump...");
}
```

- [ ] **Step 2: Run the test**

Run:

```bash
cmake --build Testing --target DSView-test
./Testing/PXTOOL/test/DSView-test --run_test=formatcapability/export_menu_labels_are_stable
```

Expected: PASS after Task 1.

- [ ] **Step 3: Add selected output ID storage**

Modify `PXTOOL/pv/storesession.h` and add this private member near `_outModule`:

```cpp
    QString _selectedOutputFormatId;
```

Add this public setter near `getSuportedExportFormats()`:

```cpp
    void setSelectedOutputFormatId(const QString &format_id);
```

- [ ] **Step 4: Implement output ID setter**

Add this method to `PXTOOL/pv/storesession.cpp` near `getSuportedExportFormats()`:

```cpp
void StoreSession::setSelectedOutputFormatId(const QString &format_id)
{
    _selectedOutputFormatId = format_id;
}
```

- [ ] **Step 5: Use selected output ID in MakeExportFile**

In `StoreSession::MakeExportFile(bool bDlg)`, after `QString selfilter;`, add:

```cpp
    if (!_selectedOutputFormatId.isEmpty()) {
        const struct sr_output_module **supportedModules = sr_output_list();
        while (*supportedModules) {
            if (_selectedOutputFormatId == QString::fromUtf8((*supportedModules)->id)) {
                selfilter = QString("%1 (*.%2)")
                    .arg(QString::fromUtf8((*supportedModules)->desc))
                    .arg(QString::fromUtf8((*supportedModules)->id));
                break;
            }
            supportedModules++;
        }
    }
```

Then change:

```cpp
    if (app.userHistory.exportFormat != "" 
            && _session->get_device()->get_work_mode() == LOGIC){
```

to:

```cpp
    if (selfilter.isEmpty() && app.userHistory.exportFormat != ""
            && _session->get_device()->get_work_mode() == LOGIC){
```

Change:

```cpp
    else{
        selfilter.append(".csv");
    }
```

to:

```cpp
    else if (selfilter.isEmpty()){
        selfilter.append(".csv");
    }
```

- [ ] **Step 6: Build StoreSession changes**

Run:

```bash
cmake --build Testing --target DSView
```

Expected: PASS.

- [ ] **Step 7: Commit export selection**

Run:

```bash
git add PXTOOL/pv/storesession.h PXTOOL/pv/storesession.cpp PXTOOL/test/test_formatcapability.cpp
git commit -m "feat: select export format by descriptor id"
```

### Task 6: PulseView-Style Export Menu Entries

**Files:**
- Modify: `PXTOOL/pv/toolbars/filebar.h`
- Modify: `PXTOOL/pv/toolbars/filebar.cpp`
- Modify: `PXTOOL/pv/mainwindow.cpp`
- Modify: `PXTOOL/pv/mainwindow.h`
- Modify: `PXTOOL/pv/dialogs/storeprogress.h`
- Modify: `PXTOOL/pv/dialogs/storeprogress.cpp`
- Test: app build target

- [ ] **Step 1: Add export signal and members**

Modify `PXTOOL/pv/toolbars/filebar.h`.

Add this signal after `void sig_export();`:

```cpp
    void sig_export_format(QString format_id);
```

Add this private slot after `void on_import_format_triggered();`:

```cpp
    void on_export_format_triggered();
```

Add these private members after `_action_export`:

```cpp
    QMenu   *_menu_export;
    QMap<QAction *, QString> _export_format_ids;
```

- [ ] **Step 2: Build export menu in FileBar**

In `PXTOOL/pv/toolbars/filebar.cpp`, after `_action_export->setObjectName(QString::fromUtf8("actionExport"));`, add:

```cpp
    _menu_export = new QMenu(this);
    _menu_export->setObjectName(QString::fromUtf8("menuExport"));
```

Replace this menu line:

```cpp
    _menu->addAction(_action_export);
```

with:

```cpp
    _menu->addAction(_action_export);
    _menu->addMenu(_menu_export);
```

After the import menu population loop, add:

```cpp
    const QVector<pv::data::FormatCapability> export_formats = pv::data::exportFormats();
    for (const pv::data::FormatCapability &format : export_formats) {
        QAction *action = _menu_export->addAction(format.menuText);
        action->setData(format.id);
        _export_format_ids.insert(action, format.id);
        connect(action, SIGNAL(triggered()), this, SLOT(on_export_format_triggered()));
    }
```

- [ ] **Step 3: Translate and style the export menu**

In `FileBar::retranslateUi()`, add:

```cpp
    _menu_export->setTitle(tr("E&xport Format"));
```

Place it after `_action_export->setText(tr("&Export..."));`.

In `FileBar::reStyle()`, add:

```cpp
    _menu_export->setIcon(QIcon(iconPath+"/export.svg"));
```

Place it after `_action_export->setIcon(QIcon(iconPath+"/export.svg"));`.

- [ ] **Step 4: Implement export slot**

Add this method to `PXTOOL/pv/toolbars/filebar.cpp` after `on_import_format_triggered()`:

```cpp
void FileBar::on_export_format_triggered()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;

    const QString format_id = action->data().toString();
    if (format_id.isEmpty())
        return;

    sig_export_format(format_id);
}
```

- [ ] **Step 5: Connect export format signal in MainWindow**

In `PXTOOL/pv/mainwindow.cpp`, near the existing `sig_export` connection, add:

```cpp
        connect(_file_bar, SIGNAL(sig_export_format(QString)),
                this, SLOT(on_export_format(QString)));
```

If `MainWindow` does not already have `on_export_format(QString)`, add this slot to `PXTOOL/pv/mainwindow.h`:

```cpp
    void on_export_format(QString format_id);
```

Add a selected-format member to `PXTOOL/pv/mainwindow.h` near `_action_export` or other export-related members:

```cpp
    QString _selected_export_format_id;
```

Add this implementation near the existing `MainWindow::on_export()` handler:

```cpp
    void MainWindow::on_export_format(QString format_id)
    {
        dsv_info("Export data: selected format=%s", format_id.toUtf8().constData());
        _selected_export_format_id = format_id;
        on_export();
    }
```

- [ ] **Step 6: Add StoreProgress selected-format setter**

In `PXTOOL/pv/dialogs/storeprogress.h`, add this public method after `SetView(view::View *view)`:

```cpp
    void setSelectedOutputFormatId(const QString &format_id);
```

In `PXTOOL/pv/dialogs/storeprogress.cpp`, add this implementation after the constructor:

```cpp
void StoreProgress::setSelectedOutputFormatId(const QString &format_id)
{
    if (_store_session)
        _store_session->setSelectedOutputFormatId(format_id);
}
```

- [ ] **Step 7: Pass selected export ID into StoreProgress**

Modify `MainWindow::on_export()` so it sets the selected ID before running the dialog:

```cpp
    void MainWindow::on_export()
    {
        using pv::dialogs::StoreProgress;

        if (_session->is_working()){
            dsv_info("Export data: stop the current device.");
            _session->stop_capture();
        }

        StoreProgress *dlg = new StoreProgress(_session, this);
        dlg->SetView(_view);
        if (!_selected_export_format_id.isEmpty())
            dlg->setSelectedOutputFormatId(_selected_export_format_id);
        _selected_export_format_id.clear();
        dlg->export_run();
    }
```

- [ ] **Step 8: Build to verify export menu**

Run:

```bash
cmake --build Testing --target DSView
```

Expected: PASS.

- [ ] **Step 9: Commit export menu**

Run:

```bash
git add PXTOOL/pv/toolbars/filebar.h PXTOOL/pv/toolbars/filebar.cpp PXTOOL/pv/mainwindow.h PXTOOL/pv/mainwindow.cpp PXTOOL/pv/dialogs/storeprogress.h PXTOOL/pv/dialogs/storeprogress.cpp
git commit -m "feat: add libsigrok export menu entries"
```

### Task 7: Verification and Documentation

**Files:**
- Modify: `docs/superpowers/specs/2026-07-20-libsigrok-import-export-design.md` only if implementation behavior intentionally differs from the spec.

- [ ] **Step 1: Run focused tests**

Run:

```bash
cmake --build Testing --target DSView-test
./Testing/PXTOOL/test/DSView-test --run_test=formatcapability
```

Expected: PASS.

- [ ] **Step 2: Run app build**

Run:

```bash
cmake --build Testing --target DSView
```

Expected: PASS.

- [ ] **Step 3: Inspect final diff**

Run:

```bash
git status --short
git diff --stat HEAD
```

Expected: only intentional implementation files remain modified. Existing unrelated user changes may still appear in `git status --short`; do not revert them.

- [ ] **Step 4: Final commit if verification changed tracked files**

If Step 1 or Step 2 required small fixes, commit them with:

```bash
git add PXTOOL/pv/data/formatcapability.h PXTOOL/pv/data/formatcapability.cpp PXTOOL/test/test_formatcapability.cpp PXTOOL/test/CMakeLists.txt CMakeLists.txt PXTOOL/pv/toolbars/filebar.h PXTOOL/pv/toolbars/filebar.cpp PXTOOL/pv/mainwindow.h PXTOOL/pv/mainwindow.cpp PXTOOL/pv/storesession.h PXTOOL/pv/storesession.cpp
git commit -m "fix: verify libsigrok import export menus"
```

## Self-Review

- Spec coverage: descriptor layer, native workflow preservation, PulseView-style menus, first-slice format scope, error behavior, and tests are each covered by tasks.
- Placeholder scan: no task depends on an unnamed file or an undefined public type. Upstream module sync is explicitly outside this first implementation plan.
- Type consistency: the plan consistently uses `FormatCapability`, `FormatKind`, `importFormats()`, `exportFormats()`, and `findFormatById()`.
