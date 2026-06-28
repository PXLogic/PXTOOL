#include <boost/test/unit_test.hpp>

#include "../pv/theme/thememanager.h"

using pv::theme::ThemeId;
using pv::theme::ThemeManager;

BOOST_AUTO_TEST_SUITE(ThemeManagerTest)

BOOST_AUTO_TEST_CASE(provides_built_in_theme_metadata_in_menu_order)
{
    const QList<pv::theme::ThemeInfo> themes = ThemeManager::builtInThemes();

    BOOST_REQUIRE_EQUAL(themes.size(), 6);

    BOOST_CHECK_EQUAL(themes[0].id.toStdString(), ThemeId::Dark);
    BOOST_CHECK_EQUAL(themes[0].label.toStdString(), "Dark");
    BOOST_CHECK(themes[0].jsonResource.isEmpty());
    BOOST_CHECK(themes[0].legacy);
    BOOST_CHECK(themes[0].darkLike);

    BOOST_CHECK_EQUAL(themes[1].id.toStdString(), ThemeId::Light);
    BOOST_CHECK_EQUAL(themes[1].label.toStdString(), "Light");
    BOOST_CHECK(themes[1].jsonResource.isEmpty());
    BOOST_CHECK(themes[1].legacy);
    BOOST_CHECK(!themes[1].darkLike);

    BOOST_CHECK_EQUAL(themes[2].id.toStdString(), ThemeId::Atom);
    BOOST_CHECK_EQUAL(themes[2].label.toStdString(), "Atom One Dark");
    BOOST_CHECK_EQUAL(themes[2].jsonResource.toStdString(), ":/atom.json");
    BOOST_CHECK(!themes[2].legacy);
    BOOST_CHECK(themes[2].darkLike);

    BOOST_CHECK_EQUAL(themes[3].id.toStdString(), ThemeId::Ayu);
    BOOST_CHECK_EQUAL(themes[3].label.toStdString(), "Ayu Light");
    BOOST_CHECK_EQUAL(themes[3].jsonResource.toStdString(), ":/ayu.json");
    BOOST_CHECK(!themes[3].legacy);
    BOOST_CHECK(!themes[3].darkLike);

    BOOST_CHECK_EQUAL(themes[4].id.toStdString(), ThemeId::DarkCards);
    BOOST_CHECK_EQUAL(themes[4].label.toStdString(), "Dark Colored Cards");
    BOOST_CHECK_EQUAL(themes[4].jsonResource.toStdString(), ":/dark_cards.json");
    BOOST_CHECK(!themes[4].legacy);
    BOOST_CHECK(themes[4].darkLike);

    BOOST_CHECK_EQUAL(themes[5].id.toStdString(), ThemeId::LightCards);
    BOOST_CHECK_EQUAL(themes[5].label.toStdString(), "Light Colored Cards");
    BOOST_CHECK_EQUAL(themes[5].jsonResource.toStdString(), ":/light_cards.json");
    BOOST_CHECK(!themes[5].legacy);
    BOOST_CHECK(!themes[5].darkLike);
}

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
