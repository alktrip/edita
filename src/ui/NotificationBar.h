// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Avisos sin ventanas modales (Notepad++ usa cuadros de mensaje): una barra discreta
// encima del editor con un icono, el texto, botones opcionales y una ✕. Los avisos
// informativos se ocultan solos; los de advertencia esperan a que se cierren. Si
// llegan varios, se ven uno tras otro.

#pragma once

#include <QFrame>
#include <QStringList>
#include <QTimer>

#include <deque>
#include <functional>
#include <vector>

class QHBoxLayout;
class QLabel;
class QToolButton;

namespace edita {

class NotificationBar : public QFrame
{
    Q_OBJECT

public:
    enum class Kind { Info, Warning };

    struct Action
    {
        QString text;
        std::function<void()> run;
    };

    explicit NotificationBar(QWidget* parent = nullptr);

    // title (en negrita) y el texto; las líneas de más quedan en el aviso emergente
    void show(Kind kind, const QString& title, const QString& text, std::vector<Action> actions = {});
    void dismiss();

    // Para las pruebas
    QString currentText() const;
    int pending() const { return static_cast<int>(_queue.size()); }

private:
    struct Notice
    {
        Kind kind;
        QString title;
        QString text;
        std::vector<Action> actions;
    };
    void present(const Notice& notice);
    void applyColors(Kind kind);

    std::deque<Notice> _queue;
    QLabel* _icon = nullptr;
    QLabel* _label = nullptr;
    QHBoxLayout* _buttons = nullptr;
    QToolButton* _close = nullptr;
    QTimer _timer;
    bool _showing = false;
};

} // namespace edita
