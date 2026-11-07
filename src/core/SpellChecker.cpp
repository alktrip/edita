// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/SpellChecker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#ifdef EDITA_HUNSPELL
#include <hunspell.h>
#include <iconv.h>
#endif

#include <algorithm>

namespace edita {

namespace {

bool isWordLetter(QChar c)
{
    return c.isLetter() || c.category() == QChar::Mark_NonSpacing || c.category() == QChar::Mark_SpacingCombining;
}

bool isApostrophe(QChar c)
{
    return c == QLatin1Char('\'') || c == QChar(0x2019);
}

} // namespace

std::vector<WordSpan> wordsToCheck(const QString& text, bool skipAllCaps)
{
    std::vector<WordSpan> words;
    const qsizetype n = text.size();
    qsizetype i = 0;
    while (i < n) {
        // Un «token»: lo que no es espacio. Si parece una URL, un correo o una ruta, entero fuera
        if (text.at(i).isSpace()) {
            ++i;
            continue;
        }
        qsizetype end = i;
        while (end < n && !text.at(end).isSpace())
            ++end;
        const QStringView token = QStringView(text).mid(i, end - i);
        if (token.contains(QLatin1String("://")) || token.contains(QLatin1Char('@')) || token.count(QLatin1Char('/')) >= 2
            || token.startsWith(QLatin1Char('/')) || token.startsWith(QLatin1String("~/"))) {
            i = end;
            continue;
        }
        // Dentro del token, las palabras
        qsizetype p = i;
        while (p < end) {
            if (!isWordLetter(text.at(p)) && !text.at(p).isDigit() && text.at(p) != QLatin1Char('_')) {
                ++p;
                continue;
            }
            const qsizetype start = p;
            bool digit = false;
            bool underscore = false;
            while (p < end) {
                const QChar c = text.at(p);
                if (isWordLetter(c)) {
                    ++p;
                } else if (c.isDigit()) {
                    digit = true;
                    ++p;
                } else if (c == QLatin1Char('_')) {
                    underscore = true;
                    ++p;
                } else if (isApostrophe(c) && p + 1 < end && isWordLetter(text.at(p + 1)) && p > start) {
                    ++p; // «don't», «l'eau»
                } else {
                    break;
                }
            }
            const QStringView word = QStringView(text).mid(start, p - start);
            if (digit || underscore || word.size() < 2)
                continue;
            bool upperAfterLower = false;
            bool allUpper = true;
            for (qsizetype k = 0; k < word.size(); ++k) {
                if (word.at(k).isLower())
                    allUpper = false;
                if (k > 0 && word.at(k).isUpper() && word.at(k - 1).isLower())
                    upperAfterLower = true;
            }
            if (upperAfterLower || (skipAllCaps && allUpper))
                continue;
            words.push_back({start, word.size()});
        }
        i = end;
    }
    return words;
}

// --- Diccionarios -----------------------------------------------------------------------

struct SpellChecker::Dictionary
{
    QString code;
#ifdef EDITA_HUNSPELL
    Hunhandle* handle = nullptr;
    bool utf8 = true;
    iconv_t toDic = reinterpret_cast<iconv_t>(-1);
    iconv_t fromDic = reinterpret_cast<iconv_t>(-1);

    ~Dictionary()
    {
        if (toDic != reinterpret_cast<iconv_t>(-1))
            iconv_close(toDic);
        if (fromDic != reinterpret_cast<iconv_t>(-1))
            iconv_close(fromDic);
        if (handle)
            Hunspell_destroy(handle);
    }

    static QByteArray convert(iconv_t cd, const QByteArray& in, bool* ok)
    {
        *ok = false;
        if (cd == reinterpret_cast<iconv_t>(-1))
            return {};
        iconv(cd, nullptr, nullptr, nullptr, nullptr);
        QByteArray out(in.size() * 4 + 8, '\0');
        char* src = const_cast<char*>(in.constData());
        std::size_t srcLeft = static_cast<std::size_t>(in.size());
        char* dst = out.data();
        std::size_t dstLeft = static_cast<std::size_t>(out.size());
        if (iconv(cd, &src, &srcLeft, &dst, &dstLeft) == static_cast<std::size_t>(-1))
            return {}; // una letra que el diccionario no tiene: no es suya
        out.truncate(out.size() - static_cast<qsizetype>(dstLeft));
        *ok = true;
        return out;
    }

    QByteArray encode(const QString& word, bool* ok) const
    {
        if (utf8) {
            *ok = true;
            return word.toUtf8();
        }
        return convert(toDic, word.toUtf8(), ok);
    }

