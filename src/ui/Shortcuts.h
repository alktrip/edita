// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
//
// Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team).
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <QKeySequence>
#include <QString>

#include <map>
#include <vector>

namespace edita {

// Un atajo como lo guarda N++: Ctrl/Alt/Mayús y la tecla virtual de Windows (VK_*)
struct KeyCombo
{
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    int key = 0; // 0 = sin atajo

    bool isEmpty() const { return key == 0; }
    QKeySequence toKeySequence() const;
    // Vacío si la tecla no tiene equivalente en Windows (no se podría guardar)
    static KeyCombo fromKeySequence(const QKeySequence& sequence);

    bool operator==(const KeyCombo&) const = default;
};

// <PluginCommand moduleName="…" internalID="…" …/>: la orden número `index` de
// getFuncsArray del plugin `module` (el nombre del archivo, "mimeTools.so")
struct PluginShortcut
{
    QString module;
    int index = 0;
    KeyCombo key;

    bool operator==(const PluginShortcut&) const = default;
};

// Los atajos cambiados por el usuario en shortcuts.xml (Shortcut Mapper de N++).
// Lo que no está aquí tiene su atajo de serie.
struct ShortcutsConfig
{
    std::map<int, KeyCombo> internal;               // <InternalCommands><Shortcut id="IDM_…">
    std::map<QString, KeyCombo> edita;              // <EditaCommands><Shortcut name="…">: órdenes sin IDM en N++
    std::vector<PluginShortcut> plugins;            // <PluginCommands>
    std::map<int, std::vector<KeyCombo>> scintilla; // <ScintillaKeys><ScintKey ScintID="SCI_…"> (+ <NextKey>)

    bool operator==(const ShortcutsConfig&) const = default;
};

// Una fila del Shortcut Mapper: una orden del menú, una macro, una orden de un
// plugin o una de Scintilla, con sus atajos actuales y los de serie
struct ShortcutEntry
{
    enum class Kind { Menu, Macros, RunCommands, Plugin, Scintilla }; // el orden de las pestañas de N++
    Kind kind = Kind::Menu;
    QString id;       // objectName de la acción, índice de la macro o de la orden, "módulo:índice", SCI_*
    QString name;
    QString category; // menú, plugin
    std::vector<KeyCombo> keys;
    std::vector<KeyCombo> defaults;
};

// NppParameters::feedShortcut/feedPluginCustomizedCmds/feedScintKeys. Un archivo que
// no existe da una configuración vacía y true.
bool readShortcuts(const QString& path, ShortcutsConfig& config);
// Reescribe esas secciones conservando <Macros> y <UserDefinedCommands> (o crea el archivo)
bool writeShortcuts(const QString& path, const ShortcutsConfig& config);

// SCI_ASSIGNCMDKEY: tecla de Scintilla (SCK_* o carácter) con los modificadores
// (SCMOD_*) en los 16 bits altos; 0 si no tiene equivalente
int scintillaKeyDefinition(const KeyCombo& combo);

} // namespace edita
