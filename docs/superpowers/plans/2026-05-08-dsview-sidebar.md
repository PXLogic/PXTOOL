# DSView Right Sidebar Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace DSView's five separate right-side QDockWidgets with a single unified tabbed sidebar containing Trigger, Decodes, Measures, Search, and Options tabs — styled to match the shadcn/ui dark aesthetic from logic-viewer-web-version.

**Architecture:** A new `SideBar` QWidget (in `pv/dock/sidebar.h/cpp`) contains a `QStackedWidget` for content panels on the left and a fixed 52px icon strip on the right. This widget is wrapped in a single `QDockWidget` added to `Qt::RightDockWidgetArea`. The five existing dock content classes (TriggerDock, DsoTriggerDock, ProtocolDock, MeasureDock, SearchDock) are instantiated directly inside SideBar, eliminating the five individual QDockWidgets from MainWindow.

**Tech Stack:** Qt5/Qt6 C++, QSS (Qt StyleSheets), Lucide SVG icons, CMake

---

## File Map

| Action | File |
|--------|------|
| Create | `DSView/DSView/pv/dock/sidebar.h` |
| Create | `DSView/DSView/pv/dock/sidebar.cpp` |
| Create | `DSView/DSView/icons/sidebar/zap.svg` |
| Create | `DSView/DSView/icons/sidebar/layers.svg` |
| Create | `DSView/DSView/icons/sidebar/ruler.svg` |
| Create | `DSView/DSView/icons/sidebar/search.svg` |
| Create | `DSView/DSView/icons/sidebar/settings-2.svg` |
| Modify | `DSView/DSView/DSView.qrc` — add 5 new icon entries |
| Modify | `DSView/CMakeLists.txt` — add sidebar.cpp and sidebar.h |
| Modify | `DSView/DSView/pv/mainwindow.h` — replace 10 dock members with 2 |
| Modify | `DSView/DSView/pv/mainwindow.cpp` — setup_ui, connections, all widget refs |
| Modify | `DSView/DSView/themes/dark.qss` — append sidebar styles |
| Modify | `DSView/DSView/themes/light.qss` — append sidebar styles |

---

## Task 1: Create Lucide SVG Icons

**Files:**
- Create: `DSView/DSView/icons/sidebar/zap.svg`
- Create: `DSView/DSView/icons/sidebar/layers.svg`
- Create: `DSView/DSView/icons/sidebar/ruler.svg`
- Create: `DSView/DSView/icons/sidebar/search.svg`
- Create: `DSView/DSView/icons/sidebar/settings-2.svg`

- [ ] **Step 1: Create icon directory and write all 5 SVG files**

```bash
mkdir -p /Users/yuanji/Desktop/project/DSView/DSView/icons/sidebar
```

Write `DSView/DSView/icons/sidebar/zap.svg`:
```xml
<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#8b8b8b" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M4 14a1 1 0 0 1-.78-1.63l9.9-10.2a.5.5 0 0 1 .86.46l-1.92 6.02A1 1 0 0 0 13 10h7a1 1 0 0 1 .78 1.63l-9.9 10.2a.5.5 0 0 1-.86-.46l1.92-6.02A1 1 0 0 0 11 14z"/>
</svg>
```

Write `DSView/DSView/icons/sidebar/layers.svg`:
```xml
<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#8b8b8b" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M12.83 2.18a2 2 0 0 0-1.66 0L2.6 6.08a1 1 0 0 0 0 1.83l8.58 3.91a2 2 0 0 0 1.66 0l8.58-3.9a1 1 0 0 0 0-1.83z"/>
  <path d="m22 17.65-9.17 4.16a2 2 0 0 1-1.66 0L2 17.65"/>
  <path d="m22 12.65-9.17 4.16a2 2 0 0 1-1.66 0L2 12.65"/>
</svg>
```

