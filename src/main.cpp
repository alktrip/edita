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
#include "ui/SingleInstance.h"
#include "ui/Translations.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    edita::initGlobalLocale(); // antes de cualquier regex: ver core/Locale.h
    QApplication::setApplicationName(QString::fromUtf8(edita::kAppId));
    QApplication::setApplicationDisplayName(QString::fromUtf8(edita::kAppName));
    QApplication::setApplicationVersion(QString::fromUtf8(edita::kAppVersion));
    QApplication::setWindowIcon(edita::MainWindow::applicationIcon());
    QGuiApplication::setDesktopFileName(QString::fromUtf8(edita::kAppId)); // Wayland: une la ventana con su .desktop
    edita::paths::useOwnQtSettings(); // el diálogo de archivos de Qt, sin compartir QtProject.conf
    // El idioma antes de crear cualquier texto: el de config.xml o el del sistema
    edita::Preferences prefs;
    {
        edita::ConfigXml config;
        config.load(edita::paths::configDir() + QStringLiteral("/config.xml"));
        prefs = edita::loadPreferences(config);
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
    // -multiInst de N++: otra ventana aunque ya haya una abierta
    const QCommandLineOption multiInstOption(QStringLiteral("multiInst"),
                                             QCoreApplication::translate("main", "Open a new instance even if Edita is already running."));
    parser.addOption(lineOption);
    parser.addOption(columnOption);
    parser.addOption(noSessionOption);
    parser.addOption(multiInstOption);
    parser.addPositionalArgument(QCoreApplication::translate("main", "files"), QCoreApplication::translate("main", "Files to open."),
                                 QCoreApplication::translate("main", "[files…]"));
    parser.process(app);

    // Rutas completas: la instancia que las recibe puede estar en otra carpeta
    edita::InstanceRequest request;
    for (const QString& file : parser.positionalArguments())
        request.files << QFileInfo(file).absoluteFilePath();
    if (parser.isSet(lineOption))
        request.line = parser.value(lineOption).toInt();
    if (parser.isSet(columnOption))
        request.column = parser.value(columnOption).toInt();

    // winmain.cpp de N++: con una sola instancia (lo normal), si ya hay una abierta se
    // le pasan los archivos y esta termina
    const QString socket = edita::InstanceServer::socketName(edita::paths::configDir());
    const bool multiInst = parser.isSet(multiInstOption) || prefs.multiInstSetting == edita::MultiInst::Always;
    if (!multiInst && edita::InstanceServer::send(socket, request))
        return 0;
    // Con varias instancias, solo la primera recuerda la sesión
    const bool firstInstance = !edita::InstanceServer::isRunning(socket);

    edita::MainWindow window;
    window.restoreWindowGeometry();
    window.show();
    // Como N++: primero la sesión anterior y después los archivos de la línea de comandos
    window.setSessionEnabled(!parser.isSet(noSessionOption) && firstInstance);
    window.restoreLastSession();
    window.openFromOtherInstance(request, /*raise*/ false);
    window.notifyPluginsReady(); // NPPN_READY: todo listo

    edita::InstanceServer server;
    if (firstInstance && server.listen(socket))
        QObject::connect(&server, &edita::InstanceServer::requestReceived, &window,
                         [&window](const edita::InstanceRequest& r) { window.openFromOtherInstance(r, /*raise*/ true); });
    return QApplication::exec();
}
