// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lsp/LspServers.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include <map>

namespace edita::lsp {

const std::vector<ServerSpec>& knownServers()
{
    static const std::vector<ServerSpec> servers = {
        {QStringLiteral("clangd"), QStringLiteral("C, C++, Objective-C"), {"c", "cpp", "objective-c", "objective-cpp", "cuda"},
         {"clangd"}, {"compile_commands.json", "compile_flags.txt", ".clangd", ".git"}},
        {QStringLiteral("python"), QStringLiteral("Python"), {"python"},
         {"basedpyright-langserver --stdio", "pyright-langserver --stdio", "pylsp", "jedi-language-server", "ruff server"},
         {"pyproject.toml", "setup.py", "setup.cfg", "requirements.txt", "Pipfile", ".git"}},
        {QStringLiteral("typescript"), QStringLiteral("JavaScript, TypeScript"),
         {"javascript", "typescript", "javascriptreact", "typescriptreact"}, {"typescript-language-server --stdio"},
         {"tsconfig.json", "jsconfig.json", "package.json", ".git"}},
        {QStringLiteral("rust"), QStringLiteral("Rust"), {"rust"}, {"rust-analyzer"}, {"Cargo.toml", ".git"}},
        {QStringLiteral("go"), QStringLiteral("Go"), {"go"}, {"gopls"}, {"go.work", "go.mod", ".git"}},
        {QStringLiteral("bash"), QStringLiteral("Bash"), {"shellscript"}, {"bash-language-server start"}, {".git"}},
        {QStringLiteral("lua"), QStringLiteral("Lua"), {"lua"}, {"lua-language-server"}, {".luarc.json", ".git"}},
        {QStringLiteral("json"), QStringLiteral("JSON"), {"json", "jsonc"},
         {"vscode-json-language-server --stdio", "vscode-json-languageserver --stdio"}, {".git"}},
        {QStringLiteral("css"), QStringLiteral("CSS"), {"css", "scss", "less"}, {"vscode-css-language-server --stdio"}, {".git"}},
        {QStringLiteral("html"), QStringLiteral("HTML"), {"html"}, {"vscode-html-language-server --stdio"}, {".git"}},
        {QStringLiteral("yaml"), QStringLiteral("YAML"), {"yaml"}, {"yaml-language-server --stdio"}, {".git"}},
        {QStringLiteral("toml"), QStringLiteral("TOML"), {"toml"}, {"taplo lsp stdio"}, {".git"}},
        {QStringLiteral("markdown"), QStringLiteral("Markdown"), {"markdown"}, {"marksman server"}, {".marksman.toml", ".git"}},
        {QStringLiteral("java"), QStringLiteral("Java"), {"java"}, {"jdtls"}, {"pom.xml", "build.gradle", "build.gradle.kts", ".git"}},
        {QStringLiteral("php"), QStringLiteral("PHP"), {"php"}, {"intelephense --stdio", "phpactor language-server"}, {"composer.json", ".git"}},
        {QStringLiteral("csharp"), QStringLiteral("C#"), {"csharp"}, {"csharp-ls"}, {".git"}},
        {QStringLiteral("ruby"), QStringLiteral("Ruby"), {"ruby"}, {"ruby-lsp", "solargraph stdio"}, {"Gemfile", ".git"}},
        {QStringLiteral("latex"), QStringLiteral("LaTeX"), {"latex", "tex"}, {"texlab"}, {".git"}},
        {QStringLiteral("cmake"), QStringLiteral("CMake"), {"cmake"}, {"neocmakelsp --stdio", "cmake-language-server"}, {".git"}},
        {QStringLiteral("haskell"), QStringLiteral("Haskell"), {"haskell"}, {"haskell-language-server-wrapper --lsp"}, {"stack.yaml", "cabal.project", ".git"}},
        {QStringLiteral("swift"), QStringLiteral("Swift"), {"swift"}, {"sourcekit-lsp"}, {"Package.swift", ".git"}},
        {QStringLiteral("zig"), QStringLiteral("Zig"), {"zig"}, {"zls"}, {"build.zig", ".git"}},
        {QStringLiteral("dart"), QStringLiteral("Dart"), {"dart"}, {"dart language-server"}, {"pubspec.yaml", ".git"}},
        {QStringLiteral("kotlin"), QStringLiteral("Kotlin"), {"kotlin"}, {"kotlin-language-server"}, {"build.gradle.kts", ".git"}},
        {QStringLiteral("xml"), QStringLiteral("XML"), {"xml"}, {"lemminx"}, {".git"}},
        {QStringLiteral("nim"), QStringLiteral("Nim"), {"nim"}, {"nimlangserver"}, {".git"}},
        {QStringLiteral("d"), QStringLiteral("D"), {"d"}, {"serve-d"}, {"dub.json", "dub.sdl", ".git"}},
    };
    return servers;
}

const ServerSpec* serverForLanguage(const QString& languageId)
{
    for (const ServerSpec& spec : knownServers()) {
        if (spec.languageIds.contains(languageId))
            return &spec;
    }
    return nullptr;
}

QString languageIdFor(LangType lang, const QString& userLanguage, const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    switch (lang) {
    case LangType::C: return QStringLiteral("c");
    case LangType::Cpp: return suffix == QLatin1String("cu") ? QStringLiteral("cuda") : QStringLiteral("cpp");
    case LangType::ObjC: return suffix == QLatin1String("mm") ? QStringLiteral("objective-cpp") : QStringLiteral("objective-c");
    case LangType::Python: return QStringLiteral("python");
    case LangType::JavaScript:
    case LangType::JsEmbedded: return suffix == QLatin1String("jsx") ? QStringLiteral("javascriptreact") : QStringLiteral("javascript");
    case LangType::TypeScript: return suffix == QLatin1String("tsx") ? QStringLiteral("typescriptreact") : QStringLiteral("typescript");
    case LangType::Rust: return QStringLiteral("rust");
    case LangType::Go: return QStringLiteral("go");
    case LangType::Bash: return QStringLiteral("shellscript");
    case LangType::Lua: return QStringLiteral("lua");
    case LangType::Json:
    case LangType::Json5: return suffix == QLatin1String("jsonc") ? QStringLiteral("jsonc") : QStringLiteral("json");
    case LangType::Css: return suffix == QLatin1String("scss") ? QStringLiteral("scss") : suffix == QLatin1String("less") ? QStringLiteral("less") : QStringLiteral("css");
    case LangType::Html: return QStringLiteral("html");
    case LangType::Yaml: return QStringLiteral("yaml");
    case LangType::Toml: return QStringLiteral("toml");
    case LangType::Java: return QStringLiteral("java");
    case LangType::Php: return QStringLiteral("php");
    case LangType::CSharp: return QStringLiteral("csharp");
    case LangType::Ruby: return QStringLiteral("ruby");
    case LangType::Tex:
    case LangType::Latex: return QStringLiteral("latex");
    case LangType::CMake: return QStringLiteral("cmake");
    case LangType::Haskell: return QStringLiteral("haskell");
    case LangType::Swift: return QStringLiteral("swift");
    case LangType::Xml: return QStringLiteral("xml");
    case LangType::Nim: return QStringLiteral("nim");
    case LangType::D: return QStringLiteral("d");
    default: break;
    }
    // Sin lenguaje propio en N++: por el lenguaje del usuario o la extensión
    if (userLanguage.contains(QLatin1String("markdown"), Qt::CaseInsensitive))
        return QStringLiteral("markdown");
    static const QMap<QString, QString> bySuffix = {
        {QStringLiteral("md"), QStringLiteral("markdown")},   {QStringLiteral("markdown"), QStringLiteral("markdown")},
        {QStringLiteral("zig"), QStringLiteral("zig")},       {QStringLiteral("dart"), QStringLiteral("dart")},
        {QStringLiteral("kt"), QStringLiteral("kotlin")},     {QStringLiteral("kts"), QStringLiteral("kotlin")},
    };
    return bySuffix.value(suffix);
}

std::optional<ResolvedServer> resolveServer(const ServerSpec& spec, const QMap<QString, QString>& overrides)
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
        if (!program.isEmpty())
            return ResolvedServer{spec.id, program, parts, command};
    }
    return std::nullopt;
}

