// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
//
// Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team).
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "core/LineOperations.h"

#include <ScintillaEditBase.h>

#include <string>
#include <utility>
#include <vector>

// Órdenes de edición de los menús Editar, Buscar y Ver que trabajan sobre una
// vista de Scintilla. Son las funciones de Notepad_plus / ScintillaEditView que
// las implementan en N++, sin ventanas: lo que necesitan del resto (símbolos de
// comentario, portapapeles, opciones) se les pasa.
namespace edita {

// Marcador de los marcadores de línea (MARK_BOOKMARK en ScintillaEditView.h)
inline constexpr int kBookmarkMarker = 20;
// La bombilla de las acciones de código del servidor de lenguaje (no existe en N++)
inline constexpr int kLightbulbMarker = 24;

// ScintillaEditView::getSelectionLinesRange: líneas de la selección principal; si
// acaba al principio de una línea, esa no cuenta
std::pair<sptr_t, sptr_t> selectionLinesRange(ScintillaEditBase& view);

// --- Editar > Líneas --------------------------------------------------------------------
// IDM_EDIT_SORTLINES_*: las líneas seleccionadas (dos o más), las columnas de una
// selección rectangular o todo el documento. Devuelve 0, o el número de línea
// (desde 1) que impidió el orden numérico; entonces no cambia nada.
sptr_t sortLines(ScintillaEditBase& view, const SortRequest& request);
// IDM_EDIT_REMOVE_ANY_DUP_LINES / IDM_EDIT_REMOVE_CONSECUTIVE_DUP_LINES
void removeDuplicateLines(ScintillaEditBase& view, bool onlyConsecutive);
// IDM_EDIT_REMOVEEMPTYLINES / IDM_EDIT_REMOVEEMPTYLINESWITHBLANK
void removeEmptyLines(ScintillaEditBase& view, bool alsoBlank);
// IDM_EDIT_BLANKLINEABOVECURRENT / IDM_EDIT_BLANKLINEBELOWCURRENT
void insertBlankLine(ScintillaEditBase& view, bool above);
// IDM_EDIT_SPLIT_LINES: parte en la línea de margen (o en el ancho de la ventana)
void splitLines(ScintillaEditBase& view);
// IDM_EDIT_JOIN_LINES
void joinLines(ScintillaEditBase& view);

// --- Editar > Operaciones con espacios ----------------------------------------------------
// Notepad_plus::doTrim: en las líneas seleccionadas o en todo el documento
void trimLines(ScintillaEditBase& view, bool leading, bool trailing);
// Notepad_plus::eol2ws: une las líneas con un espacio
void eolToSpace(ScintillaEditBase& view);
// IDM_EDIT_TRIMALL: quitar espacios sobrantes y los fines de línea
void trimAll(ScintillaEditBase& view);
// Notepad_plus::wsTabConvert
void convertTabs(ScintillaEditBase& view, TabConversion conversion);

// --- Editar > Convertir ------------------------------------------------------------------
// ScintillaEditView::convertSelectedTextTo: la selección (o cada una de varias)
void convertCase(ScintillaEditBase& view, TextCase textCase, unsigned seed = 0);

// --- Editar > Comentar ------------------------------------------------------------------------
// Símbolos de comentario del lenguaje: commentLine/commentStart/commentEnd de
// langs.xml, o las casillas 00/03/04 de un lenguaje del usuario
struct CommentSymbols
{
    std::string line;
    std::string start;
    std::string end;
    bool avoidIndent = false;       // Fortran 77 y BaanC: el comentario va en la columna 1
    bool commentEmptyLines = false; // BaanC
    bool noSpaceAfterLine = false;  // BaanC: sin espacio tras el símbolo
};

enum class CommentMode { Toggle, Comment, Uncomment }; // comment_mode de N++

// Notepad_plus::doBlockComment (Ctrl+Q, Ctrl+K, Ctrl+Mayús+K). Sin símbolo de
// línea usa los de bloque en cada línea. false si el lenguaje no tiene comentarios.
bool lineComment(ScintillaEditBase& view, const CommentSymbols& symbols, CommentMode mode);
// Notepad_plus::doStreamComment (Ctrl+Mayús+Q)
bool streamComment(ScintillaEditBase& view, const CommentSymbols& symbols);
// Notepad_plus::undoStreamComment
bool undoStreamComment(ScintillaEditBase& view, const CommentSymbols& symbols, bool tryLineComment = true);

// --- Buscar > Marcadores --------------------------------------------------------------------
bool hasBookmark(ScintillaEditBase& view, sptr_t line);
void toggleBookmark(ScintillaEditBase& view, sptr_t line);
// Notepad_plus::bookmarkNext: da la vuelta al llegar al final
void goToBookmark(ScintillaEditBase& view, bool forward);
void clearBookmarks(ScintillaEditBase& view);
std::vector<sptr_t> bookmarkedLines(ScintillaEditBase& view);
void setBookmarks(ScintillaEditBase& view, const std::vector<sptr_t>& lines);
// Texto de las líneas marcadas, con su fin de línea (copyMarkedLines)
std::string markedLinesText(ScintillaEditBase& view);
// deleteMarkedLines: las marcadas, o las que no lo están
void deleteMarkedLines(ScintillaEditBase& view, bool marked);
// pasteToMarkedLines: sustituye cada línea marcada por `text`
void replaceMarkedLines(ScintillaEditBase& view, const std::string& text);
void inverseBookmarks(ScintillaEditBase& view);

// --- Buscar > Historial de cambios -------------------------------------------------------------
// Notepad_plus::changedHistoryGoTo: siguiente o anterior bloque de líneas cambiadas
void goToChange(ScintillaEditBase& view, bool forward);
// Notepad_plus::clearChangesHistory (también vacía el deshacer, como N++)
void clearChangeHistory(ScintillaEditBase& view);

// --- Buscar > Estilo de token ---------------------------------------------------------------
// FindReplaceDlg::markAll(texto, estilo): todas las apariciones. Devuelve cuántas.
int markAllOccurrences(ScintillaEditBase& view, const std::string& text, int indicator, bool matchCase, bool wholeWord);
// IDM_SEARCH_MARKONEEXT*: solo la selección (o la palabra del cursor)
void markSelection(ScintillaEditBase& view, int indicator);
void clearIndicator(ScintillaEditBase& view, int indicator);
// Notepad_plus::goToNextIndicator / goToPreviousIndicator (dan la vuelta)
bool goToIndicator(ScintillaEditBase& view, int indicator, bool forward);
// Texto seleccionado o, si no hay, la palabra del cursor (getSelectedTextToWChar(true))
std::string selectedTextOrWord(ScintillaEditBase& view);

// --- Buscar > Llave pareja ----------------------------------------------------------------------
// IDM_SEARCH_GOTOMATCHINGBRACE / IDM_SEARCH_SELECTMATCHINGBRACES
void goToMatchingBrace(ScintillaEditBase& view, bool select);

// --- Ver > Plegado --------------------------------------------------------------------------
// ScintillaEditView::foldAll, foldLevel (también para lenguajes plegados por
// sangría, como Python) y foldCurrentPos. `expand` = desplegar.
void foldAll(ScintillaEditBase& view, bool expand);
void foldLevel(ScintillaEditBase& view, int level, bool expand);
void foldCurrent(ScintillaEditBase& view, bool expand);

// --- Editar > Editor de columnas -------------------------------------------------------------
// ColumnEditorDlg (IDM_EDIT_COLUMNMODE): cuántos sitios recibirán texto. Con selección
// rectangular o varias selecciones, cada una; si no, desde la línea del cursor hasta
// la última.
std::size_t columnInsertCount(ScintillaEditBase& view);
// Pone `texts[i]` en el sitio i (en orden de posición): sustituye cada selección, o
// inserta en la columna del cursor rellenando con espacios las líneas más cortas.
// Una sola acción de deshacer.
void columnInsert(ScintillaEditBase& view, const std::vector<std::string>& texts);

} // namespace edita
