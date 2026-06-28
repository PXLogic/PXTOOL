# PXTOOL Built-In Theme Presets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add four PXView-derived built-in theme presets to PXTOOL while preserving the existing Dark and Light themes as the visual baseline.

**Architecture:** Keep `dark` and `light` on the existing legacy `dark.qss`/`light.qss` path. Add a small theme runtime module that knows all six built-in theme IDs, loads token preset JSON for `atom`, `ayu`, `dark_cards`, and `light_cards`, and exposes token colors to self-painted UI through `AppConfig`.

**Tech Stack:** C++17-style Qt 5 code, Qt resource files, Boost unit tests, CMake.

---

## File Structure

- Create `PXTOOL/pv/theme/thememanager.h`: built-in theme metadata, theme ID normalization, legacy-vs-token classification, token JSON/QSS loading API.
- Create `PXTOOL/pv/theme/thememanager.cpp`: implementation of metadata lookup, resource loading, token extraction/replacement, SVG token processing, and legacy compatibility tokens.
- Modify `PXTOOL/pv/config/appconfig.h`: define four new theme IDs; add runtime token API to `AppConfig`.
- Modify `PXTOOL/pv/config/appconfig.cpp`: store runtime tokens; update `IsDarkStyle()`, `GetStyleColor()`, and `GetIconPath()` for new built-in themes.
- Modify `PXTOOL/pv/mainwindow.h`: add QAction members for the four new themes and a helper to sync checked menu state.
- Modify `PXTOOL/pv/mainwindow.cpp`: expand the Window > Themes menu; route theme loading through `ThemeManager`; keep legacy themes on existing QSS files; update checked menu state.
- Modify `PXTOOL/pv/toolbars/trigbar.h`: add four theme actions and one shared theme action slot.
- Modify `PXTOOL/pv/toolbars/trigbar.cpp`: expand the Display > Themes submenu; make all six actions checkable; emit the selected theme ID.
- Modify `PXTOOL/themes/breeze.qrc`: include token preset resources.
- Copy from `D:\ap\PXView\PXView\themes\`: `theme.qss`, `atom.json`, `ayu.json`, `dark_cards.json`, `light_cards.json`.
- Modify `PXTOOL/pv/dock/sidebar.cpp`, `PXTOOL/pv/view/edge_nav_button.cpp`, `PXTOOL/pv/view/verticalscrollbar.cpp`, `PXTOOL/pv/view/waveformscrollbar.cpp`, and the session tab styling in `PXTOOL/pv/mainwindow.cpp`: use runtime tokens for token preset themes while retaining exact legacy branches for `dark` and `light`.
- Modify `CMakeLists.txt`: add the new theme manager source/header to the app target.
- Modify `PXTOOL/test/CMakeLists.txt`: compile `test_theme.cpp` with `thememanager.cpp`.
- Create `PXTOOL/test/test_theme.cpp`: metadata and token replacement unit tests that do not require launching the full GUI.

## Task 1: Theme Runtime and AppConfig Tokens

**Files:**
- Create: `PXTOOL/pv/theme/thememanager.h`
- Create: `PXTOOL/pv/theme/thememanager.cpp`
- Modify: `PXTOOL/pv/config/appconfig.h`
- Modify: `PXTOOL/pv/config/appconfig.cpp`
- Modify: `CMakeLists.txt`
- Create: `PXTOOL/test/test_theme.cpp`
- Modify: `PXTOOL/test/CMakeLists.txt`

- [ ] **Step 1: Add failing tests for theme metadata**

Create `PXTOOL/test/test_theme.cpp`:

```cpp
#include <boost/test/unit_test.hpp>

#include "../pv/theme/thememanager.h"

using pv::theme::ThemeId;
using pv::theme::ThemeManager;

BOOST_AUTO_TEST_SUITE(ThemeManagerTest)

BOOST_AUTO_TEST_CASE(normalizes_empty_and_unknown_to_dark)
{
    BOOST_CHECK_EQUAL(ThemeManager::normalizeId("").toStdString(), ThemeId::Dark);
    BOOST_CHECK_EQUAL(ThemeManager::normalizeId("missing").toStdString(), ThemeId::Dark);
}

BOOST_AUTO_TEST_CASE(classifies_legacy_and_token_presets)
{
    BOOST_CHECK(ThemeManager::isLegacyTheme(ThemeId::Dark));
    BOOST_CHECK(ThemeManager::isLegacyTheme(ThemeId::Light));
    BOOST_CHECK(!ThemeManager::isLegacyTheme(ThemeId::Atom));
    BOOST_CHECK(!ThemeManager::isLegacyTheme(ThemeId::Ayu));
    BOOST_CHECK(!ThemeManager::isLegacyTheme(ThemeId::DarkCards));
    BOOST_CHECK(!ThemeManager::isLegacyTheme(ThemeId::LightCards));
}

BOOST_AUTO_TEST_CASE(classifies_dark_like_theme_ids)
{
    BOOST_CHECK(ThemeManager::isDarkTheme(ThemeId::Dark));
    BOOST_CHECK(!ThemeManager::isDarkTheme(ThemeId::Light));
    BOOST_CHECK(ThemeManager::isDarkTheme(ThemeId::Atom));
    BOOST_CHECK(!ThemeManager::isDarkTheme(ThemeId::Ayu));
    BOOST_CHECK(ThemeManager::isDarkTheme(ThemeId::DarkCards));
    BOOST_CHECK(!ThemeManager::isDarkTheme(ThemeId::LightCards));
}

