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
#include <QFile>
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

QString baseConfigDir()
{
    // $XDG_CONFIG_HOME manda también fuera de Linux (en macOS Qt usaría ~/Library/Preferences;
    // las pruebas lo usan para no tocar la configuración real)
    const QString xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (!xdg.isEmpty())
        return QDir(xdg).filePath(appId());
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath(appId());
}

QString cloudChoice()
{
    QFile file(QDir(baseConfigDir()).filePath(QStringLiteral("cloud/choice")));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString configDir()
{
    // NppParameters::load: con cloud/choice y su carpeta, la configuración está ahí. Se decide
    // una vez por carpeta base (la elección nueva vale al reiniciar, como en N++)
    static QString lastBase;
    static QString lastDir;
    const QString base = baseConfigDir();
    if (base != lastBase) {
        lastBase = base;
        const QString cloud = cloudChoice();
        lastDir = !cloud.isEmpty() && QFileInfo(cloud).isDir() ? QDir::cleanPath(cloud) : base;
    }
    return lastDir;
}

bool setCloudChoice(const QString& dir, QString* error)
{
    const QString choice = QDir(baseConfigDir()).filePath(QStringLiteral("cloud/choice"));
    if (dir.isEmpty()) {
        if (QFile::exists(choice) && !QFile::remove(choice)) {
            if (error)
                *error = QCoreApplication::translate("Paths", "Cannot remove %1").arg(choice);
            return false;
        }
        return true;
    }
    if (!QFileInfo(dir).isDir()) {
        if (error)
            *error = QCoreApplication::translate("Paths", "Invalid path.");
        return false;
    }
    // writeSettingsFilesOnCloudForThe1stTime: la configuración de ahora, si allí no la hay
    const QDir from(configDir());
    const QDir to(dir);
    if (QFileInfo(from.absolutePath()) != QFileInfo(to.absolutePath())) {
        for (const QString& name : from.entryList({QStringLiteral("*.xml")}, QDir::Files)) {
            if (!to.exists(name))
                QFile::copy(from.filePath(name), to.filePath(name));
        }
        const QString udl = QStringLiteral("userDefineLangs");
        if (from.exists(udl) && !to.exists(udl)) {
            to.mkpath(udl);
            for (const QString& name : QDir(from.filePath(udl)).entryList(QDir::Files))
                QFile::copy(from.filePath(udl + QLatin1Char('/') + name), to.filePath(udl + QLatin1Char('/') + name));
        }
    }
    QDir().mkpath(QFileInfo(choice).absolutePath());
    QFile file(choice);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(QDir::cleanPath(dir).toUtf8()) < 0) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
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
