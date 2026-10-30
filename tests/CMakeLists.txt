# Tests con QtTest. Se ejecutan sin pantalla (QT_QPA_PLATFORM=offscreen).
#   edita_add_test(nombre LIBS ...)  → tests/nombre.cpp enlazado con esas librerías
function(edita_add_test name)
    cmake_parse_arguments(ARG "" "" "LIBS;LABELS" ${ARGN})
    add_executable(${name} ${name}.cpp)
    # Antes de Qt 6.12 (el AppImage y macOS usan 6.8), las macros QTRY_* de Qt convierten
    # un tiempo de 64 bits a int dentro de cada prueba: las mismas advertencias, sin las de
    # conversión (solo en las pruebas)
    if(Qt6Core_VERSION VERSION_LESS 6.12)
        target_link_libraries(${name} PRIVATE ${ARG_LIBS} Qt6::Test)
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
            -Woverloaded-virtual $<$<BOOL:${EDITA_WERROR}>:-Werror>)
    else()
        target_link_libraries(${name} PRIVATE ${ARG_LIBS} edita_warnings Qt6::Test)
    endif()
    target_compile_definitions(${name} PRIVATE
        EDITA_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        EDITA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    set_target_properties(${name} PROPERTIES AUTOMOC ON)
    # GCC 12: falso positivo de -Wrestrict dentro de std::string (corregido en GCC 13)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
        target_compile_options(${name} PRIVATE -Wno-restrict)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    # EDITA_LSP=0: sin servidores de lenguaje (los que haya instalados); tst_lsp los activa
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EDITA_LSP=0")
    if(ARG_LABELS)
        set_tests_properties(${name} PROPERTIES LABELS "${ARG_LABELS}")
    endif()
endfunction()

# Núcleo: sin widgets ni Scintilla
edita_add_test(tst_encoding LIBS edita_core)
edita_add_test(tst_textfile LIBS edita_core)
edita_add_test(tst_paths LIBS edita_core)
edita_add_test(tst_session LIBS edita_core)
edita_add_test(tst_lineops LIBS edita_core)

# Sintaxis (lexers y estilos sobre Scintilla)
edita_add_test(tst_syntax LIBS edita_syntax)

# Buscar y reemplazar
edita_add_test(tst_search LIBS edita_search)

# Documentos sobre Scintilla
edita_add_test(tst_documentmanager LIBS edita_doc)
edita_add_test(tst_perf LIBS edita_doc LABELS perf)

# Motor de edición y UI
edita_add_test(tst_engine LIBS edita_ui)
edita_add_test(tst_mainwindow LIBS edita_ui)
edita_add_test(tst_appearance LIBS edita_ui)
edita_add_test(tst_styleconfigurator LIBS edita_ui)
edita_add_test(tst_columneditor LIBS edita_ui)
edita_add_test(tst_commandpalette LIBS edita_ui)
edita_add_test(tst_gitchanges LIBS edita_ui)
edita_add_test(tst_visual LIBS edita_ui)
edita_add_test(tst_welcome LIBS edita_ui)
edita_add_test(tst_statusbar LIBS edita_ui)
edita_add_test(tst_navigation LIBS edita_ui)
edita_add_test(tst_findbar LIBS edita_ui)
edita_add_test(tst_regextester LIBS edita_ui)
edita_add_test(tst_terminal LIBS edita_ui)
edita_add_test(tst_lsp LIBS edita_ui)
edita_add_test(tst_compare LIBS edita_ui)
edita_add_test(tst_gitexplorer LIBS edita_ui)
edita_add_test(tst_snippets LIBS edita_ui)
edita_add_test(tst_manual LIBS edita_ui)
edita_add_test(tst_mark LIBS edita_ui)
edita_add_test(tst_namedsessions LIBS edita_ui)
edita_add_test(tst_run LIBS edita_ui)
edita_add_test(tst_hidelines LIBS edita_ui)
edita_add_test(tst_windows LIBS edita_ui)
edita_add_test(tst_panels LIBS edita_ui)
# Una sola instancia: también arranca el binario de verdad
edita_add_test(tst_singleinstance LIBS edita_ui)
add_dependencies(tst_singleinstance ${EDITA_APP_ID})
target_compile_definitions(tst_singleinstance PRIVATE EDITA_BINARY="$<TARGET_FILE:${EDITA_APP_ID}>")
edita_add_test(tst_findui LIBS edita_ui)
edita_add_test(tst_splitview LIBS edita_ui)
edita_add_test(tst_datasafety LIBS edita_ui)
edita_add_test(tst_preferences LIBS edita_ui)
edita_add_test(tst_editing LIBS edita_ui)
edita_add_test(tst_printing LIBS edita_ui)
edita_add_test(tst_udl LIBS edita_ui)
edita_add_test(tst_editcommands LIBS edita_ui)
if(EDITA_BUILD_PLUGINS)
    edita_add_test(tst_plugins LIBS edita_ui)
    add_dependencies(tst_plugins EditaDemo)
    target_compile_definitions(tst_plugins PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
    # Shortcut Mapper (usa el plugin de ejemplo para las órdenes de plugins)
    edita_add_test(tst_shortcuts LIBS edita_ui)
    add_dependencies(tst_shortcuts EditaDemo)
    target_compile_definitions(tst_shortcuts PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
    # Idiomas de la interfaz (usa el plugin de ejemplo para NPPM_GETNATIVELANGFILENAME)
    edita_add_test(tst_i18n LIBS edita_ui)
    add_dependencies(tst_i18n EditaDemo)
    target_compile_definitions(tst_i18n PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
    if(LIBZIP_FOUND)
        edita_add_test(tst_pluginsadmin LIBS edita_ui PkgConfig::LIBZIP)
        add_dependencies(tst_pluginsadmin EditaDemo)
        target_compile_definitions(tst_pluginsadmin PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
        # Compila plugins de verdad: tarda más que el resto
        edita_add_test(tst_recipes LIBS edita_ui PkgConfig::LIBZIP)
        add_dependencies(tst_recipes EditaDemo)
        target_compile_definitions(tst_recipes PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
        set_tests_properties(tst_recipes PROPERTIES TIMEOUT 900)
    endif()
    edita_add_test(tst_markdownviewer LIBS edita_ui)
    add_dependencies(tst_markdownviewer MarkdownViewerPlusPlus)
    target_compile_definitions(tst_markdownviewer PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
    if(TARGET PythonScript)
        edita_add_test(tst_pythonscript LIBS edita_ui)
        add_dependencies(tst_pythonscript PythonScript)
        target_compile_definitions(tst_pythonscript PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
    endif()
endif()

# Las capturas del manual de usuario (data/manual/<idioma>/images/), sin pantalla:
# «cmake --build build --target edita_manual_images»
add_executable(manual_screenshots manual_screenshots.cpp)
target_link_libraries(manual_screenshots PRIVATE edita_ui edita_warnings)
set_target_properties(manual_screenshots PROPERTIES AUTOMOC ON)
add_custom_target(edita_manual_images
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen EDITA_LSP=0
            $<TARGET_FILE:manual_screenshots> en ${PROJECT_SOURCE_DIR}/data/manual/en/images
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen EDITA_LSP=0
            $<TARGET_FILE:manual_screenshots> es ${PROJECT_SOURCE_DIR}/data/manual/es/images
    DEPENDS manual_screenshots
    COMMENT "Capturas del manual de usuario"
    VERBATIM)
