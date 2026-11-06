// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/CrashReport.h"

#include "core/Paths.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cxxabi.h>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>

namespace edita::crash_report {

namespace {

// Lo que el manejador necesita, preparado antes (en una señal no se puede reservar memoria)
char g_path[4096] = {};
char g_header[512] = {};
std::size_t g_headerLength = 0;
constexpr int kSignals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS};

const char* nameOf(int signal)
{
    switch (signal) {
    case SIGSEGV: return "SIGSEGV (segmentation fault)";
    case SIGABRT: return "SIGABRT (abort)";
    case SIGFPE: return "SIGFPE (arithmetic error)";
    case SIGILL: return "SIGILL (illegal instruction)";
    case SIGBUS: return "SIGBUS (bus error)";
    default: return "?";
    }
}

void writeAll(int fd, const char* text, std::size_t length)
{
    while (length > 0) {
        const ssize_t n = ::write(fd, text, length);
        if (n <= 0)
            return;
        text += n;
        length -= static_cast<std::size_t>(n);
    }
}

void writeText(int fd, const char* text)
{
    writeAll(fd, text, std::strlen(text));
}

// Un número en decimal, sin snprintf (no es seguro en una señal)
void writeNumber(int fd, long long value)
{
    char digits[24];
    int i = 0;
    const bool negative = value < 0;
    unsigned long long v = negative ? static_cast<unsigned long long>(-value) : static_cast<unsigned long long>(value);
    do {
        digits[i++] = static_cast<char>('0' + v % 10);
        v /= 10;
    } while (v && i < 22);
    if (negative)
        digits[i++] = '-';
    char out[24];
    for (int j = 0; j < i; ++j)
        out[j] = digits[i - 1 - j];
    writeAll(fd, out, static_cast<std::size_t>(i));
}

void writeReport(int signal)
{
    const int fd = ::open(g_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0)
        return;
    writeAll(fd, g_header, g_headerLength);
    writeText(fd, "Signal: ");
    writeText(fd, nameOf(signal));
    writeText(fd, "\nTime: ");
    writeNumber(fd, static_cast<long long>(std::time(nullptr)));
    writeText(fd, "\nBacktrace:\n");
    void* frames[64];
    const int count = ::backtrace(frames, 64);
    ::backtrace_symbols_fd(frames, count, fd);
    ::close(fd);
}

void handler(int signal)
{
    writeReport(signal);
    // SA_RESETHAND ya puso el manejador por defecto: el proceso termina como sin él (con su
    // volcado de memoria, si el sistema lo guarda)
    ::raise(signal);
}

} // namespace

QString defaultDirectory()
{
    return QDir(paths::configDir()).filePath(QStringLiteral("crash"));
}

QString reportPath(const QString& directory)
{
    return QDir(directory).filePath(QStringLiteral("last-crash.txt"));
}

void install(const QString& directory, const QString& version)
{
    QDir().mkpath(directory);
    const QByteArray path = QFile::encodeName(reportPath(directory));
    if (path.size() >= static_cast<qsizetype>(sizeof(g_path)))
        return;
    std::memcpy(g_path, path.constData(), static_cast<std::size_t>(path.size()) + 1);
    const QByteArray header = QStringLiteral("Edita %1\n").arg(version).toUtf8();
    g_headerLength = std::min(sizeof(g_header) - 1, static_cast<std::size_t>(header.size()));
    std::memcpy(g_header, header.constData(), g_headerLength);
    // backtrace carga libgcc la primera vez (reserva memoria): mejor ahora que en la señal
    void* warmUp[2];
    ::backtrace(warmUp, 2);
    // Una pila aparte, para poder escribir el reporte aunque el fallo sea un desbordamiento de pila
    static char alternate[64 * 1024];
    stack_t stack{};
    stack.ss_sp = alternate;
    stack.ss_size = sizeof(alternate);
    ::sigaltstack(&stack, nullptr);
    struct sigaction action{};
    action.sa_handler = handler;
    action.sa_flags = static_cast<int>(SA_RESETHAND | SA_ONSTACK);
    sigemptyset(&action.sa_mask);
    for (const int signal : kSignals)
        ::sigaction(signal, &action, nullptr);
}

QString pending(const QString& directory)
{
    QFile file(reportPath(directory));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

void clear(const QString& directory)
{
    QFile::remove(reportPath(directory));
}

QString readable(const QString& report)
{
    // «/ruta/edita(_ZN5edita10MainWindow4saveEv+0x1c) [0x55…]» → «edita::MainWindow::save()»
    static const QRegularExpression mangled(QStringLiteral("\\((_Z[A-Za-z0-9_]+)\\+"));
    QString out = report;
    QRegularExpressionMatchIterator it = mangled.globalMatch(report);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        int status = 0;
        char* name = abi::__cxa_demangle(m.captured(1).toLatin1().constData(), nullptr, nullptr, &status);
        if (status == 0 && name)
            out.replace(m.captured(1), QString::fromLatin1(name));
        std::free(name);
    }
    return out;
}

QString signalName(const QString& report)
{
    static const QRegularExpression re(QStringLiteral("^Signal: (SIG[A-Z]+)"), QRegularExpression::MultilineOption);
    return re.match(report).captured(1);
}

void writeReportForTesting(int signal)
{
    writeReport(signal);
}

} // namespace edita::crash_report