Write `DSView/DSView/icons/sidebar/ruler.svg`:
```xml
<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#8b8b8b" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M21.3 15.3a2.4 2.4 0 0 1 0 3.4l-2.6 2.6a2.4 2.4 0 0 1-3.4 0L2.7 8.7a2.41 2.41 0 0 1 0-3.4l2.6-2.6a2.41 2.41 0 0 1 3.4 0Z"/>
  <path d="m14.5 12.5 2-2"/>
  <path d="m11.5 9.5 2-2"/>
  <path d="m8.5 6.5 2-2"/>
  <path d="m17.5 15.5 2-2"/>
</svg>
```

Write `DSView/DSView/icons/sidebar/search.svg`:
```xml
<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#8b8b8b" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <circle cx="11" cy="11" r="8"/>
  <path d="m21 21-4.3-4.3"/>
</svg>
```

Write `DSView/DSView/icons/sidebar/settings-2.svg`:
```xml
<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#8b8b8b" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M20 7h-9"/>
  <path d="M14 17H5"/>
  <circle cx="17" cy="17" r="3"/>
  <circle cx="7" cy="7" r="3"/>
</svg>
```

- [ ] **Step 2: Verify files exist**

```bash
ls /Users/yuanji/Desktop/project/DSView/DSView/icons/sidebar/
```
Expected: `zap.svg  layers.svg  ruler.svg  search.svg  settings-2.svg`

---

## Task 2: Register Icons in QRC

**Files:**
- Modify: `DSView/DSView/DSView.qrc`

- [ ] **Step 1: Add sidebar icon entries to DSView.qrc**

In `DSView/DSView/DSView.qrc`, find the line:
```xml
    <file>icons/win_title_logo.svg</file>
```
Add immediately after it:
```xml
        <file>icons/sidebar/zap.svg</file>
        <file>icons/sidebar/layers.svg</file>
        <file>icons/sidebar/ruler.svg</file>
        <file>icons/sidebar/search.svg</file>
        <file>icons/sidebar/settings-2.svg</file>
```

---

## Task 3: Create `sidebar.h`

**Files:**
- Create: `DSView/DSView/pv/dock/sidebar.h`

- [ ] **Step 1: Write the header file**

```cpp
/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef DSVIEW_PV_DOCK_SIDEBAR_H
#define DSVIEW_PV_DOCK_SIDEBAR_H

#include <QWidget>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include "../ui/uimanager.h"

namespace pv {

class SigSession;

namespace view {
class View;
}

namespace dock {

class TriggerDock;
class DsoTriggerDock;
class ProtocolDock;
class MeasureDock;
class SearchDock;

class SideBar : public QWidget, public IUiWindow
{
    Q_OBJECT

public:
    enum Tab {
        TabTrigger  = 0,
        TabDecodes  = 1,
        TabMeasures = 2,
        TabSearch   = 3,
        TabOptions  = 4,
        TabCount    = 5
    };

    SideBar(QWidget *parent, view::View &view, SigSession *session);
    ~SideBar();

    TriggerDock    *trigger_widget()        { return _trigger_widget; }
    DsoTriggerDock *dso_trigger_widget()    { return _dso_trigger_widget; }
    ProtocolDock   *protocol_widget()       { return _protocol_widget; }
    MeasureDock    *measure_widget()        { return _measure_widget; }
    SearchDock     *search_widget()         { return _search_widget; }

    void showTab(Tab tab, bool visible);
    void setDsoMode(bool isDso);

    // IUiWindow
    void UpdateLanguage() override;
    void UpdateTheme()    override;
    void UpdateFont()     override;

signals:
    void sig_search_visible(bool visible);

private slots:
    void onButtonClicked(int tab);

private:
    QStackedWidget *_stack;
    QWidget        *_icon_strip;
    QToolButton    *_btns[TabCount];
    int             _active_tab;
    bool            _panel_visible;

    QStackedWidget *_trigger_stack;
    TriggerDock    *_trigger_widget;
    DsoTriggerDock *_dso_trigger_widget;
    ProtocolDock   *_protocol_widget;
    MeasureDock    *_measure_widget;
    SearchDock     *_search_widget;
    QWidget        *_options_widget;

    SigSession     *_session;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_DOCK_SIDEBAR_H
```

---

## Task 4: Create `sidebar.cpp`

**Files:**
- Create: `DSView/DSView/pv/dock/sidebar.cpp`

- [ ] **Step 1: Write the implementation file**

