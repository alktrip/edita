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

#include "ui/DocStatusBar.h"

#include "doc/Buffer.h"
#include "ui/EditorView.h"

#include <QCoreApplication>
#include <QLabel>
#include <QLocale>
#include <QToolButton>

#include <algorithm>

namespace edita {

namespace {

// commafyInt de N++: número con separador de miles del idioma
QString grouped(sptr_t value)
{
    return QLocale().toString(static_cast<qlonglong>(value));
}

QToolButton* makeMenuButton(QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setAutoRaise(true);
    button->setPopupMode(QToolButton::InstantPopup);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    return button;
}

} // namespace

DocStatusBar::DocStatusBar(QWidget* parent)
    : QStatusBar(parent)
    , _language(new QLabel(this))
    , _length(new QLabel(this))
    , _position(new QLabel(this))
    , _eol(makeMenuButton(this))
    , _encoding(makeMenuButton(this))
    , _insertMode(new QLabel(this))
{
    setSizeGripEnabled(true);
    addWidget(_language, 1);
    addPermanentWidget(_length);
    addPermanentWidget(_position);
    addPermanentWidget(_eol);
    addPermanentWidget(_encoding);
    addPermanentWidget(_insertMode);
    _insertMode->setMinimumWidth(_insertMode->fontMetrics().horizontalAdvance(QStringLiteral("SOB ")));
    _eol->setToolTip(tr("End of line (click to convert)"));
    _encoding->setToolTip(tr("Encoding (click to change it)"));
}

void DocStatusBar::setEolMenu(QMenu* menu)
{
    _eol->setMenu(menu);
}

void DocStatusBar::setEncodingMenu(QMenu* menu)
{
    _encoding->setMenu(menu);
}

void DocStatusBar::updateDocument(const Buffer* buffer)
{
    if (!buffer)
        return;
    // Como N++: la descripción larga del lenguaje ("C++ source file")
    if (buffer->language() == LangType::User && !buffer->userLanguage().isEmpty())
        _language->setText(tr("User Defined language - %1").arg(buffer->userLanguage()));
    else
        _language->setText(QCoreApplication::translate("LangType", languageInfo(buffer->language()).longName));
    _eol->setText(QString::fromLatin1(eolDisplayName(buffer->eol())));
    _encoding->setText(QString::fromStdString(encodingDisplayName(buffer->encoding())));
}

void DocStatusBar::updateCaret(const EditorView* editor)
{
    const sptr_t length = editor->sci(SCI_GETLENGTH);
    _length->setText(tr("length: %1    lines: %2").arg(grouped(length), grouped(editor->sci(SCI_GETLINECOUNT))));

    const sptr_t pos = editor->sci(SCI_GETCURRENTPOS);
    const sptr_t line = editor->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos));
    const sptr_t column = editor->sci(SCI_GETCOLUMN, static_cast<uptr_t>(pos));
    QString text = tr("Ln: %1    Col: %2    ").arg(grouped(line + 1), grouped(column + 1));

    // Notepad_plus::updateStatusBar: "Pos" sin selección, "Sel: caracteres | líneas" con ella
    const sptr_t selections = editor->sci(SCI_GETSELECTIONS);
    sptr_t chars = 0;
    sptr_t firstLine = -1;
    sptr_t lastLine = -1;
    for (sptr_t i = 0; i < selections; ++i) {
        const sptr_t start = editor->sci(SCI_GETSELECTIONNSTART, static_cast<uptr_t>(i));
        const sptr_t end = editor->sci(SCI_GETSELECTIONNEND, static_cast<uptr_t>(i));
        chars += editor->sci(SCI_COUNTCHARACTERS, static_cast<uptr_t>(start), end);
        const sptr_t l1 = editor->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(start));
        const sptr_t l2 = editor->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(end));
        firstLine = firstLine < 0 ? l1 : std::min(firstLine, l1);
        lastLine = std::max(lastLine, l2);
    }
    if (selections == 1 && chars == 0)
        text += tr("Pos: %1").arg(grouped(pos + 1));
    else if (selections == 1)
        text += tr("Sel: %1 | %2").arg(grouped(chars), grouped(lastLine - firstLine + 1));
    else if (editor->sci(SCI_SELECTIONISRECTANGLE))
        text += tr("Sel: %1 | %2").arg(grouped(chars), grouped(selections));
    else
        text += tr("Sel %1 : %2").arg(grouped(selections), grouped(chars));
    _position->setText(text);

    _insertMode->setText(editor->sci(SCI_GETOVERTYPE) ? tr("OVR") : tr("INS"));
}

void DocStatusBar::setFieldText(int field, const QString& text)
{
    switch (field) {
    case 0: _language->setText(text); break;
    case 1: _length->setText(text); break;
    case 2: _position->setText(text); break;
    case 3: _eol->setText(text); break;
    case 4: _encoding->setText(text); break;
    case 5: _insertMode->setText(text); break;
    default: break;
    }
}

QString DocStatusBar::languageText() const { return _language->text(); }
QString DocStatusBar::lengthText() const { return _length->text(); }
QString DocStatusBar::positionText() const { return _position->text(); }
QString DocStatusBar::eolText() const { return _eol->text(); }
QString DocStatusBar::encodingText() const { return _encoding->text(); }
QString DocStatusBar::insertModeText() const { return _insertMode->text(); }

} // namespace edita
