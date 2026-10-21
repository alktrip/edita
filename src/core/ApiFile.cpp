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


#include "core/ApiFile.h"

#include "core/Paths.h"

#include <pugixml.hpp>

#include <QFile>

#include <QDir>
#include <QFileInfo>

#include <cctype>
#include <cstring>

namespace edita {

namespace {

bool equalNoCase(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

char firstChar(const pugi::xml_node& node, const char* attr, char fallback)
{
    const char* value = node.attribute(attr).as_string("");
    return value[0] ? value[0] : fallback;
}

} // namespace

const ApiKeyword* ApiLanguage::find(std::string_view name) const
{
    for (const ApiKeyword& keyword : keywords) {
        if (ignoreCase ? equalNoCase(keyword.name, name) : keyword.name == name)
            return &keyword;
    }
    return nullptr;
}

bool loadApiFile(const QString& path, ApiLanguage& api)
{
    api = ApiLanguage{};
    if (path.isEmpty())
        return false;
    pugi::xml_document doc;
    if (!doc.load_file(QFile::encodeName(path).constData()))
        return false;
    const pugi::xml_node autoNode = doc.child("NotepadPlus").child("AutoComplete");
    if (!autoNode.child("KeyWord"))
        return false;

    if (const pugi::xml_node env = autoNode.child("Environment")) {
        api.ignoreCase = std::strcmp(env.attribute("ignoreCase").as_string("yes"), "no") != 0;
        api.startFunc = firstChar(env, "startFunc", api.startFunc);
        api.stopFunc = firstChar(env, "stopFunc", api.stopFunc);
        api.paramSeparator = firstChar(env, "paramSeparator", api.paramSeparator);
        api.terminal = firstChar(env, "terminal", api.terminal);
        api.additionalWordChars = env.attribute("additionalWordChar").as_string("");
    }
    for (const pugi::xml_node node : autoNode.children("KeyWord")) {
        ApiKeyword keyword;
        keyword.name = node.attribute("name").as_string("");
        if (keyword.name.empty())
            continue;
        keyword.isFunction = std::strcmp(node.attribute("func").as_string("no"), "yes") == 0;
        for (const pugi::xml_node overloadNode : node.children("Overload")) {
            if (!overloadNode.attribute("retVal"))
                continue; // nodo mal formado, como en FunctionCallTip::loadFunction
            ApiOverload overload;
            overload.retVal = overloadNode.attribute("retVal").as_string("");
            overload.description = overloadNode.attribute("descr").as_string("");
            for (const pugi::xml_node param : overloadNode.children("Param")) {
                if (const char* name = param.attribute("name").as_string(nullptr))
                    overload.params.emplace_back(name);
            }
            keyword.overloads.push_back(std::move(overload));
        }
        api.keywords.push_back(std::move(keyword));
    }
    return !api.keywords.empty();
}

QString apiFileName(LangType lang)
{
    if (lang == LangType::JavaScript)
        lang = LangType::JsEmbedded;
    return QString::fromUtf8(languageInfo(lang).name);
}

QString findApiFile(LangType lang)
{
    const QString relative = QStringLiteral("autoCompletion/") + apiFileName(lang) + QStringLiteral(".xml");
    const QString user = QDir(paths::configDir()).filePath(relative);
    if (QFileInfo::exists(user))
        return user;
    return paths::findDataFile(relative);
}

} // namespace edita
