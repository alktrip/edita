// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Editar › Sobre la selección (el submenú «On Selection» de N++): abrir la ruta seleccionada,
// tapar la selección y buscarla en internet

#pragma once

#include <QString>
#include <QUrl>

#include <string_view>
#include <utility>

namespace edita {

// El buscador de «Buscar en internet» (SearchEngineChoice de N++, con sus mismos números en
// <GUIConfig name="searchEngine" searchEngineChoice="2" searchEngineCustom="">). Bing ya no
// está en N++: se cambia por DuckDuckGo
enum class SearchEngine { Custom = 0, DuckDuckGo = 1, Google = 2, Bing = 3, Yahoo = 4, StackOverflow = 5 };

// La dirección con $(CURRENT_WORD) (la de N++ para cada buscador). La personalizada, sin
// espacios, y si no empieza por http:// o https://, la de Google
QString searchEngineTemplate(SearchEngine engine, const QString& custom);

// La búsqueda del texto: $(CURRENT_WORD) cambia por el texto codificado para la URL
QUrl searchOnInternetUrl(SearchEngine engine, const QString& custom, const QString& text);

// La ruta alrededor de la columna `column` de `line` cuando no hay selección
// (NPPM_GETFILENAMEATCURSOR): empieza tras un espacio, tabulador, [, (, " o <, y acaba
// antes de un espacio, tabulador, :, (, ), [, ], <, >, " o el fin de línea
QString fileNameAtColumn(const QString& line, int column);

// La ruta de la selección: con las variables de entorno ($HOME, ${HOME}) y «~» expandidas
// (ExpandEnvironmentStrings en N++), file:// convertida en ruta, y si no existe y es relativa,
// junto a `currentDir` (la carpeta del documento)
QString resolveSelectedPath(const QString& text, const QString& currentDir);

// Tapar la selección (IDM_EDIT_REDACT_SELECTION): cada carácter cambia por █ (o ● con
// `bullet`), salvo los fines de línea
QString redactText(const QString& text, bool bullet);

// Ctrl + doble clic (SCN_DOUBLECLICK de N++ con los delimitadores de Preferencias ›
// Delimitadores): las posiciones del delimitador de apertura y del de cierre alrededor de
// `click` en `text` (una línea o el documento), o {-1, -1}. Con los dos iguales (comillas),
// los más cercanos, sin contar \" ; si no, la pareja más interior (para «f(a, g(b))»)
std::pair<qsizetype, qsizetype> delimiterRange(std::string_view text, std::size_t click, char left, char right);

// Buscar › Buscar caracteres en un rango (FindCharsInRangeDlg de N++): el siguiente carácter
// (código Unicode) entre `low` y `high` desde `from` (en unidades de QString), hacia delante o
// hacia atrás, y con `wrap`, dando la vuelta. {inicio, longitud} o {-1, 0}
std::pair<qsizetype, qsizetype> findCharInRange(const QString& text, qsizetype from, char32_t low, char32_t high, bool forward,
                                                bool wrap);

} // namespace edita
