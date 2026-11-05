// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "syntax/ExternalLexers.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace edita {

bool ExternalLanguage::matchesExtension(std::string_view extension) const
{
    return !extension.empty()
        && (isInSpaceSeparatedList(extension, styles.userExtensions)
            || isInSpaceSeparatedList(extension, definition.extensions));
}

QStringList ExternalLexers::add(const QString& pluginName, const std::vector<std::string>& names,
                                CreateLexerFunction create, const QString& xmlPath)
{
    QStringList warnings;
    // El XML: sus <Languages> y sus <LexerStyles> (los dos son opcionales)
    LanguageRegistry languages;
    StyleSheet styles;
    QString error;
    if (!xmlPath.isEmpty()) {
        QFile file(xmlPath);
        if (!file.open(QIODevice::ReadOnly)) {
            warnings << QCoreApplication::translate("ExternalLexers", "%1: cannot read %2").arg(pluginName, xmlPath);
        } else {
            const QByteArray xml = file.readAll();
            const std::string_view text(xml.constData(), static_cast<std::size_t>(xml.size()));
            languages.loadFromString(text); // sin <Languages> no hay palabras clave: no es un error
            if (!styles.loadFromString(text, &error) && xml.contains("<LexerStyles"))
                warnings << QStringLiteral("%1: %2").arg(xmlPath, error);
        }
    }
    for (const std::string& name : names) {
        if (name.empty() || find(name))
            continue;
        if (static_cast<int>(_languages.size()) >= kMaxExternalLanguages) {
            warnings << QCoreApplication::translate("ExternalLexers",
                                                    "%1: there is no room for more plugin languages (%2 at most)")
                            .arg(pluginName)
                            .arg(kMaxExternalLanguages);
            break;
        }
        ExternalLanguage language;
        language.name = name;
        language.pluginName = pluginName;
        language.xmlPath = xmlPath;
        language.create = create;
        const auto def = std::find_if(languages.languages().begin(), languages.languages().end(),
                                      [&name](const LanguageDefinition& d) { return d.name == name; });
        if (def != languages.languages().end())
            language.definition = *def;
        language.definition.name = name;
        language.definition.type = LangType::External;
        if (const LexerStyles* ls = styles.lexer(name)) {
            language.styles = *ls;
            language.hasStyles = true;
        } else {
            language.styles.name = name;
            language.styles.description = name;
        }
        _languages.push_back(std::move(language));
    }
    return warnings;
}

const ExternalLanguage* ExternalLexers::find(std::string_view name) const
{
    const int i = indexOf(name);
    return i < 0 ? nullptr : &_languages[static_cast<std::size_t>(i)];
}

int ExternalLexers::indexOf(std::string_view name) const
{
    for (std::size_t i = 0; i < _languages.size(); ++i) {
        if (_languages[i].name == name)
            return static_cast<int>(i);
    }
    return -1;
}

std::string ExternalLexers::languageForExtension(std::string_view extension) const
{
    for (const ExternalLanguage& language : _languages) {
        if (language.matchesExtension(extension))
            return language.name;
    }
    return {};
}

void ExternalLexers::addStylesTo(StyleSheet& styles) const
{
    for (const ExternalLanguage& language : _languages) {
        if (language.hasStyles && !styles.lexer(language.name))
            styles.lexers().push_back(language.styles);
    }
}

bool ExternalLexers::saveStyles(const StyleSheet& styles, QString* error)
{
    QStringList saved;
    for (ExternalLanguage& language : _languages) {
        const LexerStyles* ls = styles.lexer(language.name);
        if (!ls || !language.hasStyles || language.xmlPath.isEmpty())
            continue;
        language.styles = *ls;
        if (saved.contains(language.xmlPath))
            continue;
        // StyleSheet::save solo cambia los LexerType que ya están en el archivo
        if (!styles.save(language.xmlPath, error))
            return false;
        saved << language.xmlPath;
    }
    return true;
}

QString findExternalLexerXml(const QString& configDir, const QString& pluginPath)
{
    const QFileInfo plugin(pluginPath);
    const QString leaf = plugin.completeBaseName() + QStringLiteral(".xml");
    for (const QString& candidate : {QDir(configDir).filePath(leaf), plugin.dir().filePath(QStringLiteral("Config/") + leaf)}) {
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return {};
}

} // namespace edita
