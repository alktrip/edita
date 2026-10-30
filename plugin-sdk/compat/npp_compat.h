/* This file is part of Edita.
 * Copyright (C) 2026 Edita contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Compatibilidad para compilar en Linux y macOS el código fuente de plugins de Notepad++.
 *
 * El código original incluye <windows.h> (que en esta carpeta solo incluye este
 * archivo) y sus propias PluginInterface.h y Notepad_plus_msgs.h. Aquí están los
 * tipos y las funciones de Win32 que usan los plugins:
 *   - ::SendMessage(hwnd, …) llega a Edita (data.send) con el mismo hwnd, que es
 *     data.host, data.mainEditor o data.subEditor;
 *   - una parte de KERNEL32, SHLWAPI, SHELL32 y USER32 que no necesita ventanas
 *     (archivos, archivos .ini, conversiones de texto, rutas, portapapeles,
 *     MessageBox, ShellExecute, strsafe.h): npp_win32.cpp;
 *   - los diálogos de recursos (.rc) no existen: CreateDialogParam/DialogBoxParam
 *     muestran un aviso con el nombre del plugin.
 * Las funciones que exporta un plugin de N++ (setInfo, getFuncsArray…) se
 * renombran a npp_plugin_*: npp_compat.cpp exporta las de Edita y las llama.
 *
 * Dos modos, como en Windows:
 *   - sin UNICODE, TCHAR es char y las cadenas son UTF-8, como en la API de Edita;
 *   - con UNICODE (edita_npp_plugin(… UNICODE)), TCHAR es wchar_t, como en los plugins
 *     de N++ desde la 8.0: npp_compat.cpp convierte los nombres, las órdenes y los
 *     mensajes NPPM_* con cadenas. En Linux y macOS wchar_t mide 4 bytes (UTF-32), no
 *     2: el código que solo usa las funciones de Win32 funciona igual.
 * Las rutas que reciben las funciones de archivos pueden usar '\' (se convierte a '/').
 */

#ifndef EDITA_NPP_COMPAT_H
#define EDITA_NPP_COMPAT_H

#if defined(UNICODE) || defined(_UNICODE)
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#endif

#ifdef __cplusplus
/* Antes de las macros de abajo: <cwchar> quita las macros de swprintf y compañía */
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#endif
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#ifdef __cplusplus
#define EDITA_COMPAT_EXTERN_C extern "C"
#define EDITA_COMPAT_BEGIN extern "C" {
#define EDITA_COMPAT_END }
#else
#define EDITA_COMPAT_EXTERN_C
#define EDITA_COMPAT_BEGIN
#define EDITA_COMPAT_END
#endif

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
#define EDITA_COMPAT_DECLSPEC_nothrow
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __fastcall
#define __fastcall
#endif
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif
#define WINAPI
#define WINAPIV
#define APIENTRY
#define CALLBACK
#define PASCAL
#define FAR
#define NEAR
#define CONST const
#define VOID void
#define STDMETHODCALLTYPE
#ifndef _In_
#define _In_
#define _In_opt_
#define _In_z_
#define _In_reads_(n)
#define _Out_
#define _Out_opt_
#define _Out_writes_(n)
#define _Out_writes_z_(n)
#define _Inout_
#define _Inout_opt_
#define _Printf_format_string_
#define _Success_(x)
#define _Ret_maybenull_
#define _Check_return_
#endif
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(p) ((void)(p))
#endif
#ifndef _countof
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#endif
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) _countof(a)
#endif