QMap<QString, QString> parseOverrides(const QString& text)
{
    QMap<QString, QString> overrides;
    for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq > 0)
            overrides.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    return overrides;
}

QString formatOverrides(const QMap<QString, QString>& overrides)
{
    QStringList lines;
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        if (!it.value().trimmed().isEmpty())
            lines << it.key() + QLatin1Char('=') + it.value().trimmed();
    }
    return lines.join(QLatin1Char('\n'));
}

QString findRoot(const QString& filePath, const QStringList& workspaceRoots, const QStringList& markers)
{
    const QString file = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
    // La carpeta de trabajo más profunda que lo contiene
    QString best;
    for (const QString& root : workspaceRoots) {
        const QString clean = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
        if (file.startsWith(clean + QLatin1Char('/')) && clean.size() > best.size())
            best = clean;
    }
    if (!best.isEmpty())
        return best;
    for (QDir dir = QFileInfo(file).absoluteDir();;) {
        for (const QString& marker : markers) {
            if (QFileInfo::exists(dir.filePath(marker)))
                return dir.absolutePath();
        }
        if (!dir.cdUp())
            break;
    }
    return QFileInfo(file).absolutePath();
}

int lspCharacter(QByteArrayView lineUtf8, int byteColumn, PositionEncoding encoding)
{
    const int column = std::clamp(byteColumn, 0, static_cast<int>(lineUtf8.size()));
    if (encoding == PositionEncoding::Utf8)
        return column;
    return static_cast<int>(QString::fromUtf8(lineUtf8.first(column)).size()); // unidades UTF-16
}

