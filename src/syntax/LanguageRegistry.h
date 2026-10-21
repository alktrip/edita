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

#include "core/LangType.h"

#include <QString>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace edita {

// Índices de las listas de palabras clave (LangIdxStyle en N++, NppConstants.h)
namespace KeywordIndex {
enum : int
{
    Instr = 0,     // "instre1"
    Instr2 = 1,    // "instre2"
    Type = 2,      // "type1"
    Type2 = 3,
    Type3 = 4,
    Type4 = 5,
    Type5 = 6,
    Type6 = 7,
    Type7 = 8,     // "type7"
    Substyle1 = 9, // "substyle1"…"substyle8" = 9…16
    Substyle5 = 13,
};
}
inline constexpr int kKeywordListCount = 20; // NB_LIST

// getKwClassFromName: "instre1" → 0 … "substyle8" → 16; "0"…"8" valen como número.
// -1 si no es un nombre válido.
int keywordIndexFromName(std::string_view name);

// Un <Language> de langs.xml (struct Lang en N++)
struct LanguageDefinition
{
    LangType type = LangType::Text;
    std::string name;
    std::string extensions; // "c cpp h" (separadas por espacios)
    std::array<std::string, kKeywordListCount> keywords;
    std::string commentLine;
    std::string commentStart;
    std::string commentEnd;
    int tabSettings = -1;
    bool backspaceUnindent = false;
};

// Contenido de langs.xml (NppParameters::feedKeyWordsParameters)
class LanguageRegistry
{
public:
    bool load(const QString& path, QString* error = nullptr);
    bool loadFromString(std::string_view xml, QString* error = nullptr);

    const std::vector<LanguageDefinition>& languages() const { return _languages; }
    const LanguageDefinition* find(LangType type) const;

    // NppParameters::getWordList; vacío si no hay
    std::string_view keywords(LangType type, int index) const;

    // Parte de getLangFromExt que mira langs.xml (la de stylers.xml está en
    // StyleSheet). Recorre los lenguajes del último al primero, como N++, y
    // compara sin distinguir mayúsculas. Text si ninguno la declara.
    LangType langFromExtension(std::string_view extension) const;

private:
    std::vector<LanguageDefinition> _languages;
};

// isInList de N++: ¿está `token` en la lista separada por espacios? (sin
// distinguir mayúsculas en ASCII)
bool isInSpaceSeparatedList(std::string_view token, std::string_view list);

} // namespace edita