/* --- Tipos -------------------------------------------------------------------- */
typedef int BOOL;
typedef unsigned char BOOLEAN;
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
typedef int32_t INT32;
typedef uint32_t UINT32;
typedef int64_t INT64;
typedef uint64_t UINT64;
typedef int64_t LONG64;
typedef uint64_t ULONG64;
typedef uint64_t DWORD64;
typedef uintptr_t UINT_PTR;
typedef intptr_t INT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef size_t SIZE_T;
typedef intptr_t SSIZE_T;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef void* PVOID;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef BYTE* LPBYTE;
typedef BYTE* PBYTE;
typedef WORD* LPWORD;
typedef LONG* LPLONG;
typedef LONG* PLONG;
typedef BOOL* LPBOOL;
typedef BOOL* PBOOL;
typedef DWORD* LPDWORD;
typedef DWORD* PDWORD;
typedef UINT* PUINT;
typedef void* HANDLE;
typedef HANDLE* PHANDLE;
typedef HANDLE* LPHANDLE;
typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HMODULE;
typedef void* HMENU;
typedef void* HICON;
typedef void* HBITMAP;
typedef void* HBRUSH;
typedef void* HFONT;
typedef void* HPEN;
typedef void* HRGN;
typedef void* HDC;
typedef void* HGDIOBJ;
typedef void* HGLOBAL;
typedef void* HLOCAL;
typedef void* HCURSOR;
typedef void* HACCEL;
typedef void* HKEY;
typedef void* HRSRC;
typedef void* HIMAGELIST;
typedef void* HTREEITEM;
typedef WORD ATOM;
typedef WORD LANGID;
typedef DWORD LCID;
typedef DWORD COLORREF;
typedef long HRESULT;
typedef char CHAR;
typedef wchar_t WCHAR;
typedef char* LPSTR;
typedef char* PSTR;
typedef char* PCHAR;
typedef const char* LPCSTR;
typedef const char* PCSTR;
typedef wchar_t* LPWSTR;
typedef wchar_t* PWSTR;
typedef wchar_t* PWCHAR;
typedef const wchar_t* LPCWSTR;
typedef const wchar_t* PCWSTR;
#ifdef UNICODE
typedef wchar_t TCHAR;
typedef wchar_t _TCHAR;
typedef wchar_t TBYTE;
#else
typedef char TCHAR;
typedef char _TCHAR;
typedef unsigned char TBYTE;
#endif
typedef TCHAR* LPTSTR;
typedef TCHAR* PTSTR;
typedef const TCHAR* LPCTSTR;
typedef const TCHAR* PCTSTR;
typedef INT_PTR (*DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR (*FARPROC)(void);
#ifndef __int64
#define __int64 long long
#endif
#ifndef _ERRNO_T_DEFINED
#define _ERRNO_T_DEFINED
typedef int errno_t;
#endif

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#ifdef UNICODE
#define __TEXT(s) L##s
#else
#define __TEXT(s) s
#endif
#define TEXT(s) __TEXT(s)
#define _T(s) __TEXT(s)
#define _TEXT(s) __TEXT(s)
#define __T(s) __TEXT(s)

#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define MAKEWPARAM(l, h) ((WPARAM)(DWORD)MAKELONG(l, h))
#define MAKELPARAM(l, h) ((LPARAM)(DWORD)MAKELONG(l, h))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb) ((BYTE)(rgb))
#define GetGValue(rgb) ((BYTE)(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) ((BYTE)((rgb) >> 16))
#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCEW(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))
#ifdef UNICODE
#define MAKEINTRESOURCE MAKEINTRESOURCEW
#else
#define MAKEINTRESOURCE MAKEINTRESOURCEA
#endif

typedef struct tagRECT { LONG left; LONG top; LONG right; LONG bottom; } RECT, *LPRECT, *PRECT;
typedef const RECT* LPCRECT;
typedef struct tagPOINT { LONG x; LONG y; } POINT, *LPPOINT, *PPOINT;
typedef struct tagSIZE { LONG cx; LONG cy; } SIZE, *LPSIZE, *PSIZE;
typedef struct tagNMHDR { HWND hwndFrom; UINT_PTR idFrom; UINT code; } NMHDR, *LPNMHDR;
typedef struct _FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; } FILETIME, *LPFILETIME, *PFILETIME;
typedef struct _SYSTEMTIME {
    WORD wYear; WORD wMonth; WORD wDayOfWeek; WORD wDay;
    WORD wHour; WORD wMinute; WORD wSecond; WORD wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME, *PSYSTEMTIME;
typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;
typedef union _ULARGE_INTEGER {
    struct { DWORD LowPart; DWORD HighPart; } u;
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;
typedef struct _SECURITY_ATTRIBUTES { DWORD nLength; LPVOID lpSecurityDescriptor; BOOL bInheritHandle; } SECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;
typedef struct _OVERLAPPED { ULONG_PTR Internal; ULONG_PTR InternalHigh; DWORD Offset; DWORD OffsetHigh; HANDLE hEvent; } OVERLAPPED, *LPOVERLAPPED;
typedef struct _WIN32_FIND_DATAA {
    DWORD dwFileAttributes; FILETIME ftCreationTime; FILETIME ftLastAccessTime; FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh; DWORD nFileSizeLow; DWORD dwReserved0; DWORD dwReserved1;
    CHAR cFileName[MAX_PATH]; CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA, *LPWIN32_FIND_DATAA, *PWIN32_FIND_DATAA;
typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes; FILETIME ftCreationTime; FILETIME ftLastAccessTime; FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh; DWORD nFileSizeLow; DWORD dwReserved0; DWORD dwReserved1;
    WCHAR cFileName[MAX_PATH]; WCHAR cAlternateFileName[14];
} WIN32_FIND_DATAW, *LPWIN32_FIND_DATAW, *PWIN32_FIND_DATAW;
#ifdef UNICODE
typedef WIN32_FIND_DATAW WIN32_FIND_DATA;
typedef LPWIN32_FIND_DATAW LPWIN32_FIND_DATA;
typedef PWIN32_FIND_DATAW PWIN32_FIND_DATA;
#else
typedef WIN32_FIND_DATAA WIN32_FIND_DATA;
typedef LPWIN32_FIND_DATAA LPWIN32_FIND_DATA;
typedef PWIN32_FIND_DATAA PWIN32_FIND_DATA;
#endif

/* --- Mensajes y constantes ------------------------------------------------------ */
#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_ENABLE 0x000A
#define WM_SETREDRAW 0x000B
#define WM_SETTEXT 0x000C
#define WM_GETTEXT 0x000D
#define WM_GETTEXTLENGTH 0x000E
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_SHOWWINDOW 0x0018
#define WM_SETFONT 0x0030
#define WM_GETFONT 0x0031
#define WM_NOTIFY 0x004E
#define WM_CONTEXTMENU 0x007B
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_INITDIALOG 0x0110
#define WM_COMMAND 0x0111
#define WM_SYSCOMMAND 0x0112
#define WM_TIMER 0x0113
#define WM_HSCROLL 0x0114
#define WM_VSCROLL 0x0115
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_MOUSEWHEEL 0x020A
#define WM_DROPFILES 0x0233
#define WM_USER 0x0400
#define WM_APP 0x8000

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
#define MB_DEFBUTTON1 0x00000000L
#define MB_DEFBUTTON2 0x00000100L
#define MB_DEFBUTTON3 0x00000200L
#define MB_APPLMODAL 0x00000000L
#define MB_SYSTEMMODAL 0x00001000L
#define MB_TASKMODAL 0x00002000L
#define MB_SETFOREGROUND 0x00010000L
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
#define SWP_NOREDRAW 0x0008
#define SWP_NOACTIVATE 0x0010
#define SWP_FRAMECHANGED 0x0020
#define SWP_SHOWWINDOW 0x0040
#define SWP_HIDEWINDOW 0x0080
#define HWND_TOP ((HWND)0)
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_NORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_MAXIMIZE 3
#define SW_SHOWNOACTIVATE 4
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_SHOWNA 8
#define SW_RESTORE 9
#define SW_SHOWDEFAULT 10

#define BST_UNCHECKED 0x0000
#define BST_CHECKED 0x0001
#define BST_INDETERMINATE 0x0002

#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_LWIN 0x5B
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define VK_OEM_1 0xBA
#define VK_OEM_PLUS 0xBB
#define VK_OEM_COMMA 0xBC
#define VK_OEM_MINUS 0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2 0xBF
#define VK_OEM_3 0xC0
#define VK_OEM_4 0xDB
#define VK_OEM_5 0xDC
#define VK_OEM_6 0xDD
#define VK_OEM_7 0xDE

/* Códigos de error (GetLastError) y HRESULT */
#define ERROR_SUCCESS 0L
#define NO_ERROR 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_PATH_NOT_FOUND 3L
#define ERROR_ACCESS_DENIED 5L
#define ERROR_INVALID_HANDLE 6L
#define ERROR_NOT_ENOUGH_MEMORY 8L
#define ERROR_NO_MORE_FILES 18L
#define ERROR_FILE_EXISTS 80L
#define ERROR_INVALID_PARAMETER 87L
#define ERROR_INSUFFICIENT_BUFFER 122L
#define ERROR_ALREADY_EXISTS 183L
#define ERROR_MORE_DATA 234L
#define ERROR_NO_UNICODE_TRANSLATION 1113L
#define S_OK ((HRESULT)0L)
#define S_FALSE ((HRESULT)1L)
#define E_FAIL ((HRESULT)0x80004005L)
#define E_INVALIDARG ((HRESULT)0x80070057L)
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#define E_NOTIMPL ((HRESULT)0x80004001L)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

/* Archivos */
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define INVALID_FILE_SIZE ((DWORD)0xFFFFFFFF)
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define GENERIC_READ 0x80000000L
#define GENERIC_WRITE 0x40000000L
#define GENERIC_EXECUTE 0x20000000L
#define GENERIC_ALL 0x10000000L
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define FILE_SHARE_DELETE 0x00000004
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5
#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_HIDDEN 0x00000002
#define FILE_ATTRIBUTE_SYSTEM 0x00000004
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_ARCHIVE 0x00000020
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_ATTRIBUTE_TEMPORARY 0x00000100
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define MOVEFILE_REPLACE_EXISTING 0x00000001
#define MOVEFILE_COPY_ALLOWED 0x00000002

/* Memoria global y local (portapapeles) */
#define GMEM_FIXED 0x0000
#define GMEM_MOVEABLE 0x0002
#define GMEM_ZEROINIT 0x0040
#define GMEM_DDESHARE 0x2000
#define GHND (GMEM_MOVEABLE | GMEM_ZEROINIT)
#define GPTR (GMEM_FIXED | GMEM_ZEROINIT)
#define LMEM_FIXED 0x0000
#define LMEM_MOVEABLE 0x0002
#define LMEM_ZEROINIT 0x0040
#define LPTR (LMEM_FIXED | LMEM_ZEROINIT)
#define HEAP_ZERO_MEMORY 0x00000008

/* Portapapeles */
#define CF_TEXT 1
#define CF_OEMTEXT 7
#define CF_UNICODETEXT 13
#define CF_HDROP 15
#define CF_LOCALE 16

/* Conversiones de texto */
#define CP_ACP 0
#define CP_OEMCP 1
#define CP_MACCP 2
#define CP_THREAD_ACP 3
#define CP_UTF7 65000
#define CP_UTF8 65001
#define MB_PRECOMPOSED 0x00000001
#define MB_ERR_INVALID_CHARS 0x00000008
#define WC_COMPOSITECHECK 0x00000200
#define WC_NO_BEST_FIT_CHARS 0x00000400
#define WC_ERR_INVALID_CHARS 0x00000080

/* Carpetas especiales (SHGetFolderPath) */
#define CSIDL_DESKTOP 0x0000
#define CSIDL_PERSONAL 0x0005
#define CSIDL_MYDOCUMENTS CSIDL_PERSONAL
#define CSIDL_APPDATA 0x001A
#define CSIDL_LOCAL_APPDATA 0x001C
#define CSIDL_COMMON_APPDATA 0x0023
#define CSIDL_PROFILE 0x0028
#define CSIDL_PROGRAM_FILES 0x0026
#define CSIDL_FLAG_CREATE 0x8000
#define SHGFP_TYPE_CURRENT 0

/* strsafe.h */
#define STRSAFE_E_INSUFFICIENT_BUFFER ((HRESULT)0x8007007AL)
#define STRSAFE_E_INVALID_PARAMETER ((HRESULT)0x80070057L)
#define STRSAFE_MAX_CCH 2147483647

/* Memoria (kernel32: macros de winnt.h) */
#define ZeroMemory(d, n) memset((d), 0, (n))
#define SecureZeroMemory(d, n) memset((d), 0, (n))
#define FillMemory(d, n, v) memset((d), (v), (n))
#define CopyMemory(d, s, n) memcpy((d), (s), (n))
#define MoveMemory(d, s, n) memmove((d), (s), (n))
#define RtlZeroMemory ZeroMemory
#define RtlCopyMemory CopyMemory
#define RtlMoveMemory MoveMemory

/* --- Funciones de Win32 (npp_win32.cpp) ----------------------------------------- */
EDITA_COMPAT_BEGIN

/* El módulo del plugin (el HINSTANCE que recibe DllMain) */
HINSTANCE edita_compat_module(void);

/* Errores */
DWORD GetLastError(void);
void SetLastError(DWORD error);

/* Mensajes al editor (npp_compat.cpp) */
LRESULT edita_compat_SendMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

/* Cuadros de mensaje */
int MessageBoxA(HWND parent, LPCSTR text, LPCSTR caption, UINT type);
int MessageBoxW(HWND parent, LPCWSTR text, LPCWSTR caption, UINT type);
BOOL MessageBeep(UINT type);

/* Diálogos de recursos: no existen (npp_compat.cpp muestra un aviso) */
HWND edita_compat_CreateDialogParam(HINSTANCE instance, const void* templateName, HWND parent, void* proc, LPARAM init);
INT_PTR edita_compat_DialogBoxParam(HINSTANCE instance, const void* templateName, HWND parent, void* proc, LPARAM init);

/* Conversiones de texto: CP_UTF8, CP_ACP, CP_OEMCP y CP_THREAD_ACP son UTF-8;
 * 1252 y 28591 son Latin-1 */
int MultiByteToWideChar(UINT codePage, DWORD flags, LPCSTR multiByte, int multiByteCount, LPWSTR wide, int wideCount);
int WideCharToMultiByte(UINT codePage, DWORD flags, LPCWSTR wide, int wideCount, LPSTR multiByte, int multiByteCount,
                        LPCSTR defaultChar, LPBOOL usedDefaultChar);

/* Cadenas (kernel32) */
int lstrlenA(LPCSTR s);
int lstrlenW(LPCWSTR s);
LPSTR lstrcpyA(LPSTR dst, LPCSTR src);
LPWSTR lstrcpyW(LPWSTR dst, LPCWSTR src);
LPSTR lstrcpynA(LPSTR dst, LPCSTR src, int count);
LPWSTR lstrcpynW(LPWSTR dst, LPCWSTR src, int count);
LPSTR lstrcatA(LPSTR dst, LPCSTR src);
LPWSTR lstrcatW(LPWSTR dst, LPCWSTR src);
int lstrcmpA(LPCSTR a, LPCSTR b);
int lstrcmpW(LPCWSTR a, LPCWSTR b);
int lstrcmpiA(LPCSTR a, LPCSTR b);
int lstrcmpiW(LPCWSTR a, LPCWSTR b);
LPSTR CharUpperA(LPSTR s);
LPWSTR CharUpperW(LPWSTR s);
LPSTR CharLowerA(LPSTR s);
LPWSTR CharLowerW(LPWSTR s);
DWORD CharUpperBuffW(LPWSTR s, DWORD length);
DWORD CharLowerBuffW(LPWSTR s, DWORD length);
BOOL IsCharAlphaW(WCHAR c);
BOOL IsCharAlphaNumericW(WCHAR c);
BOOL IsCharUpperW(WCHAR c);
BOOL IsCharLowerW(WCHAR c);

/* printf de MSVC: en las funciones anchas, %s y %c son wchar_t (en C son char) y
 * %S y %C, char; también %I64d. Se traduce el formato antes de llamar a vswprintf */
int edita_compat_vswprintf(wchar_t* buffer, size_t count, const wchar_t* format, va_list args);
int edita_compat_swprintf(wchar_t* buffer, size_t count, const wchar_t* format, ...);
int edita_compat_vsnprintf(char* buffer, size_t count, const char* format, va_list args);
int edita_compat_snprintf(char* buffer, size_t count, const char* format, ...);
int wsprintfA(LPSTR buffer, LPCSTR format, ...);
int wsprintfW(LPWSTR buffer, LPCWSTR format, ...);
int wvsprintfA(LPSTR buffer, LPCSTR format, va_list args);
int wvsprintfW(LPWSTR buffer, LPCWSTR format, va_list args);
int _swprintf(wchar_t* buffer, const wchar_t* format, ...);
int _snwprintf(wchar_t* buffer, size_t count, const wchar_t* format, ...);
int _snwprintf_s(wchar_t* buffer, size_t size, size_t count, const wchar_t* format, ...);
int _vsnwprintf(wchar_t* buffer, size_t count, const wchar_t* format, va_list args);
int swprintf_s(wchar_t* buffer, size_t size, const wchar_t* format, ...);
int vswprintf_s(wchar_t* buffer, size_t size, const wchar_t* format, va_list args);
int _snprintf(char* buffer, size_t count, const char* format, ...);
int _snprintf_s(char* buffer, size_t size, size_t count, const char* format, ...);
int sprintf_s(char* buffer, size_t size, const char* format, ...);
int vsprintf_s(char* buffer, size_t size, const char* format, va_list args);
int _vsnprintf(char* buffer, size_t count, const char* format, va_list args);
int _scwprintf(const wchar_t* format, ...);
int _scprintf(const char* format, ...);

/* CRT de MSVC */
errno_t strcpy_s(char* dst, size_t size, const char* src);
errno_t strncpy_s(char* dst, size_t size, const char* src, size_t count);
errno_t strcat_s(char* dst, size_t size, const char* src);
errno_t strncat_s(char* dst, size_t size, const char* src, size_t count);
errno_t wcscpy_s(wchar_t* dst, size_t size, const wchar_t* src);
errno_t wcsncpy_s(wchar_t* dst, size_t size, const wchar_t* src, size_t count);
errno_t wcscat_s(wchar_t* dst, size_t size, const wchar_t* src);
errno_t wcsncat_s(wchar_t* dst, size_t size, const wchar_t* src, size_t count);
errno_t memcpy_s(void* dst, size_t size, const void* src, size_t count);
errno_t memmove_s(void* dst, size_t size, const void* src, size_t count);
char* _strlwr(char* s);
char* _strupr(char* s);
wchar_t* _wcslwr(wchar_t* s);
wchar_t* _wcsupr(wchar_t* s);
errno_t _strlwr_s(char* s, size_t size);
errno_t _strupr_s(char* s, size_t size);
errno_t _wcslwr_s(wchar_t* s, size_t size);
errno_t _wcsupr_s(wchar_t* s, size_t size);
wchar_t* _wcsrev(wchar_t* s);
char* _strrev(char* s);
char* _itoa(int value, char* buffer, int radix);
wchar_t* _itow(int value, wchar_t* buffer, int radix);
char* _ltoa(long value, char* buffer, int radix);
wchar_t* _ltow(long value, wchar_t* buffer, int radix);
char* _ultoa(unsigned long value, char* buffer, int radix);
wchar_t* _ultow(unsigned long value, wchar_t* buffer, int radix);
char* _i64toa(long long value, char* buffer, int radix);
wchar_t* _i64tow(long long value, wchar_t* buffer, int radix);
errno_t _itoa_s(int value, char* buffer, size_t size, int radix);
errno_t _itow_s(int value, wchar_t* buffer, size_t size, int radix);
int _wtoi(const wchar_t* s);
long _wtol(const wchar_t* s);
long long _wtoi64(const wchar_t* s);
double _wtof(const wchar_t* s);
FILE* _wfopen(const wchar_t* path, const wchar_t* mode);
errno_t _wfopen_s(FILE** file, const wchar_t* path, const wchar_t* mode);
errno_t fopen_s(FILE** file, const char* path, const char* mode);
int _wremove(const wchar_t* path);
int _wrename(const wchar_t* from, const wchar_t* to);
int _waccess(const wchar_t* path, int mode);
int _wmkdir(const wchar_t* path);
int _wrmdir(const wchar_t* path);
wchar_t* _wgetenv(const wchar_t* name);

/* Archivos */
HANDLE CreateFileA(LPCSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE templateFile);
HANDLE CreateFileW(LPCWSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE templateFile);
BOOL ReadFile(HANDLE file, LPVOID buffer, DWORD toRead, LPDWORD read, LPOVERLAPPED overlapped);
BOOL WriteFile(HANDLE file, LPCVOID buffer, DWORD toWrite, LPDWORD written, LPOVERLAPPED overlapped);
DWORD GetFileSize(HANDLE file, LPDWORD high);
BOOL GetFileSizeEx(HANDLE file, PLARGE_INTEGER size);
DWORD SetFilePointer(HANDLE file, LONG distance, PLONG high, DWORD method);
BOOL SetFilePointerEx(HANDLE file, LARGE_INTEGER distance, PLARGE_INTEGER newPointer, DWORD method);
BOOL SetEndOfFile(HANDLE file);
BOOL FlushFileBuffers(HANDLE file);
BOOL CloseHandle(HANDLE handle);
DWORD GetFileAttributesA(LPCSTR path);
DWORD GetFileAttributesW(LPCWSTR path);
BOOL SetFileAttributesA(LPCSTR path, DWORD attributes);
BOOL SetFileAttributesW(LPCWSTR path, DWORD attributes);
BOOL DeleteFileA(LPCSTR path);
BOOL DeleteFileW(LPCWSTR path);
BOOL CreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES security);
BOOL CreateDirectoryW(LPCWSTR path, LPSECURITY_ATTRIBUTES security);
BOOL RemoveDirectoryA(LPCSTR path);
BOOL RemoveDirectoryW(LPCWSTR path);
BOOL MoveFileA(LPCSTR from, LPCSTR to);
BOOL MoveFileW(LPCWSTR from, LPCWSTR to);
BOOL MoveFileExA(LPCSTR from, LPCSTR to, DWORD flags);
BOOL MoveFileExW(LPCWSTR from, LPCWSTR to, DWORD flags);
BOOL CopyFileA(LPCSTR from, LPCSTR to, BOOL failIfExists);
BOOL CopyFileW(LPCWSTR from, LPCWSTR to, BOOL failIfExists);
HANDLE FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA data);
HANDLE FindFirstFileW(LPCWSTR pattern, LPWIN32_FIND_DATAW data);
BOOL FindNextFileA(HANDLE find, LPWIN32_FIND_DATAA data);
BOOL FindNextFileW(HANDLE find, LPWIN32_FIND_DATAW data);
BOOL FindClose(HANDLE find);
DWORD GetCurrentDirectoryA(DWORD size, LPSTR buffer);
DWORD GetCurrentDirectoryW(DWORD size, LPWSTR buffer);
BOOL SetCurrentDirectoryA(LPCSTR path);
BOOL SetCurrentDirectoryW(LPCWSTR path);
DWORD GetTempPathA(DWORD size, LPSTR buffer);
DWORD GetTempPathW(DWORD size, LPWSTR buffer);
DWORD GetFullPathNameA(LPCSTR path, DWORD size, LPSTR buffer, LPSTR* filePart);
DWORD GetFullPathNameW(LPCWSTR path, DWORD size, LPWSTR buffer, LPWSTR* filePart);

