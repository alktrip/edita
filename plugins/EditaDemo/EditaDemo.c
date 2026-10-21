/* This file is part of Edita.
 * Copyright (C) 2026 Edita contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Plugin de ejemplo, en C, con la misma estructura que el "Plugin Template"
 * de Notepad++ (PluginDefinition.cpp): un array de FuncItem, órdenes que hablan
 * con Scintilla y con el editor por mensajes, y beNotified para las
 * notificaciones. Sirve de guía para portar plugins de N++: basta con cambiar
 * ::SendMessage(h, …) por data.send(h, …) y wchar_t por UTF-8.
 */

#include <edita_plugin.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static EditaData g_data;
/* Textos en inglés y, si Edita está en español (NPPM_GETNATIVELANGFILENAME), en español */
static int g_spanish = 0;

/* --- Utilidades -------------------------------------------------------------------- */

/* El editor visible (la vista activa), como getCurrentScintilla() del template */
static void* currentEditor(void)
{
    int view = 0;
    g_data.send(g_data.host, NPPM_GETCURRENTSCINTILLA, 0, (intptr_t)&view);
    return view == 0 ? g_data.mainEditor : g_data.subEditor;
}

static intptr_t sci(unsigned int message, uintptr_t wParam, intptr_t lParam)
{
    return g_data.send(currentEditor(), message, wParam, lParam);
}

static void setStatus(const char* text)
{
    g_data.send(g_data.host, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (intptr_t)text);
}

/* --- Órdenes -------------------------------------------------------------------------- */

#define IDM_FILE_NEW 41001 /* menuCmdID.h de N++ */

static void helloEdita(void)
{
    g_data.send(g_data.host, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
    sci(SCI_REPLACESEL, 0, (intptr_t)(g_spanish ? "¡Hola desde un plugin de Edita!\n" : "Hello from an Edita plugin!\n"));
}

static void insertDateTime(void)
{
    char text[64];
    const time_t now = time(NULL);
    strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", localtime(&now));
    sci(SCI_REPLACESEL, 0, (intptr_t)text);
}

static int compareLines(const void* a, const void* b)
{
    return strcmp(*(char* const*)a, *(char* const*)b);
}

/* Ordena las líneas seleccionadas (o todas) */
static void sortLines(void)
{
    intptr_t start = sci(SCI_GETSELECTIONSTART, 0, 0);
    intptr_t end = sci(SCI_GETSELECTIONEND, 0, 0);
    intptr_t firstLine = sci(SCI_LINEFROMPOSITION, (uintptr_t)start, 0);
    intptr_t lastLine = sci(SCI_LINEFROMPOSITION, (uintptr_t)end, 0);
    if (start == end) {
        firstLine = 0;
        lastLine = sci(SCI_GETLINECOUNT, 0, 0) - 1;
    } else if (lastLine > firstLine && sci(SCI_POSITIONFROMLINE, (uintptr_t)lastLine, 0) == end) {
        --lastLine; /* la selección acaba al principio de una línea: esa no cuenta */
    }
    const intptr_t from = sci(SCI_POSITIONFROMLINE, (uintptr_t)firstLine, 0);
    const intptr_t to = sci(SCI_GETLINEENDPOSITION, (uintptr_t)lastLine, 0);
    const size_t count = (size_t)(lastLine - firstLine + 1);
    if (count < 2)
        return;

    char** lines = calloc(count, sizeof(char*));
    size_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        const intptr_t line = firstLine + (intptr_t)i;
        const intptr_t length = sci(SCI_GETLINEENDPOSITION, (uintptr_t)line, 0) - sci(SCI_POSITIONFROMLINE, (uintptr_t)line, 0);
        lines[i] = calloc((size_t)length + 2, 1);
        Sci_TextRangeFull range = {{sci(SCI_POSITIONFROMLINE, (uintptr_t)line, 0), sci(SCI_GETLINEENDPOSITION, (uintptr_t)line, 0)}, lines[i]};
        sci(SCI_GETTEXTRANGEFULL, 0, (intptr_t)&range);
        total += (size_t)length + 2;
    }
    qsort(lines, count, sizeof(char*), compareLines);

    const int eolMode = (int)sci(SCI_GETEOLMODE, 0, 0);
    const char* eol = eolMode == SC_EOL_CRLF ? "\r\n" : (eolMode == SC_EOL_CR ? "\r" : "\n");
    char* text = calloc(total + 1, 1);
    for (size_t i = 0; i < count; ++i) {
        strcat(text, lines[i]);
        if (i + 1 < count)
            strcat(text, eol);
        free(lines[i]);
    }
    free(lines);
    sci(SCI_SETTARGETRANGE, (uintptr_t)from, to);
    sci(SCI_REPLACETARGET, (uintptr_t)-1, (intptr_t)text);
    free(text);
}

