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

#include "search/SearchTarget.h"

#include <ScintillaEditBase.h>

// Internos de Scintilla para usar Document sin vista (mismo orden que BoostRegExSearch.cxx)
#include <optional>
#include <vector>
#include "Scintilla.h"
#include "ScintillaTypes.h"
#include "ScintillaMessages.h"
#include "Debugging.h"
#include "Geometry.h"
#include "Platform.h"
#include "ILoader.h"
#include "ILexer.h"
#include "Position.h"
#include "UniqueString.h"
#include "SplitVector.h"
#include "Partitioning.h"
#include "RunStyles.h"
#include "ContractionState.h"
#include "CellBuffer.h"
#include "CharClassify.h"
#include "Decoration.h"
#include "CaseFolder.h"
#include "CharacterCategoryMap.h"
#include "Document.h"
#include "BoostRegexSearch.h"

namespace edita {

using Scintilla::Internal::Document;

std::string SearchTarget::lastRegexError()
{
    return g_exceptionMessage;
}

// --- ViewSearchTarget ----------------------------------------------------------------

SciPos ViewSearchTarget::length() const
{
    return _view.send(SCI_GETLENGTH);
}

SciPos ViewSearchTarget::find(SciPos start, SciPos end, std::string_view pattern, unsigned int flags, SciPos& matchEnd)
{
    // ScintillaEditView::searchInTarget
    _view.send(SCI_SETSEARCHFLAGS, flags);
    _view.send(SCI_SETTARGETRANGE, static_cast<uptr_t>(start), end);
    const SciPos pos = _view.send(SCI_SEARCHINTARGET, pattern.size(), reinterpret_cast<sptr_t>(pattern.data()));
    matchEnd = pos >= 0 ? _view.send(SCI_GETTARGETEND) : pos;
    return pos;
}

SciPos ViewSearchTarget::replace(SciPos start, SciPos end, std::string_view text, bool regex)
{
    // replaceTarget / replaceTargetRegExMode: con -1 Scintilla usaría strlen
    _view.send(SCI_SETTARGETRANGE, static_cast<uptr_t>(start), end);
    return _view.send(regex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET, text.size(), reinterpret_cast<sptr_t>(text.data()));
}

char ViewSearchTarget::charAt(SciPos pos) const
{
    return static_cast<char>(_view.send(SCI_GETCHARAT, static_cast<uptr_t>(pos)));
}

SciPos ViewSearchTarget::lineFromPosition(SciPos pos) const
{
    return _view.send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos));
}

SciPos ViewSearchTarget::lineStart(SciPos line) const
{
    return _view.send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
}

SciPos ViewSearchTarget::lineEnd(SciPos line) const
{
    return _view.send(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line));
}

SciPos ViewSearchTarget::lineCount() const
{
    return _view.send(SCI_GETLINECOUNT);
}

std::string ViewSearchTarget::text(SciPos start, SciPos end) const
{
    if (end <= start)
        return {};
    std::string out(static_cast<std::size_t>(end - start) + 1, '\0');
    Sci_TextRangeFull range{{start, end}, out.data()};
    _view.send(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&range));
    out.resize(static_cast<std::size_t>(end - start));
    return out;
}

// --- DocumentSearchTarget --------------------------------------------------------------

DocumentSearchTarget::DocumentSearchTarget(std::string_view utf8)
    : _doc(new Document(Scintilla::DocumentOption::StylesNone | Scintilla::DocumentOption::TextLarge))
{
    _doc->AddRef();
    _doc->SetDBCSCodePage(SC_CP_UTF8);
    // Como ScintillaQt::CaseFolderForEncoding con UTF-8 (para "sin distinguir mayúsculas")
    _doc->SetCaseFolder(std::make_unique<Scintilla::Internal::CaseFolderUnicode>());
    _doc->SetUndoCollection(false);
    _doc->InsertString(0, utf8);
}

DocumentSearchTarget::~DocumentSearchTarget()
{
    _doc->Release();
}

SciPos DocumentSearchTarget::length() const
{
    return _doc->Length();
}

SciPos DocumentSearchTarget::find(SciPos start, SciPos end, std::string_view pattern, unsigned int flags, SciPos& matchEnd)
{
    // Lo mismo que Editor::SearchInTarget: el patrón se pasa con su longitud
    const std::string search(pattern); // FindText puede leer search[length]
    Sci::Position length = static_cast<Sci::Position>(search.size());
    const SciPos pos = _doc->FindText(start, end, search.c_str(), static_cast<Scintilla::FindOption>(flags), &length);
    matchEnd = pos >= 0 ? pos + length : pos;
    return pos;
}

SciPos DocumentSearchTarget::replace(SciPos start, SciPos end, std::string_view text, bool regex)
{
    // Editor::ReplaceTarget: en regex la sustitución sale del último resultado
    std::string replacement;
    if (regex) {
        const std::string templ(text);
        Sci::Position length = static_cast<Sci::Position>(templ.size());
        const char* substituted = _doc->SubstituteByPosition(templ.c_str(), &length);
        if (substituted)
            replacement.assign(substituted, static_cast<std::size_t>(length));
    } else {
        replacement.assign(text);
    }
    if (end > start)
        _doc->DeleteChars(start, end - start);
    _doc->InsertString(start, replacement);
    return static_cast<SciPos>(replacement.size());
}

char DocumentSearchTarget::charAt(SciPos pos) const
{
    return _doc->CharAt(pos);
}

SciPos DocumentSearchTarget::lineFromPosition(SciPos pos) const
{
    return _doc->SciLineFromPosition(pos);
}

SciPos DocumentSearchTarget::lineStart(SciPos line) const
{
    return _doc->LineStart(line);
}

SciPos DocumentSearchTarget::lineEnd(SciPos line) const
{
    return _doc->LineEnd(line);
}

SciPos DocumentSearchTarget::lineCount() const
{
    return _doc->LinesTotal();
}

std::string DocumentSearchTarget::text(SciPos start, SciPos end) const
{
    if (end <= start)
        return {};
    std::string out(static_cast<std::size_t>(end - start), '\0');
    _doc->GetCharRange(out.data(), start, end - start);
    return out;
}

std::string_view DocumentSearchTarget::allText() const
{
    return {_doc->BufferPointer(), static_cast<std::size_t>(_doc->Length())};
}

} // namespace edita
