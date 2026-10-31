// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Une un plugin de Notepad++ compilado con npp_compat.h a la API de Edita:
// exporta las funciones de edita_plugin.h y llama a las del plugin original
// (renombradas a npp_plugin_*).
//
// Sin UNICODE, las estructuras NppData y FuncItem del plugin tienen la misma
// disposición en memoria que las de Edita (TCHAR = char) y las cadenas ya son UTF-8.
// Con UNICODE (TCHAR = wchar_t), aquí se convierte:
//   - getName() y los nombres de las órdenes (FuncItem._itemName), a UTF-8; Edita
//     rellena los cmdID en su copia y se pasan a la tabla del plugin;
//   - los mensajes NPPM_* con cadenas (rutas, nombres, sesiones…), en los dos sentidos;
//   - NPPM_MSGTOPLUGIN, en los dos sentidos, con los nombres de módulo de N++
//     ("Plugin.dll") y de Edita ("Plugin.so").
// En los dos modos, los mensajes a las ventanas de la capa (npp_window.cpp) van a su
// procedimiento, y los de Edita que llevan un HWND (paneles acoplables, editores de
// Scintilla) llevan el QWidget* que espera Edita.

#define EDITA_NPP_COMPAT_GLUE
#include "npp_compat.h"

#include <edita_plugin.h>

#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QString>

#include <algorithm>
#include <string>
#include <vector>

#ifndef EDITA_PLUGIN_VERSION
#define EDITA_PLUGIN_VERSION ""
#endif
#ifndef EDITA_PLUGIN_DISPLAY_NAME
#define EDITA_PLUGIN_DISPLAY_NAME "Plugin"
#endif
#ifndef EDITA_PLUGIN_HOMEPAGE
#define EDITA_PLUGIN_HOMEPAGE ""
#endif

#ifndef NPPM_MODELESSDIALOG
#define NPPM_MODELESSDIALOG (NPPMSG + 12)
#endif

namespace {

EditaData g_data{};

struct NppDataMirror // NppData de PluginInterface.h
{
    HWND nppHandle;
    HWND scintillaMainHandle;
    HWND scintillaSecondHandle;
};

#ifdef UNICODE
struct NppFuncItem // FuncItem de PluginInterface.h, con wchar_t
{
    wchar_t _itemName[EDITA_MENU_ITEM_SIZE];
    EditaPluginCommand _pFunc;
    int _cmdID;
    bool _init2Check;
    EditaShortcutKey* _pShKey;
};

struct NppCommunicationInfo // CommunicationInfo de Notepad_plus_msgs.h
{
    long internalMsg;
    const wchar_t* srcModuleName;
    void* info;
};
#endif

} // namespace

// Las funciones del plugin original (npp_compat.h las renombra)
extern "C" void npp_plugin_setInfo(NppDataMirror);
#ifdef UNICODE
extern "C" const wchar_t* npp_plugin_getName();
extern "C" NppFuncItem* npp_plugin_getFuncsArray(int*);
#else
extern "C" const char* npp_plugin_getName();
extern "C" EditaFuncItem* npp_plugin_getFuncsArray(int*);
#endif
extern "C" void npp_plugin_beNotified(SCNotification*) __attribute__((weak));
extern "C" LRESULT npp_plugin_messageProc(UINT, WPARAM, LPARAM) __attribute__((weak));
// DllMain con enlace de C++ (lo habitual) o de C
BOOL DllMain(HANDLE, DWORD, LPVOID) __attribute__((weak));
extern "C" BOOL edita_compat_DllMain_c(HANDLE, DWORD, LPVOID) __asm__("DllMain") __attribute__((weak));

