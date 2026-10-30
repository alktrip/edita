// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/PluginRecipe.h"

#include "core/Paths.h"
#include "core/PluginInstaller.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QThread>

namespace edita {

namespace {

QStringList stringList(const QJsonValue& value)
{
    QStringList out;
    for (const QJsonValue& v : value.toArray())
        out << v.toString();
    return out;
}

bool copyTree(const QString& from, const QString& to, QString* error)
{
    QDirIterator it(from, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        const QString target = QDir(to).filePath(QDir(from).relativeFilePath(file));
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile::remove(target);
        if (!QFile::copy(file, target)) {
            if (error)
                *error = QObject::tr("Could not copy %1").arg(file);
            return false;
        }
    }
    return true;
}

// La ruta de un #include "…" que no existe tal cual, resuelta sin distinguir mayúsculas
// carpeta a carpeta desde la del archivo; vacía si no existe
QString resolveCaseInsensitive(const QString& fromDir, const QString& include)
{
    QDir dir(fromDir);
    QStringList parts;
    for (const QString& part : include.split(QLatin1Char('/'))) {
        if (part.isEmpty() || part == QLatin1String(".") || part == QLatin1String("..")) {
            if (part == QLatin1String("..") && !dir.cdUp())
                return {};
            if (!part.isEmpty())
                parts << part;
            continue;
        }
        QString match;
        for (const QString& entry : dir.entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)) {
            if (entry == part) {
                match = entry;
                break;
            }
            if (match.isEmpty() && entry.compare(part, Qt::CaseInsensitive) == 0)
                match = entry;
        }
        if (match.isEmpty())
            return {};
        parts << match;
        if (!dir.cd(match)) {
            // Un archivo: tiene que ser la última parte
            if (parts.size() != include.split(QLatin1Char('/')).size())
                return {};
        }
    }
    return parts.join(QLatin1Char('/'));
}

} // namespace

int fixWindowsIncludes(const QString& sourceDir, const QStringList& headerDirs)
{
    // Los nombres de archivo del código (con sus rutas) y los de las cabeceras del SDK
    QHash<QString, QStringList> byLowerName;
    QSet<QString> exactNames;
    QStringList sources;
    static const QStringList extensions = {QStringLiteral("c"), QStringLiteral("cc"), QStringLiteral("cpp"),
                                           QStringLiteral("cxx"), QStringLiteral("h"), QStringLiteral("hh"),
                                           QStringLiteral("hpp"), QStringLiteral("hxx"), QStringLiteral("inl")};
    QDirIterator it(sourceDir, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        const QString name = it.fileName();
        byLowerName[name.toLower()] << file;
        exactNames.insert(name);
        if (extensions.contains(QFileInfo(file).suffix().toLower()))
            sources << file;
    }
    QHash<QString, QString> sdkNames;
    for (const QString& dir : headerDirs) {
        for (const QString& name : QDir(dir).entryList(QDir::Files))
            sdkNames.insert(name.toLower(), name);
    }

    static const QRegularExpression include(QStringLiteral(R"(^(\s*#\s*include\s*)([<"])([^>"\n]+)([>"]))"),
                                            QRegularExpression::MultilineOption);
    int changed = 0;
    for (const QString& file : sources) {
        QFile in(file);
        if (!in.open(QIODevice::ReadOnly))
            continue;
        // Latin-1: los bytes del archivo salen tal cual, esté en la codificación que esté
        const QString text = QString::fromLatin1(in.readAll());
        in.close();
        const QString here = QFileInfo(file).absolutePath();
        QString out;
        qsizetype last = 0;
        bool modified = false;
        QRegularExpressionMatchIterator matches = include.globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch m = matches.next();
            const bool quoted = m.captured(2) == QLatin1String("\"");
            const QString original = m.captured(3);
            QString path = original;
            path.replace(QLatin1Char('\\'), QLatin1Char('/'));
            const QString base = QFileInfo(path).fileName();
            const QString lower = base.toLower();
            QString fixed = path;
            const bool existsAsIs = quoted ? QFileInfo::exists(QDir(here).filePath(path)) : false;
            if (!existsAsIs && !exactNames.contains(base)) {
                if (quoted) {
                    const QString resolved = resolveCaseInsensitive(here, path);
                    if (!resolved.isEmpty())
                        fixed = resolved;
                    else if (sdkNames.contains(lower))
                        fixed = sdkNames.value(lower);
                    else if (byLowerName.value(lower).size() == 1)
                        fixed = QDir(here).relativeFilePath(byLowerName.value(lower).first());
                } else if (sdkNames.contains(lower) && !path.contains(QLatin1Char('/'))) {
                    fixed = sdkNames.value(lower);
                }
            }
            if (fixed == original)
                continue;
            out += text.mid(last, m.capturedStart(3) - last) + fixed;
            last = m.capturedEnd(3);
            modified = true;
        }
        if (!modified)
            continue;
        out += text.mid(last);
        QFile dst(file);
        if (dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            dst.write(out.toLatin1());
            ++changed;
        }
    }
    return changed;
}

