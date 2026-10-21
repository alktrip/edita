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

#include "ui/Shortcuts.h"

#include "core/Macro.h"

#include <QFile>
#include <QFileInfo>

#include <pugixml.hpp>

namespace edita {

namespace {

bool isYes(const pugi::xml_node& node, const char* name)
{
    return std::string_view(node.attribute(name).as_string("no")) == "yes";
}

KeyCombo readCombo(const pugi::xml_node& node)
{
    return KeyCombo{isYes(node, "Ctrl"), isYes(node, "Alt"), isYes(node, "Shift"), node.attribute("Key").as_int(0)};
}

void writeCombo(pugi::xml_node node, const KeyCombo& combo)
{
    node.append_attribute("Ctrl") = combo.ctrl ? "yes" : "no";
    node.append_attribute("Alt") = combo.alt ? "yes" : "no";
    node.append_attribute("Shift") = combo.shift ? "yes" : "no";
    node.append_attribute("Key") = combo.key;
}

pugi::xml_node section(pugi::xml_node root, const char* name)
{
    pugi::xml_node node = root.child(name);
    if (node)
        node.remove_children();
    else
        node = root.append_child(name);
    return node;
}

} // namespace

QKeySequence KeyCombo::toKeySequence() const
{
    const int qtKey = qtKeyFromVirtualKey(key);
    if (qtKey == 0)
        return {};
    Qt::KeyboardModifiers mods;
    if (ctrl)
        mods |= Qt::ControlModifier;
    if (alt)
        mods |= Qt::AltModifier;
    if (shift)
        mods |= Qt::ShiftModifier;
    return QKeySequence(QKeyCombination(mods, static_cast<Qt::Key>(qtKey)));
}

KeyCombo KeyCombo::fromKeySequence(const QKeySequence& sequence)
{
    if (sequence.isEmpty())
        return {};
    const QKeyCombination combination = sequence[0];
    const int vk = virtualKeyFromQtKey(combination.key());
    if (vk == 0)
        return {};
    const Qt::KeyboardModifiers mods = combination.keyboardModifiers();
    return KeyCombo{mods.testFlag(Qt::ControlModifier), mods.testFlag(Qt::AltModifier), mods.testFlag(Qt::ShiftModifier), vk};
}

bool readShortcuts(const QString& path, ShortcutsConfig& config)
{
    config = ShortcutsConfig{};
    if (!QFileInfo::exists(path))
        return true;
    pugi::xml_document doc;
    if (!doc.load_file(QFile::encodeName(path).constData()))
        return false;
    const pugi::xml_node root = doc.child("NotepadPlus");
    for (const pugi::xml_node node : root.child("InternalCommands").children("Shortcut")) {
        if (const int id = node.attribute("id").as_int(0))
            config.internal[id] = readCombo(node);
    }
    for (const pugi::xml_node node : root.child("EditaCommands").children("Shortcut")) {
        const QString name = QString::fromUtf8(node.attribute("name").as_string(""));
        if (!name.isEmpty())
            config.edita[name] = readCombo(node);
    }
    for (const pugi::xml_node node : root.child("PluginCommands").children("PluginCommand")) {
        PluginShortcut plugin;
        plugin.module = QString::fromUtf8(node.attribute("moduleName").as_string(""));
        plugin.index = node.attribute("internalID").as_int(-1);
        plugin.key = readCombo(node);
        if (!plugin.module.isEmpty() && plugin.index >= 0)
            config.plugins.push_back(plugin);
    }
    for (const pugi::xml_node node : root.child("ScintillaKeys").children("ScintKey")) {
        const int id = node.attribute("ScintID").as_int(0);
        if (!id)
            continue;
        std::vector<KeyCombo>& keys = config.scintilla[id];
        if (const KeyCombo first = readCombo(node); !first.isEmpty())
            keys.push_back(first);
        for (const pugi::xml_node next : node.children("NextKey")) {
            if (const KeyCombo combo = readCombo(next); !combo.isEmpty())
                keys.push_back(combo);
        }
    }
    return true;
}

bool writeShortcuts(const QString& path, const ShortcutsConfig& config)
{
    pugi::xml_document doc;
    if (QFileInfo::exists(path) && !doc.load_file(QFile::encodeName(path).constData()))
        return false; // no se pisa un shortcuts.xml que no se entiende
    if (!doc.child("NotepadPlus")) {
        pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
        decl.append_attribute("version") = "1.0";
        decl.append_attribute("encoding") = "UTF-8";
        pugi::xml_node root = doc.append_child("NotepadPlus");
        root.append_child("InternalCommands");
        root.append_child("Macros");
        root.append_child("UserDefinedCommands");
        root.append_child("PluginCommands");
        root.append_child("ScintillaKeys");
    }
    pugi::xml_node root = doc.child("NotepadPlus");

    pugi::xml_node internal = section(root, "InternalCommands");
    for (const auto& [id, combo] : config.internal) {
        pugi::xml_node node = internal.append_child("Shortcut");
        node.append_attribute("id") = id;
        writeCombo(node, combo);
    }
    pugi::xml_node plugins = section(root, "PluginCommands");
    for (const PluginShortcut& plugin : config.plugins) {
        pugi::xml_node node = plugins.append_child("PluginCommand");
        node.append_attribute("moduleName") = plugin.module.toUtf8().constData();
        node.append_attribute("internalID") = plugin.index;
        writeCombo(node, plugin.key);
    }
    pugi::xml_node scintilla = section(root, "ScintillaKeys");
    for (const auto& [id, keys] : config.scintilla) {
        pugi::xml_node node = scintilla.append_child("ScintKey");
        node.append_attribute("ScintID") = id;
        node.append_attribute("menuCmdID") = 0;
        writeCombo(node, keys.empty() ? KeyCombo{} : keys.front());
        for (std::size_t i = 1; i < keys.size(); ++i)
            writeCombo(node.append_child("NextKey"), keys[i]);
    }
    // Las órdenes de Edita que N++ no tiene, en una sección propia (N++ la ignora)
    if (!config.edita.empty() || root.child("EditaCommands")) {
        pugi::xml_node edita = section(root, "EditaCommands");
        for (const auto& [name, combo] : config.edita) {
            pugi::xml_node node = edita.append_child("Shortcut");
            node.append_attribute("name") = name.toUtf8().constData();
            writeCombo(node, combo);
        }
    }
    return doc.save_file(QFile::encodeName(path).constData(), "    ", pugi::format_default, pugi::encoding_utf8);
}

int scintillaKeyDefinition(const KeyCombo& combo)
{
    // ScintillaKeyMap::toKeyDef: VK_* → SCK_* (o el carácter)
    int key = 0;
    switch (combo.key) {
    case 0x28: key = 300; break; // SCK_DOWN
    case 0x26: key = 301; break; // SCK_UP
    case 0x25: key = 302; break; // SCK_LEFT
    case 0x27: key = 303; break; // SCK_RIGHT
    case 0x24: key = 304; break; // SCK_HOME
    case 0x23: key = 305; break; // SCK_END
    case 0x21: key = 306; break; // SCK_PRIOR
    case 0x22: key = 307; break; // SCK_NEXT
    case 0x2E: key = 308; break; // SCK_DELETE
    case 0x2D: key = 309; break; // SCK_INSERT
    case 0x1B: key = 7; break;   // SCK_ESCAPE
    case 0x08: key = 8; break;   // SCK_BACK
    case 0x09: key = 9; break;   // SCK_TAB
    case 0x0D: key = 13; break;  // SCK_RETURN
    case 0x6B: key = 310; break; // SCK_ADD
    case 0x6D: key = 311; break; // SCK_SUBTRACT
    case 0x6F: key = 312; break; // SCK_DIVIDE
    case 0xBA: key = ';'; break; // VK_OEM_1…
    case 0xBB: key = '='; break;
    case 0xBC: key = ','; break;
    case 0xBD: key = '-'; break;
    case 0xBE: key = '.'; break;
    case 0xBF: key = '/'; break;
    case 0xC0: key = '`'; break;
    case 0xDB: key = '['; break;
    case 0xDC: key = '\\'; break;
    case 0xDD: key = ']'; break;
    case 0xDE: key = '\''; break;
    default:
        if ((combo.key >= 'A' && combo.key <= 'Z') || (combo.key >= '0' && combo.key <= '9') || combo.key == ' ')
            key = combo.key;
        break;
    }
    if (key == 0)
        return 0;
    const int mods = (combo.shift ? 1 : 0) | (combo.ctrl ? 2 : 0) | (combo.alt ? 4 : 0); // SCMOD_SHIFT/CTRL/ALT
    return key | (mods << 16);
}

} // namespace edita
