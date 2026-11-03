// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// El catálogo de plugins de Edita (alktrip/edita-plugins): una lista firmada con minisign
// que se descarga al abrir el administrador de plugins. N++ trae su lista dentro de una DLL
// firmada y la actualiza con cada versión; Edita la descarga, para que un plugin nuevo no
// necesite una versión nueva de Edita.
//
// Antes de usar una lista se comprueba:
//   - la firma, con la clave pública del catálogo que trae Edita;
//   - que no sea más antigua que la última que se aceptó ("version": no se vuelve atrás);
//   - para cada binario: la arquitectura, la API de plugins y que el Qt de Edita no sea más
//     antiguo que el de la compilación. Si no sirve, el plugin se compila con su receta.
// La última lista aceptada se guarda (con su firma, que se vuelve a comprobar al leerla) en
// <plugins>/Config/catalog/.

#pragma once

#include "core/Minisign.h"
#include "core/PluginCatalog.h"

#include <QUrl>

namespace edita::remote_catalog {

// La lista publicada y la clave con la que se firma (edita-plugins/keys/catalog.pub)
extern const char* const kDefaultListUrl;
extern const char* const kPublicKey;

struct Settings
{
    QUrl listUrl;        // vacío: sin catálogo
    QUrl signatureUrl;   // listUrl + ".minisig"
    minisign::PublicKey key;
    QString cacheDir;    // <plugins>/Config/catalog
    bool enabled() const { return listUrl.isValid() && !listUrl.isEmpty() && key.isValid(); }
};

// Con EDITA_PLUGIN_CATALOG_URL (una URL, una ruta o "none") y EDITA_PLUGIN_CATALOG_PUBKEY
// (otra clave: para las pruebas o un catálogo propio)
Settings settings(const QString& userPluginsDir);

// Para qué Edita sirven los binarios de este equipo
struct Runtime
{
    QString arch;       // "linux-x86_64"
    int pluginApi = 0;  // EDITA_PLUGIN_API_VERSION
    QString qtVersion;  // el Qt con el que se ejecuta Edita
};
// Este equipo y este Edita (con su EDITA_PLUGIN_API_VERSION, de edita_plugin.h)
Runtime currentRuntime(int pluginApi);

// Comprueba una lista descargada (firma, formato y que su versión no sea menor que
// `knownVersion`) y la lee
bool accept(const QByteArray& json, const QByteArray& signature, const minisign::PublicKey& key, const QString& knownVersion,
            PluginCatalog& catalog, QString* error = nullptr);

// Los binarios que este Edita no puede cargar pasan a compilarse con su receta (o, sin
// receta, se quitan de la lista). Devuelve por qué no sirven los binarios (vacío si sirven).
QString adapt(PluginCatalog& catalog, const Runtime& runtime);

// La última lista aceptada (comprobando su firma otra vez)
bool loadCached(const Settings& settings, PluginCatalog& catalog, QString* error = nullptr);
bool store(const Settings& settings, const QByteArray& json, const QByteArray& signature, QString* error = nullptr);

// La lista que trae Edita y la del catálogo: los de Edita primero; del catálogo, los que no
// estén ya. La versión y los datos del catálogo, los del catálogo.
PluginCatalog merge(const PluginCatalog& bundled, const PluginCatalog& remote);

// recipes.zip del catálogo, comprobado con el SHA-256 de la lista firmada, extraído en
// <cacheDir>/recipes-<sha>/ (si ya está, no se vuelve a extraer). Devuelve esa carpeta.
QString recipesDir(const Settings& settings, const PluginCatalog& catalog);
bool unpackRecipes(const Settings& settings, const PluginCatalog& catalog, const QByteArray& zip, QString* error = nullptr);

} // namespace edita::remote_catalog
