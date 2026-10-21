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

#include "syntax/StyleSheet.h"

#include <QDialog>

#include <functional>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QToolButton;

namespace edita {

class Prompter;
class SyntaxManager;

// Configurador de estilos de N++ (WordStyleDlg, Configuración > Configurador de
// estilos…): tema; lenguaje ("Estilos globales" y cada lexer) y estilo; colores,
// fuente, tamaño, negrita/cursiva/subrayado; el Global override; las extensiones
// y palabras clave del usuario. Los cambios se ven al momento en el editor;
// "Guardar y cerrar" los escribe en el archivo de estilos y "Cancelar" los deshace.
class StyleConfiguratorDialog : public QDialog
{
    Q_OBJECT

public:
    // Cambia de tema como Ver > Tema (MainWindow::setTheme: lo recuerda para el modo)
    using ThemeSwitcher = std::function<bool(const QString& path)>;

    StyleConfiguratorDialog(SyntaxManager& syntax, Prompter& prompter, ThemeSwitcher switchTheme,
                            QWidget* parent = nullptr);

    // Selección: -1 = Estilos globales; si no, el índice del lexer
    bool selectLanguage(int lexerIndex);
    bool selectStyle(const QString& name);
    // El estilo seleccionado (en la copia que se está editando)
    const Style* currentStyle() const;
    const StyleSheet& workingStyles() const { return _working; }
    bool isDirty() const { return _dirty; }

    // Lo que hacen los botones de color (para no pasar por QColorDialog)
    void setStyleColor(bool foreground, const QColor& color);

    bool saveAndClose();
    void reject() override;

private:
    void buildUi();
    void fillThemes();
    void fillLanguages();
    void fillStyles();
    void showStyle();
    void switchTheme(int index);
    Style* current();
    void changed();
    void pickColor(bool foreground);
    void setFontStyleBit(int bit, bool on);

    SyntaxManager& _syntax;
    Prompter& _prompter;
    ThemeSwitcher _switchTheme;
    StyleSheet _working;
    StyleSheet _original;
    int _lexerIndex = -1;
    int _styleIndex = 0;
    bool _dirty = false;
    bool _updating = false;

    QComboBox* _themes = nullptr;
    QListWidget* _languages = nullptr;
    QListWidget* _styles = nullptr;
    QLabel* _styleInfo = nullptr;
    QToolButton* _foreground = nullptr;
    QToolButton* _background = nullptr;
    QComboBox* _fontName = nullptr;
    QComboBox* _fontSize = nullptr;
    QCheckBox* _bold = nullptr;
    QCheckBox* _italic = nullptr;
    QCheckBox* _underline = nullptr;
    QGroupBox* _override = nullptr;
    QCheckBox* _overrideChecks[7] = {};
    QGroupBox* _extensions = nullptr;
    QLabel* _defaultExtensions = nullptr;
    QLineEdit* _userExtensions = nullptr;
    QGroupBox* _keywords = nullptr;
    QPlainTextEdit* _defaultKeywords = nullptr;
    QPlainTextEdit* _userKeywords = nullptr;
};

} // namespace edita