/* Módulos: GetModuleFileName(NULL) es el ejecutable de Edita; con el módulo del
 * plugin, su .so */
DWORD GetModuleFileNameA(HMODULE module, LPSTR buffer, DWORD size);
DWORD GetModuleFileNameW(HMODULE module, LPWSTR buffer, DWORD size);
HMODULE GetModuleHandleA(LPCSTR name);
HMODULE GetModuleHandleW(LPCWSTR name);

/* Archivos .ini: UTF-8 (también leen UTF-16 con BOM, como los que deja N++) */
DWORD GetPrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR fallback, LPSTR buffer, DWORD size, LPCSTR file);
DWORD GetPrivateProfileStringW(LPCWSTR section, LPCWSTR key, LPCWSTR fallback, LPWSTR buffer, DWORD size, LPCWSTR file);
UINT GetPrivateProfileIntA(LPCSTR section, LPCSTR key, INT fallback, LPCSTR file);
UINT GetPrivateProfileIntW(LPCWSTR section, LPCWSTR key, INT fallback, LPCWSTR file);
BOOL WritePrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR value, LPCSTR file);
BOOL WritePrivateProfileStringW(LPCWSTR section, LPCWSTR key, LPCWSTR value, LPCWSTR file);
DWORD GetPrivateProfileSectionA(LPCSTR section, LPSTR buffer, DWORD size, LPCSTR file);
DWORD GetPrivateProfileSectionW(LPCWSTR section, LPWSTR buffer, DWORD size, LPCWSTR file);
DWORD GetPrivateProfileSectionNamesA(LPSTR buffer, DWORD size, LPCSTR file);
DWORD GetPrivateProfileSectionNamesW(LPWSTR buffer, DWORD size, LPCWSTR file);

