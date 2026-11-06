// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reporte de fallos: si Edita se cierra por un error (SIGSEGV, SIGABRT…), deja la versión,
// la señal y la traza en <configuración>/crash/last-crash.txt; al volver a abrirlo, ofrece
// reportarlo (MainWindow). El manejador solo usa llamadas seguras en una señal (open, write,
// backtrace_symbols_fd) y después deja que el proceso termine como lo haría sin él.
//
// Las direcciones son de cada módulo («edita(+0x1a2b3c)»): con el AppImage de esa versión,
// que conserva la información de depuración, addr2line da el archivo y la línea.

#pragma once

#include <QString>

namespace edita::crash_report {

// Instala los manejadores (una vez, al arrancar). `directory` se crea si no existe
void install(const QString& directory, const QString& version);

// La carpeta de los reportes: <configuración>/crash
QString defaultDirectory();
QString reportPath(const QString& directory);

// El reporte del último fallo, si hay (vacío si no)
QString pending(const QString& directory);
// Lo borra (reportado, copiado o descartado)
void clear(const QString& directory);

// El reporte con los nombres de C++ legibles (abi::__cxa_demangle), para mostrarlo y enviarlo
QString readable(const QString& report);
// La señal del reporte («SIGSEGV»), o vacío
QString signalName(const QString& report);

// Solo para las pruebas: lo que haría el manejador, sin fallar (escribe el reporte)
void writeReportForTesting(int signal);

} // namespace edita::crash_report
