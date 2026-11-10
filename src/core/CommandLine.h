// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Las opciones de la línea de órdenes (las de Notepad++: -n, -c, -nosession, -multiInst,
// -openSession), para main.cpp y para Ayuda › Argumentos de la línea de órdenes

#pragma once

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QString>
#include <QStringList>

namespace edita {

struct CommandLineOptions
{
    QCommandLineOption line;
    QCommandLineOption column;
    QCommandLineOption noSession;
    QCommandLineOption multiInst;
    QCommandLineOption session;
    QCommandLineOption language;
};

// Añade al parser la ayuda, la versión, las opciones y los archivos
CommandLineOptions setupCommandLine(QCommandLineParser& parser);

// Los argumentos para abrir `path` en otra ventana de Edita (docOpenInNewInstance de N++): otra
// instancia, sin sesión, con el lenguaje (`langName`, el de langs.xml; vacío, sin -l) y el cursor
QStringList newInstanceArguments(const QString& path, const QString& langName, int line, int column);

// El texto de --help (IDM_CMDLINEARGUMENTS de N++)
QString commandLineHelp();

} // namespace edita
