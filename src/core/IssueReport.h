// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Informar de un problema: la dirección del formulario de GitHub (.github/ISSUE_TEMPLATE), con
// la versión y el sistema ya escritos. GitHub rellena un campo de un formulario con el
// parámetro de la URL que tiene su «id».

#pragma once

#include <QString>
#include <QList>
#include <QUrl>

#include <utility>

namespace edita::issue_report {

// Lo que se sabe del sistema, para el campo «system»
struct SystemInfo {
    QString os;          // QSysInfo::prettyProductName()
    QString kernel;      // QSysInfo::kernelVersion()
    QString architecture; // QSysInfo::currentCpuArchitecture()
    QString qtVersion;   // qVersion()
    QString platform;    // QGuiApplication::platformName(): wayland, xcb…
    QString desktop;     // $XDG_CURRENT_DESKTOP
    bool appImage = false; // $APPIMAGE
};

SystemInfo currentSystem(const QString& platform);
QString describe(const SystemInfo& system);

// homepage: https://github.com/<dueño>/<repo>; form: el archivo de la plantilla (bug.yml).
// `fields`: más campos del formulario, por su id ({"what", …}, {"extra", …})
QUrl formUrl(const QString& homepage, const QString& form, const QString& version, const QString& system,
             const QList<std::pair<QString, QString>>& fields = {});

} // namespace edita::issue_report
