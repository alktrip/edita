# Plugin Python Script: Python integrado con la API del PythonScript de Notepad++.
# Necesita las cabeceras de Python para incrustar el intérprete (python3-embed).
find_package(Python3 3.8 COMPONENTS Development.Embed)
if(NOT Python3_Development.Embed_FOUND)
    message(STATUS "Python Script: no se compila (faltan las cabeceras de Python)")
    return()
endif()

edita_add_plugin(PythonScript PythonScript.cpp Config.cpp Config.h NppPythonScript.h)
set_target_properties(PythonScript PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
# Python.h usa "slots" como nombre de campo: sin las palabras clave de Qt
target_compile_definitions(PythonScript PRIVATE QT_NO_KEYWORDS)
target_link_libraries(PythonScript PRIVATE Python3::Python Qt6::Widgets ${CMAKE_DL_LIBS})

# Traducciones de sus diálogos (translations/pythonscript_<idioma>.ts), dentro del .so;
# "cmake --build build --target pythonscript_lupdate" las actualiza
set(PS_TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/pythonscript_es.ts)
qt_add_lupdate(TS_FILES ${PS_TS_FILES} SOURCE_TARGETS PythonScript LUPDATE_TARGET pythonscript_lupdate
    OPTIONS -no-obsolete -locations relative)
qt_add_lrelease(TS_FILES ${PS_TS_FILES} LRELEASE_TARGET pythonscript_lrelease QM_FILES_OUTPUT_VARIABLE PS_QM_FILES)
qt_add_resources(PythonScript pythonscript_translations PREFIX /pythonscript/i18n
    BASE ${CMAKE_CURRENT_BINARY_DIR} FILES ${PS_QM_FILES})

# El paquete Npp y los scripts junto al .so (en el build y en la instalación)
set(PS_OUT ${PROJECT_BINARY_DIR}/plugins/PythonScript)
add_custom_target(PythonScriptData ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/lib ${PS_OUT}/lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/scripts ${PS_OUT}/scripts
    COMMENT "Python Script: copiando lib/ y scripts/"
    VERBATIM)
add_dependencies(PythonScript PythonScriptData)
install(DIRECTORY lib scripts DESTINATION ${CMAKE_INSTALL_LIBDIR}/${EDITA_APP_ID}/plugins/PythonScript
        PATTERN "__pycache__" EXCLUDE)
install(FILES NppPythonScript.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/edita)
