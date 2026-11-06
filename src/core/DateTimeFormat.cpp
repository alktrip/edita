// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/DateTimeFormat.h"

#include <QLocale>

namespace edita {

QString formatDateTime(const QString& format, const QDateTime& when)
{
    // Casi todo coincide con el formato de Qt; cambia el AM/PM: «tt» es «AP» y «t», «A»
    QString qt;
    bool quoted = false;
    for (qsizetype i = 0; i < format.size(); ++i) {
        const QChar c = format.at(i);
        if (c == QLatin1Char('\'')) {
            quoted = !quoted;
            qt += c;
        } else if (!quoted && c == QLatin1Char('t')) {
            const bool two = i + 1 < format.size() && format.at(i + 1) == QLatin1Char('t');
            qt += two ? QStringLiteral("AP") : QStringLiteral("A");
            i += two ? 1 : 0;
        } else {
            qt += c;
        }
    }
    return QLocale::system().toString(when, qt);
}

QString localDateTime(const QDateTime& when, bool longFormat, bool dateFirst)
{
    const QLocale locale = QLocale::system();
    const QString date = locale.toString(when.date(), longFormat ? QLocale::LongFormat : QLocale::ShortFormat);
    const QString time = locale.toString(when.time(), QLocale::ShortFormat);
    return dateFirst ? date + QLatin1Char(' ') + time : time + QLatin1Char(' ') + date;
}

} // namespace edita
