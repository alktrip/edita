// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/CompareSession.h"

#include <ScintillaEditBase.h>

#include <QChar>
#include <QScrollBar>

#include <algorithm>
#include <map>
#include <string>

namespace edita {

namespace {

constexpr int kMaxLineEdits = 5000; // más cambios: el resto de lo distinto, como un bloque
constexpr int kMaxCharEdits = 300;  // en una línea: más, solo el fondo amarillo

QColor fromScintilla(sptr_t c)
{
    return QColor(static_cast<int>(c & 0xFF), static_cast<int>((c >> 8) & 0xFF), static_cast<int>((c >> 16) & 0xFF));
}

sptr_t toScintilla(const QColor& c)
{
    return c.red() | (c.green() << 8) | (c.blue() << 16);
}

bool isDark(ScintillaEditBase& view)
{
    return fromScintilla(view.send(SCI_STYLEGETBACK, STYLE_DEFAULT)).lightness() < 128;
}

std::string_view documentText(ScintillaEditBase& view)
{
    const sptr_t length = view.send(SCI_GETLENGTH);
    const auto* text = reinterpret_cast<const char*>(view.send(SCI_GETCHARACTERPOINTER));
    return text ? std::string_view(text, static_cast<std::size_t>(length)) : std::string_view();
}

// La línea como se compara: sin espacios o en minúsculas, según las opciones
std::string normalized(std::string_view line, const CompareSession::Options& options)
{
    if (!options.ignoreSpaces && !options.ignoreCase)
        return std::string(line);
    QString text = QString::fromUtf8(line.data(), static_cast<qsizetype>(line.size()));
    if (options.ignoreSpaces)
        text.remove(QChar(' ')).remove(QChar('\t'));
    if (options.ignoreCase)
        text = text.toCaseFolded();
    return text.toStdString();
}

// Los caracteres (UTF-8) de una línea, cada uno como texto
std::vector<std::string_view> characters(std::string_view line)
{
    std::vector<std::string_view> chars;
    for (std::size_t i = 0; i < line.size();) {
        std::size_t length = 1;
        const auto c = static_cast<unsigned char>(line[i]);
        if (c >= 0xF0)
            length = 4;
        else if (c >= 0xE0)
            length = 3;
        else if (c >= 0xC0)
            length = 2;
        length = std::min(length, line.size() - i);
        chars.push_back(line.substr(i, length));
        i += length;
    }
    return chars;
}

void markLines(ScintillaEditBase& view, int marker, int first, int count)
{
    for (int line = first; line < first + count; ++line)
        view.send(SCI_MARKERADD, static_cast<uptr_t>(line), marker);
}

// Las líneas en blanco que alinean las dos vistas: tantas como haga falta tras cada línea
void addPadding(ScintillaEditBase& view, const std::map<int, int>& padding)
{
    for (const auto& [line, count] : padding) {
        if (count <= 0)
            continue;
        std::string text(" ");
        for (int i = 1; i < count; ++i)
            text += "\n ";
        const auto target = static_cast<uptr_t>(std::max(0, line));
        view.send(SCI_ANNOTATIONSETTEXT, target, reinterpret_cast<sptr_t>(text.c_str()));
        view.send(SCI_ANNOTATIONSETSTYLE, target, 0);
    }
}

} // namespace

CompareSession::CompareSession(ScintillaEditBase* oldView, ScintillaEditBase* newView, QObject* parent)
    : QObject(parent)
    , _old(oldView)
    , _new(newView)
{
    _timer.setSingleShot(true);
    _timer.setInterval(300);
    connect(&_timer, &QTimer::timeout, this, &CompareSession::recompute);
    for (ScintillaEditBase* view : {oldView, newView}) {
        // Solo lo que cambia el texto (las anotaciones y marcas que pone esto también avisan)
        connect(view, &ScintillaEditBase::modified, this, [this](Scintilla::ModificationFlags type) {
            if (static_cast<int>(type) & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
                _timer.start();
        });
        // Las barras de desplazamiento cambian en cuanto se mueve la vista (SCN_UPDATEUI
        // llega al pintar)
        connect(view->verticalScrollBar(), &QScrollBar::valueChanged, this, [this, view] { syncScroll(view); });
        connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this, view] { syncScroll(view); });
    }
}

