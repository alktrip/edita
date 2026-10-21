// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 15: plugins de Notepad++ compilados en el equipo desde su código fuente
// (recetas), con la capa de compatibilidad Win32 del SDK (compat/).

#include "FakePrompter.h"
#include "core/PluginInstaller.h"
#include "core/PluginRecipe.h"
#include "ui/DocStatusBar.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/PluginManager.h"
#include "ui/PluginsAdminDialog.h"

#include <QAction>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QPlainTextEdit>
#include <QProcess>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

#include <zip.h>

#include <map>

using namespace edita;
using edita::test::FakePrompter;
using namespace std::string_literals;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QByteArray makeZip(const std::map<QString, QByteArray>& files)
{
    QTemporaryDir dir;
    const QByteArray path = dir.filePath(QStringLiteral("s.zip")).toUtf8();
    int err = 0;
    zip_t* z = zip_open(path.constData(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    for (const auto& [name, data] : files) {
        zip_source_t* src = zip_source_buffer(z, data.constData(), static_cast<zip_uint64_t>(data.size()), 0);
        zip_file_add(z, name.toUtf8().constData(), src, ZIP_FL_OVERWRITE);
    }
    zip_close(z);
    return readFile(QString::fromUtf8(path));
}

// Un plugin "de Notepad++" pequeño, con la estructura de los de verdad: su
// PluginInterface.h, DllMain que rellena la tabla, ::SendMessage y lstrcpy.
// Trae su propio Scintilla.h (de Windows), que la receta quita.
QByteArray upstreamSource(const char* greeting = "Hola desde Win32")
{
    const QByteArray pluginInterface = R"cpp(#pragma once
#include <windows.h>
#include "Scintilla.h"
#define NPPMSG (WM_USER + 1000)
#define NPPM_GETCURRENTSCINTILLA (NPPMSG + 4)
struct NppData { HWND _nppHandle; HWND _scintillaMainHandle; HWND _scintillaSecondHandle; };
typedef void (__cdecl * PFUNCPLUGINCMD)();
struct ShortcutKey { bool _isCtrl; bool _isAlt; bool _isShift; UCHAR _key; };
struct FuncItem { TCHAR _itemName[64]; PFUNCPLUGINCMD _pFunc; int _cmdID; bool _init2Check; ShortcutKey* _pShKey; };
extern "C" __declspec(dllexport) void setInfo(NppData);
extern "C" __declspec(dllexport) const TCHAR* getName();
extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int*);
extern "C" __declspec(dllexport) void beNotified(SCNotification*);
extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM);
)cpp";
    QByteArray plugin = R"cpp(#include "PluginInterface.h"
NppData nppData;
FuncItem funcItem[2];
static long notified = 0;
HWND currentScintilla() {
    int which = 0;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return which == 0 ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}
void greet() {
    HWND sci = currentScintilla();
    ::SendMessage(sci, SCI_REPLACESEL, 0, (LPARAM)"GREETING");
}
void upper() { ::SendMessage(currentScintilla(), SCI_UPPERCASE, 0, 0); }
BOOL APIENTRY DllMain(HANDLE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        lstrcpy(funcItem[0]._itemName, TEXT("Greet"));
        funcItem[0]._pFunc = greet;
        lstrcpy(funcItem[1]._itemName, TEXT("Upper"));
        funcItem[1]._pFunc = upper;
    }
    return TRUE;
}
extern "C" __declspec(dllexport) void setInfo(NppData data) { nppData = data; }
extern "C" __declspec(dllexport) const TCHAR* getName() { return TEXT("Win32 Demo"); }
extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* n) { *n = 2; return funcItem; }
extern "C" __declspec(dllexport) void beNotified(SCNotification* n) { if (n->nmhdr.code == 1001) ++notified; }
extern "C" __declspec(dllexport) LRESULT messageProc(UINT m, WPARAM, LPARAM) { return m == 1 ? notified : TRUE; }
)cpp";
    plugin.replace("GREETING", greeting);
    return makeZip({{"demo-1.0/src/PluginInterface.h", pluginInterface},
                    {"demo-1.0/src/plugin.cpp", plugin},
                    {"demo-1.0/src/Scintilla.h", "#error esta es la Scintilla.h de Windows; la receta la quita\n"},
                    {"demo-1.0/readme.txt", "Plugin de ejemplo\n"}});
}

