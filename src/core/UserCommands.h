// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Las órdenes del menú Ejecutar (UserCommand y RunDlg de N++): una línea de órdenes con
// variables como $(FULL_CURRENT_PATH), guardada con un nombre y un atajo en la sección
// <UserDefinedCommands> de shortcuts.xml, con el formato de N++:
//   <Command name="Wikipedia Search" Ctrl="no" Alt="yes" Shift="no" Key="114">…</Command>

#pragma once

#include <QString>

#include <vector>

namespace edita {

struct UserCommand
{
    QString name;
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    int key = 0; // código de tecla virtual de Windows, como las macros; 0 = sin atajo
    QString command;

    bool operator==(const UserCommand&) const = default;
};

// NppParameters::feedUserCmds. Un archivo que no existe da una lista vacía y true.
bool readUserCommands(const QString& path, std::vector<UserCommand>& commands);
// Reescribe <UserDefinedCommands> conservando el resto de shortcuts.xml (o lo crea)
bool writeUserCommands(const QString& path, const std::vector<UserCommand>& commands);

// Las tres que trae N++, para Linux y macOS: ayuda de PHP y búsqueda en Wikipedia de la
// palabra del cursor, y abrir la ruta del cursor en otra ventana de Edita
std::vector<UserCommand> defaultUserCommands(const QString& editaPath);

// Lo que valen las variables de RunDlg (whichVar) en el momento de ejecutar
struct RunVariables
{
    QString fullCurrentPath; // FULL_CURRENT_PATH (el nombre de la pestaña si no tiene archivo)
    QString currentWord;     // CURRENT_WORD: la selección o la palabra del cursor
    QString currentLineText; // CURRENT_LINESTR
    long long line = 1;      // CURRENT_LINE (desde 1)
    long long column = 1;    // CURRENT_COLUMN (desde 1)
    QString appDirectory;    // NPP_DIRECTORY
    QString appPath;         // NPP_FULL_FILE_PATH
};

// expandNppEnvironmentStrs: sustituye las $(VARIABLE) conocidas; las demás se quedan como
// están (en la shell, «$(orden)» es la sustitución de órdenes)
QString expandRunVariables(const QString& command, const RunVariables& variables);

// Los nombres de las variables, en el orden del menú del diálogo
const std::vector<QString>& runVariableNames();

// Una orden que es solo una dirección (https://…): N++ la abre con ShellExecute, que la
// manda al navegador; aquí, con el programa del escritorio
bool isUrlCommand(const QString& expanded);

} // namespace edita