BOOST_AUTO_TEST_CASE(replaces_longer_tokens_before_shorter_tokens)
{
    QHash<QString, QString> tokens;
    tokens["@bg"] = "#111111";
    tokens["@bg-base"] = "#222222";

    QString qss = "QWidget { color: @bg; background: @bg-base; }";
    ThemeManager::replaceTokens(qss, tokens);

    BOOST_CHECK_EQUAL(qss.toStdString(),
                      "QWidget { color: #111111; background: #222222; }");
}

BOOST_AUTO_TEST_CASE(provides_legacy_compatibility_tokens)
{
    const QHash<QString, QString> dark = ThemeManager::legacyTokens(ThemeId::Dark);
    const QHash<QString, QString> light = ThemeManager::legacyTokens(ThemeId::Light);

    BOOST_CHECK_EQUAL(dark.value("@bg-base").toStdString(), "#262626");
    BOOST_CHECK_EQUAL(light.value("@bg-base").toStdString(), "#F8F8F8");
}

BOOST_AUTO_TEST_SUITE_END()
```

- [ ] **Step 2: Wire test source into the test target**

Modify `PXTOOL/test/CMakeLists.txt` so the target includes the new test and the theme manager implementation:

```cmake
add_executable(DSView-test
    test.cpp
    test_diskcachesettings.cpp
    test_logsearch.cpp
    test_theme.cpp
    ../pv/dock/logsearch.cpp
    ../pv/theme/thememanager.cpp
)
```

- [ ] **Step 3: Run tests and verify failure**

Run:

```powershell
cmake --build build.windows --target DSView-test
```

Expected: compilation fails because `PXTOOL/pv/theme/thememanager.h` does not exist yet. If `build.windows` is not configured with `ENABLE_TESTS=ON`, run:

```powershell
cmake -S . -B build.windows -DENABLE_TESTS=ON
cmake --build build.windows --target DSView-test
```

- [ ] **Step 4: Add `ThemeManager` header**

Create `PXTOOL/pv/theme/thememanager.h`:

```cpp
#pragma once

#include <QHash>
#include <QList>
#include <QString>

namespace pv {
namespace theme {

struct ThemeId {
    static constexpr const char *Dark = "dark";
    static constexpr const char *Light = "light";
    static constexpr const char *Atom = "atom";
    static constexpr const char *Ayu = "ayu";
    static constexpr const char *DarkCards = "dark_cards";
    static constexpr const char *LightCards = "light_cards";
};

struct ThemeInfo {
    QString id;
    QString label;
    QString jsonResource;
    bool legacy;
    bool darkLike;
};

class ThemeManager {
public:
    static QList<ThemeInfo> builtInThemes();
    static ThemeInfo themeInfo(const QString &id);
    static QString normalizeId(const QString &id);
    static bool isKnownTheme(const QString &id);
    static bool isLegacyTheme(const QString &id);
    static bool isDarkTheme(const QString &id);
    static QHash<QString, QString> legacyTokens(const QString &id);
    static bool loadTokenPreset(const QString &id,
                                QHash<QString, QString> &tokens,
                                QString *errorMessage = nullptr);
    static bool buildStyleSheet(const QString &id,
                                QHash<QString, QString> &tokens,
                                QString &styleSheet,
                                QString *errorMessage = nullptr);
    static void replaceTokens(QString &content,
                              const QHash<QString, QString> &tokens);
};

} // namespace theme
} // namespace pv
```

- [ ] **Step 5: Add `ThemeManager` implementation**

Create `PXTOOL/pv/theme/thememanager.cpp`:

```cpp
#include "thememanager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QDir>
#include <algorithm>

