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

#include <QString>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace edita {

// Los ordenadores de N++ (MISC/Common/Sorters.h y SortLocale), uno por orden del
// menú Editar > Líneas
enum class SortKind
{
    Lexicographic,                // LexicographicSorter
    LexicographicCaseInsensitive, // LexicographicCaseInsensitiveSorter
    Locale,                       // SortLocale: según el idioma del sistema (QCollator)
    Integer,                      // IntegerSorter: números dentro del texto ("a10" va después de "a9")
    DecimalComma,                 // DecimalCommaSorter: "1,5"
    DecimalDot,                   // DecimalDotSorter: "1.5"
    Length,                       // LineLengthSorter
    Reverse,                      // ReverseSorter
    Random,                       // RandomSorter
};

struct SortRequest
{
    SortKind kind = SortKind::Lexicographic;
    bool descending = false;
    unsigned seed = 0; // Random: semilla (N++ usa la hora)
};

// ISorter::sort sobre líneas en UTF-8 sin fin de línea. Si `keys` no está vacío
// (selección rectangular: el texto de las columnas elegidas) se ordena por la
// clave de cada línea. En los órdenes decimales, las líneas vacías van al
// principio (al final si es descendente), como en N++.
// Devuelve -1, o el índice de la primera línea que no es un número (en ese caso
// las líneas no se tocan; N++ avisa con "SortingError").
std::ptrdiff_t sortLines(std::vector<std::string>& lines, const std::vector<std::string>& keys, const SortRequest& request);

// IntegerSorter: compara trozos de texto y números (con signo y de cualquier
// longitud). < 0, 0 o > 0.
int compareWithIntegers(std::string_view a, std::string_view b);

// ScintillaEditView::removeAnyDuplicateLines (vecRemoveDuplicates): deja la
// primera aparición de cada línea. true si quitó alguna.
bool removeAnyDuplicateLines(std::vector<std::string>& lines);
// Notepad_plus::removeDuplicateLines: solo las repetidas seguidas
bool removeConsecutiveDuplicateLines(std::vector<std::string>& lines);
// Notepad_plus::removeEmptyLine: líneas vacías, o también las que solo tienen
// espacios y tabulaciones
bool removeEmptyLines(std::vector<std::string>& lines, bool alsoBlank);

// TextCase de N++ (ScintillaEditView.h), en el orden del menú Convertir
enum class TextCase
{
    Upper,
    Lower,
    ProperForce,   // "Tipo Título": primera letra de cada palabra en mayúscula, el resto en minúscula
    ProperBlend,   // igual, pero sin tocar las demás letras
    SentenceForce, // "Tipo oración"
    SentenceBlend,
    Invert,
    Random,
};

// ScintillaEditView::changeCase (sobre UTF-16, como N++). `seed` solo para Random.
QString changeCase(const QString& text, TextCase textCase, unsigned seed = 0);

// Notepad_plus::wsTabConvert para una línea (sin fin de línea)
enum class TabConversion
{
    TabsToSpaces,  // tab2Space
    SpacesToTabsLeading, // space2TabLeading: solo la sangría
    SpacesToTabsAll,     // space2TabAll
};

// Devuelve la línea convertida. `caretOffset` (bytes desde el inicio de la línea),
// si no es nulo, se ajusta para que el cursor quede junto al mismo carácter.
std::string convertTabs(std::string_view line, int tabWidth, TabConversion conversion, std::ptrdiff_t* caretOffset = nullptr);

} // namespace edita
