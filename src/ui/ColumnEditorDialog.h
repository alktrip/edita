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

#include "core/ColumnEditor.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QRadioButton;
class ScintillaEditBase;

namespace edita {

class ConfigXml;

// Editor de columnas de N++ (ColumnEditorDlg, Editar > Editor de columnas…, Alt+C):
// un texto, o números con valor inicial, incremento, repetición, base y relleno.
// Recuerda lo último en config.xml (<GUIConfig name="ColumnEditor" …/>).
class ColumnEditorDialog : public QDialog
{
    Q_OBJECT

public:
    ColumnEditorDialog(ScintillaEditBase& view, ConfigXml& config, QWidget* parent = nullptr);

    // Lo que hace Aceptar: comprueba los números, inserta y guarda las opciones.
    // Falso (y el campo se marca) si un número no es válido en la base elegida.
    bool insert();

private:
    NumberBase base() const;
    void updateEnabled();
    void load();
    void save() const;

    ScintillaEditBase& _view;
    ConfigXml& _config;
    QRadioButton* _textMode = nullptr;
    QRadioButton* _numberMode = nullptr;
    QLineEdit* _text = nullptr;
    QLineEdit* _initial = nullptr;
    QLineEdit* _increase = nullptr;
    QLineEdit* _repeat = nullptr;
    QRadioButton* _decimal = nullptr;
    QRadioButton* _hex = nullptr;
    QRadioButton* _octal = nullptr;
    QRadioButton* _binary = nullptr;
    QComboBox* _hexCase = nullptr;
    QComboBox* _leading = nullptr;
};

} // namespace edita
