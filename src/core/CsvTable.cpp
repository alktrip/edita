// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CsvTable.h"

#include <QLocale>

#include <algorithm>
#include <map>
#include <optional>

namespace edita::csv {

namespace {

// El texto de un campo sin los espacios de alrededor (los de dentro de unas comillas, sí)
QString trimmedField(const QString& field)
{
    return field.trimmed();
}

// El valor de un campo para comparar: sin comillas ni espacios, con "" → "
QString valueOf(const QString& field)
{
    QString v = field.trimmed();
    if (v.size() >= 2 && v.front() == QLatin1Char('"') && v.back() == QLatin1Char('"'))
        v = v.mid(1, v.size() - 2).replace(QStringLiteral("\"\""), QStringLiteral("\""));
    return v;
}

std::optional<double> numberOf(const QString& field)
{
    const QString v = valueOf(field);
    if (v.isEmpty())
        return std::nullopt;
    bool ok = false;
    const double d = QLocale::c().toDouble(v, &ok);
    return ok ? std::optional<double>(d) : std::nullopt;
}

// Los registros con su texto original (para volver a escribirlos tal cual) y sus campos
struct Record
{
    QString raw;
    QStringList fields;
};

std::vector<Record> records(const QString& text, QChar delimiter, bool* trailingEol)
{
    std::vector<Record> out;
    Record current;
    QString field;
    bool quoted = false;
    qsizetype recordStart = 0;
    auto endRecord = [&](qsizetype end) {
        current.fields << field;
        current.raw = text.mid(recordStart, end - recordStart);
        out.push_back(current);
        current = Record();
        field.clear();
    };
    qsizetype i = 0;
    for (; i < text.size(); ++i) {
        const QChar c = text[i];
        if (quoted) {
            field += c;
            if (c == QLatin1Char('"')) {
                if (i + 1 < text.size() && text[i + 1] == QLatin1Char('"')) {
                    field += text[++i];
                } else {
                    quoted = false;
                }
            }
            continue;
        }
        if (c == QLatin1Char('"')) {
            quoted = true;
            field += c;
        } else if (c == delimiter) {
            current.fields << field;
            field.clear();
        } else if (c == QLatin1Char('\r') || c == QLatin1Char('\n')) {
            endRecord(i);
            if (c == QLatin1Char('\r') && i + 1 < text.size() && text[i + 1] == QLatin1Char('\n'))
                ++i;
            recordStart = i + 1;
        } else {
            field += c;
        }
    }
    const bool endsWithEol = recordStart == text.size() && !text.isEmpty();
    if (!endsWithEol && !text.isEmpty())
        endRecord(text.size());
    if (trailingEol)
        *trailingEol = endsWithEol;
    return out;
}

QString join(const std::vector<QString>& lines, const QString& eol, bool trailingEol)
{
    QString out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0)
            out += eol;
        out += lines[i];
    }
    if (trailingEol)
        out += eol;
    return out;
}

} // namespace

QChar detectDelimiter(const QString& text)
{
    // Las primeras líneas que no están vacías
    QStringList lines;
    for (const QString& line : text.left(64 * 1024).split(QLatin1Char('\n'))) {
        if (!line.trimmed().isEmpty())
            lines << line;
        if (lines.size() == 20)
            break;
    }
    QChar best = QLatin1Char(',');
    int bestScore = 0;
    for (const QChar d : {QLatin1Char(','), QLatin1Char(';'), QLatin1Char('\t'), QLatin1Char('|')}) {
        // Cuántas líneas tienen el número de campos más frecuente (si es más de uno)
        std::map<size_t, int> counts;
        for (const QString& line : lines)
            ++counts[splitLine(line, d).size()];
        int score = 0;
        for (const auto& [fields, n] : counts) {
            if (fields > 1)
                score = std::max(score, n * 100 + static_cast<int>(fields));
        }
        if (score > bestScore) {
            bestScore = score;
            best = d;
        }
    }
    return best;
}

std::vector<Span> splitLine(QStringView line, QChar delimiter)
{
    std::vector<Span> spans;
    qsizetype start = 0;
    bool quoted = false;
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (quoted) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < line.size() && line[i + 1] == QLatin1Char('"'))
                    ++i;
                else
                    quoted = false;
            }
        } else if (c == QLatin1Char('"')) {
            quoted = true;
        } else if (c == delimiter) {
            spans.push_back({start, i - start});
            start = i + 1;
        }
    }
    spans.push_back({start, line.size() - start});
    return spans;
}

int columnAt(QStringView line, QChar delimiter, qsizetype pos)
{
    const std::vector<Span> spans = splitLine(line, delimiter);
    for (size_t i = 0; i < spans.size(); ++i) {
        if (pos <= spans[i].start + spans[i].length)
            return static_cast<int>(i);
    }
    return static_cast<int>(spans.size()) - 1;
}