```cpp
/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "sidebar.h"
#include "triggerdock.h"
#include "dsotriggerdock.h"
#include "protocoldock.h"
#include "measuredock.h"
#include "searchdock.h"
#include "../view/view.h"
#include <QLabel>
#include <QIcon>
#include <QSize>
#include <QSizePolicy>
#include <QSpacerItem>

namespace pv {
namespace dock {

SideBar::SideBar(QWidget *parent, view::View &view, SigSession *session)
    : QWidget(parent)
    , _session(session)
    , _active_tab(-1)
    , _panel_visible(false)
{
    setObjectName("sidebar");

    // --- Trigger content (inner stack for logic vs DSO mode) ---
    _trigger_stack = new QStackedWidget(this);
    _trigger_widget     = new TriggerDock(_trigger_stack, session);
    _dso_trigger_widget = new DsoTriggerDock(_trigger_stack, session);
    _trigger_stack->addWidget(_trigger_widget);      // index 0 = logic
    _trigger_stack->addWidget(_dso_trigger_widget);  // index 1 = DSO

    // --- Other content panels ---
    _protocol_widget = new ProtocolDock(this, view, session);
    _measure_widget  = new MeasureDock(this, view, session);
    _search_widget   = new SearchDock(this, view, session);

    _options_widget  = new QWidget(this);
    _options_widget->setObjectName("options_panel");
    auto *optLayout = new QVBoxLayout(_options_widget);
    optLayout->setContentsMargins(16, 16, 16, 16);
    optLayout->setSpacing(8);
    auto *optTitle = new QLabel(tr("OPTIONS"), _options_widget);
    optTitle->setObjectName("options_title_label");
    optLayout->addWidget(optTitle);
    auto *optHint = new QLabel(tr("Configure sampling and device\nsettings in the toolbar above."), _options_widget);
    optHint->setObjectName("options_hint_label");
    optHint->setWordWrap(true);
    optLayout->addWidget(optHint);
    optLayout->addStretch();

    // --- Outer stacked widget (one page per tab) ---
    _stack = new QStackedWidget(this);
    _stack->addWidget(_trigger_stack);   // page 0
    _stack->addWidget(_protocol_widget); // page 1
    _stack->addWidget(_measure_widget);  // page 2
    _stack->addWidget(_search_widget);   // page 3
    _stack->addWidget(_options_widget);  // page 4
    _stack->setVisible(false);
    _stack->setMinimumWidth(280);

    // --- Icon strip (52px fixed, far right) ---
    _icon_strip = new QWidget(this);
    _icon_strip->setObjectName("icon_strip");
    _icon_strip->setFixedWidth(52);

    auto *stripLayout = new QVBoxLayout(_icon_strip);
    stripLayout->setContentsMargins(0, 8, 0, 8);
    stripLayout->setSpacing(0);

    static const char *iconPaths[TabCount] = {
        ":/icons/sidebar/zap.svg",
        ":/icons/sidebar/layers.svg",
        ":/icons/sidebar/ruler.svg",
        ":/icons/sidebar/search.svg",
        ":/icons/sidebar/settings-2.svg"
    };
    static const char *labels[TabCount] = {
        "TRIGGER", "DECODE", "MEASURE", "SEARCH", "OPTIONS"
    };

    for (int i = 0; i < TabCount; i++) {
        _btns[i] = new QToolButton(this);
        _btns[i]->setObjectName("sidebar_tab_btn");
        _btns[i]->setIcon(QIcon(iconPaths[i]));
        _btns[i]->setText(labels[i]);
        _btns[i]->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        _btns[i]->setIconSize(QSize(18, 18));
        _btns[i]->setFixedSize(52, 54);
        _btns[i]->setCheckable(true);

        if (i < TabOptions) {
            stripLayout->addWidget(_btns[i]);
        }
    }
    stripLayout->addStretch(1);
    stripLayout->addWidget(_btns[TabOptions]); // Options pinned to bottom

    // Connect each button
    for (int i = 0; i < TabCount; i++) {
        connect(_btns[i], &QToolButton::clicked, [this, i]() {
            onButtonClicked(i);
        });
    }

    // --- Main layout: [content panel] [icon strip] ---
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(_stack, 1);
    mainLayout->addWidget(_icon_strip, 0);

    ADD_UI(this);
}

SideBar::~SideBar()
{
    REMOVE_UI(this);
}

void SideBar::onButtonClicked(int tab)
{
    if (_active_tab == tab && _panel_visible) {
        // Toggle off: hide the panel
        _panel_visible = false;
        _stack->setVisible(false);
        _btns[tab]->setChecked(false);
        _active_tab = -1;
        if (tab == TabSearch)
            emit sig_search_visible(false);
    } else {
        // Switch to (or re-open) this tab
        bool prevWasSearch = (_active_tab == TabSearch && _panel_visible);
        if (_active_tab >= 0 && _active_tab < TabCount)
            _btns[_active_tab]->setChecked(false);

        _active_tab    = tab;
        _panel_visible = true;
        _stack->setCurrentIndex(tab);
        _stack->setVisible(true);
        _btns[tab]->setChecked(true);

        if (tab == TabSearch)
            emit sig_search_visible(true);
        else if (prevWasSearch)
            emit sig_search_visible(false);
    }
}

void SideBar::showTab(Tab tab, bool visible)
{
    if (visible) {
        if (_active_tab != (int)tab || !_panel_visible)
            onButtonClicked((int)tab);
    } else {
        if (_active_tab == (int)tab && _panel_visible)
            onButtonClicked((int)tab); // toggle off
    }
}

void SideBar::setDsoMode(bool isDso)
{
    _trigger_stack->setCurrentIndex(isDso ? 1 : 0);
    if (isDso)
        _dso_trigger_widget->update_view();
    else
        _trigger_widget->update_view();
}

void SideBar::UpdateLanguage() {}
void SideBar::UpdateTheme()    {}
void SideBar::UpdateFont()     {}

} // namespace dock
} // namespace pv
```

