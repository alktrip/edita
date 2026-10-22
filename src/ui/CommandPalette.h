// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Paleta de órdenes e «Ir a archivo» (no existen en Notepad++): un buscador flotante
// arriba de la ventana. Según el primer carácter de lo escrito busca
//   (nada)  archivos: los abiertos, los recientes y los de las carpetas de trabajo
//   >       órdenes: todo lo de los menús, los plugins y las macros
//   @       símbolos del documento (los analizadores de la Lista de funciones)
//   :       una línea («:120» o «:120:5»)
// con búsqueda difusa (core/FuzzyMatch). Lo que se ejecuta lo hace quien la usa:
// la paleta solo pide los elementos (itemsRequested) y llama a su `run`.

#pragma once

#include <QFrame>
#include <QIcon>
#include <QPointer>

#include <functional>
#include <vector>

class QLineEdit;
class QListView;
class QStandardItemModel;
class QLabel;

namespace edita {

class CommandPalette : public QFrame
{
    Q_OBJECT

public:
    enum class Mode { Files, Commands, Symbols, Line };

    struct Item
    {
        QString text;      // lo que se busca y se ve
        QString detail;    // menú, carpeta, clase… (atenuado)
        QString shortcut;  // atajo, a la derecha
        QString id;        // para el historial de órdenes
        QIcon icon;
        bool checked = false;  // una opción marcada (✓)
        int order = 0;         // orden sin búsqueda (posición en el documento, recientes…)
        std::function<void()> run;
    };

    explicit CommandPalette(QWidget* parent);

    // Abre la paleta con ese texto inicial (">" órdenes, "@" símbolos, "" archivos)
    void open(const QString& prefix);
    void close();

    // Los elementos de un modo (se piden con itemsRequested; los archivos pueden llegar
    // en varias tandas mientras se recorren las carpetas)
    void setItems(Mode mode, std::vector<Item> items);
    void addItems(Mode mode, std::vector<Item> items);
    void setBusy(Mode mode, bool busy);

    // Las órdenes ejecutadas, las más recientes primero (salen primero sin búsqueda)
    void setHistory(const QStringList& ids) { _history = ids; }
    QStringList history() const { return _history; }

    static Mode modeFor(const QString& text);

    // Para las pruebas
    QLineEdit* input() const { return _input; }
    QStringList visibleTexts() const;
    bool activateRow(int row);

signals:
    void itemsRequested(edita::CommandPalette::Mode mode);
    void goToLineRequested(int line, int column);
    void historyChanged(const QStringList& ids);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void filter();
    void reposition();
    void activate(const QModelIndex& index);

    QLineEdit* _input = nullptr;
    QListView* _list = nullptr;
    QLabel* _status = nullptr;
    QStandardItemModel* _model = nullptr;
    std::vector<Item> _items[4];
    bool _requested[4] = {};
    bool _busy[4] = {};
    std::vector<int> _shown; // índice en _items del modo actual, por fila
    Mode _mode = Mode::Files;
    QStringList _history;
};

} // namespace edita
