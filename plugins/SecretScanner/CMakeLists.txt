# Detector de secretos (fase 88; no existe en Notepad++): un plugin propio de Edita.
# El motor (SecretRules) no depende de la interfaz: las pruebas lo compilan aparte.
edita_add_plugin(SecretScanner SecretScanner.cpp SecretRules.cpp SecretRules.h)
set_target_properties(SecretScanner PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_link_libraries(SecretScanner PRIVATE Qt6::Widgets)

# Traducciones del menú, el panel y los avisos (translations/secretscanner_<idioma>.ts), dentro
# del .so; "cmake --build build --target secretscanner_lupdate" las actualiza
set(SS_TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/secretscanner_es.ts
    ${CMAKE_CURRENT_SOURCE_DIR}/translations/secretscanner_en.ts)
qt_add_lupdate(TS_FILES ${SS_TS_FILES} SOURCE_TARGETS SecretScanner LUPDATE_TARGET secretscanner_lupdate
    OPTIONS -no-obsolete -locations relative)
qt_add_lrelease(TS_FILES ${SS_TS_FILES} LRELEASE_TARGET secretscanner_lrelease QM_FILES_OUTPUT_VARIABLE SS_QM_FILES)
qt_add_resources(SecretScanner secretscanner_translations PREFIX /secretscanner/i18n
    BASE ${CMAKE_CURRENT_BINARY_DIR} FILES ${SS_QM_FILES})
