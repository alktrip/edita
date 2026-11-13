// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Lo que muestra Ayuda › Acerca de Edita (fase 91), sin interfaz: los componentes de
// terceros con su versión y su licencia, cómo se instaló Edita, la compilación (commit y
// fecha) y el resumen que se copia para un reporte.

#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <vector>

namespace edita::about {

struct Component
{
    QString name;
    QString version;
    QString license;     // identificador SPDX
    QUrl homepage;
    QString usedFor;     // para qué lo usa Edita (traducido)
    QString licenseText; // recurso con el texto de su licencia (vacío: solo el enlace)
};

// Los componentes de terceros que van dentro de Edita, en el orden en que se muestran
std::vector<Component> components();

enum class Install {
    AppImage,
    Deb,    // /opt/edita, instalado con dpkg
    Rpm,    // /opt/edita, instalado con rpm
    System, // un paquete de la distribución (/usr)
    Source, // compilado por el usuario
};

// Cómo se instaló, a partir de la ruta del ejecutable y de $APPIMAGE; exists responde si
// existe un archivo (el registro de dpkg)
Install detectInstall(const QString& executable, const QString& appImage, const std::function<bool(const QString&)>& exists);
Install currentInstall();
QString installName(Install install); // «AppImage», «.deb»…

// La compilación: «a5777d0, 2026-11-13» (vacío si no se compiló desde git)
QString buildCommit();
QString buildDate();

// «Edita 1.0.0-rc.3 (x86_64, AppImage)»
QString versionLine(const QString& version, const QString& architecture, Install install);
// Lo que copia el botón: la versión, la compilación, Qt, el sistema y los plugins
QString copyText(const QString& versionLine, const QString& build, const QString& system, const QStringList& plugins);
// Las notas de esta versión: <página>/releases/tag/v<versión>
QUrl releaseNotesUrl(const QString& homepage, const QString& version);

} // namespace edita::about
