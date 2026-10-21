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

#include "syntax/StyleSheet.h"

#include <QString>
#include <QStringList>

#include <array>
#include <string>
#include <vector>

namespace edita {

// Listas de un lenguaje del usuario (SCE_USER_KWLIST_* de SciLexer.h)
namespace UserKeywordList {
enum : int
{
    Comments = 0,
    NumberPrefix1, NumberPrefix2, NumberExtras1, NumberExtras2, NumberSuffix1, NumberSuffix2, NumberRange,
    Operators1, Operators2,
    FoldersInCode1Open, FoldersInCode1Middle, FoldersInCode1Close,
    FoldersInCode2Open, FoldersInCode2Middle, FoldersInCode2Close,
    FoldersInCommentOpen, FoldersInCommentMiddle, FoldersInCommentClose,
    Keywords1, Keywords2, Keywords3, Keywords4, Keywords5, Keywords6, Keywords7, Keywords8,
    Delimiters,
    Total, // 28
};
}

// Estilos (SCE_USER_STYLE_*)
inline constexpr int kUserStyleCount = 24; // de DEFAULT (0) a DELIMITERS8 (23)

// Un <UserLang> de userDefineLang.xml (UserLangContainer en N++, Parameters.h)
struct UserLanguage
{
    QString name;
    QString extensions;          // "ext": separadas por espacios
    bool darkModeTheme = false;
    bool caseIgnored = false;
    bool allowFoldOfComments = false;
    bool foldCompact = false;
    int forcePureLC = 0;         // 0 en cualquier sitio, 1 al principio de línea, 2 tras espacios
    int decimalSeparator = 0;    // 0 punto, 1 coma, 2 ambos
    std::array<bool, 8> isPrefix{};
    std::array<std::string, UserKeywordList::Total> keywordLists;
    std::vector<Style> styles;   // uno por SCE_USER_STYLE_*, con su nombre de N++
    QString sourceFile;          // de qué archivo vino (para guardarlo en el mismo)
    int lexerId = 0;             // identifica la definición ante LexUser (cambia al editarla)

    const Style* style(int id) const;
    Style* style(int id);
    // ¿Es para este archivo? (ext coincide con la extensión o con el nombre entero)
    bool matches(const QString& filePath) const;
};

// Nombres de N++ de cada lista y cada estilo (GlobalMappers en UserDefineDialog.h)
const char* userKeywordListName(int index);
const char* userStyleName(int id);

// Las listas "Comments" y "Delimiters" guardan varias casillas en un solo texto,
// cada palabra precedida del número de casilla con dos cifras ("00// 03/* 04*/").
// Estas funciones son retrieve() y convertTo() de UserDefineDialog.cpp (con los
// grupos "((varias palabras))").
QString userSlotWords(const std::string& list, int slot);
std::string userComposeSlots(const QStringList& slotTexts);

// Lenguaje nuevo con todos los estilos por defecto (lo que hace N++ al crearlo)
UserLanguage newUserLanguage(const QString& name);

// Todos los lenguajes del usuario: ~/.config/edita/userDefineLang.xml, los de
// ~/.config/edita/userDefineLangs/*.xml (NppParameters::getUserDefineLangsFromXmlTree)
// y, detrás, los que trae la aplicación en share/edita/userDefineLangs/
class UserLanguages
{
public:
    QStringList load(const QString& configDir); // devuelve avisos
    const std::vector<UserLanguage>& languages() const { return _languages; }
    const UserLanguage* find(const QString& name) const;
    // Buffer::setFileName: los lenguajes del usuario se miran antes que los de N++
    // getUserDefinedLangNameFromExt: el del modo (claro u oscuro, darkModeTheme) si hay
    // uno de cada; si no, el que haya
    const UserLanguage* forFile(const QString& filePath, bool darkMode = false) const;

    // Añade o sustituye (por nombre) y lo guarda en su archivo. Un lenguaje nuevo
    // va a userDefineLangs/<nombre>.xml.
    bool save(UserLanguage language, QString* error = nullptr);
    bool remove(const QString& name, QString* error = nullptr);
    // Copia un archivo de N++ a userDefineLangs/ y carga sus lenguajes
    bool importFile(const QString& path, QStringList* imported = nullptr, QString* error = nullptr);
    // NppParameters::exportUDLToFile
    bool exportLanguage(const QString& name, const QString& path, QString* error = nullptr) const;

    // Lectura y escritura de un archivo con <UserLang>
    static bool readFile(const QString& path, std::vector<UserLanguage>& languages, QString* error = nullptr);
    static bool writeFile(const QString& path, const std::vector<UserLanguage>& languages, QString* error = nullptr);

private:
    bool rewriteSource(const QString& file, QString* error);
    bool isUserFile(const QString& file) const;

    QString _configDir;
    std::vector<UserLanguage> _languages;
};

} // namespace edita