QString PluginRecipe::directory() const
{
    return QFileInfo(path).absolutePath();
}

bool PluginRecipe::parse(const QString& path, PluginRecipe& recipe, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QObject::tr("Cannot read %1").arg(path);
        return false;
    }
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject o = doc.object();
    const QJsonObject source = o.value(QStringLiteral("source")).toObject();
    if (!doc.isObject() || o.value(QStringLiteral("folder-name")).toString().isEmpty()
        || source.value(QStringLiteral("url")).toString().isEmpty()) {
        if (error)
            *error = QObject::tr("%1: invalid recipe (folder-name or source.url is missing) %2").arg(path, parseError.errorString());
        return false;
    }
    recipe = PluginRecipe{};
    recipe.path = QFileInfo(path).absoluteFilePath();
    PluginInfo& info = recipe.info;
    info.folderName = o.value(QStringLiteral("folder-name")).toString();
    info.displayName = o.value(QStringLiteral("display-name")).toString(info.folderName);
    info.version = PluginVersion::parse(o.value(QStringLiteral("version")).toString());
    info.description = o.value(QStringLiteral("description")).toString();
    info.author = o.value(QStringLiteral("author")).toString();
    info.homepage = o.value(QStringLiteral("homepage")).toString();
    info.compatibleVersions = parseVersionInterval(o.value(QStringLiteral("npp-compatible-versions")).toString());
    info.recipePath = recipe.path;
    recipe.sourceUrl = source.value(QStringLiteral("url")).toString();
    recipe.sourceSha256 = source.value(QStringLiteral("sha256")).toString();
    recipe.stripComponents = source.value(QStringLiteral("strip-components")).toInt(1);
    recipe.remove = stringList(o.value(QStringLiteral("remove")));
    recipe.patches = stringList(o.value(QStringLiteral("patches")));
    recipe.overlay = o.value(QStringLiteral("overlay")).toString();
    recipe.buildDir = o.value(QStringLiteral("build-dir")).toString(QStringLiteral("."));
    recipe.requires_ = stringList(o.value(QStringLiteral("requires")));
    recipe.fixIncludes = o.value(QStringLiteral("fix-includes")).toBool(true);
    return true;
}

std::vector<PluginRecipe> loadRecipes(const QStringList& directories, QStringList* warnings)
{
    std::vector<PluginRecipe> recipes;
    QStringList seen;
    for (const QString& directory : directories) {
        const QDir dir(directory);
        for (const QString& name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            const QString path = dir.filePath(name + QStringLiteral("/recipe.json"));
            if (!QFileInfo::exists(path))
                continue;
            PluginRecipe recipe;
            QString error;
            if (!PluginRecipe::parse(path, recipe, &error)) {
                if (warnings)
                    *warnings << error;
                continue;
            }
            if (seen.contains(recipe.info.folderName, Qt::CaseInsensitive))
                continue;
            seen << recipe.info.folderName;
            recipes.push_back(std::move(recipe));
        }
    }
    return recipes;
}

