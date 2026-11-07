// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/ContextMenuXml.h"

#include <QRegularExpression>

#include <pugixml.hpp>

#include <utility>

namespace edita {

namespace {

struct CommandName
{
    const char* name;
    int id;
};

const CommandName kCommandNames[] = {
#include "core/NppCommandNames.inc"
};

// Las órdenes de cada menú de N++ por su rango de ids (menuCmdID.h): así «Edit/Copy» es la
// de Editar aunque otra orden se llame igual
std::pair<int, int> menuRange(const QString& entry)
{
    const QString e = contextMenuText(entry).toLower();
    if (e == QLatin1String("file"))
        return {41000, 41999};
    if (e == QLatin1String("edit"))
        return {42000, 42999};
    if (e == QLatin1String("search"))
        return {43000, 43999};
    if (e == QLatin1String("view"))
        return {44000, 44999};
    if (e == QLatin1String("encoding"))
        return {45000, 45999};
    if (e == QLatin1String("language"))
        return {46000, 46999};
    if (e == QLatin1String("settings"))
        return {48000, 48499};
    if (e == QLatin1String("tools"))
        return {48500, 48999};
    if (e == QLatin1String("run"))
        return {49000, 49999};
    if (e == QLatin1String("?") || e == QLatin1String("help"))
        return {47000, 47999};
    return {0, 0};
}

} // namespace

QString contextMenuText(const QString& text)
{
    QString plain = text;
    plain.remove(QRegularExpression(QStringLiteral("\\t.*$"))); // el atajo, si lo trae
    plain.replace(QLatin1String("&&"), QLatin1String("\x01"));
    plain.remove(QLatin1Char('&'));
    plain.replace(QLatin1Char('\x01'), QLatin1Char('&'));
    plain = plain.trimmed();
    for (const QString& dots : {QStringLiteral("..."), QStringLiteral("…")}) {
        if (plain.endsWith(dots))
            plain.chop(dots.size());
    }
    return plain.trimmed();
}

int nppCommandIdFromName(const QString& menuEntry, const QString& itemName)
{
    const QString wanted = contextMenuText(itemName);
    const auto [low, high] = menuRange(menuEntry);
    int any = -1;
    for (const CommandName& command : kCommandNames) {
        if (QString::fromUtf8(command.name).compare(wanted, Qt::CaseInsensitive) != 0)
            continue;
        if (command.id >= low && command.id <= high)
            return command.id;
        if (any < 0)
            any = command.id;
    }
    return any;
}

std::vector<ContextMenuItem> readContextMenu(const QString& path, bool* ok)
{
    std::vector<ContextMenuItem> items;
    pugi::xml_document doc;
    const bool loaded = doc.load_file(path.toUtf8().constData(), pugi::parse_default, pugi::encoding_utf8);
    const pugi::xml_node menu = doc.child("NotepadPlus").child("ScintillaContextMenu");
    if (ok)
        *ok = loaded && menu;
    for (const pugi::xml_node node : menu.children("Item")) {
        ContextMenuItem item;
        item.id = node.attribute("id").as_int(-1);
        item.menuEntry = QString::fromUtf8(node.attribute("MenuEntryName").as_string());
        item.menuItem = QString::fromUtf8(node.attribute("MenuItemName").as_string());
        item.pluginEntry = QString::fromUtf8(node.attribute("PluginEntryName").as_string());
        item.pluginCommand = QString::fromUtf8(node.attribute("PluginCommandItemName").as_string());
        item.folder = QString::fromUtf8(node.attribute("FolderName").as_string());
        item.displayAs = QString::fromUtf8(node.attribute("ItemNameAs").as_string());
        items.push_back(std::move(item));
    }
    return items;
}

} // namespace edita
