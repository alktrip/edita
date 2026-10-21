# Flags de warnings del código propio (no se aplican a third_party/)
add_library(edita_warnings INTERFACE)
target_compile_options(edita_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor -Woverloaded-virtual
    $<$<BOOL:${EDITA_WERROR}>:-Werror>)

configure_file(config/AppInfo.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/AppInfo.h @ONLY)

# Núcleo sin widgets (documentos, encoding, configuración…): testeable por separado
add_library(edita_core STATIC
    core/ApiFile.cpp
    core/ApiFile.h
    core/Backup.cpp
    core/Backup.h
    core/Codec.cpp
    core/Codec.h
    core/ConfigXml.cpp
    core/ConfigXml.h
    core/Encoding.cpp
    core/Encoding.h
    core/EncodingDetection.cpp
    core/EncodingDetection.h
    core/Eol.cpp
    core/Eol.h
    core/LangType.cpp
    core/LangType.h
    core/LineOperations.cpp
    core/LineOperations.h
    core/Locale.cpp
    core/Locale.h
    core/Macro.cpp
    core/Macro.h
    core/Paths.cpp
    core/Paths.h
    core/PluginCatalog.cpp
    core/PluginCatalog.h
    core/PluginInstaller.cpp
    core/PluginInstaller.h
    core/PluginRecipe.cpp
    core/PluginRecipe.h
    core/Preferences.cpp
    core/Preferences.h
    core/Position.h
    core/Session.cpp
    core/Session.h
    core/TextFile.cpp
    core/TextFile.h)