namespace {

LRESULT sendToEdita(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return hwnd && g_data.send ? g_data.send(hwnd, message, wParam, lParam) : 0;
}

#ifdef UNICODE

// --- Órdenes -------------------------------------------------------------------------------

std::string g_name;
std::vector<EditaFuncItem> g_items;   // la copia en UTF-8 que recibe Edita
NppFuncItem* g_pluginItems = nullptr;  // la tabla del plugin

// Edita da los cmdID al cargar el plugin, después de getFuncsArray: el plugin los usa
// (NPPM_SETMENUITEMCHECK…) y los lee de su tabla
void syncCommandIds()
{
    if (!g_pluginItems)
        return;
    for (std::size_t i = 0; i < g_items.size(); ++i)
        g_pluginItems[i]._cmdID = g_items[i].cmdID;
}

// --- Cadenas ---------------------------------------------------------------------------------

std::string utf8(const wchar_t* text)
{
    return text ? QString::fromWCharArray(text).toStdString() : std::string();
}

std::wstring wide(const char* text)
{
    return text ? QString::fromUtf8(text).toStdWString() : std::wstring();
}

// "Plugin.dll" de N++ ↔ "Plugin.so" de Edita
std::string moduleToEdita(const wchar_t* name)
{
    QString text = QString::fromWCharArray(name ? name : L"");
    if (text.endsWith(QLatin1String(".dll"), Qt::CaseInsensitive))
        text = text.left(text.size() - 4) + QStringLiteral(".so");
    return text.toStdString();
}

std::wstring moduleToPlugin(const char* name)
{
    QString text = QString::fromUtf8(name ? name : "");
    if (text.endsWith(QLatin1String(".so")))
        text = text.left(text.size() - 3) + QStringLiteral(".dll");
    return text.toStdWString();
}

constexpr std::size_t kMaxBuffer = 1 << 20;

// Un mensaje (tamaño, char* búfer) de Edita: el texto, o nullopt si Edita devuelve 0
bool fetchString(UINT message, std::size_t chars, std::string* out)
{
    // Un carácter ocupa como mucho 4 bytes en UTF-8: si no cabe ahí, tampoco cabe en chars
    const std::size_t bytes = std::min(chars, kMaxBuffer) * 4 + 1;
    std::vector<char> buffer(bytes, '\0');
    if (!sendToEdita(g_data.host, message, bytes, reinterpret_cast<LPARAM>(buffer.data())))
        return false;
    *out = buffer.data();
    return true;
}

// (tamaño en wchar_t, wchar_t* búfer): TRUE si cabe, como en N++
LRESULT pathToBuffer(UINT message, WPARAM size, LPARAM lParam)
{
    auto* out = reinterpret_cast<wchar_t*>(lParam);
    std::string text;
    if (!out || size == 0 || !fetchString(message, size, &text))
        return FALSE;
    const std::wstring converted = wide(text.c_str());
    if (converted.size() >= size)
        return FALSE;
    std::copy_n(converted.c_str(), converted.size() + 1, out);
    return TRUE;
}

// (tamaño, wchar_t* búfer o NULL): la longitud; 0 si no cabe
LRESULT lengthAndBuffer(UINT message, WPARAM size, LPARAM lParam)
{
    std::string text;
    if (!fetchString(message, kMaxBuffer, &text))
        return 0;
    const std::wstring converted = wide(text.c_str());
    if (auto* out = reinterpret_cast<wchar_t*>(lParam)) {
        if (size <= converted.size())
            return 0;
        std::copy_n(converted.c_str(), converted.size() + 1, out);
    }
    return static_cast<LRESULT>(converted.size());
}

// (id, wchar_t* búfer o NULL): la longitud, o -1 (NPPM_GETFULLPATHFROMBUFFERID…)
LRESULT stringById(UINT message, WPARAM id, LPARAM lParam, LRESULT missing)
{
    const LRESULT bytes = sendToEdita(g_data.host, message, id, 0);
    if (bytes < 0 || bytes == missing)
        return missing;
    std::vector<char> buffer(static_cast<std::size_t>(bytes) + 1, '\0');
    sendToEdita(g_data.host, message, id, reinterpret_cast<LPARAM>(buffer.data()));
    const std::wstring converted = wide(buffer.data());
    if (auto* out = reinterpret_cast<wchar_t*>(lParam))
        std::copy_n(converted.c_str(), converted.size() + 1, out);
    return static_cast<LRESULT>(converted.size());
}

LRESULT withUtf8(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    const std::string text = utf8(reinterpret_cast<const wchar_t*>(lParam));
    return sendToEdita(hwnd, message, wParam, lParam ? reinterpret_cast<LPARAM>(text.c_str()) : 0);
}

LRESULT withUtf8Both(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    const std::string first = utf8(reinterpret_cast<const wchar_t*>(wParam));
    const std::string second = utf8(reinterpret_cast<const wchar_t*>(lParam));
    return sendToEdita(hwnd, message, wParam ? reinterpret_cast<WPARAM>(first.c_str()) : 0,
                       lParam ? reinterpret_cast<LPARAM>(second.c_str()) : 0);
}

// Los mensajes NPPM_* con cadenas, del plugin (wchar_t) a Edita (UTF-8)
LRESULT sendWide(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case NPPM_GETFULLCURRENTPATH:
    case NPPM_GETCURRENTDIRECTORY:
    case NPPM_GETFILENAME:
    case NPPM_GETNAMEPART:
    case NPPM_GETEXTPART:
    case NPPM_GETCURRENTWORD:
    case NPPM_GETNPPDIRECTORY:
    case NPPM_GETPLUGINSCONFIGDIR:
        return pathToBuffer(message, wParam, lParam);
    case NPPM_GETPLUGINHOMEPATH:
    case NPPM_GETCURRENTCMDLINE:
    case NPPM_GETSETTINGSONCLOUDPATH:
        return lengthAndBuffer(message, wParam, lParam);
    case NPPM_GETFULLPATHFROMBUFFERID:
        return stringById(message, wParam, lParam, -1);
    case NPPM_GETLANGUAGENAME:
    case NPPM_GETLANGUAGEDESC:
        return stringById(message, wParam, lParam, 0);
    case NPPM_DOOPEN:
    case NPPM_SWITCHTOFILE:
    case NPPM_RELOADFILE:
    case NPPM_SAVECURRENTFILEAS:
    case NPPM_SAVEFILE:
    case NPPM_SETSTATUSBAR:
    case NPPM_LOADSESSION:
    case NPPM_SAVECURRENTSESSION:
    case NPPM_GETNBSESSIONFILES:
    case NPPM_CREATELEXER:
        return withUtf8(hwnd, message, wParam, lParam);
    case NPPM_LAUNCHFINDINFILESDLG:
        return withUtf8Both(hwnd, message, wParam, lParam);
    case NPPM_MSGTOPLUGIN: {
        const auto* info = reinterpret_cast<const NppCommunicationInfo*>(lParam);
        const std::string target = moduleToEdita(reinterpret_cast<const wchar_t*>(wParam));
        const std::string source = moduleToEdita(info ? info->srcModuleName : nullptr);
        EditaCommunicationInfo converted{info ? info->internalMsg : 0, source.c_str(), info ? info->info : nullptr};
        return sendToEdita(hwnd, message, reinterpret_cast<WPARAM>(target.c_str()),
                           info ? reinterpret_cast<LPARAM>(&converted) : 0);
    }
    default:
        return sendToEdita(hwnd, message, wParam, lParam);
    }
}