/* Entorno, tiempo y proceso */
DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buffer, DWORD size);
DWORD GetEnvironmentVariableW(LPCWSTR name, LPWSTR buffer, DWORD size);
BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value);
BOOL SetEnvironmentVariableW(LPCWSTR name, LPCWSTR value);
DWORD ExpandEnvironmentStringsA(LPCSTR source, LPSTR buffer, DWORD size);
DWORD ExpandEnvironmentStringsW(LPCWSTR source, LPWSTR buffer, DWORD size);
DWORD GetTickCount(void);
ULONGLONG GetTickCount64(void);
void Sleep(DWORD milliseconds);
void GetLocalTime(LPSYSTEMTIME time);
void GetSystemTime(LPSYSTEMTIME time);
void GetSystemTimeAsFileTime(LPFILETIME time);
BOOL QueryPerformanceCounter(LARGE_INTEGER* counter);
BOOL QueryPerformanceFrequency(LARGE_INTEGER* frequency);
DWORD GetCurrentProcessId(void);
DWORD GetCurrentThreadId(void);
void OutputDebugStringA(LPCSTR text);
void OutputDebugStringW(LPCWSTR text);

/* Memoria */
HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes);
LPVOID GlobalLock(HGLOBAL memory);
BOOL GlobalUnlock(HGLOBAL memory);
SIZE_T GlobalSize(HGLOBAL memory);
HGLOBAL GlobalFree(HGLOBAL memory);
HLOCAL LocalAlloc(UINT flags, SIZE_T bytes);
HLOCAL LocalFree(HLOCAL memory);
HANDLE GetProcessHeap(void);
LPVOID HeapAlloc(HANDLE heap, DWORD flags, SIZE_T bytes);
BOOL HeapFree(HANDLE heap, DWORD flags, LPVOID memory);

