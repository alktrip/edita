/* crtdbg.h para compilar plugins de Notepad++ con el SDK de Edita: ver npp_compat.h */
#pragma once
#include "npp_compat.h"
#include <assert.h>
#ifndef _ASSERT
#define _ASSERT(x) assert(x)
#define _ASSERTE(x) assert(x)
#endif
