/* io.h para compilar plugins de Notepad++ con el SDK de Edita: ver npp_compat.h */
#pragma once
#include "npp_compat.h"
#include <fcntl.h>
#define _open open
#define _close close
#define _read read
#define _write write
#define _lseek lseek
#define _dup dup
#define _isatty isatty
#define _O_RDONLY O_RDONLY
#define _O_WRONLY O_WRONLY
#define _O_RDWR O_RDWR
#define _O_CREAT O_CREAT
#define _O_TRUNC O_TRUNC
#define _O_APPEND O_APPEND
#define _O_EXCL O_EXCL
#define _O_BINARY 0
#define _O_TEXT 0
