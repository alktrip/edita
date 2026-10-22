# Plugin MarkdownViewer++: port nativo de MarkdownViewer++ de Notepad++ (C#, MIT).
# Markdown con md4c (third_party/md4c); el panel, la exportación y la impresión con Qt.
edita_add_plugin(MarkdownViewerPlusPlus MarkdownViewerPlusPlus.cpp)
set_target_properties(MarkdownViewerPlusPlus PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_link_libraries(MarkdownViewerPlusPlus PRIVATE md4c Qt6::Widgets Qt6::PrintSupport)

# Traducciones del panel y los diálogos (translations/markdownviewer_<idioma>.ts), dentro
# del .so; "cmake --build build --target markdownviewer_lupdate" las actualiza
set(MV_TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/markdownviewer_es.ts)
qt_add_lupdate(TS_FILES ${MV_TS_FILES} SOURCE_TARGETS MarkdownViewerPlusPlus LUPDATE_TARGET markdownviewer_lupdate
    OPTIONS -no-obsolete -locations relative -tr-function-alias tr+=tr)
qt_add_lrelease(TS_FILES ${MV_TS_FILES} LRELEASE_TARGET markdownviewer_lrelease QM_FILES_OUTPUT_VARIABLE MV_QM_FILES)
qt_add_resources(MarkdownViewerPlusPlus markdownviewer_translations PREFIX /markdownviewer/i18n
    BASE ${CMAKE_CURRENT_BINARY_DIR} FILES ${MV_QM_FILES})
