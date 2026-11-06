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
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

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

bool updateLangsFromModel(const QString& userPath, const QString& modelPath, QString* error)
{
    pugi::xml_document user, model;
    const QByteArray userFile = userPath.toUtf8();
    if (!user.load_file(userFile.constData(), pugi::parse_default | pugi::parse_comments)
        || !model.load_file(modelPath.toUtf8().constData()))
        return false; // el langs.xml dañado lo trata loadAll
    pugi::xml_node userRoot = user.child("NotepadPlus");
    const pugi::xml_node modelRoot = model.child("NotepadPlus");
    const long long modelDate = modelRoot.attribute("modelDate").as_llong(0);
    if (modelDate == 0 || userRoot.attribute("modelDate").as_llong(0) >= modelDate)
        return false;
    pugi::xml_node userLanguages = userRoot.child("Languages");
    const pugi::xml_node modelLanguages = modelRoot.child("Languages");
    if (!userLanguages || !modelLanguages)
        return false;

    std::map<std::string, pugi::xml_node> byName;
    for (pugi::xml_node lang : userLanguages.children("Language"))
        byName[lang.attribute("name").as_string()] = lang;
    auto words = [](const char* text) {
        std::vector<std::string> list;
        std::istringstream in(text ? text : "");
        for (std::string w; in >> w;)
            list.push_back(w);
        return list;
    };
    for (const pugi::xml_node modelLang : modelLanguages.children("Language")) {
        const std::string name = modelLang.attribute("name").as_string();
        if (name.empty())
            continue;
        const auto found = byName.find(name);
        if (found == byName.end()) { // un lenguaje nuevo: entero
            userLanguages.append_copy(modelLang);
            continue;
        }
        pugi::xml_node userLang = found->second;
        // Las listas de palabras: las que falten, enteras; en las que ya están, las palabras que falten
        for (const pugi::xml_node modelKw : modelLang.children("Keywords")) {
            const char* kwName = modelKw.attribute("name").as_string(nullptr);
            if (!kwName)
                continue;
            pugi::xml_node userKw = userLang.find_child_by_attribute("Keywords", "name", kwName);
            if (!userKw) {
                userLang.append_copy(modelKw);
                continue;
            }
            std::vector<std::string> list = words(userKw.child_value());
            const std::set<std::string> have(list.begin(), list.end());
            bool added = false;
            for (const std::string& w : words(modelKw.child_value())) {
                if (!have.count(w)) {
                    list.push_back(w);
                    added = true;
                }
            }
            if (added) {
                std::sort(list.begin(), list.end());
                std::string text;
                for (const std::string& w : list)
                    text += (text.empty() ? "" : " ") + w;
                userKw.text().set(text.c_str());
            }
        }
        // Los atributos que falten; en «ext», las extensiones que falten (uno vacío se respeta: es a propósito)
        for (const pugi::xml_attribute attr : modelLang.attributes()) {
            pugi::xml_attribute mine = userLang.attribute(attr.name());
            if (!mine) {
                userLang.append_attribute(attr.name()).set_value(attr.value());
            } else if (std::string(attr.name()) == "ext" && mine.value()[0]) {
                std::vector<std::string> exts = words(mine.value());
                const std::set<std::string> have(exts.begin(), exts.end());
                for (const std::string& e : words(attr.value()))
                    if (!have.count(e))
                        exts.push_back(e);
                std::string text;
                for (const std::string& e : exts)
                    text += (text.empty() ? "" : " ") + e;
                mine.set_value(text.c_str());
            }
        }
    }
    pugi::xml_attribute date = userRoot.attribute("modelDate");
    if (!date)
        date = userRoot.append_attribute("modelDate");
    date.set_value(modelDate);
    if (!user.save_file(userFile.constData(), "    ", pugi::format_default, pugi::encoding_utf8)) {
        if (error)
            *error = QCoreApplication::translate("LanguageRegistry", "Cannot write %1").arg(userPath);
        return false;
    }
    return true;
}

} // namespace edita
