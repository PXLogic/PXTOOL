/*
 * This file is part of the PXTOOL project.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "thememanager.h"

#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>

namespace pv {
namespace theme {

namespace {

void writeThemedSvgResources(QString &styleSheet, const QHash<QString, QString> &tokens)
{
    QRegularExpression imageExpression(QStringLiteral(R"(image:\s*url\((:[^)]+\.svg)\))"));
    QRegularExpressionMatchIterator matches = imageExpression.globalMatch(styleSheet);

    QHash<QString, QString> themedPaths;
    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                             + QStringLiteral("/DSView-theme-svg");

    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const QString resourcePath = match.captured(1);
        if (themedPaths.contains(resourcePath))
            continue;

        QFile svgFile(resourcePath);
        if (!svgFile.open(QFile::ReadOnly | QFile::Text))
            continue;

        QString svg = QString::fromUtf8(svgFile.readAll());
        ThemeManager::replaceTokens(svg, tokens);

        QDir dir(tempRoot);
        if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
            continue;

        const QByteArray svgBytes = svg.toUtf8();
        QString fileName = resourcePath;
        fileName.remove(0, 2);
        fileName.replace(QLatin1Char('/'), QLatin1Char('_'));
        fileName.replace(QStringLiteral(".svg"),
                         QStringLiteral("_%1.svg").arg(QString::fromLatin1(
                             QCryptographicHash::hash(svgBytes, QCryptographicHash::Sha1).toHex().left(12))));

        const QString themedPath = dir.absoluteFilePath(fileName);
        QFile themedFile(themedPath);
        if (!themedFile.open(QFile::WriteOnly | QFile::Text | QFile::Truncate))
            continue;

        themedFile.write(svgBytes);
        themedPaths.insert(resourcePath, themedPath);
    }

    for (auto it = themedPaths.constBegin(); it != themedPaths.constEnd(); ++it)
        styleSheet.replace(QStringLiteral("url(%1)").arg(it.key()),
                           QStringLiteral("url(%1)").arg(it.value()));
}

} // namespace

QList<ThemeInfo> ThemeManager::builtInThemes()
{
    return {
        {ThemeId::Dark, "Dark", QString(), true, true},
        {ThemeId::Light, "Light", QString(), true, false},
        {ThemeId::Atom, "Atom One Dark", ":/atom.json", false, true},
        {ThemeId::Ayu, "Ayu Light", ":/ayu.json", false, false},
        {ThemeId::DarkCards, "Dark Colored Cards", ":/dark_cards.json", false, true},
        {ThemeId::LightCards, "Light Colored Cards", ":/light_cards.json", false, false},
    };
}

ThemeInfo ThemeManager::themeInfo(const QString &id)
{
    const QString normalizedId = normalizeId(id);
    const QList<ThemeInfo> themes = builtInThemes();
    for (const ThemeInfo &theme : themes) {
        if (theme.id == normalizedId)
            return theme;
    }
    return themes.first();
}

QString ThemeManager::normalizeId(const QString &id)
{
    const QString trimmedId = id.trimmed();
    const QList<ThemeInfo> themes = builtInThemes();
    for (const ThemeInfo &theme : themes) {
        if (theme.id == trimmedId)
            return theme.id;
    }
    return ThemeId::Dark;
}

bool ThemeManager::isKnownTheme(const QString &id)
{
    const QList<ThemeInfo> themes = builtInThemes();
    for (const ThemeInfo &theme : themes) {
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
    if (normalizeId(id) == ThemeId::Light) {
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

bool ThemeManager::loadTokenPreset(const QString &id, QHash<QString, QString> &tokens, QString *errorMessage)
{
    const ThemeInfo theme = themeInfo(id);
    if (theme.legacy) {
        tokens = legacyTokens(theme.id);
        return true;
    }

    tokens.clear();

    QFile file(theme.jsonResource);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        if (errorMessage)
            *errorMessage = QString("Unable to open theme preset: %1").arg(theme.jsonResource);
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage)
            *errorMessage = QString("Invalid theme preset: %1").arg(theme.jsonResource);
        return false;
    }

    const QJsonObject tokenObject = doc.object().value("tokens").toObject();
    if (tokenObject.isEmpty()) {
        if (errorMessage)
            *errorMessage = QString("Theme preset %1 has no tokens").arg(theme.jsonResource);
        return false;
    }

    for (auto it = tokenObject.constBegin(); it != tokenObject.constEnd(); ++it) {
        if (!it.value().isString()) {
            tokens.clear();
            if (errorMessage)
                *errorMessage = QString("Theme preset %1 has non-string token: %2")
                                    .arg(theme.jsonResource)
                                    .arg(it.key());
            return false;
        }
        tokens.insert(it.key(), it.value().toString());
    }

    if (tokens.isEmpty()) {
        if (errorMessage)
            *errorMessage = QString("Theme preset %1 has no string tokens").arg(theme.jsonResource);
        return false;
    }

    return true;
}

bool ThemeManager::buildStyleSheet(const QString &id, QHash<QString, QString> &tokens, QString &styleSheet, QString *errorMessage)
{
    QFile file(":/theme.qss");
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        if (errorMessage)
            *errorMessage = "Unable to open :/theme.qss";
        return false;
    }

    styleSheet = QString::fromUtf8(file.readAll());

    if (!loadTokenPreset(id, tokens, errorMessage))
        return false;

    replaceTokens(styleSheet, tokens);
    writeThemedSvgResources(styleSheet, tokens);
    return true;
}

void ThemeManager::replaceTokens(QString &content, const QHash<QString, QString> &tokens)
{
    QList<QString> keys = tokens.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &left, const QString &right) {
        return left.size() > right.size();
    });

    for (const QString &key : keys)
        content.replace(key, tokens.value(key));
}

} // namespace theme
} // namespace pv