CompareSession::~CompareSession()
{
    for (ScintillaEditBase* view : {_old.data(), _new.data()}) {
        if (view)
            clearDecorations(*view);
    }
}

void CompareSession::setOptions(const Options& options)
{
    _options = options;
    recompute();
}

QColor CompareSession::color(int marker, bool dark)
{
    switch (marker) {
    case kMarkerAdded: return dark ? QColor(0x2E, 0x5E, 0x2E) : QColor(0xC8, 0xF0, 0xC8);
    case kMarkerRemoved: return dark ? QColor(0x6B, 0x2B, 0x2B) : QColor(0xF8, 0xC8, 0xC8);
    case kMarkerChanged: return dark ? QColor(0x5E, 0x55, 0x22) : QColor(0xFA, 0xEE, 0xB0);
    default: return dark ? QColor(0x9E, 0x86, 0x1F) : QColor(0xE8, 0xB8, 0x30); // caracteres distintos
    }
}

void CompareSession::configure(ScintillaEditBase& view)
{
    const bool dark = isDark(view);
    for (const int marker : {kMarkerAdded, kMarkerRemoved, kMarkerChanged}) {
        view.send(SCI_MARKERDEFINE, static_cast<uptr_t>(marker), SC_MARK_BACKGROUND);
        view.send(SCI_MARKERSETBACK, static_cast<uptr_t>(marker), toScintilla(color(marker, dark)));
    }
    // Un marcador de fondo solo pinta la línea si no está en la máscara de ningún margen
    // (el de símbolos de Edita acepta todos los que no son de plegado)
    const int bits = (1 << kMarkerAdded) | (1 << kMarkerRemoved) | (1 << kMarkerChanged);
    for (uptr_t margin = 0; margin < 5; ++margin)
        view.send(SCI_SETMARGINMASKN, margin, view.send(SCI_GETMARGINMASKN, margin) & ~bits);
    view.send(SCI_INDICSETSTYLE, kIndicatorChars, INDIC_FULLBOX);
    view.send(SCI_INDICSETFORE, kIndicatorChars, toScintilla(color(0, dark)));
    view.send(SCI_INDICSETALPHA, kIndicatorChars, dark ? 150 : 120);
    view.send(SCI_INDICSETOUTLINEALPHA, kIndicatorChars, 0);
    view.send(SCI_INDICSETUNDER, kIndicatorChars, 1);
    // Las líneas de relleno: un estilo propio (extendido), rayado en gris
    if (view.send(SCI_ANNOTATIONGETSTYLEOFFSET) == 0)
        view.send(SCI_ANNOTATIONSETSTYLEOFFSET, static_cast<uptr_t>(view.send(SCI_ALLOCATEEXTENDEDSTYLES, 1)));
    const auto style = static_cast<uptr_t>(view.send(SCI_ANNOTATIONGETSTYLEOFFSET));
    const QColor back = fromScintilla(view.send(SCI_STYLEGETBACK, STYLE_DEFAULT));
    view.send(SCI_STYLESETBACK, style, toScintilla(dark ? back.lighter(125) : back.darker(108)));
    view.send(SCI_STYLESETFORE, style, toScintilla(back));
    view.send(SCI_STYLESETSIZEFRACTIONAL, style, view.send(SCI_STYLEGETSIZEFRACTIONAL, STYLE_DEFAULT));
    view.send(SCI_ANNOTATIONSETVISIBLE, ANNOTATION_STANDARD);
}