/* Portapapeles: el de Qt (CF_TEXT, CF_UNICODETEXT y los formatos registrados) */
BOOL OpenClipboard(HWND owner);
BOOL CloseClipboard(void);
BOOL EmptyClipboard(void);
HANDLE SetClipboardData(UINT format, HANDLE data);
HANDLE GetClipboardData(UINT format);
BOOL IsClipboardFormatAvailable(UINT format);
UINT RegisterClipboardFormatA(LPCSTR name);
UINT RegisterClipboardFormatW(LPCWSTR name);

/* Teclado: el estado de Mayús, Ctrl y Alt (bit alto = pulsada) */
SHORT GetKeyState(int virtualKey);
SHORT GetAsyncKeyState(int virtualKey);

/* shlwapi: aceptan '\' y '/'; los separadores que añaden son '/' */
BOOL PathFileExistsA(LPCSTR path);
BOOL PathFileExistsW(LPCWSTR path);
BOOL PathIsDirectoryA(LPCSTR path);
BOOL PathIsDirectoryW(LPCWSTR path);
BOOL PathIsRelativeA(LPCSTR path);
BOOL PathIsRelativeW(LPCWSTR path);
LPSTR PathFindFileNameA(LPCSTR path);
LPWSTR PathFindFileNameW(LPCWSTR path);
LPSTR PathFindExtensionA(LPCSTR path);
LPWSTR PathFindExtensionW(LPCWSTR path);
BOOL PathRemoveFileSpecA(LPSTR path);
BOOL PathRemoveFileSpecW(LPWSTR path);
void PathRemoveExtensionA(LPSTR path);
void PathRemoveExtensionW(LPWSTR path);
BOOL PathRenameExtensionA(LPSTR path, LPCSTR extension);
BOOL PathRenameExtensionW(LPWSTR path, LPCWSTR extension);
void PathStripPathA(LPSTR path);
void PathStripPathW(LPWSTR path);
BOOL PathAppendA(LPSTR path, LPCSTR more);
BOOL PathAppendW(LPWSTR path, LPCWSTR more);
LPSTR PathCombineA(LPSTR dst, LPCSTR dir, LPCSTR file);
LPWSTR PathCombineW(LPWSTR dst, LPCWSTR dir, LPCWSTR file);
LPSTR PathAddBackslashA(LPSTR path);
LPWSTR PathAddBackslashW(LPWSTR path);
LPSTR PathRemoveBackslashA(LPSTR path);
LPWSTR PathRemoveBackslashW(LPWSTR path);
int StrCmpIA(LPCSTR a, LPCSTR b);
int StrCmpIW(LPCWSTR a, LPCWSTR b);
int StrCmpNIA(LPCSTR a, LPCSTR b, int count);
int StrCmpNIW(LPCWSTR a, LPCWSTR b, int count);
LPSTR StrStrIA(LPCSTR haystack, LPCSTR needle);
LPWSTR StrStrIW(LPCWSTR haystack, LPCWSTR needle);
LPSTR StrChrA(LPCSTR s, WORD c);
LPWSTR StrChrW(LPCWSTR s, WCHAR c);
LPSTR StrRChrA(LPCSTR s, LPCSTR end, WORD c);
LPWSTR StrRChrW(LPCWSTR s, LPCWSTR end, WCHAR c);
int StrToIntA(LPCSTR s);
int StrToIntW(LPCWSTR s);

