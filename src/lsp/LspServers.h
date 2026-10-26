// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Qué servidor de lenguaje usa cada lenguaje (clangd, pyright, rust-analyzer…), en qué
// carpeta empieza el proyecto y cómo se pasa de las posiciones de LSP (línea y carácter
// en UTF-16 o UTF-8) a los bytes de Scintilla.

#pragma once

#include "core/LangType.h"

#include <QByteArrayView>
#include <QMap>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace edita::lsp {

struct ServerSpec
{
    QString id;               // clave de la preferencia («clangd», «python»…)
    QString languages;        // para mostrar: «C, C++, Objective-C»
    QStringList languageIds;  // los de LSP que atiende
    QStringList candidates;   // órdenes, en orden de preferencia (la primera que esté instalada)
    QStringList rootMarkers;  // archivos o carpetas que marcan la raíz del proyecto
};

const std::vector<ServerSpec>& knownServers();
const ServerSpec* serverForLanguage(const QString& languageId);

// El languageId de LSP de un documento («cpp», «python», «typescriptreact»…); vacío si no hay
QString languageIdFor(LangType lang, const QString& userLanguage, const QString& path);

struct ResolvedServer
{
    QString specId;
    QString program;     // la ruta completa
    QStringList arguments;
    QString command;     // como se escribió
};

// La orden del servidor: la de la preferencia (`overrides[spec.id]`) o la primera de
// las candidatas que esté en el PATH. «off» desactiva el servidor
std::optional<ResolvedServer> resolveServer(const ServerSpec& spec, const QMap<QString, QString>& overrides);
// «id=orden» por línea (la preferencia EditaLspServers)
QMap<QString, QString> parseOverrides(const QString& text);
QString formatOverrides(const QMap<QString, QString>& overrides);

// La raíz del proyecto: la carpeta de trabajo que contiene al archivo; si no, la primera
// carpeta hacia arriba con un marcador (.git, Cargo.toml…); si no, la del archivo
QString findRoot(const QString& filePath, const QStringList& workspaceRoots, const QStringList& markers);

enum class PositionEncoding { Utf16, Utf8 };
// Carácter de LSP ↔ byte dentro de la línea (UTF-8)
int lspCharacter(QByteArrayView lineUtf8, int byteColumn, PositionEncoding encoding);
int byteColumn(QByteArrayView lineUtf8, int character, PositionEncoding encoding);

// Un fragmento de LSP («push_back(${1:valor})$0») como texto: los marcadores, con su
// texto; lo seleccionado al final, el primero (o donde va $0)
struct ExpandedSnippet
{
    QString text;
    int selectionStart = 0; // en unidades de QString
    int selectionEnd = 0;
};
ExpandedSnippet expandSnippet(const QString& snippet);

QString uriFromPath(const QString& path);
QString pathFromUri(const QString& uri);

} // namespace edita::lsp
