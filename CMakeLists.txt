cmake_minimum_required(VERSION 3.25)

project(Edita
    VERSION 1.0.0
    DESCRIPTION "Editor de texto nativo para Linux inspirado en Notepad++"
    HOMEPAGE_URL "https://github.com/alktrip/edita"
    LANGUAGES C CXX)
# La versión que ve el usuario: la numérica más la etiqueta de la etapa («-beta.1»; vacía en
# una versión estable). Los plugins y las comparaciones usan solo la numérica
set(EDITA_VERSION_SUFFIX "-rc.2")
set(EDITA_VERSION "${PROJECT_VERSION}${EDITA_VERSION_SUFFIX}" CACHE INTERNAL "Versión de Edita")
# Avisar de las versiones nuevas (Ayuda › Buscar actualizaciones): en el AppImage, sí; un
# paquete de una distribución lo desactiva, porque se actualiza con su gestor de paquetes
option(EDITA_UPDATE_CHECK "Avisar cuando hay una versión nueva de Edita" ON)
if(EDITA_UPDATE_CHECK)
    set(EDITA_UPDATE_CHECK_VALUE true)
else()
    set(EDITA_UPDATE_CHECK_VALUE false)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Solo las bibliotecas que se usan: Qt6::Gui arrastra libOpenGL.so.0 y libGLX.so.0 (glvnd)
# aunque Edita no llama a OpenGL, y hay Ubuntu de escritorio sin libopengl0 (26.10)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux") # el enlazador de Apple no lo conoce
    add_link_options(LINKER:--as-needed)
endif()
# …y que FindOpenGL (por Qt6::Gui) dé libGL.so.1, la que ya pide el libQt6Gui oficial, y no
# libOpenGL + libGLX: el enlazador las conserva aunque no se usen, porque resuelven símbolos
# de libQt6Gui que este no declara como suyas
set(OpenGL_GL_PREFERENCE LEGACY)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Tipo de build" FORCE)
endif()

option(EDITA_WERROR "Tratar los warnings del código propio como errores" ON)
option(EDITA_BUILD_TESTS "Compilar los tests" ON)
option(EDITA_SOURCE_DATA "Buscar los datos también en el árbol de fuentes (builds sin instalar); los paquetes lo desactivan" ON)
option(EDITA_BUILD_PLUGINS "Compilar el plugin de ejemplo" ON)

# Nombre del binario y del directorio de configuración (~/.config/<EDITA_APP_ID>/)
set(EDITA_APP_ID "edita" CACHE STRING "Identificador de la aplicación")

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Network Core5Compat Svg LinguistTools)

# libzip: instalar paquetes desde el administrador de plugins (opcional)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBZIP IMPORTED_TARGET GLOBAL libzip)
    # Corrector ortográfico (opcional): Hunspell, con los diccionarios del sistema
    pkg_check_modules(HUNSPELL IMPORTED_TARGET GLOBAL hunspell)
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
install(DIRECTORY plugin-sdk/compat plugin-sdk/cmake plugin-sdk/tools DESTINATION ${EDITA_SDK_DATA})
if(EDITA_BUILD_PLUGINS)
    add_subdirectory(plugins)
endif()

if(EDITA_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
