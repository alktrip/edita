# This file is part of Edita.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# FTP y SFTP para el catálogo de plugins: la receta de edita-plugins (recipes/EditaFTP)
# compila esta carpeta con el SDK de Edita (sin moc, sin lrelease: los .qm están en el
# repositorio).
cmake_minimum_required(VERSION 3.20)
project(EditaFTP CXX)
include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)
edita_native_plugin(EditaFTP
    SOURCES ../EditaFTP.cpp ../FtpCore.cpp ../FtpSession.cpp
    LIBRARIES ${CMAKE_DL_LIBS})
set_target_properties(EditaFTP PROPERTIES CXX_STANDARD 20)
