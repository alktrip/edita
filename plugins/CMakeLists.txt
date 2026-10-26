# Plugins de ejemplo. Cada uno se genera en <build>/plugins/<Nombre>/<Nombre>.so,
# donde el binario de desarrollo los busca, y se instala en
# <prefijo>/lib/<id>/plugins/<Nombre>/.
function(edita_add_plugin name)
    add_library(${name} MODULE ${ARGN})
    set_target_properties(${name} PROPERTIES
        PREFIX ""
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/plugins/${name})
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/plugin-sdk
        ${PROJECT_SOURCE_DIR}/third_party/scintilla/include)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic $<$<BOOL:${EDITA_WERROR}>:-Werror>)
    install(TARGETS ${name} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/${EDITA_APP_ID}/plugins/${name})
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