static void countWords(void)
{
    const intptr_t length = sci(SCI_GETLENGTH, 0, 0);
    const char* text = (const char*)sci(SCI_GETCHARACTERPOINTER, 0, 0);
    long words = 0;
    int inWord = 0;
    for (intptr_t i = 0; i < length; ++i) {
        const unsigned char c = (unsigned char)text[i];
        const int space = c == ' ' || c == '\t' || c == '\n' || c == '\r';
        if (!space && !inWord)
            ++words;
        inWord = !space;
    }
    char message[96];
    snprintf(message, sizeof message, g_spanish ? "Palabras: %ld" : "Words: %ld", words);
    setStatus(message);
}

static int g_reportOnSave = 0;
static void toggleReportOnSave(void);

/* --- Tabla de órdenes (funcItem del template) ------------------------------------------- */

static EditaShortcutKey g_dateShortcut = {true, true, false, 'D'}; /* Ctrl+Alt+D */

static EditaFuncItem g_funcs[] = {
    {"Hello, Edita", helloEdita, 0, false, NULL},
    {"Insert date and time", insertDateTime, 0, false, &g_dateShortcut},
    {"Sort lines", sortLines, 0, false, NULL},
    {"Count words", countWords, 0, false, NULL},
    {"", NULL, 0, false, NULL}, /* separador */
    {"Report on save", toggleReportOnSave, 0, false, NULL},
};
enum { kFuncCount = sizeof g_funcs / sizeof g_funcs[0], kReportIndex = 5 };

static void toggleReportOnSave(void)
{
    g_reportOnSave = !g_reportOnSave;
    g_data.send(g_data.host, NPPM_SETMENUITEMCHECK, (uintptr_t)g_funcs[kReportIndex].cmdID, g_reportOnSave);
}

/* --- Notificaciones ------------------------------------------------------------------------ */

/* Cuántas notificaciones de cada código han llegado (lo consultan los tests con messageProc) */
static long g_received[64];

static void countNotification(unsigned int code)
{
    if (code >= NPPN_FIRST && code < NPPN_FIRST + 64)
        ++g_received[code - NPPN_FIRST];
}

/* --- Interfaz del plugin ------------------------------------------------------------------- */

EDITA_PLUGIN_EXPORT int edita_plugin_api_version(void)
{
    return EDITA_PLUGIN_API_VERSION;
}

EDITA_PLUGIN_EXPORT const char* edita_plugin_version(void)
{
    return "1.0";
}

EDITA_PLUGIN_EXPORT void setInfo(EditaData data)
{
    static const char* const english[] = {"Hello, Edita", "Insert date and time", "Sort lines", "Count words", "",
                                          "Report on save"};
    static const char* const spanish[] = {"Hola, Edita", "Insertar fecha y hora", "Ordenar líneas", "Contar palabras", "",
                                          "Informar al guardar"};
    char language[64] = "";
    int i;
    g_data = data;
    /* Como un plugin de N++: el idioma es el nombre del nativeLang.xml en uso */
    g_spanish = data.send(data.host, NPPM_GETNATIVELANGFILENAME, sizeof language, (intptr_t)language) > 0
                && strcmp(language, "spanish.xml") == 0;
    for (i = 0; i < kFuncCount; ++i)
        snprintf(g_funcs[i].itemName, sizeof g_funcs[i].itemName, "%s", g_spanish ? spanish[i] : english[i]);
}

EDITA_PLUGIN_EXPORT const char* getName(void)
{
    return "Demo";
}

EDITA_PLUGIN_EXPORT EditaFuncItem* getFuncsArray(int* count)
{
    *count = kFuncCount;
    return g_funcs;
}

EDITA_PLUGIN_EXPORT void beNotified(SCNotification* notification)
{
    const unsigned int code = notification->nmhdr.code;
    if (notification->nmhdr.hwndFrom == g_data.host) {
        countNotification(code);
        if (code == NPPN_FILESAVED && g_reportOnSave) {
            char path[1024] = "";
            g_data.send(g_data.host, NPPM_GETFULLPATHFROMBUFFERID, notification->nmhdr.idFrom, (intptr_t)path);
            char message[1200];
            snprintf(message, sizeof message, g_spanish ? "Guardado: %s (%ld líneas)" : "Saved: %s (%ld lines)", path,
                     (long)sci(SCI_GETLINECOUNT, 0, 0));
            setStatus(message);
        }
    } else if (code == SCN_CHARADDED) {
        ++g_received[63]; /* caracteres escritos en cualquiera de las vistas */
    }
}

/* messageProc(1, código, 0): cuántas notificaciones de ese código (o 63 = SCN_CHARADDED) */
EDITA_PLUGIN_EXPORT intptr_t messageProc(unsigned int message, uintptr_t wParam, intptr_t lParam)
{
    (void)lParam;
    if (message == 1) {
        const uintptr_t index = wParam == 63 ? 63 : wParam - NPPN_FIRST;
        return index < 64 ? g_received[index] : -1;
    }
    return 0;
}
