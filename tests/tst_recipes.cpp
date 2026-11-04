// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 15: plugins de Notepad++ compilados en el equipo desde su código fuente
// (recetas), con la capa de compatibilidad Win32 del SDK (compat/).

#include "FakePrompter.h"
#include "core/Minisign.h"
#include "core/PluginInstaller.h"
#include "core/RemoteCatalog.h"
#include "core/PluginRecipe.h"
#include "ui/DocStatusBar.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/PluginManager.h"
#include "ui/PluginsAdminDialog.h"
#include "ui/RecipeCommands.h"

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QDockWidget>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLibraryInfo>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include <QTreeWidget>

#include <zip.h>

#include <functional>
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

// Fase 51: un plugin moderno de N++ (desde la 8.0 son Unicode): FuncItem y getName()
// con wchar_t, mensajes NPPM_* con cadenas wchar_t, la parte de Win32 sin ventanas
// (shlwapi, strsafe, .ini, conversiones, GetModuleFileName) y #include escritos en Windows
// (mayúsculas distintas y '\'), que la receta corrige.
QByteArray unicodeSource()
{
    const QByteArray msgs = R"cpp(#pragma once
#define NPPMSG (WM_USER + 1000)
#define NPPM_GETCURRENTSCINTILLA (NPPMSG + 4)
#define NPPM_SETMENUITEMCHECK (NPPMSG + 40)
#define NPPM_GETPLUGINSCONFIGDIR (NPPMSG + 46)
#define NPPM_MSGTOPLUGIN (NPPMSG + 47)
#define RUNCOMMAND_USER (WM_USER + 3000)
#define NPPM_GETFULLCURRENTPATH (RUNCOMMAND_USER + 1)
struct CommunicationInfo { long internalMsg; const wchar_t* srcModuleName; void* info; };
)cpp";
    const QByteArray pluginInterface = R"cpp(#pragma once
#include "Scintilla.h"
#include "Notepad_plus_msgs.h"
struct NppData { HWND _nppHandle = nullptr; HWND _scintillaMainHandle = nullptr; HWND _scintillaSecondHandle = nullptr; };
typedef void (__cdecl * PFUNCPLUGINCMD)();
struct ShortcutKey { bool _isCtrl = false; bool _isAlt = false; bool _isShift = false; UCHAR _key = 0; };
const int menuItemSize = 64;
struct FuncItem { wchar_t _itemName[menuItemSize] = { '\0' }; PFUNCPLUGINCMD _pFunc = nullptr; int _cmdID = 0; bool _init2Check = false; ShortcutKey *_pShKey = nullptr; };
extern "C" __declspec(dllexport) void setInfo(NppData);
extern "C" __declspec(dllexport) const wchar_t * getName();
extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *);
extern "C" __declspec(dllexport) void beNotified(SCNotification *);
extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lParam);
extern "C" __declspec(dllexport) BOOL isUnicode();
)cpp";
    const QByteArray plugin = R"cpp(#include <Windows.h>
#include <Shlwapi.h>
#include <StrSafe.h>
#include <TCHAR.h>
#include "..\include\PluginInterface.h"
#include "Resource.H"

NppData nppData;
FuncItem funcItem[3];
HINSTANCE module = nullptr;

static HWND currentScintilla() {
    int which = 0;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return which == 0 ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}
static void insertWide(const wchar_t* text) {
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    char* utf8 = new char[bytes];
    WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, bytes, nullptr, nullptr);
    ::SendMessage(currentScintilla(), SCI_REPLACESEL, 0, (LPARAM)utf8);
    delete[] utf8;
}
void insertPath() {
    TCHAR path[MAX_PATH];
    if (!::SendMessage(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)path))
        return;
    TCHAR line[MAX_PATH + 32];
    swprintf(line, MAX_PATH + 32, L"[%s] %s", PathFindFileName(path), GREETING);
    insertWide(line);
}
void saveOption() {
    TCHAR ini[MAX_PATH];
    ::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)ini);
    PathAppend(ini, TEXT("UniDemo.ini"));
    WritePrivateProfileString(TEXT("Opciones"), TEXT("saludo"), GREETING, ini);
    ::SendMessage(nppData._nppHandle, NPPM_SETMENUITEMCHECK, funcItem[1]._cmdID, TRUE);
}
void moduleName() {
    TCHAR path[MAX_PATH];
    GetModuleFileName((HMODULE)module, path, MAX_PATH);
    TCHAR name[64];
    StringCchCopy(name, _countof(name), PathFindFileName(path));
    insertWide(name);
}
BOOL APIENTRY DllMain(HANDLE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        module = (HINSTANCE)hModule;
        lstrcpy(funcItem[0]._itemName, TEXT("Insertar ruta"));
        funcItem[0]._pFunc = insertPath;
        lstrcpy(funcItem[1]._itemName, TEXT("Guardar opción"));
        funcItem[1]._pFunc = saveOption;
        lstrcpy(funcItem[2]._itemName, TEXT("Nombre del módulo"));
        funcItem[2]._pFunc = moduleName;
    }
    return TRUE;
}
extern "C" __declspec(dllexport) void setInfo(NppData data) { nppData = data; }
extern "C" __declspec(dllexport) const wchar_t* getName() { return L"Unicode Demo ñ"; }
extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* n) { *n = 3; return funcItem; }
extern "C" __declspec(dllexport) void beNotified(SCNotification*) {}
extern "C" __declspec(dllexport) LRESULT messageProc(UINT m, WPARAM w, LPARAM l) {
    if (m == NPPM_MSGTOPLUGIN) {
        const CommunicationInfo* info = (const CommunicationInfo*)l;
        const bool names = lstrcmp((const wchar_t*)w, L"UniDemo.dll") == 0 && lstrcmp(info->srcModuleName, L"Otro.dll") == 0;
        return names ? info->internalMsg : -1;
    }
    return TRUE;
}
extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }
)cpp";
    return makeZip({{"unidemo-2.0/include/PluginInterface.h", pluginInterface},
                    {"unidemo-2.0/include/Notepad_plus_msgs.h", msgs},
                    {"unidemo-2.0/src/plugin.cpp", plugin},
                    {"unidemo-2.0/src/resource.h", "#pragma once\n#define GREETING L\"¡hola ñandú!\"\n"}});
}

