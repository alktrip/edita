// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Para tst_crashreport: instala el manejador de fallos y falla a propósito, como lo haría Edita
//   crash_helper <carpeta> segv|abort|overflow

#include "core/CrashReport.h"

#include <QCoreApplication>

#include <cstdlib>
#include <cstring>

namespace {

// Desbordamiento de pila: el manejador tiene que escribir el reporte con su pila aparte
int recurse(int depth)
{
    volatile char buffer[4096];
    buffer[0] = static_cast<char>(depth);
    return recurse(depth + 1) + buffer[0];
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc < 3)
        return 2;
    edita::crash_report::install(QString::fromLocal8Bit(argv[1]), QStringLiteral("9.9.9-prueba"));
    if (std::strcmp(argv[2], "abort") == 0)
        std::abort();
    if (std::strcmp(argv[2], "overflow") == 0)
        return recurse(0);
    volatile int* null = nullptr;
    return *null;
}