namespace pv {
namespace theme {

QList<ThemeInfo> ThemeManager::builtInThemes()
{
    return {
        {ThemeId::Dark,       QStringLiteral("Dark"),                QString(),                    true,  true},
        {ThemeId::Light,      QStringLiteral("Light"),               QString(),                    true,  false},
        {ThemeId::Atom,       QStringLiteral("Atom One Dark"),       QStringLiteral(":/atom.json"), false, true},
        {ThemeId::Ayu,        QStringLiteral("Ayu Light"),           QStringLiteral(":/ayu.json"),  false, false},
        {ThemeId::DarkCards,  QStringLiteral("Dark Colored Cards"),  QStringLiteral(":/dark_cards.json"),  false, true},
        {ThemeId::LightCards, QStringLiteral("Light Colored Cards"), QStringLiteral(":/light_cards.json"), false, false},
    };
}

ThemeInfo ThemeManager::themeInfo(const QString &id)
{
    const QString normalized = normalizeId(id);
    for (const ThemeInfo &theme : builtInThemes()) {
        if (theme.id == normalized)
            return theme;
    }
    return builtInThemes().front();
}

QString ThemeManager::normalizeId(const QString &id)
{
    for (const ThemeInfo &theme : builtInThemes()) {
        if (theme.id == id)
            return id;
    }
    return QStringLiteral("dark");
}

bool ThemeManager::isKnownTheme(const QString &id)
{
    for (const ThemeInfo &theme : builtInThemes()) {
        if (theme.id == id)
            return true;
    }
    return false;
}

bool ThemeManager::isLegacyTheme(const QString &id)
{
    return themeInfo(id).legacy;
}

bool ThemeManager::isDarkTheme(const QString &id)
{
    return themeInfo(id).darkLike;
}

QHash<QString, QString> ThemeManager::legacyTokens(const QString &id)
{
    if (isDarkTheme(id)) {
        return {
            {"@bg-base", "#262626"},
            {"@bg-overlay", "#2D2D2D"},
            {"@panel-bg", "#262626"},
            {"@panel-text", "#EFF0F1"},
            {"@fg-base", "#EFF0F1"},
            {"@fg-bright", "#FFFFFF"},
            {"@fg-muted", "#7A7A7A"},
            {"@border-strong", "#37373B"},
            {"@accent", "#528BFF"},
            {"@sidebar-accent", "#528BFF"},
            {"@group-card-bg", "#2D2D2D"},
            {"@group-card-colored", "false"},
        };
    }

    return {
        {"@bg-base", "#F8F8F8"},
        {"@bg-overlay", "#E8E8E8"},
        {"@panel-bg", "#F8F8F8"},
        {"@panel-text", "#2A2A2A"},
        {"@fg-base", "#2A2A2A"},
        {"@fg-bright", "#000000"},
        {"@fg-muted", "#8A8A8A"},
        {"@border-strong", "#D5D5D5"},
        {"@accent", "#528BFF"},
        {"@sidebar-accent", "#528BFF"},
        {"@group-card-bg", "#FFFFFF"},
        {"@group-card-colored", "false"},
    };
}

bool ThemeManager::loadTokenPreset(const QString &id,
                                   QHash<QString, QString> &tokens,
                                   QString *errorMessage)
{
    const ThemeInfo info = themeInfo(id);
    if (info.legacy) {
        tokens = legacyTokens(info.id);
        return true;
    }

    QFile file(info.jsonResource);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unable to open theme preset %1").arg(info.jsonResource);
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unable to parse theme preset %1: %2")
                                .arg(info.jsonResource, parseError.errorString());
        return false;
    }

    QJsonObject tokenObject = doc.object().value(QStringLiteral("tokens")).toObject();
    if (tokenObject.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Theme preset %1 has no tokens").arg(info.jsonResource);
        return false;
    }

    tokens.clear();
    for (auto it = tokenObject.constBegin(); it != tokenObject.constEnd(); ++it) {
        if (it.value().isString())
            tokens[it.key()] = it.value().toString();
    }
    return true;
}

void ThemeManager::replaceTokens(QString &content,
                                 const QHash<QString, QString> &tokens)
{
    QList<QString> keys = tokens.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
        return a.length() > b.length();
    });

    for (const QString &key : keys)
        content.replace(key, tokens.value(key));
}

bool ThemeManager::buildStyleSheet(const QString &id,
                                   QHash<QString, QString> &tokens,
                                   QString &styleSheet,
                                   QString *errorMessage)
{
    QFile qss(QStringLiteral(":/theme.qss"));
    if (!qss.open(QFile::ReadOnly | QFile::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unable to open :/theme.qss");
        return false;
    }

    styleSheet = QString::fromUtf8(qss.readAll());

    if (!loadTokenPreset(id, tokens, errorMessage))
        return false;

    replaceTokens(styleSheet, tokens);
    return true;
}

} // namespace theme
} // namespace pv
```

- [ ] **Step 6: Add app runtime tokens**

Modify `PXTOOL/pv/config/appconfig.h`:

```cpp
#include <QHash>

#define THEME_STYLE_DARK        "dark"
#define THEME_STYLE_LIGHT       "light"
#define THEME_STYLE_ATOM        "atom"
#define THEME_STYLE_AYU         "ayu"
#define THEME_STYLE_DARK_CARDS  "dark_cards"
#define THEME_STYLE_LIGHT_CARDS "light_cards"
```

Add these methods to `AppConfig`:

```cpp
  void SetThemeTokens(const QHash<QString, QString> &tokens);
  QString GetThemeTokenValue(const QString &tokenName) const;
  QColor GetThemeColor(const QString &tokenName) const;
```

Add this private member:

```cpp
  QHash<QString, QString> _themeTokens;
```

- [ ] **Step 7: Implement app token methods and dark/light classification**

Modify `PXTOOL/pv/config/appconfig.cpp`:

```cpp
#include "../theme/thememanager.h"
```

Replace `IsDarkStyle()`, `GetStyleColor()`, and `GetIconPath()` with:

```cpp
bool AppConfig::IsDarkStyle()
{
    return pv::theme::ThemeManager::isDarkTheme(frameOptions.style);
}

QColor AppConfig::GetStyleColor()
{
    QColor c = GetThemeColor("@bg-base");
    if (c.isValid())
        return c;

    return IsDarkStyle() ? QColor(38, 38, 38) : QColor(248, 248, 248);
}

void AppConfig::SetThemeTokens(const QHash<QString, QString> &tokens)
{
    _themeTokens = tokens;
}

QString AppConfig::GetThemeTokenValue(const QString &tokenName) const
{
    return _themeTokens.value(tokenName, QString());
}

QColor AppConfig::GetThemeColor(const QString &tokenName) const
{
    QColor color(GetThemeTokenValue(tokenName));
    return color;
}

