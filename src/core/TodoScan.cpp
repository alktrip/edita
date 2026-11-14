// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TodoScan.h"

#include <QFile>
#include <QRegularExpression>

namespace edita::todo {

QStringList keywords()
{
    return {QStringLiteral("TODO"), QStringLiteral("FIXME"), QStringLiteral("HACK"), QStringLiteral("XXX"), QStringLiteral("BUG"),
            QStringLiteral("NOTE")};
}

std::vector<Item> scanText(const QString& text, const QString& path)
{
    // La palabra justo después de una marca de comentario (solo espacios entre las dos). La
    // marca, al principio de la línea o después de un espacio: así «http://x/TODO» no cuenta.
    // « * » solo al principio de la línea (la continuación de /* … */)
    static const QRegularExpression re(QStringLiteral(
        R"((?:^\s*\*+|(?:^|(?<=\s))(?://+|#+|/\*+|--+|;+|<!--|%+))\s*\b(TODO|FIXME|HACK|XXX|BUG|NOTE)\b)"));
    static const QRegularExpression closing(QStringLiteral(R"(\s*(?:\*+/|-->)\s*$)"));
    std::vector<Item> items;
    int line = 0;
    qsizetype start = 0;
    while (start <= text.size()) {
        qsizetype end = text.indexOf(QLatin1Char('\n'), start);
        if (end < 0)
            end = text.size();
        QString current = text.mid(start, end - start);
        if (current.endsWith(QLatin1Char('\r')))
            current.chop(1);
        if (const auto m = re.match(current); m.hasMatch()) {
            Item item;
            item.path = path;
            item.line = line;
            item.column = static_cast<int>(m.capturedStart(1));
            item.kind = m.captured(1);
            QString rest = current.mid(m.capturedEnd(1));
            rest.remove(closing);
            while (!rest.isEmpty() && (rest.front() == QLatin1Char(':') || rest.front().isSpace()))
                rest.remove(0, 1);
            item.text = rest.trimmed();
            items.push_back(item);
        }
        if (end == text.size())
            break;
        start = end + 1;
        ++line;
    }
    return items;
}

std::vector<Item> scanFile(const QString& path, qint64 maxBytes)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > maxBytes)
        return {};
    const QByteArray data = f.readAll();
    // Un binario: un byte 0 en los primeros 8 KB
    if (data.left(8192).contains('\0'))
        return {};
    return scanText(QString::fromUtf8(data), path);
}

} // namespace edita::todo