int byteColumn(QByteArrayView lineUtf8, int character, PositionEncoding encoding)
{
    if (character <= 0)
        return 0;
    if (encoding == PositionEncoding::Utf8)
        return std::min(character, static_cast<int>(lineUtf8.size()));
    const QString line = QString::fromUtf8(lineUtf8);
    const int units = std::min(character, static_cast<int>(line.size()));
    return static_cast<int>(QStringView(line).first(units).toUtf8().size());
}

namespace {

// Lee hasta `stop` (o el final); `tabstops` guarda dónde quedó cada marcador
QString parseSnippet(const QString& s, qsizetype& i, QChar stop, std::map<int, std::pair<int, int>>& tabstops, int offset)
{
    QString out;
    auto isDigit = [](QChar c) { return c >= QLatin1Char('0') && c <= QLatin1Char('9'); };
    while (i < s.size()) {
        const QChar c = s[i];
        if (!stop.isNull() && c == stop)
            return out;
        if (c == QLatin1Char('\\') && i + 1 < s.size() && QStringLiteral("$}\\,|").contains(s[i + 1])) {
            out += s[i + 1];
            i += 2;
            continue;
        }
        if (c != QLatin1Char('$')) {
            out += c;
            ++i;
            continue;
        }
        ++i;
        if (i < s.size() && isDigit(s[i])) { // $1
            int n = 0;
            while (i < s.size() && isDigit(s[i]))
                n = n * 10 + s[i++].digitValue();
            tabstops.try_emplace(n, offset + static_cast<int>(out.size()), offset + static_cast<int>(out.size()));
            continue;
        }
        if (i < s.size() && s[i] == QLatin1Char('{')) {
            ++i;
            int n = -1;
            if (i < s.size() && isDigit(s[i])) {
                n = 0;
                while (i < s.size() && isDigit(s[i]))
                    n = n * 10 + s[i++].digitValue();
            } else { // ${variable} o ${variable:defecto}
                while (i < s.size() && (s[i].isLetterOrNumber() || s[i] == QLatin1Char('_')))
                    ++i;
            }
            const int start = offset + static_cast<int>(out.size());
            if (i < s.size() && s[i] == QLatin1Char(':')) { // ${1:texto}
                ++i;
                out += parseSnippet(s, i, QLatin1Char('}'), tabstops, start);
            } else if (i < s.size() && s[i] == QLatin1Char('|')) { // ${1|uno,dos|}: el primero
                ++i;
                QString choice;
                while (i < s.size() && s[i] != QLatin1Char(',') && s[i] != QLatin1Char('|'))
                    choice += s[i++];
                while (i < s.size() && s[i] != QLatin1Char('}'))
                    ++i;
                out += choice;
            }
            if (i < s.size() && s[i] == QLatin1Char('}'))
                ++i;
            if (n >= 0)
                tabstops.try_emplace(n, start, offset + static_cast<int>(out.size()));
            continue;
        }
        // $variable: sin valor
        while (i < s.size() && (s[i].isLetterOrNumber() || s[i] == QLatin1Char('_')))
            ++i;
    }
    return out;
}

} // namespace

ExpandedSnippet expandSnippet(const QString& snippet)
{
    std::map<int, std::pair<int, int>> tabstops;
    qsizetype i = 0;
    ExpandedSnippet result;
    result.text = parseSnippet(snippet, i, QChar(), tabstops, 0);
    result.selectionStart = result.selectionEnd = static_cast<int>(result.text.size());
    // El primer marcador (1, 2…); si no hay, $0
    auto it = tabstops.upper_bound(0);
    if (it == tabstops.end())
        it = tabstops.find(0);
    if (it != tabstops.end()) {
        result.selectionStart = it->second.first;
        result.selectionEnd = it->second.second;
    }
    return result;
}

QString uriFromPath(const QString& path)
{
    return QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toString(QUrl::FullyEncoded);
}

QString pathFromUri(const QString& uri)
{
    const QUrl url(uri);
    return url.isLocalFile() ? QDir::cleanPath(url.toLocalFile()) : QString();
}

} // namespace edita::lsp
