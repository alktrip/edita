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

#include <QKeySequence>
#include <QString>
#include <QStringList>

#include <cstddef>

class QWidget;

namespace edita {

// Todas las preguntas que la ventana principal hace al usuario. Separarlas en
// una interfaz permite que los tests respondan sin abrir diálogos reales.
class Prompter
{
public:
    enum class SaveChoice { Save, Discard, Cancel, SaveAll, DiscardAll };
    enum class UnmappableChoice { SaveAnyway, ConvertToUtf8, Cancel };

    virtual ~Prompter() = default;

    // "¿Guardar los cambios de X?" — `offerAll` añade "Guardar todo" / "No guardar ninguno"
    virtual SaveChoice askSaveChanges(QWidget* parent, const QString& documentName, bool offerAll) = 0;

    // Pregunta de sí/no
    virtual bool confirm(QWidget* parent, const QString& title, const QString& text) = 0;

    virtual void warn(QWidget* parent, const QString& title, const QString& text) = 0;

    // Diálogos de archivo. Vacío = cancelado. `filters` con el formato de
    // QFileDialog ("Python file (*.py *.pyw);;…"); `selectedFilter` preselecciona uno.
    virtual QStringList openFileNames(QWidget* parent, const QString& directory, const QString& filters) = 0;
    virtual QString saveFileName(QWidget* parent, const QString& suggestedPath, const QString& filters,
                                 const QString& selectedFilter) = 0;

    // Al guardar en un juego de caracteres que no puede representar todo el texto
    virtual UnmappableChoice askUnmappable(QWidget* parent, const QString& documentName, std::size_t count,
                                           const QString& encodingName) = 0;

    // "Guardar la macro actual": nombre y atajo (vacío = sin atajo). false = cancelado
    virtual bool askMacroName(QWidget* parent, QString& name, QKeySequence& shortcut) = 0;

    // Un texto (p. ej. el nombre nuevo al renombrar). false = cancelado
    virtual bool askText(QWidget* parent, const QString& title, const QString& label, QString& text) = 0;
};

// Implementación con QMessageBox y QFileDialog
class DialogPrompter : public Prompter
{
public:
    SaveChoice askSaveChanges(QWidget* parent, const QString& documentName, bool offerAll) override;
    bool confirm(QWidget* parent, const QString& title, const QString& text) override;
    void warn(QWidget* parent, const QString& title, const QString& text) override;
    QStringList openFileNames(QWidget* parent, const QString& directory, const QString& filters) override;
    QString saveFileName(QWidget* parent, const QString& suggestedPath, const QString& filters,
                         const QString& selectedFilter) override;
    UnmappableChoice askUnmappable(QWidget* parent, const QString& documentName, std::size_t count,
                                   const QString& encodingName) override;
    bool askMacroName(QWidget* parent, QString& name, QKeySequence& shortcut) override;
    bool askText(QWidget* parent, const QString& title, const QString& label, QString& text) override;
};

} // namespace edita