---

## Task 5: Update CMakeLists.txt

**Files:**
- Modify: `DSView/CMakeLists.txt` (root, not DSView/DSView/)

- [ ] **Step 1: Add sidebar.cpp to the sources list**

In `DSView/CMakeLists.txt`, find:
```
    DSView/pv/dock/searchdock.cpp
```
Add immediately after:
```
    DSView/pv/dock/sidebar.cpp
```

- [ ] **Step 2: Add sidebar.h to the headers list**

Find:
```
    DSView/pv/dock/searchdock.h
```
Add immediately after:
```
    DSView/pv/dock/sidebar.h
```

---

## Task 6: Update `mainwindow.h`

**Files:**
- Modify: `DSView/DSView/pv/mainwindow.h`

- [ ] **Step 1: Add SideBar forward declaration**

Find the namespace dock forward-declarations block:
```cpp
namespace dock{
class ProtocolDock;
class TriggerDock;
class DsoTriggerDock;
class MeasureDock;
class SearchDock;
}
```
Replace with:
```cpp
namespace dock{
class ProtocolDock;
class TriggerDock;
class DsoTriggerDock;
class MeasureDock;
class SearchDock;
class SideBar;
}
```

- [ ] **Step 2: Replace individual dock member variables with sidebar members**

Find the block (lines 219–228):
```cpp
    QDockWidget             *_protocol_dock;
    dock::ProtocolDock      *_protocol_widget;
    QDockWidget             *_trigger_dock;
    QDockWidget             *_dso_trigger_dock;
    dock::TriggerDock       *_trigger_widget;
    dock::DsoTriggerDock    *_dso_trigger_widget;
    QDockWidget             *_measure_dock;
    dock::MeasureDock       *_measure_widget;
    QDockWidget             *_search_dock;
    dock::SearchDock        *_search_widget;
```
Replace with:
```cpp
    QDockWidget             *_sidebar_dock;
    dock::SideBar           *_sidebar_widget;
```

---

## Task 7: Update `mainwindow.cpp`

**Files:**
- Modify: `DSView/DSView/pv/mainwindow.cpp`

This task has multiple steps. Complete all of them.

- [ ] **Step 1: Add sidebar.h include**

