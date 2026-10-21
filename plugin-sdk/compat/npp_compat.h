/* This file is part of Edita.
 * Copyright (C) 2026 Edita contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Compatibilidad para compilar en Linux el código fuente de plugins de Notepad++.
 *
 * El código original incluye <windows.h> (que en esta carpeta solo incluye este
 * archivo) y sus propias PluginInterface.h y Notepad_plus_msgs.h. Aquí están los
 * tipos y las funciones de Win32 que usan los plugins para hablar con N++:
 *   - ::SendMessage(hwnd, …) llega a Edita (data.send) con el mismo hwnd, que es
 *     data.host, data.mainEditor o data.subEditor;
 *   - MessageBox se muestra con Qt;
 *   - los diálogos de recursos (.rc) no existen: CreateDialogParam/DialogBoxParam
 *     muestran un aviso con el nombre del plugin.
 * Las funciones que exporta un plugin de N++ (setInfo, getFuncsArray…) se
 * renombran a npp_plugin_*: npp_compat.cpp exporta las de Edita y las llama.
 *
 * Se compila sin UNICODE: TCHAR es char y las cadenas son UTF-8, como en la API
 * de Edita. Un plugin que use wchar_t explícitamente necesita un parche.
 */

#ifndef EDITA_NPP_COMPAT_H
#define EDITA_NPP_COMPAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* --- Las exportaciones del plugin de N++, con otro nombre ------------------------- */
#ifndef EDITA_NPP_COMPAT_GLUE
#define setInfo npp_plugin_setInfo
#define getName npp_plugin_getName
#define getFuncsArray npp_plugin_getFuncsArray
#define beNotified npp_plugin_beNotified
#define messageProc npp_plugin_messageProc
#define isUnicode npp_plugin_isUnicode
#endif

/* --- Palabras clave de MSVC ---------------------------------------------------- */
#define __declspec(x) EDITA_COMPAT_DECLSPEC_##x
#define EDITA_COMPAT_DECLSPEC_dllexport __attribute__((visibility("default")))
#define EDITA_COMPAT_DECLSPEC_dllimport
#define EDITA_COMPAT_DECLSPEC_noinline __attribute__((noinline))
#define EDITA_COMPAT_DECLSPEC_noreturn __attribute__((noreturn))
#define EDITA_COMPAT_DECLSPEC_selectany __attribute__((weak))
#define EDITA_COMPAT_DECLSPEC_novtable
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#define WINAPI
#define APIENTRY
#define CALLBACK
#define PASCAL
#define FAR
#define NEAR
#define CONST const
#define VOID void
#ifndef _In_
#define _In_
#define _In_opt_
#define _Out_
#define _Out_opt_
#define _Inout_
#define __inout
#endif

