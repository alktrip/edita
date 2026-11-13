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
    # EDITA_LSP=0: sin servidores de lenguaje (los que haya instalados); tst_lsp los activa.
    # EDITA_PLUGIN_CATALOG_URL=none: el administrador de plugins no descarga el catálogo de
    # Edita (las pruebas del catálogo ponen el suyo)
    # EDITA_UPDATE_URL=none: no se pregunta a GitHub por versiones nuevas (tst_updatecheck
    # pone la suya)
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EDITA_LSP=0;EDITA_PLUGIN_CATALOG_URL=none;EDITA_UPDATE_URL=none")
    if(ARG_LABELS)
        set_tests_properties(${name} PROPERTIES LABELS "${ARG_LABELS}")
    endif()
endfunction()

# Núcleo: sin widgets ni Scintilla
edita_add_test(tst_encoding LIBS edita_core)
edita_add_test(tst_textfile LIBS edita_core)
edita_add_test(tst_paths LIBS edita_core)
edita_add_test(tst_catalog LIBS edita_core)
edita_add_test(tst_session LIBS edita_core)
edita_add_test(tst_lineops LIBS edita_core)

# Sintaxis (lexers y estilos sobre Scintilla)
edita_add_test(tst_syntax LIBS edita_syntax)

# Buscar y reemplazar
edita_add_test(tst_search LIBS edita_search)

