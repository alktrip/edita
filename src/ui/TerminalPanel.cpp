// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/TerminalPanel.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QProcess>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace edita {

TerminalPanel::TerminalPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("terminalPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    _tabs = new QTabWidget(this);
    _tabs->setObjectName(QStringLiteral("terminalTabs"));
    _tabs->setDocumentMode(true);
    _tabs->setTabsClosable(true);
    _tabs->setMovable(true);
    _tabs->tabBar()->setExpanding(false);
    layout->addWidget(_tabs);

    auto* corner = new QWidget(_tabs);
    auto* cornerLayout = new QHBoxLayout(corner);
    cornerLayout->setContentsMargins(0, 0, 4, 0);
    cornerLayout->setSpacing(0);
    auto* add = new QToolButton(corner);
    add->setObjectName(QStringLiteral("terminalNew"));
    add->setText(QStringLiteral("+"));
    add->setToolTip(tr("New Terminal"));
    add->setAutoRaise(true);
    cornerLayout->addWidget(add);
    _tabs->setCornerWidget(corner, Qt::TopRightCorner);

    connect(add, &QToolButton::clicked, this, &TerminalPanel::newTerminalRequested);
    connect(_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) { closeTerminal(terminalAt(index)); });
    connect(_tabs, &QTabWidget::currentChanged, this, [this] {
        if (TerminalView* view = currentTerminal())
            view->setFocus();
    });
}

QString TerminalPanel::resolveShell(const QString& preference)
{
    if (!preference.trimmed().isEmpty())
        return preference.trimmed();
    const QString shell = qEnvironmentVariable("SHELL");
    if (!shell.isEmpty() && QFileInfo(shell).isExecutable())
        return shell;
    return QStringLiteral("/bin/sh");
}

TerminalView* TerminalPanel::newTerminal(const QString& directory)
{
    auto* view = new TerminalView(_tabs);
    if (_hasFont)
        view->setTerminalFont(_font);
    if (_hasColors)
        view->setColors(_colors);
    // La shell y sus argumentos: «zsh -l», «/usr/bin/fish»…
    QStringList command = QProcess::splitCommand(resolveShell(_shell));
    const QString program = command.isEmpty() ? QStringLiteral("/bin/sh") : command.takeFirst();
    const QString dir = QFileInfo(directory).isDir() ? directory : QDir::homePath();
    const int index = _tabs->addTab(view, QFileInfo(program).fileName());
    _tabs->setTabToolTip(index, dir);
    _tabs->setCurrentIndex(index);
    connect(view, &TerminalView::titleChanged, this, [this, view] { updateTabTitle(view); });
    connect(view, &TerminalView::finished, this, [this, view] { closeTerminal(view); });
    if (!view->start(program, command, dir)) {
        view->feed(tr("Could not start %1").arg(program).toUtf8());
        return view;
    }
    view->setFocus();
    return view;
}

void TerminalPanel::updateTabTitle(TerminalView* view)
{
    const int index = _tabs->indexOf(view);
    if (index < 0)
        return;
    QString title = view->title();
    if (title.size() > 40)
        title = title.left(39) + QStringLiteral("…");
    if (!title.isEmpty())
        _tabs->setTabText(index, title);
    _tabs->setTabToolTip(index, view->title());
}

TerminalView* TerminalPanel::currentTerminal() const
{
    return qobject_cast<TerminalView*>(_tabs->currentWidget());
}

int TerminalPanel::count() const
{
    return _tabs->count();
}

TerminalView* TerminalPanel::terminalAt(int index) const
{
    return qobject_cast<TerminalView*>(_tabs->widget(index));
}

void TerminalPanel::closeTerminal(TerminalView* view)
{
    const int index = _tabs->indexOf(view);
    if (index < 0)
        return;
    _tabs->removeTab(index);
    view->deleteLater(); // cuelga la shell si sigue viva
    if (_tabs->count() == 0)
        emit lastTerminalClosed();
}

void TerminalPanel::setTerminalFont(const QFont& font)
{
    _font = font;
    _hasFont = true;
    for (int i = 0; i < count(); ++i)
        terminalAt(i)->setTerminalFont(font);
}

void TerminalPanel::setColors(const QColor& foreground, const QColor& background)
{
    _colors = TerminalView::defaultColors(foreground, background);
    _hasColors = true;
    for (int i = 0; i < count(); ++i)
        terminalAt(i)->setColors(_colors);
}

} // namespace edita
