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

#include "core/Paths.h"

#include "AppInfo.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace edita::paths {

namespace {

QString appId()
{
    return QString::fromUtf8(kAppId);
}

} // namespace

QString configDir()
{
    // $XDG_CONFIG_HOME manda también fuera de Linux (en macOS Qt usaría ~/Library/Preferences;
    // las pruebas lo usan para no tocar la configuración real)
    const QString xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (!xdg.isEmpty())
        return QDir(xdg).filePath(appId());
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath(appId());
}

bool ensureConfigDir()
{
    return QDir().mkpath(configDir());
}

void useOwnQtSettings()
{
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, configDir());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configDir());
}

QStringList dataDirs()
{
    QStringList dirs;
    const QString fromEnv = qEnvironmentVariable("EDITA_DATA_DIR");
    if (!fromEnv.isEmpty())
        dirs << fromEnv;
    if (QCoreApplication::instance()) {
        const QDir appDir(QCoreApplication::applicationDirPath());
        dirs << appDir.filePath(QStringLiteral("../share/") + appId());
        dirs << appDir.filePath(QStringLiteral("../Resources/") + appId()); // paquete .app de macOS
    }
    for (const QString& base : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        dirs << QDir(base).filePath(appId());
#ifdef EDITA_SOURCE_DATA_DIR
    dirs << QStringLiteral(EDITA_SOURCE_DATA_DIR); // build sin instalar
#endif
    dirs.removeDuplicates();
    return dirs;
}

QString findDataFile(const QString& relativePath)
{
    for (const QString& dir : dataDirs()) {
        const QString candidate = QDir(dir).filePath(relativePath);
        if (QFileInfo::exists(candidate))
            return QDir::cleanPath(candidate);
    }
    return {};
}

} // namespace edita::paths
