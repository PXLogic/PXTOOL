# Device Options Dialog UI Alignment — Design Spec

**Reference:** `logic-viewer-web-version/components/logic-analyzer/options-modal.tsx`
**Approach:** B — QSS + footer restructure (same pattern as Search Options dialog)

---

## Scope

Style-only changes. All business logic (mode switching, channel enable/disable, probe commit, DSO calibration, analog tabs) stays byte-for-byte identical to the main branch. Only visual presentation changes.

---

## 1. C++ Structural Changes (`deviceoptions.cpp`)

### 1.1 Dialog identity & layout margins
```cpp
setObjectName("deviceOptionsDialog");
layout()->setContentsMargins(0, 5, 0, 0);  // full-width dividers; same as Search dialog
// SetTitleSpace(0) and layout()->setSpacing(0) already present — no change
```

### 1.2 Top divider (below TitleBar)
Replace the absence of a divider with an explicit 1 px separator widget immediately after `SetTitleSpace(8)`:
```cpp
SetTitleSpace(8);   // breathing room below title icon
auto *top_sep = new QWidget(this);
top_sep->setObjectName("device_options_divider");
top_sep->setFixedHeight(1);
layout()->addWidget(top_sep);
```
`_scroll_panel` is then added after `top_sep`.

### 1.3 Container margins
Both `_container_lay` and the scroll panel layout need horizontal padding to match the reference `mx-4`:
```cpp
_container_lay->setContentsMargins(12, 8, 12, 8);
```

### 1.4 Footer restructure
Remove the bare 5 px space widget + raw `QDialogButtonBox`. Replace with:
```cpp
// Bottom divider
auto *bot_sep = new QWidget(this);
bot_sep->setObjectName("device_options_divider");
bot_sep->setFixedHeight(1);
layout()->addWidget(bot_sep);

// Right-aligned OK button
auto *ok_btn = new QPushButton(L_S(..., "OK"), this);
ok_btn->setObjectName("device_ok_btn");
auto *footer_lay = new QHBoxLayout();
footer_lay->setContentsMargins(12, 10, 12, 10);
footer_lay->addStretch();
footer_lay->addWidget(ok_btn);
layout()->addLayout(footer_lay);
connect(ok_btn, SIGNAL(clicked()), this, SLOT(accept()));
```

### 1.5 Enable/Disable All button objectNames
In `logic_probes()`:
```cpp
enable_all_probes->setObjectName("device_ch_btn");
disable_all_probes->setObjectName("device_ch_btn");
```

---

## 2. QSS Changes (dark.qss + light.qss)

### 2.1 TitleBar font (scoped to this dialog)
```css
QDialog#deviceOptionsDialog QWidget#TitleBar QLabel {
    font-size: 14px;
    font-weight: 600;
    color: #e0e0e0;   /* light: #222222 */
}
```

### 2.2 Top/bottom divider
```css
QDialog#deviceOptionsDialog QWidget#device_options_divider {
    background-color: #333333;   /* light: #dddddd */
    min-height: 1px;
    max-height: 1px;
}
```

### 2.3 Group boxes (fieldset-style)
```css
QDialog#deviceOptionsDialog QGroupBox {
    border: 1px solid #333333;
    border-radius: 4px;
    margin-top: 10px;
    padding: 8px 6px 6px 6px;
    color: #888888;
    font-size: 11px;
}
QDialog#deviceOptionsDialog QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 8px;
    padding: 0 4px;
    color: #666666;
    font-size: 11px;
}
```

### 2.4 ComboBox (Operation Mode, Max Height dropdowns)
```css
QDialog#deviceOptionsDialog QComboBox {
    background-color: #2a2a2a;
    border: 1px solid #444444;
    border-radius: 3px;
    color: #cccccc;
    font-size: 12px;
    padding: 2px 20px 2px 6px;
    min-width: 88px;
    min-height: 22px;
}
QDialog#deviceOptionsDialog QComboBox::drop-down {
    border: none;
    width: 16px;
}
QDialog#deviceOptionsDialog QComboBox::down-arrow {
    image: url(:/icons/sidebar/chevron-down.svg);
    width: 10px;
    height: 10px;
}
```

### 2.5 Radio buttons (channel count mode)
```css
QDialog#deviceOptionsDialog QRadioButton {
    color: #aaaaaa;
    font-size: 12px;
    spacing: 6px;
}
QDialog#deviceOptionsDialog QRadioButton::indicator {
    width: 13px;
    height: 13px;
    border: 1px solid #444444;
    border-radius: 7px;
    background-color: #2a2a2a;
}
QDialog#deviceOptionsDialog QRadioButton::indicator:checked {
    background-color: #7c3aed;
    border-color: #7c3aed;
}
```

### 2.6 Channel checkboxes (ChannelLabel QCheckBox)
Use SVG images to override macOS Aqua rendering:
```css
QDialog#deviceOptionsDialog QCheckBox::indicator {
    width: 16px;
    height: 16px;
    image: url(:/icons/sidebar/checkbox-unchecked.svg);
}
QDialog#deviceOptionsDialog QCheckBox::indicator:checked {
    image: url(:/icons/sidebar/checkbox-checked.svg);
}
```
Reuses the existing `checkbox-unchecked.svg` / `checkbox-checked.svg` already added to `DSView.qrc` for the Measures panel.

### 2.7 Enable/Disable All buttons
```css
QPushButton#device_ch_btn {
    background-color: transparent;
    border: 1px solid #444444;
    border-radius: 3px;
    color: #aaaaaa;
    font-size: 12px;
    padding: 3px 10px;
    min-height: 22px;
}
QPushButton#device_ch_btn:hover {
    background-color: #2a2a2a;
    border-color: #555555;
}
```

### 2.8 OK button (footer)
```css
QPushButton#device_ok_btn {
    background-color: #7c3aed;
    border: none;
    border-radius: 4px;
    color: #ffffff;
    font-size: 12px;
    padding: 4px 16px;
    min-height: 24px;
}
QPushButton#device_ok_btn:hover {
    background-color: #6d28d9;
}
```

---

## 3. Light theme equivalents

All selectors repeated in `light.qss` with:
- `#333333` → `#dddddd` (borders/dividers)
- `#2a2a2a` → `#f0f0f0` (input backgrounds)
- `#444444` → `#cccccc` (input borders)
- `#cccccc` → `#222222` (input text)
- `#aaaaaa` → `#555555` (secondary text)
- `#888888` → `#888888` (group title)
- `#666666` → `#666666` (group legend)
- Purple values unchanged

---

## 4. No-change areas

- `ChannelLabel` class: no modifications
- `logic_probes()`, `analog_probes()`, `dynamic_widget()`, `channel_check()`, `accept()`, and all business logic
- `try_resize_scroll()` sizing math — unchanged
- DSO calibration buttons — no restyling (not in reference scope)
- `deviceoptions.h` — no changes needed

---

## 5. Files touched

| File | Change type |
|---|---|
| `DSView/pv/dialogs/deviceoptions.cpp` | Structural: objectName, margins, dividers, footer |
| `DSView/themes/dark.qss` | New rule block `/* ── Device Options Dialog (dark) ── */` |
| `DSView/themes/light.qss` | New rule block `/* ── Device Options Dialog (light) ── */` |
