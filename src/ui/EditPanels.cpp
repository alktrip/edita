// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team): la tabla de las
// entidades HTML es la de AsciiListView (WinControls/AnsiCharPanel/asciiListView.cpp).

#include "ui/EditPanels.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QListWidget>
#include <QMimeData>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace edita {

namespace {

// AsciiListView::getHtmlName
const char* const kHtmlNames[256] = {
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 0
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 8
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 16
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 24
    nullptr, "&excl;", "&quot;", "&num;", "&dollar;", "&percnt;", "&amp;", "&apos;",  // 32
    "&lpar;", "&rpar;", "&ast;", "&plus;", "&comma;", "&minus;", "&period;", "&sol;",  // 40
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 48
    nullptr, nullptr, "&colon;", "&semi;", "&lt;", "&equals;", "&gt;", "&quest;",  // 56
    "&commat;", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 64
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 72
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 80
    nullptr, nullptr, nullptr, "&lbrack;", "&bsol;", "&rbrack;", "&Hat;", "&lowbar;",  // 88
    "&grave;", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 96
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 104
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 112
    nullptr, nullptr, nullptr, "&lbrace;", "&vert;", "&rbrace;", nullptr, nullptr,  // 120
    "&euro;", nullptr, "&sbquo;", "&fnof;", "&bdquo;", "&hellip;", "&dagger;", "&Dagger;",  // 128
    "&circ;", "&permil;", "&Scaron;", "&lsaquo;", "&OElig;", nullptr, "&Zcaron;", nullptr,  // 136
    nullptr, "&lsquo;", "&rsquo;", "&ldquo;", "&rdquo;", "&bull;", "&ndash;", "&mdash;",  // 144
    "&tilde;", "&trade;", "&scaron;", "&rsaquo;", "&oelig;", nullptr, "&zcaron;", "&Yuml;",  // 152
    "&nbsp;", "&iexcl;", "&cent;", "&pound;", "&curren;", "&yen;", "&brvbar;", "&sect;",  // 160
    "&uml;", "&copy;", "&ordf;", "&laquo;", "&not;", "&shy;", "&reg;", "&macr;",  // 168
    "&deg;", "&plusmn;", "&sup2;", "&sup3;", "&acute;", "&micro;", "&para;", "&middot;",  // 176
    "&cedil;", "&sup1;", "&ordm;", "&raquo;", "&frac14;", "&frac12;", "&frac34;", "&iquest;",  // 184
    "&Agrave;", "&Aacute;", "&Acirc;", "&Atilde;", "&Auml;", "&Aring;", "&AElig;", "&Ccedil;",  // 192
    "&Egrave;", "&Eacute;", "&Ecirc;", "&Euml;", "&Igrave;", "&Iacute;", "&Icirc;", "&Iuml;",  // 200
    "&ETH;", "&Ntilde;", "&Ograve;", "&Oacute;", "&Ocirc;", "&Otilde;", "&Ouml;", "&times;",  // 208
    "&Oslash;", "&Ugrave;", "&Uacute;", "&Ucirc;", "&Uuml;", "&Yacute;", "&THORN;", "&szlig;",  // 216
    "&agrave;", "&aacute;", "&acirc;", "&atilde;", "&auml;", "&aring;", "&aelig;", "&ccedil;",  // 224
    "&egrave;", "&eacute;", "&ecirc;", "&euml;", "&igrave;", "&iacute;", "&icirc;", "&iuml;",  // 232
    "&eth;", "&ntilde;", "&ograve;", "&oacute;", "&ocirc;", "&otilde;", "&ouml;", "&divide;",  // 240
    "&oslash;", "&ugrave;", "&uacute;", "&ucirc;", "&uuml;", "&yacute;", "&thorn;", "&yuml;",  // 248
};

// AsciiListView::getAscii: los nombres de los caracteres de control
const char* const kControlNames[33] = {
    "NULL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL", "BS",  "TAB", "LF",  "VT",  "FF", "CR", "SO", "SI",  "DLE",
    "DC1",  "DC2", "DC3", "DC4", "NAK", "SYN", "ETB", "CAN", "EM",  "SUB", "ESC", "FS",  "GS", "RS", "US", "Space",
};

// Windows-1252 entre 0x80 y 0x9F (los huecos, 0: el carácter de control del mismo número,
// como MultiByteToWideChar)
const char16_t kCp1252High[32] = {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178,
};

} // namespace

// --- Panel de caracteres ---------------------------------------------------------------------

CharacterPanel::CharacterPanel(QWidget* parent)
    : QWidget(parent)
    , _list(new QTreeWidget(this))
{
    _list->setObjectName(QStringLiteral("characterList"));
    _list->setRootIsDecorated(false);
    _list->setUniformRowHeights(true);
    _list->setAllColumnsShowFocus(true);
    _list->setHeaderLabels({tr("Value"), tr("Hex"), tr("Character"), tr("HTML Name"), tr("HTML Decimal"), tr("HTML Hexadecimal")});
    QList<QTreeWidgetItem*> items;
    items.reserve(256);
    for (int i = 0; i < 256; ++i) {
        auto* item = new QTreeWidgetItem;
        for (int column = 0; column < ColumnCount; ++column)
            item->setText(column, column == Character ? characterLabel(i) : textFor(i, column));
        item->setData(0, Qt::UserRole, i);
        items << item;
    }
    _list->addTopLevelItems(items);
    _list->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    _list->setCurrentItem(_list->topLevelItem(0));
    _list->installEventFilter(this);
    connect(_list, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int column) {
        const QString text = textFor(item->data(0, Qt::UserRole).toInt(), column);
        if (!text.isEmpty())
            emit insertText(text);
    });
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_list);
}

