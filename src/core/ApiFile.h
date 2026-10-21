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

#include <string>
#include <vector>

namespace edita {

// Archivos de autocompletado de N++ (installer/APIs/<lenguaje>.xml):
//   <NotepadPlus><AutoComplete>
//     <Environment ignoreCase="no" startFunc="(" stopFunc=")" paramSeparator="," terminal=";" additionalWordChar="."/>
//     <KeyWord name="print" func="yes"><Overload retVal="" descr="…"><Param name="x"/></Overload></KeyWord>
struct ApiOverload
{
    std::string retVal;
    std::string description;
    std::vector<std::string> params;
};

struct ApiKeyword
{
    std::string name;
    bool isFunction = false;
    std::vector<ApiOverload> overloads;
};

struct ApiLanguage
{
    bool ignoreCase = true;
    char startFunc = '(';
    char stopFunc = ')';
    char paramSeparator = ',';
    char terminal = ';';
    std::string additionalWordChars;
    std::vector<ApiKeyword> keywords; // en el orden del archivo

    bool empty() const { return keywords.empty(); }
    // Palabra clave con ese nombre (respetando ignoreCase); nullptr si no hay
    const ApiKeyword* find(std::string_view name) const;
};

// AutoCompletion::setLanguage: false si el archivo no existe o no tiene <KeyWord>
bool loadApiFile(const QString& path, ApiLanguage& api);

// Nombre del archivo para un lenguaje (AutoCompletion::getApiFileName): el nombre
// corto de N++ ("cpp", "python"); JavaScript usa el de JavaScript incrustado
QString apiFileName(LangType lang);

// Ruta del archivo de un lenguaje: ~/.config/edita/autoCompletion/ y luego los
// datos de la aplicación (share/edita/autoCompletion). Vacía si no hay.
QString findApiFile(LangType lang);

} // namespace edita
