# Editor hexadecimal (fase 89): un plugin propio de Edita, escrito desde cero en Qt. Hace lo
# que el plugin HEX-Editor de Notepad++, sobre los bytes del archivo en el disco.
# El modelo (HexBuffer) y la vista (HexView) no dependen del plugin: las pruebas los compilan.
edita_add_plugin(EditaHex EditaHex.cpp HexBuffer.cpp HexBuffer.h HexView.cpp HexView.h)
set_target_properties(EditaHex PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON AUTOMOC ON)
target_link_libraries(EditaHex PRIVATE Qt6::Widgets)

# Traducciones (translations/editahex_<idioma>.ts) dentro del .so; el inglés solo lleva los plurales
set(EH_TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/editahex_es.ts ${CMAKE_CURRENT_SOURCE_DIR}/translations/editahex_en.ts)
qt_add_lupdate(TS_FILES ${EH_TS_FILES} SOURCE_TARGETS EditaHex LUPDATE_TARGET editahex_lupdate
    OPTIONS -no-obsolete -locations relative)
qt_add_lrelease(TS_FILES ${EH_TS_FILES} LRELEASE_TARGET editahex_lrelease QM_FILES_OUTPUT_VARIABLE EH_QM_FILES)
qt_add_resources(EditaHex editahex_translations PREFIX /editahex/i18n BASE ${CMAKE_CURRENT_BINARY_DIR} FILES ${EH_QM_FILES})