void writeRecipe(const QString& dir, const QString& source, const QString& sha, bool withPatch = true,
                 const QString& version = QStringLiteral("1.0"))
{
    QJsonObject recipe{{"folder-name", "Win32Demo"}, {"display-name", "Win32 Demo"}, {"version", version},
                       {"description", "Plugin de N++ de prueba"}, {"author", "Pruebas"}, {"homepage", ""},
                       {"source", QJsonObject{{"url", source}, {"sha256", sha}, {"strip-components", 1}}},
                       {"remove", QJsonArray{"src/Scintilla.h"}},
                       {"overlay", "overlay"},
                       {"build-dir", "edita"},
                       {"requires", QJsonArray{"cmake", "c++"}}};
    if (withPatch)
        recipe["patches"] = QJsonArray{"saludo.patch"};
    writeFile(dir + "/recipe.json", QJsonDocument(recipe).toJson());
    writeFile(dir + "/overlay/edita/CMakeLists.txt",
              "cmake_minimum_required(VERSION 3.20)\nproject(Win32Demo CXX)\n"
              "include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)\n"
              "edita_npp_plugin(Win32Demo SOURCES ../src/plugin.cpp)\n");
    writeFile(dir + "/saludo.patch",
              "--- a/readme.txt\n+++ b/readme.txt\n@@ -1 +1,2 @@\n Plugin de ejemplo\n+Parcheado por la receta\n");
}

} // namespace

