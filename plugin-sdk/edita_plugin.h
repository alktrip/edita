/* This file is part of Edita.
 * Copyright (C) 2026 Edita contributors
 *
 * Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team).
 * This header mirrors the plugin interface of Notepad++ (PluginInterface.h and
 * Notepad_plus_msgs.h) for Linux.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * API de plugins de Edita
 * =======================
 *
 * Un plugin es una biblioteca compartida (.so) en una carpeta con su mismo nombre:
 *
 *     ~/.config/edita/plugins/MiPlugin/MiPlugin.so
 *
 * (también <prefijo>/lib/edita/plugins/ y las carpetas de $EDITA_PLUGIN_PATH).
 *
 * Es la misma idea que los plugins de Notepad++, con las mismas funciones
 * exportadas, los mismos FuncItem/ShortcutKey y los mismos números de mensaje
 * NPPM_* y de notificación NPPN_*. Las diferencias:
 *
 *   - En lugar de ::SendMessage(hwnd, msg, w, l) se usa data.send(target, msg, w, l),
 *     con target = data.host (mensajes NPPM_*) o data.mainEditor / data.subEditor
 *     (mensajes SCI_* de Scintilla).
 *   - Las cadenas son UTF-8 (char*), no wchar_t. Los mensajes que devuelven una
 *     ruta reciben en wParam el tamaño del búfer en bytes, contando el nulo.
 *   - Hay que exportar también edita_plugin_api_version(), que devuelve
 *     EDITA_PLUGIN_API_VERSION, para que Edita no cargue plugins incompatibles.
 *
 * Funciones que exporta el plugin (todas con EDITA_PLUGIN_EXPORT):
 *
 *     int edita_plugin_api_version(void);
 *     void setInfo(EditaData data);                    // se llama primero
 *     const char* getName(void);                       // nombre del submenú
 *     EditaFuncItem* getFuncsArray(int* count);        // órdenes del submenú
 *     void beNotified(SCNotification* notification);   // NPPN_* y SCN_*
 *     intptr_t messageProc(unsigned int message, uintptr_t wParam, intptr_t lParam); // opcional
 *
 * Un FuncItem sin función (pFunc == NULL) es un separador, como en N++.
 */

#ifndef EDITA_PLUGIN_H
#define EDITA_PLUGIN_H

#include <stdbool.h>
#include <stdint.h>

#include "Scintilla.h" /* SCNotification y los mensajes SCI_* */

#ifndef __cplusplus
/* En C, los struct de Scintilla.h no tienen typedef */
typedef struct Sci_TextRangeFull Sci_TextRangeFull;
typedef struct Sci_CharacterRangeFull Sci_CharacterRangeFull;
#endif

#ifdef __cplusplus
#define EDITA_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define EDITA_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#define EDITA_PLUGIN_API_VERSION 1

/* Opcional: la versión del plugin ("1.2.3"), que en N++ es el recurso de versión
 * de la DLL. La usa el administrador de plugins para ofrecer actualizaciones:
 *     EDITA_PLUGIN_EXPORT const char* edita_plugin_version(void) { return "1.2.3"; }
 */

/* --- Datos que recibe el plugin (NppData) ------------------------------------------ */

typedef intptr_t (*EditaSendFn)(void* target, unsigned int message, uintptr_t wParam, intptr_t lParam);

typedef struct EditaData
{
    void* host;          /* _nppHandle */
    void* mainEditor;    /* _scintillaMainHandle */
    void* subEditor;     /* _scintillaSecondHandle */
    EditaSendFn send;    /* ::SendMessage */
} EditaData;

/* --- Órdenes del menú (FuncItem, ShortcutKey) ------------------------------------- */

typedef struct EditaShortcutKey
{
    bool isCtrl;
    bool isAlt;
    bool isShift;
    unsigned char key;   /* código de tecla virtual de Windows: 'A'…'Z', '0'…'9', VK_F1 = 0x70… */
} EditaShortcutKey;

typedef void (*EditaPluginCommand)(void);

#define EDITA_MENU_ITEM_SIZE 64

typedef struct EditaFuncItem
{
    char itemName[EDITA_MENU_ITEM_SIZE];  /* UTF-8 */
    EditaPluginCommand pFunc;             /* NULL = separador */
    int cmdID;                            /* lo rellena Edita */
    bool init2Check;                      /* aparece marcada */
    EditaShortcutKey* pShKey;             /* atajo o NULL */
} EditaFuncItem;

