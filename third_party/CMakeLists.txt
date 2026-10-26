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

# --- md4c 0.5.2 (Markdown CommonMark con las extensiones de GitHub, MIT) -------
# El analizador que usa Qt (QTextDocument::setMarkdown) y su salida HTML: para el
# plugin MarkdownViewer++. Copiado de https://github.com/mity/md4c (release-0.5.2).
add_library(md4c STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/md4c/md4c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/md4c/md4c-html.c
    ${CMAKE_CURRENT_SOURCE_DIR}/md4c/entity.c)
target_include_directories(md4c SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/md4c)
target_compile_options(md4c PRIVATE -w)
# Símbolos ocultos: Qt Gui trae su propia copia de md4c (otra versión, con otros valores
# de las opciones) y, con los símbolos visibles, md_html del plugin llamaba a su md_parse
set_target_properties(md4c PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)

# --- libvterm 0.3.3 (emulador VT220/xterm, MIT) ---------------------------------
# La emulación de la terminal integrada (ui/TerminalPanel); el dibujo lo hace Edita.
# Copiado de https://launchpad.net/libvterm (v0.3.3), con las tablas ya generadas.
add_library(vterm STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libvterm/src/encoding.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvterm/src/keyboard.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvterm/src/mouse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvterm/src/parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvterm/src/pen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvterm/src/screen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvterm/src/state.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvterm/src/unicode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvterm/src/vterm.c)
target_include_directories(vterm SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libvterm/include)
target_compile_options(vterm PRIVATE -w -std=c99)
target_compile_definitions(vterm PRIVATE _XOPEN_SOURCE=600 _DEFAULT_SOURCE)
set_target_properties(vterm PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