QString GetIconPath()
{
    return AppConfig::Instance().IsDarkStyle()
        ? QStringLiteral(":/icons/dark")
        : QStringLiteral(":/icons/light");
}
```

- [ ] **Step 8: Add theme manager to app build**

Modify the app source list in `CMakeLists.txt` to include:

```cmake
    PXTOOL/pv/theme/thememanager.cpp
```

Modify the header list to include:

```cmake
    PXTOOL/pv/theme/thememanager.h
```

- [ ] **Step 9: Run tests**

Run:

```powershell
cmake --build build.windows --target DSView-test
.\build.windows\DSView-test.exe
```

Expected: all existing tests plus `ThemeManagerTest` pass.

- [ ] **Step 10: Commit**

```powershell
git add PXTOOL/pv/theme/thememanager.h PXTOOL/pv/theme/thememanager.cpp PXTOOL/pv/config/appconfig.h PXTOOL/pv/config/appconfig.cpp CMakeLists.txt PXTOOL/test/CMakeLists.txt PXTOOL/test/test_theme.cpp
git commit -m "feat: add built-in theme runtime"
```

## Task 2: Token Preset Resources and Stylesheet Loading

**Files:**
- Copy: `D:\ap\PXView\PXView\themes\theme.qss` to `PXTOOL/themes/theme.qss`
- Copy: `D:\ap\PXView\PXView\themes\atom.json` to `PXTOOL/themes/atom.json`
- Copy: `D:\ap\PXView\PXView\themes\ayu.json` to `PXTOOL/themes/ayu.json`
- Copy: `D:\ap\PXView\PXView\themes\dark_cards.json` to `PXTOOL/themes/dark_cards.json`
- Copy: `D:\ap\PXView\PXView\themes\light_cards.json` to `PXTOOL/themes/light_cards.json`
- Modify: `PXTOOL/themes/breeze.qrc`
- Modify: `PXTOOL/themes/breeze.qrc.depends`
- Modify: `PXTOOL/pv/theme/thememanager.cpp`
- Modify: `PXTOOL/test/test_theme.cpp`

- [ ] **Step 1: Add resource-load tests**

Append to `PXTOOL/test/test_theme.cpp`:

```cpp
BOOST_AUTO_TEST_CASE(loads_all_token_presets)
{
    const QStringList ids = {
        ThemeId::Atom,
        ThemeId::Ayu,
        ThemeId::DarkCards,
        ThemeId::LightCards,
    };

    for (const QString &id : ids) {
        QHash<QString, QString> tokens;
        QString error;
        BOOST_CHECK_MESSAGE(ThemeManager::loadTokenPreset(id, tokens, &error),
                            error.toStdString());
        BOOST_CHECK(tokens.contains("@bg-base"));
        BOOST_CHECK(tokens.contains("@fg-base"));
        BOOST_CHECK(tokens.contains("@icon-dir"));
    }
}
```

- [ ] **Step 2: Run tests and verify resource failure**

Run:

```powershell
cmake --build build.windows --target DSView-test
.\build.windows\DSView-test.exe --run_test=ThemeManagerTest/loads_all_token_presets
```

Expected: test fails because token preset resources are not available in the test binary yet.

- [ ] **Step 3: Copy PXView theme preset resources**

Copy the files exactly:

```powershell
Copy-Item -LiteralPath 'D:\ap\PXView\PXView\themes\theme.qss' -Destination 'PXTOOL\themes\theme.qss'
Copy-Item -LiteralPath 'D:\ap\PXView\PXView\themes\atom.json' -Destination 'PXTOOL\themes\atom.json'
Copy-Item -LiteralPath 'D:\ap\PXView\PXView\themes\ayu.json' -Destination 'PXTOOL\themes\ayu.json'
Copy-Item -LiteralPath 'D:\ap\PXView\PXView\themes\dark_cards.json' -Destination 'PXTOOL\themes\dark_cards.json'
Copy-Item -LiteralPath 'D:\ap\PXView\PXView\themes\light_cards.json' -Destination 'PXTOOL\themes\light_cards.json'
```

- [ ] **Step 4: Add resources to qrc files**

Modify `PXTOOL/themes/breeze.qrc` and `PXTOOL/themes/breeze.qrc.depends` by adding after `dark.qss`:

```xml
        <file>theme.qss</file>
        <file>atom.json</file>
        <file>ayu.json</file>
        <file>dark_cards.json</file>
        <file>light_cards.json</file>
```

- [ ] **Step 5: Compile qrc resources into DSView-test**

Modify `PXTOOL/test/CMakeLists.txt`:

```cmake
set(DSVIEW_TEST_QRC
    ../themes/breeze.qrc
)

qt5_add_resources(DSVIEW_TEST_RESOURCES ${DSVIEW_TEST_QRC})