    QString decode(const char* text) const
    {
        if (utf8)
            return QString::fromUtf8(text);
        bool ok = false;
        const QByteArray out = convert(fromDic, QByteArray(text), &ok);
        return ok ? QString::fromUtf8(out) : QString();
    }
#endif
};

SpellChecker::SpellChecker() = default;
SpellChecker::~SpellChecker() = default;

bool SpellChecker::available()
{
#ifdef EDITA_HUNSPELL
    return true;
#else
    return false;
#endif
}

QStringList SpellChecker::dictionaryDirs(const QStringList& extraDirs)
{
    QStringList dirs = extraDirs;
    for (const QString& dir : qEnvironmentVariable("DICPATH").split(QLatin1Char(':'), Qt::SkipEmptyParts))
        dirs << dir;
    dirs << QDir::home().filePath(QStringLiteral(".local/share/hunspell")) << QStringLiteral("/usr/share/hunspell")
         << QStringLiteral("/usr/share/myspell") << QStringLiteral("/usr/share/myspell/dicts")
         << QStringLiteral("/usr/local/share/hunspell");
    return dirs;
}

QStringList SpellChecker::availableDictionaries(const QStringList& extraDirs)
{
    QStringList codes;
    for (const QString& dir : dictionaryDirs(extraDirs)) {
        for (const QFileInfo& dic : QDir(dir).entryInfoList({QStringLiteral("*.dic")}, QDir::Files | QDir::Readable)) {
            const QString code = dic.completeBaseName();
            // hyph_*.dic son de guiones, no de palabras
            if (code.startsWith(QLatin1String("hyph_")) || codes.contains(code)
                || !QFileInfo::exists(dic.absolutePath() + QLatin1Char('/') + code + QStringLiteral(".aff")))
                continue;
            codes << code;
        }
    }
    std::sort(codes.begin(), codes.end());
    return codes;
}

QStringList SpellChecker::defaultDictionaries(const QString& localeName, const QStringList& available)
{
    QStringList chosen;
    auto pick = [&](const QString& name) {
        const QString lang = name.section(QLatin1Char('_'), 0, 0);
        // El exacto; si no, uno del mismo idioma; si no, el idioma solo
        QString found = available.contains(name) ? name : QString();
        for (const QString& code : available) {
            if (found.isEmpty() && code.startsWith(lang + QLatin1Char('_')))
                found = code;
        }
        if (found.isEmpty() && available.contains(lang))
            found = lang;
        if (!found.isEmpty() && !chosen.contains(found))
            chosen << found;
    };
    pick(localeName.section(QLatin1Char('.'), 0, 0));
    pick(QStringLiteral("en_US"));
    return chosen;
}

int SpellChecker::load(const QStringList& codes, const QStringList& extraDirs)
{
    _dictionaries.clear();
#ifdef EDITA_HUNSPELL
    const QStringList dirs = dictionaryDirs(extraDirs);
    for (const QString& code : codes) {
        for (const QString& dir : dirs) {
            const QString base = QDir(dir).filePath(code);
            if (!QFileInfo::exists(base + QStringLiteral(".dic")) || !QFileInfo::exists(base + QStringLiteral(".aff")))
                continue;
            auto dictionary = std::make_unique<Dictionary>();
            dictionary->code = code;
            dictionary->handle = Hunspell_create(QFile::encodeName(base + QStringLiteral(".aff")).constData(),
                                                 QFile::encodeName(base + QStringLiteral(".dic")).constData());
            if (!dictionary->handle)
                break;
            const QByteArray encoding = QByteArray(Hunspell_get_dic_encoding(dictionary->handle)).toUpper();
            dictionary->utf8 = encoding.isEmpty() || encoding == "UTF-8" || encoding == "UTF8";
            if (!dictionary->utf8) {
                dictionary->toDic = iconv_open(encoding.constData(), "UTF-8");
                dictionary->fromDic = iconv_open("UTF-8", encoding.constData());
            }
            _dictionaries.push_back(std::move(dictionary));
            break;
        }
    }
#else
    Q_UNUSED(codes)
    Q_UNUSED(extraDirs)
#endif
    return static_cast<int>(_dictionaries.size());
}

QStringList SpellChecker::loaded() const
{
    QStringList codes;
    for (const auto& dictionary : _dictionaries)
        codes << dictionary->code;
    return codes;
}

bool SpellChecker::isLoaded() const
{
    return !_dictionaries.empty();
}

void SpellChecker::setPersonalDictionary(const QString& path)
{
    _personalPath = path;
    _personal.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString word = in.readLine().trimmed();
        if (!word.isEmpty())
            _personal.insert(word);
    }
}

bool SpellChecker::addToPersonal(const QString& word)
{
    if (word.isEmpty() || _personal.contains(word))
        return true;
    _personal.insert(word);
    if (_personalPath.isEmpty())
        return true;
    QDir().mkpath(QFileInfo(_personalPath).absolutePath());
    QFile file(_personalPath);
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return false;
    file.write((word + QLatin1Char('\n')).toUtf8());
    return true;
}

void SpellChecker::ignore(const QString& word)
{
    _ignored.insert(word);
}

bool SpellChecker::spell(const QString& word) const
{
    if (_personal.contains(word) || _ignored.contains(word))
        return true;
    // «Hola» al principio de una frase: también la de minúscula del diccionario personal
    if (!word.isEmpty() && word.at(0).isUpper()) {
        const QString lower = word.at(0).toLower() + word.mid(1);
        if (_personal.contains(lower) || _ignored.contains(lower))
            return true;
    }
#ifdef EDITA_HUNSPELL
    for (const auto& dictionary : _dictionaries) {
        bool ok = false;
        const QByteArray encoded = dictionary->encode(word, &ok);
        if (ok && Hunspell_spell(dictionary->handle, encoded.constData()) != 0)
            return true;
    }
    return _dictionaries.empty(); // sin diccionarios, nada está mal
#else
    return true;
#endif
}

QStringList SpellChecker::suggest(const QString& word, int max) const
{
    QStringList suggestions;
#ifdef EDITA_HUNSPELL
    for (const auto& dictionary : _dictionaries) {
        bool ok = false;
        const QByteArray encoded = dictionary->encode(word, &ok);
        if (!ok)
            continue;
        char** list = nullptr;
        const int count = Hunspell_suggest(dictionary->handle, &list, encoded.constData());
        for (int i = 0; i < count; ++i) {
            const QString suggestion = dictionary->decode(list[i]);
            if (!suggestion.isEmpty() && !suggestions.contains(suggestion))
                suggestions << suggestion;
        }
        if (list)
            Hunspell_free_list(dictionary->handle, &list, count);
    }
#else
    Q_UNUSED(word)
#endif
    return suggestions.mid(0, max);
}

} // namespace edita
