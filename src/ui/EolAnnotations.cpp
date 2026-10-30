// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/EolAnnotations.h"

#include <ScintillaEditBase.h>

#include <QColor>

namespace edita {

namespace {

QColor fromScintilla(sptr_t c)
{
    return QColor(static_cast<int>(c & 0xFF), static_cast<int>((c >> 8) & 0xFF), static_cast<int>((c >> 16) & 0xFF));
}

sptr_t toScintilla(const QColor& c)
{
    return c.red() | (c.green() << 8) | (c.blue() << 16);
}

} // namespace

EolAnnotations::EolAnnotations(ScintillaEditBase* view)
    : QObject(view)
    , _view(view)
{
    setObjectName(QStringLiteral("eolAnnotations"));
    // Las anotaciones se mueven con el texto: sus líneas, también
    connect(view, &ScintillaEditBase::modified, this,
            [this](Scintilla::ModificationFlags, Scintilla::Position position, Scintilla::Position, Scintilla::Position linesAdded) {
                if (linesAdded == 0 || !_view || _lines.isEmpty())
                    return;
                const auto line = static_cast<int>(_view->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(position)));
                QMap<int, QMap<int, QString>> moved;
                for (auto it = _lines.begin(); it != _lines.end(); ++it) {
                    const int key = it.key() > line ? it.key() + static_cast<int>(linesAdded) : it.key();
                    if (key >= 0 && (it.key() <= line || key > line))
                        moved.insert(key, it.value());
                }
                _lines = std::move(moved);
            });
}

EolAnnotations* EolAnnotations::of(ScintillaEditBase* view)
{
    if (auto* existing = view->findChild<EolAnnotations*>(QStringLiteral("eolAnnotations"), Qt::FindDirectChildrenOnly))
        return existing;
    return new EolAnnotations(view);
}

void EolAnnotations::configureStyle()
{
    // Un estilo propio (extendido): el color del texto, a medio camino del fondo, en cursiva
    if (_view->send(SCI_EOLANNOTATIONGETSTYLEOFFSET) == 0)
        _view->send(SCI_EOLANNOTATIONSETSTYLEOFFSET, static_cast<uptr_t>(_view->send(SCI_ALLOCATEEXTENDEDSTYLES, 1)));
    const auto style = static_cast<uptr_t>(_view->send(SCI_EOLANNOTATIONGETSTYLEOFFSET));
    const QColor fore = fromScintilla(_view->send(SCI_STYLEGETFORE, STYLE_DEFAULT));
    const QColor back = fromScintilla(_view->send(SCI_STYLEGETBACK, STYLE_DEFAULT));
    const QColor mixed = QColor::fromRgbF(static_cast<float>((fore.redF() + back.redF() * 1.2) / 2.2),
                                          static_cast<float>((fore.greenF() + back.greenF() * 1.2) / 2.2),
                                          static_cast<float>((fore.blueF() + back.blueF() * 1.2) / 2.2));
    _view->send(SCI_STYLESETFORE, style, toScintilla(mixed));
    _view->send(SCI_STYLESETBACK, style, toScintilla(back));
    _view->send(SCI_STYLESETITALIC, style, 1);
    _view->send(SCI_STYLESETSIZEFRACTIONAL, style, _view->send(SCI_STYLEGETSIZEFRACTIONAL, STYLE_DEFAULT));
    _view->send(SCI_EOLANNOTATIONSETVISIBLE, EOLANNOTATION_STANDARD);
}

void EolAnnotations::set(Owner owner, int line, const QString& text)
{
    if (line < 0)
        return;
    QMap<int, QString>& parts = _lines[line];
    if (text.isEmpty())
        parts.remove(owner);
    else
        parts.insert(owner, text);
    if (parts.isEmpty())
        _lines.remove(line);
    apply(line);
}

void EolAnnotations::clear(Owner owner)
{
    QList<int> touched;
    for (auto it = _lines.begin(); it != _lines.end();) {
        if (it->remove(owner) > 0)
            touched << it.key();
        it = it->isEmpty() ? _lines.erase(it) : std::next(it);
    }
    for (const int line : touched)
        apply(line);
}

void EolAnnotations::reset()
{
    _lines.clear();
    if (_view)
        _view->send(SCI_EOLANNOTATIONCLEARALL);
}

QString EolAnnotations::textAt(int line) const
{
    const auto parts = _lines.value(line);
    QStringList texts;
    for (const QString& t : parts)
        texts << t;
    return texts.join(QStringLiteral("    "));
}

void EolAnnotations::apply(int line)
{
    if (!_view || line >= _view->send(SCI_GETLINECOUNT))
        return;
    const QString text = textAt(line);
    if (text.isEmpty()) {
        _view->send(SCI_EOLANNOTATIONSETTEXT, static_cast<uptr_t>(line), reinterpret_cast<sptr_t>(""));
        return;
    }
    configureStyle();
    const QByteArray utf8 = (QStringLiteral("    ") + text).toUtf8();
    _view->send(SCI_EOLANNOTATIONSETTEXT, static_cast<uptr_t>(line), reinterpret_cast<sptr_t>(utf8.constData()));
    _view->send(SCI_EOLANNOTATIONSETSTYLE, static_cast<uptr_t>(line), 0);
}

} // namespace edita
