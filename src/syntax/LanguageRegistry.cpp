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

#include "syntax/LanguageRegistry.h"

#include <pugixml.hpp>

#include <QCoreApplication>

#include <algorithm>
#include <cctype>

namespace edita {

namespace {

bool equalsIgnoreCase(std::string_view a, std::string_view b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

QString parseError(const pugi::xml_parse_result& result)
{
    return QCoreApplication::translate("LanguageRegistry", "Invalid XML (position %1): %2")
        .arg(result.offset)
        .arg(QString::fromUtf8(result.description()));
}

} // namespace

int keywordIndexFromName(std::string_view name)
{
    static constexpr std::pair<std::string_view, int> kNames[] = {
        {"instre1", 0}, {"instre2", 1}, {"type1", 2}, {"type2", 3}, {"type3", 4}, {"type4", 5},
        {"type5", 6}, {"type6", 7}, {"type7", 8}, {"substyle1", 9}, {"substyle2", 10},
        {"substyle3", 11}, {"substyle4", 12}, {"substyle5", 13}, {"substyle6", 14},
        {"substyle7", 15}, {"substyle8", 16},
    };
    for (const auto& [n, index] : kNames) {
        if (name == n)
            return index;
    }
    if (name.size() == 1 && name[0] >= '0' && name[0] <= '8') // hasta KEYWORDSET_MAX
        return name[0] - '0';
    return -1;
}

bool isInSpaceSeparatedList(std::string_view token, std::string_view list)
{
    if (token.empty())
        return false;
    while (!list.empty()) {
        const std::size_t space = list.find(' ');
        const std::string_view word = list.substr(0, space);
        if (!word.empty() && equalsIgnoreCase(token, word))
            return true;
        if (space == std::string_view::npos)
            break;
        list.remove_prefix(space + 1);
    }
    return false;
}

namespace {

bool parseLanguages(const pugi::xml_document& doc, std::vector<LanguageDefinition>& languages, QString* error)
{
    const pugi::xml_node languagesNode = doc.child("NotepadPlus").child("Languages");
    if (!languagesNode) {
        if (error)
            *error = QCoreApplication::translate("LanguageRegistry", "<NotepadPlus><Languages> is missing");
        return false;
    }
    for (pugi::xml_node node : languagesNode.children("Language")) {
        const char* name = node.attribute("name").as_string(nullptr);
        if (!name)
            continue;
        LanguageDefinition def;
        def.name = name;
        def.type = langTypeFromName(def.name);
        def.extensions = node.attribute("ext").as_string("");
        def.commentLine = node.attribute("commentLine").as_string("");
        def.commentStart = node.attribute("commentStart").as_string("");
        def.commentEnd = node.attribute("commentEnd").as_string("");
        def.tabSettings = node.attribute("tabSettings").as_int(-1);
        def.backspaceUnindent = node.attribute("backspaceUnindent").as_bool(false);
        for (pugi::xml_node kw : node.children("Keywords")) {
            const int index = keywordIndexFromName(kw.attribute("name").as_string(""));
            if (index >= 0 && index < kKeywordListCount)
                def.keywords[static_cast<std::size_t>(index)] = kw.child_value();
        }
        languages.push_back(std::move(def));
    }
    return true;
}

} // namespace

bool LanguageRegistry::load(const QString& path, QString* error)
{
    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_file(path.toUtf8().constData());
    if (!result) {
        if (error)
            *error = parseError(result);
        return false;
    }
    std::vector<LanguageDefinition> languages;
    if (!parseLanguages(doc, languages, error))
        return false;
    _languages = std::move(languages);
    return true;
}

bool LanguageRegistry::loadFromString(std::string_view xml, QString* error)
{
    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_buffer(xml.data(), xml.size());
    if (!result) {
        if (error)
            *error = parseError(result);
        return false;
    }
    std::vector<LanguageDefinition> languages;
    if (!parseLanguages(doc, languages, error))
        return false;
    _languages = std::move(languages);
    return true;
}

const LanguageDefinition* LanguageRegistry::find(LangType type) const
{
    const auto it = std::find_if(_languages.begin(), _languages.end(),
        [type](const LanguageDefinition& l) { return l.type == type && (type != LangType::Text || l.name == "normal"); });
    return it == _languages.end() ? nullptr : &*it;
}

std::string_view LanguageRegistry::keywords(LangType type, int index) const
{
    const LanguageDefinition* def = find(type);
    if (!def || index < 0 || index >= kKeywordListCount)
        return {};
    return def->keywords[static_cast<std::size_t>(index)];
}

LangType LanguageRegistry::langFromExtension(std::string_view extension) const
{
    for (auto it = _languages.rbegin(); it != _languages.rend(); ++it) {
        if (!it->extensions.empty() && isInSpaceSeparatedList(extension, it->extensions))
            return it->type;
    }
    return LangType::Text;
}

} // namespace edita
