// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tareas del proyecto (fase 84; no existe en N++): lo que se puede compilar, probar o ejecutar
// en la carpeta del proyecto (Makefile, CMake, package.json, Cargo, Go, Python, Meson, just) y
// los errores que salen al ejecutarlo, para ir a cada uno. El menú Ejecutar de N++ lanza una
// orden y no lee lo que sale

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace edita::tasks {

struct Task
{
    QString label;   // «make test», «npm run build»
    QString command; // la orden para la shell
    QString cwd;     // la carpeta del proyecto
    QString source;  // «Makefile», «package.json»…
};

// La carpeta del proyecto: la más cercana a `start` (un archivo o una carpeta), subiendo, que
// tenga uno de los archivos que se reconocen o .git. Vacía si no hay ninguna
QString findProjectRoot(const QString& start);

// Las tareas de esa carpeta, en el orden en que se muestran
QList<Task> detectTasks(const QString& root);

// Los objetivos de un Makefile (sin los especiales ni los de patrones)
QStringList makeTargets(const QString& makefileText);

struct Problem
{
    QString file; // ruta absoluta
    int line = 0; // desde 1
    int column = 0; // desde 1; 0 si no se sabe
    int severity = 1; // 1 error, 2 aviso, 3 nota (como LSP)
    QString message;
    QString code; // E0308, TS2322…
};

// Quita los colores y demás secuencias de escape de la terminal
QString stripAnsi(const QString& text);

// Lee la salida línea a línea y reconoce los errores de GCC/Clang, Go, rustc, tsc, Python,
// pytest y similares. Las rutas relativas se resuelven desde `cwd`; una línea con una ruta que
// no existe no cuenta (para no tomar «http://x:80:1» por un error)
class ProblemParser
{
public:
    explicit ProblemParser(QString cwd);
    // Una línea (sin el salto). Devuelve el problema que completa, si hay uno
    std::optional<Problem> feed(const QString& line);

private:
    std::optional<QString> resolve(const QString& path) const;

    QString _cwd;
    // rustc: «error[E0308]: mensaje» y, unas líneas después, « --> archivo:línea:col»
    std::optional<Problem> _pendingRust;
    // Python: la última línea «File "x", line N» de una traza; el error es la última línea
    std::optional<Problem> _pendingPython;
};

} // namespace edita::tasks
