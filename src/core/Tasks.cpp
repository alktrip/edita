// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/Tasks.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace edita::tasks {

namespace {

const QStringList kMakefiles{QStringLiteral("GNUmakefile"), QStringLiteral("makefile"), QStringLiteral("Makefile")};
const QStringList kJustfiles{QStringLiteral("justfile"), QStringLiteral("Justfile"), QStringLiteral(".justfile")};
const QStringList kPythonMarkers{QStringLiteral("pyproject.toml"), QStringLiteral("pytest.ini"), QStringLiteral("setup.py"),
                                 QStringLiteral("setup.cfg"), QStringLiteral("tox.ini")};

QStringList rootMarkers()
{
    QStringList markers = kMakefiles + kJustfiles + kPythonMarkers;
    markers << QStringLiteral("CMakeLists.txt") << QStringLiteral("package.json") << QStringLiteral("Cargo.toml")
            << QStringLiteral("go.mod") << QStringLiteral("meson.build") << QStringLiteral(".git");
    return markers;
}

QString readText(const QString& path)
{
    QFile file(path);
    if (file.size() > 2 * 1024 * 1024 || !file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

QString firstExisting(const QDir& dir, const QStringList& names)
{
    for (const QString& name : names) {
        if (QFileInfo(dir.filePath(name)).isFile())
            return name;
    }
    return {};
}

// Las recetas de un justfile: «nombre args: dependencias» al principio de la línea
QStringList justRecipes(const QString& text)
{
    static const QRegularExpression recipe(QStringLiteral("^@?([A-Za-z0-9_][A-Za-z0-9_-]*)(?:\\s+[^:\\n]*?)?:(?!=)"),
                                           QRegularExpression::MultilineOption);
    QStringList names;
    for (auto it = recipe.globalMatch(text); it.hasNext();) {
        const QString name = it.next().captured(1);
        if (!names.contains(name) && name != QLatin1String("set") && name != QLatin1String("export") && name != QLatin1String("alias"))
            names << name;
    }
    return names;
}

} // namespace

QString findProjectRoot(const QString& start)
{
    const QFileInfo info(start);
    QDir dir(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
    const QStringList markers = rootMarkers();
    while (true) {
        for (const QString& marker : markers) {
            if (QFileInfo::exists(dir.filePath(marker)))
                return dir.absolutePath();
        }
        if (!dir.cdUp())
            return {};
    }
}

QStringList makeTargets(const QString& makefileText)
{
    // «objetivo: dependencias», sin «:=» (asignación), patrones (%), variables ($) ni los
    // especiales que empiezan por punto
    static const QRegularExpression rule(QStringLiteral("^([A-Za-z0-9_][A-Za-z0-9_./-]*(?:[ \\t]+[A-Za-z0-9_][A-Za-z0-9_./-]*)*)[ \\t]*:(?![:=])"),
                                         QRegularExpression::MultilineOption);
    QStringList targets;
    for (auto it = rule.globalMatch(makefileText); it.hasNext();) {
        for (const QString& target : it.next().captured(1).split(QRegularExpression(QStringLiteral("[ \\t]+")), Qt::SkipEmptyParts)) {
            if (!targets.contains(target))
                targets << target;
        }
    }
    return targets;
}

QList<Task> detectTasks(const QString& root)
{
    QList<Task> result;
    if (root.isEmpty())
        return result;
    const QDir dir(root);
    auto add = [&](const QString& command, const QString& source) {
        result << Task{command, command, dir.absolutePath(), source};
    };

    if (const QString makefile = firstExisting(dir, kMakefiles); !makefile.isEmpty()) {
        add(QStringLiteral("make"), makefile);
        const QStringList targets = makeTargets(readText(dir.filePath(makefile)));
        for (qsizetype i = 0; i < targets.size() && i < 40; ++i)
            add(QStringLiteral("make ") + targets[i], makefile);
    }
    if (QFileInfo(dir.filePath(QStringLiteral("CMakeLists.txt"))).isFile()) {
        QString build;
        for (const QString& candidate : {QStringLiteral("build"), QStringLiteral("cmake-build-debug"), QStringLiteral("cmake-build-release"),
                                         QStringLiteral("out/build")}) {
            if (QFileInfo::exists(dir.filePath(candidate + QStringLiteral("/CMakeCache.txt")))) {
                build = candidate;
                break;
            }
        }
        if (build.isEmpty()) {
            add(QStringLiteral("cmake -S . -B build"), QStringLiteral("CMakeLists.txt"));
            build = QStringLiteral("build");
        }
        add(QStringLiteral("cmake --build ") + build, QStringLiteral("CMakeLists.txt"));
        add(QStringLiteral("ctest --test-dir ") + build + QStringLiteral(" --output-on-failure"), QStringLiteral("CMakeLists.txt"));
    }
    if (QFileInfo(dir.filePath(QStringLiteral("package.json"))).isFile()) {
        QString runner = QStringLiteral("npm run");
        if (QFileInfo::exists(dir.filePath(QStringLiteral("pnpm-lock.yaml"))))
            runner = QStringLiteral("pnpm run");
        else if (QFileInfo::exists(dir.filePath(QStringLiteral("yarn.lock"))))
            runner = QStringLiteral("yarn run");
        else if (QFileInfo::exists(dir.filePath(QStringLiteral("bun.lockb"))) || QFileInfo::exists(dir.filePath(QStringLiteral("bun.lock"))))
            runner = QStringLiteral("bun run");
        const QJsonObject scripts = QJsonDocument::fromJson(readText(dir.filePath(QStringLiteral("package.json"))).toUtf8())
                                        .object()
                                        .value(QStringLiteral("scripts"))
                                        .toObject();
        for (const QString& name : scripts.keys())
            add(runner + QLatin1Char(' ') + name, QStringLiteral("package.json"));
    }
    if (QFileInfo(dir.filePath(QStringLiteral("Cargo.toml"))).isFile()) {
        for (const char* sub : {"build", "test", "run", "check", "clippy"})
            add(QStringLiteral("cargo ") + QLatin1String(sub), QStringLiteral("Cargo.toml"));
    }
    if (QFileInfo(dir.filePath(QStringLiteral("go.mod"))).isFile()) {
        for (const char* command : {"go build ./...", "go test ./...", "go vet ./...", "go run ."})
            add(QLatin1String(command), QStringLiteral("go.mod"));
    }
    if (const QString marker = firstExisting(dir, kPythonMarkers); !marker.isEmpty())
        add(QStringLiteral("python3 -m pytest"), marker);
    if (QFileInfo(dir.filePath(QStringLiteral("meson.build"))).isFile()) {
        if (QFileInfo::exists(dir.filePath(QStringLiteral("builddir/meson-private")))) {
            add(QStringLiteral("meson compile -C builddir"), QStringLiteral("meson.build"));
            add(QStringLiteral("meson test -C builddir"), QStringLiteral("meson.build"));
        } else {
            add(QStringLiteral("meson setup builddir"), QStringLiteral("meson.build"));
        }
    }
    if (const QString justfile = firstExisting(dir, kJustfiles); !justfile.isEmpty()) {
        for (const QString& recipe : justRecipes(readText(dir.filePath(justfile))))
            add(QStringLiteral("just ") + recipe, justfile);
    }
    return result;
}

QString stripAnsi(const QString& text)
{
    // CSI (colores, cursor), OSC (títulos, enlaces) y los escapes de un carácter
    static const QRegularExpression ansi(QStringLiteral("\\x1B(?:\\[[0-?]*[ -/]*[@-~]|\\][^\\x07\\x1B]*(?:\\x07|\\x1B\\\\)|[@-Z\\\\-_])"));
    QString result = text;
    result.remove(ansi);
    return result;
}

ProblemParser::ProblemParser(QString cwd)
    : _cwd(std::move(cwd))
{
}

std::optional<QString> ProblemParser::resolve(const QString& path) const
{
    QString p = path.trimmed();
    if (p.isEmpty() || p.contains(QLatin1String("://")))
        return std::nullopt;
    const QFileInfo info(QDir::isAbsolutePath(p) ? p : QDir(_cwd).filePath(p));
    if (!info.isFile())
        return std::nullopt;
    return QDir::cleanPath(info.absoluteFilePath());
}

std::optional<Problem> ProblemParser::feed(const QString& rawLine)
{
    const QString line = stripAnsi(rawLine);
    auto severityOf = [](const QString& word) {
        const QString w = word.toLower();
        if (w.contains(QLatin1String("warning")))
            return 2;
        if (w == QLatin1String("note") || w == QLatin1String("info") || w == QLatin1String("help"))
            return 3;
        return 1;
    };

    // rustc: la cabecera y después la flecha con la posición
    static const QRegularExpression rustHead(QStringLiteral("^(error|warning)(?:\\[([A-Z]\\d+)\\])?: (.+)$"));
    static const QRegularExpression rustArrow(QStringLiteral("^\\s*--> (.+):(\\d+):(\\d+)$"));
    if (const auto m = rustHead.match(line); m.hasMatch()) {
        _pendingRust = Problem{QString(), 0, 0, severityOf(m.captured(1)), m.captured(3), m.captured(2)};
        return std::nullopt;
    }
    if (_pendingRust) {
        if (const auto m = rustArrow.match(line); m.hasMatch()) {
            Problem p = *_pendingRust;
            _pendingRust.reset();
            if (const auto file = resolve(m.captured(1))) {
                p.file = *file;
                p.line = m.captured(2).toInt();
                p.column = m.captured(3).toInt();
                return p;
            }
            return std::nullopt;
        }
        if (line.trimmed().isEmpty())
            _pendingRust.reset();
    }

    // Python: «  File "x.py", line 3, in f» … y el error al final, sin sangría
    static const QRegularExpression pyFrame(QStringLiteral("^\\s*File \"(.+)\", line (\\d+)"));
    static const QRegularExpression pyError(QStringLiteral("^([A-Za-z_][\\w.]*(?:Error|Exception|Exit|Interrupt|Warning)\\b.*)$"));
    if (const auto m = pyFrame.match(line); m.hasMatch()) {
        if (const auto file = resolve(m.captured(1)))
            _pendingPython = Problem{*file, m.captured(2).toInt(), 0, 1, QString(), QString()};
        return std::nullopt;
    }
    if (_pendingPython && !line.startsWith(QLatin1Char(' ')) && !line.isEmpty()) {
        Problem p = *_pendingPython;
        _pendingPython.reset();
        if (const auto m = pyError.match(line); m.hasMatch()) {
            p.message = m.captured(1);
            p.severity = line.contains(QLatin1String("Warning")) ? 2 : 1;
            return p;
        }
    }

    // tsc: «src/a.ts(3,5): error TS2322: mensaje»
    static const QRegularExpression tsc(QStringLiteral("^(.+?)\\((\\d+),(\\d+)\\): (error|warning) (TS\\d+): (.+)$"));
    if (const auto m = tsc.match(line); m.hasMatch()) {
        if (const auto file = resolve(m.captured(1)))
            return Problem{*file, m.captured(2).toInt(), m.captured(3).toInt(), severityOf(m.captured(4)), m.captured(6), m.captured(5)};
    }

    // GCC, Clang, Go, ESLint (unix), pytest…: «archivo:línea[:col]: [error|warning|note:] mensaje»
    static const QRegularExpression gcc(
        QStringLiteral("^(.+?):(\\d+)(?::(\\d+))?:\\s*(?:(fatal error|error|warning|note|info)\\s*:\\s*)?(.+)$"));
    if (const auto m = gcc.match(line); m.hasMatch()) {
        // Sin columna ni «error:», solo si parece un mensaje (pytest: «a.py:12: AssertionError»)
        if (const auto file = resolve(m.captured(1))) {
            const QString kind = m.captured(4);
            return Problem{*file, m.captured(2).toInt(), m.captured(3).toInt(), kind.isEmpty() ? 1 : severityOf(kind),
                           m.captured(5).trimmed(), QString()};
        }
    }
    return std::nullopt;
}

} // namespace edita::tasks