#endif // UNICODE

// --- Paneles acoplables y otras ventanas (los dos modos) -----------------------------------

std::string utf8T(const TCHAR* text)
{
#ifdef UNICODE
    return text ? QString::fromWCharArray(text).toStdString() : std::string();
#else
    return text ? std::string(text) : std::string();
#endif
}

std::string moduleNameToEdita(const TCHAR* name)
{
    QString text = QString::fromStdString(utf8T(name));
    if (text.endsWith(QLatin1String(".dll"), Qt::CaseInsensitive))
        text = text.left(text.size() - 4) + QStringLiteral(".so");
    return text.toStdString();
}

struct NppTbData // tTbData (DockedWidgetData) de Docking.h
{
    HWND hClient;
    const TCHAR* pszName;
    int dlgID;
    UINT uMask;
    HICON hIconTab;
    const TCHAR* pszAddInfo;
    RECT rcFloat;
    int iPrevCont;
    const TCHAR* pszModuleName;
};

constexpr UINT kDmnClose = 1050 + 1; // DMN_CLOSE de dockingResource.h
constexpr UINT kDmnDock = 1050 + 2;
constexpr UINT kDmnFloat = 1050 + 3;

void notifyDock(HWND client, UINT code)
{
    NMHDR header{g_data.host, 0, code};
    edita_compat_windowSend(client, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&header));
}

// Cerrar, acoplar o soltar el panel de Edita: DMN_CLOSE, DMN_DOCK o DMN_FLOAT al diálogo,
// como el DockingManager de N++
class DockWatcher : public QObject
{
public:
    explicit DockWatcher(HWND client, QObject* parent) : QObject(parent), _client(client) {}

    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() == QEvent::Close && edita_compat_isWindow(_client))
            notifyDock(_client, kDmnClose);
        return QObject::eventFilter(object, event);
    }

private:
    HWND _client;
};

