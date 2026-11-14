// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Herramientas de CSV (fase 99), sin interfaz: el separador, los campos (con comillas, como en
// RFC 4180: "" es una comilla dentro de un campo entre comillas), alinear y compactar las
// columnas, y ordenar por una columna. Notepad++ no tiene nada para CSV.

#pragma once

#include <QChar>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <vector>

namespace edita::csv {

// El separador de un texto: el de , ; tabulador | que da el mismo número de campos (más de
// uno) en más de sus primeras líneas. Sin ninguno, la coma
QChar detectDelimiter(const QString& text);

// Un campo de una línea: dónde empieza y cuánto mide (con sus comillas, sin el separador)
struct Span
{
    qsizetype start = 0;
    qsizetype length = 0;
    bool operator==(const Span&) const = default;
};
// Los campos de una línea. Unas comillas que no se cierran llegan hasta el final de la línea
std::vector<Span> splitLine(QStringView line, QChar delimiter);
// La columna (desde 0) de una posición de la línea
int columnAt(QStringView line, QChar delimiter, qsizetype pos);

// Los registros del texto, con sus campos tal cual (con comillas); un campo entre comillas
// puede tener saltos de línea
std::vector<QStringList> parse(const QString& text, QChar delimiter);

// El salto de línea del texto (\r\n, \r o \n)
QString lineEnding(const QString& text);

// Alinear: cada campo, con espacios detrás hasta el ancho de su columna (la última, sin
// espacios). Compactar: sin los espacios de alrededor de cada campo (dentro de unas comillas,
// no se tocan). Un registro con saltos de línea dentro de un campo no se alinea: el texto
// sale igual
QString align(const QString& text, QChar delimiter);
QString shrink(const QString& text, QChar delimiter);

// ¿La primera fila es un encabezado? Si todos sus campos son texto (no números) y, en alguna
// columna donde ella tiene texto, las demás filas tienen números
bool hasHeader(const std::vector<QStringList>& records);

// Ordenar las filas por una columna: de números, si todos sus valores lo son; si no, como texto
// (sin distinguir mayúsculas). Estable; el encabezado se queda arriba
QString sortByColumn(const QString& text, QChar delimiter, int column, bool descending);

} // namespace edita::csv