void CompareSession::clearDecorations(ScintillaEditBase& view)
{
    for (const int marker : {kMarkerAdded, kMarkerRemoved, kMarkerChanged})
        view.send(SCI_MARKERDELETEALL, static_cast<uptr_t>(marker));
    view.send(SCI_ANNOTATIONCLEARALL);
    const sptr_t previous = view.send(SCI_GETINDICATORCURRENT);
    view.send(SCI_SETINDICATORCURRENT, kIndicatorChars);
    view.send(SCI_INDICATORCLEARRANGE, 0, view.send(SCI_GETLENGTH));
    view.send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(previous));
}

void CompareSession::recompute()
{
    _timer.stop();
    if (!_old || !_new)
        return;
    ScintillaEditBase& oldView = *_old;
    ScintillaEditBase& newView = *_new;
    const sptr_t oldFirst = oldView.send(SCI_GETFIRSTVISIBLELINE);
    const sptr_t newFirst = newView.send(SCI_GETFIRSTVISIBLELINE);
    for (ScintillaEditBase* view : {&oldView, &newView}) {
        clearDecorations(*view);
        configure(*view);
    }

    const std::vector<std::string_view> oldLines = splitLines(documentText(oldView));
    const std::vector<std::string_view> newLines = splitLines(documentText(newView));
    // Con espacios o mayúsculas ignorados se compara una copia normalizada de cada línea
    std::vector<std::string> oldNormalized;
    std::vector<std::string> newNormalized;
    std::vector<std::string_view> oldKeys = oldLines;
    std::vector<std::string_view> newKeys = newLines;
    if (_options.ignoreSpaces || _options.ignoreCase) {
        for (std::string_view line : oldLines)
            oldNormalized.push_back(normalized(line, _options));
        for (std::string_view line : newLines)
            newNormalized.push_back(normalized(line, _options));
        oldKeys.assign(oldNormalized.begin(), oldNormalized.end());
        newKeys.assign(newNormalized.begin(), newNormalized.end());
    }
    _changes = diffLines(oldKeys, newKeys, kMaxLineEdits);

    std::map<int, int> oldPadding;
    std::map<int, int> newPadding;
    auto markChars = [&](int oldLine, int newLine) {
        // Los caracteres distintos de un par de líneas cambiadas
        const std::string_view a = oldLines[static_cast<std::size_t>(oldLine)];
        const std::string_view b = newLines[static_cast<std::size_t>(newLine)];
        const auto aChars = characters(a);
        const auto bChars = characters(b);
        bool complete = true;
        const auto diff = diffLines(aChars, bChars, kMaxCharEdits, &complete);
        if (!complete)
            return;
        auto byteOffset = [](const std::vector<std::string_view>& chars, std::string_view line, int index) {
            if (index >= static_cast<int>(chars.size()))
                return static_cast<sptr_t>(line.size());
            return static_cast<sptr_t>(chars[static_cast<std::size_t>(index)].data() - line.data());
        };
        const sptr_t oldStart = oldView.send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(oldLine));
        const sptr_t newStart = newView.send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(newLine));
        for (const LineChange& c : diff) {
            if (c.oldCount > 0) {
                const sptr_t from = byteOffset(aChars, a, c.oldLine);
                const sptr_t to = byteOffset(aChars, a, c.oldLine + c.oldCount);
                oldView.send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(oldStart + from), to - from);
            }
            if (c.count > 0) {
                const sptr_t from = byteOffset(bChars, b, c.line);
                const sptr_t to = byteOffset(bChars, b, c.line + c.count);
                newView.send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(newStart + from), to - from);
            }
        }
    };
    oldView.send(SCI_SETINDICATORCURRENT, kIndicatorChars);
    newView.send(SCI_SETINDICATORCURRENT, kIndicatorChars);
    for (const LineChange& c : _changes) {
        switch (c.kind) {
        case LineChangeKind::Added:
            markLines(newView, kMarkerAdded, c.line, c.count);
            oldPadding[c.oldLine - 1] += c.count; // (al principio, tras la primera línea)
            break;
        case LineChangeKind::Deleted:
            markLines(oldView, kMarkerRemoved, c.oldLine, c.oldCount);
            newPadding[c.line - 1] += c.oldCount;
            break;
        case LineChangeKind::Modified:
            markLines(oldView, kMarkerChanged, c.oldLine, c.oldCount);
            markLines(newView, kMarkerChanged, c.line, c.count);
            for (int i = 0; i < std::min(c.count, c.oldCount); ++i)
                markChars(c.oldLine + i, c.line + i);
            if (c.count > c.oldCount)
                oldPadding[c.oldLine + c.oldCount - 1] += c.count - c.oldCount;
            else if (c.oldCount > c.count)
                newPadding[c.line + c.count - 1] += c.oldCount - c.count;
            break;
        }
    }
    addPadding(oldView, oldPadding);
    addPadding(newView, newPadding);
    oldView.send(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(oldFirst));
    newView.send(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(newFirst));
    syncScroll(&newView);
    emit updated();
}

