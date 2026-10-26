// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/Breadcrumbs.h"

#include "ui/FileIcons.h"
#include "ui/Icons.h"
#include "ui/QuickPicker.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QToolButton>

namespace edita {

namespace {

constexpr int kMaxFolders = 3;   // carpetas que se ven (las de más arriba, «…»)
constexpr int kMaxFiles = 300;   // archivos en la lista de una carpeta

QToolButton* segment(QWidget* parent, const QString& text, const QIcon& icon = QIcon())
{
    auto* button = new QToolButton(parent);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setText(text);
    button->setProperty("breadcrumb", true);
    if (!icon.isNull()) {
        button->setIcon(icon);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    } else {
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }
    return button;
}

} // namespace

Breadcrumbs::Breadcrumbs(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("breadcrumbs"));
    _layout = new QHBoxLayout(this);
    _layout->setContentsMargins(6, 1, 6, 1);
    _layout->setSpacing(0);
    const int icon = fontMetrics().height();
    setStyleSheet(QStringLiteral("QToolButton[breadcrumb=\"true\"] { padding: 1px 3px; border: none; } "
                                 "QToolButton[breadcrumb=\"true\"]:hover { text-decoration: underline; }"));
    setMaximumHeight(icon + 8);
    rebuild();
}

void Breadcrumbs::setFile(const QString& path, const QString& displayName, const QStringList& roots)
{
    _path = path;
    _displayName = displayName;
    _roots = roots;
    rebuild();
}

void Breadcrumbs::setSymbols(std::vector<Symbol> symbols, bool fallback)
{
    _symbols = std::move(symbols);
    _fallback = fallback;
    setCaretLine(_caretLine);
    rebuild();
}

void Breadcrumbs::setCaretLine(int line)
{
    _caretLine = line;
    std::vector<int> trail;
    for (int i = 0; i < static_cast<int>(_symbols.size()); ++i) {
        const Symbol& s = _symbols[static_cast<size_t>(i)];
        if (s.line <= line && line <= s.endLine)
            trail.push_back(i);
    }
    // De fuera hacia dentro: los que empiezan antes (y, a igualdad, los más largos) primero
    std::stable_sort(trail.begin(), trail.end(), [this](int a, int b) {
        const Symbol& x = _symbols[static_cast<size_t>(a)];
        const Symbol& y = _symbols[static_cast<size_t>(b)];
        return x.line != y.line ? x.line < y.line : x.endLine > y.endLine;
    });
    if (trail != _trail) {
        _trail = std::move(trail);
        rebuild();
    }
}

void Breadcrumbs::rebuild()
{
    while (QLayoutItem* item = _layout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater(); // puede ser el botón que se acaba de pulsar
        }
        delete item;
    }
    const int iconSize = fontMetrics().height() - 2;
    auto separator = [this] {
        auto* label = new QLabel(QStringLiteral("›"), this);
        label->setEnabled(false);
        label->setContentsMargins(2, 0, 2, 0);
        _layout->addWidget(label);
    };
    // Las carpetas: desde la carpeta de trabajo que contiene el archivo, o las últimas
    if (!_path.isEmpty()) {
        const QString dir = QFileInfo(_path).absolutePath();
        QString root;
        for (const QString& r : _roots) {
            if ((dir == r || dir.startsWith(r + QLatin1Char('/'))) && r.size() > root.size())
                root = r;
        }
        QStringList folders;
        QStringList paths;
        QString current = dir;
        while (!current.isEmpty()) {
            folders.prepend(QFileInfo(current).fileName().isEmpty() ? current : QFileInfo(current).fileName());
            paths.prepend(current);
            if (current == root || QFileInfo(current).isRoot() || current == QDir::homePath())
                break;
            const QString parent = QFileInfo(current).absolutePath();
            if (parent == current)
                break;
            current = parent;
        }
        if (root.isEmpty() && folders.size() > kMaxFolders) {
            folders = folders.mid(folders.size() - kMaxFolders);
            paths = paths.mid(paths.size() - kMaxFolders);
            auto* more = new QLabel(QStringLiteral("…"), this);
            more->setEnabled(false);
            _layout->addWidget(more);
            separator();
        }
        for (int i = 0; i < folders.size(); ++i) {
            const QString folder = paths[i];
            auto* button = segment(this, folders[i]);
            button->setToolTip(folder);
            connect(button, &QToolButton::clicked, this, [this, folder, button] { showFolder(folder, button); });
            _layout->addWidget(button);
            separator();
        }
    }
    // El archivo
    auto* file = segment(this, _path.isEmpty() ? _displayName : QFileInfo(_path).fileName(),
                         fileicons::enabled() ? fileicons::forFile(_path) : QIcon());
    file->setIconSize(QSize(iconSize, iconSize));
    connect(file, &QToolButton::clicked, this, [this, file] { showSymbols(file); });
    _layout->addWidget(file);
    // Los símbolos que contienen al cursor
    for (int index : _trail) {
        separator();
        const Symbol& s = _symbols[static_cast<size_t>(index)];
        auto* button = segment(this, s.name, _fallback ? QIcon() : icons::icon(QStringLiteral("function-list")));
        button->setIconSize(QSize(iconSize, iconSize));
        connect(button, &QToolButton::clicked, this, [this, button] { showSymbols(button); });
        _layout->addWidget(button);
    }
    _layout->addStretch(1);
}

void Breadcrumbs::showFolder(const QString& folder, QWidget* anchor)
{
    std::vector<QuickPicker::Item> items;
    const QFileInfoList entries =
        QDir(folder).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& entry : entries) {
        if (static_cast<int>(items.size()) >= kMaxFiles)
            break;
        const QString path = entry.absoluteFilePath();
        if (entry.isDir()) {
            // Una subcarpeta: su lista (encima del mismo segmento)
            QPointer<QWidget> at(anchor); // el segmento puede haberse rehecho entretanto
            items.push_back({entry.fileName() + QLatin1Char('/'), tr("folder"), false,
                             [this, path, at] { showFolder(path, at ? at.data() : this); }});
        } else {
            items.push_back({entry.fileName(), QString(), path == _path, [this, path] { emit openFileRequested(path); }});
        }
    }
    if (!_picker)
        _picker = new QuickPicker(this);
    _picker->popup(anchor, QDir::toNativeSeparators(folder), std::move(items));
}

void Breadcrumbs::showSymbols(QWidget* anchor)
{
    std::vector<QuickPicker::Item> items;
    const int innermost = _trail.empty() ? -1 : _trail.back();
    for (int i = 0; i < static_cast<int>(_symbols.size()); ++i) {
        const Symbol& s = _symbols[static_cast<size_t>(i)];
        const int line = s.line;
        items.push_back({s.name, tr("line %1").arg(s.line + 1), i == innermost, [this, line] { emit goToLineRequested(line); }});
    }
    if (!_picker)
        _picker = new QuickPicker(this);
    _picker->popup(anchor, items.empty() ? tr("No symbols in this document") : tr("Go to a symbol"), std::move(items));
}

QStringList Breadcrumbs::segments() const
{
    QStringList texts;
    for (int i = 0; i < _layout->count(); ++i) {
        if (auto* button = qobject_cast<QToolButton*>(_layout->itemAt(i)->widget()))
            texts << button->text();
    }
    return texts;
}

bool Breadcrumbs::clickSegment(int index)
{
    int n = 0;
    for (int i = 0; i < _layout->count(); ++i) {
        if (auto* button = qobject_cast<QToolButton*>(_layout->itemAt(i)->widget())) {
            if (n++ == index) {
                button->click();
                return true;
            }
        }
    }
    return false;
}

} // namespace edita