class TestRecipes : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    QString userPlugins() const { return _tmp.filePath(QStringLiteral("config/edita/plugins")); }
    QString recipesDir() const { return _tmp.filePath(QStringLiteral("recipes")); }

    PluginsAdminContext context(MainWindow& w) const
    {
        PluginsAdminContext c = w.pluginsAdminContext();
        c.buildRoot = _tmp.filePath(QStringLiteral("cache"));
        return c;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_PLUGIN_PATH", EDITA_PLUGIN_BUILD_DIR);
        qputenv("EDITA_PLUGIN_RECIPES", recipesDir().toUtf8());
        writeFile(_tmp.filePath("list.json"), R"({"version": "1", "npp-plugins": []})");
        qputenv("EDITA_PLUGIN_LIST", _tmp.filePath("list.json").toUtf8());
    }

    void cleanup()
    {
        QDir(userPlugins()).removeRecursively();
        QDir(recipesDir()).removeRecursively();
        QDir(_tmp.filePath("cache")).removeRecursively();
    }

    void recipesAndSdk()
    {
        writeRecipe(recipesDir() + "/Win32Demo", "https://example.org/demo.zip", "abc");
        writeFile(recipesDir() + "/Roto/recipe.json", "{\"folder-name\": \"Roto\"}"); // sin source
        QStringList warnings;
        const std::vector<PluginRecipe> recipes = loadRecipes({recipesDir()}, &warnings);
        QCOMPARE(recipes.size(), std::size_t{1});
        QCOMPARE(warnings.size(), 1);
        const PluginRecipe& r = recipes.front();
        QCOMPARE(r.info.displayName, QStringLiteral("Win32 Demo"));
        QCOMPARE(r.info.version.toString(), QStringLiteral("1"));
        QCOMPARE(r.stripComponents, 1);
        QCOMPARE(r.remove, QStringList{"src/Scintilla.h"});
        QCOMPARE(r.patches, QStringList{"saludo.patch"});
        QCOMPARE(r.info.recipePath, r.path);
        QVERIFY(r.info.describe().contains(QStringLiteral("Built on your computer")));

        // Las recetas de Edita: MIME Tools y XML Tools, con el SHA-256 del código de GitHub
        const std::vector<PluginRecipe> shipped = loadRecipes({QStringLiteral(EDITA_SOURCE_DIR "/data/plugins/recipes")});
        QCOMPARE(shipped.size(), std::size_t{2});
        for (const PluginRecipe& recipe : shipped) {
            QVERIFY(recipe.sourceUrl.startsWith(QStringLiteral("https://github.com/")));
            QCOMPARE(recipe.sourceSha256.size(), 64);
        }

        // El SDK sin instalar: el del código y la Scintilla de third_party
        const PluginSdk sdk = findPluginSdk();
        QVERIFY(sdk.isValid());
        QVERIFY(QFileInfo::exists(sdk.sdkDir + "/compat/windows.h"));
        QVERIFY(QFileInfo::exists(sdk.sdkDir + "/cmake/EditaNppPlugin.cmake"));
        QVERIFY(QFileInfo::exists(sdk.scintillaInclude + "/Scintilla.h"));

        QVERIFY(missingRequirements({"cmake", "c++"}).isEmpty());
        QCOMPARE(missingRequirements({"no-existe-esta-orden", "pkg-config:no-existe-este-modulo"}).size(), 2);
    }

    void builderCompilesAnUpstreamPlugin()
    {
        const QByteArray source = upstreamSource();
        writeRecipe(recipesDir() + "/Win32Demo", "x", plugin_installer::sha256Hex(source));
        PluginRecipe recipe;
        QVERIFY(PluginRecipe::parse(recipesDir() + "/Win32Demo/recipe.json", recipe));
        RecipeBuilder builder(recipe, findPluginSdk(), _tmp.filePath("cache/Win32Demo"));
        QSignalSpy finished(&builder, &RecipeBuilder::finished);
        builder.start(source);
        QVERIFY(finished.wait(180000));
        QVERIFY2(finished.first().at(0).toBool(), qPrintable(builder.log().right(3000)));
        QVERIFY(builder.log().contains(QStringLiteral("Removed from the original: src/Scintilla.h")));
        QVERIFY(builder.log().contains(QStringLiteral("Applying saludo.patch")));
        QVERIFY(readFile(_tmp.filePath("cache/Win32Demo/extracted/demo-1.0/readme.txt")).contains("Parcheado"));
        // El paquete: Win32Demo.so con las exportaciones de Edita
        QVERIFY(QFileInfo::exists(builder.packagePath()));
        QCOMPARE(builder.packageSha256(), plugin_installer::sha256Hex(readFile(builder.packagePath())));
        QVERIFY(plugin_installer::installPackage(readFile(builder.packagePath()), "Win32Demo", builder.packageSha256(),
                                                 userPlugins(), false));
        QProcess nm;
        nm.start(QStringLiteral("nm"), {QStringLiteral("-D"), QStringLiteral("--defined-only"), userPlugins() + "/Win32Demo/Win32Demo.so"});
        QVERIFY(nm.waitForFinished());
        const QByteArray symbols = nm.readAll();
        for (const char* symbol : {"edita_plugin_api_version", "edita_plugin_version", "setInfo", "getFuncsArray", "beNotified"})
            QVERIFY2(symbols.contains(symbol), symbol);

        // Edita lo carga como un plugin más y sus órdenes funcionan (::SendMessage → Scintilla)
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.notifyPluginsReady();
        const PluginManager::Plugin* plugin = nullptr;
        for (const auto& p : w.plugins()->plugins()) {
            if (p->name == QLatin1String("Win32 Demo"))
                plugin = p.get();
        }
        QVERIFY2(plugin, qPrintable(w.pluginWarnings().join('\n')));
        QCOMPARE(plugin->version, QStringLiteral("1"));
        QCOMPARE(plugin->commands.size(), std::size_t{2});
        QCOMPARE(plugin->commands[0].name, QStringLiteral("Greet"));
        w.editor()->sends(SCI_SETTEXT, 0, "");
        w.plugins()->runCommand(plugin->commands[0].cmdId);
        w.editor()->sci(SCI_SELECTALL);
        w.plugins()->runCommand(plugin->commands[1].cmdId);
        QCOMPARE(w.documents().text(w.currentBuffer()), "HOLA DESDE WIN32"s);
        QCOMPARE(plugin->messageProc(1, 0, 0), intptr_t{1}); // NPPN_READY llegó a su beNotified
    }

    void builderUsesTheQtOfEdita()
    {
        // Un SDK con cabeceras de Qt (como el del AppImage): la receta se compila con ellas y
        // con las bibliotecas del Qt en uso, sin buscar el Qt del sistema
        const QString sdkDir = _tmp.filePath("sdk-with-qt");
        QDir().mkpath(sdkDir + "/qt/include");
        const QString source = QStringLiteral(EDITA_SOURCE_DIR "/plugin-sdk");
        for (const char* name : {"compat", "cmake", "edita_plugin.h"})
            QVERIFY(QFile::link(source + '/' + name, sdkDir + '/' + name));
        const QDir scintilla(QStringLiteral(EDITA_SOURCE_DIR "/third_party/scintilla/include"));
        for (const QString& name : scintilla.entryList(QDir::Files))
            QVERIFY(QFile::link(scintilla.filePath(name), sdkDir + '/' + name));
        const QString headers = QLibraryInfo::path(QLibraryInfo::HeadersPath);
        for (const char* module : {"QtCore", "QtGui", "QtWidgets"})
            QVERIFY(QFile::link(headers + '/' + module, sdkDir + "/qt/include/" + module));
        qputenv("EDITA_PLUGIN_SDK", QFile::encodeName(sdkDir));
        const PluginSdk sdk = findPluginSdk();
        qunsetenv("EDITA_PLUGIN_SDK");
        QCOMPARE(sdk.qtInclude, QDir(sdkDir + "/qt/include").absolutePath());
        QCOMPARE(sdk.qtLibDir, QDir(QLibraryInfo::path(QLibraryInfo::LibrariesPath)).absolutePath());
        QVERIFY(recipeBuildTag(sdk) != recipeBuildTag(findPluginSdk()));

        const QByteArray zip = upstreamSource();
        writeRecipe(recipesDir() + "/Win32Demo", "x", plugin_installer::sha256Hex(zip));
        PluginRecipe recipe;
        QVERIFY(PluginRecipe::parse(recipesDir() + "/Win32Demo/recipe.json", recipe));
        RecipeBuilder builder(recipe, sdk, _tmp.filePath("cache/Win32DemoQt"));
        QSignalSpy finished(&builder, &RecipeBuilder::finished);
        builder.start(zip);
        QVERIFY(finished.wait(180000));
        QVERIFY2(finished.first().at(0).toBool(), qPrintable(builder.log().right(3000)));
        QDirIterator caches(_tmp.filePath("cache/Win32DemoQt"), {QStringLiteral("CMakeCache.txt")}, QDir::Files,
                            QDirIterator::Subdirectories);
        QVERIFY(caches.hasNext());
        const QByteArray cache = readFile(caches.next());
        QVERIFY(cache.contains("EDITA_QT_INCLUDE"));
        QVERIFY(!cache.contains("Qt6_DIR")); // find_package(Qt6) no se usó
        QVERIFY(plugin_installer::installPackage(readFile(builder.packagePath()), "Win32Demo", builder.packageSha256(),
                                                 userPlugins(), false));
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        bool loaded = false;
        for (const auto& p : w.plugins()->plugins())
            loaded = loaded || p->name == QLatin1String("Win32 Demo");
        QVERIFY2(loaded, qPrintable(w.pluginWarnings().join('\n')));
    }

    void builderFailures()
    {
        const QByteArray source = upstreamSource();
        writeRecipe(recipesDir() + "/Win32Demo", "x", QString(64, '0'));
        PluginRecipe recipe;
        QVERIFY(PluginRecipe::parse(recipesDir() + "/Win32Demo/recipe.json", recipe));
        // SHA-256 del código que no coincide
        {
            RecipeBuilder builder(recipe, findPluginSdk(), _tmp.filePath("cache/a"));
            QSignalSpy finished(&builder, &RecipeBuilder::finished);
            builder.start(source);
            QCOMPARE(finished.size(), 1);
            QVERIFY(!finished.first().at(0).toBool());
            QVERIFY(finished.first().at(1).toString().contains(QStringLiteral("SHA-256")));
        }
        // Falta algo en el sistema
        recipe.sourceSha256.clear();
        recipe.requires_ << QStringLiteral("pkg-config:no-existe-este-modulo");
        {
            RecipeBuilder builder(recipe, findPluginSdk(), _tmp.filePath("cache/b"));
            QSignalSpy finished(&builder, &RecipeBuilder::finished);
            builder.start(source);
            QCOMPARE(finished.size(), 1);
            QVERIFY(finished.first().at(1).toString().contains(QStringLiteral("no-existe-este-modulo")));
        }
        // Un error de compilación queda en el registro
        recipe.requires_ = {QStringLiteral("cmake")};
        writeFile(recipe.directory() + "/overlay/edita/CMakeLists.txt",
                  "cmake_minimum_required(VERSION 3.20)\nproject(Win32Demo CXX)\n"
                  "include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)\n"
                  "edita_npp_plugin(Win32Demo SOURCES ../src/no_existe.cpp)\n");
        {
            RecipeBuilder builder(recipe, findPluginSdk(), _tmp.filePath("cache/c"));
            QSignalSpy finished(&builder, &RecipeBuilder::finished);
            builder.start(source);
            QVERIFY(finished.wait(120000));
            QVERIFY(!finished.first().at(0).toBool());
            QVERIFY(builder.log().contains(QStringLiteral("no_existe.cpp")));
        }
        // Cancelar
        writeRecipe(recipesDir() + "/Win32Demo", "x", QString());
        QVERIFY(PluginRecipe::parse(recipesDir() + "/Win32Demo/recipe.json", recipe));
        {
            RecipeBuilder builder(recipe, findPluginSdk(), _tmp.filePath("cache/d"));
            QSignalSpy finished(&builder, &RecipeBuilder::finished);
            builder.start(source);
            builder.cancel();
            QVERIFY(finished.size() == 1 || finished.wait(60000));
            QVERIFY(!finished.first().at(0).toBool());
            QVERIFY(finished.first().at(1).toString().contains(QStringLiteral("canceled")));
        }
    }

    void pluginsAdminBuildsAndCaches()
    {
        const QByteArray source = upstreamSource("Desde el administrador");
        writeFile(_tmp.filePath("demo-src.zip"), source);
        writeRecipe(recipesDir() + "/Win32Demo", _tmp.filePath("demo-src.zip"), plugin_installer::sha256Hex(source), false);
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        FakePrompter prompter;
        PluginsAdminDialog dialog(context(w), prompter);
        // En Disponibles, marcado como "se compila"
        QTreeWidget* available = dialog.list(PluginsAdminDialog::Available);
        // (además de las recetas que trae Edita: MIME Tools y XML Tools)
        const QList<QTreeWidgetItem*> found = available->findItems(QStringLiteral("Win32 Demo"), Qt::MatchExactly);
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first()->text(1), QStringLiteral("1 (from source)"));
        QCOMPARE(available->findItems(QStringLiteral("MIME Tools"), Qt::MatchExactly).size(), 1);
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Available, "Win32Demo"));
        prompter.confirmations.enqueue(true);
        QVERIFY2(dialog.installChecked(), qPrintable(prompter.warnings.join('\n') + dialog.lastBuildLog().right(2000)));
        QVERIFY(QFileInfo::exists(userPlugins() + "/Win32Demo/Win32Demo.so"));
        QVERIFY(QFileInfo::exists(userPlugins() + "/local/Win32Demo-1.zip")); // reempaquetado, sin subirlo a ningún sitio
        QVERIFY(dialog.lastBuildLog().contains(QStringLiteral("Building")));
        QCOMPARE(dialog.list(PluginsAdminDialog::Installed)->topLevelItemCount() >= 1, true);
        // Quitarlo y volver a instalarlo: usa el paquete ya compilado
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Installed, "Win32Demo"));
        prompter.confirmations.enqueue(true);
        QVERIFY(dialog.removeChecked(PluginsAdminDialog::Installed));
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Available, "Win32Demo"));
        prompter.confirmations.enqueue(true);
        QVERIFY(dialog.installChecked());
        QVERIFY(dialog.lastBuildLog().contains(QStringLiteral("Package already built")));
        QVERIFY(QFileInfo::exists(userPlugins() + "/Win32Demo/Win32Demo.so"));
        // Compilado con otro Qt (el paquete de Edita instalado, abierto desde el AppImage): se
        // vuelve a compilar, porque ese Qt podría no poder cargarlo
        QCOMPARE(readFile(userPlugins() + "/local/Win32Demo-1.zip.build").trimmed(), recipeBuildTag(findPluginSdk()).toUtf8());
        writeFile(userPlugins() + "/local/Win32Demo-1.zip.build", "Qt 6.99.0 (system)\n");
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Installed, "Win32Demo"));
        prompter.confirmations.enqueue(true);
        QVERIFY(dialog.removeChecked(PluginsAdminDialog::Installed));
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Available, "Win32Demo"));
        prompter.confirmations.enqueue(true);
        QVERIFY(dialog.installChecked());
        QVERIFY(!dialog.lastBuildLog().contains(QStringLiteral("Package already built")));
        QVERIFY(dialog.lastBuildLog().contains(QStringLiteral("Building")));
        QCOMPARE(readFile(userPlugins() + "/local/Win32Demo-1.zip.build").trimmed(), recipeBuildTag(findPluginSdk()).toUtf8());

        // Una versión nueva de la receta: Actualizaciones, y se vuelve a compilar
        writeRecipe(recipesDir() + "/Win32Demo", _tmp.filePath("demo-src.zip"), plugin_installer::sha256Hex(source), false,
                    QStringLiteral("2.0"));
        PluginsAdminContext c = context(w);
        c.loadedVersions.insert(userPlugins() + "/Win32Demo/Win32Demo.so", "1.0");
        PluginsAdminDialog second(c, prompter);
        QCOMPARE(second.list(PluginsAdminDialog::Updates)->topLevelItemCount(), 1);
        QVERIFY(second.setChecked(PluginsAdminDialog::Updates, "Win32Demo"));
        prompter.confirmations.enqueue(true);
        QVERIFY2(second.updateChecked(), qPrintable(prompter.warnings.join('\n')));
        QVERIFY(QFileInfo::exists(userPlugins() + "/local/Win32Demo-2.zip"));
    }

    void realNotepadPlusPlusPlugins_data()
    {
        QTest::addColumn<QString>("folder");
        QTest::addColumn<QString>("repository");
        QTest::addColumn<QString>("tag");
        QTest::addColumn<QString>("prefix");
        QTest::newRow("MIME Tools") << "mimeTools" << "mimetools" << "v3.1" << "mimetools-3.1/";
        QTest::newRow("XML Tools") << "XMLTools" << "xmltools" << "3.1.1.13" << "xmltools-3.1.1.13/";
    }

    void realNotepadPlusPlusPlugins()
    {
        // Las recetas de Edita con el código real (git archive de referencia/plugins, sin red)
        QFETCH(QString, folder);
        QFETCH(QString, repository);
        QFETCH(QString, tag);
        QFETCH(QString, prefix);
        if (folder == QLatin1String("XMLTools") && !missingRequirements({"pkg-config:libxml-2.0"}).isEmpty())
            QSKIP("falta libxml2");
        PluginRecipe recipe;
        QVERIFY(PluginRecipe::parse(QStringLiteral(EDITA_SOURCE_DIR "/data/plugins/recipes/") + folder + "/recipe.json", recipe));
        // Con EDITA_TEST_PLUGIN_ARCHIVES, los .zip descargados de GitHub (con su SHA-256)
        QByteArray source;
        const QString archives = qEnvironmentVariable("EDITA_TEST_PLUGIN_ARCHIVES");
        if (!archives.isEmpty() && QFileInfo::exists(archives + '/' + repository + '-' + tag + ".zip")) {
            source = readFile(archives + '/' + repository + '-' + tag + ".zip");
        } else {
            const QString repo = QStringLiteral(EDITA_SOURCE_DIR "/../referencia/plugins/") + repository;
            if (!QFileInfo::exists(repo + "/.git"))
                QSKIP("no está referencia/plugins (el código original de los plugins)");
            QProcess git;
            git.start(QStringLiteral("git"), {QStringLiteral("-C"), repo, QStringLiteral("archive"), QStringLiteral("--format=zip"),
                                              QStringLiteral("--prefix=") + prefix, tag});
            QVERIFY(git.waitForFinished(60000));
            source = git.readAllStandardOutput();
            recipe.sourceSha256.clear(); // el zip de git archive no es byte a byte el de GitHub
        }
        QVERIFY(!source.isEmpty());
        RecipeBuilder builder(recipe, findPluginSdk(), _tmp.filePath("cache/") + folder);
        QSignalSpy finished(&builder, &RecipeBuilder::finished);
        builder.start(source);
        QVERIFY(finished.wait(300000));
        QVERIFY2(finished.first().at(0).toBool(), qPrintable(builder.log().right(3000)));
        QVERIFY(plugin_installer::installPackage(readFile(builder.packagePath()), folder, builder.packageSha256(), userPlugins(), false));

        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        const PluginManager::Plugin* plugin = nullptr;
        for (const auto& p : w.plugins()->plugins()) {
            if (p->path.endsWith(folder + ".so"))
                plugin = p.get();
        }
        QVERIFY2(plugin, qPrintable(w.pluginWarnings().join('\n')));
        QCOMPARE(plugin->version, recipe.info.version.toString());
        auto run = [&](const QString& name) {
            for (const PluginManager::Command& c : plugin->commands) {
                if (c.name == name)
                    return w.plugins()->runCommand(c.cmdId);
            }
            return false;
        };
        if (folder == QLatin1String("mimeTools")) {
            // El código original de MIME Tools, sin cambios
            QCOMPARE(plugin->name, QStringLiteral("MIME Tools"));
            w.editor()->sends(SCI_SETTEXT, 0, "Hola, Edita ñ");
            w.editor()->sci(SCI_SELECTALL);
            QVERIFY(run(QStringLiteral("Base64 Encode with padding")));
            QCOMPARE(w.documents().text(w.currentBuffer()), "SG9sYSwgRWRpdGEgw7E="s);
            w.editor()->sci(SCI_SELECTALL);
            QVERIFY(run(QStringLiteral("Base64 Decode")));
            QCOMPARE(w.documents().text(w.currentBuffer()), "Hola, Edita ñ"s);
            w.editor()->sends(SCI_SETTEXT, 0, "a b&c");
            w.editor()->sci(SCI_SELECTALL);
            QVERIFY(run(QStringLiteral("URL Encode (Extended)")));
            QCOMPARE(w.documents().text(w.currentBuffer()), "a%20b%26c"s);
        } else {
            QCOMPARE(plugin->name, QStringLiteral("XML Tools"));
            w.editor()->sends(SCI_SETTEXT, 0, "<a><b x=\"1\">t</b><c/></a>");
            w.editor()->sci(SCI_SETUSETABS, 1);
            QVERIFY(run(QStringLiteral("Pretty print")));
            QCOMPARE(w.documents().text(w.currentBuffer()), "<a>\n\t<b x=\"1\">t</b>\n\t<c/>\n</a>"s);
            QVERIFY(run(QStringLiteral("Linearize")));
            QCOMPARE(w.documents().text(w.currentBuffer()), "<a><b x=\"1\">t</b><c/></a>"s);
            w.editor()->sends(SCI_SETTEXT, 0, "<x>&</x>");
            w.editor()->sci(SCI_SELECTALL);
            QVERIFY(run(QStringLiteral("Escape characters in selection (<> → &&lt;&&gt;)")));
            QCOMPARE(w.documents().text(w.currentBuffer()), "&lt;x&gt;&amp;&lt;/x&gt;"s);

            // El menú completo del original (nppMenu.cpp): 29 órdenes y 9 separadores
            int commands = 0;
            for (const PluginManager::Command& c : plugin->commands)
                commands += c.function ? 1 : 0;
            QCOMPARE(commands, 29);
            QCOMPARE(static_cast<int>(plugin->commands.size()), 38);
            auto command = [&](const QString& name) -> const PluginManager::Command* {
                for (const PluginManager::Command& c : plugin->commands) {
                    if (c.name == name)
                        return &c;
                }
                return nullptr;
            };
            for (const char* name : {"First error", "Previous error", "Next error", "Last error", "Tag auto-close",
                                     "Set XML type automatically", "Prevent XXE", "Allow huge files", "Apply to all open files",
                                     "Tokenize (debug)", "Options...", "Debug window..."})
                QVERIFY2(command(QString::fromUtf8(name)), name);
            QVERIFY(command(QStringLiteral("Tag auto-close"))->checked);
            QVERIFY(command(QStringLiteral("Prevent XXE"))->checked);
            QVERIFY(!command(QStringLiteral("Allow huge files"))->checked);

            auto notify = [&](unsigned code, int ch = 0) {
                SCNotification n{};
                n.nmhdr.hwndFrom = w.editor();
                n.nmhdr.code = code;
                n.ch = ch;
                n.updated = SC_UPDATE_SELECTION;
                w.plugins()->notify(n);
            };
            const auto language = [&] { return w.documents().buffer(w.currentBuffer())->language(); };

            // Set XML type automatically y Tag auto-close: al escribir el '>' de <root>
            w.setLanguage(LangType::Text);
            w.editor()->sends(SCI_SETTEXT, 0, "<?xml version=\"1.0\"?>\n<root a=\"1\">");
            w.editor()->sci(SCI_DOCUMENTEND);
            notify(SCN_CHARADDED, '>');
            QCOMPARE(language(), LangType::Xml);
            QCOMPARE(w.documents().text(w.currentBuffer()), "<?xml version=\"1.0\"?>\n<root a=\"1\"></root>"s);
            QCOMPARE(w.editor()->sci(SCI_GETCURRENTPOS), intptr_t{34});
            // No después de />, de </x> ni de comentarios
            w.editor()->sends(SCI_SETTEXT, 0, "<a/>");
            w.editor()->sci(SCI_DOCUMENTEND);
            notify(SCN_CHARADDED, '>');
            QCOMPARE(w.documents().text(w.currentBuffer()), "<a/>"s);
            w.editor()->sends(SCI_SETTEXT, 0, "<!-- c -->");
            w.editor()->sci(SCI_DOCUMENTEND);
            notify(SCN_CHARADDED, '>');
            QCOMPARE(w.documents().text(w.currentBuffer()), "<!-- c -->"s);

            // La ruta del nodo actual, en la barra de estado (xpathOnStatusbar), con los
            // atributos de identidad sin su nombre (dumpAttributeName de serie, false)
            w.editor()->sends(SCI_SETTEXT, 0, "<a><b id=\"x\"><c/></b></a>");
            w.editor()->sci(SCI_GOTOPOS, 15);
            notify(SCN_UPDATEUI);
            QCOMPARE(w.docStatusBar()->languageText(), QStringLiteral("/a/b[x]/c"));

            // Errores: anotaciones bajo sus líneas y First/Last error
            const auto caretLine = [&] {
                return w.editor()->sci(SCI_LINEFROMPOSITION, static_cast<uintptr_t>(w.editor()->sci(SCI_GETCURRENTPOS)));
            };
            w.editor()->sends(SCI_SETTEXT, 0, "<a>\n  <b>\n</a>\n<c>");
            QVERIFY(run(QStringLiteral("Check XML syntax now")));
            const intptr_t firstLine = caretLine();
            QVERIFY(w.editor()->sci(SCI_ANNOTATIONGETTEXT, static_cast<uintptr_t>(firstLine)) > 0);
            QVERIFY(run(QStringLiteral("Last error")));
            const intptr_t lastLine = caretLine();
            QVERIFY(lastLine >= firstLine);
            QVERIFY(run(QStringLiteral("First error")));
            QCOMPARE(caretLine(), firstLine);
            // Al editar, las anotaciones se quitan
            w.editor()->sends(SCI_INSERTTEXT, 0, " ");
            QCOMPARE(w.editor()->sci(SCI_ANNOTATIONGETTEXT, static_cast<uintptr_t>(firstLine)), intptr_t{0});

            // Apply to all open files: Pretty print en todos los documentos
            QVERIFY(run(QStringLiteral("Apply to all open files")));
            QVERIFY(command(QStringLiteral("Apply to all open files"))->checked);
            w.editor()->sends(SCI_SETTEXT, 0, "<a><b/></a>");
            const BufferId first = w.currentBuffer();
            w.newDocument();
            w.editor()->sends(SCI_SETTEXT, 0, "<x><y/></x>");
            const BufferId second = w.currentBuffer();
            QVERIFY(run(QStringLiteral("Pretty print")));
            QCOMPARE(w.documents().text(first), "<a>\n\t<b/>\n</a>"s);
            QCOMPARE(w.documents().text(second).substr(0, 3), "<x>"s);
            QVERIFY(w.documents().text(second).find("<y/>") != std::string::npos);
            QVERIFY(w.documents().text(second) != "<x><y/></x>");
            QCOMPARE(w.currentBuffer(), second); // vuelve al documento en el que se estaba
            QVERIFY(run(QStringLiteral("Apply to all open files")));
            QVERIFY(!command(QStringLiteral("Apply to all open files"))->checked);
            // Las opciones se guardan en XMLTools.ini, con las claves del original
            QVERIFY(QFileInfo::exists(QDir(userPlugins()).filePath("config/XMLTools.ini")));
            QSettings ini(QDir(userPlugins()).filePath("config/XMLTools.ini"), QSettings::IniFormat);
            QCOMPARE(ini.value("XML Tools/doPrettyPrintAllOpenFiles").toInt(), 0);
            QCOMPARE(ini.value("XML Tools/doCloseTag").toInt(), 1);

            // Tokenize (debug): los tokens de QuickXml, en la ventana de depuración
            w.editor()->sends(SCI_SETTEXT, 0, "<a>t</a>");
            QVERIFY(run(QStringLiteral("Tokenize (debug)")));
            QWidget* debug = nullptr;
            for (QWidget* top : QApplication::topLevelWidgets()) {
                if (top->objectName() == QLatin1String("XMLToolsDebug") && top->isVisible())
                    debug = top;
            }
            QVERIFY(debug);
            const auto* log = debug->findChild<QPlainTextEdit*>();
            QVERIFY(log);
            QVERIFY2(log->toPlainText().contains(QLatin1String("TokenizeFast")), qPrintable(log->toPlainText()));
            debug->close();
        }
    }
};

QTEST_MAIN(TestRecipes)
#include "tst_recipes.moc"
