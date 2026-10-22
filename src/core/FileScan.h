// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Los archivos de las carpetas de trabajo, para «Ir a archivo»: se recorren en otro
// hilo y se entregan por tandas. Sin carpetas ocultas (.git, .cache…), node_modules ni
// __pycache__, sin seguir enlaces y con un máximo de archivos.

#pragma once

#include <QStringList>

#include <atomic>
#include <functional>

namespace edita {

struct FileScanOptions
{
    int maxFiles = 50000;
    int chunkSize = 500;
};

// Devuelve false si se llegó al máximo (la lista está incompleta)
bool scanFiles(const QStringList& roots, const std::atomic_bool& cancel, const std::function<void(const QStringList&)>& chunk,
               const FileScanOptions& options = {});

// ¿Se salta esta carpeta al recorrer?
bool skipScanDirectory(const QString& name);

} // namespace edita
