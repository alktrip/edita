/* direct.h para compilar plugins de Notepad++ con el SDK de Edita: ver npp_compat.h */
#pragma once
#include "npp_compat.h"
#define _mkdir(p) mkdir((p), 0777)
#define _rmdir rmdir
#define _chdir chdir