QChar CharacterPanel::character(int value)
{
    if (value >= 0x80 && value <= 0x9F && kCp1252High[value - 0x80])
        return QChar(kCp1252High[value - 0x80]);
    return QChar(static_cast<char16_t>(value));
}

QString CharacterPanel::characterLabel(int value)
{
    if (value >= 0 && value <= 32)
        return QString::fromLatin1(kControlNames[value]);
    if (value == 127)
        return QStringLiteral("DEL");
    if (value >= 0x80 && value <= 0x9F && !kCp1252High[value - 0x80])
        return {};
    return QString(character(value));
}

QString CharacterPanel::htmlName(int value)
{
    return value >= 0 && value < 256 && kHtmlNames[value] ? QString::fromLatin1(kHtmlNames[value]) : QString();
}

int CharacterPanel::htmlNumber(int value)
{
    // AsciiListView::setValues: los imprimibles, su número; el guion, el del signo menos
    // (como N++), y entre 0x80 y 0x9F, el del carácter de Windows-1252
    if (value == 45)
        return 8722;
    if ((value >= 32 && value <= 126) || (value >= 160 && value <= 255))
        return value;
    if (value >= 0x80 && value <= 0x9F && kCp1252High[value - 0x80])
        return kCp1252High[value - 0x80];
    return -1;
}

QString CharacterPanel::textFor(int value, int column)
{
    switch (column) {
    case Value:
        return QString::number(value);
    case Hex:
        return QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
    case Character:
        return QString(character(value));
    case HtmlName:
        return htmlName(value);
    case HtmlDecimal:
        return htmlNumber(value) < 0 ? QString() : QStringLiteral("&#%1;").arg(htmlNumber(value));
    case HtmlHex:
        return htmlNumber(value) < 0 ? QString() : QStringLiteral("&#x%1;").arg(htmlNumber(value), 0, 16);
    default:
        return {};
    }
}

bool CharacterPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Intro: el carácter de la fila elegida
    if (watched == _list && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if ((key == Qt::Key_Return || key == Qt::Key_Enter) && _list->currentItem()) {
            emit insertText(QString(character(_list->currentItem()->data(0, Qt::UserRole).toInt())));
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// --- Historial del portapapeles --------------------------------------------------------------

ClipboardHistoryPanel::ClipboardHistoryPanel(QWidget* parent)
    : QWidget(parent)
    , _list(new QListWidget(this))
{
    _list->setObjectName(QStringLiteral("clipboardHistoryList"));
    _list->setUniformItemSizes(true);
    _list->installEventFilter(this);
    connect(_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        const int row = _list->row(item);
        if (row >= 0 && row < _entries.size())
            emit insertText(_entries.at(row));
    });
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_list);
}

void ClipboardHistoryPanel::startTracking()
{
    if (_tracking)
        return;
    _tracking = true;
    QClipboard* clipboard = QApplication::clipboard();
    auto take = [this, clipboard] {
        const QMimeData* mime = clipboard->mimeData(QClipboard::Clipboard);
        if (mime && mime->hasText())
            addEntry(mime->text());
    };
    connect(clipboard, &QClipboard::dataChanged, this, take);
    take(); // lo que ya había
}

void ClipboardHistoryPanel::addEntry(const QString& text)
{
    if (text.isEmpty() || (!_entries.isEmpty() && _entries.front() == text))
        return;
    _entries.removeAll(text);
    _entries.prepend(text);
    while (_entries.size() > kMaxEntries)
        _entries.removeLast();
    rebuild();
}

void ClipboardHistoryPanel::clear()
{
    _entries.clear();
    rebuild();
}

QString ClipboardHistoryPanel::displayText(const QString& text)
{
    // En una línea: los fines de línea y los tabuladores, como espacios
    QString shown = text.left(kDisplayLength);
    for (QChar& c : shown) {
        if (c == QLatin1Char('\r') || c == QLatin1Char('\n') || c == QLatin1Char('\t'))
            c = QLatin1Char(' ');
    }
    if (text.size() > kDisplayLength)
        shown += QStringLiteral("…");
    return shown;
}

void ClipboardHistoryPanel::rebuild()
{
    _list->clear();
    for (const QString& text : _entries) {
        auto* item = new QListWidgetItem(displayText(text), _list);
        item->setToolTip(text.left(1000));
    }
}

bool ClipboardHistoryPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _list && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        const int row = _list->currentRow();
        if ((key == Qt::Key_Return || key == Qt::Key_Enter) && row >= 0 && row < _entries.size()) {
            emit insertText(_entries.at(row));
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace edita
