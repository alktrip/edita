// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/RainbowBrackets.h"

#include "ui/EditorView.h"

#include <string>

namespace edita {

namespace {

constexpr sptr_t kMaxBytes = 2 * 1024 * 1024; // documentos mayores: sin colores

// 0xBBGGRR. Los de VS Code (bracketPairColorization), que se leen en su tema
constexpr sptr_t kLight[] = {0xFA3104, 0x319331, 0x14387B};
constexpr sptr_t kDark[] = {0x00D7FF, 0xD670DA, 0xFF9F17};
constexpr sptr_t kErrorLight = 0x2E22CF;
constexpr sptr_t kErrorDark = 0x4951F8;

char closerOf(char open)
{
    return open == '(' ? ')' : open == '[' ? ']' : '}';
}

} // namespace

RainbowBrackets::RainbowBrackets(EditorView* view)
    : QObject(view)
    , _view(view)
{
    view->send(SCI_INDICSETSTYLE, kIndicator, INDIC_TEXTFORE);
    view->send(SCI_INDICSETFLAGS, kIndicator, SC_INDICFLAG_VALUEFORE);
    _timer.setSingleShot(true);
    _timer.setInterval(60);
    connect(&_timer, &QTimer::timeout, this, &RainbowBrackets::updateNow);
    connect(view, &ScintillaEditBase::updateUi, this, [this](Scintilla::Update updated) {
        if (static_cast<int>(updated) & SC_UPDATE_V_SCROLL)
            schedule();
    });
    connect(view, &ScintillaEditBase::modified, this, [this](Scintilla::ModificationFlags type, Scintilla::Position, Scintilla::Position, Scintilla::Position, const QByteArray&,
                                                       Scintilla::Position, Scintilla::FoldLevel, Scintilla::FoldLevel) {
        // Solo el texto: sus propios indicadores también avisan, y volver a calcular por
        // eso era un bucle sin fin (que además no dejaba en paz a los plugins)
        if (static_cast<int>(type) & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
            schedule();
    });
}

void RainbowBrackets::setEnabled(bool enabled)
{
    _enabled = enabled;
    if (enabled)
        schedule();
    else
        clear();
}

void RainbowBrackets::setIgnoredStyles(const std::bitset<256>& styles)
{
    _ignored = styles;
    schedule();
}

void RainbowBrackets::schedule()
{
    if (_enabled)
        _timer.start();
}

void RainbowBrackets::clear()
{
    if (!_view)
        return;
    _view->send(SCI_SETINDICATORCURRENT, kIndicator);
    _view->send(SCI_INDICATORCLEARRANGE, 0, _view->send(SCI_GETLENGTH));
}

void RainbowBrackets::updateNow()
{
    _timer.stop();
    if (!_view)
        return;
    const sptr_t length = _view->send(SCI_GETLENGTH);
    if (!_enabled || length > kMaxBytes) {
        clear();
        return;
    }
    // Lo visible (con margen), y la profundidad contada desde el principio
    const sptr_t firstVisible = _view->send(SCI_GETFIRSTVISIBLELINE);
    const sptr_t firstLine = _view->send(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(firstVisible));
    const sptr_t lastLine = _view->send(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(firstVisible + _view->send(SCI_LINESONSCREEN) + 1));
    const sptr_t start = _view->send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(firstLine));
    const sptr_t end = _view->send(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(lastLine));
    // Los estilos tienen que estar calculados hasta ahí para saber qué es comentario
    _view->send(SCI_COLOURISE, 0, end);
    const auto* text = reinterpret_cast<const char*>(_view->send(SCI_GETCHARACTERPOINTER));
    const sptr_t back = _view->send(SCI_STYLEGETBACK, STYLE_DEFAULT);
    const bool dark = ((back & 0xFF) * 3 + ((back >> 8) & 0xFF) * 6 + ((back >> 16) & 0xFF)) / 10 < 128;
    const sptr_t* palette = dark ? kDark : kLight;
    const sptr_t error = dark ? kErrorDark : kErrorLight;

    _view->send(SCI_SETINDICATORCURRENT, kIndicator);
    _view->send(SCI_INDICATORCLEARRANGE, static_cast<uptr_t>(start), end - start);
    std::string stack; // los que abren, en orden
    auto paint = [&](sptr_t pos, sptr_t color) {
        if (pos < start)
            return;
        _view->send(SCI_SETINDICATORVALUE, static_cast<uptr_t>(SC_INDICVALUEBIT | color));
        _view->send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(pos), 1);
    };
    for (sptr_t pos = 0; pos < end; ++pos) {
        const char c = text[pos];
        if (c != '(' && c != ')' && c != '[' && c != ']' && c != '{' && c != '}')
            continue;
        const auto style = static_cast<size_t>(_view->send(SCI_GETSTYLEINDEXAT, static_cast<uptr_t>(pos)) & 0xFF);
        if (_ignored.test(style))
            continue;
        if (c == '(' || c == '[' || c == '{') {
            paint(pos, palette[stack.size() % 3]);
            stack.push_back(c);
        } else if (!stack.empty() && closerOf(stack.back()) == c) {
            stack.pop_back();
            paint(pos, palette[stack.size() % 3]);
        } else {
            paint(pos, error); // sin pareja o la que no le toca
        }
    }
}

intptr_t RainbowBrackets::colorAt(intptr_t pos) const
{
    if (!_view || !_view->send(SCI_INDICATORVALUEAT, kIndicator, pos))
        return -1;
    return _view->send(SCI_INDICATORVALUEAT, kIndicator, pos) & 0xFFFFFF;
}

} // namespace edita
