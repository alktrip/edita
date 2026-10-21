# This file is part of Edita.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compilar el código fuente de un plugin de Notepad++ para Edita.
#
#   cmake_minimum_required(VERSION 3.20)
#   project(MiPlugin CXX C)
#   include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)
#   edita_npp_plugin(MiPlugin SOURCES ../src/plugin.cpp … [INCLUDES …] [LIBRARIES …])
#
# Variables (las pone el administrador de plugins al compilar una receta):
#   EDITA_PLUGIN_SDK          carpeta del SDK (edita_plugin.h y compat/)
#   EDITA_SCINTILLA_INCLUDE   carpeta con Scintilla.h y Sci_Position.h (por defecto, el SDK)
#   EDITA_PLUGIN_VERSION      versión ("3.1")
#   EDITA_PLUGIN_DISPLAY_NAME, EDITA_PLUGIN_HOMEPAGE: para los diálogos que no existen
#
# El resultado es <carpeta>.so, con las exportaciones de Edita (compat/npp_compat.cpp).

if(NOT EDITA_PLUGIN_SDK)
    get_filename_component(EDITA_PLUGIN_SDK "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
if(NOT EDITA_SCINTILLA_INCLUDE)
    set(EDITA_SCINTILLA_INCLUDE "${EDITA_PLUGIN_SDK}")
endif()

function(edita_npp_plugin name)
    cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDES;LIBRARIES;DEFINITIONS" ${ARGN})
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_library(${name} MODULE ${ARG_SOURCES} "${EDITA_PLUGIN_SDK}/compat/npp_compat.cpp")
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
    target_link_libraries(${name} PRIVATE Qt6::Widgets ${ARG_LIBRARIES})
endfunction()

# Un port escrito para la API de Edita (edita_plugin.h) que reutiliza partes
# portables del plugin original: sin la capa de Win32.
#   edita_native_plugin(<carpeta> SOURCES … [INCLUDES …] [LIBRARIES …] [DEFINITIONS …])
function(edita_native_plugin name)
    cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDES;LIBRARIES;DEFINITIONS" ${ARGN})
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
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
    target_link_libraries(${name} PRIVATE Qt6::Widgets ${ARG_LIBRARIES})
endfunction()
