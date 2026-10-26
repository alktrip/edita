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

#include "core/PluginInstaller.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#ifdef EDITA_HAVE_LIBZIP
#include <zip.h>
#endif

namespace edita::plugin_installer {

namespace {

#ifdef EDITA_HAVE_LIBZIP
// ZIP_LENGTH_TO_END es de libzip 1.10; antes, -1 ya significaba «hasta el final»
// (Ubuntu 22.04 trae la 1.7)
#ifdef ZIP_LENGTH_TO_END
constexpr zip_int64_t kZipLengthToEnd = ZIP_LENGTH_TO_END;
#else
constexpr zip_int64_t kZipLengthToEnd = -1;
#endif
#endif

void setError(QString* error, const QString& text)
{
    if (error)
        *error = text;
}

#ifdef EDITA_HAVE_LIBZIP
// Una entrada del .zip no puede salir de la carpeta de destino
bool isSafeEntry(const QString& name)
{
    if (name.isEmpty() || name.startsWith(QLatin1Char('/')) || name.contains(QLatin1Char('\\')) || name.contains(QLatin1Char(':')))
        return false;
    for (const QString& part : name.split(QLatin1Char('/'))) {
        if (part == QLatin1String(".."))
            return false;
    }
    return true;
}
#endif

} // namespace

QString sha256Hex(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool zipSupported()
{
#ifdef EDITA_HAVE_LIBZIP
    return true;
#else
    return false;
#endif
}

bool extractZip(const QByteArray& zipData, const QString& dir, QString* error)
{
#ifdef EDITA_HAVE_LIBZIP
    zip_error_t zerr;
    zip_error_init(&zerr);
    zip_source_t* source = zip_source_buffer_create(zipData.constData(), static_cast<zip_uint64_t>(zipData.size()), 0, &zerr);
    if (!source) {
        setError(error, QString::fromUtf8(zip_error_strerror(&zerr)));
        zip_error_fini(&zerr);
        return false;
    }
    zip_t* archive = zip_open_from_source(source, ZIP_RDONLY, &zerr);
    if (!archive) {
        setError(error, QCoreApplication::translate("PluginInstaller", "Not a valid .zip file: %1").arg(QString::fromUtf8(zip_error_strerror(&zerr))));
        zip_source_free(source);
        zip_error_fini(&zerr);
        return false;
    }
    zip_error_fini(&zerr);

    bool ok = true;
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    for (zip_int64_t i = 0; ok && i < count; ++i) {
        zip_stat_t st;
        if (zip_stat_index(archive, static_cast<zip_uint64_t>(i), 0, &st) != 0 || !st.name)
            continue;
        const QString name = QString::fromUtf8(st.name);
        if (!isSafeEntry(name)) {
            setError(error, QCoreApplication::translate("PluginInstaller", "Path not allowed in the package: %1").arg(name));
            ok = false;
            break;
        }
        const QString target = QDir(dir).filePath(name);
        if (name.endsWith(QLatin1Char('/'))) {
            QDir().mkpath(target);
            continue;
        }
        QDir().mkpath(QFileInfo(target).absolutePath());
        zip_file_t* entry = zip_fopen_index(archive, static_cast<zip_uint64_t>(i), 0);
        QFile out(target);
        if (!entry || !out.open(QIODevice::WriteOnly)) {
            setError(error, QCoreApplication::translate("PluginInstaller", "Could not extract %1").arg(name));
            if (entry)
                zip_fclose(entry);
            ok = false;
            break;
        }
        char buffer[65536];
        zip_int64_t n = 0;
        while ((n = zip_fread(entry, buffer, sizeof buffer)) > 0)
            out.write(buffer, n);
        zip_fclose(entry);
        if (n < 0) {
            setError(error, QCoreApplication::translate("PluginInstaller", "Damaged package: %1").arg(name));
            ok = false;
        }
    }
    zip_discard(archive);
    return ok;
#else
    Q_UNUSED(zipData)
    Q_UNUSED(dir)
    setError(error, QCoreApplication::translate("PluginInstaller", "Edita was built without .zip support (libzip)"));
    return false;
#endif
}

bool installPackage(const QByteArray& zip, const QString& folderName, const QString& expectedId, const QString& pluginsRoot,
                    bool clean, QString* error)
{
    if (folderName.isEmpty() || folderName.contains(QLatin1Char('/')) || folderName.startsWith(QLatin1Char('.'))) {
        setError(error, QCoreApplication::translate("PluginInstaller", "Invalid folder name: %1").arg(folderName));
        return false;
    }
    // gup comprueba que el paquete descargado es el de la lista
    if (!expectedId.isEmpty() && sha256Hex(zip).compare(expectedId, Qt::CaseInsensitive) != 0) {
        setError(error, QCoreApplication::translate("PluginInstaller", "The package SHA-256 does not match the one in the plugin list (%1).").arg(folderName));
        return false;
    }
    const QString destination = QDir(pluginsRoot).filePath(folderName);
    if (QFileInfo::exists(destination) && !clean) {
        setError(error, QCoreApplication::translate("PluginInstaller", "There is already a folder %1").arg(destination));
        return false;
    }
    QDir().mkpath(pluginsRoot);
    QTemporaryDir staging(QDir(pluginsRoot).filePath(QStringLiteral(".edita-install-XXXXXX")));
    if (!staging.isValid()) {
        setError(error, QCoreApplication::translate("PluginInstaller", "Could not write to %1").arg(pluginsRoot));
        return false;
    }
    if (!extractZip(zip, staging.path(), error))
        return false;

    // El .so en la raíz del paquete o dentro de una carpeta con su nombre
    QString content = staging.path();
    const QString library = folderName + QStringLiteral(".so");
    if (!QFileInfo::exists(QDir(content).filePath(library))) {
        const QString nested = QDir(content).filePath(folderName);
        if (QFileInfo::exists(QDir(nested).filePath(library)))
            content = nested;
        else {
            setError(error, QCoreApplication::translate("PluginInstaller", "The package does not contain %1").arg(library));
            return false;
        }
    }
    if (QFileInfo::exists(destination) && !QDir(destination).removeRecursively()) {
        setError(error, QCoreApplication::translate("PluginInstaller", "Could not remove the previous version of %1").arg(folderName));
        return false;
    }
    if (!QDir().rename(content, destination)) {
        setError(error, QCoreApplication::translate("PluginInstaller", "Could not create %1").arg(destination));
        return false;
    }
    return true;
}

bool createZip(const QString& zipPath, const std::vector<std::pair<QString, QString>>& files, QString* error)
{
#ifdef EDITA_HAVE_LIBZIP
    QFile::remove(zipPath);
    int code = 0;
    zip_t* archive = zip_open(QFile::encodeName(zipPath).constData(), ZIP_CREATE | ZIP_TRUNCATE, &code);
    if (!archive) {
        setError(error, QCoreApplication::translate("PluginInstaller", "Could not create %1").arg(zipPath));
        return false;
    }
    for (const auto& [name, path] : files) {
        zip_source_t* source = zip_source_file(archive, QFile::encodeName(path).constData(), 0, kZipLengthToEnd);
        if (!source || zip_file_add(archive, name.toUtf8().constData(), source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8) < 0) {
            if (source)
                zip_source_free(source);
            setError(error, QCoreApplication::translate("PluginInstaller", "Could not add %1 to the package").arg(path));
            zip_discard(archive);
            return false;
        }
    }
    if (zip_close(archive) != 0) {
        setError(error, QCoreApplication::translate("PluginInstaller", "Could not write %1").arg(zipPath));
        zip_discard(archive);
        return false;
    }
    return true;
#else
    Q_UNUSED(zipPath)
    Q_UNUSED(files)
    setError(error, QCoreApplication::translate("PluginInstaller", "Edita was built without .zip support (libzip)"));
    return false;
#endif
}

bool removePlugin(const QString& pluginsRoot, const QString& folderName, QString* error)
{
    const QString path = QDir(pluginsRoot).filePath(folderName);
    if (folderName.isEmpty() || !QFileInfo(path).isDir()) {
        setError(error, QCoreApplication::translate("PluginInstaller", "%1 does not exist").arg(path));
        return false;
    }
    if (!QDir(path).removeRecursively()) {
        setError(error, QCoreApplication::translate("PluginInstaller", "Could not delete %1").arg(path));
        return false;
    }
    return true;
}

bool movePlugin(const QString& fromRoot, const QString& toRoot, const QString& folderName, QString* error)
{
    const QString from = QDir(fromRoot).filePath(folderName);
    const QString to = QDir(toRoot).filePath(folderName);
    if (folderName.isEmpty() || !QFileInfo(from).isDir()) {
        setError(error, QCoreApplication::translate("PluginInstaller", "%1 does not exist").arg(from));
        return false;
    }
    QDir().mkpath(toRoot);
    // gup sobrescribe si ya existe en el destino
    if (QFileInfo::exists(to))
        QDir(to).removeRecursively();
    if (!QDir().rename(from, to)) {
        setError(error, QCoreApplication::translate("PluginInstaller", "Could not move %1 to %2").arg(from, toRoot));
        return false;
    }
    return true;
}

} // namespace edita::plugin_installer
