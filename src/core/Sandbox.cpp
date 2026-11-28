// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/Sandbox.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace edita::sandbox {

namespace {

QString& storedShell()
{
    static QString shell;
    return shell;
}

// Comillas simples de sh
QString shellQuote(const QString& word)
{
    QString quoted = word;
    quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

} // namespace

bool isFlatpak()
{
    const QByteArray forced = qgetenv("EDITA_SANDBOX");
    if (forced == "flatpak")
        return true;
    if (forced == "none")
        return false;
    return QFileInfo::exists(QStringLiteral("/.flatpak-info"));
}

QStringList hostLauncher()
{
    // host-spawn va dentro del paquete (/app/bin): reenvía el tamaño de la terminal y las señales
    for (const QString& candidate : {QStringLiteral("/app/bin/host-spawn"), QStandardPaths::findExecutable(QStringLiteral("host-spawn"))}) {
        if (!candidate.isEmpty() && QFileInfo(candidate).isExecutable())
            return {candidate};
    }
    return {QStringLiteral("flatpak-spawn"), QStringLiteral("--host"), QStringLiteral("--watch-bus")};
}

QString visibleHostDir(const QString& hostDir)
{
    const QString dir = QDir::cleanPath(hostDir);
    for (const char* top : {"/usr", "/bin", "/sbin", "/lib", "/lib32", "/lib64", "/libx32", "/etc"}) {
        const QString prefix = QString::fromLatin1(top);
        if (dir == prefix || dir.startsWith(prefix + QLatin1Char('/')))
            return QStringLiteral("/run/host") + dir;
    }
    return dir;
}

int createBridge(const QString& dir, const QStringList& hostPath, const QStringList& launcher, const QSet<QString>& skip)
{
    QDir(dir).removeRecursively();
    if (!QDir().mkpath(dir))
        return 0;
    const QString runner = QDir(dir).filePath(QStringLiteral(".edita-host-run"));
    {
        QStringList words;
        for (const QString& word : launcher)
            words << shellQuote(word);
        QFile f(runner);
        if (!f.open(QIODevice::WriteOnly))
            return 0;
        // El nombre con el que se llamó (el del enlace) es el programa del sistema
        f.write(QStringLiteral("#!/bin/sh\nexec %1 \"${0##*/}\" \"$@\"\n").arg(words.join(QLatin1Char(' '))).toUtf8());
        f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
    int count = 0;
    QSet<QString> seen = skip;
    for (const QString& hostDir : hostPath) {
        const QDir visible(visibleHostDir(hostDir));
        if (hostDir.isEmpty() || !visible.exists())
            continue;
        for (const QFileInfo& file : visible.entryInfoList(QDir::Files | QDir::Executable | QDir::NoDotAndDotDot, QDir::Name)) {
            const QString name = file.fileName();
            // El primero del PATH gana, como en el sistema
            if (seen.contains(name) || name.startsWith(QLatin1Char('.')))
                continue;
            seen.insert(name);
            if (QFile::link(QStringLiteral(".edita-host-run"), QDir(dir).filePath(name)))
                ++count;
        }
    }
    return count;
}

void setup()
{
    if (!isFlatpak())
        return;
    // El PATH y la shell del sistema, desde fuera
    QProcess host;
    QStringList command = hostLauncher();
    const QString program = command.takeFirst();
    command << QStringLiteral("sh") << QStringLiteral("-c") << QStringLiteral("printf '%s\\n%s\\n' \"$PATH\" \"$SHELL\"");
    host.start(program, command);
    QStringList hostPath;
    if (host.waitForFinished(5000) && host.exitCode() == 0) {
        const QStringList lines = QString::fromLocal8Bit(host.readAllStandardOutput()).split(QLatin1Char('\n'));
        hostPath = lines.value(0).split(QLatin1Char(':'), Qt::SkipEmptyParts);
        storedShell() = lines.value(1).trimmed();
    }
    if (hostPath.isEmpty())
        hostPath = {QStringLiteral("/usr/local/bin"), QStringLiteral("/usr/bin"), QStringLiteral("/bin")};
    // Lo que tiene que seguir siendo de la sandbox: Edita, los lanzadores y abrir con el portal
    const QSet<QString> skip{QStringLiteral("edita"), QStringLiteral("host-spawn"), QStringLiteral("flatpak-spawn"),
                             QStringLiteral("xdg-open"), QStringLiteral("xdg-email"), QStringLiteral("flatpak")};
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty())
        runtime = QDir::tempPath();
    const QString dir = QDir(runtime).filePath(QStringLiteral("edita-host-bin"));
    if (createBridge(dir, hostPath, hostLauncher(), skip) > 0) {
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", QFile::encodeName(dir) + (path.isEmpty() ? QByteArray() : ':' + path));
    }
}

QString hostShell()
{
    return storedShell();
}

QStringList hostDictionaryDirs()
{
    return {QStringLiteral("/run/host/usr/share/hunspell"), QStringLiteral("/run/host/usr/share/myspell"),
            QStringLiteral("/run/host/usr/share/myspell/dicts")};
}

} // namespace edita::sandbox
