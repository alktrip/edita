// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "syntax/DesktopFonts.h"

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTextStream>

namespace edita::desktopfonts {

namespace {

QtMessageHandler g_previous = nullptr;
std::optional<QFont> g_fixed;  // la de kdeglobals, si Qt no pudo leerla

void filter(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtWarningMsg && message.startsWith(QLatin1String("QFont::fromString: Invalid description")))
        return;
    if (g_previous)
        g_previous(type, context, message);
}

// ¿Qt entiende esta descripción? (sin que el aviso llegue a la consola)
bool qtUnderstands(const QString& description)
{
    QFont font;
    return font.fromString(description);
}

} // namespace

void installWarningFilter()
{
    g_previous = qInstallMessageHandler(&filter);
}

QStringList kdeglobalsFiles()
{
    // ~/.config primero y después XDG_CONFIG_DIRS (kdedefaults, /etc/xdg), como KConfig
    QStringList files;
    for (const QString& dir : QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation))
        files << QDir(dir).filePath(QStringLiteral("kdeglobals"));
    return files;
}

QString kdeglobalsValue(const QString& key, const QStringList& files)
{
    // Lectura directa: QSettings tomaría las comas como una lista
    for (const QString& path : files) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QTextStream in(&file);
        bool general = false;
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.startsWith(QLatin1Char('['))) {
                general = line == QLatin1String("[General]");
                continue;
            }
            if (!general)
                continue;
            const qsizetype equals = line.indexOf(QLatin1Char('='));
            if (equals > 0 && line.left(equals).trimmed() == key)
                return line.mid(equals + 1).trimmed();
        }
    }
    return {};
}

// «familia,tamaño,píxeles,pista,grosor,estilo,…» con el número de campos de cualquier
// versión de Qt: los cuatro que importan están siempre en el mismo sitio
std::optional<QFont> parseFontDescription(const QString& description)
{
    const QStringList fields = description.split(QLatin1Char(','));
    if (fields.size() < 2 || fields[0].trimmed().isEmpty())
        return std::nullopt;
    QFont font(fields[0].trimmed());
    bool ok = false;
    const double points = fields[1].toDouble(&ok);
    if (ok && points > 0)
        font.setPointSizeF(points);
    if (fields.size() > 4) {
        const int weight = fields[4].toInt(&ok);
        // Qt 6: 1–1000 (400 normal); Qt 5: 0–99 (50 normal)
        if (ok && weight > 0)
            font.setWeight(static_cast<QFont::Weight>(weight > 99 ? std::clamp(weight, 1, 1000) : weight * 8));
    }
    if (fields.size() > 5)
        font.setItalic(fields[5].toInt() != 0);
    return font;
}

void apply()
{
    const QStringList files = kdeglobalsFiles();
    const QString general = kdeglobalsValue(QStringLiteral("font"), files);
    if (!general.isEmpty() && !qtUnderstands(general)) {
        if (const std::optional<QFont> font = parseFontDescription(general))
            QGuiApplication::setFont(*font);
    }
    const QString fixed = kdeglobalsValue(QStringLiteral("fixed"), files);
    if (!fixed.isEmpty() && !qtUnderstands(fixed))
        g_fixed = parseFontDescription(fixed);
}

QFont fixedFont()
{
    if (g_fixed)
        return *g_fixed;
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

} // namespace edita::desktopfonts