# Documentos sobre Scintilla
edita_add_test(tst_documentmanager LIBS edita_doc)
edita_add_test(tst_perf LIBS edita_ui LABELS perf)

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
edita_add_test(tst_updatecheck LIBS edita_ui)
# Fase 91: Ayuda › Acerca de
edita_add_test(tst_about LIBS edita_ui)
edita_add_test(tst_morelanguages LIBS edita_ui)
# Fase 66: un proceso que falla de verdad, con el manejador de fallos
add_executable(crash_helper crash_helper.cpp)
target_link_libraries(crash_helper PRIVATE edita_core)
edita_add_test(tst_crashreport LIBS edita_ui)
edita_add_test(tst_viewtabs LIBS edita_ui)
edita_add_test(tst_edittools LIBS edita_ui)
edita_add_test(tst_onselection LIBS edita_ui)
edita_add_test(tst_preferencepages LIBS edita_ui)
edita_add_test(tst_importmenu LIBS edita_ui)
edita_add_test(tst_formatting LIBS edita_ui)
edita_add_test(tst_spelling LIBS edita_ui)
edita_add_test(tst_shortcommands LIBS edita_ui)
edita_add_test(tst_transfer LIBS edita_ui)
edita_add_test(tst_fileassociation LIBS edita_ui)
add_dependencies(tst_crashreport crash_helper)
target_compile_definitions(tst_crashreport PRIVATE EDITA_CRASH_HELPER="$<TARGET_FILE:crash_helper>")
edita_add_test(tst_statusbar LIBS edita_ui)
edita_add_test(tst_navigation LIBS edita_ui)
edita_add_test(tst_findbar LIBS edita_ui)
edita_add_test(tst_uipolish LIBS edita_ui)
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
# Fase 82: la API de plugins de la 1.x no cambia
edita_add_test(tst_pluginapi LIBS edita_ui)
# Fase 83: el historial local
edita_add_test(tst_localhistory LIBS edita_ui)
# Fase 84: las tareas del proyecto
edita_add_test(tst_tasks LIBS edita_ui)
# Fase 85: Git desde el margen y el panel Git
edita_add_test(tst_githunks LIBS edita_ui)
if(EDITA_BUILD_PLUGINS)
    edita_add_test(tst_plugins LIBS edita_ui)
    add_dependencies(tst_plugins EditaDemo)
    # Fase 63: un plugin de lexer (los lenguajes «externos» de N++). Fuera de tests/plugins,
    # que es la carpeta de plugins de los ejecutables de prueba: las demás suites lo cargarían
    add_library(EditaTestLexer MODULE fixtures/lexer-plugin/EditaTestLexer.cpp)
    set_target_properties(EditaTestLexer PROPERTIES PREFIX "" CXX_VISIBILITY_PRESET hidden
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lexer-plugins/EditaTestLexer)
    target_include_directories(EditaTestLexer PRIVATE ${PROJECT_SOURCE_DIR}/plugin-sdk
        ${PROJECT_SOURCE_DIR}/third_party/scintilla/include)
    target_compile_options(EditaTestLexer PRIVATE -Wall -Wextra -Wpedantic $<$<BOOL:${EDITA_WERROR}>:-Werror>)
    edita_add_test(tst_externallexers LIBS edita_ui)
    add_dependencies(tst_externallexers EditaTestLexer)
    target_compile_definitions(tst_externallexers PRIVATE EDITA_TEST_PLUGIN_DIR="${CMAKE_CURRENT_BINARY_DIR}/lexer-plugins")
    target_compile_definitions(tst_plugins PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
    # Shortcut Mapper (usa el plugin de ejemplo para las órdenes de plugins)
    edita_add_test(tst_shortcuts LIBS edita_ui)
    add_dependencies(tst_shortcuts EditaDemo)
    target_compile_definitions(tst_shortcuts PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
    # Idiomas de la interfaz (usa el plugin de ejemplo para NPPM_GETNATIVELANGFILENAME)
    edita_add_test(tst_i18n LIBS edita_ui)
    add_dependencies(tst_i18n EditaDemo)
    target_compile_definitions(tst_i18n PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
    # La capa de compatibilidad Win32 para plugins de N++ (sin ventanas), en modo UNICODE
    edita_add_test(tst_npp_compat LIBS Qt6::Widgets ${CMAKE_DL_LIBS})
    target_sources(tst_npp_compat PRIVATE ${PROJECT_SOURCE_DIR}/plugin-sdk/compat/npp_win32.cpp
        ${PROJECT_SOURCE_DIR}/plugin-sdk/compat/npp_window.cpp)
    target_include_directories(tst_npp_compat PRIVATE ${PROJECT_SOURCE_DIR}/plugin-sdk/compat
        ${PROJECT_SOURCE_DIR}/third_party/scintilla/include)
    target_compile_definitions(tst_npp_compat PRIVATE UNICODE _UNICODE)
    # edita_rc (el convertidor de .rc del SDK), con los avisos del proyecto, y los recursos
    # de prueba de tests/fixtures/rc en tst_npp_compat
    add_executable(edita_rc ${PROJECT_SOURCE_DIR}/plugin-sdk/tools/edita_rc.cpp
        ${PROJECT_SOURCE_DIR}/plugin-sdk/tools/edita_rc_constants.cpp)
    target_include_directories(edita_rc PRIVATE ${PROJECT_SOURCE_DIR}/plugin-sdk/tools
        ${PROJECT_SOURCE_DIR}/plugin-sdk/compat ${PROJECT_SOURCE_DIR}/third_party/scintilla/include)
    target_compile_options(edita_rc PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        $<$<BOOL:${EDITA_WERROR}>:-Werror>)
    set(rcFixtures ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/rc)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tst_npp_compat_resources.cpp
        COMMAND edita_rc -o ${CMAKE_CURRENT_BINARY_DIR}/tst_npp_compat_resources.cpp ${rcFixtures}/prueba.rc ${rcFixtures}/utf16.rc
        DEPENDS edita_rc ${rcFixtures}/prueba.rc ${rcFixtures}/utf16.rc ${rcFixtures}/prueba_res.h
        VERBATIM)
    target_sources(tst_npp_compat PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/tst_npp_compat_resources.cpp)
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
    # Fase 88: el detector de secretos (el motor se compila también en la prueba)
    edita_add_test(tst_secretscanner LIBS edita_ui)
    target_sources(tst_secretscanner PRIVATE ${PROJECT_SOURCE_DIR}/plugins/SecretScanner/SecretRules.cpp)
    add_dependencies(tst_secretscanner SecretScanner)
    target_compile_definitions(tst_secretscanner PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
    # Fase 89: el editor hexadecimal (el modelo y la vista se compilan también en la prueba)
    edita_add_test(tst_hexeditor LIBS edita_ui)
    target_sources(tst_hexeditor PRIVATE ${PROJECT_SOURCE_DIR}/plugins/EditaHex/HexBuffer.cpp
        ${PROJECT_SOURCE_DIR}/plugins/EditaHex/HexView.cpp ${PROJECT_SOURCE_DIR}/plugins/EditaHex/HexView.h)
    add_dependencies(tst_hexeditor EditaHex)
    target_compile_definitions(tst_hexeditor PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
    # Fase 90: FTP y SFTP (el núcleo y la sesión se compilan también en la prueba)
    edita_add_test(tst_editaftp LIBS edita_ui)
    target_sources(tst_editaftp PRIVATE ${PROJECT_SOURCE_DIR}/plugins/EditaFTP/FtpCore.cpp
        ${PROJECT_SOURCE_DIR}/plugins/EditaFTP/FtpSession.cpp)
    add_dependencies(tst_editaftp EditaFTP)
    target_compile_definitions(tst_editaftp PRIVATE EDITA_PLUGIN_BUILD_DIR="${PROJECT_BINARY_DIR}/plugins")
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

# Las capturas del README (docs/images): «cmake --build build --target edita_readme_images».
# La del administrador de plugins necesita el catálogo publicado en EDITA_PLUGIN_CATALOG_URL
add_executable(readme_screenshots readme_screenshots.cpp)
target_link_libraries(readme_screenshots PRIVATE edita_ui edita_warnings)
set_target_properties(readme_screenshots PROPERTIES AUTOMOC ON)
add_custom_target(edita_readme_images
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 EDITA_LSP=0 EDITA_UPDATE_URL=none
            EDITA_PLUGIN_PATH=${PROJECT_BINARY_DIR}/plugins
            $<TARGET_FILE:readme_screenshots> es ${PROJECT_SOURCE_DIR}/docs/images
    # Las del README en inglés (README.md); las de arriba son las de README.es.md
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 EDITA_LSP=0 EDITA_UPDATE_URL=none
            EDITA_PLUGIN_PATH=${PROJECT_BINARY_DIR}/plugins
            $<TARGET_FILE:readme_screenshots> en ${PROJECT_SOURCE_DIR}/docs/images/en
    DEPENDS readme_screenshots
    COMMENT "Capturas del README")

# Con sanitizers (EDITA_SANITIZE, fase 93) no tienen sentido: tst_perf mide tiempos (todo va de
# 2 a 3 veces más lento) y tst_crashreport provoca fallos a propósito, que UBSan y ASan
# atrapan antes que el manejador de Edita
if(EDITA_SANITIZE)
    set_tests_properties(tst_perf tst_crashreport PROPERTIES DISABLED TRUE)
endif()
