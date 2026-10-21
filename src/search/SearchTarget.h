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

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

class ScintillaEditBase;

namespace Scintilla::Internal {
class Document;
}

namespace edita {

using SciPos = std::intptr_t;

// Resultados especiales de SearchTarget::find (como SCI_SEARCHINTARGET)
inline constexpr SciPos kNotFound = -1;
inline constexpr SciPos kInvalidRegex = -2; // FIND_INVALID_REGULAR_EXPRESSION
inline constexpr SciPos kRegexFailure = -3; // p. ej. expresión demasiado compleja

// Un texto sobre el que buscar y reemplazar. Lo que N++ hace con mensajes SCI_*
// sobre una vista (ScintillaEditView::searchInTarget, replaceTarget…), separado
// para poder usar también un documento sin vista en un hilo de trabajo.
// Las posiciones son bytes UTF-8 del documento.
class SearchTarget
{
public:
    virtual ~SearchTarget() = default;

    virtual SciPos length() const = 0;
    // Busca entre `start` y `end` (end < start = hacia atrás) con flags SCFIND_*.
    // Devuelve el inicio y deja el final en `matchEnd`; o kNotFound, kInvalidRegex, kRegexFailure.
    virtual SciPos find(SciPos start, SciPos end, std::string_view pattern, unsigned int flags, SciPos& matchEnd) = 0;
    // Sustituye [start, end) — que debe ser lo último encontrado — por `text`.
    // En regex, `text` es una plantilla ($1, \1…). Devuelve la longitud insertada.
    virtual SciPos replace(SciPos start, SciPos end, std::string_view text, bool regex) = 0;

    virtual char charAt(SciPos pos) const = 0;
    virtual SciPos lineFromPosition(SciPos pos) const = 0;
    virtual SciPos lineStart(SciPos line) const = 0;
    virtual SciPos lineEnd(SciPos line) const = 0; // sin el fin de línea
    virtual SciPos lineCount() const = 0;
    virtual std::string text(SciPos start, SciPos end) const = 0;

    // Mensaje de la última regex inválida (g_exceptionMessage de BoostRegexSearch)
    static std::string lastRegexError();
};

// Una vista de Scintilla (el documento que muestre en ese momento)
class ViewSearchTarget : public SearchTarget
{
public:
    explicit ViewSearchTarget(ScintillaEditBase& view) : _view(view) {}

    SciPos length() const override;
    SciPos find(SciPos start, SciPos end, std::string_view pattern, unsigned int flags, SciPos& matchEnd) override;
    SciPos replace(SciPos start, SciPos end, std::string_view text, bool regex) override;
    char charAt(SciPos pos) const override;
    SciPos lineFromPosition(SciPos pos) const override;
    SciPos lineStart(SciPos line) const override;
    SciPos lineEnd(SciPos line) const override;
    SciPos lineCount() const override;
    std::string text(SciPos start, SciPos end) const override;

private:
    ScintillaEditBase& _view;
};

// Un documento de Scintilla sin vista (Scintilla::Internal::Document), que se
// puede usar en cualquier hilo. Es la misma clase que hay detrás de cada vista,
// con la misma regex Boost, así que busca exactamente igual.
class DocumentSearchTarget : public SearchTarget
{
public:
    explicit DocumentSearchTarget(std::string_view utf8);
    ~DocumentSearchTarget() override;
    DocumentSearchTarget(const DocumentSearchTarget&) = delete;
    DocumentSearchTarget& operator=(const DocumentSearchTarget&) = delete;

    SciPos length() const override;
    SciPos find(SciPos start, SciPos end, std::string_view pattern, unsigned int flags, SciPos& matchEnd) override;
    SciPos replace(SciPos start, SciPos end, std::string_view text, bool regex) override;
    char charAt(SciPos pos) const override;
    SciPos lineFromPosition(SciPos pos) const override;
    SciPos lineStart(SciPos line) const override;
    SciPos lineEnd(SciPos line) const override;
    SciPos lineCount() const override;
    std::string text(SciPos start, SciPos end) const override;

    // Todo el texto (para guardarlo tras reemplazar)
    std::string_view allText() const;

private:
    Scintilla::Internal::Document* _doc;
};

} // namespace edita
