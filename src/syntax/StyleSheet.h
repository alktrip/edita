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
#include <string_view>
#include <vector>

namespace edita {

inline constexpr int kStyleNotUsed = -1; // STYLE_NOT_USED

// Bits de Style::colorStyle (COLORSTYLE_* en N++)
inline constexpr int kColorStyleForeground = 0x01;
inline constexpr int kColorStyleBackground = 0x02;
inline constexpr int kColorStyleAll = kColorStyleForeground | kColorStyleBackground;

// Bits de Style::fontStyle (FONTSTYLE_* en N++)
inline constexpr int kFontStyleBold = 0x01;
inline constexpr int kFontStyleItalic = 0x02;
inline constexpr int kFontStyleUnderline = 0x04;

// Un <WordsStyle> o <WidgetStyle> (struct Style en N++, Parameters.h).
// Los colores están ya en el formato de Scintilla (0x00BBGGRR, el COLORREF de
// Windows), igual que los guarda N++ tras leer el "RRGGBB" del XML.
struct Style
{
    int id = kStyleNotUsed;
    std::string name;
    int fgColor = kStyleNotUsed;
    int bgColor = kStyleNotUsed;
    int colorStyle = kColorStyleAll;
    std::string fontName;
    int fontStyle = kStyleNotUsed;
    int fontSize = kStyleNotUsed;
    int keywordClass = kStyleNotUsed;
    std::string keywords; // palabras añadidas por el usuario
    int nesting = 0;      // lenguajes del usuario: qué estilos pueden ir dentro (SCE_USER_MASK_NESTING_*)
};

// Un <LexerType> (LexerStyler en N++)
struct LexerStyles
{
    std::string name;
    std::string description;
    std::string userExtensions; // atributo "ext": extensiones añadidas por el usuario
    std::vector<Style> styles;

    const Style* findById(int id) const;
    const Style* findByName(std::string_view name) const;
};

// "Global override" (GlobalOverride en N++). En N++ los interruptores están en
// config.xml; por defecto todos apagados.
struct GlobalOverride
{
    bool foreground = false;
    bool background = false;
    bool font = false;
    bool fontSize = false;
    bool bold = false;
    bool italic = false;
    bool underline = false;

    bool isEnabled() const { return foreground || background || font || fontSize || bold || italic || underline; }
};

// Contenido de stylers.xml o de un tema (NppParameters::feedStylerArray).
class StyleSheet
{
public:
    bool load(const QString& path, QString* error = nullptr);
    bool loadFromString(std::string_view xml, QString* error = nullptr);

    const std::vector<LexerStyles>& lexers() const { return _lexers; }
    const LexerStyles* lexer(std::string_view name) const;

    const std::vector<Style>& globalStyles() const { return _globalStyles; }
    const Style* globalById(int id) const;
    const Style* globalByName(std::string_view name) const;

    // Primera parte de getLangFromExt: extensiones que el usuario añadió a un
    // lexer en el Configurador de estilos. Text si ninguno la declara.
    LangType langFromUserExtension(std::string_view extension) const;

    GlobalOverride& globalOverride() { return _globalOverride; }
    const GlobalOverride& globalOverride() const { return _globalOverride; }

private:
    std::vector<LexerStyles> _lexers;
    std::vector<Style> _globalStyles;
    GlobalOverride _globalOverride;
};

} // namespace edita
