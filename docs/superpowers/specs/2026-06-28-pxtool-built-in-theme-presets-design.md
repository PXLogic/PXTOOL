# PXTOOL Built-In Theme Presets Design

## Goal

Add four PXView-derived built-in theme presets to PXTOOL/DSView while preserving the existing Dark and Light themes as the default visual baseline.

Users select themes only from the existing Themes menu. The feature does not include a custom color picker, token editor, style import/export UI, or user-defined preset management.

## Requirements

- Keep the current Dark theme visually unchanged by default.
- Keep the current Light theme visually unchanged by default.
- Add menu-selectable built-in presets:
  - Atom One Dark
  - Ayu Light
  - Dark Colored Cards
  - Light Colored Cards
- Persist the selected theme in the existing frame settings.
- Restore the selected theme on startup.
- Use PXView's tokenized theme assets for the four new presets where practical.
- Keep icon directory behavior compatible with current `:/icons/dark` and `:/icons/light` resources.
- Do not expose user theme editing, custom colors, `.pxstyle` import/export, or saved custom presets.

## Non-Goals

- No Style page in Display Options.
- No user custom theme directory scanning.
- No user-created preset saving or deletion.
- No requirement to make Dark and Light use PXView token JSON files.
- No broad UI redesign beyond the colors needed for the new built-in presets.

## Architecture

PXTOOL will support two theme application paths:

1. Legacy themes: `dark` and `light` continue loading the existing `:/dark.qss` and `:/light.qss`.
2. Token preset themes: `atom`, `ayu`, `dark_cards`, and `light_cards` load a shared `:/theme.qss` and replace token placeholders with values from the matching JSON preset.

This keeps existing behavior stable for current users while allowing the new presets to share PXView's maintainable token model.

## Theme IDs

The persisted `FrameOptions::style` value becomes the canonical theme ID.

- `dark`: existing PXTOOL dark theme, legacy QSS path
- `light`: existing PXTOOL light theme, legacy QSS path
- `atom`: Atom One Dark, token preset path
- `ayu`: Ayu Light, token preset path
- `dark_cards`: Dark Colored Cards, token preset path
- `light_cards`: Light Colored Cards, token preset path

Unknown or empty theme IDs fall back to `dark`.

## Resources

Add these PXView-derived resources to `PXTOOL/themes` and `breeze.qrc`:

- `theme.qss`
- `atom.json`
- `ayu.json`
- `dark_cards.json`
- `light_cards.json`

Existing `dark.qss`, `light.qss`, `dark/`, and `light/` resources stay in place and keep their current role.

## Application Flow

On startup:

1. `AppConfig::LoadAll()` loads `frameOptions.style`.
2. `MainWindow::switchTheme(style)` applies the selected theme.
3. Legacy IDs load their current QSS files.
4. Token preset IDs load `theme.qss`, load the matching JSON, replace tokens, set runtime theme tokens, and call `qApp->setStyleSheet(...)`.
5. UI managers receive theme/font update notifications as they do today.

On menu selection:

1. The user opens Themes.
2. The user chooses one of the six built-in menu actions.
3. `TrigBar` emits `sig_setTheme(themeId)`.
4. `MainWindow::switchTheme(themeId)` saves the selected ID and reapplies the theme.

## Token Runtime

`AppConfig` gains a small runtime token store:

- `SetThemeTokens(QHash<QString, QString>)`
- `GetThemeTokenValue(QString)`
- `GetThemeColor(QString)`

For legacy Dark and Light, the token store is populated from a compatibility map that mirrors the existing visual baseline closely enough for self-painted widgets to choose reasonable colors. Legacy QSS remains authoritative for normal Qt widgets.

For token preset themes, the token store is populated from the preset JSON.

`IsDarkStyle()` returns:

- `true` for `dark`, `atom`, and `dark_cards`
- `false` for `light`, `ayu`, and `light_cards`
- fallback based on `@bg-base` lightness if a future built-in preset is added without explicit classification

`GetIconPath()` returns only existing icon directories:

- dark icon directory for dark-like themes
- light icon directory for light-like themes

## Menu Design

The current Themes menu expands from two actions to six actions:

- Dark
- Light
- Atom One Dark
- Ayu Light
- Dark Colored Cards
- Light Colored Cards

The display toolbar Themes submenu mirrors the same six actions. Theme actions are checkable and mutually exclusive in both menu surfaces. The currently active theme is checked after startup and after every theme switch.

## Self-Painted UI Migration

PXTOOL currently contains self-painted or manually styled areas that branch on `IsDarkStyle()` or hard-coded colors. The implementation should migrate high-impact areas to runtime tokens only where needed for the new presets:

- main session tab bar colors
- disk cache footer separator colors
- side bar section title colors
- log dock colors
- custom scrollbars and edge navigation buttons
- waveform view background/panel colors where current code already has theme-sensitive behavior

Legacy `dark` and `light` must keep their current appearance. If a self-painted area cannot be matched exactly through tokens, it should keep the existing legacy branch for `dark` and `light`, and use token colors only for the four new presets.

## Error Handling

- Missing token JSON: log the issue and fall back to the matching legacy base (`dark` for dark-like IDs, `light` for light-like IDs).
- Malformed token JSON: log the parse failure and fall back to the matching legacy base.
- Missing token value: leave the placeholder fallback parsed from `theme.qss` when available; otherwise use a safe color from the legacy theme classification.
- Missing SVG resource while recoloring: keep the original resource path and continue applying the stylesheet.

## Testing

Minimum verification:

- Build succeeds.
- `dark` and `light` still load `dark.qss` and `light.qss`.
- Selecting each new menu item applies a visually distinct theme without crashing.
- The selected new theme persists after restart.
- `GetIconPath()` returns `:/icons/dark` for `atom` and `dark_cards`.
- `GetIconPath()` returns `:/icons/light` for `ayu` and `light_cards`.
- Malformed or missing preset JSON falls back without leaving the application unstyled.
- Existing Dark and Light screenshots are compared manually against the current baseline for the main window, dialogs, menus, sidebar, and waveform area.

## Rollout

Implement in small slices:

1. Add theme IDs, token runtime, and legacy compatibility behavior.
2. Add resources and token stylesheet loader.
3. Add six theme menu actions and persistence.
4. Theme key self-painted UI areas.
5. Run build and manual visual verification.