LRESULT registerDock(HWND hwnd, const NppTbData* data)
{
    auto* client = data ? static_cast<QWidget*>(edita_compat_widget(data->hClient)) : nullptr;
    if (!client)
        return 0;
    // El título: el del tTbData, el del diálogo o el del plugin
    std::string name = utf8T(data->pszName);
    if (name.empty()) {
        TCHAR title[256] = {};
        GetWindowText(data->hClient, title, 256);
        name = utf8T(title);
    }
    if (name.empty())
        name = EDITA_PLUGIN_DISPLAY_NAME;
    std::string module = moduleNameToEdita(data->pszModuleName);
    if (module.empty()) {
        TCHAR path[MAX_PATH] = {};
        GetModuleFileName(edita_compat_module(), path, MAX_PATH);
        module = moduleNameToEdita(PathFindFileName(path));
    }
    const std::string addInfo = utf8T(data->pszAddInfo);
    EditaDockData dock{client, name.c_str(), data->dlgID, data->uMask, nullptr, data->pszAddInfo ? addInfo.c_str() : nullptr,
                       module.c_str()};
    const LRESULT registered = sendToEdita(hwnd, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&dock));
    if (auto* dockWidget = qobject_cast<QDockWidget*>(client->parentWidget()); registered && dockWidget) {
        HWND hClient = data->hClient;
        dockWidget->installEventFilter(new DockWatcher(hClient, dockWidget));
        QObject::connect(dockWidget, &QDockWidget::topLevelChanged, dockWidget, [hClient, dockWidget](bool floating) {
            if (edita_compat_isWindow(hClient) && dockWidget->isVisible())
                notifyDock(hClient, floating ? kDmnFloat : kDmnDock);
        });
    }
    return registered;
}

// Los mensajes a Edita que llevan ventanas, en los dos modos
bool sendWindowMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    switch (message) {
    case NPPM_DMMREGASDCKDLG:
        *result = registerDock(hwnd, reinterpret_cast<const NppTbData*>(lParam));
        return true;
    case NPPM_DMMSHOW:
    case NPPM_DMMHIDE:
    case NPPM_DMMUPDATEDISPINFO:
        *result = sendToEdita(hwnd, message, wParam, reinterpret_cast<LPARAM>(edita_compat_widget(reinterpret_cast<HWND>(lParam))));
        return true;
    case NPPM_DMMGETPLUGINHWNDBYNAME: {
        const std::string name = utf8T(reinterpret_cast<const TCHAR*>(wParam));
        const std::string module = moduleNameToEdita(reinterpret_cast<const TCHAR*>(lParam));
        void* widget = reinterpret_cast<void*>(sendToEdita(hwnd, message, reinterpret_cast<WPARAM>(name.c_str()),
                                                           lParam ? reinterpret_cast<LPARAM>(module.c_str()) : 0));
        HWND client = widget ? edita_compat_hwndOfWidget(widget) : nullptr;
        *result = reinterpret_cast<LRESULT>(client ? client : static_cast<HWND>(widget));
        return true;
    }
    case NPPM_CREATESCINTILLAHANDLE: {
        // El padre es una ventana de la capa: Edita espera su QWidget*
        HWND parent = reinterpret_cast<HWND>(lParam);
        void* editor = reinterpret_cast<void*>(sendToEdita(hwnd, message, 0, reinterpret_cast<LPARAM>(edita_compat_widget(parent))));
        edita_compat_registerForeign(editor, parent, 0);
        *result = reinterpret_cast<LRESULT>(editor);
        return true;
    }
    case NPPM_GETMENUHANDLE:
        // CheckMenuItem sobre él marca las órdenes del plugin (NPPM_SETMENUITEMCHECK)
        *result = reinterpret_cast<LRESULT>(edita_compat_hostMenu());
        return true;
    case NPPM_MODELESSDIALOG:
        // En N++, para IsDialogMessage; aquí cada diálogo procesa su teclado
        *result = lParam;
        return true;
    default:
        return false;
    }
}

} // namespace

// --- Win32 ---------------------------------------------------------------------------------

extern "C" LRESULT edita_compat_SendMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
#ifdef UNICODE
    syncCommandIds();
#endif
    if (edita_compat_isWindow(hwnd))
        return edita_compat_windowSend(hwnd, message, wParam, lParam);
    if (hwnd && hwnd == g_data.host) {
        LRESULT result = 0;
        if (sendWindowMessage(hwnd, message, wParam, lParam, &result))
            return result;
#ifdef UNICODE
        return sendWide(hwnd, message, wParam, lParam);
#endif
    }
    return sendToEdita(hwnd, message, wParam, lParam);
}

// A las ventanas de la capa, en cola (como en Windows); a Edita, en el momento
extern "C" BOOL edita_compat_PostMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (edita_compat_isWindow(hwnd)) {
        edita_compat_windowPost(hwnd, message, wParam, lParam);
        return TRUE;
    }
    edita_compat_SendMessage(hwnd, message, wParam, lParam);
    return hwnd ? TRUE : FALSE;
}

extern "C" void* edita_compat_createScintilla(void* parentWidget)
{
    return reinterpret_cast<void*>(sendToEdita(g_data.host, NPPM_CREATESCINTILLAHANDLE, 0, reinterpret_cast<LPARAM>(parentWidget)));
}

