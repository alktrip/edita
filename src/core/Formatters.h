// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Formatear con herramientas externas (no existe en N++): clang-format, prettier, black/ruff,
// rustfmt, gofmt, shfmt, xmllint… Cada una recibe el documento por la entrada estándar y
// devuelve el resultado por la salida, en la carpeta del archivo (para que encuentre su
// configuración: .clang-format, pyproject.toml, .editorconfig…).

#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace edita {

struct FormatterSpec
{
    QString id;              // clave de la preferencia («clang-format», «python»…)
    QString languages;       // para mostrar
    QStringList languageIds; // los de lsp::languageIdFor que atiende
    QStringList candidates;  // órdenes, en orden de preferencia (la primera instalada)
    // Variables de entorno («XMLLINT_INDENT=$(INDENT)»)
    QStringList environment;
};

// Las conocidas. En las órdenes: $(FILE_PATH), $(FILE_NAME), $(TAB_SIZE) y $(INDENT) (un
// tabulador o tantos espacios como el ancho de tabulación)
const std::vector<FormatterSpec>& knownFormatters();
const FormatterSpec* formatterForLanguage(const QString& languageId);

struct ResolvedFormatter
{
    QString specId;
    QString program;     // la ruta completa
    QStringList arguments;
    QStringList environment;
    QString command;     // como se escribió
    bool supportsLines = false; // clang-format: --lines=desde:hasta (formatear la selección)
};

// La primera orden instalada; con `overrides` (id → orden, o «off»), esa
std::optional<ResolvedFormatter> resolveFormatter(const FormatterSpec& spec, const QMap<QString, QString>& overrides);

struct FormatRequest
{
    QString filePath;   // vacío en un documento sin guardar
    int tabSize = 4;
    bool useTabs = false;
    int firstLine = 0;  // con supportsLines: las líneas (desde 1) que se formatean; 0, todas
    int lastLine = 0;
    int timeoutMs = 10000;
};

struct FormatResult
{
    bool ok = false;
    QByteArray output;
    QString error;      // lo que dijo la herramienta (o por qué no se pudo ejecutar)
};

FormatResult runFormatter(const ResolvedFormatter& formatter, const QByteArray& input, const FormatRequest& request);

} // namespace edita
