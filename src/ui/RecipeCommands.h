// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Las recetas de plugins desde la línea de órdenes, sin la interfaz: para la CI del
// catálogo (edita-plugins) y para quien escribe una receta.
//
//   edita --build-recipe receta.json --output carpeta
//       descarga el código (y sus fuentes adicionales), lo comprueba, lo compila y deja
//       <carpeta>/<plugin>-<versión>.zip y su .sha256
//   edita --check-plugin paquete.zip --recipe receta.json
//       instala el paquete en una configuración temporal, abre Edita (sin ventana visible),
//       carga el plugin y ejecuta la prueba de la receta ("check")

#pragma once

#include <QString>
#include <QStringList>
#include <QTextStream>

namespace edita {

// Si los argumentos piden una de las dos órdenes, la ejecuta y devuelve su código de
// salida; si no, -1
int runRecipeCommandLine(const QStringList& arguments, QTextStream& out);

int buildRecipeCommand(const QString& recipePath, const QString& outputDir, QTextStream& out);
int checkPluginCommand(const QString& packagePath, const QString& recipePath, QTextStream& out);

} // namespace edita