// Fase 52: un plugin con un panel acoplable hecho con las clases de la plantilla oficial de
// N++ (Window, StaticDialog, DockingDlgInterface: tests/fixtures/npp-plugintemplate, sin
// cambios). Como los .rc todavía no se leen, el panel crea sus controles con CreateWindowEx
QByteArray templatePanelSource()
{
    const QByteArray plugin = R"cpp(#include "PluginInterface.h"
#include "DockingFeature/DockingDlgInterface.h"

#define IDD_PANEL 2500
#define IDC_TEXT 2501
#define IDC_INSERT 2502

NppData nppData;
FuncItem funcItem[1];
HINSTANCE module = nullptr;

static HWND currentScintilla() {
    int which = 0;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return which == 0 ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

class PanelDlg : public DockingDlgInterface {
public:
    PanelDlg() : DockingDlgInterface(IDD_PANEL) {}
protected:
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override {
        switch (message) {
        case WM_INITDIALOG:
            ::CreateWindowEx(0, TEXT("EDIT"), TEXT("desde el panel ñ"), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 4, 4, 150, 22,
                             _hSelf, (HMENU)IDC_TEXT, _hInst, nullptr);
            ::CreateWindowEx(0, TEXT("BUTTON"), TEXT("&Insertar"), WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 160, 4, 70, 22,
                             _hSelf, (HMENU)IDC_INSERT, _hInst, nullptr);
            return TRUE;
        case WM_SIZE:
            ::MoveWindow(::GetDlgItem(_hSelf, IDC_TEXT), 4, 4, LOWORD(lParam) - 90, 22, TRUE);
            ::MoveWindow(::GetDlgItem(_hSelf, IDC_INSERT), LOWORD(lParam) - 80, 4, 70, 22, TRUE);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_INSERT && HIWORD(wParam) == BN_CLICKED) {
                wchar_t text[256];
                ::GetDlgItemText(_hSelf, IDC_TEXT, text, 256);
                char utf8[1024];
                ::WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8), nullptr, nullptr);
                ::SendMessage(currentScintilla(), SCI_REPLACESEL, 0, (LPARAM)utf8);
                return TRUE;
            }
            break;
        case WM_NOTIFY: {
            LPNMHDR header = (LPNMHDR)lParam;
            if (header->hwndFrom == _hParent && LOWORD(header->code) == DMN_CLOSE)
                ::SendMessage(_hParent, NPPM_SETMENUITEMCHECK, funcItem[0]._cmdID, FALSE);
            break;
        }
        default:
            break;
        }
        return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
    }
};

PanelDlg panel;

void showPanel() {
    if (!panel.isCreated()) {
        tTbData data{};
        panel.create(&data);
        data.uMask = DWS_DF_CONT_RIGHT;
        data.pszModuleName = panel.getPluginFileName();
        data.dlgID = 0;
        ::SendMessage(nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&data);
    }
    panel.display();
    ::SendMessage(nppData._nppHandle, NPPM_SETMENUITEMCHECK, funcItem[0]._cmdID, TRUE);
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        module = (HINSTANCE)hModule;
        lstrcpy(funcItem[0]._itemName, TEXT("Panel"));
        funcItem[0]._pFunc = showPanel;
    }
    return TRUE;
}
extern "C" __declspec(dllexport) void setInfo(NppData data) {
    nppData = data;
    panel.init(module, nppData._nppHandle);
}
extern "C" __declspec(dllexport) const wchar_t* getName() { return L"Panel Demo"; }
extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* n) { *n = 1; return funcItem; }
extern "C" __declspec(dllexport) void beNotified(SCNotification*) {}
extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) { return TRUE; }
extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }
)cpp";
    std::map<QString, QByteArray> files{{"paneldemo-1.0/src/PanelDemo.cpp", plugin}};
    const QString fixture = QStringLiteral(EDITA_FIXTURES_DIR "/npp-plugintemplate/src");
    QDirIterator it(fixture, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        files["paneldemo-1.0/src/" + QDir(fixture).relativeFilePath(path)] = readFile(path);
    }
    return makeZip(files);
}

