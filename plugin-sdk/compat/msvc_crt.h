/* This file is part of Edita.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Las funciones de cadenas del CRT de MSVC que no necesitan windows.h: con MSVC están en
 * <string.h> y <wchar.h>, así que el código las usa a veces sin incluir nada de Windows.
 * edita_npp_plugin incluye este archivo antes de cada fuente del plugin (-include), y
 * npp_compat.h también.
 */
#ifndef EDITA_MSVC_CRT_H
#define EDITA_MSVC_CRT_H

#include <string.h>
#include <strings.h>
#include <wchar.h>

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

#endif
