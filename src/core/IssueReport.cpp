// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/IssueReport.h"

#include <QStringList>
#include <QSysInfo>
#include <QUrlQuery>
#include <QtGlobal>

namespace edita::issue_report {

SystemInfo currentSystem(const QString& platform)
{
    SystemInfo s;
    s.os = QSysInfo::prettyProductName();
    s.kernel = QSysInfo::kernelVersion();
    s.architecture = QSysInfo::currentCpuArchitecture();
    s.qtVersion = QString::fromLatin1(qVersion());
    s.platform = platform;
    s.desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    s.appImage = qEnvironmentVariableIsSet("APPIMAGE");
    return s;
}

QString describe(const SystemInfo& s)
{
    QStringList parts;
    parts << QStringLiteral("%1 (%2, Linux %3)").arg(s.os, s.architecture, s.kernel);
    if (!s.desktop.isEmpty())
        parts << s.desktop;
    if (!s.platform.isEmpty())
        parts << s.platform;
    parts << QStringLiteral("Qt %1").arg(s.qtVersion);
    parts << (s.appImage ? QStringLiteral("AppImage") : QStringLiteral("not an AppImage"));
    return parts.join(QStringLiteral(", "));
}

QUrl formUrl(const QString& homepage, const QString& form, const QString& version, const QString& system,
             const QList<std::pair<QString, QString>>& fields)
{
    // Cada valor, codificado entero: QUrlQuery deja el «+» tal cual, y GitHub lo lee como un
    // espacio (las trazas de un fallo están llenas de «+0x1c»)
    QList<std::pair<QString, QString>> items{{QStringLiteral("template"), form},
                                             {QStringLiteral("version"), version},
                                             {QStringLiteral("system"), system}};
    items += fields;
    QByteArray query;
    for (const auto& [id, value] : items) {
        if (!query.isEmpty())
            query += '&';
        query += QUrl::toPercentEncoding(id) + '=' + QUrl::toPercentEncoding(value);
    }
    QUrl url(homepage + QStringLiteral("/issues/new"));
    url.setQuery(QString::fromLatin1(query), QUrl::StrictMode);
    return url;
}

} // namespace edita::issue_report
