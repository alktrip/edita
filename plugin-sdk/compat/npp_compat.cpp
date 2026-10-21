// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Une un plugin de Notepad++ compilado con npp_compat.h a la API de Edita:
// exporta las funciones de edita_plugin.h y llama a las del plugin original
// (renombradas a npp_plugin_*), y da cuerpo a las funciones de Win32 de
// npp_compat.h. Las estructuras NppData y FuncItem del plugin tienen la misma
// disposición en memoria que las de Edita (sin UNICODE, TCHAR = char).

#define EDITA_NPP_COMPAT_GLUE
#include "npp_compat.h"

#include <edita_plugin.h>

#include <QApplication>
#include <QMessageBox>
#include <QString>

#include <utility>
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

namespace {

EditaData g_data{};

struct NppDataMirror // NppData de PluginInterface.h
{
    HWND nppHandle;
    HWND scintillaMainHandle;
    HWND scintillaSecondHandle;
};

} // namespace

// Las funciones del plugin original (npp_compat.h las renombra)
extern "C" void npp_plugin_setInfo(NppDataMirror);
extern "C" const char* npp_plugin_getName();
extern "C" EditaFuncItem* npp_plugin_getFuncsArray(int*);
extern "C" void npp_plugin_beNotified(SCNotification*) __attribute__((weak));
extern "C" LRESULT npp_plugin_messageProc(UINT, WPARAM, LPARAM) __attribute__((weak));
// DllMain con enlace de C++ (lo habitual) o de C
BOOL DllMain(HANDLE, DWORD, LPVOID) __attribute__((weak));
extern "C" BOOL edita_compat_DllMain_c(HANDLE, DWORD, LPVOID) __asm__("DllMain") __attribute__((weak));

// --- Win32 ---------------------------------------------------------------------------------

extern "C" LRESULT edita_compat_SendMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return hwnd && g_data.send ? g_data.send(hwnd, message, wParam, lParam) : 0;
}

extern "C" int edita_compat_MessageBox(HWND, LPCSTR text, LPCSTR caption, UINT type)
{
    QMessageBox box(QApplication::activeWindow());
    box.setWindowTitle(QString::fromUtf8(caption ? caption : ""));
    box.setText(QString::fromUtf8(text ? text : ""));
    switch (type & 0xF0) {
    case MB_ICONHAND: box.setIcon(QMessageBox::Critical); break;
    case MB_ICONQUESTION: box.setIcon(QMessageBox::Question); break;
    case MB_ICONEXCLAMATION: box.setIcon(QMessageBox::Warning); break;
    case MB_ICONASTERISK: box.setIcon(QMessageBox::Information); break;
    default: break;
    }
    std::vector<std::pair<QMessageBox::StandardButton, int>> buttons;
    switch (type & 0xF) {
    case MB_OKCANCEL: buttons = {{QMessageBox::Ok, IDOK}, {QMessageBox::Cancel, IDCANCEL}}; break;
    case MB_ABORTRETRYIGNORE: buttons = {{QMessageBox::Abort, IDABORT}, {QMessageBox::Retry, IDRETRY}, {QMessageBox::Ignore, IDIGNORE}}; break;
    case MB_YESNOCANCEL: buttons = {{QMessageBox::Yes, IDYES}, {QMessageBox::No, IDNO}, {QMessageBox::Cancel, IDCANCEL}}; break;
    case MB_YESNO: buttons = {{QMessageBox::Yes, IDYES}, {QMessageBox::No, IDNO}}; break;
    case MB_RETRYCANCEL: buttons = {{QMessageBox::Retry, IDRETRY}, {QMessageBox::Cancel, IDCANCEL}}; break;
    default: buttons = {{QMessageBox::Ok, IDOK}}; break;
    }
    QMessageBox::StandardButtons standard;
    for (const auto& b : buttons)
        standard |= b.first;
    box.setStandardButtons(standard);
    const int clicked = box.exec();
    for (const auto& b : buttons) {
        if (clicked == b.first)
            return b.second;
    }
    return IDCANCEL;
}

namespace {

// Los diálogos de los recursos .rc del plugin original no existen en Linux
void showMissingDialog()
{
    QString text = QStringLiteral("<b>%1 %2</b><br>Plugin de Notepad++ compilado para Edita.<br><br>"
                                  "Este diálogo del plugin original es de Windows y no está disponible en Linux.")
                       .arg(QString::fromUtf8(EDITA_PLUGIN_DISPLAY_NAME), QString::fromUtf8(EDITA_PLUGIN_VERSION));
    const QString homepage = QString::fromUtf8(EDITA_PLUGIN_HOMEPAGE);
    if (!homepage.isEmpty())
        text += QStringLiteral("<br><a href=\"%1\">%1</a>").arg(homepage);
    QMessageBox::information(QApplication::activeWindow(), QString::fromUtf8(EDITA_PLUGIN_DISPLAY_NAME), text);
}

} // namespace

extern "C" HWND edita_compat_CreateDialogParam(HINSTANCE, LPCSTR, HWND, void*, LPARAM)
{
    showMissingDialog();
    return nullptr;
}

extern "C" INT_PTR edita_compat_DialogBoxParam(HINSTANCE, LPCSTR, HWND, void*, LPARAM)
{
    showMissingDialog();
    return IDCANCEL;
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
    // N++ llama a DllMain(DLL_PROCESS_ATTACH) al cargar la DLL; muchos plugins
    // rellenan ahí su tabla de órdenes
    static bool attached = false;
    if (!attached) {
        attached = true;
        if (DllMain)
            DllMain(nullptr, DLL_PROCESS_ATTACH, nullptr);
        else if (edita_compat_DllMain_c)
            edita_compat_DllMain_c(nullptr, DLL_PROCESS_ATTACH, nullptr);
    }
    npp_plugin_setInfo(NppDataMirror{data.host, data.mainEditor, data.subEditor});
}

EDITA_PLUGIN_EXPORT const char* getName(void)
{
    return npp_plugin_getName();
}

EDITA_PLUGIN_EXPORT EditaFuncItem* getFuncsArray(int* count)
{
    return npp_plugin_getFuncsArray(count);
}

EDITA_PLUGIN_EXPORT void beNotified(SCNotification* notification)
{
    if (npp_plugin_beNotified)
        npp_plugin_beNotified(notification);
}

EDITA_PLUGIN_EXPORT intptr_t messageProc(unsigned int message, uintptr_t wParam, intptr_t lParam)
{
    return npp_plugin_messageProc ? npp_plugin_messageProc(message, wParam, lParam) : 0;
}