add_executable(DSView-test
    test.cpp
    test_diskcachesettings.cpp
    test_logsearch.cpp
    test_theme.cpp
    ../pv/dock/logsearch.cpp
    ../pv/theme/thememanager.cpp
    ${DSVIEW_TEST_RESOURCES}
)
```

- [ ] **Step 6: Add SVG token processing to `buildStyleSheet()`**

Extend `PXTOOL/pv/theme/thememanager.cpp` inside `buildStyleSheet()` after `replaceTokens(styleSheet, tokens);`:

```cpp
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                      QStringLiteral("/pxtool_themed_svgs");
    QDir().mkpath(tempDir);

    QRegularExpression svgRe(QStringLiteral("image:\\s*url\\((:[^)]+\\.svg)\\)"));
    QRegularExpressionMatchIterator svgIt = svgRe.globalMatch(styleSheet);
    QSet<QString> processedSvgs;

    while (svgIt.hasNext()) {
        QRegularExpressionMatch match = svgIt.next();
        QString svgResPath = match.captured(1);
        if (processedSvgs.contains(svgResPath))
            continue;
        processedSvgs.insert(svgResPath);

        QFile svgFile(svgResPath);
        if (!svgFile.open(QFile::ReadOnly | QFile::Text))
            continue;

        QString svgContent = QString::fromUtf8(svgFile.readAll());
        replaceTokens(svgContent, tokens);

        QString fileName = svgResPath;
        fileName.replace(":/", "");
        fileName.replace("/", "_");
        QString tempPath = tempDir + "/" + fileName;

        QFile tempFile(tempPath);
        if (tempFile.open(QFile::WriteOnly | QFile::Text)) {
            tempFile.write(svgContent.toUtf8());
            styleSheet.replace(svgResPath, tempPath);
        }
    }
```

- [ ] **Step 7: Run resource tests**

Run:

```powershell
cmake --build build.windows --target DSView-test
.\build.windows\DSView-test.exe --run_test=ThemeManagerTest
```

Expected: all theme manager tests pass.

- [ ] **Step 8: Commit**

```powershell
git add PXTOOL/themes/theme.qss PXTOOL/themes/atom.json PXTOOL/themes/ayu.json PXTOOL/themes/dark_cards.json PXTOOL/themes/light_cards.json PXTOOL/themes/breeze.qrc PXTOOL/themes/breeze.qrc.depends PXTOOL/pv/theme/thememanager.cpp PXTOOL/test/CMakeLists.txt PXTOOL/test/test_theme.cpp
git commit -m "feat: add token theme preset resources"
```

## Task 3: Theme Menus and Persistence

**Files:**
- Modify: `PXTOOL/pv/mainwindow.h`
- Modify: `PXTOOL/pv/mainwindow.cpp`
- Modify: `PXTOOL/pv/toolbars/trigbar.h`
- Modify: `PXTOOL/pv/toolbars/trigbar.cpp`
- Modify: `PXTOOL/pv/theme/thememanager.h`
- Modify: `PXTOOL/pv/theme/thememanager.cpp`

- [ ] **Step 1: Add menu label helper**

Add this method to `ThemeManager` in `PXTOOL/pv/theme/thememanager.h`:

```cpp
static QString displayLabel(const QString &id);
```

Implementation:

```cpp
QString ThemeManager::displayLabel(const QString &id)
{
    return themeInfo(id).label;
}
```

- [ ] **Step 2: Expand `TrigBar` members**

Modify `PXTOOL/pv/toolbars/trigbar.h`:

```cpp
private slots:
    void on_theme_action_triggered();
```

Add members:

```cpp
    QActionGroup *_theme_group;
    QAction     *_atom_style;
    QAction     *_ayu_style;
    QAction     *_dark_cards_style;
    QAction     *_light_cards_style;
```

- [ ] **Step 3: Build six checkable toolbar theme actions**

Modify the theme action setup in `PXTOOL/pv/toolbars/trigbar.cpp`:

```cpp
    _theme_group = new QActionGroup(this);
    _theme_group->setExclusive(true);

    _dark_style = new QAction(this);
    _dark_style->setObjectName(QString::fromUtf8("actionDark"));
    _dark_style->setData(THEME_STYLE_DARK);
    _dark_style->setCheckable(true);

    _light_style = new QAction(this);
    _light_style->setObjectName(QString::fromUtf8("actionLight"));
    _light_style->setData(THEME_STYLE_LIGHT);
    _light_style->setCheckable(true);

    _atom_style = new QAction(this);
    _atom_style->setObjectName(QString::fromUtf8("actionAtom"));
    _atom_style->setData(THEME_STYLE_ATOM);
    _atom_style->setCheckable(true);

    _ayu_style = new QAction(this);
    _ayu_style->setObjectName(QString::fromUtf8("actionAyu"));
    _ayu_style->setData(THEME_STYLE_AYU);
    _ayu_style->setCheckable(true);

    _dark_cards_style = new QAction(this);
    _dark_cards_style->setObjectName(QString::fromUtf8("actionDarkCards"));
    _dark_cards_style->setData(THEME_STYLE_DARK_CARDS);
    _dark_cards_style->setCheckable(true);

    _light_cards_style = new QAction(this);
    _light_cards_style->setObjectName(QString::fromUtf8("actionLightCards"));
    _light_cards_style->setData(THEME_STYLE_LIGHT_CARDS);
    _light_cards_style->setCheckable(true);

    for (QAction *action : {_dark_style, _light_style, _atom_style, _ayu_style,
                            _dark_cards_style, _light_cards_style}) {
        _theme_group->addAction(action);
        connect(action, &QAction::triggered, this, &TrigBar::on_theme_action_triggered);
    }

    _themes = new QMenu(this);
    _themes->setObjectName(QString::fromUtf8("menuThemes"));
    _themes->addAction(_dark_style);
    _themes->addAction(_light_style);
    _themes->addAction(_atom_style);
    _themes->addAction(_ayu_style);
    _themes->addAction(_dark_cards_style);
    _themes->addAction(_light_cards_style);
