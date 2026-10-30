// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/PluginCatalog.h"

#include <QObject>
#include <QStringList>

#include <memory>
#include <vector>

class QProcess;

namespace edita {

// Una receta para compilar un plugin de Notepad++ desde su código fuente
// (recipes/<carpeta>/recipe.json):
//   folder-name, display-name, version, description, author, homepage y
//   npp-compatible-versions, como en la lista de plugins;
//   "source": {"url": …, "sha256": …, "strip-components": 1}  el .zip del autor;
//   "remove": [rutas]       archivos del original que se quitan (su Scintilla.h…);
//   "patches": [archivos]   parches (patch -p1), junto a la receta;
//   "overlay": "carpeta"    se copia encima del código (CMakeLists.txt, el port…);
//   "build-dir": "edita"    dónde está el CMakeLists.txt, dentro del código;
//   "requires": ["cmake", "c++", "pkg-config:libxml-2.0", …];
//   "fix-includes": false   no corregir los #include de Windows (ver fixWindowsIncludes).
struct PluginRecipe
{
    PluginInfo info;          // como se lista en el administrador
    QString path;             // el recipe.json
    QString sourceUrl;
    QString sourceSha256;
    int stripComponents = 1;
    QStringList remove;
    QStringList patches;
    QString overlay;
    QString buildDir;
    QStringList requires_;
    bool fixIncludes = true;

    QString directory() const;
    static bool parse(const QString& path, PluginRecipe& recipe, QString* error = nullptr);
};

// Corrige en el código los #include que solo funcionan en Windows, donde el sistema de
// archivos no distingue mayúsculas y la barra es '\': <Windows.h>, "..\\PluginInterface.h",
// "Resource.H". Un #include se reescribe solo si en Linux no encuentra su archivo: con la
// ruta que existe sin distinguir mayúsculas, con el nombre de una cabecera de headerDirs
// (las del SDK) o, si en el código hay un único archivo con ese nombre, con su ruta.
// Devuelve cuántos archivos cambió.
int fixWindowsIncludes(const QString& sourceDir, const QStringList& headerDirs);

// Las recetas de esas carpetas (cada una con <carpeta>/recipe.json). Si dos tienen
// la misma carpeta, gana la primera.
std::vector<PluginRecipe> loadRecipes(const QStringList& directories, QStringList* warnings = nullptr);

// Dónde está el SDK para compilar: (carpeta del SDK, carpeta con Scintilla.h).
// Instalado: <datos>/plugin-sdk; sin instalar: editor/plugin-sdk y la Scintilla del
// código. Vacío si no se encuentra. $EDITA_PLUGIN_SDK tiene prioridad.
struct PluginSdk
{
    QString sdkDir;
    QString scintillaInclude;
    // Las cabeceras de Qt que trae el SDK (solo en el AppImage, en plugin-sdk/qt/include)
    // y la carpeta de las bibliotecas del Qt con el que se ejecuta Edita: las recetas se
    // compilan con ese Qt y no con el del sistema, que puede ser más nuevo
    QString qtInclude;
    QString qtLibDir;
    bool isValid() const { return !sdkDir.isEmpty(); }
};
PluginSdk findPluginSdk();

// Con qué se compila un plugin (el Qt de Edita): un paquete de plugins/local compilado
// con otro no se reutiliza
QString recipeBuildTag(const PluginSdk& sdk);

// Lo que falta de `requires` (programas que no están en el PATH, módulos de pkg-config)
QStringList missingRequirements(const QStringList& requirements);

// Compila una receta: comprueba el .zip del código, lo descomprime, quita, parchea,
// superpone, configura y compila con CMake y empaqueta <carpeta>.so en un .zip
// para instalarlo con plugin_installer::installPackage. Todo en `workDir`.
class RecipeBuilder : public QObject
{
    Q_OBJECT

public:
    RecipeBuilder(PluginRecipe recipe, PluginSdk sdk, QString workDir, QObject* parent = nullptr);
    ~RecipeBuilder() override;

    // Empieza con el .zip del código ya descargado. Termina con finished().
    void start(const QByteArray& sourceZip);
    void cancel();
    bool isRunning() const { return _running; }

    // Tras terminar bien: el paquete y su SHA-256
    const QString& packagePath() const { return _package; }
    const QString& packageSha256() const { return _packageSha; }
    const QString& log() const { return _log; }

signals:
    void output(const QString& text);
    void finished(bool ok, const QString& error);

private:
    void step(const QString& text);
    void fail(const QString& error);
    void runNext();
    void runProcess(const QString& program, const QStringList& arguments, const QString& workingDir);
    bool prepareSources(const QByteArray& sourceZip, QString* error);
    bool package(QString* error);

    PluginRecipe _recipe;
    PluginSdk _sdk;
    QString _workDir;
    QString _sourceDir;
    QString _buildDir;
    QString _package;
    QString _packageSha;
    QString _log;
    std::unique_ptr<QProcess> _process;
    QStringList _pendingPatches;
    int _stage = 0;
    bool _running = false;
    bool _cancelled = false;
};

} // namespace edita