extern "C" HWND edita_compat_hostHwnd(void)
{
    return g_data.host;
}

// --- Exportaciones de Edita ------------------------------------------------------------------

EDITA_PLUGIN_EXPORT int edita_plugin_api_version(void)
{
    return EDITA_PLUGIN_API_VERSION;
}

EDITA_PLUGIN_EXPORT const char* edita_plugin_version(void)
{
    return EDITA_PLUGIN_VERSION;
}

EDITA_PLUGIN_EXPORT void setInfo(EditaData data)
{
    g_data = data;
    // Las ventanas de Edita: sirven para medir, como padre y en GetMenu
    edita_compat_registerForeign(data.host, nullptr, 0);
    edita_compat_registerForeign(data.mainEditor, nullptr, 0);
    edita_compat_registerForeign(data.subEditor, nullptr, 0);
    // N++ llama a DllMain(DLL_PROCESS_ATTACH) al cargar la DLL; muchos plugins
    // rellenan ahí su tabla de órdenes y guardan el HINSTANCE del módulo
    static bool attached = false;
    if (!attached) {
        attached = true;
        if (DllMain)
            DllMain(edita_compat_module(), DLL_PROCESS_ATTACH, nullptr);
        else if (edita_compat_DllMain_c)
            edita_compat_DllMain_c(edita_compat_module(), DLL_PROCESS_ATTACH, nullptr);
    }
    npp_plugin_setInfo(NppDataMirror{data.host, data.mainEditor, data.subEditor});
}

EDITA_PLUGIN_EXPORT const char* getName(void)
{
#ifdef UNICODE
    g_name = utf8(npp_plugin_getName());
    return g_name.c_str();
#else
    return npp_plugin_getName();
#endif
}

EDITA_PLUGIN_EXPORT EditaFuncItem* getFuncsArray(int* count)
{
#ifdef UNICODE
    g_pluginItems = npp_plugin_getFuncsArray(count);
    const int n = g_pluginItems && count ? *count : 0;
    g_items.assign(static_cast<std::size_t>(std::max(n, 0)), EditaFuncItem{});
    for (std::size_t i = 0; i < g_items.size(); ++i) {
        const NppFuncItem& source = g_pluginItems[i];
        EditaFuncItem& item = g_items[i];
        // El nombre, cortado en un carácter entero si no cabe en 64 bytes
        const std::wstring name(source._itemName, ::wcsnlen(source._itemName, EDITA_MENU_ITEM_SIZE));
        QByteArray bytes = QString::fromStdWString(name).toUtf8();
        while (bytes.size() >= EDITA_MENU_ITEM_SIZE) {
            bytes.chop(1);
            while (!bytes.isEmpty() && (static_cast<unsigned char>(bytes.back()) & 0xC0) == 0x80)
                bytes.chop(1);
            if (!bytes.isEmpty() && (static_cast<unsigned char>(bytes.back()) & 0xC0) == 0xC0)
                bytes.chop(1);
        }
        std::copy_n(bytes.constData(), bytes.size(), item.itemName);
        item.pFunc = source._pFunc;
        item.cmdID = source._cmdID;
        item.init2Check = source._init2Check;
        item.pShKey = source._pShKey;
    }
    return g_items.empty() ? nullptr : g_items.data();
#else
    return npp_plugin_getFuncsArray(count);
#endif
}

EDITA_PLUGIN_EXPORT void beNotified(SCNotification* notification)
{
#ifdef UNICODE
    syncCommandIds();
#endif
    if (npp_plugin_beNotified)
        npp_plugin_beNotified(notification);
}

EDITA_PLUGIN_EXPORT intptr_t messageProc(unsigned int message, uintptr_t wParam, intptr_t lParam)
{
    if (!npp_plugin_messageProc)
        return 0;
#ifdef UNICODE
    syncCommandIds();
    if (message == NPPM_MSGTOPLUGIN && lParam) {
        // De otro plugin (UTF-8, "Otro.so") a este (wchar_t, "Otro.dll")
        const auto* info = reinterpret_cast<const EditaCommunicationInfo*>(lParam);
        const std::wstring target = moduleToPlugin(reinterpret_cast<const char*>(wParam));
        const std::wstring source = moduleToPlugin(info->srcModuleName);
        NppCommunicationInfo converted{info->internalMsg, source.c_str(), info->info};
        return npp_plugin_messageProc(message, reinterpret_cast<WPARAM>(target.c_str()), reinterpret_cast<LPARAM>(&converted));
    }
#endif
    return npp_plugin_messageProc(message, wParam, lParam);
}
