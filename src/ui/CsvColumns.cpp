// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/CsvColumns.h"

#include "core/CsvTable.h"
#include "ui/EditorView.h"

#include <algorithm>

namespace edita {

namespace {

// 0xBBGGRR. Ocho tonos distintos entre sí, que se leen en cada tema
constexpr sptr_t kLight[] = {0x8B3A1F, 0x2E7D32, 0x0050C8, 0x8E24AA, 0x00838F, 0x6D4C41, 0x3949AB, 0x00897B};
constexpr sptr_t kDark[] = {0xF0C27B, 0x8BD88B, 0x5AB4FF, 0xE39BE6, 0xD4D04D, 0xA1B4C8, 0xFF9E9E, 0x9EE6C8};

} // namespace

CsvColumns::CsvColumns(EditorView* view)
    : QObject(view)
    , _view(view)
{
    view->send(SCI_INDICSETSTYLE, kIndicator, INDIC_TEXTFORE);
    view->send(SCI_INDICSETFLAGS, kIndicator, SC_INDICFLAG_VALUEFORE);
    _timer.setSingleShot(true);
    _timer.setInterval(60);
    connect(&_timer, &QTimer::timeout, this, &CsvColumns::updateNow);
    connect(view, &ScintillaEditBase::updateUi, this, [this](Scintilla::Update updated) {
        if (static_cast<int>(updated) & (SC_UPDATE_V_SCROLL | SC_UPDATE_H_SCROLL))
            schedule();
    });
    connect(view, &ScintillaEditBase::modified, this,
            [this](Scintilla::ModificationFlags type, Scintilla::Position, Scintilla::Position, Scintilla::Position, const QByteArray&,
                   Scintilla::Position, Scintilla::FoldLevel, Scintilla::FoldLevel) {
                // Solo el texto: sus propios indicadores también avisan
                if (static_cast<int>(type) & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
                    schedule();
            });
}

void CsvColumns::setActive(bool active, QChar delimiter)
{
    if (active == _active && delimiter == _delimiter)
        return;
    _active = active;
    _delimiter = delimiter;
    // Después: puede llegar a mitad de un cambio del texto (al ordenar, el documento pasa a
    // «modificado» antes de terminar de reemplazarse), y leerlo entonces no es seguro
    if (active)
        schedule();
    else
        clear();
}

void CsvColumns::schedule()
{
    if (_active)
        _timer.start();
}

void CsvColumns::clear()
{
    _timer.stop();
    if (!_view)
        return;
    _view->send(SCI_SETINDICATORCURRENT, kIndicator);
    _view->send(SCI_INDICATORCLEARRANGE, 0, _view->send(SCI_GETLENGTH));
}

void CsvColumns::updateNow()
{
    _timer.stop();
    if (!_view || !_active)
        return;
    const sptr_t firstVisible = _view->send(SCI_GETFIRSTVISIBLELINE);
    const sptr_t firstLine = _view->send(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(firstVisible));
    // La pantalla puede tener más líneas que el documento: solo las que existen
    const sptr_t lastLine = std::min(_view->send(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(firstVisible + _view->send(SCI_LINESONSCREEN) + 1)),
                                     _view->send(SCI_GETLINECOUNT) - 1);
    const sptr_t back = _view->send(SCI_STYLEGETBACK, STYLE_DEFAULT);
    const bool dark = ((back & 0xFF) * 3 + ((back >> 8) & 0xFF) * 6 + ((back >> 16) & 0xFF)) / 10 < 128;
    const sptr_t* palette = dark ? kDark : kLight;
    constexpr int kColors = 8;

    _view->send(SCI_SETINDICATORCURRENT, kIndicator);
    const sptr_t start = _view->send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(firstLine));
    const sptr_t end = _view->send(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(lastLine));
    _view->send(SCI_INDICATORCLEARRANGE, static_cast<uptr_t>(start), end - start);
    for (sptr_t line = firstLine; line <= lastLine; ++line) {
        const sptr_t lineStart = _view->send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
        const sptr_t lineEnd = _view->send(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line));
        if (lineEnd <= lineStart)
            continue;
        const auto* raw = reinterpret_cast<const char*>(_view->send(SCI_GETRANGEPOINTER, static_cast<uptr_t>(lineStart), lineEnd - lineStart));
        // Los campos, en UTF-16; las posiciones del documento, en bytes de UTF-8
        const QByteArray bytes(raw, static_cast<qsizetype>(lineEnd - lineStart));
        const QString text = QString::fromUtf8(bytes);
        const std::vector<csv::Span> spans = csv::splitLine(text, _delimiter);
        for (size_t i = 0; i < spans.size(); ++i) {
            if (spans[i].length == 0)
                continue;
            const sptr_t from = lineStart + text.left(spans[i].start).toUtf8().size();
            const sptr_t length = text.mid(spans[i].start, spans[i].length).toUtf8().size();
            _view->send(SCI_SETINDICATORVALUE, static_cast<uptr_t>(SC_INDICVALUEBIT | palette[i % kColors]));
            _view->send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(from), length);
        }
    }
}

intptr_t CsvColumns::colorAt(intptr_t pos) const
{
    if (!_view || !_view->send(SCI_INDICATORVALUEAT, kIndicator, pos))
        return -1;
    return _view->send(SCI_INDICATORVALUEAT, kIndicator, pos) & 0xFFFFFF;
}

} // namespace edita
