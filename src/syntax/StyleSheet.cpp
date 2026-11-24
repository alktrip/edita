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
#include <map>
#include <string>
#include <cstdio>
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

namespace {

// 0x00BBGGRR → "RRGGBB" (writeStyle2Element: "%.6lX")
std::string hexFromColor(int bgr)
{
    const auto c = static_cast<unsigned int>(bgr);
    const unsigned int rgb = ((c & 0xFF) << 16) | (c & 0xFF00) | ((c >> 16) & 0xFF);
    char text[8];
    std::snprintf(text, sizeof text, "%.6X", rgb);
    return text;
}

// writeStyle2Element de N++
void writeStyle(const Style& style, pugi::xml_node node)
{
    auto set = [&node](const char* name, const std::string& value) {
        pugi::xml_attribute attr = node.attribute(name);
        if (!attr)
            attr = node.append_attribute(name);
        attr.set_value(value.c_str());
    };
    if (style.fgColor != kStyleNotUsed)
        set("fgColor", hexFromColor(style.fgColor));
    if (style.bgColor != kStyleNotUsed)
        set("bgColor", hexFromColor(style.bgColor));
    if (style.colorStyle != kColorStyleAll)
        set("colorStyle", std::to_string(style.colorStyle));
    else
        node.remove_attribute("colorStyle");
    if (!style.fontName.empty() || node.attribute("fontName"))
        set("fontName", style.fontName);
    if (style.fontSize != kStyleNotUsed)
        set("fontSize", style.fontSize == 0 ? std::string() : std::to_string(style.fontSize));
    if (style.fontStyle != kStyleNotUsed)
        set("fontStyle", std::to_string(style.fontStyle));
    if (style.keywordClass != kStyleNotUsed || !style.keywords.empty()) {
        pugi::xml_node text = node.first_child();
        if (!text || text.type() != pugi::node_pcdata) {
            node.remove_children();
            text = node.append_child(pugi::node_pcdata);
        }
        text.set_value(style.keywords.c_str());
    }
}

} // namespace

bool StyleSheet::save(const QString& path, QString* error) const
{
    pugi::xml_document doc;
    const QByteArray file = path.toUtf8();
    const pugi::xml_parse_result result = doc.load_file(file.constData(), pugi::parse_default | pugi::parse_comments);
    if (!result) {
        if (error)
            *error = parseError(result);
        return false;
    }
    pugi::xml_node root = doc.child("NotepadPlus");
    for (pugi::xml_node lexerNode : root.child("LexerStyles").children("LexerType")) {
        const LexerStyles* ls = lexer(lexerNode.attribute("name").as_string(""));
        if (!ls)
            continue;
        if (!ls->userExtensions.empty() || lexerNode.attribute("ext")) {
            pugi::xml_attribute ext = lexerNode.attribute("ext");
            if (!ext)
                ext = lexerNode.append_attribute("ext");
            ext.set_value(ls->userExtensions.c_str());
        }
        for (pugi::xml_node styleNode : lexerNode.children("WordsStyle")) {
            if (const Style* style = ls->findById(styleNode.attribute("styleID").as_int(-1)))
                writeStyle(*style, styleNode);
        }
    }
    // Los globales, por nombre: varios comparten styleID (0)
    for (pugi::xml_node styleNode : root.child("GlobalStyles").children("WidgetStyle")) {
        if (const Style* style = globalByName(styleNode.attribute("name").as_string("")))
            writeStyle(*style, styleNode);
    }
    if (!doc.save_file(file.constData(), "    ", pugi::format_default, pugi::encoding_utf8)) {
        if (error)
            *error = QCoreApplication::translate("StyleSheet", "Cannot write %1").arg(path);
        return false;
    }
    return true;
}

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

