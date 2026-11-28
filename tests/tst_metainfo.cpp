// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 104: los metadatos de AppStream (GNOME Software, KDE Discover, Flathub) y el .desktop.
// Son válidos, se corresponden entre sí y la versión de Edita está en <releases> (al publicar
// una versión hay que añadirla, si no esta prueba falla).

#include "AppInfo.h"

#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTest>

using namespace edita;

namespace {

QString generated(const QString& name)
{
    return QStringLiteral(EDITA_GENERATED_DIR "/") + name;
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// 1.0.0-rc.7 → 1.0.0~rc.7: en AppStream, «~» ordena antes que la versión final
QString appstreamVersion()
{
    return QString::fromUtf8(kAppVersion).replace(QLatin1Char('-'), QLatin1Char('~'));
}

} // namespace

class TestMetainfo : public QObject
{
    Q_OBJECT

private slots:
    void metainfoMatchesTheApplication()
    {
        const QString metainfo = generated(QString::fromUtf8(kMetainfoId) + QStringLiteral(".metainfo.xml"));
        const QByteArray xml = readAll(metainfo);
        QVERIFY2(!xml.isEmpty(), qPrintable(metainfo));
        QVERIFY(xml.contains(QStringLiteral("<id>%1</id>").arg(QString::fromUtf8(kMetainfoId)).toUtf8()));
        QVERIFY(xml.contains(QStringLiteral("<launchable type=\"desktop-id\">%1.desktop</launchable>").arg(QString::fromUtf8(kDesktopId)).toUtf8()));
        QVERIFY(xml.contains(QStringLiteral("<binary>%1</binary>").arg(QString::fromUtf8(kAppId)).toUtf8()));
        // La versión de esta compilación, en <releases> (la primera: la más nueva)
        const QRegularExpression release(QStringLiteral("<release version=\"([^\"]+)\" date=\"(\\d{4}-\\d{2}-\\d{2})\""));
        const QRegularExpressionMatch first = release.match(QString::fromUtf8(xml));
        QVERIFY(first.hasMatch());
        QVERIFY2(first.captured(1) == appstreamVersion(),
                 qPrintable(QStringLiteral("Falta <release version=\"%1\"> en resources/edita.metainfo.xml.in").arg(appstreamVersion())));
        // Una versión de prueba se marca como tal
        if (kAppPrerelease)
            QVERIFY(xml.contains(QStringLiteral("<release version=\"%1\" date=\"%2\" type=\"development\">")
                                     .arg(first.captured(1), first.captured(2)).toUtf8()));
    }

    void validatesWithAppstreamcli()
    {
        const QString tool = QStandardPaths::findExecutable(QStringLiteral("appstreamcli"));
        if (tool.isEmpty())
            QSKIP("appstreamcli no está instalado");
        // Las etiquetas de AppStream 1.0 (vcs-browser, contribute, developer): el 0.15 de
        // Ubuntu 22.04 no las conoce
        QProcess version;
        version.start(tool, {QStringLiteral("--version")});
        QVERIFY(version.waitForFinished(30000));
        const QRegularExpressionMatch m = QRegularExpression(QStringLiteral("(\\d+)\\.(\\d+)")).match(QString::fromUtf8(version.readAll()));
        if (!m.hasMatch() || m.captured(1).toInt() < 1)
            QSKIP("appstreamcli anterior a 1.0");
        QProcess p;
        p.start(tool, {QStringLiteral("validate"), QStringLiteral("--strict"), QStringLiteral("--no-net"),
                       generated(QString::fromUtf8(kMetainfoId) + QStringLiteral(".metainfo.xml"))});
        QVERIFY(p.waitForFinished(60000));
        QVERIFY2(p.exitCode() == 0, p.readAllStandardOutput().constData());
    }

    void desktopFileMatches()
    {
        const QString desktop = generated(QString::fromUtf8(kDesktopId) + QStringLiteral(".desktop"));
        const QByteArray text = readAll(desktop);
        QVERIFY2(!text.isEmpty(), qPrintable(desktop));
        QVERIFY(text.contains(QStringLiteral("\nIcon=%1\n").arg(QString::fromUtf8(kDesktopId)).toUtf8()));
        QVERIFY(text.contains(QStringLiteral("\nExec=%1 %F\n").arg(QString::fromUtf8(kAppId)).toUtf8()));
        const QString tool = QStandardPaths::findExecutable(QStringLiteral("desktop-file-validate"));
        if (tool.isEmpty())
            QSKIP("desktop-file-validate no está instalado");
        QProcess p;
        p.start(tool, {desktop});
        QVERIFY(p.waitForFinished(30000));
        QVERIFY2(p.exitCode() == 0, p.readAllStandardOutput().constData());
        QVERIFY2(!p.readAllStandardOutput().contains("error"), "desktop-file-validate");
    }
};

QTEST_MAIN(TestMetainfo)
#include "tst_metainfo.moc"
