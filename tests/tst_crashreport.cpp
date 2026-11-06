// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 66: el reporte de fallos. Un proceso aparte (crash_helper) falla de verdad y deja su
// reporte; al volver a abrir Edita, la barra de avisos ofrece reportarlo

#include "AppInfo.h"
#include "FakePrompter.h"
#include "core/CrashReport.h"
#include "core/IssueReport.h"
#include "ui/MainWindow.h"
#include "ui/NotificationBar.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

using namespace edita;
using edita::test::FakePrompter;

class TestCrashReport : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    QString crash(const QString& how)
    {
        const QString dir = _tmp.filePath(how);
        QDir(dir).removeRecursively();
        QProcess p;
        p.start(QStringLiteral(EDITA_CRASH_HELPER), {dir, how});
        if (!p.waitForFinished(20000))
            return QStringLiteral("(no terminó)");
        if (p.exitStatus() != QProcess::CrashExit)
            return QStringLiteral("(no falló: %1)").arg(p.exitCode());
        return crash_report::pending(dir);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        QVERIFY(QFileInfo::exists(EDITA_CRASH_HELPER));
    }

    // Un fallo de verdad: el proceso termina como sin manejador, y deja la versión, la señal y la traza
    void writesTheReportWhenItCrashes_data()
    {
        QTest::addColumn<QString>("how");
        QTest::addColumn<QString>("signal");
        QTest::newRow("puntero nulo") << "segv" << "SIGSEGV";
        QTest::newRow("abort") << "abort" << "SIGABRT";
        QTest::newRow("desbordamiento de pila") << "overflow" << "SIGSEGV";
    }
    void writesTheReportWhenItCrashes()
    {
        QFETCH(QString, how);
        QFETCH(QString, signal);
        const QString report = crash(how);
        QVERIFY2(report.startsWith(QStringLiteral("Edita 9.9.9-prueba\n")), qPrintable(report));
        QCOMPARE(crash_report::signalName(report), signal);
        QVERIFY(report.contains(QStringLiteral("\nTime: ")));
        QVERIFY(report.contains(QStringLiteral("Backtrace:\n")));
        QVERIFY2(report.contains(QStringLiteral("crash_helper")), qPrintable(report)); // el módulo de cada marco
        QVERIFY(report.count(QLatin1Char('\n')) > 5);
    }

    void readableNames()
    {
        const QString report = QStringLiteral("Edita 1\nSignal: SIGSEGV (segmentation fault)\nBacktrace:\n"
                                              "/x/edita(_ZN5edita10MainWindow4saveEv+0x1c) [0x55]\n/x/edita(+0x123) [0x56]\n");
        const QString readable = crash_report::readable(report);
        QVERIFY2(readable.contains(QStringLiteral("edita::MainWindow::save()+0x1c")), qPrintable(readable));
        QVERIFY(readable.contains(QStringLiteral("/x/edita(+0x123)")));
        QCOMPARE(crash_report::signalName(report), QStringLiteral("SIGSEGV"));
        QVERIFY(crash_report::signalName(QStringLiteral("otra cosa")).isEmpty());
    }

    // El «+» de las trazas no puede llegar a GitHub como un espacio
    void formUrlEncodesEverything()
    {
        const QUrl url = issue_report::formUrl(QStringLiteral("https://github.com/alktrip/edita"), QStringLiteral("bug.yml"),
                                               QStringLiteral("1.0"), QStringLiteral("Linux & Qt"),
                                               {{QStringLiteral("extra"), QStringLiteral("f(+0x1c) #1\nlínea")}});
        const QString encoded = url.toString(QUrl::FullyEncoded);
        QVERIFY2(encoded.contains(QStringLiteral("%2B0x1c")), qPrintable(encoded));
        QVERIFY(encoded.contains(QStringLiteral("Linux%20%26%20Qt")));
        QCOMPARE(QUrlQuery(url).queryItemValue(QStringLiteral("extra"), QUrl::FullyDecoded),
                 QStringLiteral("f(+0x1c) #1\nlínea"));
    }

    // Al volver a abrir Edita: la barra de avisos, y las tres salidas
    void offersTheReportOnTheNextStart()
    {
        auto window = [] {
            auto w = std::make_unique<MainWindow>();
            w->setPrompter(std::make_unique<FakePrompter>());
            QCoreApplication::processEvents();
            return w;
        };
        auto button = [](MainWindow& w, const QString& text) -> QPushButton* {
            for (QPushButton* b : w.notificationBar()->findChildren<QPushButton*>())
                if (b->text() == text)
                    return b;
            return nullptr;
        };
        const QString dir = _tmp.filePath("ui");
        auto w = window();
        QVERIFY(!w->offerCrashReport(dir)); // nada que reportar

        // Reportar: el formulario con la versión, el sistema, la señal y la traza
        const QString report = crash(QStringLiteral("segv"));
        QDir().mkpath(dir);
        QFile file(crash_report::reportPath(dir));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(report.toUtf8());
        file.close();
        QVERIFY(w->offerCrashReport(dir));
        QVERIFY(w->notificationBar()->currentText().contains(QStringLiteral("SIGSEGV")));
        QList<QUrl> opened;
        w->setUrlOpener([&opened](const QUrl& u) { opened << u; return true; });
        button(*w, QStringLiteral("Report…"))->click();
        QCOMPARE(opened.size(), 1);
        const QUrlQuery query(opened[0]);
        QCOMPARE(query.queryItemValue(QStringLiteral("template")), QStringLiteral("bug.yml"));
        QCOMPARE(query.queryItemValue(QStringLiteral("version")), QString::fromUtf8(kAppVersion));
        QVERIFY(query.queryItemValue(QStringLiteral("what"), QUrl::FullyDecoded).contains(QStringLiteral("SIGSEGV")));
        const QString extra = query.queryItemValue(QStringLiteral("extra"), QUrl::FullyDecoded);
        QVERIFY(extra.startsWith(QStringLiteral("```\nEdita 9.9.9-prueba")));
        QVERIFY(extra.contains(QStringLiteral("crash_helper")));
        QVERIFY(opened[0].toString(QUrl::FullyEncoded).size() < 8000); // cabe en una URL
        QVERIFY(crash_report::pending(dir).isEmpty()); // reportado: ya no se ofrece

        // Copiar los detalles
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(report.toUtf8());
        file.close();
        QVERIFY(w->offerCrashReport(dir));
        button(*w, QStringLiteral("Copy Details"))->click();
        QVERIFY(QGuiApplication::clipboard()->text().contains(QStringLiteral("Signal: SIGSEGV")));
        QVERIFY(crash_report::pending(dir).isEmpty());

        // Descartar
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(report.toUtf8());
        file.close();
        QVERIFY(w->offerCrashReport(dir));
        button(*w, QStringLiteral("Discard"))->click();
        QVERIFY(crash_report::pending(dir).isEmpty());
        QVERIFY(!w->offerCrashReport(dir));
    }
};

QTEST_MAIN(TestCrashReport)
#include "tst_crashreport.moc"