CompareSession::Stats CompareSession::stats() const
{
    Stats s;
    for (const LineChange& c : _changes) {
        ++s.blocks;
        if (c.kind == LineChangeKind::Added) {
            s.added += c.count;
        } else if (c.kind == LineChangeKind::Deleted) {
            s.removed += c.oldCount;
        } else {
            s.changed += std::min(c.count, c.oldCount);
            s.added += std::max(0, c.count - c.oldCount);
            s.removed += std::max(0, c.oldCount - c.count);
        }
    }
    return s;
}

void CompareSession::syncScroll(ScintillaEditBase* from)
{
    if (_syncing || !_old || !_new)
        return;
    ScintillaEditBase* to = from == _old ? _new.data() : _old.data();
    // Las dos tienen las mismas líneas a la vista (las de relleno cuentan): la misma primera
    _syncing = true;
    to->send(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(from->send(SCI_GETFIRSTVISIBLELINE)));
    to->send(SCI_SETXOFFSET, static_cast<uptr_t>(from->send(SCI_GETXOFFSET)));
    _syncing = false;
}

int CompareSession::goToDifference(ScintillaEditBase* view, bool forward)
{
    if (_changes.empty() || !_old || !_new)
        return 0;
    const bool isOld = view == _old;
    auto startIn = [](const LineChange& c, bool old) { return old ? c.oldLine : c.line; };
    const auto caretLine = static_cast<int>(view->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(view->send(SCI_GETCURRENTPOS))));
    const int n = static_cast<int>(_changes.size());
    int index = -1;
    if (forward) {
        for (int i = 0; i < n && index < 0; ++i)
            index = startIn(_changes[static_cast<std::size_t>(i)], isOld) > caretLine ? i : -1;
        if (index < 0)
            index = 0; // vuelta al principio
    } else {
        for (int i = n - 1; i >= 0 && index < 0; --i)
            index = startIn(_changes[static_cast<std::size_t>(i)], isOld) < caretLine ? i : -1;
        if (index < 0)
            index = n - 1;
    }
    const LineChange& c = _changes[static_cast<std::size_t>(index)];
    // El cursor en el bloque en las dos vistas; la línea, a un tercio de la altura
    for (ScintillaEditBase* v : {_old.data(), _new.data()}) {
        const int line = startIn(c, v == _old);
        v->send(SCI_GOTOLINE, static_cast<uptr_t>(std::min<sptr_t>(line, v->send(SCI_GETLINECOUNT) - 1)));
    }
    const sptr_t display = view->send(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(startIn(c, isOld)));
    view->send(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(std::max<sptr_t>(0, display - view->send(SCI_LINESONSCREEN) / 3)));
    syncScroll(view);
    return index + 1;
}

} // namespace edita
