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


#include "core/Macro.h"

#include <pugixml.hpp>

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <cstring>
#include <sstream>

namespace edita {

namespace {

// Mensajes de Scintilla que necesita la normalización (Scintilla.h)
constexpr int kSciReplaceSel = 2170;
constexpr int kSciNewLine = 2329;

// IDM_EDIT_TRIMTRAILING e IDM_FILE_SAVE de N++ (menuCmdID.h)
constexpr int kIdmEditTrimTrailing = 42024;
constexpr int kIdmFileSave = 41006;

MacroStep newLineStep()
{
    MacroStep step;
    step.type = MacroStep::UseLParameter;
    step.message = kSciNewLine;
    return step;
}

bool isYes(const pugi::xml_node& node, const char* attr)
{
    return std::strcmp(node.attribute(attr).as_string("no"), "yes") == 0;
}

void readActions(const pugi::xml_node& macroNode, Macro& macro)
{
    for (const pugi::xml_node action : macroNode.children("Action")) {
        const int type = action.attribute("type").as_int(4);
        if (type < 0 || type > 3)
            continue;
        MacroStep step;
        step.type = static_cast<MacroStep::Type>(type);
        step.message = action.attribute("message").as_int(0);
        step.wParam = static_cast<std::uintptr_t>(action.attribute("wParam").as_ullong(0));
        step.lParam = static_cast<std::intptr_t>(action.attribute("lParam").as_llong(0));
        step.sParam = action.attribute("sParam").as_string("");

        // Macros antiguas: un SCI_REPLACESEL con el salto de línea pasa a SCI_NEWLINE, y
        // un "\r" seguido de "\n" o "\r\n" no debe dar dos saltos
        const bool prevIsCR = !macro.empty() && macro.back().message == kSciReplaceSel && macro.back().sParam == "\r";
        const bool isCR = step.sParam == "\r";
        if (step.message == kSciReplaceSel && (isCR || step.sParam == "\r\n" || step.sParam == "\n")) {
            if (prevIsCR) {
                if (isCR)
                    macro.back() = newLineStep();
                else
                    macro.pop_back();
            }
            macro.push_back(isCR ? step : newLineStep());
        } else {
            if (prevIsCR)
                macro.back() = newLineStep();
            macro.push_back(step);
        }
    }
    if (!macro.empty() && macro.back().message == kSciReplaceSel && macro.back().sParam == "\r")
        macro.back() = newLineStep();
}

} // namespace

bool readMacros(const QString& path, std::vector<SavedMacro>& macros)
{
    macros.clear();
    if (!QFileInfo::exists(path))
        return true;
    pugi::xml_document doc;
    if (!doc.load_file(QFile::encodeName(path).constData()))
        return false;
    for (const pugi::xml_node node : doc.child("NotepadPlus").child("Macros").children("Macro")) {
        SavedMacro macro;
        macro.name = QString::fromUtf8(node.attribute("name").as_string(""));
        if (macro.name.isEmpty())
            continue;
        macro.ctrl = isYes(node, "Ctrl");
        macro.alt = isYes(node, "Alt");
        macro.shift = isYes(node, "Shift");
        macro.key = node.attribute("Key").as_int(0);
        readActions(node, macro.steps);
        macros.push_back(std::move(macro));
    }
    return true;
}

bool writeMacros(const QString& path, const std::vector<SavedMacro>& macros)
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
    pugi::xml_node macrosNode = root.child("Macros");
    if (macrosNode)
        macrosNode.remove_children();
    else
        macrosNode = root.append_child("Macros");
    for (const SavedMacro& macro : macros) {
        pugi::xml_node node = macrosNode.append_child("Macro");
        node.append_attribute("name") = macro.name.toUtf8().constData();
        node.append_attribute("Ctrl") = macro.ctrl ? "yes" : "no";
        node.append_attribute("Alt") = macro.alt ? "yes" : "no";
        node.append_attribute("Shift") = macro.shift ? "yes" : "no";
        node.append_attribute("Key") = macro.key;
        for (const MacroStep& step : macro.steps) {
            pugi::xml_node action = node.append_child("Action");
            action.append_attribute("type") = static_cast<int>(step.type);
            action.append_attribute("message") = step.message;
            action.append_attribute("wParam") = static_cast<unsigned long long>(step.wParam);
            action.append_attribute("lParam") = static_cast<long long>(step.lParam);
            action.append_attribute("sParam") = step.sParam.c_str();
        }
    }
    std::ostringstream out;
    doc.save(out, "    ", pugi::format_indent);
    const std::string text = out.str();
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(text.data(), static_cast<qint64>(text.size())) == static_cast<qint64>(text.size()) && file.commit();
}

