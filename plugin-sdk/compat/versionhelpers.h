/* versionhelpers.h para compilar plugins de Notepad++ con el SDK de Edita: ver npp_compat.h
 * Para el plugin, el sistema es un Windows moderno */
#pragma once
#include "npp_compat.h"
static inline BOOL IsWindowsXPOrGreater(void) { return TRUE; }
static inline BOOL IsWindowsVistaOrGreater(void) { return TRUE; }
static inline BOOL IsWindows7OrGreater(void) { return TRUE; }
static inline BOOL IsWindows8OrGreater(void) { return TRUE; }
static inline BOOL IsWindows8Point1OrGreater(void) { return TRUE; }
static inline BOOL IsWindows10OrGreater(void) { return TRUE; }
static inline BOOL IsWindowsServer(void) { return FALSE; }