/* shell32: ShellExecute abre archivos y direcciones con la aplicación del sistema o
 * lanza un programa; SHGetFolderPath conoce APPDATA, LOCAL_APPDATA, PERSONAL y PROFILE */
HINSTANCE ShellExecuteA(HWND hwnd, LPCSTR verb, LPCSTR file, LPCSTR parameters, LPCSTR directory, INT show);
HINSTANCE ShellExecuteW(HWND hwnd, LPCWSTR verb, LPCWSTR file, LPCWSTR parameters, LPCWSTR directory, INT show);
HRESULT SHGetFolderPathA(HWND hwnd, int csidl, HANDLE token, DWORD flags, LPSTR path);
HRESULT SHGetFolderPathW(HWND hwnd, int csidl, HANDLE token, DWORD flags, LPWSTR path);
BOOL SHGetSpecialFolderPathA(HWND hwnd, LPSTR path, int csidl, BOOL create);
BOOL SHGetSpecialFolderPathW(HWND hwnd, LPWSTR path, int csidl, BOOL create);

/* strsafe.h */
HRESULT StringCchCopyA(LPSTR dst, size_t size, LPCSTR src);
HRESULT StringCchCopyW(LPWSTR dst, size_t size, LPCWSTR src);
HRESULT StringCchCopyNA(LPSTR dst, size_t size, LPCSTR src, size_t count);
HRESULT StringCchCopyNW(LPWSTR dst, size_t size, LPCWSTR src, size_t count);
HRESULT StringCchCatA(LPSTR dst, size_t size, LPCSTR src);
HRESULT StringCchCatW(LPWSTR dst, size_t size, LPCWSTR src);
HRESULT StringCchCatNA(LPSTR dst, size_t size, LPCSTR src, size_t count);
HRESULT StringCchCatNW(LPWSTR dst, size_t size, LPCWSTR src, size_t count);
HRESULT StringCchLengthA(LPCSTR s, size_t max, size_t* length);
HRESULT StringCchLengthW(LPCWSTR s, size_t max, size_t* length);
HRESULT StringCchPrintfA(LPSTR dst, size_t size, LPCSTR format, ...);
HRESULT StringCchPrintfW(LPWSTR dst, size_t size, LPCWSTR format, ...);
HRESULT StringCchVPrintfA(LPSTR dst, size_t size, LPCSTR format, va_list args);
HRESULT StringCchVPrintfW(LPWSTR dst, size_t size, LPCWSTR format, va_list args);
HRESULT StringCbCopyA(LPSTR dst, size_t bytes, LPCSTR src);
HRESULT StringCbCopyW(LPWSTR dst, size_t bytes, LPCWSTR src);
HRESULT StringCbCatA(LPSTR dst, size_t bytes, LPCSTR src);
HRESULT StringCbCatW(LPWSTR dst, size_t bytes, LPCWSTR src);
HRESULT StringCbPrintfA(LPSTR dst, size_t bytes, LPCSTR format, ...);
HRESULT StringCbPrintfW(LPWSTR dst, size_t bytes, LPCWSTR format, ...);

EDITA_COMPAT_END

/* --- SendMessage, MessageBox y diálogos ------------------------------------------ */
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
template<typename W, typename L>
inline BOOL PostMessage(HWND hwnd, UINT message, W wParam, L lParam)
{
    edita_compat_SendMessage(hwnd, message, (WPARAM)wParam, (LPARAM)lParam);
    return TRUE;
}
template<typename N, typename P>
inline HWND CreateDialogParam(HINSTANCE instance, N name, HWND parent, P proc, LPARAM init)
{
    return edita_compat_CreateDialogParam(instance, (const void*)name, parent, (void*)proc, init);
}
template<typename N, typename P>
inline INT_PTR DialogBoxParam(HINSTANCE instance, N name, HWND parent, P proc, LPARAM init)
{
    return edita_compat_DialogBoxParam(instance, (const void*)name, parent, (void*)proc, init);
}
template<typename N, typename P>
inline INT_PTR DialogBox(HINSTANCE instance, N name, HWND parent, P proc)
{
    return edita_compat_DialogBoxParam(instance, (const void*)name, parent, (void*)proc, 0);
}
#else
#define SendMessage(h, m, w, l) edita_compat_SendMessage((HWND)(h), (UINT)(m), (WPARAM)(w), (LPARAM)(l))
#define PostMessage(h, m, w, l) (edita_compat_SendMessage((HWND)(h), (UINT)(m), (WPARAM)(w), (LPARAM)(l)), TRUE)
#define CreateDialogParam(i, n, p, d, l) edita_compat_CreateDialogParam((i), (const void*)(n), (p), (void*)(d), (l))
#define DialogBoxParam(i, n, p, d, l) edita_compat_DialogBoxParam((i), (const void*)(n), (p), (void*)(d), (l))
#define DialogBox(i, n, p, d) edita_compat_DialogBoxParam((i), (const void*)(n), (p), (void*)(d), 0)
#endif
#define SendMessageA SendMessage
#define SendMessageW SendMessage
#define PostMessageA PostMessage
#define PostMessageW PostMessage
#define CreateDialogParamA CreateDialogParam
#define CreateDialogParamW CreateDialogParam
#define DialogBoxParamA DialogBoxParam
#define DialogBoxParamW DialogBoxParam

/* Ventanas: los diálogos de recursos no existen; estas no hacen nada */
static inline BOOL EndDialog(HWND w, INT_PTR r) { (void)w; (void)r; return TRUE; }
static inline BOOL GetClientRect(HWND w, LPRECT r) { (void)w; if (r) { r->left = r->top = 0; r->right = r->bottom = 0; } return TRUE; }
static inline BOOL GetWindowRect(HWND w, LPRECT r) { return GetClientRect(w, r); }
static inline BOOL ClientToScreen(HWND w, LPPOINT p) { (void)w; (void)p; return TRUE; }
static inline BOOL SetWindowPos(HWND w, HWND a, int x, int y, int cx, int cy, UINT f) { (void)w; (void)a; (void)x; (void)y; (void)cx; (void)cy; (void)f; return TRUE; }
static inline BOOL ShowWindow(HWND w, int c) { (void)w; (void)c; return TRUE; }
static inline BOOL DestroyWindow(HWND w) { (void)w; return TRUE; }
static inline BOOL IsWindowVisible(HWND w) { (void)w; return FALSE; }
static inline HWND GetDlgItem(HWND w, int id) { (void)w; (void)id; return 0; }
static inline BOOL SetDlgItemTextA(HWND w, int id, LPCSTR t) { (void)w; (void)id; (void)t; return TRUE; }
static inline BOOL SetDlgItemTextW(HWND w, int id, LPCWSTR t) { (void)w; (void)id; (void)t; return TRUE; }

