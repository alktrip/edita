// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Una lista flotante con buscador (búsqueda difusa, core/FuzzyMatch) que se abre sobre
// un control: la usa la barra de estado para cambiar el lenguaje, la codificación, el
// fin de línea y la sangría sin ir a los menús. Intro elige, Esc cierra.

#pragma once

#include <QFrame>
#include <QIcon>

#include <functional>
#include <vector>

class QLineEdit;
class QListWidget;

namespace edita {

class QuickPicker : public QFrame
{
    Q_OBJECT

public:
    struct Item
    {
        QString text;
        QString detail;  // atenuado, a la derecha (menú, grupo)
        bool checked = false;
        std::function<void()> run;
    };

    explicit QuickPicker(QWidget* parent = nullptr);

    // Se abre encima de `anchor` (la barra de estado está abajo); el marcado, elegido
    void popup(QWidget* anchor, const QString& placeholder, std::vector<Item> items);
    // Otros elementos sin cerrar ni borrar lo escrito (listas que se piden al escribir)
    void setItems(std::vector<Item> items);

    // Para las pruebas
    QLineEdit* input() const { return _input; }
    QStringList visibleTexts() const;
    bool activate(const QString& text);

signals:
    void queryChanged(const QString& query);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void filter();
    void choose(int row);

    QLineEdit* _input = nullptr;
    QListWidget* _list = nullptr;
    std::vector<Item> _items;
};

} // namespace edita
