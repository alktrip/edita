# This file is part of Edita.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# El commit y su fecha, para Ayuda › Acerca de (fase 91). Se ejecuta en cada compilación
# (cmake -DSOURCE_DIR=… -DOUTPUT=… -P BuildInfo.cmake) y solo reescribe el encabezado si
# cambió, para no recompilar de más. Sin git (un .tar.gz), los dos quedan vacíos.
set(commit "")
set(date "")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" log -1 --format=%h%n%cs
                    OUTPUT_VARIABLE out RESULT_VARIABLE result ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(result EQUAL 0)
        string(REPLACE "\n" ";" parts "${out}")
        list(GET parts 0 commit)
        list(GET parts 1 date)
    endif()
endif()
set(content "// Generado en cada compilación por cmake/BuildInfo.cmake. No editar.
#pragma once
#define EDITA_BUILD_COMMIT \"${commit}\"
#define EDITA_BUILD_DATE \"${date}\"
")
if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" old)
endif()
if(NOT old STREQUAL content)
    file(WRITE "${OUTPUT}" "${content}")
endif()