namespace {

PluginSdk locatePluginSdk()
{
    PluginSdk sdk;
    auto valid = [](const QString& dir) { return QFileInfo::exists(QDir(dir).filePath(QStringLiteral("compat/npp_compat.h"))); };
    const QString env = qEnvironmentVariable("EDITA_PLUGIN_SDK");
    if (!env.isEmpty() && valid(env)) {
        sdk.sdkDir = env;
        sdk.scintillaInclude = QFileInfo::exists(QDir(env).filePath(QStringLiteral("Scintilla.h")))
            ? env
            : QDir(env).filePath(QStringLiteral("../third_party/scintilla/include"));
        return sdk;
    }
    for (const QString& data : paths::dataDirs()) {
        const QString installed = QDir(data).filePath(QStringLiteral("plugin-sdk"));
        if (valid(installed)) {
            sdk.sdkDir = QDir(installed).absolutePath();
            sdk.scintillaInclude = sdk.sdkDir; // la instalación copia Scintilla.h al SDK
            return sdk;
        }
        // Sin instalar: editor/data → editor/plugin-sdk y editor/third_party/scintilla/include
        const QString source = QDir(data).filePath(QStringLiteral("../plugin-sdk"));
        if (valid(source)) {
            sdk.sdkDir = QDir(source).absolutePath();
            sdk.scintillaInclude = QDir(QDir(data).filePath(QStringLiteral("../third_party/scintilla/include"))).absolutePath();
            return sdk;
        }
    }
    return sdk;
}

} // namespace

PluginSdk findPluginSdk()
{
    PluginSdk sdk = locatePluginSdk();
    const QString qtInclude = QDir(sdk.sdkDir).filePath(QStringLiteral("qt/include"));
    if (!sdk.isValid() || !QFileInfo::exists(QDir(qtInclude).filePath(QStringLiteral("QtWidgets/QWidget"))))
        return sdk;
    // Las bibliotecas del Qt en uso (las del AppImage: <usr>/lib, junto a share/edita/plugin-sdk)
    const QStringList libDirs = {QLibraryInfo::path(QLibraryInfo::LibrariesPath),
                                 QDir(sdk.sdkDir).filePath(QStringLiteral("../../../lib"))};
    for (const QString& dir : libDirs) {
        if (QFileInfo::exists(QDir(dir).filePath(QStringLiteral("libQt6Widgets.so.6")))) {
            sdk.qtInclude = QDir(qtInclude).absolutePath();
            sdk.qtLibDir = QDir(dir).absolutePath();
            break;
        }
    }
    return sdk;
}

QString recipeBuildTag(const PluginSdk& sdk)
{
    return QStringLiteral("Qt %1 (%2)").arg(QString::fromLatin1(qVersion()),
                                            sdk.qtInclude.isEmpty() ? QStringLiteral("system") : QStringLiteral("bundled"));
}

QStringList missingRequirements(const QStringList& requirements)
{
    QStringList missing;
    for (const QString& requirement : requirements) {
        if (requirement.startsWith(QLatin1String("pkg-config:"))) {
            const QString module = requirement.mid(11);
            QProcess pkg;
            pkg.start(QStringLiteral("pkg-config"), {QStringLiteral("--exists"), module});
            if (!pkg.waitForFinished(10000) || pkg.exitStatus() != QProcess::NormalExit || pkg.exitCode() != 0)
                missing << QObject::tr("%1 (development library, pkg-config)").arg(module);
        } else if (requirement == QLatin1String("c++")) {
            if (QStandardPaths::findExecutable(QStringLiteral("c++")).isEmpty()
                && QStandardPaths::findExecutable(QStringLiteral("g++")).isEmpty()
                && QStandardPaths::findExecutable(QStringLiteral("clang++")).isEmpty())
                missing << QObject::tr("a C++ compiler (g++ or clang++)");
        } else if (QStandardPaths::findExecutable(requirement).isEmpty()) {
            missing << requirement;
        }
    }
    return missing;
}

// --- RecipeBuilder -------------------------------------------------------------------------

namespace {

enum Stage { Patching = 0, Configure = 1, Build = 2, Package = 3, Done = 4 };

} // namespace

RecipeBuilder::RecipeBuilder(PluginRecipe recipe, PluginSdk sdk, QString workDir, QObject* parent)
    : QObject(parent)
    , _recipe(std::move(recipe))
    , _sdk(std::move(sdk))
    , _workDir(std::move(workDir))
{
}

RecipeBuilder::~RecipeBuilder()
{
    if (_process && _process->state() != QProcess::NotRunning) {
        _process->kill();
        _process->waitForFinished(3000);
    }
}

void RecipeBuilder::step(const QString& text)
{
    const QString line = QStringLiteral("==> ") + text + QLatin1Char('\n');
    _log += line;
    emit output(line);
}

void RecipeBuilder::fail(const QString& error)
{
    _running = false;
    const QString line = tr("Error: %1\n").arg(error);
    _log += line;
    emit output(line);
    emit finished(false, error);
}

