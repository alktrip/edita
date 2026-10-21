# Port de XML Tools a Edita: XMLToolsEdita.cpp (las órdenes del menú del original
# sobre la API de Edita) con la biblioteca QuickXml del original, sin cambios.
# libxml2 sustituye a MSXML para la sintaxis, la validación y XPath; libxslt, si
# está, para la transformación XSL.
cmake_minimum_required(VERSION 3.20)
project(XMLTools CXX)
include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBXML2 REQUIRED IMPORTED_TARGET libxml-2.0)
pkg_check_modules(LIBXSLT IMPORTED_TARGET libxslt)
set(xslt_libs "")
set(xslt_defs "")
if(LIBXSLT_FOUND)
    set(xslt_libs PkgConfig::LIBXSLT)
    set(xslt_defs XMLTOOLS_HAVE_XSLT)
endif()
edita_native_plugin(XMLTools
    SOURCES XMLToolsEdita.cpp ../QuickXmlLib/QuickXml/src/XmlParser.cpp ../QuickXmlLib/QuickXml/src/XmlFormater.cpp
    INCLUDES ../QuickXmlLib/QuickXml/src
    LIBRARIES PkgConfig::LIBXML2 ${xslt_libs}
    DEFINITIONS ${xslt_defs})
# QuickXml se escribió para MSVC, que incluye <cstring> de forma indirecta
set_source_files_properties(../QuickXmlLib/QuickXml/src/XmlParser.cpp ../QuickXmlLib/QuickXml/src/XmlFormater.cpp
    PROPERTIES COMPILE_OPTIONS "-include;cstring;-w")
