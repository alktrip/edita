// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// El menú contextual del editor según contextMenu.xml (NppParameters::getContextMenuFromXmlTree):
// <NotepadPlus><ScintillaContextMenu><Item …/></ScintillaContextMenu></NotepadPlus>

#pragma once

#include <QString>

#include <vector>

namespace edita {

struct ContextMenuItem
{
    int id = -1;           // id="…": IDM_* de N++; 0 es un separador
    QString menuEntry;     // MenuEntryName="Edit"
    QString menuItem;      // MenuItemName="Cut" (en inglés)
    QString pluginEntry;   // PluginEntryName="MIME Tools"
    QString pluginCommand; // PluginCommandItemName="Base64 Encode"
    QString folder;        // FolderName: el submenú donde va
    QString displayAs;     // ItemNameAs: otro texto

    bool isSeparator() const { return id == 0; }
};

// Los elementos de <ScintillaContextMenu> (vacío si el archivo no existe o no vale)
std::vector<ContextMenuItem> readContextMenu(const QString& path, bool* ok = nullptr);

// El id de N++ de una orden por su nombre en inglés (el de english.xml, sin & ni «...»; sin
// distinguir mayúsculas), mirando primero las del menú `menuEntry` (File, Edit, Search…). -1
// si no la hay
int nppCommandIdFromName(const QString& menuEntry, const QString& itemName);

// Sin el & de las teclas de acceso ni los puntos suspensivos finales, como compara N++
QString contextMenuText(const QString& text);

} // namespace edita