Find the include block containing all dock headers:
```cpp
#include "dock/triggerdock.h"
#include "dock/dsotriggerdock.h"
#include "dock/measuredock.h"
#include "dock/searchdock.h"
#include "dock/protocoldock.h"
```
Add `#include "dock/sidebar.h"` immediately after these lines:
```cpp
#include "dock/triggerdock.h"
#include "dock/dsotriggerdock.h"
#include "dock/measuredock.h"
#include "dock/searchdock.h"
#include "dock/protocoldock.h"
#include "dock/sidebar.h"
```

- [ ] **Step 2: Replace dock widget creation in `setup_ui()`**

Find the entire block from `// trigger dock` through `addDockWidget(Qt::BottomDockWidgetArea, _search_dock);` (approximately lines 168–231):
```cpp
        // trigger dock
        _trigger_dock = new QDockWidget(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_DOCK_TITLE), "Trigger Setting..."), this);
        _trigger_dock->setObjectName("trigger_dock");
        _trigger_dock->setFeatures(QDockWidget::DockWidgetMovable);
        _trigger_dock->setAllowedAreas(Qt::RightDockWidgetArea);
        _trigger_dock->setVisible(false);
        _trigger_widget = new dock::TriggerDock(_trigger_dock, _session);        
        _trigger_dock->setWidget(_trigger_widget);

        _dso_trigger_dock = new QDockWidget(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_DOCK_TITLE), "Trigger Setting..."), this);
        _dso_trigger_dock->setObjectName("dso_trigger_dock");
        _dso_trigger_dock->setFeatures(QDockWidget::DockWidgetMovable);
        _dso_trigger_dock->setAllowedAreas(Qt::RightDockWidgetArea);
        _dso_trigger_dock->setVisible(false);
        _dso_trigger_widget = new dock::DsoTriggerDock(_dso_trigger_dock, _session);
        _dso_trigger_dock->setWidget(_dso_trigger_widget);
```

And the block from `// Setup the dockWidget` through `addDockWidget(Qt::BottomDockWidgetArea, _search_dock);`:
```cpp
        // Setup the dockWidget
        _protocol_dock = new QDockWidget(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_PROTOCOL_DOCK_TITLE), "Decode Protocol"), this);
        _protocol_dock->setObjectName("protocol_dock");
        _protocol_dock->setFeatures(QDockWidget::DockWidgetMovable);
        _protocol_dock->setAllowedAreas(Qt::RightDockWidgetArea);
        _protocol_dock->setVisible(false);
        _protocol_widget = new dock::ProtocolDock(_protocol_dock, *_view, _session);
        _protocol_dock->setWidget(_protocol_widget);

        _session->set_decoder_pannel(_protocol_widget);

        // measure dock
        _measure_dock = new QDockWidget(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MEASURE_DOCK_TITLE), "Measurement"), this);
        _measure_dock->setObjectName("measure_dock");
        _measure_dock->setFeatures(QDockWidget::DockWidgetMovable);
        _measure_dock->setAllowedAreas(Qt::RightDockWidgetArea);
        _measure_dock->setVisible(false);
        _measure_widget = new dock::MeasureDock(_measure_dock, *_view, _session);
        _measure_dock->setWidget(_measure_widget);

        // search dock
        _search_dock = new QDockWidget(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_DOCK_TITLE), "Search..."), this);
        _search_dock->setObjectName("search_dock");
        _search_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        _search_dock->setTitleBarWidget(new QWidget(_search_dock));
        _search_dock->setAllowedAreas(Qt::BottomDockWidgetArea);
        _search_dock->setVisible(false);

        _search_widget = new dock::SearchDock(_search_dock, *_view, _session);
        _search_dock->setWidget(_search_widget);

        addDockWidget(Qt::RightDockWidgetArea, _protocol_dock);
        addDockWidget(Qt::RightDockWidgetArea, _trigger_dock);
        addDockWidget(Qt::RightDockWidgetArea, _dso_trigger_dock);
        addDockWidget(Qt::RightDockWidgetArea, _measure_dock);
        addDockWidget(Qt::BottomDockWidgetArea, _search_dock);
```

