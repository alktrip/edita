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
#include <QHBoxLayout>
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

// Un campo que se puede pulsar: plano, como una etiqueta, resaltado al pasar el ratón
QToolButton* makeFieldButton(QWidget* parent, const char* name)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QString::fromLatin1(name));
    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    // Sin ancho mínimo por su texto: con la ventana estrecha, el campo se recorta en vez de
    // ensanchar la ventana (fase 101)
    button->setMinimumWidth(16);
    return button;
}

// Un texto en un campo, en una línea y de 80 caracteres como mucho: la barra pide el
// ancho de sus campos y la ventana, el de la barra (con un texto de un complemento o el
// nombre de un lenguaje de miles de caracteres, en Wayland la ventana salía negra, fase
// 101). El texto completo queda en una propiedad, para los getters
template <typename Field>
void setFieldButtonText(Field* button, const QString& text)
{
    constexpr int kMaxField = 80;
    const QString line = text.size() > kMaxField || text.contains(QLatin1Char('\n')) ? text.simplified() : text;
    button->setText(line.size() > kMaxField ? line.left(kMaxField) + QChar(0x2026) : line);
    button->setProperty("fullText", text);
}

QString fieldButtonText(const QWidget* button)
{
    return button->property("fullText").toString();
}

} // namespace

DocStatusBar::DocStatusBar(QWidget* parent)
    : QStatusBar(parent)
    , _language(makeFieldButton(this, "statusLanguage"))
    , _length(new QLabel(this))
    , _position(makeFieldButton(this, "statusPosition"))
    , _indentation(makeFieldButton(this, "statusIndentation"))
    , _eol(makeFieldButton(this, "statusEol"))
    , _encoding(makeFieldButton(this, "statusEncoding"))
    , _insertMode(makeFieldButton(this, "statusInsertMode"))
    , _problems(makeFieldButton(this, "statusProblems"))
{
    setSizeGripEnabled(true);
    // El lenguaje, a la izquierda como en N++ (el botón no se estira: el resto, vacío)
    auto* left = new QWidget(this);
    auto* leftLayout = new QHBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(_language);
    leftLayout->addWidget(_problems);
    leftLayout->addStretch(1);
    _problems->hide();
    addWidget(left, 1);
    // Un mensaje temporal se dibuja en el mismo sitio: mientras dura, el campo se oculta
    connect(this, &QStatusBar::messageChanged, left, [left](const QString& message) { left->setVisible(message.isEmpty()); });
    addPermanentWidget(_length);
    addPermanentWidget(_position);
    addPermanentWidget(_indentation);
    addPermanentWidget(_eol);
    addPermanentWidget(_encoding);
    addPermanentWidget(_insertMode);
    _length->setMinimumWidth(16);
    _insertMode->setMinimumWidth(_insertMode->fontMetrics().horizontalAdvance(QStringLiteral("SOB ")) + 12);
    _language->setToolTip(tr("Language (click to change it)"));
    _position->setToolTip(tr("Click to go to a line"));
    _indentation->setToolTip(tr("Indentation (click to change it)"));
    _eol->setToolTip(tr("End of line (click to convert)"));
    _encoding->setToolTip(tr("Encoding (click to change it)"));
    _insertMode->setToolTip(tr("Insert or overwrite (click to switch)"));
    const std::pair<QToolButton*, Field> fields[] = {{_language, Field::Language}, {_position, Field::Position},
                                                     {_indentation, Field::Indentation}, {_eol, Field::Eol},
                                                     {_encoding, Field::Encoding}, {_insertMode, Field::InsertMode},
                                                     {_problems, Field::Problems}};
    for (const auto& [button, which] : fields) {
        const Field f = which;
        connect(button, &QToolButton::clicked, this, [this, f] { emit fieldClicked(f); });
    }
}

QWidget* DocStatusBar::field(Field which) const
{
    switch (which) {
    case Field::Language: return _language;
    case Field::Position: return _position;
    case Field::Indentation: return _indentation;
    case Field::Eol: return _eol;
    case Field::Encoding: return _encoding;
    case Field::InsertMode: return _insertMode;
    case Field::Problems: return _problems;
    }
    return nullptr;
}

void DocStatusBar::setProblems(bool visible, int errors, int warnings, const QString& toolTip)
{
    _problems->setVisible(visible);
    _problems->setText(QStringLiteral("⨯ %1   ⚠ %2").arg(errors).arg(warnings));
    _problems->setToolTip(toolTip);
}

QString DocStatusBar::problemsText() const
{
    return _problems->isVisible() ? _problems->text() : QString();
}

void DocStatusBar::updateIndentation(bool useTabs, int width)
{
    _indentation->setText(useTabs ? tr("Tabs: %1").arg(width) : tr("Spaces: %1").arg(width));
}

QString DocStatusBar::indentationText() const
{
    return _indentation->text();
}

void DocStatusBar::updateDocument(const Buffer* buffer)
{
    if (!buffer)
        return;
    // Como N++: la descripción larga del lenguaje ("C++ source file")
    if (buffer->language() == LangType::User && !buffer->userLanguage().isEmpty())
        setFieldButtonText(_language, tr("User Defined language - %1").arg(buffer->userLanguage()));
    else if (buffer->language() == LangType::External && !buffer->userLanguage().isEmpty())
        setFieldButtonText(_language, buffer->userLanguage()); // getLangDesc: el nombre del lexer
    else
        setFieldButtonText(_language, QCoreApplication::translate("LangType", languageInfo(buffer->language()).longName));
    setFieldButtonText(_eol, QString::fromLatin1(eolDisplayName(buffer->eol())));
    setFieldButtonText(_encoding, QString::fromStdString(encodingDisplayName(buffer->encoding())));
}

void DocStatusBar::updateCaret(const EditorView* editor)
{
    const sptr_t length = editor->sci(SCI_GETLENGTH);
    setFieldButtonText(_length, tr("length: %1    lines: %2").arg(grouped(length), grouped(editor->sci(SCI_GETLINECOUNT))));

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
    setFieldButtonText(_position, text);

    setFieldButtonText(_insertMode, editor->sci(SCI_GETOVERTYPE) ? tr("OVR") : tr("INS"));
    const sptr_t indent = editor->sci(SCI_GETINDENT);
    updateIndentation(editor->sci(SCI_GETUSETABS) != 0, static_cast<int>(indent > 0 ? indent : editor->sci(SCI_GETTABWIDTH)));
}

void DocStatusBar::setFieldText(int field, const QString& text)
{
    switch (field) {
    case 0: setFieldButtonText(_language, text); break;
    case 1: setFieldButtonText(_length, text); break;
    case 2: setFieldButtonText(_position, text); break;
    case 3: setFieldButtonText(_eol, text); break;
    case 4: setFieldButtonText(_encoding, text); break;
    case 5: setFieldButtonText(_insertMode, text); break;
    default: break;
    }
}

QString DocStatusBar::languageText() const { return fieldButtonText(_language); }
QString DocStatusBar::lengthText() const { return fieldButtonText(_length); }
QString DocStatusBar::positionText() const { return fieldButtonText(_position); }
QString DocStatusBar::eolText() const { return fieldButtonText(_eol); }
QString DocStatusBar::encodingText() const { return fieldButtonText(_encoding); }
QString DocStatusBar::insertModeText() const { return fieldButtonText(_insertMode); }

} // namespace edita