/* --- Nombres genéricos: la variante A o W según UNICODE ----------------------------- */
#ifdef UNICODE
#define EDITA_COMPAT_AW(name) name##W
#else
#define EDITA_COMPAT_AW(name) name##A
#endif
#define MessageBox EDITA_COMPAT_AW(MessageBox)
#define SetDlgItemText EDITA_COMPAT_AW(SetDlgItemText)
#define lstrlen EDITA_COMPAT_AW(lstrlen)
#define lstrcpy EDITA_COMPAT_AW(lstrcpy)
#define lstrcpyn EDITA_COMPAT_AW(lstrcpyn)
#define lstrcat EDITA_COMPAT_AW(lstrcat)
#define lstrcmp EDITA_COMPAT_AW(lstrcmp)
#define lstrcmpi EDITA_COMPAT_AW(lstrcmpi)
#define CharUpper EDITA_COMPAT_AW(CharUpper)
#define CharLower EDITA_COMPAT_AW(CharLower)
#define wsprintf EDITA_COMPAT_AW(wsprintf)
#define wvsprintf EDITA_COMPAT_AW(wvsprintf)
#define CreateFile EDITA_COMPAT_AW(CreateFile)
#define GetFileAttributes EDITA_COMPAT_AW(GetFileAttributes)
#define SetFileAttributes EDITA_COMPAT_AW(SetFileAttributes)
#define DeleteFile EDITA_COMPAT_AW(DeleteFile)
#define CreateDirectory EDITA_COMPAT_AW(CreateDirectory)
#define RemoveDirectory EDITA_COMPAT_AW(RemoveDirectory)
#define MoveFile EDITA_COMPAT_AW(MoveFile)
#define MoveFileEx EDITA_COMPAT_AW(MoveFileEx)
#define CopyFile EDITA_COMPAT_AW(CopyFile)
#define FindFirstFile EDITA_COMPAT_AW(FindFirstFile)
#define FindNextFile EDITA_COMPAT_AW(FindNextFile)
#define GetCurrentDirectory EDITA_COMPAT_AW(GetCurrentDirectory)
#define SetCurrentDirectory EDITA_COMPAT_AW(SetCurrentDirectory)
#define GetTempPath EDITA_COMPAT_AW(GetTempPath)
#define GetFullPathName EDITA_COMPAT_AW(GetFullPathName)
#define GetModuleFileName EDITA_COMPAT_AW(GetModuleFileName)
#define GetModuleHandle EDITA_COMPAT_AW(GetModuleHandle)
#define GetPrivateProfileString EDITA_COMPAT_AW(GetPrivateProfileString)
#define GetPrivateProfileInt EDITA_COMPAT_AW(GetPrivateProfileInt)
#define WritePrivateProfileString EDITA_COMPAT_AW(WritePrivateProfileString)
#define GetPrivateProfileSection EDITA_COMPAT_AW(GetPrivateProfileSection)
#define GetPrivateProfileSectionNames EDITA_COMPAT_AW(GetPrivateProfileSectionNames)
#define GetEnvironmentVariable EDITA_COMPAT_AW(GetEnvironmentVariable)
#define SetEnvironmentVariable EDITA_COMPAT_AW(SetEnvironmentVariable)
#define ExpandEnvironmentStrings EDITA_COMPAT_AW(ExpandEnvironmentStrings)
#define OutputDebugString EDITA_COMPAT_AW(OutputDebugString)
#define RegisterClipboardFormat EDITA_COMPAT_AW(RegisterClipboardFormat)
#define PathFileExists EDITA_COMPAT_AW(PathFileExists)
#define PathIsDirectory EDITA_COMPAT_AW(PathIsDirectory)
#define PathIsRelative EDITA_COMPAT_AW(PathIsRelative)
#define PathFindFileName EDITA_COMPAT_AW(PathFindFileName)
#define PathFindExtension EDITA_COMPAT_AW(PathFindExtension)
#define PathRemoveFileSpec EDITA_COMPAT_AW(PathRemoveFileSpec)
#define PathRemoveExtension EDITA_COMPAT_AW(PathRemoveExtension)
#define PathRenameExtension EDITA_COMPAT_AW(PathRenameExtension)
#define PathStripPath EDITA_COMPAT_AW(PathStripPath)
#define PathAppend EDITA_COMPAT_AW(PathAppend)
#define PathCombine EDITA_COMPAT_AW(PathCombine)
#define PathAddBackslash EDITA_COMPAT_AW(PathAddBackslash)
#define PathRemoveBackslash EDITA_COMPAT_AW(PathRemoveBackslash)
#define StrCmpI EDITA_COMPAT_AW(StrCmpI)
#define StrCmpNI EDITA_COMPAT_AW(StrCmpNI)
#define StrStrI EDITA_COMPAT_AW(StrStrI)
#define StrChr EDITA_COMPAT_AW(StrChr)
#define StrRChr EDITA_COMPAT_AW(StrRChr)
#define StrToInt EDITA_COMPAT_AW(StrToInt)
#define ShellExecute EDITA_COMPAT_AW(ShellExecute)
#define SHGetFolderPath EDITA_COMPAT_AW(SHGetFolderPath)
#define SHGetSpecialFolderPath EDITA_COMPAT_AW(SHGetSpecialFolderPath)
#define StringCchCopy EDITA_COMPAT_AW(StringCchCopy)
#define StringCchCopyN EDITA_COMPAT_AW(StringCchCopyN)
#define StringCchCat EDITA_COMPAT_AW(StringCchCat)
#define StringCchCatN EDITA_COMPAT_AW(StringCchCatN)
#define StringCchLength EDITA_COMPAT_AW(StringCchLength)
#define StringCchPrintf EDITA_COMPAT_AW(StringCchPrintf)
#define StringCchVPrintf EDITA_COMPAT_AW(StringCchVPrintf)
#define StringCbCopy EDITA_COMPAT_AW(StringCbCopy)
#define StringCbCat EDITA_COMPAT_AW(StringCbCat)
#define StringCbPrintf EDITA_COMPAT_AW(StringCbPrintf)

/* --- CRT de MSVC -------------------------------------------------------------------- */
/* printf ancho con la semántica de MSVC (ver arriba) */
#define swprintf edita_compat_swprintf
#define vswprintf edita_compat_vswprintf
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _strcmpi strcasecmp
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _strdup strdup
#define _wcsicmp wcscasecmp
#define _wcsnicmp wcsncasecmp
#define _wcsdup wcsdup
#define _wcsicoll wcscasecmp
#define wcsicmp wcscasecmp
#define _access access
#define _getcwd getcwd
#define _fileno fileno
#define _unlink unlink
#define _strtoui64 strtoull
#define _strtoi64 strtoll
#define _wcstoui64 wcstoull
#define _wcstoi64 wcstoll
#define _atoi64 atoll
#define _MAX_PATH MAX_PATH
#define _MAX_FNAME 256
#define _MAX_EXT 256
#define _MAX_DIR MAX_PATH
#define _MAX_DRIVE 3
#define _TRUNCATE ((size_t)-1)

