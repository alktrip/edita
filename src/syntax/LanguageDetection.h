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

#include <string_view>

namespace edita {

class LanguageRegistry;
class StyleSheet;

// Buffer::setFileName de N++: lenguaje según la extensión (primero las que el
// usuario añadió en stylers.xml, después langs.xml) y, si no hay, según nombres
// conocidos (Makefile, CMakeLists.txt, PKGBUILD…).
LangType languageFromFileName(const QString& filePath, const LanguageRegistry& languages, const StyleSheet& styles);

// FileManager::detectLanguageFromTextBeginning: shebang (#!…sh/python/perl/
// php/ruby/node) y primeras líneas conocidas (<?xml, <?php, <html, <!DOCTYPE html, <?).
LangType languageFromTextBeginning(std::string_view text);

// Las dos cosas en el orden de N++: el contenido solo se mira si el nombre no decidió
LangType detectLanguage(const QString& filePath, std::string_view textStart, const LanguageRegistry& languages,
                        const StyleSheet& styles);

} // namespace edita
