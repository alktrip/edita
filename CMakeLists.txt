cmake_minimum_required(VERSION 3.25)

project(Edita
    VERSION 0.0.1
    DESCRIPTION "Editor de texto nativo para Linux inspirado en Notepad++"
    HOMEPAGE_URL "https://github.com/notepad-plus-plus/notepad-plus-plus"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Tipo de build" FORCE)
endif()

option(EDITA_WERROR "Tratar los warnings del código propio como errores" ON)
option(EDITA_BUILD_TESTS "Compilar los tests" ON)
option(EDITA_BUILD_PLUGINS "Compilar el plugin de ejemplo" ON)

# Nombre del binario y del directorio de configuración (~/.config/<EDITA_APP_ID>/)
set(EDITA_APP_ID "edita" CACHE STRING "Identificador de la aplicación")

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Network Core5Compat Svg LinguistTools)

# libzip: instalar paquetes desde el administrador de plugins (opcional)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBZIP IMPORTED_TARGET GLOBAL libzip)
endif()

add_subdirectory(third_party)
add_subdirectory(src)

# Cabeceras para escribir plugins (edita_plugin.h + las de Scintilla que usa)
include(GNUInstallDirs)
install(FILES plugin-sdk/edita_plugin.h
              third_party/scintilla/include/Scintilla.h
              third_party/scintilla/include/Sci_Position.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${EDITA_APP_ID})
# SDK completo para compilar plugins desde su código (recetas del administrador de
# plugins): la capa de compatibilidad con Win32 y el módulo de CMake
set(EDITA_SDK_DATA ${CMAKE_INSTALL_DATADIR}/${EDITA_APP_ID}/plugin-sdk)
install(FILES plugin-sdk/edita_plugin.h
              third_party/scintilla/include/Scintilla.h
              third_party/scintilla/include/Sci_Position.h
        DESTINATION ${EDITA_SDK_DATA})
install(DIRECTORY plugin-sdk/compat plugin-sdk/cmake DESTINATION ${EDITA_SDK_DATA})
if(EDITA_BUILD_PLUGINS)
    add_subdirectory(plugins)
endif()

if(EDITA_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
