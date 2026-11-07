# This file is part of Edita.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compilar el código fuente de un plugin de Notepad++ para Edita.
#
#   cmake_minimum_required(VERSION 3.20)
#   project(MiPlugin CXX C)
#   include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)
#   edita_npp_plugin(MiPlugin [UNICODE] SOURCES ../src/plugin.cpp … [RESOURCES ../src/plugin.rc …]
#                    [INCLUDES …] [LIBRARIES …] [EXPORTS ../exports.def])
#
# EXPORTS es el .def del plugin, si exporta más funciones que las de la API de plugins (en
# Windows, con un .def en vez de __declspec(dllexport)): los plugins de lexer exportan las de
# Lexilla (CreateLexer, GetLexerCount, GetLexerName…). El .so exporta los nombres del .def
# y la API de Edita, y nada más, como la DLL.
#
# RESOURCES son los .rc del plugin: edita_rc (tools/) los convierte al compilar en los
# diálogos, menús y cadenas que usan CreateDialogParam, DialogBoxParam, LoadMenu y
# LoadString. Sus #include se buscan junto al .rc y en INCLUDES.
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
#   EDITA_PLUGIN_SOURCE_ROOT  la raíz del código del plugin (por defecto, la carpeta de
#       arriba de este CMakeLists.txt)
#
# Los plugins son reproducibles: el mismo código con el mismo SDK y el mismo compilador da
# el mismo .so, se compile en la carpeta que se compile (ver _edita_reproducible).
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
    # Solo lo que usa: Qt6::Gui arrastra libOpenGL.so.0 y libGLX.so.0, que no todos los
    # sistemas tienen (Ubuntu 26.10 de escritorio, sin libopengl0)
    target_link_options(${name} PRIVATE LINKER:--as-needed)
endfunction()

# Sin las rutas de la máquina dentro del .so (__FILE__ de los assert, de Qt…): las de
# trabajo se cambian por nombres fijos. Y sin RUNPATH: el .so lo carga Edita, que ya tiene
# Qt cargado (y la carpeta de Qt del AppImage es temporal)
function(_edita_reproducible name)
    if(NOT EDITA_PLUGIN_SOURCE_ROOT)
        get_filename_component(EDITA_PLUGIN_SOURCE_ROOT "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)
    endif()
    set(maps "${EDITA_PLUGIN_SDK}=/edita-plugin-sdk" "${EDITA_PLUGIN_SOURCE_ROOT}=/plugin-source"
             "${CMAKE_BINARY_DIR}=/plugin-build")
    if(NOT EDITA_SCINTILLA_INCLUDE STREQUAL EDITA_PLUGIN_SDK)
        list(APPEND maps "${EDITA_SCINTILLA_INCLUDE}=/edita-scintilla")
    endif()
    if(EDITA_QT_INCLUDE)
        list(APPEND maps "${EDITA_QT_INCLUDE}=/edita-qt/include")
    endif()
    foreach(map ${maps})
        target_compile_options(${name} PRIVATE "-ffile-prefix-map=${map}")
    endforeach()
    set_target_properties(${name} PROPERTIES SKIP_BUILD_RPATH ON)
endfunction()

# edita_rc: el convertidor de .rc, compilado con el compilador del equipo
function(_edita_rc_tool)
    if(TARGET edita_rc)
        return()
    endif()
    add_executable(edita_rc "${EDITA_PLUGIN_SDK}/tools/edita_rc.cpp" "${EDITA_PLUGIN_SDK}/tools/edita_rc_constants.cpp")
    target_include_directories(edita_rc PRIVATE "${EDITA_PLUGIN_SDK}/tools" "${EDITA_PLUGIN_SDK}/compat" "${EDITA_SCINTILLA_INCLUDE}")
    set_target_properties(edita_rc PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    target_compile_options(edita_rc PRIVATE -w)
endfunction()

# Los recursos de los .rc en un .cpp generado, añadido al plugin
function(_edita_plugin_resources name)
    cmake_parse_arguments(ARG "" "" "RESOURCES;INCLUDES" ${ARGN})
    if(NOT ARG_RESOURCES)
        return()
    endif()
    _edita_rc_tool()
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${name}_resources.cpp")
    set(rcFiles "")
    set(includeArgs "")
    foreach(rc ${ARG_RESOURCES})
        get_filename_component(rcPath "${rc}" ABSOLUTE)
        list(APPEND rcFiles "${rcPath}")
    endforeach()
    foreach(dir ${ARG_INCLUDES})
        get_filename_component(dirPath "${dir}" ABSOLUTE)
        list(APPEND includeArgs "-I${dirPath}")
    endforeach()
    add_custom_command(OUTPUT "${output}"
        COMMAND edita_rc -o "${output}" ${includeArgs} ${rcFiles}
        DEPENDS edita_rc ${rcFiles}
        COMMENT "Recursos de ${name} (edita_rc)"
        VERBATIM)
    target_sources(${name} PRIVATE "${output}")
endfunction()

function(edita_npp_plugin name)
    cmake_parse_arguments(ARG "UNICODE" "EXPORTS" "SOURCES;RESOURCES;INCLUDES;LIBRARIES;DEFINITIONS" ${ARGN})
    add_library(${name} MODULE ${ARG_SOURCES}
        "${EDITA_PLUGIN_SDK}/compat/npp_compat.cpp" "${EDITA_PLUGIN_SDK}/compat/npp_win32.cpp"
        "${EDITA_PLUGIN_SDK}/compat/npp_window.cpp")
    _edita_plugin_resources(${name} RESOURCES ${ARG_RESOURCES} INCLUDES ${ARG_INCLUDES})
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
    # El código original se escribió para MSVC: sus avisos no interesan aquí. Para MSVC, «not»,
    # «and» y «or» no son palabras reservadas y se usan como nombres (-fno-operator-names)
    # En C, la misma tolerancia que MSVC con los punteros (un callback con WPARAM donde va LPARAM),
    # que GCC 14 trata como error
    # Las funciones de cadenas de MSVC (_stricmp…) existen sin incluir nada, como con MSVC
    target_compile_options(${name} PRIVATE -include "${EDITA_PLUGIN_SDK}/compat/msvc_crt.h")
    target_compile_options(${name} PRIVATE -w $<$<COMPILE_LANGUAGE:CXX>:-fpermissive -fno-operator-names>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=incompatible-pointer-types -Wno-error=int-conversion
        -Wno-error=implicit-function-declaration>)
    _edita_link_qt(${name})
    _edita_reproducible(${name})
    target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES} ${CMAKE_DL_LIBS})
    if(ARG_EXPORTS)
        _edita_def_exports(${name} "${ARG_EXPORTS}")
    endif()