/* --- Mensajes al editor (subconjunto de Notepad_plus_msgs.h, mismos números) ------ */

#define EDITA_WM_USER 0x0400
#define NPPMSG (EDITA_WM_USER + 1000)

#define NPPM_GETCURRENTSCINTILLA  (NPPMSG + 4)   /* (0, int* view): 0 principal, 1 secundaria */
#define NPPM_GETCURRENTLANGTYPE   (NPPMSG + 5)   /* (0, int* langType) */
#define NPPM_SETCURRENTLANGTYPE   (NPPMSG + 6)   /* (0, int langType) */
#define NPPM_GETNBOPENFILES       (NPPMSG + 7)   /* (0, ALL_OPEN_FILES | PRIMARY_VIEW | SECOND_VIEW) */
#define     ALL_OPEN_FILES 0
#define     PRIMARY_VIEW   1
#define     SECOND_VIEW    2
#define NPPM_SETSTATUSBAR         (NPPMSG + 24)  /* (campo, const char* texto) */
#define     STATUSBAR_DOC_TYPE     0
#define     STATUSBAR_DOC_SIZE     1
#define     STATUSBAR_CUR_POS      2
#define     STATUSBAR_EOF_FORMAT   3
#define     STATUSBAR_UNICODE_TYPE 4
#define     STATUSBAR_TYPING_MODE  5
#define NPPM_RELOADFILE           (NPPMSG + 36)  /* (alerta, const char* ruta) */
#define NPPM_SWITCHTOFILE         (NPPMSG + 37)  /* (0, const char* ruta) */
#define NPPM_SAVECURRENTFILE      (NPPMSG + 38)  /* (0, 0) */
#define NPPM_SAVEALLFILES         (NPPMSG + 39)  /* (0, 0) */
#define NPPM_SETMENUITEMCHECK     (NPPMSG + 40)  /* (cmdID, bool) */
#define NPPM_GETPLUGINSCONFIGDIR  (NPPMSG + 46)  /* (tamaño, char* búfer) */
#define NPPM_MENUCOMMAND          (NPPMSG + 48)  /* (0, IDM_* de N++ o cmdID de un plugin) */
#define NPPM_GETNPPVERSION        (NPPMSG + 50)  /* devuelve (mayor << 16) | menor */
#define NPPM_GETFULLPATHFROMBUFFERID (NPPMSG + 58) /* (bufferID, char* búfer o NULL): longitud o -1 */
#define NPPM_GETCURRENTBUFFERID   (NPPMSG + 60)  /* (0, 0) */
#define NPPM_GETBUFFERLANGTYPE    (NPPMSG + 64)  /* (bufferID, 0) */
#define NPPM_SETBUFFERLANGTYPE    (NPPMSG + 65)  /* (bufferID, langType) */
#define NPPM_DOOPEN               (NPPMSG + 77)  /* (0, const char* ruta) */
#define NPPM_GETCURRENTVIEW       (NPPMSG + 88)  /* (0, 0) */

/* Documentos y vistas */
#define NPPM_GETCURRENTDOCINDEX   (NPPMSG + 23)  /* (0, MAIN_VIEW | SUB_VIEW): índice o -1 */
#define     MAIN_VIEW 0
#define     SUB_VIEW  1
#define NPPM_ACTIVATEDOC          (NPPMSG + 28)  /* (vista, índice) */
#define NPPM_MAKECURRENTBUFFERDIRTY (NPPMSG + 44)
#define NPPM_GETPOSFROMBUFFERID   (NPPMSG + 57)  /* (bufferID, vista preferida): (vista << 30) | índice, o -1 */
#define NPPM_GETBUFFERIDFROMPOS   (NPPMSG + 59)  /* (índice, vista): bufferID o 0 */
#define NPPM_RELOADBUFFERID       (NPPMSG + 61)  /* (bufferID, alerta) */
#define NPPM_GETBUFFERENCODING    (NPPMSG + 66)  /* (bufferID, 0): UniMode o -1 */
#define NPPM_SETBUFFERENCODING    (NPPMSG + 67)  /* (bufferID, UniMode) */
#define NPPM_GETBUFFERFORMAT      (NPPMSG + 68)  /* (bufferID, 0): 0 Windows, 1 Mac, 2 Unix, -1 */
#define NPPM_SETBUFFERFORMAT      (NPPMSG + 69)  /* (bufferID, formato) */
#define NPPM_SAVECURRENTFILEAS    (NPPMSG + 78)  /* (copia, const char* ruta) */
#define NPPM_SAVEFILE             (NPPMSG + 94)  /* (0, const char* ruta) */
#define NPPM_GETLANGUAGENAME      (NPPMSG + 83)  /* (langType, char* búfer o NULL): longitud */
#define NPPM_GETLANGUAGEDESC      (NPPMSG + 84)  /* (langType, char* búfer o NULL): longitud */
#define NPPM_GETNBUSERLANG        (NPPMSG + 22)  /* (0, int* primer id o NULL): cuántos */
#define NPPM_LAUNCHFINDINFILESDLG (NPPMSG + 29)  /* (const char* carpeta, const char* filtros) */

