# FTP y SFTP (fase 90): un plugin propio de Edita que hace lo que NppFTP en Notepad++. No va
# en los paquetes de Edita: se distribuye con el catálogo de plugins (edita-plugins), que lo
# compila con sdk/CMakeLists.txt. Aquí se compila para las pruebas (tests/tst_editaftp.cpp).
# Sin moc: el SDK de las recetas no lo trae.
set(EDITAFTP_SOURCES EditaFTP.cpp FtpCore.cpp FtpCore.h FtpSession.cpp FtpSession.h)
edita_add_plugin(EditaFTP NO_INSTALL ${EDITAFTP_SOURCES})
set_target_properties(EditaFTP PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_link_libraries(EditaFTP PRIVATE Qt6::Widgets ${CMAKE_DL_LIBS})

# Traducciones: el plugin las carga de translations/ junto al .so. Los .qm también están en
# el repositorio, porque el SDK de las recetas no trae lrelease; tst_editaftp comprueba que
# son los que salen de los .ts.
set(EF_TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/editaftp_es.ts)
qt_add_lupdate(TS_FILES ${EF_TS_FILES} SOURCE_TARGETS EditaFTP LUPDATE_TARGET editaftp_lupdate
    OPTIONS -no-obsolete -locations relative)
qt_add_lrelease(TS_FILES ${EF_TS_FILES} LRELEASE_TARGET editaftp_lrelease QM_FILES_OUTPUT_VARIABLE EF_QM_FILES)
set(EF_QM_DIR ${PROJECT_BINARY_DIR}/plugins/EditaFTP/translations)
add_custom_command(TARGET EditaFTP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EF_QM_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EF_QM_FILES} ${EF_QM_DIR})
add_dependencies(EditaFTP editaftp_lrelease)