```

- [ ] **Step 4: Update toolbar labels and icons**

Modify `retranslateUi()`:

```cpp
    _dark_style->setText(tr("Dark"));
    _light_style->setText(tr("Light"));
    _atom_style->setText(tr("Atom One Dark"));
    _ayu_style->setText(tr("Ayu Light"));
    _dark_cards_style->setText(tr("Dark Colored Cards"));
    _light_cards_style->setText(tr("Light Colored Cards"));
```

Modify `reStyle()`:

```cpp
    _atom_style->setIcon(QIcon(iconPath + "/dark.svg"));
    _ayu_style->setIcon(QIcon(iconPath + "/light.svg"));
    _dark_cards_style->setIcon(QIcon(iconPath + "/dark.svg"));
    _light_cards_style->setIcon(QIcon(iconPath + "/light.svg"));

    const QString current = pv::theme::ThemeManager::normalizeId(AppConfig::Instance().frameOptions.style);
    for (QAction *action : _theme_group->actions())
        action->setChecked(action->data().toString() == current);
    _themes->setIcon(QIcon(iconPath + "/" +
        (AppConfig::Instance().IsDarkStyle() ? QStringLiteral("dark.svg") : QStringLiteral("light.svg"))));
```

Add include:

```cpp
#include "../theme/thememanager.h"
```

- [ ] **Step 5: Replace old toolbar theme slots**

Keep `on_actionDark_triggered()` and `on_actionLight_triggered()` only if other code still references them. Make them delegate:

```cpp
void TrigBar::on_theme_action_triggered()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;
    sig_setTheme(action->data().toString());
}

void TrigBar::on_actionDark_triggered()
{
    sig_setTheme(THEME_STYLE_DARK);
}

void TrigBar::on_actionLight_triggered()
{
    sig_setTheme(THEME_STYLE_LIGHT);
}
```

- [ ] **Step 6: Expand main window theme actions**

Modify `PXTOOL/pv/mainwindow.h` by adding:

```cpp
    QActionGroup *_theme_action_group = nullptr;
    QAction *_action_atom = nullptr;
    QAction *_action_ayu = nullptr;
    QAction *_action_dark_cards = nullptr;
    QAction *_action_light_cards = nullptr;

    void syncThemeActions();
```

Modify the Window menu setup in `PXTOOL/pv/mainwindow.cpp`:

```cpp
        _theme_action_group = new QActionGroup(this);
        _theme_action_group->setExclusive(true);

        auto addThemeAction = [this](const QString &text, const QString &themeId) {
            QAction *action = _menu_themes->addAction(text);
            action->setCheckable(true);
            action->setData(themeId);
            _theme_action_group->addAction(action);
            connect(action, &QAction::triggered, this, [this, action]() {
                switchTheme(action->data().toString());
            });
            return action;
        };

        _action_dark = addThemeAction(tr("Dark"), THEME_STYLE_DARK);
        _action_light = addThemeAction(tr("Light"), THEME_STYLE_LIGHT);
        _action_atom = addThemeAction(tr("Atom One Dark"), THEME_STYLE_ATOM);
        _action_ayu = addThemeAction(tr("Ayu Light"), THEME_STYLE_AYU);
        _action_dark_cards = addThemeAction(tr("Dark Colored Cards"), THEME_STYLE_DARK_CARDS);
        _action_light_cards = addThemeAction(tr("Light Colored Cards"), THEME_STYLE_LIGHT_CARDS);
```

Remove the direct signal connections from `_action_dark` and `_action_light` to `TrigBar` theme slots.

- [ ] **Step 7: Sync menu labels and checked state**

Modify `retranslateUi()` in `mainwindow.cpp`:

```cpp
        if (_action_dark)        _action_dark->setText(tr("Dark"));
        if (_action_light)       _action_light->setText(tr("Light"));
        if (_action_atom)        _action_atom->setText(tr("Atom One Dark"));
        if (_action_ayu)         _action_ayu->setText(tr("Ayu Light"));
        if (_action_dark_cards)  _action_dark_cards->setText(tr("Dark Colored Cards"));
        if (_action_light_cards) _action_light_cards->setText(tr("Light Colored Cards"));
