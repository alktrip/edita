// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/RemoteCatalog.h"

#include "core/PluginInstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSysInfo>
#include <QVersionNumber>

#include <algorithm>

namespace edita::remote_catalog {

const char* const kDefaultListUrl = "https://github.com/alktrip/edita-plugins/releases/latest/download/plugins.json";
// edita-plugins/keys/catalog.pub (id F1C3EC604B709A70)
const char* const kPublicKey = "RWRwmnBLYOzD8XeIG/MjMgTKnCcZPOdihjgWtK6kvzByfyLg5JP5RbNP";

namespace {

QString tr(const char* text)
{
    return QCoreApplication::translate("RemoteCatalog", text);
}

void setError(QString* error, const QString& text)
{
    if (error)
        *error = text;
}

QString listFile(const Settings& settings)
{
    return QDir(settings.cacheDir).filePath(QStringLiteral("plugins.json"));
}

bool readFile(const QString& path, QByteArray& data)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    data = file.readAll();
    return true;
}

bool writeFile(const QString& path, const QByteArray& data)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

} // namespace

Settings settings(const QString& userPluginsDir)
{
    Settings s;
    s.cacheDir = QDir(userPluginsDir).filePath(QStringLiteral("Config/catalog"));
    QString url = qEnvironmentVariable("EDITA_PLUGIN_CATALOG_URL");
    if (url.isEmpty())
        url = QString::fromLatin1(kDefaultListUrl);
    if (url != QLatin1String("none")) {
        const QUrl parsed(url);
        s.listUrl = parsed.scheme().size() > 1 ? parsed : QUrl::fromLocalFile(QFileInfo(url).absoluteFilePath());
        s.signatureUrl = s.listUrl;
        s.signatureUrl.setPath(s.listUrl.path() + QStringLiteral(".minisig"));
    }
    const QString key = qEnvironmentVariable("EDITA_PLUGIN_CATALOG_PUBKEY");
    minisign::parsePublicKey(key.isEmpty() ? QString::fromLatin1(kPublicKey) : key, s.key);
    return s;
}

Runtime currentRuntime(int pluginApi)
{
    Runtime runtime;
    QString cpu = QSysInfo::currentCpuArchitecture();
    if (cpu == QLatin1String("arm64"))
        cpu = QStringLiteral("aarch64");
    runtime.arch = QSysInfo::kernelType() + QLatin1Char('-') + cpu; // "linux-x86_64", "darwin-aarch64"
    runtime.pluginApi = pluginApi;
    runtime.qtVersion = QString::fromLatin1(qVersion());
    return runtime;
}

bool accept(const QByteArray& json, const QByteArray& signature, const minisign::PublicKey& key, const QString& knownVersion,
            PluginCatalog& catalog, QString* error)
{
    QString signatureError;
    if (!minisign::verify(json, signature, key, nullptr, &signatureError)) {
        setError(error, tr("the plugin list is not signed by the Edita plugin catalog: %1").arg(signatureError));
        return false;
    }
    PluginCatalog parsed;
    QString parseError;
    if (!PluginCatalog::parse(json, parsed, &parseError)) {
        setError(error, tr("the plugin list is not valid: %1").arg(parseError));
        return false;
    }
    // No se vuelve a una lista anterior (con plugins viejos o con fallos ya corregidos)
    if (!knownVersion.isEmpty() && PluginVersion::parse(parsed.version) < PluginVersion::parse(knownVersion)) {
        setError(error, tr("the plugin list %1 is older than the one already known (%2)").arg(parsed.version, knownVersion));
        return false;
    }
    for (PluginInfo& info : parsed.plugins)
        info.fromCatalog = true;
    catalog = std::move(parsed);
    return true;
}

