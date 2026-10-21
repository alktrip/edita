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

#include <string>
#include <vector>

namespace edita {

// Editor de columnas de N++ (ColumnEditorDlg): números que se insertan en columna
enum class NumberBase { Decimal, Hex, HexUpper, Octal, Binary }; // NumBase (BASE_10, BASE_16…)
enum class NumberLeading { None, Zeros, Spaces };                // leadingChoice

struct ColumnNumbers
{
    long long initial = 1;
    long long increase = 1;
    int repeat = 1; // cuántas líneas seguidas con el mismo número
    NumberBase base = NumberBase::Decimal;
    NumberLeading leading = NumberLeading::None;
};

// Los `count` números ya escritos: initial, initial+increase…, cada uno `repeat`
// veces; con relleno (ceros o espacios) todos tienen el ancho del más largo
// (getNbDigits y variedFormatNumber2String de N++)
std::vector<std::string> columnNumbers(const ColumnNumbers& params, std::size_t count);

// Un número en esa base ("ff", "FF", "17", "1010"); negativo con "-"
std::string formatNumber(long long value, NumberBase base);
// Lo contrario, para leer los campos del diálogo en la base elegida; false si no es válido
bool parseNumber(const std::string& text, NumberBase base, long long& value);

} // namespace edita