endfunction()

# Un .def de Windows (EXPORTS y un nombre por línea, quizá con @ordinal, =alias o PRIVATE)
# como version script: esos nombres y la API de Edita son globales, lo demás local. Con un
# version script, la visibilidad oculta sobra (y ocultaría los nombres del .def)
function(_edita_def_exports name def)
    get_filename_component(def "${def}" ABSOLUTE)
    file(STRINGS "${def}" lines)
    set(symbols setInfo getName getFuncsArray beNotified messageProc edita_plugin_api_version edita_plugin_version)
    set(inExports FALSE)
    foreach(line IN LISTS lines)
        string(STRIP "${line}" line)
        string(REGEX REPLACE "[ \t]*;.*$" "" line "${line}") # comentarios
        if(line MATCHES "^EXPORTS")
            set(inExports TRUE)
            string(REGEX REPLACE "^EXPORTS[ \t]*" "" line "${line}")
        elseif(line MATCHES "^(LIBRARY|NAME|SECTIONS|VERSION|HEAPSIZE|STACKSIZE)")
            set(inExports FALSE)
        endif()
        if(inExports AND line MATCHES "^([A-Za-z_][A-Za-z0-9_]*)")
            list(APPEND symbols "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES symbols)
    list(JOIN symbols ";\n        " globals)
    set(script "${CMAKE_CURRENT_BINARY_DIR}/${name}.exports")
    file(WRITE "${script}" "{\n    global:\n        ${globals};\n    local: *;\n};\n")
    set_target_properties(${name} PROPERTIES C_VISIBILITY_PRESET default CXX_VISIBILITY_PRESET default
        LINK_DEPENDS "${script}")
    target_link_options(${name} PRIVATE "LINKER:--version-script=${script}")
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
    _edita_reproducible(${name})
    target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES})
endfunction()
