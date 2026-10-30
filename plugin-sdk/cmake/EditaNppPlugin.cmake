# This file is part of Edita.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compilar el código fuente de un plugin de Notepad++ para Edita.
#
#   cmake_minimum_required(VERSION 3.20)
#   project(MiPlugin CXX C)
#   include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)
#   edita_npp_plugin(MiPlugin [UNICODE] SOURCES ../src/plugin.cpp … [INCLUDES …] [LIBRARIES …])
#
# UNICODE compila como los plugins de N++ desde la 8.0: TCHAR es wchar_t, getName() y
# FuncItem._itemName son wchar_t y los mensajes NPPM_* llevan cadenas wchar_t (la capa
# las convierte). Sin UNICODE, TCHAR es char y las cadenas son UTF-8.
#
# Variables (las pone el administrador de plugins al compilar una receta):
#   EDITA_PLUGIN_SDK          carpeta del SDK (edita_plugin.h y compat/)
#   EDITA_SCINTILLA_INCLUDE   carpeta con Scintilla.h y Sci_Position.h (por defecto, el SDK)
#   EDITA_PLUGIN_VERSION      versión ("3.1")
#   EDITA_PLUGIN_DISPLAY_NAME, EDITA_PLUGIN_HOMEPAGE: para los diálogos que no existen
#   EDITA_QT_INCLUDE, EDITA_QT_LIBDIR: las cabeceras y bibliotecas del Qt con el que se
#       ejecuta Edita (en el AppImage, el Qt que trae). Sin ellas, find_package(Qt6):
#       el Qt del sistema
#
# El resultado es <carpeta>.so, con las exportaciones de Edita (compat/npp_compat.cpp).

if(NOT EDITA_PLUGIN_SDK)
    get_filename_component(EDITA_PLUGIN_SDK "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
if(NOT EDITA_SCINTILLA_INCLUDE)
    set(EDITA_SCINTILLA_INCLUDE "${EDITA_PLUGIN_SDK}")
endif()

# Qt6::Widgets o, con EDITA_QT_INCLUDE, el Qt de Edita. Un plugin compilado con un Qt
# más nuevo que el que lo carga pide símbolos que no existen (Qt_6.12…) y no carga
function(_edita_link_qt name)
    if(EDITA_QT_INCLUDE)
        if(NOT TARGET EditaQt::Widgets)
            add_library(EditaQt::Widgets INTERFACE IMPORTED)
            set(includes "${EDITA_QT_INCLUDE}")
            set(libraries "")
            foreach(module Core Gui Widgets)
                list(APPEND includes "${EDITA_QT_INCLUDE}/Qt${module}")
                list(APPEND libraries "${EDITA_QT_LIBDIR}/libQt6${module}.so.6")
            endforeach()
            set_target_properties(EditaQt::Widgets PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${includes}"
                INTERFACE_COMPILE_DEFINITIONS "QT_CORE_LIB;QT_GUI_LIB;QT_WIDGETS_LIB;QT_NO_DEBUG"
                INTERFACE_LINK_LIBRARIES "${libraries}"
                INTERFACE_LINK_OPTIONS "-Wl,-rpath-link,${EDITA_QT_LIBDIR}")
        endif()
        target_link_libraries(${name} PRIVATE EditaQt::Widgets)
    else()
        find_package(Qt6 REQUIRED COMPONENTS Widgets)
        target_link_libraries(${name} PRIVATE Qt6::Widgets)
    endif()
endfunction()

function(edita_npp_plugin name)
    cmake_parse_arguments(ARG "UNICODE" "" "SOURCES;INCLUDES;LIBRARIES;DEFINITIONS" ${ARGN})
    add_library(${name} MODULE ${ARG_SOURCES}
        "${EDITA_PLUGIN_SDK}/compat/npp_compat.cpp" "${EDITA_PLUGIN_SDK}/compat/npp_win32.cpp")
    if(ARG_UNICODE)
        target_compile_definitions(${name} PRIVATE UNICODE _UNICODE)
    endif()
    set_target_properties(${name} PROPERTIES
        PREFIX ""
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden)
    # compat/ primero: su windows.h sustituye al de Windows
    target_include_directories(${name} BEFORE PRIVATE
        "${EDITA_PLUGIN_SDK}/compat" "${EDITA_PLUGIN_SDK}" "${EDITA_SCINTILLA_INCLUDE}" ${ARG_INCLUDES})
    target_compile_definitions(${name} PRIVATE
        EDITA_NPP_PLUGIN=1
        EDITA_PLUGIN_VERSION="${EDITA_PLUGIN_VERSION}"
        EDITA_PLUGIN_DISPLAY_NAME="${EDITA_PLUGIN_DISPLAY_NAME}"
        EDITA_PLUGIN_HOMEPAGE="${EDITA_PLUGIN_HOMEPAGE}"
        ${ARG_DEFINITIONS})
    # El código original se escribió para MSVC: sus avisos no interesan aquí
    target_compile_options(${name} PRIVATE -w $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)
    _edita_link_qt(${name})
    target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES} ${CMAKE_DL_LIBS})
endfunction()

# Un port escrito para la API de Edita (edita_plugin.h) que reutiliza partes
# portables del plugin original: sin la capa de Win32.
#   edita_native_plugin(<carpeta> SOURCES … [INCLUDES …] [LIBRARIES …] [DEFINITIONS …])
function(edita_native_plugin name)
    cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDES;LIBRARIES;DEFINITIONS" ${ARGN})
    add_library(${name} MODULE ${ARG_SOURCES})
    set_target_properties(${name} PROPERTIES
        PREFIX ""
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden)
    target_include_directories(${name} PRIVATE "${EDITA_PLUGIN_SDK}" "${EDITA_SCINTILLA_INCLUDE}" ${ARG_INCLUDES})
    target_compile_definitions(${name} PRIVATE
        EDITA_PLUGIN_VERSION="${EDITA_PLUGIN_VERSION}"
        EDITA_PLUGIN_DISPLAY_NAME="${EDITA_PLUGIN_DISPLAY_NAME}"
        EDITA_PLUGIN_HOMEPAGE="${EDITA_PLUGIN_HOMEPAGE}"
        ${ARG_DEFINITIONS})
    _edita_link_qt(${name})
    target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES})
endfunction()