/* Sesiones */
#define NPPM_GETNBSESSIONFILES    (NPPMSG + 13)  /* (0, const char* sesión) */
#define NPPM_SAVECURRENTSESSION   (NPPMSG + 16)  /* (0, const char* ruta) */
#define NPPM_LOADSESSION          (NPPMSG + 34)  /* (0, const char* ruta) */

/* Editores de Scintilla para el plugin: en Edita el HWND es un QWidget* (ScintillaEditBase) */
#define NPPM_CREATESCINTILLAHANDLE  (NPPMSG + 20)  /* (0, QWidget* padre o NULL): editor */
#define NPPM_DESTROYSCINTILLAHANDLE (NPPMSG + 21)  /* (0, editor); en N++ está obsoleto */

/* Interfaz */
#define NPPM_GETMENUHANDLE        (NPPMSG + 25)  /* (NPPPLUGINMENU | NPPMAINMENU, 0): QMenu* o QMenuBar* */
#define     NPPPLUGINMENU 0
#define     NPPMAINMENU   1
#define NPPM_HIDETABBAR           (NPPMSG + 51)  /* (0, bool): devuelve el estado anterior */
#define NPPM_ISTABBARHIDDEN       (NPPMSG + 52)
#define NPPM_HIDEMENU             (NPPMSG + 72)
#define NPPM_ISMENUHIDDEN         (NPPMSG + 73)
#define NPPM_HIDESTATUSBAR        (NPPMSG + 74)
#define NPPM_ISSTATUSBARHIDDEN    (NPPMSG + 75)
#define NPPM_GETEDITORDEFAULTFOREGROUNDCOLOR (NPPMSG + 90) /* 0x00BBGGRR */
#define NPPM_GETEDITORDEFAULTBACKGROUNDCOLOR (NPPMSG + 91)
#define NPPM_ISDARKMODEENABLED    (NPPMSG + 107)
#define NPPM_ISAUTOINDENTON       (NPPMSG + 105)
#define NPPM_GETCURRENTCMDLINE    (NPPMSG + 109) /* (tamaño, char* búfer o NULL): longitud */
#define NPPM_GETBOOKMARKID        (NPPMSG + 111)

/* Recursos compartidos entre plugins */
#define NPPM_ALLOCATECMDID        (NPPMSG + 81)  /* (cuántos, int* primero) */
#define NPPM_ALLOCATEMARKER       (NPPMSG + 82)  /* (cuántos, int* primero) */
#define NPPM_ALLOCATEINDICATOR    (NPPMSG + 113) /* (cuántos, int* primero) */
#define NPPM_GETPLUGINHOMEPATH    (NPPMSG + 97)  /* (tamaño, char* búfer o NULL): longitud */
#define NPPM_GETSETTINGSONCLOUDPATH (NPPMSG + 98) /* (tamaño, char* búfer o NULL): 0 = no hay */
#define NPPM_GETNATIVELANGFILENAME (NPPMSG + 116) /* (tamaño, char* búfer o NULL): "spanish.xml"…; 0 = inglés */
#define NPPM_ADDSCNMODIFIEDFLAGS  (NPPMSG + 117) /* (0, SC_MOD_*): más flags para SCN_MODIFIED en las dos vistas */
#define NPPM_CREATELEXER          (NPPMSG + 110) /* (0, const char* nombre): ILexer5* de Lexilla para SCI_SETILEXER */

/* Comunicación entre plugins: el destino recibe NPPM_MSGTOPLUGIN en messageProc */
#define NPPM_MSGTOPLUGIN          (NPPMSG + 47)  /* (const char* módulo destino, EditaCommunicationInfo*) */
typedef struct EditaCommunicationInfo
{
    long internalMsg;            /* definido por el plugin destino */
    const char* srcModuleName;   /* "MiPlugin.so" */
    void* info;
} EditaCommunicationInfo;

