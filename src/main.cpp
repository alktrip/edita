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

#include "AppInfo.h"
#include "core/ConfigXml.h"
#include "core/Locale.h"
#include "core/Paths.h"
#include "core/Preferences.h"
#include "ui/MainWindow.h"
#include "ui/Translations.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    edita::initGlobalLocale(); // antes de cualquier regex: ver core/Locale.h
    QApplication::setApplicationName(QString::fromUtf8(edita::kAppId));
    QApplication::setApplicationDisplayName(QString::fromUtf8(edita::kAppName));
    QApplication::setApplicationVersion(QString::fromUtf8(edita::kAppVersion));
    QApplication::setWindowIcon(edita::MainWindow::applicationIcon());
    QGuiApplication::setDesktopFileName(QString::fromUtf8(edita::kAppId)); // Wayland: une la ventana con su .desktop
    // El idioma antes de crear cualquier texto: el de config.xml o el del sistema
    {
        edita::ConfigXml config;
        config.load(edita::paths::configDir() + QStringLiteral("/config.xml"));
        const edita::Preferences prefs = edita::loadPreferences(config);
        edita::i18n::installFromPreference(prefs.language);
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, prefs.qtFileDialog);
    }

    // Mismas opciones básicas que Notepad++ (-n<línea>, -c<columna>)
    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Text editor inspired by Notepad++"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption lineOption({QStringLiteral("n"), QStringLiteral("line")},
                                        QCoreApplication::translate("main", "Go to line <n> of the last file."),
                                        QStringLiteral("n"));
    const QCommandLineOption columnOption({QStringLiteral("c"), QStringLiteral("column")},
                                          QCoreApplication::translate("main", "Go to column <c>."), QStringLiteral("c"));
    // -nosession de N++: ni se restaura ni se guarda la sesión
    const QCommandLineOption noSessionOption(QStringLiteral("nosession"),
                                             QCoreApplication::translate("main", "Do not restore or save the session."));
    parser.addOption(lineOption);
    parser.addOption(columnOption);
    parser.addOption(noSessionOption);
    parser.addPositionalArgument(QCoreApplication::translate("main", "files"), QCoreApplication::translate("main", "Files to open."),
                                 QCoreApplication::translate("main", "[files…]"));
    parser.process(app);

    edita::MainWindow window;
    window.restoreWindowGeometry();
    window.show();
    // Como N++: primero la sesión anterior y después los archivos de la línea de comandos
    window.setSessionEnabled(!parser.isSet(noSessionOption));
    window.restoreLastSession();
    window.openFiles(parser.positionalArguments(), /*askToCreate*/ true);
    if (parser.isSet(lineOption))
        window.goToLine(parser.value(lineOption).toInt(), parser.isSet(columnOption) ? parser.value(columnOption).toInt() : 1);
    window.notifyPluginsReady(); // NPPN_READY: todo listo
    return QApplication::exec();
}
