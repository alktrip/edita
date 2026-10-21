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

#include "syntax/LanguageDetection.h"

#include "syntax/LanguageRegistry.h"
#include "syntax/StyleSheet.h"

#include <QFileInfo>

#include <algorithm>
#include <string>

namespace edita {

LangType languageFromFileName(const QString& filePath, const LanguageRegistry& languages, const StyleSheet& styles)
{
    const QString fileName = QFileInfo(filePath).fileName();
    // PathFindExtension: lo que va tras el último punto, si lo hay
    const qsizetype dot = fileName.lastIndexOf(QLatin1Char('.'));
    LangType lang = LangType::Text;
    if (dot >= 0) {
        const std::string ext = fileName.mid(dot + 1).toStdString();
        lang = styles.langFromUserExtension(ext);
        if (lang == LangType::Text)
            lang = languages.langFromExtension(ext);
    }
    if (lang != LangType::Text)
        return lang;

    auto is = [&fileName](const char* name) { return fileName.compare(QLatin1String(name), Qt::CaseInsensitive) == 0; };
    if (is("makefile") || is("GNUmakefile"))
        return LangType::Makefile;
    if (is("CmakeLists.txt"))
        return LangType::CMake;
    if (is("SConstruct") || is("SConscript") || is("wscript"))
        return LangType::Python;
    if (is("Rakefile") || is("Vagrantfile"))
        return LangType::Ruby;
    if (is("crontab") || is("PKGBUILD") || is("APKBUILD"))
        return LangType::Bash;
    return LangType::Text;
}

LangType languageFromTextBeginning(std::string_view text)
{
    if (text.size() <= 3)
        return LangType::Text;

    // Saltar el BOM (aquí el texto ya es UTF-8, pero se conserva la comprobación de N++)
    std::size_t i = 0;
    if (text.starts_with("\xEF\xBB\xBF"))
        i = 3;
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r'))
        ++i;
    if (i == text.size())
        return LangType::Text;

    // Primera línea, hasta 40 caracteres (los shebang pueden ser largos)
    std::string_view line = text.substr(i, std::min<std::size_t>(40, text.size() - i));
    line = line.substr(0, std::min(line.find('\r'), line.find('\n')));

    if (line.starts_with("#!")) {
        static constexpr std::pair<std::string_view, LangType> kShebangs[] = {
            {"sh", LangType::Bash}, {"python", LangType::Python}, {"perl", LangType::Perl},
            {"php", LangType::Php}, {"ruby", LangType::Ruby},     {"node", LangType::JavaScript},
        };
        for (const auto& [pattern, lang] : kShebangs) {
            if (line.find(pattern) != std::string_view::npos)
                return lang;
        }
        return LangType::Text;
    }

    static constexpr std::pair<std::string_view, LangType> kFirstLines[] = {
        {"<?xml", LangType::Xml},
        {"<?php", LangType::Php},
        {"<html", LangType::Html},
        {"<!DOCTYPE html", LangType::Html},
        {"<?", LangType::Php}, // después de <?php y <?xml
    };
    for (const auto& [pattern, lang] : kFirstLines) {
        if (line.starts_with(pattern))
            return lang;
    }
    return LangType::Text;
}

LangType detectLanguage(const QString& filePath, std::string_view textStart, const LanguageRegistry& languages,
                        const StyleSheet& styles)
{
    const LangType byName = filePath.isEmpty() ? LangType::Text : languageFromFileName(filePath, languages, styles);
    return byName != LangType::Text ? byName : languageFromTextBeginning(textStart);
}

} // namespace edita
