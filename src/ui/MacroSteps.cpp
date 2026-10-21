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


#include "ui/MacroSteps.h"

namespace edita {

MacroStep recordedStep(int message, uptr_t wParam, sptr_t lParam)
{
    MacroStep step;
    step.type = MacroStep::UseLParameter;
    step.message = message;
    step.wParam = wParam;
    step.lParam = lParam;
    if (lParam) {
        switch (message) {
        case SCI_SETTEXT:
        case SCI_REPLACESEL:
        case SCI_REPLACETARGET:
        case SCI_REPLACETARGETRE:
        case SCI_SEARCHINTARGET:
        case SCI_ADDTEXT:
        case SCI_INSERTTEXT:
        case SCI_APPENDTEXT:
        case SCI_SEARCHNEXT:
        case SCI_SEARCHPREV:
            step.sParam = reinterpret_cast<const char*>(lParam);
            step.type = MacroStep::UseSParameter;
            step.lParam = 0;
            break;
        default:
            break;
        }
    }
    return step;
}

bool isMacroable(const MacroStep& step)
{
    switch (step.message) {
    case SCI_REPLACESEL:
    case SCI_ADDTEXT:
    case SCI_INSERTTEXT:
    case SCI_APPENDTEXT:
    case SCI_SEARCHNEXT:
    case SCI_SEARCHPREV:
        return step.type == MacroStep::UseSParameter;
    case SCI_GOTOLINE:
    case SCI_GOTOPOS:
    case SCI_SETSELECTIONMODE:
    case SCI_CUT:
    case SCI_COPY:
    case SCI_PASTE:
    case SCI_CLEAR:
    case SCI_CLEARALL:
    case SCI_SELECTALL:
    case SCI_SEARCHANCHOR:
    case SCI_LINEDOWN:
    case SCI_LINEDOWNEXTEND:
    case SCI_PARADOWN:
    case SCI_PARADOWNEXTEND:
    case SCI_LINEUP:
    case SCI_LINEUPEXTEND:
    case SCI_PARAUP:
    case SCI_PARAUPEXTEND:
    case SCI_CHARLEFT:
    case SCI_CHARLEFTEXTEND:
    case SCI_CHARRIGHT:
    case SCI_CHARRIGHTEXTEND:
    case SCI_WORDLEFT:
    case SCI_WORDLEFTEXTEND:
    case SCI_WORDRIGHT:
    case SCI_WORDRIGHTEXTEND:
    case SCI_WORDPARTLEFT:
    case SCI_WORDPARTLEFTEXTEND:
    case SCI_WORDPARTRIGHT:
    case SCI_WORDPARTRIGHTEXTEND:
    case SCI_WORDLEFTEND:
    case SCI_WORDLEFTENDEXTEND:
    case SCI_WORDRIGHTEND:
    case SCI_WORDRIGHTENDEXTEND:
    case SCI_HOME:
    case SCI_HOMEEXTEND:
    case SCI_LINEEND:
    case SCI_LINEENDEXTEND:
    case SCI_HOMEWRAP:
    case SCI_HOMEWRAPEXTEND:
    case SCI_LINEENDWRAP:
    case SCI_LINEENDWRAPEXTEND:
    case SCI_DOCUMENTSTART:
    case SCI_DOCUMENTSTARTEXTEND:
    case SCI_DOCUMENTEND:
    case SCI_DOCUMENTENDEXTEND:
    case SCI_STUTTEREDPAGEUP:
    case SCI_STUTTEREDPAGEUPEXTEND:
    case SCI_STUTTEREDPAGEDOWN:
    case SCI_STUTTEREDPAGEDOWNEXTEND:
    case SCI_PAGEUP:
    case SCI_PAGEUPEXTEND:
    case SCI_PAGEDOWN:
    case SCI_PAGEDOWNEXTEND:
    case SCI_EDITTOGGLEOVERTYPE:
    case SCI_CANCEL:
    case SCI_DELETEBACK:
    case SCI_TAB:
    case SCI_BACKTAB:
    case SCI_FORMFEED:
    case SCI_VCHOME:
    case SCI_VCHOMEEXTEND:
    case SCI_VCHOMEWRAP:
    case SCI_VCHOMEWRAPEXTEND:
    case SCI_VCHOMEDISPLAY:
    case SCI_VCHOMEDISPLAYEXTEND:
    case SCI_DELWORDLEFT:
    case SCI_DELWORDRIGHT:
    case SCI_DELWORDRIGHTEND:
    case SCI_DELLINELEFT:
    case SCI_DELLINERIGHT:
    case SCI_LINECOPY:
    case SCI_LINECUT:
    case SCI_LINEDELETE:
    case SCI_LINETRANSPOSE:
    case SCI_LINEDUPLICATE:
    case SCI_LOWERCASE:
    case SCI_UPPERCASE:
    case SCI_LINESCROLLDOWN:
    case SCI_LINESCROLLUP:
    case SCI_DELETEBACKNOTLINE:
    case SCI_HOMEDISPLAY:
    case SCI_HOMEDISPLAYEXTEND:
    case SCI_LINEENDDISPLAY:
    case SCI_LINEENDDISPLAYEXTEND:
    case SCI_LINEDOWNRECTEXTEND:
    case SCI_LINEUPRECTEXTEND:
    case SCI_CHARLEFTRECTEXTEND:
    case SCI_CHARRIGHTRECTEXTEND:
    case SCI_HOMERECTEXTEND:
    case SCI_VCHOMERECTEXTEND:
    case SCI_LINEENDRECTEXTEND:
    case SCI_PAGEUPRECTEXTEND:
    case SCI_PAGEDOWNRECTEXTEND:
    case SCI_SELECTIONDUPLICATE:
    case SCI_COPYALLOWLINE:
    case SCI_VERTICALCENTRECARET:
    case SCI_MOVESELECTEDLINESUP:
    case SCI_MOVESELECTEDLINESDOWN:
    case SCI_SCROLLTOSTART:
    case SCI_SCROLLTOEND:
    case SCI_SETVIRTUALSPACEOPTIONS:
    case SCI_SETCARETLINEBACKALPHA:
    case SCI_NEWLINE:
        return true;
    default:
        return false;
    }
}

void playScintillaStep(ScintillaEditBase& view, const MacroStep& step)
{
    if (step.type > MacroStep::MenuCommand || !isMacroable(step))
        return;
    const auto message = static_cast<unsigned int>(step.message);
    if (step.type == MacroStep::UseSParameter)
        view.send(message, step.wParam, reinterpret_cast<sptr_t>(step.sParam.c_str()));
    else
        view.send(message, step.wParam, step.lParam);
}

} // namespace edita
