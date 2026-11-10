// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/CommandLine.h"

#include <QCoreApplication>
#include <QStringList>

namespace edita {

CommandLineOptions setupCommandLine(QCommandLineParser& parser)
{
    // Mismas opciones básicas que Notepad++ (-n<línea>, -c<columna>)
    parser.setApplicationDescription(QCoreApplication::translate("main", "Text editor inspired by Notepad++"));
    parser.addHelpOption();
    parser.addVersionOption();
    CommandLineOptions options{
        QCommandLineOption({QStringLiteral("n"), QStringLiteral("line")},
                           QCoreApplication::translate("main", "Go to line <n> of the last file."), QStringLiteral("n")),
        QCommandLineOption({QStringLiteral("c"), QStringLiteral("column")}, QCoreApplication::translate("main", "Go to column <c>."),
                           QStringLiteral("c")),
        // -nosession de N++: ni se restaura ni se guarda la sesión
        QCommandLineOption(QStringLiteral("nosession"), QCoreApplication::translate("main", "Do not restore or save the session.")),
        // -multiInst de N++: otra ventana aunque ya haya una abierta
        QCommandLineOption(QStringLiteral("multiInst"),
                           QCoreApplication::translate("main", "Open a new instance even if Edita is already running.")),
        // -openSession de N++: el archivo es una sesión (sus archivos se abren)
        QCommandLineOption(QStringLiteral("openSession"), QCoreApplication::translate("main", "Open the files of a session file."),
                           QStringLiteral("file")),
        // -l de N++: el lenguaje del archivo (cpp, python…), aunque su extensión diga otro
        QCommandLineOption({QStringLiteral("l"), QStringLiteral("language")},
                           QCoreApplication::translate("main", "Use language <lang> (cpp, python…) for the last file."),
                           QStringLiteral("lang")),
    };
    parser.addOption(options.line);
    parser.addOption(options.column);
    parser.addOption(options.noSession);
    parser.addOption(options.multiInst);
    parser.addOption(options.session);
    parser.addOption(options.language);
    // Las recetas de plugins (se atienden antes, sin ventana: ui/RecipeCommands.h); aquí, para --help
    parser.addOption(QCommandLineOption(QStringLiteral("build-recipe"),
        QCoreApplication::translate("main", "Build a plugin recipe (with --output <folder>) and exit."), QStringLiteral("recipe.json")));
    parser.addOption(QCommandLineOption(QStringLiteral("check-plugin"),
        QCoreApplication::translate("main", "Install a plugin package in a temporary configuration and run the checks of its recipe (with --recipe <recipe.json>), then exit."),
        QStringLiteral("package.zip")));
    parser.addPositionalArgument(QCoreApplication::translate("main", "files"), QCoreApplication::translate("main", "Files to open."),
                                 QCoreApplication::translate("main", "[files…]"));
    return options;
}

QStringList newInstanceArguments(const QString& path, const QString& langName, int line, int column)
{
    QStringList arguments{path, QStringLiteral("--multiInst"), QStringLiteral("--nosession")};
    if (!langName.isEmpty())
        arguments << QStringLiteral("-l") << langName;
    arguments << QStringLiteral("-n") << QString::number(line) << QStringLiteral("-c") << QString::number(column);
    return arguments;
}

QString commandLineHelp()
{
    QCommandLineParser parser;
    setupCommandLine(parser);
    return parser.helpText();
}

} // namespace edita