/* --- Tipos -------------------------------------------------------------------- */
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned char UCHAR;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef int INT;
typedef long LONG;
typedef unsigned long ULONG;
typedef short SHORT;
typedef unsigned short USHORT;
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;
typedef uintptr_t UINT_PTR;
typedef intptr_t INT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef void* HANDLE;
typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HMODULE;
typedef void* HMENU;
typedef void* HICON;
typedef void* HBITMAP;
typedef void* HBRUSH;
typedef void* HFONT;
typedef void* HDC;
typedef void* HGLOBAL;
typedef void* HCURSOR;
typedef void* HKEY;
typedef DWORD COLORREF;
typedef char CHAR;
typedef wchar_t WCHAR;
typedef char TCHAR;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef wchar_t* LPWSTR;
typedef const wchar_t* LPCWSTR;
typedef char* LPTSTR;
typedef const char* LPCTSTR;
typedef BOOL* LPBOOL;
typedef DWORD* LPDWORD;
typedef long HRESULT;
typedef INT_PTR (*DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR (*FARPROC)(void);

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#define TEXT(s) s
#define _T(s) s
#define __T(s) s
#define _TEXT(s) s

#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb) ((BYTE)(rgb))
#define GetGValue(rgb) ((BYTE)(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) ((BYTE)((rgb) >> 16))
#define MAKEINTRESOURCE(i) ((LPTSTR)((ULONG_PTR)((WORD)(i))))

typedef struct tagRECT { LONG left; LONG top; LONG right; LONG bottom; } RECT, *LPRECT;
typedef struct tagPOINT { LONG x; LONG y; } POINT, *LPPOINT;
typedef struct tagNMHDR { HWND hwndFrom; UINT_PTR idFrom; UINT code; } NMHDR;

/* --- Mensajes y constantes ------------------------------------------------------ */
#define WM_USER 0x0400
#define WM_COMMAND 0x0111
#define WM_INITDIALOG 0x0110
#define WM_CLOSE 0x0010
#define WM_DESTROY 0x0002
#define WM_NOTIFY 0x004E
#define WM_SIZE 0x0005
#define WM_SETREDRAW 0x000B

#define DLL_PROCESS_DETACH 0
#define DLL_PROCESS_ATTACH 1
#define DLL_THREAD_ATTACH 2
#define DLL_THREAD_DETACH 3

#define MB_OK 0x00000000L
#define MB_OKCANCEL 0x00000001L
#define MB_ABORTRETRYIGNORE 0x00000002L
#define MB_YESNOCANCEL 0x00000003L
#define MB_YESNO 0x00000004L
#define MB_RETRYCANCEL 0x00000005L
#define MB_ICONHAND 0x00000010L
#define MB_ICONQUESTION 0x00000020L
#define MB_ICONEXCLAMATION 0x00000030L
#define MB_ICONASTERISK 0x00000040L
#define MB_ICONERROR MB_ICONHAND
#define MB_ICONSTOP MB_ICONHAND
#define MB_ICONWARNING MB_ICONEXCLAMATION
#define MB_ICONINFORMATION MB_ICONASTERISK
#define MB_APPLMODAL 0x00000000L
#define MB_TASKMODAL 0x00002000L
#define MB_TOPMOST 0x00040000L
#define IDOK 1
#define IDCANCEL 2
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
#define IDYES 6
#define IDNO 7
#define IDCLOSE 8
#define IDHELP 9
#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_SHOWWINDOW 0x0040
#define SWP_FRAMECHANGED 0x0020
#define HWND_TOP ((HWND)0)
#define SW_HIDE 0
#define SW_SHOW 5

#define CP_ACP 0
#define CP_UTF8 65001

/* --- Funciones (npp_compat.cpp) ------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif

LRESULT edita_compat_SendMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
int edita_compat_MessageBox(HWND parent, LPCSTR text, LPCSTR caption, UINT type);
HWND edita_compat_CreateDialogParam(HINSTANCE instance, LPCSTR templateName, HWND parent, void* proc, LPARAM init);
INT_PTR edita_compat_DialogBoxParam(HINSTANCE instance, LPCSTR templateName, HWND parent, void* proc, LPARAM init);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
inline LRESULT SendMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return edita_compat_SendMessage(hwnd, message, wParam, lParam);
}
template<typename W, typename L>
inline LRESULT SendMessage(HWND hwnd, UINT message, W wParam, L lParam)
{
    return edita_compat_SendMessage(hwnd, message, (WPARAM)wParam, (LPARAM)lParam);
}
inline BOOL PostMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    edita_compat_SendMessage(hwnd, message, wParam, lParam);
    return TRUE;
}
inline int MessageBox(HWND parent, LPCSTR text, LPCSTR caption, UINT type)
{
    return edita_compat_MessageBox(parent, text, caption, type);
}
template<typename P>
inline HWND CreateDialogParam(HINSTANCE instance, LPCSTR name, HWND parent, P proc, LPARAM init)
{
    return edita_compat_CreateDialogParam(instance, name, parent, (void*)proc, init);
}
template<typename P>
inline INT_PTR DialogBoxParam(HINSTANCE instance, LPCSTR name, HWND parent, P proc, LPARAM init)
{
    return edita_compat_DialogBoxParam(instance, name, parent, (void*)proc, init);
}
template<typename P>
inline INT_PTR DialogBox(HINSTANCE instance, LPCSTR name, HWND parent, P proc)
{
    return edita_compat_DialogBoxParam(instance, name, parent, (void*)proc, 0);
}
#else
#define SendMessage(h, m, w, l) edita_compat_SendMessage((HWND)(h), (UINT)(m), (WPARAM)(w), (LPARAM)(l))
#define MessageBox edita_compat_MessageBox
#endif
#define SendMessageA SendMessage
#define SendMessageW SendMessage
#define MessageBoxA MessageBox

/* Ventanas: los diálogos de recursos no existen; estas no hacen nada */
static inline BOOL EndDialog(HWND, INT_PTR) { return TRUE; }
static inline BOOL GetClientRect(HWND, LPRECT r) { if (r) { r->left = r->top = 0; r->right = r->bottom = 0; } return TRUE; }
static inline BOOL GetWindowRect(HWND, LPRECT r) { return GetClientRect(0, r); }
static inline BOOL ClientToScreen(HWND, LPPOINT) { return TRUE; }
static inline BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT) { return TRUE; }
static inline BOOL ShowWindow(HWND, int) { return TRUE; }
static inline BOOL DestroyWindow(HWND) { return TRUE; }
static inline BOOL IsWindowVisible(HWND) { return FALSE; }
static inline HWND GetDlgItem(HWND, int) { return 0; }
static inline BOOL SetDlgItemText(HWND, int, LPCSTR) { return TRUE; }
static inline void OutputDebugString(LPCSTR text) { fputs(text, stderr); }
static inline DWORD GetLastError(void) { return 0; }

/* Cadenas (TCHAR = char) */
#define lstrcpy strcpy
#define lstrcpyA strcpy
#define lstrcat strcat
#define lstrlen(s) ((int)strlen(s))
#define lstrcmp strcmp
#define lstrcmpi strcasecmp
#define lstrcpyn(d, s, n) (strncpy((d), (s), (size_t)(n) - 1), (d)[(n) - 1] = '\0', (d))
#define _tcslen strlen
#define _tcscpy strcpy
#define _tcscat strcat
#define _tcscmp strcmp
#define _tcsicmp strcasecmp
#define _tcsncmp strncmp
#define _tcsnicmp strncasecmp
#define _tcschr strchr
#define _tcsrchr strrchr
#define _tcsstr strstr
#define _stprintf sprintf
#define _sntprintf snprintf
#define _tcstol strtol
#define _ttoi atoi
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define wsprintf sprintf
#define _tcscpy_s(d, n, s) (strncpy((d), (s), (n) - 1), (d)[(n) - 1] = '\0', 0)
#define strcpy_s(d, n, s) (strncpy((d), (s), (n) - 1), (d)[(n) - 1] = '\0', 0)
#define strcat_s(d, n, s) (strncat((d), (s), (n) - strlen(d) - 1), 0)
#define sprintf_s snprintf
#define _countof(a) (sizeof(a) / sizeof((a)[0]))

#endif /* EDITA_NPP_COMPAT_H */