target_include_directories(edita_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(edita_core
    PUBLIC  Qt6::Core
    PRIVATE uchardet_npp pugixml edita_warnings)
# Builds de desarrollo: los datos se buscan también en el árbol de fuentes
target_compile_definitions(edita_core PRIVATE EDITA_SOURCE_DATA_DIR="${PROJECT_SOURCE_DIR}/data")
# Paquetes .zip del administrador de plugins (libzip, opcional; se busca en el CMakeLists principal)
if(LIBZIP_FOUND)
    target_link_libraries(edita_core PRIVATE PkgConfig::LIBZIP)
    target_compile_definitions(edita_core PRIVATE EDITA_HAVE_LIBZIP)
else()
    message(STATUS "libzip no encontrado: el administrador de plugins no podrá instalar paquetes")
endif()

# Documentos abiertos sobre Scintilla (FileManager/Buffer de N++)
add_library(edita_doc STATIC
    doc/Buffer.cpp
    doc/Buffer.h
    doc/DocumentManager.cpp
    doc/DocumentManager.h)
target_link_libraries(edita_doc
    PUBLIC  edita_core scintilla_qt
    PRIVATE edita_warnings)
set_target_properties(edita_doc PROPERTIES AUTOMOC ON)
set_target_properties(edita_core PROPERTIES AUTOMOC ON)

# Sintaxis: langs.xml, stylers.xml/temas y su aplicación a Scintilla
add_library(edita_syntax STATIC
    syntax/LanguageDetection.cpp
    syntax/LanguageDetection.h
    syntax/LanguageRegistry.cpp
    syntax/LanguageRegistry.h
    syntax/StyleSheet.cpp
    syntax/StyleSheet.h
    syntax/StyleXml.h
    syntax/UserLanguages.cpp
    syntax/UserLanguages.h
    syntax/SyntaxManager.cpp
    syntax/SyntaxManager.h
    syntax/SyntaxStyler.cpp
    syntax/SyntaxStyler.h)
target_link_libraries(edita_syntax
    PUBLIC  edita_core scintilla_qt lexilla Qt6::Gui
    PRIVATE pugixml edita_warnings)
set_target_properties(edita_syntax PROPERTIES AUTOMOC ON)

# Buscar y reemplazar (FindReplaceDlg de N++ sin la interfaz) y buscar en archivos
add_library(edita_search STATIC
    search/FileSearch.cpp
    search/FileSearch.h
    search/SearchEngine.cpp
    search/SearchEngine.h
    search/SearchOptions.cpp
    search/SearchOptions.h
    search/SearchTarget.cpp
    search/SearchTarget.h)
target_link_libraries(edita_search
    PUBLIC  edita_core scintilla_qt
    PRIVATE edita_warnings)
# SCI_OWNREGEX: declara g_exceptionMessage en BoostRegexSearch.h
target_compile_definitions(edita_search PRIVATE SCI_OWNREGEX)
set_target_properties(edita_search PROPERTIES AUTOMOC ON)

# Biblioteca con todo menos main(): la enlazan la aplicación y los tests
add_library(edita_ui STATIC
    ui/AutoCompletion.cpp
    ui/AutoCompletion.h
    ui/DocStatusBar.cpp
    ui/DocStatusBar.h
    ui/DocTabBar.cpp
    ui/DocTabBar.h
    ui/EditCommands.cpp
    ui/EditCommands.h
    ui/EditorPane.cpp
    ui/EditorPane.h
    ui/EditorView.cpp
    ui/EditorView.h
    ui/FindReplaceDialog.cpp
    ui/FindReplaceDialog.h
    ui/Indentation.cpp
    ui/Indentation.h
    ui/MacroSteps.cpp
    ui/MacroSteps.h
    ui/MainWindow.cpp
    ui/MainWindow.h
    ui/PreferencesDialog.cpp
    ui/PreferencesDialog.h
    ui/Printer.cpp
    ui/Printer.h
    ui/PluginManager.cpp
    ui/PluginManager.h
    ui/PluginsAdminDialog.cpp
    ui/PluginsAdminDialog.h
    ui/Prompter.cpp
    ui/Prompter.h
    ui/SearchResultsPanel.cpp
    ui/SearchResultsPanel.h
    ui/SingleInstance.cpp
    ui/SingleInstance.h
    ui/SmartHighlighter.cpp
    ui/SmartHighlighter.h
    ui/StyleConfiguratorDialog.cpp
    ui/StyleConfiguratorDialog.h
    ui/Appearance.cpp
    ui/Appearance.h
    ui/Icons.cpp
    ui/Icons.h
    ui/Translations.cpp
    ui/Translations.h
    ui/UserLanguageDialog.cpp
    ui/UserLanguageDialog.h
    ui/XmlTagMatcher.cpp
    ui/XmlTagMatcher.h)
target_link_libraries(edita_ui
    PUBLIC  edita_doc edita_syntax edita_search lexilla Qt6::Widgets Qt6::PrintSupport Qt6::Network Qt6::Svg
    PRIVATE edita_warnings)
target_include_directories(edita_ui PUBLIC ${PROJECT_SOURCE_DIR}/plugin-sdk)
set_target_properties(edita_ui PROPERTIES AUTOMOC ON)
# Logo e ícono (resources/), dentro del binario
set(EDITA_ICON_SIZES 16 22 24 32 48 64 128 256 512)
set(EDITA_ICON_FILES "")
foreach(size IN LISTS EDITA_ICON_SIZES)
    list(APPEND EDITA_ICON_FILES icons/hicolor/${size}x${size}/apps/edita.png)
endforeach()
list(TRANSFORM EDITA_ICON_FILES PREPEND ${PROJECT_SOURCE_DIR}/resources/)
# Iconos de línea de la interfaz (se pintan con el color del tema, ui/Icons)
file(GLOB EDITA_UI_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/resources/icons/ui/*.svg)
list(APPEND EDITA_ICON_FILES ${EDITA_UI_ICONS})
qt_add_resources(edita_ui edita_resources
    PREFIX /edita
    BASE ${PROJECT_SOURCE_DIR}/resources
    FILES ${PROJECT_SOURCE_DIR}/resources/logo.png ${EDITA_ICON_FILES})

add_executable(${EDITA_APP_ID} main.cpp)
target_link_libraries(${EDITA_APP_ID} PRIVATE edita_ui edita_warnings)
set_target_properties(${EDITA_APP_ID} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

# Traducciones: el código está en inglés y translations/edita_<idioma>.ts trae cada
# idioma; edita_en.ts solo tiene las formas de plural del inglés (%n).
# "cmake --build build --target edita_lupdate" actualiza los .ts con los textos del código.
set(EDITA_TS_FILES ${PROJECT_SOURCE_DIR}/translations/edita_es.ts)
set(EDITA_TS_PLURALS ${PROJECT_SOURCE_DIR}/translations/edita_en.ts)
qt_add_lupdate(TS_FILES ${EDITA_TS_FILES}
    PLURALS_TS_FILE ${EDITA_TS_PLURALS}
    SOURCE_TARGETS edita_core edita_doc edita_syntax edita_search edita_ui ${EDITA_APP_ID}
    LUPDATE_TARGET edita_lupdate
    OPTIONS -no-obsolete -locations relative)
qt_add_lrelease(TS_FILES ${EDITA_TS_FILES} ${EDITA_TS_PLURALS}
    LRELEASE_TARGET edita_lrelease
    QM_FILES_OUTPUT_VARIABLE EDITA_QM_FILES)
qt_add_resources(edita_ui edita_translations
    PREFIX /edita/i18n
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${EDITA_QM_FILES})

include(GNUInstallDirs)
install(TARGETS ${EDITA_APP_ID} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY ${PROJECT_SOURCE_DIR}/data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/${EDITA_APP_ID})
# Menú del escritorio: <id>.desktop y el ícono <id>.png en el tema hicolor
configure_file(${PROJECT_SOURCE_DIR}/resources/edita.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/${EDITA_APP_ID}.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${EDITA_APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
foreach(size IN LISTS EDITA_ICON_SIZES)
    install(FILES ${PROJECT_SOURCE_DIR}/resources/icons/hicolor/${size}x${size}/apps/edita.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps RENAME ${EDITA_APP_ID}.png)
endforeach()
