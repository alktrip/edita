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

#pragma once

#include "ui/Shortcuts.h"

#include <QDialog>

class QLineEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;

namespace edita {

class MainWindow;
class Prompter;

// Shortcut Mapper de N++ (Configuración > Shortcut Mapper…): pestañas Menú
// principal, Macros, Órdenes de plugins y Órdenes de Scintilla, con filtro y los
// botones Modificar, Borrar y Eliminar. Los cambios se aplican y se guardan en
// shortcuts.xml al momento, como en N++.
class ShortcutMapperDialog : public QDialog
{
    Q_OBJECT

public:
    ShortcutMapperDialog(MainWindow& window, Prompter& prompter, QWidget* parent = nullptr);

    QTableWidget* table(ShortcutEntry::Kind kind) const;
    // Lo que hace "Modificar" con la tecla elegida: si otra orden la usa, pregunta
    // si se le quita. Falso si no se asignó.
    bool assign(ShortcutEntry::Kind kind, const QString& id, const KeyCombo& combo);
    // "Borrar": la orden se queda sin atajo
    bool clear(ShortcutEntry::Kind kind, const QString& id);
    void setFilter(const QString& text);

private:
    void refresh();
    const ShortcutEntry* selected() const;
    void modifySelected();

    MainWindow& _window;
    Prompter& _prompter;
    std::vector<ShortcutEntry> _entries;
    QTabWidget* _tabs = nullptr;
    QTableWidget* _tables[4] = {};
    QLineEdit* _filter = nullptr;
    QPushButton* _delete = nullptr;
};

} // namespace edita