Replace ALL of the above (both blocks together) with:
```cpp
        // Unified sidebar (replaces individual trigger/protocol/measure/search docks)
        _sidebar_dock = new QDockWidget(this);
        _sidebar_dock->setObjectName("sidebar_dock");
        _sidebar_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        _sidebar_dock->setTitleBarWidget(new QWidget(_sidebar_dock));
        _sidebar_dock->setAllowedAreas(Qt::RightDockWidgetArea);

        _sidebar_widget = new dock::SideBar(_sidebar_dock, *_view, _session);
        _sidebar_dock->setWidget(_sidebar_widget);

        _session->set_decoder_pannel(_sidebar_widget->protocol_widget());

        addDockWidget(Qt::RightDockWidgetArea, _sidebar_dock);
```

- [ ] **Step 3: Replace event filter installations**

Find:
```cpp
        _dso_trigger_dock->installEventFilter(this);
        _trigger_dock->installEventFilter(this);
        _protocol_dock->installEventFilter(this);
        _measure_dock->installEventFilter(this);
        _search_dock->installEventFilter(this);
```
Replace with:
```cpp
        _sidebar_dock->installEventFilter(this);
```

- [ ] **Step 4: Replace view/trigger signal connections**

Find these connect calls:
```cpp
        connect(_view, SIGNAL(cursor_update()), _measure_widget, SLOT(cursor_update()));
        connect(_view, SIGNAL(cursor_moving()), _measure_widget, SLOT(cursor_moving()));
        connect(_view, SIGNAL(cursor_moved()), _measure_widget, SLOT(reCalc()));
        connect(_view, SIGNAL(prgRate(int)), this, SIGNAL(prgRate(int)));
        connect(_view, SIGNAL(auto_trig(int)), _dso_trigger_widget, SLOT(auto_trig(int)));
```
Replace with:
```cpp
        connect(_view, SIGNAL(cursor_update()), _sidebar_widget->measure_widget(), SLOT(cursor_update()));
        connect(_view, SIGNAL(cursor_moving()), _sidebar_widget->measure_widget(), SLOT(cursor_moving()));
        connect(_view, SIGNAL(cursor_moved()), _sidebar_widget->measure_widget(), SLOT(reCalc()));
        connect(_view, SIGNAL(prgRate(int)), this, SIGNAL(prgRate(int)));
        connect(_view, SIGNAL(auto_trig(int)), _sidebar_widget->dso_trigger_widget(), SLOT(auto_trig(int)));
        connect(_sidebar_widget, SIGNAL(sig_search_visible(bool)), _view, SLOT(show_search_cursor(bool)));
```

- [ ] **Step 5: Replace protocol and dso_trigger signal connections**

Find:
```cpp
        connect(_protocol_widget, SIGNAL(protocol_updated()), this, SLOT(on_signals_changed()));
```
Replace with:
```cpp
        connect(_sidebar_widget->protocol_widget(), SIGNAL(protocol_updated()), this, SLOT(on_signals_changed()));
```

Find:
```cpp
        connect(_dso_trigger_widget, SIGNAL(set_trig_pos(int)), _view, SLOT(set_trig_pos(int)));
```
Replace with:
```cpp
        connect(_sidebar_widget->dso_trigger_widget(), SIGNAL(set_trig_pos(int)), _view, SLOT(set_trig_pos(int)));
```

- [ ] **Step 6: Update `retranslateUi()`**

Find:
```cpp
        _trigger_dock->setWindowTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_DOCK_TITLE), "Trigger Setting..."));
        _dso_trigger_dock->setWindowTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_DOCK_TITLE), "Trigger Setting..."));
        _protocol_dock->setWindowTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_PROTOCOL_DOCK_TITLE), "Decode Protocol"));
        _measure_dock->setWindowTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MEASURE_DOCK_TITLE), "Measurement"));
        _search_dock->setWindowTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_DOCK_TITLE), "Search..."));
```
Replace with:
```cpp
        // Sidebar has no visible title bar; individual panel labels are static
```

- [ ] **Step 7: Update `on_protocol()` slot**