// Fase 53: el panel «Go To Line #» de la plantilla oficial de N++, sin cambios
// (GoToLineDlg.cpp, StaticDialog.cpp y el diálogo de goLine.rc), como lo usa el plugin de
// demostración de N++; solo el archivo principal es de la prueba
QByteArray templateGoToLineSource()
{
    const QByteArray plugin = R"cpp(#include "PluginDefinition.h"
#include "DockingFeature/GoToLineDlg.h"

FuncItem funcItem[nbFunc];
NppData nppData;
HINSTANCE module = nullptr;
DemoDlg goToLine;

void showGoToLine() {
    goToLine.setParent(nppData._nppHandle);
    tTbData data{};
    if (!goToLine.isCreated()) {
        goToLine.create(&data);
        data.uMask = DWS_DF_CONT_RIGHT;
        data.pszModuleName = goToLine.getPluginFileName();
        data.dlgID = 0;
        ::SendMessage(nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&data);
    }
    goToLine.display();
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        module = (HINSTANCE)hModule;
        lstrcpy(funcItem[0]._itemName, TEXT("Go To Line"));
        funcItem[0]._pFunc = showGoToLine;
    }
    return TRUE;
}
extern "C" __declspec(dllexport) void setInfo(NppData data) {
    nppData = data;
    goToLine.init(module, nppData._nppHandle);
}
extern "C" __declspec(dllexport) const wchar_t* getName() { return L"Go To Line Demo"; }
extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* n) { *n = nbFunc; return funcItem; }
extern "C" __declspec(dllexport) void beNotified(SCNotification*) {}
extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) { return TRUE; }
extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }
)cpp";
    std::map<QString, QByteArray> files{{"golinedemo-1.0/src/GoLineDemo.cpp", plugin}};
    const QString fixture = QStringLiteral(EDITA_FIXTURES_DIR "/npp-plugintemplate/src");
    QDirIterator it(fixture, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        files["golinedemo-1.0/src/" + QDir(fixture).relativeFilePath(path)] = readFile(path);
    }
    return makeZip(files);
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

        // Una carpeta de recetas que no existe (la de Edita ya no trae ninguna: están en el
        // catálogo, alktrip/edita-plugins, fase 59): ni recetas ni avisos
        warnings.clear();
        QVERIFY(loadRecipes({_tmp.filePath(QStringLiteral("no-existe"))}, &warnings).empty());
        QVERIFY(warnings.isEmpty());

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

    // Fase 54: una fuente adicional (un submódulo, otro .zip fijado con su SHA-256), la
    // prueba declarada en la receta y las dos órdenes de línea de la CI del catálogo
    void recipeCommandsBuildAndCheck()
    {
        const QString dir = recipesDir() + "/Win32Demo";
        const QByteArray source = upstreamSource();
        const QByteArray extra = makeZip({{"template-abc123/NOTICE.txt", "La plantilla de plugins\n"},
                                          {"template-abc123/Scintilla.h", "#error la receta la quita\n"}});
        writeRecipe(dir, "source.zip", plugin_installer::sha256Hex(source), false);
        writeFile(dir + "/source.zip", source);
        writeFile(dir + "/template.zip", extra);
        auto setRecipe = [&](const std::function<void(QJsonObject&)>& change) {
            QJsonObject recipe = QJsonDocument::fromJson(readFile(dir + "/recipe.json")).object();
            change(recipe);
            writeFile(dir + "/recipe.json", QJsonDocument(recipe).toJson());
        };
        setRecipe([&](QJsonObject& r) {
            r["extra-sources"] = QJsonArray{QJsonObject{{"url", "template.zip"}, {"sha256", plugin_installer::sha256Hex(extra)},
                                                        {"strip-components", 1}, {"dest", "plantilla"}}};
            r["remove"] = QJsonArray{"src/Scintilla.h", "plantilla/Scintilla.h"}; // después de copiar las fuentes
            r["license"] = "GPL-3.0-or-later";
            r["license-files"] = QJsonArray{"readme.txt", "plantilla/NOTICE.txt"};
            r["check"] = QJsonObject{
                {"commands", QJsonArray{"Greet", "Upper"}},
                {"cases", QJsonArray{QJsonObject{{"command", "Upper"}, {"text", "uno\ndos"}, {"expect", "UNO\nDOS"}},
                                     QJsonObject{{"command", "Greet"}, {"select", "none"}, {"text", "!"},
                                                 {"expect", "Hola desde Win32!"}}}}};
        });
        PluginRecipe recipe;
        QVERIFY(PluginRecipe::parse(dir + "/recipe.json", recipe));
        QCOMPARE(recipe.extraSources.size(), std::size_t{1});
        QCOMPARE(recipe.extraSources[0].dest, QStringLiteral("plantilla"));
        QCOMPARE(recipe.checkCommands, (QStringList{"Greet", "Upper"}));
        QCOMPARE(recipe.checks.size(), std::size_t{2});
        QCOMPARE(recipe.license, QStringLiteral("GPL-3.0-or-later"));
        QVERIFY(recipe.checks[0].selectAll);
        QVERIFY(!recipe.checks[1].selectAll);

        // --build-recipe: las rutas relativas a la receta, el paquete y su .sha256
        const QString output = _tmp.filePath("salida");
        QString log;
        QTextStream out(&log);
        QCOMPARE(runRecipeCommandLine({"edita", "--build-recipe", dir + "/recipe.json", "--output", output}, out), 0);
        QVERIFY2(log.contains(QStringLiteral("Extra source verified and copied to plantilla/")), qPrintable(log.right(3000)));
        QVERIFY(log.contains(QStringLiteral("Removed from the original: src/Scintilla.h, plantilla/Scintilla.h")));
        const QString package = output + "/Win32Demo-1.zip";
        QVERIFY(QFileInfo::exists(package));
        QVERIFY(readFile(output + "/work/Win32Demo/extracted/demo-1.0/plantilla/NOTICE.txt").contains("plantilla"));
        QVERIFY(!QFileInfo::exists(output + "/work/Win32Demo/extracted/demo-1.0/plantilla/Scintilla.h"));
        // El paquete lleva los archivos de la licencia con el .so
        QVERIFY(plugin_installer::installPackage(readFile(package), "Win32Demo", plugin_installer::sha256Hex(readFile(package)), userPlugins(), false));
        QVERIFY(QFileInfo::exists(userPlugins() + "/Win32Demo/readme.txt"));
        QVERIFY(QFileInfo::exists(userPlugins() + "/Win32Demo/NOTICE.txt"));
        QDir(userPlugins()).removeRecursively();
        QCOMPARE(readFile(package + ".sha256"), plugin_installer::sha256Hex(readFile(package)).toUtf8() + "  Win32Demo-1.zip\n");

        // --check-plugin: lo instala aparte, lo carga y pasa la prueba; la configuración
        // de quien llama queda como estaba
        const QByteArray configHome = qgetenv("XDG_CONFIG_HOME");
        log.clear();
        QCOMPARE(runRecipeCommandLine({"edita", "--check-plugin", package, "--recipe", dir + "/recipe.json"}, out), 0);
        QVERIFY2(log.contains(QStringLiteral("Plugin: Win32 Demo 1")), qPrintable(log));
        QVERIFY(log.contains(QStringLiteral("Commands: Greet | Upper")));
        QVERIFY(log.contains(QStringLiteral("PASS: \"Upper\"")));
        QVERIFY(log.contains(QStringLiteral("All checks passed")));
        QCOMPARE(qgetenv("XDG_CONFIG_HOME"), configHome);
        QVERIFY(!QFileInfo::exists(userPlugins() + "/Win32Demo"));

        // Una prueba que no pasa y una orden que no existe: código 1 y el detalle
        setRecipe([](QJsonObject& r) {
            r["check"] = QJsonObject{{"commands", QJsonArray{"Saludar"}},
                                     {"cases", QJsonArray{QJsonObject{{"command", "Upper"}, {"text", "a\tb"}, {"expect", "a\tb"}}}}};
        });
        log.clear();
        QCOMPARE(checkPluginCommand(package, dir + "/recipe.json", out), 1);
        QVERIFY(log.contains(QStringLiteral("FAIL: command \"Saludar\"")));
        QVERIFY(log.contains(QStringLiteral("expected: a\\tb")));
        QVERIFY(log.contains(QStringLiteral("got:      A\\tB")));
        QVERIFY(log.contains(QStringLiteral("2 check(s) failed")));

        // Las órdenes sin sus argumentos, y otros argumentos
        log.clear();
        QCOMPARE(runRecipeCommandLine({"edita", "--build-recipe", dir + "/recipe.json"}, out), 2);
        QVERIFY(log.startsWith(QStringLiteral("Usage: edita --build-recipe")));
        QCOMPARE(runRecipeCommandLine({"edita", "archivo.txt"}, out), -1);

        // Fuentes adicionales que no valen: una ruta que sale del código, un SHA-256 que
        // no es el de la receta, menos .zip de los que pide
        setRecipe([](QJsonObject& r) {
            r["extra-sources"] = QJsonArray{QJsonObject{{"url", "template.zip"}, {"sha256", "x"}, {"dest", "../fuera"}}};
        });
        QString error;
        QVERIFY(!PluginRecipe::parse(dir + "/recipe.json", recipe, &error));
        QVERIFY2(error.contains(QStringLiteral("invalid extra source")), qPrintable(error));
        setRecipe([](QJsonObject& r) {
            r["extra-sources"] = QJsonArray{QJsonObject{{"url", "template.zip"}, {"sha256", QString(64, '0')}, {"dest", "plantilla"}}};
        });
        QVERIFY(PluginRecipe::parse(dir + "/recipe.json", recipe));
        for (const std::vector<QByteArray>& extras : {std::vector<QByteArray>{extra}, std::vector<QByteArray>{}}) {
            RecipeBuilder builder(recipe, findPluginSdk(), _tmp.filePath("cache/Win32Demo"));
            QSignalSpy finished(&builder, &RecipeBuilder::finished);
            builder.start(source, extras);
            QVERIFY(finished.count() || finished.wait(30000));
            QVERIFY(!finished.first().at(0).toBool());
            QVERIFY2(builder.log().contains(extras.empty() ? QStringLiteral("the recipe has 1 extra sources and 0 were downloaded")
                                                           : QStringLiteral("is not the recipe's")),
                     qPrintable(builder.log()));
        }
    }

    // Fase 55: con SOURCE_DATE_EPOCH, el mismo código da el mismo paquete aunque se
    // compile en otra carpeta: así la CI del catálogo sabe si un plugin cambió y
    // cualquiera puede comprobar un binario publicado
    // Fase 60: fuentes en la página de código del autor (recode), archivos que el plugin busca
    // junto a él (package-files) y pruebas de varias órdenes seguidas (before)
    void recipeRecodePackageFilesAndBefore()
    {
        const QString dir = recipesDir() + "/Win32Demo";
        const QByteArray source = upstreamSource("\xC4\xE3\xBA\xC3"); // «你好» en GBK
        writeRecipe(dir, "source.zip", plugin_installer::sha256Hex(source), false);
        writeFile(dir + "/source.zip", source);
        auto setRecipe = [&](const std::function<void(QJsonObject&)>& change) {
            QJsonObject recipe = QJsonDocument::fromJson(readFile(dir + "/recipe.json")).object();
            change(recipe);
            writeFile(dir + "/recipe.json", QJsonDocument(recipe).toJson());
        };
        setRecipe([](QJsonObject& r) {
            r["recode"] = QJsonObject{{"from", "GBK"}, {"files", QJsonArray{"src/plugin.cpp"}}};
            r["package-files"] = QJsonArray{QJsonObject{{"from", "readme.txt"}, {"to", "Config/leeme.txt"}}};
            r["check"] = QJsonObject{
                {"commands", QJsonArray{"Greet"}},
                {"cases", QJsonArray{QJsonObject{{"command", "Greet"}, {"select", "none"}, {"text", ""}, {"expect", "你好"}},
                                     QJsonObject{{"before", QJsonArray{"Upper"}}, {"command", "Greet"}, {"text", "ab"},
                                                 {"expect", "你好"}}}}};
        });
        PluginRecipe recipe;
        QVERIFY(PluginRecipe::parse(dir + "/recipe.json", recipe));
        QCOMPARE(recipe.recodeFrom, QStringLiteral("GBK"));
        QCOMPARE(recipe.packageFiles.size(), std::size_t{1});
        QCOMPARE(recipe.checks[1].before, QStringList{"Upper"});

        const QString output = _tmp.filePath("salida60");
        QString log;
        QTextStream out(&log);
        QCOMPARE(runRecipeCommandLine({"edita", "--build-recipe", dir + "/recipe.json", "--output", output}, out), 0);
        QVERIFY2(log.contains(QStringLiteral("Converted from GBK to UTF-8: src/plugin.cpp")), qPrintable(log.right(2000)));
        const QString package = output + "/Win32Demo-1.zip";
        QVERIFY(plugin_installer::installPackage(readFile(package), "Win32Demo", plugin_installer::sha256Hex(readFile(package)),
                                                 userPlugins(), false));
        QCOMPARE(readFile(userPlugins() + "/Win32Demo/Config/leeme.txt"), QByteArray("Plugin de ejemplo\n"));
        QDir(userPlugins()).removeRecursively();
        // El saludo, ya en UTF-8; y la orden de «before» corre antes, en el mismo documento
        log.clear();
        QCOMPARE(checkPluginCommand(package, dir + "/recipe.json", out), 0);
        QVERIFY2(log.contains(QStringLiteral("All checks passed")), qPrintable(log));
        setRecipe([](QJsonObject& r) {
            r["check"] = QJsonObject{{"cases", QJsonArray{QJsonObject{{"before", QJsonArray{"NoExiste"}}, {"command", "Greet"},
                                                                      {"text", ""}, {"expect", "你好"}}}}};
        });
        log.clear();
        QCOMPARE(checkPluginCommand(package, dir + "/recipe.json", out), 1);
        QVERIFY(log.contains(QStringLiteral("FAIL: \"NoExiste\": there is no such command")));

        // Lo que no vale: una codificación desconocida, un archivo que sale del plugin
        QString error;
        setRecipe([](QJsonObject& r) { r["recode"] = QJsonObject{{"from", "klingon"}, {"files", QJsonArray{"src/plugin.cpp"}}}; });
        QVERIFY(!PluginRecipe::parse(dir + "/recipe.json", recipe, &error));
        QVERIFY2(error.contains(QStringLiteral("unknown encoding")), qPrintable(error));
        setRecipe([](QJsonObject& r) {
            r.remove("recode");
            r["package-files"] = QJsonArray{QJsonObject{{"from", "readme.txt"}, {"to", "../fuera.txt"}}};
        });
        QVERIFY(!PluginRecipe::parse(dir + "/recipe.json", recipe, &error));
        QVERIFY2(error.contains(QStringLiteral("invalid package file")), qPrintable(error));
    }

    void packagesAreReproducible()
    {
        const QByteArray source = upstreamSource();
        writeRecipe(recipesDir() + "/Win32Demo", "x", plugin_installer::sha256Hex(source));
        PluginRecipe recipe;
        QVERIFY(PluginRecipe::parse(recipesDir() + "/Win32Demo/recipe.json", recipe));
        qputenv("SOURCE_DATE_EPOCH", "1762128000");
        QStringList shas;
        for (const QString& dir : {QStringLiteral("cache/uno/Win32Demo"), QStringLiteral("cache/otra-carpeta/dos/Win32Demo")}) {
            RecipeBuilder builder(recipe, findPluginSdk(), _tmp.filePath(dir));
            QSignalSpy finished(&builder, &RecipeBuilder::finished);
            builder.start(source);
            QVERIFY(finished.wait(180000));
            QVERIFY2(finished.first().at(0).toBool(), qPrintable(builder.log().right(3000)));
            shas << builder.packageSha256();
            // Ni las carpetas de trabajo ni RUNPATH dentro del .so
            const QString so = QDirIterator(_tmp.filePath(dir + "/build"), {"Win32Demo.so"}, QDir::Files,
                                            QDirIterator::Subdirectories).next();
            const QByteArray binary = readFile(so);
            QVERIFY(!binary.contains(_tmp.filePath(dir).toUtf8()));
            QVERIFY(!binary.contains(findPluginSdk().sdkDir.toUtf8()));
            QProcess readelf;
            readelf.start(QStringLiteral("readelf"), {QStringLiteral("-d"), so});
            QVERIFY(readelf.waitForFinished());
            const QByteArray dynamic = readelf.readAll();
            QVERIFY(dynamic.contains("NEEDED"));
            QVERIFY(!dynamic.contains("RUNPATH") && !dynamic.contains("RPATH"));
        }
        QCOMPARE(shas[0], shas[1]);

        // createZip: la fecha de los archivos no cuenta; sin SOURCE_DATE_EPOCH, sí
        const QString file = _tmp.filePath("cache/a.txt");
        writeFile(file, "contenido\n");
        auto zipWithTime = [&](const QDateTime& time) {
            QFile f(file);
            if (f.open(QIODevice::ReadWrite))
                f.setFileTime(time, QFileDevice::FileModificationTime);
            f.close();
            const QString zip = _tmp.filePath("cache/a.zip");
            plugin_installer::createZip(zip, {{"a.txt", file}});
            return readFile(zip);
        };
        const QDateTime before = QDateTime::currentDateTime().addDays(-3);
        QCOMPARE(zipWithTime(before), zipWithTime(QDateTime::currentDateTime()));
        qunsetenv("SOURCE_DATE_EPOCH");
        QVERIFY(zipWithTime(before) != zipWithTime(QDateTime::currentDateTime()));
    }

    // Fase 56: un plugin del catálogo de Edita cuyo binario no sirve en este equipo (otra
    // arquitectura) se compila con su receta, que sale de recipes.zip del catálogo
    void pluginsAdminBuildsCatalogRecipes()
    {
        const QByteArray keyId = QByteArray::fromHex("8877665544332211");
        const QByteArray seed(32, '\x2a');
        const QString server = _tmp.filePath("servidor");
        qputenv("EDITA_PLUGIN_CATALOG_PUBKEY", minisign::publicKeyFromSeed(keyId, seed).toUtf8());
        qputenv("EDITA_PLUGIN_CATALOG_URL", (server + "/plugins.json").toUtf8());
        auto restore = qScopeGuard([this, server] {
            qputenv("EDITA_PLUGIN_CATALOG_URL", "none");
            qunsetenv("EDITA_PLUGIN_CATALOG_PUBKEY");
            QDir(server).removeRecursively();
            QDir(_tmp.filePath("recetas")).removeRecursively();
        });
        // La receta, en recipes.zip como lo hace catalog.py (recipes/<carpeta>/…)
        const QByteArray source = upstreamSource("Desde el catálogo");
        writeFile(server + "/demo-src.zip", source);
        writeRecipe(_tmp.filePath("recetas/Win32Demo"), server + "/demo-src.zip", plugin_installer::sha256Hex(source), false);
        std::vector<std::pair<QString, QString>> files;
        for (QDirIterator it(_tmp.filePath("recetas"), QDir::Files, QDirIterator::Subdirectories); it.hasNext();) {
            const QString path = it.next();
            files.emplace_back("recipes/" + QDir(_tmp.filePath("recetas")).relativeFilePath(path), path);
        }
        QVERIFY(plugin_installer::createZip(server + "/recipes.zip", files));
        QJsonObject demo{{"folder-name", "Win32Demo"}, {"display-name", "Win32 Demo"}, {"version", "1.0"},
                         {"npp-compatible-versions", "[0.0.1,]"}, {"id", QString(64, 'b')},
                         {"repository", QUrl::fromLocalFile(server + "/no-existe.zip").toString()},
                         {"description", "…"}, {"author", "Pruebas"}, {"homepage", ""}, {"license", "GPL-3.0-or-later"},
                         {"edita-build", "binary"}, {"edita-recipe", "recipes/Win32Demo/recipe.json"}};
        const QByteArray json = QJsonDocument(QJsonObject{
            {"name", "Edita plugin catalog"}, {"version", "2026.11.03.6"}, {"arch", "linux-riscv64"}, {"edita-plugin-api", 1},
            {"edita-qt", "6.8.3"}, {"edita-recipes", QUrl::fromLocalFile(server + "/recipes.zip").toString()},
            {"edita-recipes-sha256", plugin_installer::sha256Hex(readFile(server + "/recipes.zip"))}, {"npp-plugins", QJsonArray{demo}}})
                                    .toJson();
        writeFile(server + "/plugins.json", json);
        writeFile(server + "/plugins.json.minisig", minisign::sign(json, keyId, seed, QStringLiteral("catalog:2026.11.03.6")));

        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        FakePrompter prompter;
        PluginsAdminDialog dialog(context(w), prompter);
        QSignalSpy refreshed(&dialog, &PluginsAdminDialog::catalogRefreshed);
        QVERIFY(refreshed.wait(10000));
        QVERIFY2(refreshed.first().at(0).toBool(), qPrintable(refreshed.first().at(1).toString()));
        const QString status = dialog.findChild<QLabel*>(QStringLiteral("status"))->text();
        QVERIFY2(status.contains(QStringLiteral("This computer builds the plugins of the catalog"))
                     && status.contains(QStringLiteral("linux-riscv64")),
                 qPrintable(status));
        const QList<QTreeWidgetItem*> found =
            dialog.list(PluginsAdminDialog::Available)->findItems(QStringLiteral("Win32 Demo"), Qt::MatchExactly);
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first()->text(1), QStringLiteral("1 (from source)"));
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Available, "Win32Demo"));
        prompter.confirmations.enqueue(true);
        QVERIFY2(dialog.installChecked(), qPrintable(prompter.warnings.join('\n') + dialog.lastBuildLog().right(2000)));
        QVERIFY(QFileInfo::exists(userPlugins() + "/Win32Demo/Win32Demo.so"));
        QVERIFY(dialog.lastBuildLog().contains(QStringLiteral("Building")));
        // Las recetas quedan extraídas una vez por catálogo
        PluginCatalog catalog;
        QVERIFY(PluginCatalog::parse(json, catalog));
        QVERIFY(QFileInfo::exists(remote_catalog::recipesDir(remote_catalog::settings(userPlugins()), catalog)
                                  + "/recipes/Win32Demo/recipe.json"));
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
        const QList<QTreeWidgetItem*> found = available->findItems(QStringLiteral("Win32 Demo"), Qt::MatchExactly);
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first()->text(1), QStringLiteral("1 (from source)"));
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

    void builderCompilesAUnicodePlugin()
    {
        const QByteArray source = unicodeSource();
        const QString dir = recipesDir() + "/UniDemo";
        writeFile(dir + "/recipe.json",
                  QJsonDocument(QJsonObject{{"folder-name", "UniDemo"}, {"display-name", "Unicode Demo"}, {"version", "2.0"},
                                            {"source", QJsonObject{{"url", "x"}, {"sha256", plugin_installer::sha256Hex(source)}}},
                                            {"overlay", "overlay"}, {"build-dir", "edita"}})
                      .toJson());
        writeFile(dir + "/overlay/edita/CMakeLists.txt",
                  "cmake_minimum_required(VERSION 3.20)\nproject(UniDemo CXX)\n"
                  "include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)\n"
                  "edita_npp_plugin(UniDemo UNICODE SOURCES ../src/plugin.cpp)\n");
        PluginRecipe recipe;
        QVERIFY(PluginRecipe::parse(dir + "/recipe.json", recipe));
        QVERIFY(recipe.fixIncludes);
        RecipeBuilder builder(recipe, findPluginSdk(), _tmp.filePath("cache/UniDemo"));
        QSignalSpy finished(&builder, &RecipeBuilder::finished);
        builder.start(source);
        QVERIFY(finished.wait(180000));
        QVERIFY2(finished.first().at(0).toBool(), qPrintable(builder.log().right(3000)));
        // Los #include de Windows, corregidos en el código
        QVERIFY(builder.log().contains(QStringLiteral("Fixed the Windows #include lines")));
        const QByteArray code = readFile(_tmp.filePath("cache/UniDemo/extracted/unidemo-2.0/src/plugin.cpp"));
        for (const char* line : {"#include <windows.h>", "#include <shlwapi.h>", "#include <strsafe.h>", "#include <tchar.h>",
                                 "#include \"../include/PluginInterface.h\"", "#include \"resource.h\""})
            QVERIFY2(code.contains(line), line);
        QVERIFY(plugin_installer::installPackage(readFile(builder.packagePath()), "UniDemo", builder.packageSha256(), userPlugins(), false));

        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.notifyPluginsReady();
        const PluginManager::Plugin* plugin = nullptr;
        for (const auto& p : w.plugins()->plugins()) {
            if (p->path.endsWith(QLatin1String("UniDemo.so")))
                plugin = p.get();
        }
        QVERIFY2(plugin, qPrintable(w.pluginWarnings().join('\n')));
        // getName() y los nombres de las órdenes, de wchar_t a UTF-8
        QCOMPARE(plugin->name, QStringLiteral("Unicode Demo ñ"));
        QCOMPARE(plugin->version, QStringLiteral("2")); // PluginVersion normaliza "2.0"
        QCOMPARE(plugin->commands.size(), std::size_t{3});
        QCOMPARE(plugin->commands[1].name, QStringLiteral("Guardar opción"));

        // NPPM_GETFULLCURRENTPATH con un búfer wchar_t, swprintf de MSVC y PathFindFileName
        const QString file = _tmp.filePath(QStringLiteral("prueba ñ.txt"));
        writeFile(file, "");
        QVERIFY(w.openFile(file) != kInvalidBuffer);
        QVERIFY(w.plugins()->runCommand(plugin->commands[0].cmdId));
        QCOMPARE(w.documents().text(w.currentBuffer()), "[prueba ñ.txt] ¡hola ñandú!"s);

        // NPPM_GETPLUGINSCONFIGDIR, PathAppend, WritePrivateProfileString y el cmdID que
        // Edita dio a la orden (NPPM_SETMENUITEMCHECK)
        QVERIFY(w.plugins()->runCommand(plugin->commands[1].cmdId));
        QCOMPARE(readFile(userPlugins() + "/config/UniDemo.ini"), QByteArray("[Opciones]\nsaludo=¡hola ñandú!\n"));
        QVERIFY(w.plugins()->command(plugin->commands[1].cmdId)->checked);

        // GetModuleFileName con el HINSTANCE de DllMain: el .so del plugin
        w.editor()->sends(SCI_SETTEXT, 0, "");
        QVERIFY(w.plugins()->runCommand(plugin->commands[2].cmdId));
        QCOMPARE(w.documents().text(w.currentBuffer()), "UniDemo.so"s);

        // NPPM_MSGTOPLUGIN: los nombres de módulo, en wchar_t y como en N++ (.dll)
        EditaCommunicationInfo info{7, "Otro.so", nullptr};
        QCOMPARE(plugin->messageProc(NPPM_MSGTOPLUGIN, reinterpret_cast<uintptr_t>("UniDemo.so"), reinterpret_cast<intptr_t>(&info)),
                 intptr_t{7});
    }

    void builderCompilesATemplateDockingPlugin()
    {
        const QByteArray source = templatePanelSource();
        const QString dir = recipesDir() + "/PanelDemo";
        writeFile(dir + "/recipe.json",
                  QJsonDocument(QJsonObject{{"folder-name", "PanelDemo"}, {"display-name", "Panel Demo"}, {"version", "1.0"},
                                            {"source", QJsonObject{{"url", "x"}, {"sha256", plugin_installer::sha256Hex(source)}}},
                                            {"overlay", "overlay"}, {"build-dir", "edita"}})
                      .toJson());
        // La plantilla de N++ tal cual: StaticDialog.cpp se compila sin cambios
        writeFile(dir + "/overlay/edita/CMakeLists.txt",
                  "cmake_minimum_required(VERSION 3.20)\nproject(PanelDemo CXX)\n"
                  "include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)\n"
                  "edita_npp_plugin(PanelDemo UNICODE SOURCES ../src/PanelDemo.cpp ../src/DockingFeature/StaticDialog.cpp\n"
                  "                 INCLUDES ../src)\n");
        PluginRecipe recipe;
        QVERIFY(PluginRecipe::parse(dir + "/recipe.json", recipe));
        RecipeBuilder builder(recipe, findPluginSdk(), _tmp.filePath("cache/PanelDemo"));
        QSignalSpy finished(&builder, &RecipeBuilder::finished);
        builder.start(source);
        QVERIFY(finished.wait(180000));
        QVERIFY2(finished.first().at(0).toBool(), qPrintable(builder.log().right(4000)));
        QVERIFY(plugin_installer::installPackage(readFile(builder.packagePath()), "PanelDemo", builder.packageSha256(), userPlugins(), false));

        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.show();
        w.notifyPluginsReady();
        const PluginManager::Plugin* plugin = nullptr;
        for (const auto& p : w.plugins()->plugins()) {
            if (p->path.endsWith(QLatin1String("PanelDemo.so")))
                plugin = p.get();
        }
        QVERIFY2(plugin, qPrintable(w.pluginWarnings().join('\n')));
        const int panelCommand = plugin->commands[0].cmdId;

        // DockingDlgInterface::create + NPPM_DMMREGASDCKDLG + NPPM_DMMSHOW: un panel de Edita
        // a la derecha, con el nombre del plugin (el diálogo no tiene título sin su .rc)
        QVERIFY(w.plugins()->runCommand(panelCommand));
        QDockWidget* dock = nullptr;
        for (QDockWidget* d : w.findChildren<QDockWidget*>()) {
            if (d->objectName().startsWith(QLatin1String("pluginDock_PanelDemo.so")))
                dock = d;
        }
        QVERIFY(dock);
        QCOMPARE(dock->windowTitle(), QStringLiteral("Panel Demo"));
        QCOMPARE(w.dockWidgetArea(dock), Qt::RightDockWidgetArea);
        QVERIFY(dock->isVisible());
        QVERIFY(w.plugins()->command(panelCommand)->checked);
        // Los controles que creó en WM_INITDIALOG
        auto* line = dock->findChild<QLineEdit*>();
        auto* button = dock->findChild<QPushButton*>();
        QVERIFY(line && button);
        QCOMPARE(line->text(), QStringLiteral("desde el panel ñ"));
        QCOMPARE(button->text(), QStringLiteral("&Insertar"));
        // WM_SIZE: el panel coloca sus controles
        dock->widget()->resize(400, 200);
        QTRY_COMPARE(line->width(), 310);
        QCOMPARE(button->x(), 320);
        // BN_CLICKED → WM_COMMAND → GetDlgItemText → SCI_REPLACESEL
        w.editor()->sends(SCI_SETTEXT, 0, "");
        button->click();
        QCOMPARE(w.documents().text(w.currentBuffer()), "desde el panel ñ"s);
        // Intro en el campo: el botón por defecto
        line->setText(QStringLiteral(" + Intro"));
        QTest::keyClick(line, Qt::Key_Return);
        QCOMPARE(w.documents().text(w.currentBuffer()), "desde el panel ñ + Intro"s);
        // Cerrar el panel: DMN_CLOSE, y el plugin desmarca su orden
        dock->close();
        QTRY_VERIFY(!w.plugins()->command(panelCommand)->checked);
        QVERIFY(!dock->isVisible());
        // Volver a abrirlo: el mismo panel
        QVERIFY(w.plugins()->runCommand(panelCommand));
        QVERIFY(dock->isVisible());
        int panels = 0;
        for (QDockWidget* d : w.findChildren<QDockWidget*>())
            panels += d->objectName().startsWith(QLatin1String("pluginDock_PanelDemo.so")) ? 1 : 0;
        QCOMPARE(panels, 1);
    }

    void builderCompilesTheTemplateGoToLineDialog()
    {
        const QByteArray source = templateGoToLineSource();
        const QString dir = recipesDir() + "/GoLineDemo";
        writeFile(dir + "/recipe.json",
                  QJsonDocument(QJsonObject{{"folder-name", "GoLineDemo"}, {"display-name", "Go To Line Demo"}, {"version", "1.0"},
                                            {"source", QJsonObject{{"url", "x"}, {"sha256", plugin_installer::sha256Hex(source)}}},
                                            {"overlay", "overlay"}, {"build-dir", "edita"}})
                      .toJson());
        writeFile(dir + "/overlay/edita/CMakeLists.txt",
                  "cmake_minimum_required(VERSION 3.20)\nproject(GoLineDemo CXX)\n"
                  "include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)\n"
                  "edita_npp_plugin(GoLineDemo UNICODE\n"
                  "    SOURCES ../src/GoLineDemo.cpp ../src/DockingFeature/StaticDialog.cpp ../src/DockingFeature/GoToLineDlg.cpp\n"
                  "    RESOURCES ../src/DockingFeature/goLine.rc\n"
                  "    INCLUDES ../src)\n");
        PluginRecipe recipe;
        QVERIFY(PluginRecipe::parse(dir + "/recipe.json", recipe));
        RecipeBuilder builder(recipe, findPluginSdk(), _tmp.filePath("cache/GoLineDemo"));
        QSignalSpy finished(&builder, &RecipeBuilder::finished);
        builder.start(source);
        QVERIFY(finished.wait(180000));
        QVERIFY2(finished.first().at(0).toBool(), qPrintable(builder.log().right(4000)));
        // edita_rc convirtió el diálogo del .rc
        QVERIFY2(builder.log().contains(QStringLiteral("edita_rc: 1 diálogos")), qPrintable(builder.log().right(4000)));
        QVERIFY(plugin_installer::installPackage(readFile(builder.packagePath()), "GoLineDemo", builder.packageSha256(), userPlugins(), false));

        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.show();
        w.notifyPluginsReady();
        const PluginManager::Plugin* plugin = nullptr;
        for (const auto& p : w.plugins()->plugins()) {
            if (p->path.endsWith(QLatin1String("GoLineDemo.so")))
                plugin = p.get();
        }
        QVERIFY2(plugin, qPrintable(w.pluginWarnings().join('\n')));
        QVERIFY(w.plugins()->runCommand(plugin->commands[0].cmdId));

        // El panel lleva el título del diálogo del .rc (CAPTION) y sus controles
        QDockWidget* dock = nullptr;
        for (QDockWidget* d : w.findChildren<QDockWidget*>()) {
            if (d->objectName().startsWith(QLatin1String("pluginDock_GoLineDemo.so")))
                dock = d;
        }
        QVERIFY(dock);
        QCOMPARE(dock->windowTitle(), QStringLiteral("Go To Line #"));
        QVERIFY(dock->isVisible());
        QLabel* label = nullptr;
        for (QLabel* l : dock->findChildren<QLabel*>()) {
            if (l->text() == QLatin1String("Go to line :"))
                label = l;
        }
        QVERIFY(label);
        auto* line = dock->findChild<QLineEdit*>();
        auto* go = dock->findChild<QPushButton*>();
        QVERIFY(line && go);
        QCOMPARE(go->text(), QStringLiteral("&Go"));
        // La geometría del .rc (unidades de diálogo): el campo, a la izquierda del botón
        QVERIFY(line->geometry().right() < go->geometry().left());
        QVERIFY(label->geometry().left() < line->geometry().left());
        // ES_NUMBER: solo dígitos
        QTest::keyClicks(line, QStringLiteral("x4"));
        QCOMPARE(line->text(), QStringLiteral("4"));

        // Go: GetDlgItemInt → SCI_GOTOLINE, en el código original de la plantilla
        w.editor()->sends(SCI_SETTEXT, 0, "uno\ndos\ntres\ncuatro\ncinco");
        w.editor()->sci(SCI_GOTOPOS, 0);
        go->click();
        const auto caretLine = [&] {
            return w.editor()->sci(SCI_LINEFROMPOSITION, static_cast<uintptr_t>(w.editor()->sci(SCI_GETCURRENTPOS)));
        };
        QCOMPARE(caretLine(), intptr_t{3});
        // Intro en el campo: el DEFPUSHBUTTON (IDOK)
        line->setText(QStringLiteral("2"));
        QTest::keyClick(line, Qt::Key_Return);
        QCOMPARE(caretLine(), intptr_t{1});
    }

    void fixWindowsIncludes()
    {
        const QString root = _tmp.filePath(QStringLiteral("includes"));
        writeFile(root + "/src/Main.cpp",
                  "#include <Windows.h>\n#include <vector>\n#include <String.h>\n#include \"..\\Common\\Util.H\"\n"
                  "#include \"Local.h\"\n#include \"Falta.h\"\n  #  include \"sub\\Other.h\"\n");
        writeFile(root + "/src/Local.h", "");
        writeFile(root + "/src/sub/other.h", "");
        writeFile(root + "/common/util.h", "");
        writeFile(root + "/src/Latin1.h", "#include <TCHAR.h>\n// \xE9\n");
        QCOMPARE(edita::fixWindowsIncludes(root, {findPluginSdk().sdkDir + "/compat"}), 2);
        QCOMPARE(readFile(root + "/src/Main.cpp"),
                 QByteArray("#include <windows.h>\n#include <vector>\n#include <String.h>\n#include \"../common/util.h\"\n"
                            "#include \"Local.h\"\n#include \"Falta.h\"\n  #  include \"sub/other.h\"\n"));
        // Los bytes que no son UTF-8 se conservan
        QCOMPARE(readFile(root + "/src/Latin1.h"), QByteArray("#include <tchar.h>\n// \xE9\n"));
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
        // Las recetas del catálogo (../edita-plugins, junto a editor/) con el código real (git
        // archive de referencia/plugins, sin red): el port de XML Tools, a fondo
        QFETCH(QString, folder);
        QFETCH(QString, repository);
        QFETCH(QString, tag);
        QFETCH(QString, prefix);
        if (folder == QLatin1String("XMLTools") && !missingRequirements({"pkg-config:libxml-2.0"}).isEmpty())
            QSKIP("falta libxml2");
        const QString recipePath = QStringLiteral(EDITA_SOURCE_DIR "/../edita-plugins/recipes/") + folder + "/recipe.json";
        if (!QFileInfo::exists(recipePath))
            QSKIP("no está ../edita-plugins (el catálogo de plugins)");
        PluginRecipe recipe;
        QVERIFY(PluginRecipe::parse(recipePath, recipe));
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
