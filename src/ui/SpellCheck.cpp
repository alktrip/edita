// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/SpellCheck.h"

#include "core/SpellChecker.h"
#include "search/SearchTarget.h"
#include "syntax/StyleSheet.h"
#include "ui/EditorView.h"

namespace edita {

std::array<bool, 256> spellCheckedStyles(EditorView& view, const LexerStyles* styles, bool onlyCommentsAndStrings)
{
    std::array<bool, 256> checked{};
    // Sin lexer (texto normal) o sin la opción: todo
    QByteArray lexer(64, '\0');
    if (view.sci(SCI_GETLEXERLANGUAGE) < 60)
        view.sci(SCI_GETLEXERLANGUAGE, 0, reinterpret_cast<sptr_t>(lexer.data()));
    const QByteArray name(lexer.constData());
    const bool hasLexer = !name.isEmpty() && name != "null";
    if (!onlyCommentsAndStrings || !hasLexer) {
        checked.fill(true);
        return checked;
    }
    QByteArray buffer(512, '\0');
    for (int style = 0; style < 256; ++style) {
        QString what;
        // Lo que dice el lexer de Lexilla (no todos lo dicen)
        for (const unsigned message : {static_cast<unsigned>(SCI_TAGSOFSTYLE), static_cast<unsigned>(SCI_NAMEOFSTYLE)}) {
            buffer.fill('\0');
            if (view.sci(message, static_cast<uptr_t>(style), 0) < 500) {
                view.sci(message, static_cast<uptr_t>(style), reinterpret_cast<sptr_t>(buffer.data()));
                what += QString::fromLatin1(buffer.constData()) + QLatin1Char(' ');
            }
        }
        if (styles) {
            if (const Style* s = styles->findById(style))
                what += QString::fromStdString(s->name);
        }
        what = what.toLower();
        const bool prose = what.contains(QLatin1String("comment")) || what.contains(QLatin1String("string"));
        const bool code = what.contains(QLatin1String("keyword")) || what.contains(QLatin1String("regex"))
                       || what.contains(QLatin1String("operator")) || what.contains(QLatin1String("tag"));
        checked[static_cast<std::size_t>(style)] = prose && !code;
    }
    return checked;
}

void highlightMisspellings(EditorView& view, const SpellChecker& checker, const std::array<bool, 256>& styles, bool enabled)
{
    if (view.sci(SCI_INDICGETSTYLE, kIndicatorSpelling) != INDIC_SQUIGGLEPIXMAP) {
        view.sci(SCI_INDICSETSTYLE, kIndicatorSpelling, INDIC_SQUIGGLEPIXMAP);
        view.sci(SCI_INDICSETFORE, kIndicatorSpelling, 0x2020D0); // rojo (BGR)
        view.sci(SCI_INDICSETUNDER, kIndicatorSpelling, 1);
    }
    const sptr_t firstLine = view.sci(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(view.sci(SCI_GETFIRSTVISIBLELINE)));
    const sptr_t lastLine = view.sci(SCI_DOCLINEFROMVISIBLE,
                                     static_cast<uptr_t>(view.sci(SCI_GETFIRSTVISIBLELINE) + view.sci(SCI_LINESONSCREEN) + 1));
    const sptr_t start = view.sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(firstLine));
    const sptr_t end = view.sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(lastLine));
    view.sci(SCI_SETINDICATORCURRENT, kIndicatorSpelling);
    view.sci(SCI_INDICATORCLEARRANGE, static_cast<uptr_t>(start), end - start);
    if (!enabled || !checker.isLoaded() || start >= end)
        return;
    // Que el lexer haya pintado lo que se ve (los estilos dicen qué es comentario)
    view.sci(SCI_COLOURISE, static_cast<uptr_t>(start), end);
    const bool utf8 = view.sci(SCI_GETCODEPAGE) == SC_CP_UTF8;
    const std::string bytes = ViewSearchTarget(view).text(start, end);
    const QString text = utf8 ? QString::fromUtf8(bytes) : QString::fromLatin1(bytes);
    // De la posición en el QString a la del documento
    qsizetype unit = 0;
    sptr_t byte = start;
    auto advance = [&](qsizetype to) {
        if (!utf8) {
            byte += to - unit;
            unit = to;
            return;
        }
        for (; unit < to; ++unit) {
            const char16_t c = text.at(unit).unicode();
            if (QChar::isHighSurrogate(c)) {
                byte += 4;
                ++unit;
            } else {
                byte += c < 0x80 ? 1 : c < 0x800 ? 2 : 3;
            }
        }
    };
    for (const WordSpan& word : wordsToCheck(text)) {
        advance(word.start);
        const sptr_t from = byte;
        advance(word.start + word.length);
        const auto style = static_cast<std::size_t>(view.sci(SCI_GETSTYLEINDEXAT, static_cast<uptr_t>(from)) & 0xFF);
        if (!styles[style] || checker.spell(text.mid(word.start, word.length)))
            continue;
        view.sci(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(from), byte - from);
    }
}

QString misspelledWordAt(EditorView& view, sptr_t position, sptr_t* start, sptr_t* end)
{
    for (const sptr_t pos : {position, position - 1}) {
        if (pos < 0 || !view.sci(SCI_INDICATORVALUEAT, kIndicatorSpelling, pos))
            continue;
        const sptr_t s = view.sci(SCI_INDICATORSTART, kIndicatorSpelling, pos);
        const sptr_t e = view.sci(SCI_INDICATOREND, kIndicatorSpelling, pos);
        if (e <= s)
            continue;
        if (start)
            *start = s;
        if (end)
            *end = e;
        return QString::fromStdString(ViewSearchTarget(view).text(s, e));
    }
    return {};
}

} // namespace edita
