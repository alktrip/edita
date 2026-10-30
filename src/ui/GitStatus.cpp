// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/GitStatus.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace edita {

namespace {

QString clean(const QString& path)
{
    return QDir::cleanPath(path);
}

GitStatus::State stateFor(char x, char y)
{
    using State = GitStatus::State;
    if (x == '?' && y == '?')
        return State::Untracked;
    if (x == '!' && y == '!')
        return State::Ignored;
    // Conflictos: los dos lados tocaron el archivo (DD, AU, UD, UA, DU, AA, UU)
    if (x == 'U' || y == 'U' || (x == 'A' && y == 'A') || (x == 'D' && y == 'D'))
        return State::Conflict;
    if (x == 'R' || x == 'C')
        return y == 'M' ? State::Modified : State::Renamed;
    if (y == 'D' || x == 'D')
        return State::Deleted;
    if (y == 'M' || x == 'M' || y == 'T' || x == 'T')
        return State::Modified;
    if (x == 'A')
        return State::Added;
    return State::None;
}

} // namespace

GitStatus::GitStatus(QObject* parent)
    : QObject(parent)
{
}

GitStatus::~GitStatus()
{
    for (QProcess* process : findChildren<QProcess*>()) {
        process->disconnect(this);
        process->kill();
        process->waitForFinished(500);
    }
}

std::vector<std::pair<QString, GitStatus::State>> GitStatus::parsePorcelain(const QByteArray& output, const QString& top)
{
    std::vector<std::pair<QString, State>> entries;
    const QList<QByteArray> parts = output.split('\0');
    for (qsizetype i = 0; i < parts.size(); ++i) {
        const QByteArray& entry = parts[i];
        if (entry.size() < 4)
            continue;
        const char x = entry[0];
        const char y = entry[1];
        QString path = QString::fromUtf8(entry.mid(3));
        if (x == 'R' || x == 'C')
            ++i; // con -z, la ruta de antes del cambio de nombre va en la entrada siguiente
        const bool isDir = path.endsWith(QLatin1Char('/'));
        path = clean(QDir(top).filePath(path));
        if (isDir)
            path += QLatin1Char('/'); // (las carpetas ignoradas o sin seguimiento acaban en /)
        const State state = stateFor(x, y);
        if (state != State::None)
            entries.emplace_back(path, state);
    }
    return entries;
}

void GitStatus::refresh(const QStringList& roots)
{
    // Lo que estuviera en marcha ya no sirve
    for (QProcess* process : findChildren<QProcess*>()) {
        process->disconnect(this);
        connect(process, &QProcess::finished, process, &QObject::deleteLater);
        process->kill();
    }
    const quint64 generation = ++_generation;
    _nextFiles.clear();
    _nextDirs.clear();
    _nextIgnored.clear();
    _nextBranches.clear();
    _pending = static_cast<int>(roots.size());
    if (roots.isEmpty()) {
        finishRoot(generation, {}, {}, {}, {});
        return;
    }
    for (const QString& root : roots) {
        // 1) La raíz del repositorio y la rama; 2) el estado de todo el repositorio
        auto* info = new QProcess(this);
        connect(info, &QProcess::finished, this, [this, info, generation, root](int code, QProcess::ExitStatus status) {
            info->deleteLater();
            if (generation != _generation)
                return;
            const QStringList lines = QString::fromUtf8(info->readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            if (status != QProcess::NormalExit || code != 0 || lines.size() < 2) {
                finishRoot(generation, root, {}, {}, {}); // no es un repositorio
                return;
            }
            const QString top = clean(lines[0].trimmed());
            const QString branch = lines[1].trimmed();
            auto* st = new QProcess(this);
            connect(st, &QProcess::finished, this, [this, st, generation, root, top, branch](int c, QProcess::ExitStatus s) {
                st->deleteLater();
                if (generation == _generation)
                    finishRoot(generation, root, top, branch, s == QProcess::NormalExit && c == 0 ? st->readAllStandardOutput() : QByteArray());
            });
            connect(st, &QProcess::errorOccurred, this, [this, st, generation, root](QProcess::ProcessError e) {
                if (e == QProcess::FailedToStart && generation == _generation) {
                    st->deleteLater();
                    finishRoot(generation, root, {}, {}, {});
                }
            });
            st->start(QStringLiteral("git"), {QStringLiteral("-C"), top, QStringLiteral("status"), QStringLiteral("--porcelain=v1"),
                                              QStringLiteral("-z"), QStringLiteral("--untracked-files=all"),
                                              QStringLiteral("--ignored=matching")});
        });
        connect(info, &QProcess::errorOccurred, this, [this, info, generation, root](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart && generation == _generation) { // sin git
                info->deleteLater();
                finishRoot(generation, root, {}, {}, {});
            }
        });
        info->start(QStringLiteral("git"), {QStringLiteral("-C"), root, QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel"),
                                            QStringLiteral("--abbrev-ref"), QStringLiteral("HEAD")});
    }
}