Find:
```cpp
    void MainWindow::on_protocol(bool visible)
    {
        _protocol_dock->setVisible(visible);

        if (!visible)
            _view->setFocus();
    }
```
Replace with:
```cpp
    void MainWindow::on_protocol(bool visible)
    {
        _sidebar_widget->showTab(dock::SideBar::TabDecodes, visible);

        if (!visible)
            _view->setFocus();
    }
```

- [ ] **Step 8: Update `on_trigger()` slot**

Find:
```cpp
    void MainWindow::on_trigger(bool visible)
    {
        if (_device_agent->get_work_mode() != DSO)
        {
            _trigger_widget->update_view();
            _trigger_dock->setVisible(visible);
            _dso_trigger_dock->setVisible(false);
        }
        else
        {
            _dso_trigger_widget->update_view();
            _trigger_dock->setVisible(false);
            _dso_trigger_dock->setVisible(visible);
        }

        if (!visible)
            _view->setFocus();
    }
```
Replace with:
```cpp
    void MainWindow::on_trigger(bool visible)
    {
        bool isDso = (_device_agent->get_work_mode() == DSO);
        _sidebar_widget->setDsoMode(isDso);
        _sidebar_widget->showTab(dock::SideBar::TabTrigger, visible);

        if (!visible)
            _view->setFocus();
    }
```

- [ ] **Step 9: Update `on_measure()` slot**

Find:
```cpp
    void MainWindow::on_measure(bool visible)
    {
        _measure_dock->setVisible(visible);

        if (!visible)
            _view->setFocus();
    }
```
Replace with:
```cpp
    void MainWindow::on_measure(bool visible)
    {
        _sidebar_widget->showTab(dock::SideBar::TabMeasures, visible);

        if (!visible)
            _view->setFocus();
    }
```

- [ ] **Step 10: Update `on_search()` slot**

Find:
```cpp
    void MainWindow::on_search(bool visible)
    {
        _search_dock->setVisible(visible);
        _view->show_search_cursor(visible);

        if (!visible)
            _view->setFocus();
    }
```
Replace with:
```cpp
    void MainWindow::on_search(bool visible)
    {
        _sidebar_widget->showTab(dock::SideBar::TabSearch, visible);

        if (!visible)
            _view->setFocus();
    }
```
Note: `show_search_cursor` is now emitted via `sig_search_visible` from SideBar.

- [ ] **Step 11: Replace all remaining direct widget references throughout mainwindow.cpp**

Run these sed substitutions (safe — each target string is unique in context):

```bash
cd /Users/yuanji/Desktop/project/DSView/DSView/pv

# Widget accessor replacements
sed -i '' \
  's/_trigger_widget->/_sidebar_widget->trigger_widget()->/g; \
   s/_dso_trigger_widget->/_sidebar_widget->dso_trigger_widget()->/g; \
   s/_protocol_widget->/_sidebar_widget->protocol_widget()->/g; \
   s/_measure_widget->/_sidebar_widget->measure_widget()->/g; \
   s/_search_widget->/_sidebar_widget->search_widget()->/g' \
  mainwindow.cpp
```

After running, verify the file compiles (next task will catch any issues).

---

## Task 8: Add Sidebar QSS Styles

**Files:**
- Modify: `DSView/DSView/themes/dark.qss`
- Modify: `DSView/DSView/themes/light.qss`

- [ ] **Step 1: Append sidebar styles to `dark.qss`**

Append to the END of `DSView/DSView/themes/dark.qss`:
```css

/* ============================================================
   Sidebar (right panel, shadcn-dark style)
   ============================================================ */

QDockWidget#sidebar_dock {
    border: none;
    background: #1a1a1a;
}

QDockWidget#sidebar_dock::title {
    background: transparent;
    max-height: 0px;
    padding: 0px;
}

QWidget#sidebar {
    background: #1a1a1a;
}

QWidget#icon_strip {
    background: #1a1a1a;
    border-left: 1px solid #333333;
}

QToolButton#sidebar_tab_btn {
    background: transparent;
    border: none;
    border-left: 2px solid transparent;
    color: #666666;
    font-size: 8px;
    font-weight: bold;
    padding: 4px 0px 2px 0px;
    margin: 0px;
    text-align: center;
}

QToolButton#sidebar_tab_btn:hover:!checked {
    background: #252525;
    color: #aaaaaa;
    border-left: 2px solid transparent;
}

QToolButton#sidebar_tab_btn:checked {
    background: rgba(147, 51, 234, 30);
    border-left: 2px solid #9333ea;
    color: #e5e5e5;
}

QWidget#options_panel {
    background: #1a1a1a;
}

QLabel#options_title_label {
    color: #888888;
    font-size: 9px;
    font-weight: bold;
    letter-spacing: 2px;
}

QLabel#options_hint_label {
    color: #555555;
    font-size: 10px;
}
```

