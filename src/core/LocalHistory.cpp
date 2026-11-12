// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/LocalHistory.h"

#include "core/Paths.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTimeZone>

#include <algorithm>

namespace edita {

namespace {

constexpr auto kPathFile = "path";
constexpr auto kSuffix = ".v";
constexpr auto kTimeFormat = "yyyyMMdd-HHmmss-zzz";

QString normalized(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

// «20261112-183005-120.v» o, si en el mismo milisegundo ya hay una, «…-120-1.v»
QDateTime timeOf(const QString& id)
{
    QDateTime when = QDateTime::fromString(id.left(19), QString::fromLatin1(kTimeFormat));
    when.setTimeZone(QTimeZone::UTC);
    return when;
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(bytes);
    return file.commit();
}

} // namespace

LocalHistory::LocalHistory(QString root)
    : _root(std::move(root))
{
}

QString LocalHistory::defaultRoot()
{
    return QDir(paths::baseConfigDir()).filePath(QStringLiteral("localHistory"));
}

QString LocalHistory::dirOf(const QString& path) const
{
    const QByteArray hash = QCryptographicHash::hash(normalized(path).toUtf8(), QCryptographicHash::Sha1).toHex();
    return QDir(_root).filePath(QString::fromLatin1(hash.left(20)));
}

bool LocalHistory::record(const QString& path, const QByteArray& bytes, const QDateTime& when,
                          const LocalHistoryLimits& limits)
{
    if (bytes.size() > limits.maxFileBytes)
        return false;
    const QList<Version> existing = versions(path);
    if (!existing.isEmpty() && existing.first().size == bytes.size() && read(path, existing.first()) == bytes)
        return false;
    const QString dir = dirOf(path);
    if (!QDir().mkpath(dir))
        return false;
    const QString pathFile = QDir(dir).filePath(QString::fromLatin1(kPathFile));
    if (!QFileInfo::exists(pathFile) && !writeFile(pathFile, normalized(path).toUtf8()))
        return false;
    QString id = when.toUTC().toString(QString::fromLatin1(kTimeFormat));
    for (int n = 1; QFileInfo::exists(QDir(dir).filePath(id + QLatin1String(kSuffix))); ++n)
        id = when.toUTC().toString(QString::fromLatin1(kTimeFormat)) + QLatin1Char('-') + QString::number(n);
    if (!writeFile(QDir(dir).filePath(id + QLatin1String(kSuffix)), bytes))
        return false;
    prune(path, limits, when);
    return true;
}

bool LocalHistory::recordFile(const QString& path, const QDateTime& when, const LocalHistoryLimits& limits)
{
    QFile file(path);
    if (file.size() > limits.maxFileBytes || !file.open(QIODevice::ReadOnly))
        return false;
    return record(path, file.readAll(), when, limits);
}

bool LocalHistory::hasHistory(const QString& path) const
{
    return !versions(path).isEmpty();
}

QList<LocalHistory::Version> LocalHistory::versions(const QString& path) const
{
    QList<Version> result;
    const QDir dir(dirOf(path));
    for (const QFileInfo& info : dir.entryInfoList({QStringLiteral("*") + QLatin1String(kSuffix)}, QDir::Files)) {
        Version v;
        v.id = info.completeBaseName();
        v.when = timeOf(v.id);
        v.size = info.size();
        if (v.when.isValid())
            result << v;
    }
    // El id ordena como la fecha (y el sufijo -1, -2… después de la del mismo milisegundo)
    std::sort(result.begin(), result.end(), [](const Version& a, const Version& b) {
        if (a.id.size() != b.id.size() && a.id.left(19) == b.id.left(19))
            return a.id.size() > b.id.size();
        return a.id > b.id;
    });
    return result;
}

QByteArray LocalHistory::read(const QString& path, const Version& version) const
{
    QFile file(QDir(dirOf(path)).filePath(version.id + QLatin1String(kSuffix)));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

void LocalHistory::rename(const QString& from, const QString& to)
{
    const QString source = dirOf(from);
    const QString target = dirOf(to);
    if (source == target || !QFileInfo(source).isDir())
        return;
    QDir().mkpath(target);
    for (const QFileInfo& info : QDir(source).entryInfoList({QStringLiteral("*") + QLatin1String(kSuffix)}, QDir::Files)) {
        const QString dest = QDir(target).filePath(info.fileName());
        if (!QFileInfo::exists(dest))
            QFile::rename(info.filePath(), dest);
    }
    QDir(source).removeRecursively();
    QFile::remove(QDir(target).filePath(QString::fromLatin1(kPathFile)));
    writeFile(QDir(target).filePath(QString::fromLatin1(kPathFile)), normalized(to).toUtf8());
}

void LocalHistory::removeVersion(const QString& path, const Version& version)
{
    QFile::remove(QDir(dirOf(path)).filePath(version.id + QLatin1String(kSuffix)));
    if (versions(path).isEmpty())
        clear(path);
}

void LocalHistory::clear(const QString& path)
{
    QDir(dirOf(path)).removeRecursively();
}

void LocalHistory::prune(const QString& path, const LocalHistoryLimits& limits, const QDateTime& now)
{
    const QList<Version> all = versions(path);
    const QDateTime oldest = now.addDays(-limits.maxDays);
    for (qsizetype i = 1; i < all.size(); ++i) { // la más nueva se queda siempre
        if (i >= limits.maxVersions || all[i].when < oldest)
            QFile::remove(QDir(dirOf(path)).filePath(all[i].id + QLatin1String(kSuffix)));
    }
}

qint64 LocalHistory::totalSize() const
{
    qint64 total = 0;
    for (QDirIterator it(_root, QDir::Files, QDirIterator::Subdirectories); it.hasNext();)
        total += it.nextFileInfo().size();
    return total;
}

} // namespace edita
