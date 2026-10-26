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

#include <QStatusBar>

class QLabel;
class QMenu;
class QToolButton;

namespace edita {

class Buffer;
class EditorView;

// Barra de estado con los mismos campos que la de Notepad++
// (Notepad_plus::updateStatusBar / setUniModeText / setDisplayFormat):
//   tipo de documento | longitud y líneas | Lín, Col, Pos (y selección) |
//   fin de línea | codificación | INS/SOB
// Además (no está en N++): la sangría del documento («Espacios: 4»), y los campos se
// pueden pulsar para cambiarlos (MainWindow abre una lista con buscador sobre el campo):
// lenguaje, posición (ir a una línea), sangría, fin de línea, codificación e INS/SOB.
class DocStatusBar : public QStatusBar
{
    Q_OBJECT

public:
    enum class Field { Language, Position, Indentation, Eol, Encoding, InsertMode, Problems };

    explicit DocStatusBar(QWidget* parent = nullptr);

    // El control de un campo (para abrir la lista encima)
    QWidget* field(Field field) const;
    // La sangría del documento: con tabulaciones o espacios, y el ancho
    void updateIndentation(bool useTabs, int width);
    QString indentationText() const;

    // Refresca los campos del documento (nombre del tipo, EOL, codificación)
    void updateDocument(const Buffer* buffer);
    // Refresca longitud, posición y modo de inserción a partir del editor
    void updateCaret(const EditorView* editor);

    // NPPM_SETSTATUSBAR: texto de un campo (0 tipo, 1 tamaño, 2 posición, 3 fin de
    // línea, 4 codificación, 5 modo); se mantiene hasta el siguiente cambio de documento
    void setFieldText(int field, const QString& text);

    QString languageText() const;
    QString lengthText() const;
    QString positionText() const;
    QString eolText() const;
    QString encodingText() const;
    QString insertModeText() const;
    // Los problemas del servidor de lenguaje (oculto si no hay ninguno en marcha)
    void setProblems(bool visible, int errors, int warnings, const QString& toolTip);
    QString problemsText() const;

signals:
    void fieldClicked(edita::DocStatusBar::Field field);

private:
    QToolButton* _language;
    QLabel* _length;
    QToolButton* _position;
    QToolButton* _indentation;
    QToolButton* _eol;
    QToolButton* _encoding;
    QToolButton* _insertMode;
    QToolButton* _problems;
};

} // namespace edita