QString adapt(PluginCatalog& catalog, const Runtime& runtime)
{
    QString reason;
    if (!catalog.arch.isEmpty() && catalog.arch != runtime.arch)
        reason = tr("its binaries are for %1 and this computer is %2").arg(catalog.arch, runtime.arch);
    else if (catalog.pluginApi != 0 && catalog.pluginApi != runtime.pluginApi)
        reason = tr("its binaries use version %1 of the plugin API and this Edita uses version %2")
                     .arg(catalog.pluginApi)
                     .arg(runtime.pluginApi);
    else if (!catalog.qtVersion.isEmpty()) {
        // Un plugin compilado con un Qt más nuevo pide símbolos que este no tiene
        const QVersionNumber built = QVersionNumber::fromString(catalog.qtVersion);
        const QVersionNumber running = QVersionNumber::fromString(runtime.qtVersion);
        if (built.majorVersion() != running.majorVersion() || running < built)
            reason = tr("its binaries need Qt %1 or newer and Edita runs with Qt %2").arg(catalog.qtVersion, runtime.qtVersion);
    }
    if (reason.isEmpty())
        return {};
    auto& plugins = catalog.plugins;
    for (PluginInfo& info : plugins) {
        if (!info.repository.isEmpty() && !info.catalogRecipe.isEmpty()) {
            info.repository.clear(); // se compila en el equipo
            info.id.clear();
        }
    }
    plugins.erase(std::remove_if(plugins.begin(), plugins.end(),
                                 [](const PluginInfo& info) { return !info.repository.isEmpty(); }),
                  plugins.end());
    return reason;
}

bool loadCached(const Settings& settings, PluginCatalog& catalog, QString* error)
{
    QByteArray json;
    QByteArray signature;
    if (!readFile(listFile(settings), json) || !readFile(listFile(settings) + QStringLiteral(".minisig"), signature)) {
        setError(error, tr("there is no saved plugin list"));
        return false;
    }
    return accept(json, signature, settings.key, QString(), catalog, error);
}

bool store(const Settings& settings, const QByteArray& json, const QByteArray& signature, QString* error)
{
    // La firma primero: si se corta a la mitad, loadCached rechaza el par y no usa nada
    if (!QDir().mkpath(settings.cacheDir) || !writeFile(listFile(settings) + QStringLiteral(".minisig"), signature)
        || !writeFile(listFile(settings), json)) {
        setError(error, tr("cannot save the plugin list in %1").arg(settings.cacheDir));
        return false;
    }
    return true;
}

PluginCatalog merge(const PluginCatalog& bundled, const PluginCatalog& remote)
{
    PluginCatalog merged = remote;
    merged.sourcePath = bundled.sourcePath;
    merged.plugins = bundled.plugins;
    for (const PluginInfo& info : remote.plugins) {
        if (!bundled.find(info.folderName))
            merged.plugins.push_back(info);
    }
    if (merged.version.isEmpty())
        merged.version = bundled.version;
    return merged;
}

QString recipesDir(const Settings& settings, const PluginCatalog& catalog)
{
    return QDir(settings.cacheDir).filePath(QStringLiteral("recipes-") + catalog.recipesSha256.left(16));
}

bool unpackRecipes(const Settings& settings, const PluginCatalog& catalog, const QByteArray& zip, QString* error)
{
    if (catalog.recipesSha256.isEmpty() || plugin_installer::sha256Hex(zip) != catalog.recipesSha256) {
        setError(error, tr("the recipes of the plugin catalog do not match the signed list"));
        return false;
    }
    const QString dir = recipesDir(settings, catalog);
    if (QFileInfo::exists(dir))
        return true;
    // Se extrae aparte y se mueve: una carpeta a medias nunca tiene el nombre final
    const QString partial = dir + QStringLiteral(".part");
    QDir(partial).removeRecursively();
    if (!QDir().mkpath(partial) || !plugin_installer::extractZip(zip, partial, error)) {
        QDir(partial).removeRecursively();
        return false;
    }
    if (!QDir().rename(partial, dir)) {
        QDir(partial).removeRecursively();
        setError(error, tr("cannot save the recipes in %1").arg(dir));
        return false;
    }
    return true;
}

} // namespace edita::remote_catalog
