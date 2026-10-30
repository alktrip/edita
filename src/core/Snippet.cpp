// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/Snippet.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

#include <algorithm>
#include <map>

namespace edita::snippet {

namespace {

class Parser
{
public:
    Parser(const QString& source, const Variables& variables)
        : _s(source)
        , _variables(variables)
    {
    }

    Parsed run()
    {
        Parsed result;
        qsizetype i = 0;
        result.text = parse(i, QChar());
        for (auto& [index, ranges] : _stops) {
            if (index != 0)
                result.tabstops.push_back({index, ranges});
        }
        // El final: $0 si está; si no, el final del texto
        const auto end = _stops.find(0);
        const int size = static_cast<int>(result.text.size());
        result.tabstops.push_back({0, end != _stops.end() ? end->second : std::vector<std::pair<int, int>>{{size, size}}});
        return result;
    }

private:
    static bool isDigit(QChar c) { return c >= QLatin1Char('0') && c <= QLatin1Char('9'); }
    static bool isNameChar(QChar c) { return c.isLetterOrNumber() || c == QLatin1Char('_'); }

    int number(qsizetype& i) const
    {
        int n = 0;
        while (i < _s.size() && isDigit(_s[i]))
            n = n * 10 + _s[i++].digitValue();
        return n;
    }

    QString name(qsizetype& i) const
    {
        const qsizetype start = i;
        while (i < _s.size() && isNameChar(_s[i]))
            ++i;
        return _s.mid(start, i - start);
    }

    // Lee hasta `stop` (sin consumirlo); `base` es dónde empieza lo leído en el texto final
    QString parse(qsizetype& i, QChar stop, int base = 0)
    {
        QString out;
        while (i < _s.size()) {
            const QChar c = _s[i];
            if (!stop.isNull() && c == stop)
                return out;
            if (c == QLatin1Char('\\') && i + 1 < _s.size() && QStringLiteral("$}\\,|").contains(_s[i + 1])) {
                out += _s[i + 1];
                i += 2;
                continue;
            }
            if (c != QLatin1Char('$') || i + 1 >= _s.size()) {
                out += c;
                ++i;
                continue;
            }
            const QChar next = _s[i + 1];
            if (isDigit(next)) { // $1: vacío, o el texto que ya tenga ese marcador
                ++i;
                const int n = number(i);
                const int start = base + static_cast<int>(out.size());
                out += _texts.value(n);
                _stops[n].emplace_back(start, base + static_cast<int>(out.size()));
                continue;
            }
            if (isNameChar(next)) { // $VARIABLE
                ++i;
                out += variable(name(i), {});
                continue;
            }
            if (next != QLatin1Char('{')) {
                out += c;
                ++i;
                continue;
            }
            i += 2; // ${
            const int start = base + static_cast<int>(out.size());
            if (i < _s.size() && isDigit(_s[i])) {
                const int n = number(i);
                QString text;
                if (i < _s.size() && _s[i] == QLatin1Char(':')) { // ${1:texto}, que puede tener otros dentro
                    ++i;
                    text = parse(i, QLatin1Char('}'), start);
                } else if (i < _s.size() && _s[i] == QLatin1Char('|')) { // ${1|uno,dos|}: el primero
                    ++i;
                    while (i < _s.size() && _s[i] != QLatin1Char(',') && _s[i] != QLatin1Char('|'))
                        text += _s[i++];
                    while (i < _s.size() && _s[i] != QLatin1Char('}'))
                        ++i;
                } else {
                    text = _texts.value(n);
                }
                if (i < _s.size() && _s[i] == QLatin1Char('}'))
                    ++i;
                if (!_texts.contains(n) && !text.isEmpty())
                    _texts.insert(n, text);
                out += text;
                _stops[n].emplace_back(start, base + static_cast<int>(out.size()));
                continue;
            }
            // ${VARIABLE}, ${VARIABLE:por defecto} o ${VARIABLE/regex/formato/} (sin transformar)
            const QString var = name(i);
            QString fallback;
            if (i < _s.size() && _s[i] == QLatin1Char(':')) {
                ++i;
                fallback = parse(i, QLatin1Char('}'), start);
            } else {
                while (i < _s.size() && _s[i] != QLatin1Char('}'))
                    ++i;
            }
            if (i < _s.size() && _s[i] == QLatin1Char('}'))
                ++i;
            out += variable(var, fallback);
        }
        return out;
    }

    QString variable(const QString& name, const QString& fallback) const
    {
        if (_variables) {
            if (const auto value = _variables(name))
                return *value;
        }
        return fallback;
    }

    const QString& _s;
    const Variables& _variables;
    std::map<int, std::vector<std::pair<int, int>>> _stops;
    QMap<int, QString> _texts; // el texto de cada marcador (para sus espejos)
};

} // namespace

Parsed parse(const QString& body, const Variables& variables)
{
    return Parser(body, variables).run();
}

QByteArray stripJsonComments(const QByteArray& json)
{
    QByteArray out;
    out.reserve(json.size());
    bool inString = false;
    for (qsizetype i = 0; i < json.size(); ++i) {
        const char c = json[i];
        if (inString) {
            out += c;
            if (c == '\\' && i + 1 < json.size())
                out += json[++i];
            else if (c == '"')
                inString = false;
            continue;
        }
        if (c == '"') {
            inString = true;
            out += c;
        } else if (c == '/' && i + 1 < json.size() && json[i + 1] == '/') {
            while (i < json.size() && json[i] != '\n')
                ++i;
            out += '\n';
        } else if (c == '/' && i + 1 < json.size() && json[i + 1] == '*') {
            i += 2;
            while (i + 1 < json.size() && !(json[i] == '*' && json[i + 1] == '/'))
                ++i;
            ++i;
        } else {
            out += c;
        }
    }
    // Comas sobrantes antes de } o ] (VS Code las acepta)
    QByteArray clean;
    for (qsizetype i = 0; i < out.size(); ++i) {
        if (out[i] == ',') {
            qsizetype j = i + 1;
            while (j < out.size() && QByteArrayView(" \t\r\n").contains(out[j]))
                ++j;
            if (j < out.size() && (out[j] == '}' || out[j] == ']'))
                continue;
        }
        clean += out[i];
    }
    return clean;
}

std::vector<Definition> loadFile(const QString& path, QString* error)
{
    std::vector<Definition> definitions;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return definitions;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(stripJsonComments(file.readAll()), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = parseError.errorString();
        return definitions;
    }
    const QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        const QJsonValue body = entry.value(QStringLiteral("body"));
        QString text;
        if (body.isArray()) {
            QStringList lines;
            for (const QJsonValue& line : body.toArray())
                lines << line.toString();
            text = lines.join(QLatin1Char('\n'));
        } else {
            text = body.toString();
        }
        // Puede haber varios prefijos para el mismo fragmento
        const QJsonValue prefix = entry.value(QStringLiteral("prefix"));
        const QStringList prefixes = prefix.isArray() ? prefix.toVariant().toStringList() : QStringList{prefix.toString()};
        for (const QString& p : prefixes) {
            if (!p.isEmpty() && !text.isEmpty())
                definitions.push_back({it.key(), p, text, entry.value(QStringLiteral("description")).toString()});
        }
    }
    return definitions;
}

} // namespace edita::snippet
