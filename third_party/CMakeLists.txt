# Librerías de terceros copiadas de Notepad++ con tools/import-third-party.sh.
# No se editan a mano: los cambios locales van en patches/ (ver PATCHES.md).
# Se compilan sin warnings (-w): no es código nuestro.

# --- Lexilla (con los lexers propios de N++: searchResult, user, objc) --------
file(GLOB LEXILLA_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/lexilla/lexlib/*.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/lexilla/lexers/*.cxx)

add_library(lexilla STATIC
    ${LEXILLA_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/lexilla/src/Lexilla.cxx)
target_include_directories(lexilla
    SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/lexilla/include
        ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lexilla/lexlib)
target_compile_options(lexilla PRIVATE -w)
set_target_properties(lexilla PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- Scintilla + port Qt (ScintillaEditBase) + regex Boost de N++ -------------
# SCI_OWNREGEX hace que Document.cxx use CreateRegexSearch() de BoostRegExSearch.cxx
# en lugar del motor RESearch propio, exactamente como en Notepad++.
file(GLOB SCINTILLA_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/src/*.cxx)

add_library(scintilla_qt STATIC
    ${SCINTILLA_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/qt/ScintillaEditBase/PlatQt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/qt/ScintillaEditBase/ScintillaQt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/qt/ScintillaEditBase/ScintillaEditBase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/qt/ScintillaEditBase/ScintillaQt.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/qt/ScintillaEditBase/ScintillaEditBase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/boostregex/BoostRegExSearch.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/boostregex/UTF8DocumentIterator.cxx)
target_include_directories(scintilla_qt
    SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/include
        ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/src
        ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/qt/ScintillaEditBase
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/boostregex)
target_compile_definitions(scintilla_qt
    PUBLIC  SCINTILLA_QT=1
    PRIVATE SCI_OWNREGEX BOOST_REGEX_STANDALONE MAKING_LIBRARY=1)
target_compile_options(scintilla_qt PRIVATE -w)
target_link_libraries(scintilla_qt PUBLIC Qt6::Widgets Qt6::Core5Compat)
set_target_properties(scintilla_qt PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)

# --- uchardet (copia de PowerEditor/src/uchardet) -----------------------------
# Se usa esta versión, y no la del sistema, para que la detección de encoding
# dé los mismos resultados que Notepad++ (FileManager::detectCodepage).
file(GLOB UCHARDET_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/uchardet/*.cpp)
add_library(uchardet_npp STATIC ${UCHARDET_SOURCES})
target_include_directories(uchardet_npp SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/uchardet)
# El código de N++ usa uintptr_t sin incluir <stdint.h> (en MSVC llega de forma
# indirecta). Se fuerza la cabecera desde el build en vez de parchear el fuente.
target_compile_options(uchardet_npp PRIVATE -w -include stdint.h)
set_target_properties(uchardet_npp PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- pugixml (copia de PowerEditor/src/pugixml, la que usa N++ para sus XML) ---
add_library(pugixml STATIC ${CMAKE_CURRENT_SOURCE_DIR}/pugixml/pugixml.cpp)
target_include_directories(pugixml SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pugixml)
target_compile_options(pugixml PRIVATE -w)
set_target_properties(pugixml PROPERTIES POSITION_INDEPENDENT_CODE ON)
