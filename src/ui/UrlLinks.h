// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team): la detección es
// la de Notepad_plus.cpp (isUrl, scanToUrlStart, scanToUrlEnd, removeUnwantedTrailingChar…).
//
// Los enlaces que se pueden pulsar (Notepad_plus::addHotSpot): las URL de las líneas visibles
// llevan el indicador URL_INDIC, subrayadas o no, y un doble clic las abre.

#pragma once

#include <QString>

#include <utility>
#include <vector>

namespace edita {

class EditorView;

// URL_INDIC de N++ (los plugins lo conocen con ese número)
inline constexpr int kIndicatorUrl = 8;

// Las URL de `text`: inicio y longitud (en unidades de QString). Valen http://, https://,
// ftp://, file://, mailto: y los esquemas de `extraSchemes` (separados por espacios, como
// «svn:// git:// news:»), sin distinguir mayúsculas
std::vector<std::pair<qsizetype, qsizetype>> findUrls(const QString& text, const QString& extraSchemes);

struct UrlLinkOptions
{
    int mode = 2; // urlMode de N++: 0 no, 1 sin subrayar, 2 subrayados, 3 y 4 con recuadro al pasar el ratón
    QString extraSchemes;
    bool allowed = true; // en un archivo grande, solo si Rendimiento lo permite
};

// Marca (o borra) las URL de las líneas visibles de la vista
void highlightUrls(EditorView& view, const UrlLinkOptions& options);

// La URL marcada en esa posición (vacía si no hay)
QString urlAt(EditorView& view, qsizetype position);

} // namespace edita