void RecipeBuilder::cancel()
{
    if (!_running)
        return;
    _cancelled = true;
    if (_process && _process->state() != QProcess::NotRunning)
        _process->kill();
    else
        fail(tr("canceled"));
}

void RecipeBuilder::start(const QByteArray& sourceZip)
{
    _running = true;
    _cancelled = false;
    _stage = Patching;
    const PluginInfo& info = _recipe.info;
    step(tr("%1 %2 from source code").arg(info.displayName, info.version.toString()));

    if (!_sdk.isValid()) {
        fail(tr("the Edita plugin SDK was not found"));
        return;
    }
    QStringList requirements = _recipe.requires_;
    requirements << QStringLiteral("cmake") << QStringLiteral("c++");
    if (!_recipe.patches.isEmpty())
        requirements << QStringLiteral("patch");
    requirements.removeDuplicates();
    step(tr("Checking requirements: %1").arg(requirements.join(QStringLiteral(", "))));
    const QStringList missing = missingRequirements(requirements);
    if (!missing.isEmpty()) {
        fail(tr("missing on the system: %1").arg(missing.join(QStringLiteral(", "))));
        return;
    }
    QString error;
    if (!prepareSources(sourceZip, &error)) {
        fail(error);
        return;
    }
    _pendingPatches = _recipe.patches;
    runNext();
}

