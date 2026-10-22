// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/NotificationBar.h"

#include "ui/Appearance.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>

namespace edita {

namespace {

constexpr int kInfoTimeout = 8000; // ms
constexpr int kMaxLines = 3;       // las demás, en el aviso emergente

} // namespace

NotificationBar::NotificationBar(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("notificationBar"));
    setAutoFillBackground(true);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 6, 6);
    layout->setSpacing(10);
    _icon = new QLabel(this);
    layout->addWidget(_icon, 0, Qt::AlignTop);
    _label = new QLabel(this);
    _label->setObjectName(QStringLiteral("notificationText"));
    _label->setWordWrap(true);
    _label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(_label, 1);
    _buttons = new QHBoxLayout;
    _buttons->setSpacing(6);
    layout->addLayout(_buttons);
    _close = new QToolButton(this);
    _close->setObjectName(QStringLiteral("notificationClose"));
    _close->setAutoRaise(true);
    _close->setText(QStringLiteral("✕"));
    _close->setToolTip(tr("Close"));
    layout->addWidget(_close, 0, Qt::AlignTop);
    connect(_close, &QToolButton::clicked, this, &NotificationBar::dismiss);
    _timer.setSingleShot(true);
    connect(&_timer, &QTimer::timeout, this, &NotificationBar::dismiss);
    hide();
}

void NotificationBar::show(Kind kind, const QString& title, const QString& text, std::vector<Action> actions)
{
    Notice notice{kind, title, text, std::move(actions)};
    if (_showing) {
        _queue.push_back(std::move(notice));
        return;
    }
    present(notice);
}

void NotificationBar::applyColors(Kind kind)
{
    // Un fondo suave del color del aviso, que se lee en claro y en oscuro
    const bool dark = appearance::isDarkApplied();
    const QColor accent = QColor::fromRgb(kind == Kind::Warning ? (dark ? 0xD2A021u : 0xBF8700u) : (dark ? 0x58A6FFu : 0x0969DAu));
    QColor back = QApplication::palette().color(QPalette::Window);
    back = QColor::fromRgbF(back.redF() * 0.85f + accent.redF() * 0.15f, back.greenF() * 0.85f + accent.greenF() * 0.15f,
                            back.blueF() * 0.85f + accent.blueF() * 0.15f);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, back);
    setPalette(pal);
    setStyleSheet(QStringLiteral("#notificationBar { border-bottom: 2px solid %1; }").arg(accent.name()));
    const QStyle::StandardPixmap pixmap = kind == Kind::Warning ? QStyle::SP_MessageBoxWarning : QStyle::SP_MessageBoxInformation;
    const int size = fontMetrics().height() + 4;
    _icon->setPixmap(style()->standardIcon(pixmap).pixmap(size, size));
}

void NotificationBar::present(const Notice& notice)
{
    _showing = true;
    applyColors(notice.kind);
    QStringList lines = notice.text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QString body;
    for (int i = 0; i < lines.size() && i < kMaxLines; ++i)
        body += (i ? QStringLiteral("<br>") : QString()) + lines[i].toHtmlEscaped();
    if (lines.size() > kMaxLines)
        body += QStringLiteral("<br><i>") + tr("…and %n more", nullptr, static_cast<int>(lines.size() - kMaxLines)).toHtmlEscaped()
              + QStringLiteral("</i>");
    _label->setText(notice.title.isEmpty() ? body : QStringLiteral("<b>%1</b> — %2").arg(notice.title.toHtmlEscaped(), body));
    _label->setToolTip(lines.size() > kMaxLines ? notice.text : QString());
    // Los botones de este aviso
    while (QLayoutItem* item = _buttons->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const Action& action : notice.actions) {
        auto* button = new QPushButton(action.text, this);
        const auto run = action.run;
        connect(button, &QPushButton::clicked, this, [this, run] {
            dismiss();
            if (run)
                run();
        });
        _buttons->addWidget(button);
    }
    QFrame::show();
    if (notice.kind == Kind::Info && notice.actions.empty())
        _timer.start(kInfoTimeout);
    else
        _timer.stop();
}

void NotificationBar::dismiss()
{
    _timer.stop();
    _showing = false;
    if (!_queue.empty()) {
        const Notice next = _queue.front();
        _queue.pop_front();
        present(next);
        return;
    }
    hide();
}

QString NotificationBar::currentText() const
{
    return isHidden() ? QString() : _label->text();
}

} // namespace edita
