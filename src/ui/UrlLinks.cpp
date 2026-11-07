// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/UrlLinks.h"

#include "search/SearchTarget.h"
#include "ui/EditorView.h"

#include <QStringList>
#include <QUrl>

#include <algorithm>

namespace edita {

namespace {

bool isSchemeStartChar(QChar c)
{
    return (c >= QLatin1Char('A') && c <= QLatin1Char('Z')) || (c >= QLatin1Char('a') && c <= QLatin1Char('z'));
}

// Lo que puede ir justo antes del esquema
bool isSchemeDelimiter(QChar c)
{
    return !((c >= QLatin1Char('0') && c <= QLatin1Char('9')) || isSchemeStartChar(c) || c == QLatin1Char('_'));
}

// isUrlTextChar: no los espacios (también los de Unicode) ni los caracteres especiales
bool isUrlTextChar(QChar c)
{
    if (c.unicode() <= ' ')
        return false;
    switch (c.unicode()) {
    case 0x00A0: case 0x2002: case 0x2003: case 0x3000: case 0x2004: case 0x2005: case 0x2006:
    case 0x2007: case 0x2008: case 0x2009: case 0x200A: case 0x200B: case 0x202F: case 0x205F: case 0xFEFF:
    case '"': case '#': case '<': case '>': case '{': case '}': case '?': case 0x7F:
        return false;
    default:
        return true;
    }
}

bool isQueryDelimiter(QChar c)
{
    return c == QLatin1Char('&') || c == QLatin1Char('+') || c == QLatin1Char('=') || c == QLatin1Char(';');
}

bool isSchemeSupported(const QString& text, qsizetype at, const QStringList& schemes)
{
    for (const QString& scheme : schemes) {
        if (text.size() - at >= scheme.size() && QStringView(text).mid(at, scheme.size()).compare(scheme, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

// scanToUrlStart: la distancia al siguiente candidato y la longitud de su esquema
bool scanToUrlStart(const QString& text, qsizetype start, const QStringList& schemes, qsizetype& distance, qsizetype& schemeLength)
{
    qsizetype p = start;
    qsizetype p0 = 0;
    bool inScheme = false;
    for (; p < text.size(); ++p) {
        if (!inScheme) {
            if (isSchemeStartChar(text.at(p)) && (p == 0 || isSchemeDelimiter(text.at(p - 1)))) {
                p0 = p;
                inScheme = true;
            }
        } else {
            if (text.at(p) == QLatin1Char(':') && isSchemeSupported(text, p0, schemes)) {
                distance = p0 - start;
                schemeLength = p - p0 + 1;
                return true;
            }
            if (!isSchemeStartChar(text.at(p)))
                inScheme = false;
        }
    }
    schemeLength = 0;
    distance = p - start;
    return false;
}

// scanToUrlEnd: el host y la ruta, la consulta (valores separados por & + = ; y quizá entre
// comillas) y el fragmento
qsizetype scanToUrlEnd(const QString& text, qsizetype start)
{
    enum { HostAndPath, Query, QueryAfterDelimiter, QueryQuotes, QueryAfterQuotes, Fragment } s = HostAndPath;
    QChar quote;
    qsizetype p = start;
    for (; p < text.size(); ++p) {
        const QChar c = text.at(p);
        switch (s) {
        case HostAndPath:
            if (c == QLatin1Char('?'))
                s = Query;
            else if (c == QLatin1Char('#'))
                s = Fragment;
            else if (!isUrlTextChar(c))
                return p - start;
            break;
        case Query:
            if (c == QLatin1Char('#'))
                s = Fragment;
            else if (isQueryDelimiter(c))
                s = QueryAfterDelimiter;
            else if (!isUrlTextChar(c))
                return p - start;
            break;
        case QueryAfterDelimiter:
            if (c == QLatin1Char('\'') || c == QLatin1Char('"') || c == QLatin1Char('`')) {
                quote = c;
                s = QueryQuotes;
            } else if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{')) {
                quote = c == QLatin1Char('(') ? QLatin1Char(')') : c == QLatin1Char('[') ? QLatin1Char(']') : QLatin1Char('}');
                s = QueryQuotes;
            } else if (isUrlTextChar(c)) {
                s = Query;
            } else {
                return p - start;
            }
            break;
        case QueryQuotes:
            if (c.unicode() < ' ')
                return p - start;
            if (c == quote)
                s = QueryAfterQuotes;
            break;
        case QueryAfterQuotes:
            if (isQueryDelimiter(c))
                s = QueryAfterDelimiter;
            else
                return p - start;
            break;
        case Fragment:
            if (c != QLatin1Char('?') && !isUrlTextChar(c))
                return p - start;
            break;
        }
    }
    return p - start;
}

// removeUnwantedTrailingCharFromUrl: un signo de puntuación final o un paréntesis de cierre
// sin su apertura
bool removeTrailingChar(QStringView url, qsizetype& length)
{
    const qsizetype last = length - 1;
    if (last <= 0)
        return false;
    const QChar c = url.at(last);
    if (QStringView(u".,:;?!#").contains(c)) {
        length = last;
        return true;
    }
    for (const auto& [close, open] : {std::pair{u')', u'('}, std::pair{u']', u'['}}) {
        if (c != QChar(close))
            continue;
        int count = 0;
        for (qsizetype j = last - 1; j >= 0; --j) {
            if (url.at(j) == QChar(close))
                ++count;
            if (url.at(j) == QChar(open)) {
                if (count > 0)
                    --count;
                else
                    return false;
            }
        }
        if (count != 0)
            return false;
        length = last;
        return true;
    }
    return false;
}

// En lugar de InternetCrackUrl: que Qt la entienda y, con «//», que tenga servidor
// (salvo file:///ruta)
bool isValidUrl(QStringView candidate)
{
    const QUrl url(candidate.toString(), QUrl::TolerantMode);
    if (!url.isValid() || url.scheme().isEmpty())
        return false;
    const qsizetype colon = candidate.indexOf(QLatin1Char(':'));
    if (candidate.mid(colon + 1).startsWith(QLatin1String("//")))
        return !url.host().isEmpty() || url.scheme().compare(QLatin1String("file"), Qt::CaseInsensitive) == 0;
    return candidate.size() > colon + 1;
}

QStringList schemeList(const QString& extraSchemes)
{
    QStringList schemes{QStringLiteral("ftp://"), QStringLiteral("http://"), QStringLiteral("https://"), QStringLiteral("mailto:"),
                        QStringLiteral("file://")};
    schemes << extraSchemes.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return schemes;
}

} // namespace

std::vector<std::pair<qsizetype, qsizetype>> findUrls(const QString& text, const QString& extraSchemes)
{
    const QStringList schemes = schemeList(extraSchemes);
    std::vector<std::pair<qsizetype, qsizetype>> found;
    qsizetype start = 0;
    while (start < text.size()) {
        qsizetype distance = 0;
        qsizetype schemeLength = 0;
        if (!scanToUrlStart(text, start, schemes, distance, schemeLength))
            break;
        start += distance;
        qsizetype length = scanToUrlEnd(text, start + schemeLength);
        if (length == 0) {
            // Solo el esquema: se salta la palabra
            qsizetype skip = 1;
            while (start + skip < text.size() && isSchemeStartChar(text.at(start + skip)))
                ++skip;
            start += skip;
            continue;
        }
        length += schemeLength;
        if (isValidUrl(QStringView(text).mid(start, length))) {
            // removeUnwantedTrailingCharFromEnclosedUrl: entre comillas simples o acentos graves
            if (start > 0 && length > 1) {
                const QChar before = text.at(start - 1);
                if ((before == QLatin1Char('\'') || before == QLatin1Char('`')) && text.at(start + length - 1) == before)
                    --length;
            }
            while (removeTrailingChar(QStringView(text).mid(start, length), length)) {
            }
            found.emplace_back(start, length);
        }
        start += length; // también si no vale: así no se vuelve a mirar (como N++)
    }
    return found;
}

void highlightUrls(EditorView& view, const UrlLinkOptions& options)
{
    // Notepad_plus::addHotSpot: el estilo del modo, y el indicador solo en lo que se ve
    const sptr_t style = options.mode == 1 || options.mode == 3 ? INDIC_HIDDEN : INDIC_PLAIN;
    const sptr_t hover = options.mode >= 3 ? INDIC_FULLBOX : INDIC_EXPLORERLINK;
    if (view.sci(SCI_INDICGETSTYLE, kIndicatorUrl) != style || view.sci(SCI_INDICGETHOVERSTYLE, kIndicatorUrl) != hover) {
        view.sci(SCI_INDICSETSTYLE, kIndicatorUrl, style);
        view.sci(SCI_INDICSETHOVERSTYLE, kIndicatorUrl, hover);
        view.sci(SCI_INDICSETALPHA, kIndicatorUrl, 70);
        view.sci(SCI_INDICSETFLAGS, kIndicatorUrl, SC_INDICFLAG_VALUEFORE);
    }
    const sptr_t firstLine = view.sci(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(view.sci(SCI_GETFIRSTVISIBLELINE)));
    const sptr_t lastLine = view.sci(SCI_DOCLINEFROMVISIBLE,
                                     static_cast<uptr_t>(view.sci(SCI_GETFIRSTVISIBLELINE) + view.sci(SCI_LINESONSCREEN) + 1));
    const sptr_t start = view.sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(firstLine));
    const sptr_t end = view.sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(lastLine));
    if (start >= end)
        return;
    view.sci(SCI_SETINDICATORCURRENT, kIndicatorUrl);
    view.sci(SCI_INDICATORCLEARRANGE, static_cast<uptr_t>(start), end - start);
    if (options.mode == 0 || !options.allowed)
        return;
    view.sci(SCI_SETINDICATORVALUE, static_cast<uptr_t>(view.sci(SCI_STYLEGETFORE, STYLE_DEFAULT)));

    const std::string bytes = ViewSearchTarget(view).text(start, end);
    const bool utf8 = view.sci(SCI_GETCODEPAGE) == SC_CP_UTF8;
    const QString text = utf8 ? QString::fromUtf8(bytes) : QString::fromLatin1(bytes);
    // De la posición en el QString a la del documento (bytes de UTF-8)
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
    for (const auto& [at, length] : findUrls(text, options.extraSchemes)) {
        advance(at);
        const sptr_t from = byte;
        advance(at + length);
        view.sci(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(from), byte - from);
    }
}

QString urlAt(EditorView& view, qsizetype position)
{
    const auto pos = static_cast<uptr_t>(position);
    if (!(view.sci(SCI_INDICATORALLONFOR, pos) & (1 << kIndicatorUrl)))
        return {};
    const sptr_t start = view.sci(SCI_INDICATORSTART, kIndicatorUrl, static_cast<sptr_t>(pos));
    const sptr_t end = view.sci(SCI_INDICATOREND, kIndicatorUrl, static_cast<sptr_t>(pos));
    if (position < start || position > end || end <= start)
        return {};
    return QString::fromStdString(ViewSearchTarget(view).text(start, end));
}

} // namespace edita