bool updateStylesFromModel(const QString& userPath, const QString& modelPath, bool useDefaultColors, QString* error)
{
    pugi::xml_document user, model;
    const QByteArray userFile = userPath.toUtf8();
    if (!user.load_file(userFile.constData(), pugi::parse_default | pugi::parse_comments)
        || !model.load_file(modelPath.toUtf8().constData()))
        return false; // el archivo dañado lo trata loadAll
    pugi::xml_node userRoot = user.child("NotepadPlus");
    const pugi::xml_node modelRoot = model.child("NotepadPlus");
    const long long modelDate = modelRoot.attribute("modelDate").as_llong(0);
    if (modelDate == 0 || userRoot.attribute("modelDate").as_llong(0) >= modelDate)
        return false;
    pugi::xml_node userGlobals = userRoot.child("GlobalStyles");
    const pugi::xml_node modelGlobals = modelRoot.child("GlobalStyles");
    pugi::xml_node userLexers = userRoot.child("LexerStyles");
    const pugi::xml_node modelLexers = modelRoot.child("LexerStyles");
    if (!userGlobals || !modelGlobals || !userLexers || !modelLexers)
        return false;

    // Los colores del «Default Style» (32) del usuario, para lo nuevo con useDefaultColors
    std::string defaultFg, defaultBg;
    if (const pugi::xml_node def = userGlobals.find_child_by_attribute("WidgetStyle", "styleID", "32")) {
        defaultFg = def.attribute("fgColor").as_string();
        defaultBg = def.attribute("bgColor").as_string();
    }
    auto recolor = [&](pugi::xml_node node, const char* only = nullptr) {
        if (!useDefaultColors)
            return;
        for (const char* name : {"fgColor", "bgColor"}) {
            if (only && std::string_view(only) != name)
                continue;
            if (pugi::xml_attribute a = node.attribute(name))
                a.set_value((std::string_view(name) == "fgColor" ? defaultFg : defaultBg).c_str());
        }
    };
    // Los atributos que falten (uno vacío se respeta: es a propósito)
    auto addMissingAttributes = [&](pugi::xml_node mine, const pugi::xml_node theirs) {
        for (const pugi::xml_attribute attr : theirs.attributes()) {
            if (mine.attribute(attr.name()))
                continue;
            mine.append_attribute(attr.name()).set_value(attr.value());
            recolor(mine, attr.name());
        }
    };
    // Estilos globales: por styleID (1-256) o, si no tiene, por nombre
    auto widgetKey = [](const pugi::xml_node node) {
        const int id = node.attribute("styleID").as_int(0);
        return id > 0 && id <= 256 ? std::to_string(id) : std::string(node.attribute("name").as_string());
    };
    std::map<std::string, pugi::xml_node> widgets;
    for (pugi::xml_node w : userGlobals.children("WidgetStyle"))
        widgets[widgetKey(w)] = w;
    for (const pugi::xml_node w : modelGlobals.children("WidgetStyle")) {
        const std::string key = widgetKey(w);
        if (key.empty())
            continue;
        if (const auto it = widgets.find(key); it != widgets.end())
            addMissingAttributes(it->second, w);
        else
            recolor(userGlobals.append_copy(w));
    }
    // Lexers: los que falten, enteros; en los que hay, los estilos que falten (por styleID)
    std::map<std::string, pugi::xml_node> lexers;
    for (pugi::xml_node l : userLexers.children("LexerType"))
        lexers[l.attribute("name").as_string()] = l;
    for (const pugi::xml_node modelLexer : modelLexers.children("LexerType")) {
        const std::string name = modelLexer.attribute("name").as_string();
        if (name.empty())
            continue;
        const auto found = lexers.find(name);
        if (found == lexers.end()) {
            pugi::xml_node copy = userLexers.append_copy(modelLexer);
            for (pugi::xml_node style : copy.children("WordsStyle"))
                recolor(style);
            continue;
        }
        pugi::xml_node userLexer = found->second;
        for (const pugi::xml_node style : modelLexer.children("WordsStyle")) {
            const char* id = style.attribute("styleID").as_string(nullptr);
            if (!id || !id[0])
                continue;
            if (pugi::xml_node mine = userLexer.find_child_by_attribute("WordsStyle", "styleID", id))
                addMissingAttributes(mine, style);
            else
                recolor(userLexer.append_copy(style));
        }
    }
    pugi::xml_attribute date = userRoot.attribute("modelDate");
    if (!date)
        date = userRoot.append_attribute("modelDate");
    date.set_value(modelDate);
    if (!user.save_file(userFile.constData(), "    ", pugi::format_default, pugi::encoding_utf8)) {
        if (error)
            *error = QCoreApplication::translate("StyleSheet", "Cannot write %1").arg(userPath);
        return false;
    }
    return true;
}

} // namespace edita
