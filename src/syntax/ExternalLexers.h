// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Lenguajes de los plugins de lexer (ExternalLangContainer de N++). Un plugin que exporta
// las funciones de Lexilla (GetLexerCount, GetLexerName, CreateLexer) da un lenguaje por
// lexer. Sus palabras clave (<Languages>), extensiones y estilos (<LexerStyles>) están en
// plugins/Config/<plugin>.xml, como en N++ (PluginsManager::loadPluginFromPath y
// NppParameters::getExternalLexerFromXmlTree).

#pragma once

#include "syntax/LanguageRegistry.h"
#include "syntax/StyleSheet.h"

#include <QString>
#include <QStringList>

#include <string>
#include <string_view>
#include <vector>

namespace Scintilla {
class ILexer5;
}

namespace edita {

// Lexilla::CreateLexerFn
using CreateLexerFunction = Scintilla::ILexer5* (*)(const char* name);

struct ExternalLanguage
{
    std::string name;          // el de GetLexerName: nombre del lenguaje y del LexerType
    QString pluginName;        // el plugin que lo trae (para los avisos)
    QString xmlPath;           // su XML (vacío si no tiene)
    CreateLexerFunction create = nullptr;
    LanguageDefinition definition; // de <Languages>: extensiones, palabras clave, comentarios
    LexerStyles styles;        // de <LexerStyles>
    bool hasStyles = false;

    // Extensiones: las de <Language ext> y las del estilo (atributo ext del LexerType)
    bool matchesExtension(std::string_view extension) const;
};

// N++ admite 30 lenguajes externos (NB_MAX_EXTERNAL_LANG)
inline constexpr int kMaxExternalLanguages = 30;

class ExternalLexers
{
public:
    // Registra los lexers de un plugin. `names` son los de GetLexerName, en orden;
    // `xmlPath` es el XML que se encontró (vacío si no hay). Como N++, un nombre ya
    // registrado (o vacío) se ignora, y pasados 30 no se registran más.
    // Devuelve los avisos.
    QStringList add(const QString& pluginName, const std::vector<std::string>& names, CreateLexerFunction create,
                    const QString& xmlPath);

    const std::vector<ExternalLanguage>& languages() const { return _languages; }
    const ExternalLanguage* find(std::string_view name) const;
    int indexOf(std::string_view name) const; // -1 si no está
    bool empty() const { return _languages.empty(); }

    // Nombre del lenguaje para una extensión (sin el punto); vacío si ninguno
    std::string languageForExtension(std::string_view extension) const;

    // Los estilos de los lenguajes que no estén ya en `styles` (stylers.xml o el tema
    // no los traen: viven en el XML del plugin)
    void addStylesTo(StyleSheet& styles) const;
    // Guarda en el XML de cada plugin los estilos de sus lenguajes, como N++
    bool saveStyles(const StyleSheet& styles, QString* error = nullptr);

private:
    std::vector<ExternalLanguage> _languages;
};

// Dónde está el XML de un plugin de lexer: <carpeta de configuración>/<módulo>.xml (la de
// NPPM_GETPLUGINSCONFIGDIR) o, si el paquete lo trae, <carpeta del plugin>/Config/<módulo>.xml.
// Vacío si no está en ninguna.
QString findExternalLexerXml(const QString& configDir, const QString& pluginPath);

} // namespace edita
