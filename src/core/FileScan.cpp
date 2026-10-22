// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/FileScan.h"

#include <QDir>
#include <QFileInfo>

namespace edita {

bool skipScanDirectory(const QString& name)
{
    return name.startsWith(QLatin1Char('.')) || name == QLatin1String("node_modules") || name == QLatin1String("__pycache__");
}

bool scanFiles(const QStringList& roots, const std::atomic_bool& cancel, const std::function<void(const QStringList&)>& chunk,
               const FileScanOptions& options)
{
    QStringList pending;
    int total = 0;
    bool complete = true;
    QStringList seenRoots;
    for (const QString& root : roots) {
        const QString clean = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
        // Una carpeta dentro de otra que ya se recorre, no otra vez
        bool nested = false;
        for (const QString& seen : seenRoots)
            nested = nested || clean == seen || clean.startsWith(seen + QLatin1Char('/'));
        if (nested)
            continue;
        seenRoots << clean;
        // Recorrido en anchura: primero lo que está más cerca de la raíz
        QStringList dirs{clean};
        while (!dirs.isEmpty() && complete) {
            if (cancel.load())
                return false;
            const QDir dir(dirs.takeFirst());
            const QFileInfoList entries =
                dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name | QDir::DirsLast);
            for (const QFileInfo& entry : entries) {
                if (entry.isSymLink())
                    continue;
                if (entry.isDir()) {
                    if (!skipScanDirectory(entry.fileName()))
                        dirs << entry.absoluteFilePath();
                    continue;
                }
                pending << entry.absoluteFilePath();
                if (++total >= options.maxFiles) {
                    complete = false;
                    break;
                }
                if (pending.size() >= options.chunkSize) {
                    chunk(pending);
                    pending.clear();
                }
            }
        }
        if (!complete)
            break;
    }
    if (!pending.isEmpty() && !cancel.load())
        chunk(pending);
    return complete;
}

} // namespace edita
