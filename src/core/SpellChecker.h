// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Corrector ortográfico (no existe en N++; allí es el plugin DSpellCheck): Hunspell con los
// diccionarios del sistema (/usr/share/hunspell…), varios a la vez, más un diccionario personal.

#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace edita {

// Una palabra del texto: inicio y longitud en unidades de QString
struct WordSpan
{
    qsizetype start = 0;
    qsizetype length = 0;
    bool operator==(const WordSpan&) const = default;
};

// Las palabras que vale la pena revisar: letras (con acentos), con apóstrofos dentro («don't»,
// «l'eau»); sin las que llevan dígitos, ni las de identificadores de código (camelCase,
// snake_case: con «_» o con una mayúscula después de una minúscula), ni las de una letra, ni las
// de URL, correos o rutas. Con `skipAllCaps`, tampoco las siglas (TODO, HTML)
std::vector<WordSpan> wordsToCheck(const QString& text, bool skipAllCaps = true);

class SpellChecker
{
public:
    SpellChecker();
    ~SpellChecker();
    SpellChecker(const SpellChecker&) = delete;
    SpellChecker& operator=(const SpellChecker&) = delete;

    // ¿Se compiló con Hunspell?
    static bool available();
    // Las carpetas de diccionarios: $DICPATH, ~/.local/share/hunspell, /usr/share/hunspell,
    // /usr/share/myspell(/dicts) y `extraDirs`
    static QStringList dictionaryDirs(const QStringList& extraDirs = {});
    // Los diccionarios instalados («es_MX», «en_US»…: un .dic con su .aff)
    static QStringList availableDictionaries(const QStringList& extraDirs = {});
    // Los del idioma del sistema y el inglés, de los que hay («es_MX» → es_MX o es_* o es)
    static QStringList defaultDictionaries(const QString& localeName, const QStringList& available);

    // Carga esos diccionarios (los que no encuentra los ignora). Devuelve cuántos cargó
    int load(const QStringList& codes, const QStringList& extraDirs = {});
    QStringList loaded() const;
    bool isLoaded() const;

    // El diccionario personal: un archivo con una palabra por línea
    void setPersonalDictionary(const QString& path);
    // Añadirla (y guardarla) o pasarla por alto en esta sesión
    bool addToPersonal(const QString& word);
    void ignore(const QString& word);

    // ¿Está bien escrita en alguno de los diccionarios (o en el personal, o pasada por alto)?
    bool spell(const QString& word) const;
    // Las sugerencias de todos, sin repetir (como mucho `max`)
    QStringList suggest(const QString& word, int max = 8) const;

private:
    struct Dictionary;
    std::vector<std::unique_ptr<Dictionary>> _dictionaries;
    QString _personalPath;
    QSet<QString> _personal;
    QSet<QString> _ignored;
};

} // namespace edita