bool RecipeBuilder::prepareSources(const QByteArray& sourceZip, QString* error)
{
    // El .zip del autor, comprobado como un paquete de la lista
    const QString sha = plugin_installer::sha256Hex(sourceZip);
    if (!_recipe.sourceSha256.isEmpty() && sha.compare(_recipe.sourceSha256, Qt::CaseInsensitive) != 0) {
        *error = tr("the SHA-256 of the downloaded code (%1) is not the recipe's").arg(sha);
        return false;
    }
    step(tr("Source code verified (SHA-256 %1)").arg(sha.left(16)));
    QDir(_workDir).removeRecursively();
    const QString extracted = QDir(_workDir).filePath(QStringLiteral("extracted"));
    QDir().mkpath(extracted);
    if (!plugin_installer::extractZip(sourceZip, extracted, error))
        return false;
    // strip-components: el .zip de GitHub trae una carpeta <proyecto>-<versión>/
    _sourceDir = extracted;
    for (int i = 0; i < _recipe.stripComponents; ++i) {
        const QStringList entries = QDir(_sourceDir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        if (entries.size() != 1 || !QFileInfo(QDir(_sourceDir).filePath(entries.first())).isDir()) {
            *error = tr("the code does not have the root folder the recipe expects");
            return false;
        }
        _sourceDir = QDir(_sourceDir).filePath(entries.first());
    }
    for (const QString& path : _recipe.remove) {
        const QString target = QDir(_sourceDir).filePath(path);
        if (QFileInfo(target).isDir())
            QDir(target).removeRecursively();
        else
            QFile::remove(target);
    }
    if (!_recipe.remove.isEmpty())
        step(tr("Removed from the original: %1").arg(_recipe.remove.join(QStringLiteral(", "))));
    if (!_recipe.overlay.isEmpty()) {
        if (!copyTree(QDir(_recipe.directory()).filePath(_recipe.overlay), _sourceDir, error))
            return false;
        step(tr("Edita port copied over the code (%1/)").arg(_recipe.overlay));
    }
    _buildDir = QDir(_workDir).filePath(QStringLiteral("build"));
    return true;
}

void RecipeBuilder::runProcess(const QString& program, const QStringList& arguments, const QString& workingDir)
{
    _log += QStringLiteral("$ %1 %2\n").arg(program, arguments.join(QLatin1Char(' ')));
    emit output(QStringLiteral("$ %1 %2\n").arg(program, arguments.join(QLatin1Char(' '))));
    _process = std::make_unique<QProcess>();
    _process->setWorkingDirectory(workingDir);
    _process->setProcessChannelMode(QProcess::MergedChannels);
    connect(_process.get(), &QProcess::readyRead, this, [this] {
        const QString text = QString::fromLocal8Bit(_process->readAll());
        _log += text;
        emit output(text);
    });
    connect(_process.get(), &QProcess::finished, this, [this, program](int code, QProcess::ExitStatus status) {
        if (_cancelled) {
            fail(tr("canceled"));
            return;
        }
        if (status != QProcess::NormalExit || code != 0) {
            fail(tr("%1 finished with exit code %2").arg(program).arg(code));
            return;
        }
        runNext();
    });
    connect(_process.get(), &QProcess::errorOccurred, this, [this, program](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            fail(tr("could not run %1").arg(program));
    });
    _process->start(program, arguments);
}

void RecipeBuilder::runNext()
{
    if (!_running)
        return;
    switch (_stage) {
    case Patching:
        if (!_pendingPatches.isEmpty()) {
            const QString patch = QDir(_recipe.directory()).filePath(_pendingPatches.takeFirst());
            step(tr("Applying %1").arg(QFileInfo(patch).fileName()));
            runProcess(QStringLiteral("patch"), {QStringLiteral("-p1"), QStringLiteral("--forward"), QStringLiteral("-i"), patch},
                       _sourceDir);
            return;
        }
        _stage = Configure;
        [[fallthrough]];
    case Configure: {
        _stage = Build;
        if (_recipe.fixIncludes) {
            const QStringList headerDirs = {QDir(_sdk.sdkDir).filePath(QStringLiteral("compat")), _sdk.sdkDir, _sdk.scintillaInclude};
            if (const int fixed = fixWindowsIncludes(_sourceDir, headerDirs))
                step(tr("Fixed the Windows #include lines (case, \\) in %n file(s)", nullptr, fixed));
        }
        step(tr("Configuring with CMake"));
        QStringList args = {QStringLiteral("-S"), QDir(_sourceDir).filePath(_recipe.buildDir), QStringLiteral("-B"), _buildDir,
                            QStringLiteral("-DCMAKE_BUILD_TYPE=Release"),
                            QStringLiteral("-DEDITA_PLUGIN_SDK=") + _sdk.sdkDir,
                            QStringLiteral("-DEDITA_SCINTILLA_INCLUDE=") + _sdk.scintillaInclude,
                            QStringLiteral("-DEDITA_PLUGIN_VERSION=") + _recipe.info.version.toString(),
                            QStringLiteral("-DEDITA_PLUGIN_DISPLAY_NAME=") + _recipe.info.displayName,
                            QStringLiteral("-DEDITA_PLUGIN_HOMEPAGE=") + _recipe.info.homepage};
        if (!_sdk.qtInclude.isEmpty())
            args << QStringLiteral("-DEDITA_QT_INCLUDE=") + _sdk.qtInclude << QStringLiteral("-DEDITA_QT_LIBDIR=") + _sdk.qtLibDir;
        if (!QStandardPaths::findExecutable(QStringLiteral("ninja")).isEmpty())
            args << QStringLiteral("-G") << QStringLiteral("Ninja");
        runProcess(QStringLiteral("cmake"), args, _sourceDir);
        return;
    }
    case Build:
        _stage = Package;
        step(tr("Building"));
        runProcess(QStringLiteral("cmake"),
                   {QStringLiteral("--build"), _buildDir, QStringLiteral("--parallel"), QString::number(std::max(1, QThread::idealThreadCount()))},
                   _sourceDir);
        return;
    case Package: {
        _stage = Done;
        QString error;
        if (!package(&error)) {
            fail(error);
            return;
        }
        _running = false;
        step(tr("Done: %1").arg(QFileInfo(_package).fileName()));
        emit finished(true, QString());
        return;
    }
    default:
        return;
    }
}

bool RecipeBuilder::package(QString* error)
{
    const QString library = _recipe.info.folderName + QStringLiteral(".so");
    QString built;
    QDirIterator it(_buildDir, {library}, QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext())
        built = it.next();
    if (built.isEmpty()) {
        *error = tr("the build did not produce %1").arg(library);
        return false;
    }
    _package = QDir(_workDir).filePath(QStringLiteral("%1-%2.zip").arg(_recipe.info.folderName, _recipe.info.version.toString()));
    if (!plugin_installer::createZip(_package, {{library, built}}, error))
        return false;
    QFile zip(_package);
    if (!zip.open(QIODevice::ReadOnly)) {
        *error = tr("cannot read %1").arg(_package);
        return false;
    }
    _packageSha = plugin_installer::sha256Hex(zip.readAll());
    return true;
}

} // namespace edita