- [ ] **Step 2: Append sidebar styles to `light.qss`**

Append to the END of `DSView/DSView/themes/light.qss`:
```css

/* ============================================================
   Sidebar (right panel, shadcn-light style)
   ============================================================ */

QDockWidget#sidebar_dock {
    border: none;
    background: #f5f5f5;
}

QDockWidget#sidebar_dock::title {
    background: transparent;
    max-height: 0px;
    padding: 0px;
}

QWidget#sidebar {
    background: #f5f5f5;
}

QWidget#icon_strip {
    background: #f5f5f5;
    border-left: 1px solid #e0e0e0;
}

QToolButton#sidebar_tab_btn {
    background: transparent;
    border: none;
    border-left: 2px solid transparent;
    color: #999999;
    font-size: 8px;
    font-weight: bold;
    padding: 4px 0px 2px 0px;
    margin: 0px;
    text-align: center;
}

QToolButton#sidebar_tab_btn:hover:!checked {
    background: #ebebeb;
    color: #555555;
    border-left: 2px solid transparent;
}

QToolButton#sidebar_tab_btn:checked {
    background: rgba(147, 51, 234, 15);
    border-left: 2px solid #9333ea;
    color: #333333;
}

QWidget#options_panel {
    background: #f5f5f5;
}

QLabel#options_title_label {
    color: #888888;
    font-size: 9px;
    font-weight: bold;
    letter-spacing: 2px;
}

QLabel#options_hint_label {
    color: #aaaaaa;
    font-size: 10px;
}
```

---

## Task 9: Build and Verify

- [ ] **Step 1: Clean and rebuild**

```bash
cd /Users/yuanji/Desktop/project/DSView
make -j$(sysctl -n hw.logicalcpu) 2>&1
```
Expected: `[100%] Built target DSView` with no errors.

If there are compile errors, they will be in `mainwindow.cpp` due to renamed members. Fix by ensuring all `_trigger_widget->` references were replaced in Step 7.11.

- [ ] **Step 2: Launch and visually verify**

```bash
open /Users/yuanji/Desktop/project/DSView/build.dir/DSView.app
```

Verify:
1. The right side shows a 52px icon strip with 5 icons (Trigger, Decode, Measure, Search, Options)
2. Clicking each icon shows the corresponding panel to the left of the icon strip
3. Clicking an active icon hides the panel
4. The existing toolbar buttons (TrigBar) still show/hide the corresponding sidebar tab
5. The panel styling matches the dark theme (dark background, purple active indicator)
6. DSO/logic trigger modes switch correctly when device mode changes

- [ ] **Step 3: Commit**

```bash
cd /Users/yuanji/Desktop/project/DSView
git add DSView/DSView/pv/dock/sidebar.h \
        DSView/DSView/pv/dock/sidebar.cpp \
        DSView/DSView/icons/sidebar/ \
        DSView/DSView/DSView.qrc \
        CMakeLists.txt \
        DSView/DSView/pv/mainwindow.h \
        DSView/DSView/pv/mainwindow.cpp \
        DSView/DSView/themes/dark.qss \
        DSView/DSView/themes/light.qss
git commit -m "feat: replace individual dock widgets with unified tabbed sidebar

Add SideBar widget (pv/dock/sidebar.h/cpp) with Trigger, Decodes,
Measures, Search, and Options tabs. Styled with shadcn/ui dark theme
aesthetics: dark #1a1a1a background, purple accent (#9333ea) active
indicator, Lucide SVG icons.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```
