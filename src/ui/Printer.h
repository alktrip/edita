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

#include "core/Preferences.h"

#include <ScintillaEditBase.h>

#include <QDateTime>
#include <QString>

class QPagedPaintDevice;

namespace edita {

class EditorView;

// Qué imprimir (los campos de PRINTDLG que usa Printer::doPrint de N++)
struct PrintRequest
{
    QString documentPath;     // ruta completa, o el nombre de la pestaña si no tiene archivo
    bool selectionOnly = false;
    sptr_t selectionStart = 0;
    sptr_t selectionEnd = 0;
    int fromPage = 0;         // 0 = todas las páginas
    int toPage = 0;
};

// Variables de la cabecera y el pie (expandNppEnvironmentStrs + las de fecha de
// Printer::doPrint): $(FULL_CURRENT_PATH), $(CURRENT_DIRECTORY), $(FILE_NAME),
// $(NAME_PART), $(EXT_PART), $(SHORT_DATE), $(LONG_DATE), $(TIME) y
// $(CURRENT_PRINTING_PAGE)
QString expandPrintVariables(QString text, const QString& documentPath, const QDateTime& now, int page);

// Printer::doPrint: pagina el documento de la vista con SCI_FORMATRANGEFULL
// sobre una impresora o un PDF, con márgenes, cabecera, pie, modo de color,
// números de línea y saltos de página (\f) según las preferencias. Devuelve las
// páginas emitidas (0 si no se pudo empezar).
int printDocument(EditorView& view, QPagedPaintDevice& device, const Preferences& prefs, const PrintRequest& request);

} // namespace edita