#ifdef UNICODE
#define _tcslen wcslen
#define _tcsnlen wcsnlen
#define _tcscpy wcscpy
#define _tcsncpy wcsncpy
#define _tcscpy_s wcscpy_s
#define _tcsncpy_s wcsncpy_s
#define _tcscat wcscat
#define _tcsncat wcsncat
#define _tcscat_s wcscat_s
#define _tcscmp wcscmp
#define _tcsncmp wcsncmp
#define _tcsicmp wcscasecmp
#define _tcsnicmp wcsncasecmp
#define _tcschr wcschr
#define _tcsrchr wcsrchr
#define _tcsstr wcsstr
#define _tcspbrk wcspbrk
#define _tcsspn wcsspn
#define _tcscspn wcscspn
#define _tcstok wcstok
#define _tcsdup wcsdup
#define _tcslwr _wcslwr
#define _tcsupr _wcsupr
#define _tcsrev _wcsrev
#define _tcstol wcstol
#define _tcstoul wcstoul
#define _tcstod wcstod
#define _tstoi _wtoi
#define _ttoi _wtoi
#define _ttol _wtol
#define _tstof _wtof
#define _itot _itow
#define _itot_s _itow_s
#define _ltot _ltow
#define _ultot _ultow
#define _stprintf _swprintf
#define _stprintf_s swprintf_s
#define _sntprintf _snwprintf
#define _sntprintf_s _snwprintf_s
#define _vstprintf_s vswprintf_s
#define _vsntprintf _vsnwprintf
#define _sctprintf _scwprintf
#define _tfopen _wfopen
#define _tfopen_s _wfopen_s
#define _tremove _wremove
#define _trename _wrename
#define _taccess _waccess
#define _tmkdir _wmkdir
#define _tgetenv _wgetenv
#define _istspace iswspace
#define _istdigit iswdigit
#define _istalpha iswalpha
#define _istalnum iswalnum
#define _istupper iswupper
#define _istlower iswlower
#define _istprint iswprint
#define _istxdigit iswxdigit
#define _totupper towupper
#define _totlower towlower
#else
#define _tcslen strlen
#define _tcsnlen strnlen
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscpy_s strcpy_s
#define _tcsncpy_s strncpy_s
#define _tcscat strcat
#define _tcsncat strncat
#define _tcscat_s strcat_s
#define _tcscmp strcmp
#define _tcsncmp strncmp
#define _tcsicmp strcasecmp
#define _tcsnicmp strncasecmp
#define _tcschr strchr
#define _tcsrchr strrchr
#define _tcsstr strstr
#define _tcspbrk strpbrk
#define _tcsspn strspn
#define _tcscspn strcspn
#define _tcstok strtok
#define _tcsdup strdup
#define _tcslwr _strlwr
#define _tcsupr _strupr
#define _tcsrev _strrev
#define _tcstol strtol
#define _tcstoul strtoul
#define _tcstod strtod
#define _tstoi atoi
#define _ttoi atoi
#define _ttol atol
#define _tstof atof
#define _itot _itoa
#define _itot_s _itoa_s
#define _ltot _ltoa
#define _ultot _ultoa
#define _stprintf sprintf
#define _stprintf_s sprintf_s
#define _sntprintf snprintf
#define _sntprintf_s _snprintf_s
#define _vstprintf_s vsprintf_s
#define _vsntprintf vsnprintf
#define _sctprintf _scprintf
#define _tfopen fopen
#define _tfopen_s fopen_s
#define _tremove remove
#define _trename rename
#define _taccess access
#define _tmkdir(p) mkdir((p), 0777)
#define _tgetenv getenv
#define _istspace isspace
#define _istdigit isdigit
#define _istalpha isalpha
#define _istalnum isalnum
#define _istupper isupper
#define _istlower islower
#define _istprint isprint
#define _istxdigit isxdigit
#define _totupper toupper
#define _totlower tolower
#endif

#ifdef __cplusplus
/* std::swprintf y std::vswprintf, con las macros de arriba */
namespace std {
using ::edita_compat_swprintf;
using ::edita_compat_vswprintf;
}

/* Las sobrecargas de MSVC para arreglos: wcscpy_s(buf, L"…"), swprintf_s(buf, L"%d", n)… */
template<size_t N> inline errno_t strcpy_s(char (&dst)[N], const char* src) { return strcpy_s(dst, N, src); }
template<size_t N> inline errno_t strcat_s(char (&dst)[N], const char* src) { return strcat_s(dst, N, src); }
template<size_t N> inline errno_t strncpy_s(char (&dst)[N], const char* src, size_t count) { return strncpy_s(dst, N, src, count); }
template<size_t N> inline errno_t wcscpy_s(wchar_t (&dst)[N], const wchar_t* src) { return wcscpy_s(dst, N, src); }
template<size_t N> inline errno_t wcscat_s(wchar_t (&dst)[N], const wchar_t* src) { return wcscat_s(dst, N, src); }
template<size_t N> inline errno_t wcsncpy_s(wchar_t (&dst)[N], const wchar_t* src, size_t count) { return wcsncpy_s(dst, N, src, count); }
template<size_t N> inline errno_t wcsncat_s(wchar_t (&dst)[N], const wchar_t* src, size_t count) { return wcsncat_s(dst, N, src, count); }
template<size_t N> inline errno_t _itoa_s(int value, char (&dst)[N], int radix) { return _itoa_s(value, dst, N, radix); }
template<size_t N> inline errno_t _itow_s(int value, wchar_t (&dst)[N], int radix) { return _itow_s(value, dst, N, radix); }
template<size_t N> inline errno_t _wcslwr_s(wchar_t (&s)[N]) { return _wcslwr_s(s, N); }
template<size_t N> inline errno_t _wcsupr_s(wchar_t (&s)[N]) { return _wcsupr_s(s, N); }
template<size_t N> inline errno_t _strlwr_s(char (&s)[N]) { return _strlwr_s(s, N); }
template<size_t N> inline errno_t _strupr_s(char (&s)[N]) { return _strupr_s(s, N); }
template<size_t N> inline int swprintf_s(wchar_t (&dst)[N], const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = vswprintf_s(dst, N, format, args);
    va_end(args);
    return written;
}
template<size_t N> inline int sprintf_s(char (&dst)[N], const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = vsprintf_s(dst, N, format, args);
    va_end(args);
    return written;
}
template<size_t N> inline int _snwprintf_s(wchar_t (&dst)[N], size_t count, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = _vsnwprintf(dst, count == _TRUNCATE || count >= N ? N : count + 1, format, args);
    va_end(args);
    return written;
}
template<size_t N> inline int vswprintf_s(wchar_t (&dst)[N], const wchar_t* format, va_list args)
{
    return vswprintf_s(dst, N, format, args);
}
#endif

#endif /* EDITA_NPP_COMPAT_H */
