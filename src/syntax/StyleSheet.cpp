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

#include "syntax/StyleSheet.h"

#include "syntax/LanguageRegistry.h"
#include "syntax/StyleXml.h"

#include <pugixml.hpp>

#include <QCoreApplication>

#include <algorithm>
#include <cstdlib>

namespace edita {

namespace {

// strVal de Parameters.cpp: "" → 0, no numérico → -1
int hexValue(const char* str)
{
    if (!str)
        return -1;
    if (!str[0])
        return 0;
    char* end = nullptr;
    const long value = std::strtol(str, &end, 16);
    return *end == '\0' ? static_cast<int>(value) : -1;
}

// "RRGGBB" → 0x00BBGGRR, conservando el -1 de "inválido" (StyleArray::addStyler)
int colorFromHex(const char* str)
{
    const auto value = static_cast<unsigned int>(hexValue(str));
    const unsigned int bgr = ((value >> 16) & 0xFF) | (value & 0xFF00) | ((value & 0xFF) << 16);
    return static_cast<int>(bgr | (value & 0xFF000000u));
}

} // namespace

Style parseStyle(const pugi::xml_node& node)
{
    Style s;
    s.id = node.attribute("styleID").as_int(-1);
    s.name = node.attribute("name").as_string("");
    if (const pugi::xml_attribute a = node.attribute("fgColor"))
        s.fgColor = colorFromHex(a.value());
    if (const pugi::xml_attribute a = node.attribute("bgColor"))
        s.bgColor = colorFromHex(a.value());
    s.colorStyle = node.attribute("colorStyle").as_int(kColorStyleAll);
    s.fontName = node.attribute("fontName").as_string("");
    // pugixml devuelve 0 para un atributo vacío (fontSize=""), igual que en N++
    s.fontStyle = node.attribute("fontStyle").as_int(kStyleNotUsed);
    s.fontSize = node.attribute("fontSize").as_int(kStyleNotUsed);
    if (const pugi::xml_attribute a = node.attribute("keywordClass"))
        s.keywordClass = keywordIndexFromName(a.value());
    s.keywords = node.child_value();
    s.nesting = node.attribute("nesting").as_int(0);
    return s;
}

namespace {

bool parseStyleSheet(const pugi::xml_document& doc, std::vector<LexerStyles>& lexers,
                     std::vector<Style>& globals, QString* error)
{
    const pugi::xml_node root = doc.child("NotepadPlus");
    const pugi::xml_node lexerRoot = root.child("LexerStyles");
    if (!lexerRoot) {
        if (error)
            *error = QCoreApplication::translate("StyleSheet", "<NotepadPlus><LexerStyles> is missing");
        return false;
    }
    for (pugi::xml_node lexerNode : lexerRoot.children("LexerType")) {
        const char* name = lexerNode.attribute("name").as_string(nullptr);
        if (!name)
            continue;
        LexerStyles ls;
        ls.name = name;
        ls.description = lexerNode.attribute("desc").as_string("");
        ls.userExtensions = lexerNode.attribute("ext").as_string("");
        for (pugi::xml_node styleNode : lexerNode.children("WordsStyle")) {
            if (styleNode.attribute("styleID").as_int(-1) != -1)
                ls.styles.push_back(parseStyle(styleNode));
        }
        lexers.push_back(std::move(ls));
    }
    for (pugi::xml_node styleNode : root.child("GlobalStyles").children("WidgetStyle")) {
        if (styleNode.attribute("styleID").as_int(-1) != -1)
            globals.push_back(parseStyle(styleNode));
    }
    return true;
}

QString parseError(const pugi::xml_parse_result& result)
{
    return QCoreApplication::translate("StyleSheet", "Invalid XML (position %1): %2")
        .arg(result.offset)
        .arg(QString::fromUtf8(result.description()));
}

} // namespace

const Style* LexerStyles::findById(int id) const
{
    const auto it = std::find_if(styles.begin(), styles.end(), [id](const Style& s) { return s.id == id; });
    return it == styles.end() ? nullptr : &*it;
}

const Style* LexerStyles::findByName(std::string_view n) const
{
    const auto it = std::find_if(styles.begin(), styles.end(), [n](const Style& s) { return s.name == n; });
    return it == styles.end() ? nullptr : &*it;
}

bool StyleSheet::load(const QString& path, QString* error)
{
    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_file(path.toUtf8().constData());
    if (!result) {
        if (error)
            *error = parseError(result);
        return false;
    }
    std::vector<LexerStyles> lexers;
    std::vector<Style> globals;
    if (!parseStyleSheet(doc, lexers, globals, error))
        return false;
    _lexers = std::move(lexers);
    _globalStyles = std::move(globals);
    return true;
}

bool StyleSheet::loadFromString(std::string_view xml, QString* error)
{
    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_buffer(xml.data(), xml.size());
    if (!result) {
        if (error)
            *error = parseError(result);
        return false;
    }
    std::vector<LexerStyles> lexers;
    std::vector<Style> globals;
    if (!parseStyleSheet(doc, lexers, globals, error))
        return false;
    _lexers = std::move(lexers);
    _globalStyles = std::move(globals);
    return true;
}

const LexerStyles* StyleSheet::lexer(std::string_view name) const
{
    const auto it = std::find_if(_lexers.begin(), _lexers.end(), [name](const LexerStyles& l) { return l.name == name; });
    return it == _lexers.end() ? nullptr : &*it;
}

const Style* StyleSheet::globalById(int id) const
{
    const auto it = std::find_if(_globalStyles.begin(), _globalStyles.end(), [id](const Style& s) { return s.id == id; });
    return it == _globalStyles.end() ? nullptr : &*it;
}

const Style* StyleSheet::globalByName(std::string_view name) const
{
    const auto it = std::find_if(_globalStyles.begin(), _globalStyles.end(), [name](const Style& s) { return s.name == name; });
    return it == _globalStyles.end() ? nullptr : &*it;
}

LangType StyleSheet::langFromUserExtension(std::string_view extension) const
{
    for (const LexerStyles& ls : _lexers) {
        if (isInSpaceSeparatedList(extension, ls.userExtensions))
            return langTypeFromName(ls.name);
    }
    return LangType::Text;
}

} // namespace edita
