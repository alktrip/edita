# Plugins de ejemplo. Cada uno se genera en <build>/plugins/<Nombre>/<Nombre>.so,
# donde el binario de desarrollo los busca, y se instala en
# <prefijo>/lib/<id>/plugins/<Nombre>/. Con NO_INSTALL no va en los paquetes de Edita: se
# distribuye con el catálogo de plugins (aquí se compila para las pruebas).
function(edita_add_plugin name)
    cmake_parse_arguments(ARG "NO_INSTALL" "" "" ${ARGN})
    add_library(${name} MODULE ${ARG_UNPARSED_ARGUMENTS})
    set_target_properties(${name} PROPERTIES
        PREFIX ""
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/plugins/${name})
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/plugin-sdk
        ${PROJECT_SOURCE_DIR}/third_party/scintilla/include)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic $<$<BOOL:${EDITA_WERROR}>:-Werror>)
    if(NOT ARG_NO_INSTALL)
        install(TARGETS ${name} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/${EDITA_APP_ID}/plugins/${name})
    endif()
endfunction()

edita_add_plugin(EditaDemo EditaDemo/EditaDemo.c)
set_target_properties(EditaDemo PROPERTIES C_STANDARD 11)

# Python Script incrusta el Python del sistema: en macOS no va en el paquete (por ahora)
if(APPLE)
    set(EDITA_PYTHONSCRIPT_DEFAULT OFF)
else()
    set(EDITA_PYTHONSCRIPT_DEFAULT ON)
endif()
option(EDITA_PYTHONSCRIPT "Compilar el plugin Python Script" ${EDITA_PYTHONSCRIPT_DEFAULT})
if(EDITA_PYTHONSCRIPT)
    add_subdirectory(PythonScript)
endif()
add_subdirectory(MarkdownViewerPlusPlus)
add_subdirectory(SecretScanner)
add_subdirectory(EditaHex)
add_subdirectory(EditaFTP)