/* Paneles acoplables (DockingManager): en Edita hClient es un QWidget* */
#define NPPM_DMMSHOW              (NPPMSG + 30)  /* (0, QWidget*) */
#define NPPM_DMMHIDE              (NPPMSG + 31)  /* (0, QWidget*) */
#define NPPM_DMMUPDATEDISPINFO    (NPPMSG + 32)  /* (0, QWidget*): vuelve a leer el título */
#define NPPM_DMMREGASDCKDLG       (NPPMSG + 33)  /* (0, EditaDockData*) */
#define NPPM_DMMGETPLUGINHWNDBYNAME (NPPMSG + 43) /* (const char* panel, const char* módulo): QWidget* */
#define DWS_ICONTAB         0x00000001
#define DWS_ICONBAR         0x00000002
#define DWS_ADDINFO         0x00000004
#define DWS_DF_CONT_LEFT    (0u << 28)
#define DWS_DF_CONT_RIGHT   (1u << 28)
#define DWS_DF_CONT_TOP     (2u << 28)
#define DWS_DF_CONT_BOTTOM  (3u << 28)
#define DWS_DF_FLOATING     0x80000000u
typedef struct EditaDockData   /* DockedWidgetData (tTbData) */
{
    void* hClient;               /* QWidget* del panel */
    const char* pszName;         /* título (UTF-8) */
    int dlgID;                   /* índice de la orden que abre el panel */
    unsigned int uMask;          /* DWS_* */
    void* hIconTab;              /* QIcon* o NULL */
    const char* pszAddInfo;      /* texto añadido al título si DWS_ADDINFO */
    const char* pszModuleName;   /* "MiPlugin.so" */
} EditaDockData;

#define RUNCOMMAND_USER (EDITA_WM_USER + 3000)
#define NPPM_GETFULLCURRENTPATH   (RUNCOMMAND_USER + 1)  /* (tamaño, char* búfer) */
#define NPPM_GETCURRENTDIRECTORY  (RUNCOMMAND_USER + 2)
#define NPPM_GETFILENAME          (RUNCOMMAND_USER + 3)
#define NPPM_GETNAMEPART          (RUNCOMMAND_USER + 4)
#define NPPM_GETEXTPART           (RUNCOMMAND_USER + 5)
#define NPPM_GETCURRENTWORD       (RUNCOMMAND_USER + 6)
#define NPPM_GETCURRENTLINE       (RUNCOMMAND_USER + 8)  /* devuelve la línea (desde 0) */
#define NPPM_GETCURRENTCOLUMN     (RUNCOMMAND_USER + 9)  /* devuelve la columna (desde 0) */
#define NPPM_GETNPPDIRECTORY      (RUNCOMMAND_USER + 7)  /* carpeta del ejecutable */

/* --- Notificaciones (NPPN_*): nmhdr.hwndFrom = data.host, nmhdr.idFrom = bufferID --- */

#define NPPN_FIRST 1000
#define NPPN_READY              (NPPN_FIRST + 1)
#define NPPN_FILEBEFORECLOSE    (NPPN_FIRST + 3)
#define NPPN_FILEOPENED         (NPPN_FIRST + 4)
#define NPPN_FILECLOSED         (NPPN_FIRST + 5)
#define NPPN_FILEBEFOREOPEN     (NPPN_FIRST + 6)
#define NPPN_FILEBEFORESAVE     (NPPN_FIRST + 7)
#define NPPN_FILESAVED          (NPPN_FIRST + 8)
#define NPPN_SHUTDOWN           (NPPN_FIRST + 9)
#define NPPN_BUFFERACTIVATED    (NPPN_FIRST + 10)
#define NPPN_LANGCHANGED        (NPPN_FIRST + 11)
#define NPPN_READONLYCHANGED    (NPPN_FIRST + 16)
#define NPPN_BEFORESHUTDOWN     (NPPN_FIRST + 19)
#define NPPN_CANCELSHUTDOWN     (NPPN_FIRST + 20)

/* Las notificaciones de Scintilla (SCN_*) llegan tal cual, con
 * nmhdr.hwndFrom = data.mainEditor o data.subEditor. */

#endif /* EDITA_PLUGIN_H */