void GitStatus::finishRoot(quint64 generation, const QString& root, const QString& top, const QString& branch, const QByteArray& status)
{
    if (generation != _generation)
        return;
    if (!root.isEmpty() && !top.isEmpty()) {
        _nextBranches.insert(clean(root), branch);
        for (const auto& [path, state] : parsePorcelain(status, top)) {
            if (state == State::Ignored) {
                _nextIgnored.insert(path.endsWith(QLatin1Char('/')) ? path.chopped(1) : path);
                continue;
            }
            const QString file = path.endsWith(QLatin1Char('/')) ? path.chopped(1) : path;
            _nextFiles.insert(file, state);
            // Las carpetas de encima, hasta la raíz del repositorio: el estado más importante
            for (QString dir = QFileInfo(file).path(); dir.size() >= top.size() && dir != QLatin1String("/"); dir = QFileInfo(dir).path()) {
                State& current = _nextDirs[dir];
                if (static_cast<int>(state) > static_cast<int>(current))
                    current = state;
                if (dir == top)
                    break;
            }
        }
    }
    if (--_pending > 0)
        return;
    _pending = 0;
    _files = std::move(_nextFiles);
    _dirs = std::move(_nextDirs);
    _ignoredDirs = std::move(_nextIgnored);
    _branches = std::move(_nextBranches);
    emit updated();
}

GitStatus::State GitStatus::stateOf(const QString& path) const
{
    const QString p = clean(path);
    if (const auto it = _files.constFind(p); it != _files.constEnd())
        return *it;
    if (const auto it = _dirs.constFind(p); it != _dirs.constEnd())
        return *it;
    // Dentro de una carpeta ignorada (o sin seguimiento: git da solo la carpeta con --ignored)
    for (QString dir = p; !dir.isEmpty() && dir != QLatin1String("/"); dir = QFileInfo(dir).path()) {
        if (_ignoredDirs.contains(dir))
            return State::Ignored;
        if (QFileInfo(dir).path() == dir)
            break;
    }
    return State::None;
}

QString GitStatus::badge(State state)
{
    switch (state) {
    case State::Modified: return QStringLiteral("M");
    case State::Added: return QStringLiteral("A");
    case State::Untracked: return QStringLiteral("U");
    case State::Renamed: return QStringLiteral("R");
    case State::Deleted: return QStringLiteral("D");
    case State::Conflict: return QStringLiteral("!");
    case State::Ignored:
    case State::None: break;
    }
    return {};
}

QString GitStatus::description(State state)
{
    switch (state) {
    case State::Modified: return tr("Modified");
    case State::Added: return tr("Added to the index");
    case State::Untracked: return tr("Untracked");
    case State::Renamed: return tr("Renamed");
    case State::Deleted: return tr("Deleted");
    case State::Conflict: return tr("Conflict");
    case State::Ignored: return tr("Ignored");
    case State::None: break;
    }
    return {};
}

QColor GitStatus::color(State state, bool dark)
{
    // Los de VS Code
    switch (state) {
    case State::Modified:
    case State::Renamed: return dark ? QColor(0xE2, 0xC0, 0x8D) : QColor(0x89, 0x5D, 0x03);
    case State::Added:
    case State::Untracked: return dark ? QColor(0x73, 0xC9, 0x91) : QColor(0x58, 0x7C, 0x0C);
    case State::Deleted:
    case State::Conflict: return dark ? QColor(0xF1, 0x4C, 0x4C) : QColor(0xC7, 0x1E, 0x1E);
    case State::Ignored: return dark ? QColor(0x7A, 0x7A, 0x7A) : QColor(0x9A, 0x9A, 0x9A);
    case State::None: break;
    }
    return {};
}

} // namespace edita
