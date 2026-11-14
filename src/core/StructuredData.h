// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Herramientas de JSON y YAML (fase 100), sin interfaz. Un analizador de cada uno que da un
// árbol en el orden del texto (QJsonDocument ordena las claves y pasa los números a double: no
// sirve para formatear sin cambiar el documento), con los errores en línea y columna; la ruta
// del nodo en una posición; formatear y compactar JSON; y convertir entre JSON y YAML.
//
// YAML: lo que se usa en archivos de configuración (Docker Compose, GitHub Actions, Kubernetes,
// OpenAPI): mapas y listas en bloque, colecciones [ ] y { }, cadenas simples, entre comillas y
// en bloque (| y >, con + y -), comentarios, anclas y alias (&a, *a), y etiquetas (!!str), que
// se ignoran. Un solo documento.

#pragma once

#include <QString>

#include <optional>
#include <utility>
#include <vector>

namespace edita::data {

struct Member;

struct Node
{
    enum class Kind { Null, Bool, Number, String, Array, Object };
    Kind kind = Kind::Null;
    QString scalar; // Null, Bool y Number: el texto; String: el valor ya sin escapes
    QString raw;    // String leído de un JSON: tal cual, con sus comillas y escapes
    std::vector<Node> items;                       // Array
    std::vector<Member> members; // Object, en orden
    qsizetype start = 0; // en el texto: dónde empieza y termina (de la clave, en un miembro)
    qsizetype end = 0;
};

// Un miembro de un objeto: la clave y su valor (un par, con el tipo completo)
struct Member
{
    QString first;
    Node second;
};

struct Error
{
    QString message;
    int line = 0;   // desde 1
    int column = 0; // desde 1
    qsizetype offset = 0;
};

std::optional<Node> parseJson(const QString& text, Error* error = nullptr);
std::optional<Node> parseYaml(const QString& text, Error* error = nullptr);

// JSON con sangría de `indent` espacios (o tabuladores, con useTabs); con indent 0, en una línea
QString formatJson(const Node& node, int indent, bool useTabs = false);
QString toYaml(const Node& node);

// La ruta del nodo más interno que contiene la posición: $.servicios.web.puertos[0]
QString pathAt(const Node& root, qsizetype offset);

} // namespace edita::data
