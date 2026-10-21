# Port de MIME Tools a Edita: el código original de src/, sin cambios, con la capa
# de compatibilidad Win32 del SDK (compat/windows.h y npp_compat.cpp).
cmake_minimum_required(VERSION 3.20)
project(mimeTools C CXX)
include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)
edita_npp_plugin(mimeTools SOURCES
    ../src/mimeTools.cpp ../src/b64.cpp ../src/qp.cpp ../src/url.cpp ../src/saml.cpp ../src/tinflate.c)
