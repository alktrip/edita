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


#include "core/Backup.h"

#include "core/Paths.h"

#include <QDir>
#include <QFileInfo>

#include <cstdint>

namespace edita {

namespace {

QString timestamp(const QDateTime& when)
{
    return when.toString(QStringLiteral("yyyy-MM-dd_HHmmss"));
}

// Milisegundos entre 1601-01-01 (origen de FILETIME) y 1970-01-01
constexpr std::int64_t kFileTimeEpochOffsetMs = 11644473600000LL;

} // namespace

QString snapshotBackupDir()
{
    return QDir(paths::configDir()).filePath(QStringLiteral("backup"));
}

QString snapshotBackupPath(const QString& dir, const QString& documentName, const QDateTime& when)
{
    const QString base = QDir(dir).filePath(documentName + QLatin1Char('@') + timestamp(when));
    QString path = base;
    // Dos documentos con el mismo nombre en el mismo segundo no se pisan
    for (int n = 2; QFileInfo::exists(path); ++n)
        path = base + QLatin1Char('_') + QString::number(n);
    return path;
}

QString saveBackupPath(const QString& filePath, const Preferences& prefs, const QDateTime& when)
{
    if (prefs.backupOnSave == BackupOnSave::None)
        return {};
    const QFileInfo info(filePath);
    QDir dir = (prefs.useCustomBackupDir && !prefs.backupDir.isEmpty()) ? QDir(prefs.backupDir) : info.absoluteDir();
    if (prefs.backupOnSave == BackupOnSave::Verbose) {
        dir.setPath(dir.filePath(QStringLiteral("nppBackup")));
        return dir.filePath(info.fileName() + QLatin1Char('.') + timestamp(when) + QStringLiteral(".bak"));
    }
    return dir.filePath(info.fileName() + QStringLiteral(".bak"));
}

FileTimeParts toFileTime(const QDateTime& time)
{
    if (!time.isValid())
        return {};
    const auto ft = static_cast<std::uint64_t>((time.toMSecsSinceEpoch() + kFileTimeEpochOffsetMs) * 10000);
    return {static_cast<int>(static_cast<std::uint32_t>(ft & 0xFFFFFFFFu)), static_cast<int>(static_cast<std::uint32_t>(ft >> 32))};
}

QDateTime fromFileTime(FileTimeParts parts)
{
    if (parts.low == 0 && parts.high == 0)
        return {};
    const std::uint64_t ft = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(parts.high)) << 32)
        | static_cast<std::uint32_t>(parts.low);
    return QDateTime::fromMSecsSinceEpoch(static_cast<std::int64_t>(ft / 10000) - kFileTimeEpochOffsetMs);
}

} // namespace edita
