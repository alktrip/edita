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

#include <ScintillaEditBase.h>

namespace edita {

struct XmlTagMatchOptions
{
    bool highlightAttributes = true; // _enableTagAttrsHilite
    bool caseSensitive = false;      // XML y XHTML distinguen mayúsculas; HTML no
    bool indentGuides = false;       // marcar la guía de sangría entre las dos etiquetas
};

// XmlMatchedTagsHighlighter::tagMatch: con el cursor en una etiqueta de XML o
// HTML, marca la etiqueta y su pareja (indicador "Tags match highlighting") y
// sus atributos ("Tags attribute"). En PHP, ASP y JSP no hace nada dentro de los
// bloques de código. Para los demás lenguajes solo borra las marcas.
void matchXmlTags(ScintillaEditBase& view, LangType lang, const XmlTagMatchOptions& options);

} // namespace edita
