// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/Formatters.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace edita {

const std::vector<FormatterSpec>& knownFormatters()
{
    static const std::vector<FormatterSpec> specs = {
        {QStringLiteral("clang-format"), QStringLiteral("C, C++, Objective-C, Java, C#, CUDA"),
         {QStringLiteral("c"), QStringLiteral("cpp"), QStringLiteral("objective-c"), QStringLiteral("objective-cpp"),
          QStringLiteral("java"), QStringLiteral("csharp"), QStringLiteral("cuda")},
         {QStringLiteral("clang-format --assume-filename=$(FILE_PATH)")}, {}},
        {QStringLiteral("prettier"), QStringLiteral("JavaScript, TypeScript, CSS, SCSS, Less, HTML, Markdown, YAML"),
         {QStringLiteral("javascript"), QStringLiteral("javascriptreact"), QStringLiteral("typescript"),
          QStringLiteral("typescriptreact"), QStringLiteral("css"), QStringLiteral("scss"), QStringLiteral("less"),
          QStringLiteral("html"), QStringLiteral("markdown"), QStringLiteral("yaml")},
         {QStringLiteral("prettier --stdin-filepath $(FILE_PATH)")}, {}},
        {QStringLiteral("json"), QStringLiteral("JSON"), {QStringLiteral("json"), QStringLiteral("jsonc")},
         {QStringLiteral("prettier --stdin-filepath $(FILE_PATH)"), QStringLiteral("jq --indent $(TAB_SIZE) .")}, {}},
        {QStringLiteral("python"), QStringLiteral("Python"), {QStringLiteral("python")},
         {QStringLiteral("ruff format --stdin-filename $(FILE_PATH) -"), QStringLiteral("black -q --stdin-filename $(FILE_PATH) -")}, {}},
        {QStringLiteral("rust"), QStringLiteral("Rust"), {QStringLiteral("rust")}, {QStringLiteral("rustfmt --edition 2021")}, {}},
        {QStringLiteral("go"), QStringLiteral("Go"), {QStringLiteral("go")}, {QStringLiteral("gofmt")}, {}},
        {QStringLiteral("shell"), QStringLiteral("Shell"), {QStringLiteral("shellscript")},
         {QStringLiteral("shfmt -filename $(FILE_PATH)")}, {}},
        {QStringLiteral("xml"), QStringLiteral("XML"), {QStringLiteral("xml")}, {QStringLiteral("xmllint --format -")},
         {QStringLiteral("XMLLINT_INDENT=$(INDENT)")}},
        {QStringLiteral("lua"), QStringLiteral("Lua"), {QStringLiteral("lua")},
         {QStringLiteral("stylua --stdin-filepath $(FILE_PATH) -")}, {}},
        {QStringLiteral("toml"), QStringLiteral("TOML"), {QStringLiteral("toml")}, {QStringLiteral("taplo fmt -")}, {}},
        {QStringLiteral("zig"), QStringLiteral("Zig"), {QStringLiteral("zig")}, {QStringLiteral("zig fmt --stdin")}, {}},
        {QStringLiteral("dart"), QStringLiteral("Dart"), {QStringLiteral("dart")}, {QStringLiteral("dart format")}, {}},
        {QStringLiteral("kotlin"), QStringLiteral("Kotlin"), {QStringLiteral("kotlin")},
         {QStringLiteral("ktfmt -"), QStringLiteral("ktlint --format --stdin")}, {}},
    };
    return specs;
}

const FormatterSpec* formatterForLanguage(const QString& languageId)
{
    for (const FormatterSpec& spec : knownFormatters()) {
        if (spec.languageIds.contains(languageId))
            return &spec;
    }
    return nullptr;
}

std::optional<ResolvedFormatter> resolveFormatter(const FormatterSpec& spec, const QMap<QString, QString>& overrides)
{
    QStringList commands = spec.candidates;
    const QString custom = overrides.value(spec.id).trimmed();
    if (custom.compare(QLatin1String("off"), Qt::CaseInsensitive) == 0)
        return std::nullopt;
    if (!custom.isEmpty())
        commands = {custom};
    for (const QString& command : commands) {
        QStringList parts = QProcess::splitCommand(command);
        if (parts.isEmpty())
            continue;
        const QString name = parts.takeFirst();
        const QString program = QFileInfo(name).isAbsolute() ? (QFileInfo(name).isExecutable() ? name : QString())
                                                             : QStandardPaths::findExecutable(name);
        if (program.isEmpty())
            continue;
        ResolvedFormatter formatter{spec.id, program, parts, spec.environment, command, false};
        formatter.supportsLines = QFileInfo(program).fileName().startsWith(QLatin1String("clang-format"));
        return formatter;
    }
    return std::nullopt;
}

FormatResult runFormatter(const ResolvedFormatter& formatter, const QByteArray& input, const FormatRequest& request)
{
    FormatResult result;
    // Sin archivo, un nombre con la extensión que no hay: lo que importa es la carpeta y el tipo
    const QString path = request.filePath.isEmpty() ? QDir::home().filePath(QStringLiteral("untitled")) : request.filePath;
    const QString indent = request.useTabs ? QStringLiteral("\t") : QString(request.tabSize, QLatin1Char(' '));
    auto expand = [&](QString text) {
        text.replace(QLatin1String("$(FILE_PATH)"), path);
        text.replace(QLatin1String("$(FILE_NAME)"), QFileInfo(path).fileName());
        text.replace(QLatin1String("$(TAB_SIZE)"), QString::number(request.tabSize));
        text.replace(QLatin1String("$(INDENT)"), indent);
        return text;
    };
    QStringList arguments;
    for (const QString& argument : formatter.arguments)
        arguments << expand(argument);
    if (formatter.supportsLines && request.firstLine > 0 && request.lastLine >= request.firstLine)
        arguments << QStringLiteral("--lines=%1:%2").arg(request.firstLine).arg(request.lastLine);

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    for (const QString& variable : formatter.environment) {
        const QString value = expand(variable);
        const qsizetype eq = value.indexOf(QLatin1Char('='));
        if (eq > 0)
            environment.insert(value.left(eq), value.mid(eq + 1));
    }
    process.setProcessEnvironment(environment);
    const QFileInfo info(path);
    process.setWorkingDirectory(info.absoluteDir().exists() ? info.absolutePath() : QDir::homePath());
    process.start(formatter.program, arguments);
    if (!process.waitForStarted(5000)) {
        result.error = process.errorString();
        return result;
    }
    process.write(input);
    process.closeWriteChannel();
    if (!process.waitForFinished(request.timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        result.error = QStringLiteral("%1: timeout").arg(QFileInfo(formatter.program).fileName());
        return result;
    }
    result.output = process.readAllStandardOutput();
    const QString errors = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        result.error = errors.isEmpty() ? QStringLiteral("%1: exit code %2").arg(QFileInfo(formatter.program).fileName()).arg(process.exitCode())
                                        : errors;
        return result;
    }
    // Una salida vacía para una entrada que no lo es: algo salió mal (no se borra el documento)
    if (result.output.isEmpty() && !input.trimmed().isEmpty()) {
        result.error = errors.isEmpty() ? QStringLiteral("%1: empty output").arg(QFileInfo(formatter.program).fileName()) : errors;
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace edita