SavedMacro defaultTrimAndSaveMacro()
{
    SavedMacro macro;
    macro.name = QStringLiteral("Trim Trailing Space and Save");
    macro.alt = true;
    macro.shift = true;
    macro.key = 'S';
    MacroStep trim;
    trim.wParam = kIdmEditTrimTrailing;
    MacroStep save;
    save.wParam = kIdmFileSave;
    macro.steps = {trim, save};
    return macro;
}

int virtualKeyFromQtKey(int qtKey)
{
    // Letras y cifras: VK == código ASCII en mayúscula
    if ((qtKey >= 'A' && qtKey <= 'Z') || (qtKey >= '0' && qtKey <= '9'))
        return qtKey;
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
        return 0x70 + (qtKey - Qt::Key_F1); // VK_F1
    switch (qtKey) {
    case Qt::Key_Backspace: return 0x08;
    case Qt::Key_Tab: return 0x09;
    case Qt::Key_Return: return 0x0D;
    case Qt::Key_Escape: return 0x1B;
    case Qt::Key_Space: return 0x20;
    case Qt::Key_PageUp: return 0x21;
    case Qt::Key_PageDown: return 0x22;
    case Qt::Key_End: return 0x23;
    case Qt::Key_Home: return 0x24;
    case Qt::Key_Left: return 0x25;
    case Qt::Key_Up: return 0x26;
    case Qt::Key_Right: return 0x27;
    case Qt::Key_Down: return 0x28;
    case Qt::Key_Insert: return 0x2D;
    case Qt::Key_Delete: return 0x2E;
    // Teclado numérico (VK_MULTIPLY, VK_ADD, VK_DIVIDE) y signos (VK_OEM_*, teclado de EE. UU.)
    case Qt::Key_Asterisk: return 0x6A;
    case Qt::Key_Plus: return 0x6B;
    case Qt::Key_Semicolon: return 0xBA;
    case Qt::Key_Equal: return 0xBB;
    case Qt::Key_Comma: return 0xBC;
    case Qt::Key_Minus: return 0xBD;
    case Qt::Key_Period: return 0xBE;
    case Qt::Key_Slash: return 0xBF;
    case Qt::Key_QuoteLeft: return 0xC0;
    case Qt::Key_BracketLeft: return 0xDB;
    case Qt::Key_Backslash: return 0xDC;
    case Qt::Key_BracketRight: return 0xDD;
    case Qt::Key_Apostrophe: return 0xDE;
    default: return 0;
    }
}

int qtKeyFromVirtualKey(int vk)
{
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
        return vk;
    if (vk >= 0x70 && vk <= 0x87)
        return Qt::Key_F1 + (vk - 0x70);
    if (vk == 0x6D) // VK_SUBTRACT: el menos del teclado numérico
        return Qt::Key_Minus;
    if (vk == 0x6F) // VK_DIVIDE
        return Qt::Key_Slash;
    for (const int key : {Qt::Key_Backspace, Qt::Key_Tab, Qt::Key_Return, Qt::Key_Escape, Qt::Key_Space, Qt::Key_PageUp,
                          Qt::Key_PageDown, Qt::Key_End, Qt::Key_Home, Qt::Key_Left, Qt::Key_Up, Qt::Key_Right,
                          Qt::Key_Down, Qt::Key_Insert, Qt::Key_Delete, Qt::Key_Asterisk, Qt::Key_Plus,
                          Qt::Key_Semicolon, Qt::Key_Equal, Qt::Key_Comma, Qt::Key_Minus, Qt::Key_Period,
                          Qt::Key_Slash, Qt::Key_QuoteLeft, Qt::Key_BracketLeft, Qt::Key_Backslash,
                          Qt::Key_BracketRight, Qt::Key_Apostrophe}) {
        if (virtualKeyFromQtKey(key) == vk)
            return key;
    }
    return 0;
}

} // namespace edita