```

Add helper:

```cpp
void MainWindow::syncThemeActions()
{
    if (!_theme_action_group)
        return;

    const QString current = pv::theme::ThemeManager::normalizeId(AppConfig::Instance().frameOptions.style);
    for (QAction *action : _theme_action_group->actions())
        action->setChecked(action->data().toString() == current);
}
```

Call `syncThemeActions()` at the end of `switchTheme()`.

- [ ] **Step 8: Route `switchTheme()` through legacy/token paths**

Add includes to `mainwindow.cpp`:

```cpp
#include "theme/thememanager.h"
#include <QJsonDocument>
#include <QJsonObject>
```

Replace the body of `MainWindow::switchTheme(QString style)`:

```cpp
void MainWindow::switchTheme(QString style)
{
    AppConfig &app = AppConfig::Instance();
    const QString themeId = pv::theme::ThemeManager::normalizeId(style);

    if (app.frameOptions.style != themeId) {
        app.frameOptions.style = themeId;
        app.SaveFrame();
    }

    if (pv::theme::ThemeManager::isLegacyTheme(themeId)) {
        QHash<QString, QString> tokens = pv::theme::ThemeManager::legacyTokens(themeId);
        app.SetThemeTokens(tokens);

        QFile qss(QStringLiteral(":/") + themeId + QStringLiteral(".qss"));
        if (qss.open(QFile::ReadOnly | QFile::Text)) {
            qApp->setStyleSheet(QString::fromUtf8(qss.readAll()));
        } else {
            dsv_err("Unable to open legacy theme qss:%s", themeId.toUtf8().data());
        }
    } else {
        QHash<QString, QString> tokens;
        QString styleSheet;
        QString error;
        if (pv::theme::ThemeManager::buildStyleSheet(themeId, tokens, styleSheet, &error)) {
            app.SetThemeTokens(tokens);
            qApp->setStyleSheet(styleSheet);
        } else {
            dsv_err("Unable to apply token theme:%s", error.toUtf8().data());
            const QString fallback = pv::theme::ThemeManager::isDarkTheme(themeId)
                ? QStringLiteral(THEME_STYLE_DARK)
                : QStringLiteral(THEME_STYLE_LIGHT);
            app.frameOptions.style = fallback;
            app.SaveFrame();
            switchTheme(fallback);
            return;
        }
    }

    UiManager::Instance()->Update(UI_UPDATE_ACTION_THEME);
    UiManager::Instance()->Update(UI_UPDATE_ACTION_FONT);

    if (_session_tab_bar)
        rebuild_tab_buttons();
    update_disk_cache_footer();
    syncThemeActions();
}
```

- [ ] **Step 9: Build app target**

Run:

```powershell
cmake --build build.windows --target PXTOOL
```

Expected: build succeeds.

- [ ] **Step 10: Manual menu smoke test**

Run `build.windows\PXTOOL.exe`, open `Window > Themes`, and verify:

- Six menu items are present.
- Selecting `Dark` and `Light` still works.
- Selecting each new preset does not crash.
- The checked menu action follows the active theme.
- Closing and reopening the app preserves the selected theme.

- [ ] **Step 11: Commit**

```powershell
git add PXTOOL/pv/mainwindow.h PXTOOL/pv/mainwindow.cpp PXTOOL/pv/toolbars/trigbar.h PXTOOL/pv/toolbars/trigbar.cpp PXTOOL/pv/theme/thememanager.h PXTOOL/pv/theme/thememanager.cpp
git commit -m "feat: expose built-in theme presets"
```

## Task 4: Token-Aware Self-Painted UI

**Files:**
- Modify: `PXTOOL/pv/mainwindow.cpp`
- Modify: `PXTOOL/pv/dock/sidebar.cpp`
- Modify: `PXTOOL/pv/view/edge_nav_button.cpp`
- Modify: `PXTOOL/pv/view/verticalscrollbar.cpp`
- Modify: `PXTOOL/pv/view/waveformscrollbar.cpp`
- [ ] **Step 1: Add a small local helper pattern for token fallback**

In each touched `.cpp` file, use this pattern near existing theme-sensitive code:

```cpp
static QColor themeColor(const QString &token, const QColor &fallback)
{
    QColor color = AppConfig::Instance().GetThemeColor(token);
    return color.isValid() ? color : fallback;
}
```

Only add it to files where it is used.

- [ ] **Step 2: Preserve legacy session tab colors**

In `MainWindow::rebuild_tab_buttons()` keep the existing `if (isDark)` values for `dark` and `light`. Add token values only for non-legacy themes:

```cpp
        const bool legacy = pv::theme::ThemeManager::isLegacyTheme(app.frameOptions.style);
        bool isDark = AppConfig::Instance().IsDarkStyle();
        QString bgColor;
        QString selBg;
        QString textColor;
        QString selText;

        if (legacy) {
            bgColor   = isDark ? "#1E1E1E" : "#E8E8E8";
            selBg     = isDark ? "#2D2D2D" : "#FFFFFF";
            textColor = isDark ? "#CCCCCC" : "#333333";
            selText   = isDark ? "#FFFFFF" : "#000000";
        } else {
            bgColor   = themeColor("@tabview-bg", isDark ? QColor("#1E1E1E") : QColor("#E8E8E8")).name();
            selBg     = themeColor("@bg-overlay", isDark ? QColor("#2D2D2D") : QColor("#FFFFFF")).name();
            textColor = themeColor("@fg-base", isDark ? QColor("#CCCCCC") : QColor("#333333")).name();
            selText   = themeColor("@fg-bright", isDark ? QColor("#FFFFFF") : QColor("#000000")).name();
        }
```

Add `#include "theme/thememanager.h"` if not already present.

- [ ] **Step 3: Tokenize disk cache footer separator for presets**

In `MainWindow::update_disk_cache_footer()`, preserve current legacy branches and use `@border-strong` for non-legacy themes:

```cpp
        const bool legacy = pv::theme::ThemeManager::isLegacyTheme(app.frameOptions.style);
        const bool isDark = AppConfig::Instance().IsDarkStyle();
        const QString border = legacy
            ? (isDark ? QStringLiteral("#333333") : QStringLiteral("#BBBBBB"))
            : themeColor("@border-strong", isDark ? QColor("#333333") : QColor("#BBBBBB")).name();
```

- [ ] **Step 4: Tokenize sidebar section titles for presets**

In `PXTOOL/pv/dock/sidebar.cpp`, keep the existing stylesheet for legacy themes. For non-legacy themes use:

```cpp
    QColor bg = themeColor("@sidebar-bg", dark ? QColor("#202020") : QColor("#F8F8F8"));
    QColor fg = themeColor("@fg-base", dark ? QColor("#EFF0F1") : QColor("#2A2A2A"));
    QColor accent = themeColor("@sidebar-accent", QColor("#528BFF"));
```

Apply these colors to the existing title stylesheet string without changing layout or text.

- [ ] **Step 5: Tokenize edge navigation buttons**

In `PXTOOL/pv/view/edge_nav_button.cpp`, replace hard-coded dark/light assignments only for non-legacy themes:

```cpp
    const bool legacy = pv::theme::ThemeManager::isLegacyTheme(AppConfig::Instance().frameOptions.style);
    if (!legacy) {
        _bgColor = themeColor("@panel-bg", _bgColor);
        _borderColor = themeColor("@border-strong", _borderColor);
        _arrowColor = themeColor("@panel-text", _arrowColor);
    }
```

Preserve the current legacy code path above it.

- [ ] **Step 6: Tokenize waveform and vertical scrollbars**

In `PXTOOL/pv/view/verticalscrollbar.cpp` and `PXTOOL/pv/view/waveformscrollbar.cpp`, preserve existing dark/light branches. For non-legacy themes, derive colors from:

```cpp
QColor track = themeColor("@bg-base", isDark ? QColor("#202020") : QColor("#F8F8F8"));
QColor thumb = themeColor("@bg-overlay", isDark ? QColor("#3A3A3A") : QColor("#D0D0D0"));
QColor border = themeColor("@border-strong", isDark ? QColor("#37373B") : QColor("#D5D5D5"));
```

Use the variable names already present in each file rather than introducing duplicate state.

- [ ] **Step 7: Build app and smoke test visual areas**

Run:

```powershell
cmake --build build.windows --target PXTOOL
```

Manual test in `build.windows\PXTOOL.exe`:

- `dark` still matches previous Dark baseline in menu, toolbar, tabs, sidebar, and waveform area.
- `light` still matches previous Light baseline in menu, toolbar, tabs, sidebar, and waveform area.
- `atom`, `ayu`, `dark_cards`, and `light_cards` visibly affect tabs, sidebar title areas, scrollbars, and edge navigation buttons.

- [ ] **Step 8: Commit**

```powershell
git add PXTOOL/pv/mainwindow.cpp PXTOOL/pv/dock/sidebar.cpp PXTOOL/pv/view/edge_nav_button.cpp PXTOOL/pv/view/verticalscrollbar.cpp PXTOOL/pv/view/waveformscrollbar.cpp
git commit -m "feat: apply theme tokens to custom UI"
```

## Task 5: Final Verification and Documentation Touches

**Files:**
- Modify: `PXTOOL_User_Manual_en.md`
- Modify: `PXTOOL_User_Manual_zh.md`

- [ ] **Step 1: Update manual theme list**

In `PXTOOL_User_Manual_en.md`, update section `8.1 Themes` from dark/light-only wording to:

```markdown
PXTOOL supports Dark, Light, Atom One Dark, Ayu Light, Dark Colored Cards, and Light Colored Cards themes. Switch themes from the Window menu or Display / Themes.
```

In `PXTOOL_User_Manual_zh.md`, update section `8.1 主题` to the Chinese equivalent of:

```markdown
PXTOOL supports Dark, Light, Atom One Dark, Ayu Light, Dark Colored Cards, and Light Colored Cards themes. Switch themes from the Window menu or Display / Themes.
```

- [ ] **Step 2: Run unit tests**

Run:

```powershell
cmake --build build.windows --target DSView-test
.\build.windows\DSView-test.exe
```

Expected: all tests pass.

- [ ] **Step 3: Run full app build**

Run:

```powershell
cmake --build build.windows --target PXTOOL
```

Expected: build succeeds.

- [ ] **Step 4: Manual persistence verification**

Run:

```powershell
.\build.windows\PXTOOL.exe
```

Verify:

- Select `Atom One Dark`, close the app, reopen, and confirm Atom remains selected.
- Select `Ayu Light`, close the app, reopen, and confirm Ayu remains selected.
- Select `Dark`, close the app, reopen, and confirm original Dark remains selected and visually unchanged.
- Select `Light`, close the app, reopen, and confirm original Light remains selected and visually unchanged.

- [ ] **Step 5: Manual fallback verification**

Temporarily remove `atom.json` from `PXTOOL/themes/breeze.qrc` in a throwaway local edit, rebuild resources, and select Atom. Expected: app logs an error and falls back to Dark without an unstyled UI. Restore `breeze.qrc` before committing.

- [ ] **Step 6: Check git diff for accidental Style editor work**

Run:

```powershell
git diff --name-only HEAD~4..HEAD
rg -n "Style page|Save as Preset|pxstyle|QColorDialog|getSaveFileName|userThemePath" PXTOOL
```

Expected: no Style editor, `.pxstyle`, user theme directory scanning, or custom color picker code was added.

- [ ] **Step 7: Commit docs**

```powershell
git add PXTOOL_User_Manual_en.md PXTOOL_User_Manual_zh.md
git commit -m "docs: document built-in theme presets"
```

- [ ] **Step 8: Final status**

Run:

```powershell
git status --short
```

Expected: no uncommitted changes except user-owned unrelated files, if any existed before this work.
