// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Las fuentes del escritorio cuando Qt no las entiende. En KDE, el tema de Qt lee las
// fuentes de ~/.config/kdeglobals («Ubuntu,10,-1,5,400,0,…»), guardadas con el formato
// del Qt del sistema. Un Qt más antiguo (el 6.8 del AppImage) no entiende el formato de
// uno más nuevo (19 campos en Qt 6.12): avisa con «QFont::fromString: Invalid
// description» y usa una fuente genérica. Aquí se leen esas fuentes de forma tolerante
// (los campos que importan: familia, tamaño, grosor, cursiva) y se aplican.

#pragma once

#include <QFont>
#include <QString>
#include <QStringList>

#include <optional>

namespace edita::desktopfonts {

// Antes de crear QApplication: sin los avisos «QFont::fromString: Invalid description»
// (el resto de mensajes, igual)
void installWarningFilter();

// Después de crear QApplication: la fuente general del escritorio, si Qt no pudo leerla
void apply();

// La monoespaciada del escritorio (la de kdeglobals si Qt no pudo leerla)
QFont fixedFont();

// Para las pruebas
std::optional<QFont> parseFontDescription(const QString& description);
// El valor de [General] key en el primer kdeglobals que lo tenga
QString kdeglobalsValue(const QString& key, const QStringList& files);
QStringList kdeglobalsFiles();

} // namespace edita::desktopfonts