std::vector<QStringList> parse(const QString& text, QChar delimiter)
{
    std::vector<QStringList> out;
    for (Record& r : records(text, delimiter, nullptr))
        out.push_back(std::move(r.fields));
    return out;
}

QString lineEnding(const QString& text)
{
    const qsizetype n = text.indexOf(QLatin1Char('\n'));
    const qsizetype r = text.indexOf(QLatin1Char('\r'));
    if (r >= 0 && (n < 0 || r < n))
        return n == r + 1 ? QStringLiteral("\r\n") : QStringLiteral("\r");
    return QStringLiteral("\n");
}

QString align(const QString& text, QChar delimiter)
{
    bool trailing = false;
    const std::vector<Record> rows = records(text, delimiter, &trailing);
    std::vector<qsizetype> widths;
    for (const Record& r : rows) {
        for (qsizetype c = 0; c < r.fields.size(); ++c) {
            const QString f = trimmedField(r.fields[c]);
            if (f.contains(QLatin1Char('\n')) || f.contains(QLatin1Char('\r')))
                return text; // un campo de varias líneas: no hay columnas que alinear
            if (static_cast<size_t>(c) >= widths.size())
                widths.resize(static_cast<size_t>(c) + 1, 0);
            widths[static_cast<size_t>(c)] = std::max(widths[static_cast<size_t>(c)], f.size());
        }
    }
    std::vector<QString> lines;
    for (const Record& r : rows) {
        QString line;
        for (qsizetype c = 0; c < r.fields.size(); ++c) {
            const QString f = trimmedField(r.fields[c]);
            if (c > 0)
                line += delimiter;
            // La última columna de la fila, sin espacios detrás
            line += c + 1 < r.fields.size() ? f.leftJustified(widths[static_cast<size_t>(c)]) : f;
        }
        lines.push_back(line);
    }
    return join(lines, lineEnding(text), trailing);
}

QString shrink(const QString& text, QChar delimiter)
{
    bool trailing = false;
    std::vector<QString> lines;
    for (const Record& r : records(text, delimiter, &trailing)) {
        QStringList fields;
        for (const QString& f : r.fields)
            fields << trimmedField(f);
        lines.push_back(fields.join(delimiter));
    }
    return join(lines, lineEnding(text), trailing);
}

bool hasHeader(const std::vector<QStringList>& rows)
{
    if (rows.size() < 2)
        return false;
    const QStringList& first = rows.front();
    for (const QString& f : first) {
        if (valueOf(f).isEmpty() || numberOf(f))
            return false;
    }
    // Una columna con texto arriba y números debajo
    for (qsizetype c = 0; c < first.size(); ++c) {
        int numbers = 0;
        int values = 0;
        for (size_t r = 1; r < rows.size(); ++r) {
            if (c >= rows[r].size() || valueOf(rows[r][c]).isEmpty())
                continue;
            ++values;
            if (numberOf(rows[r][c]))
                ++numbers;
        }
        if (values > 0 && numbers == values)
            return true;
    }
    return false;
}

QString sortByColumn(const QString& text, QChar delimiter, int column, bool descending)
{
    bool trailing = false;
    std::vector<Record> rows = records(text, delimiter, &trailing);
    std::vector<QStringList> fields;
    for (const Record& r : rows)
        fields.push_back(r.fields);
    const auto begin = rows.begin() + (hasHeader(fields) ? 1 : 0);
    auto cell = [column](const Record& r) { return column < r.fields.size() ? r.fields[column] : QString(); };
    // ¿Números? Todos los valores que hay en la columna
    bool numeric = true;
    bool any = false;
    for (auto it = begin; it != rows.end(); ++it) {
        if (valueOf(cell(*it)).isEmpty())
            continue;
        any = true;
        if (!numberOf(cell(*it))) {
            numeric = false;
            break;
        }
    }
    numeric = numeric && any;
    auto less = [&](const Record& a, const Record& b) {
        const QString va = valueOf(cell(a));
        const QString vb = valueOf(cell(b));
        // Los vacíos, primero
        if (va.isEmpty() || vb.isEmpty())
            return va.isEmpty() && !vb.isEmpty();
        if (numeric)
            return *numberOf(cell(a)) < *numberOf(cell(b));
        return QString::compare(va, vb, Qt::CaseInsensitive) < 0;
    };
    std::stable_sort(begin, rows.end(), [&](const Record& a, const Record& b) { return descending ? less(b, a) : less(a, b); });
    std::vector<QString> lines;
    for (const Record& r : rows)
        lines.push_back(r.raw);
    return join(lines, lineEnding(text), trailing);
}

} // namespace edita::csv
