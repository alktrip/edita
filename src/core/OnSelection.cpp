// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/OnSelection.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace edita {

namespace {

const QString kWord = QStringLiteral("$(CURRENT_WORD)");
const QString kGoogle = QStringLiteral("https://www.google.com/search?q=$(CURRENT_WORD)");

bool isOneOf(QChar c, const char* chars)
{
    for (const char* p = chars; *p; ++p) {
        if (c == QLatin1Char(*p))
            return true;
    }
    return false;
}

} // namespace

QString searchEngineTemplate(SearchEngine engine, const QString& custom)
{
    switch (engine) {
    case SearchEngine::Custom: {
        QString url = custom;
        url.remove(QRegularExpression(QStringLiteral("\\s")));
        if (url.startsWith(QLatin1String("http://")) || url.startsWith(QLatin1String("https://")))
            return url;
        return kGoogle;
    }
    case SearchEngine::DuckDuckGo:
    case SearchEngine::Bing:
        return QStringLiteral("https://duckduckgo.com/?q=$(CURRENT_WORD)");
    case SearchEngine::Yahoo:
        return QStringLiteral("https://search.yahoo.com/search?q=$(CURRENT_WORD)");
    case SearchEngine::StackOverflow:
        return QStringLiteral("https://stackoverflow.com/search?q=$(CURRENT_WORD)");
    case SearchEngine::Google:
        break;
    }
    return kGoogle;
}

QUrl searchOnInternetUrl(SearchEngine engine, const QString& custom, const QString& text)
{
    QString url = searchEngineTemplate(engine, custom);
    url.replace(kWord, QString::fromLatin1(QUrl::toPercentEncoding(text)));
    return QUrl(url);
}

QString fileNameAtColumn(const QString& line, int column)
{
    if (line.isEmpty())
        return {};
    qsizetype start = std::clamp<qsizetype>(column, 0, line.size());
    if (start == line.size())
        --start;
    while (start > 0 && !isOneOf(line.at(start), " \t[(\"<>"))
        --start;
    if (isOneOf(line.at(start), " \t[(\"<>"))
        ++start;
    qsizetype end = std::clamp<qsizetype>(column, 0, line.size());
    while (end < line.size() && !isOneOf(line.at(end), " \t:()[]<>\"\r\n"))
        ++end;
    return end > start ? line.mid(start, end - start) : QString();
}

QString resolveSelectedPath(const QString& text, const QString& currentDir)
{
    QString path = text.trimmed();
    if (path.startsWith(QLatin1String("file://")))
        path = QUrl(path).toLocalFile();
    // $VAR y ${VAR}; las que no existen se quedan como están
    static const QRegularExpression var(QStringLiteral("\\$(?:\\{([A-Za-z_][A-Za-z0-9_]*)\\}|([A-Za-z_][A-Za-z0-9_]*))"));
    QString expanded;
    qsizetype last = 0;
    for (QRegularExpressionMatchIterator it = var.globalMatch(path); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        const QString name = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
        expanded += path.mid(last, m.capturedStart() - last);
        expanded += qEnvironmentVariableIsSet(name.toLocal8Bit().constData()) ? qEnvironmentVariable(name.toLocal8Bit().constData())
                                                                              : m.captured();
        last = m.capturedEnd();
    }
    expanded += path.mid(last);
    path = expanded;
    if (path == QLatin1String("~") || path.startsWith(QLatin1String("~/")))
        path = QDir::homePath() + path.mid(1);
    if (path.isEmpty())
        return {};
    if (!QFileInfo::exists(path) && QDir::isRelativePath(path) && !currentDir.isEmpty())
        path = QDir(currentDir).filePath(path);
    return QDir::cleanPath(path);
}

QString redactText(const QString& text, bool bullet)
{
    const QChar mask = bullet ? QChar(0x25CF) : QChar(0x2588); // ● o █
    QString out;
    out.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\r') || c == QLatin1Char('\n'))
            out += c;
        else if (!c.isLowSurrogate()) // un carácter fuera del plano básico es uno solo
            out += mask;
    }
    return out;
}

} // namespace edita
