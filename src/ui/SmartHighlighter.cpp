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


#include "ui/SmartHighlighter.h"

#include "syntax/SyntaxStyler.h"
#include "ui/EditorView.h"

#include <algorithm>
#include <string>

namespace edita {

namespace {

// Más líneas que esto no se resaltan aunque quepan en pantalla (MAXLINEHIGHLIGHT)
constexpr sptr_t kMaxLinesHighlight = 400;

void clearSmart(EditorView& view)
{
    view.sci(SCI_SETINDICATORCURRENT, kIndicatorSmart);
    view.sci(SCI_INDICATORCLEARRANGE, 0, view.sci(SCI_GETLENGTH));
}

// SmartHighlighter::highlightViewWithWord: busca solo en las líneas visibles
void highlightWord(EditorView& view, const std::string& word, int flags)
{
    const sptr_t savedStart = view.sci(SCI_GETTARGETSTART);
    const sptr_t savedEnd = view.sci(SCI_GETTARGETEND);
    const sptr_t firstLine = view.sci(SCI_GETFIRSTVISIBLELINE);
    const sptr_t lastLine = firstLine + std::min(view.sci(SCI_LINESONSCREEN), kMaxLinesHighlight) + 1;
    const sptr_t docLength = view.sci(SCI_GETLENGTH);

    // Rango del documento que se ve, de la primera a la última línea visible
    const sptr_t fromLine = view.sci(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(firstLine));
    const sptr_t toLine = view.sci(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(lastLine));
    const sptr_t rangeStart = view.sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(fromLine));
    sptr_t rangeEnd = view.sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(toLine));
    if (rangeEnd < 0)
        rangeEnd = docLength;

    view.sci(SCI_SETINDICATORCURRENT, kIndicatorSmart);
    view.sci(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(flags));
    sptr_t pos = rangeStart;
    while (pos < rangeEnd) {
        view.sci(SCI_SETTARGETRANGE, static_cast<uptr_t>(pos), rangeEnd);
        const sptr_t found = view.sends(SCI_SEARCHINTARGET, word.size(), word.c_str());
        if (found < 0)
            break;
        const sptr_t end = view.sci(SCI_GETTARGETEND);
        view.sci(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(found), end - found);
        pos = end > found ? end : found + 1;
    }
    view.sci(SCI_SETTARGETRANGE, static_cast<uptr_t>(savedStart), savedEnd);
}

} // namespace

void smartHighlight(EditorView& view, EditorView* otherView, const SmartHighlightOptions& options)
{
    clearSmart(view);
    const bool otherVisible = options.otherView && otherView && otherView->isVisible();
    const bool otherIsClone = otherVisible && otherView->sci(SCI_GETDOCPOINTER) == view.sci(SCI_GETDOCPOINTER);
    if (otherVisible && !otherIsClone)
        clearSmart(*otherView);
    if (!options.enabled || view.sci(SCI_GETSELECTIONEMPTY) || view.sci(SCI_GETSELECTIONS) != 1)
        return;

    const sptr_t caret = view.sci(SCI_GETCURRENTPOS);
    const sptr_t selStart = view.sci(SCI_GETSELECTIONSTART);
    const sptr_t selEnd = view.sci(SCI_GETSELECTIONEND);
    if (options.wholeWord) {
        // La selección tiene que ser exactamente la palabra del cursor
        const sptr_t wordStart = view.sci(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(caret), 1);
        const sptr_t wordEnd = view.sci(SCI_WORDENDPOSITION, static_cast<uptr_t>(wordStart), 1);
        if (wordStart == wordEnd || wordStart != selStart || wordEnd != selEnd)
            return;
    } else {
        const sptr_t line = view.sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(caret));
        if (selEnd - selStart > view.sci(SCI_LINELENGTH, static_cast<uptr_t>(line)))
            return;
    }

    const auto length = static_cast<std::size_t>(selEnd - selStart);
    std::string buffer(length + 1, '\0');
    Sci_TextRangeFull range{{selStart, selEnd}, buffer.data()};
    view.sci(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&range));
    const std::string word(buffer.data(), length);
    if (word.empty() || word.find_first_of("\r\n") != std::string::npos)
        return;

    const int flags = (options.matchCase ? SCFIND_MATCHCASE : 0) | (options.wholeWord ? SCFIND_WHOLEWORD : 0);
    highlightWord(view, word, flags);
    if (otherVisible)
        highlightWord(*otherView, word, flags);
}

} // namespace edita
