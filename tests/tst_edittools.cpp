// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 68: Edición y Herramientas como en N++: fecha y hora, copiar al portapapeles, selección
// múltiple, Resumen, seguimiento (tail -f), MD5/SHA y la información de depuración

#include "AppInfo.h"
#include "FakePrompter.h"
#include "core/DateTimeFormat.h"
#include "core/Preferences.h"
#include "doc/Buffer.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/HashDialog.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLocale>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

class TestEditTools : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    FakePrompter* _prompter = nullptr;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        auto prompter = std::make_unique<FakePrompter>();
        _prompter = prompter.get();
        w->setPrompter(std::move(prompter));
        QCoreApplication::processEvents();
        return w;
    }

    QString file(const QString& name, const QByteArray& text)
    {
        const QString path = _tmp.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return {};
        f.write(text);
        return path;
    }

    static QAction* action(MainWindow& w, const char* name) { return w.findChild<QAction*>(QString::fromLatin1(name)); }
    static QString text(MainWindow& w) { return QString::fromStdString(w.editor()->text()); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    // El formato de N++ (el de Windows) y los del sistema
    void dateTimeFormats()
    {
        const QDateTime when(QDate(2026, 3, 7), QTime(9, 5, 4));
        QCOMPARE(formatDateTime(QStringLiteral("yyyy-MM-dd HH:mm:ss"), when), QStringLiteral("2026-03-07 09:05:04"));
        // «tt»: AM/PM, con el indicador del idioma del sistema (en es_MX, «a.m.»)
        QCOMPARE(formatDateTime(QStringLiteral("d/M/yy h:mm tt"), when),
                 QStringLiteral("7/3/26 9:05 ") + QLocale::system().toString(when.time(), QStringLiteral("AP")));
        QCOMPARE(formatDateTime(QStringLiteral("'Hoy:' dd"), when), QStringLiteral("Hoy: 07"));
        // ddd/MMMM con los nombres del idioma del sistema
        QCOMPARE(formatDateTime(QStringLiteral("dddd MMMM"), when),
                 QLocale::system().toString(when, QStringLiteral("dddd MMMM")));
        const QString shortTime = QLocale::system().toString(when.time(), QLocale::ShortFormat);
        const QString shortDate = QLocale::system().toString(when.date(), QLocale::ShortFormat);
        QCOMPARE(localDateTime(when, false, false), shortTime + QLatin1Char(' ') + shortDate); // como el Bloc de notas
        QCOMPARE(localDateTime(when, false, true), shortDate + QLatin1Char(' ') + shortTime);
    }

    // Editar › Insertar › Fecha y hora, y su preferencia
    void insertDateTime()
    {
        auto w = window();
        Preferences p = w->preferences();
        p.dateTimeFormat = QStringLiteral("'fecha' yyyy");
        w->setPreferences(p);
        w->editor()->sends(SCI_SETTEXT, 0, "<>");
        w->editor()->sci(SCI_SETSEL, 1, 1);
        action(*w, "actionDateTimeCustom")->trigger();
        QCOMPARE(text(*w), QStringLiteral("<fecha %1>").arg(QDate::currentDate().year()));
        w->editor()->sci(SCI_UNDO); // un solo paso
        QCOMPARE(text(*w), QStringLiteral("<>"));
        w->editor()->sci(SCI_SELECTALL);
        action(*w, "actionDateTimeShort")->trigger();
        QVERIFY(!text(*w).contains(QLatin1Char('<'))); // sustituye la selección
        QVERIFY(text(*w).contains(QString::number(QDate::currentDate().year() % 100)));
        // Se guarda en config.xml como en N++
        ConfigXml config;
        storePreferences(config, p);
        QCOMPARE(config.guiConfig("insertDateTime", "customizedFormat"), QStringLiteral("'fecha' yyyy"));
        QCOMPARE(loadPreferences(config).dateTimeFormat, QStringLiteral("'fecha' yyyy"));
    }

    // Editar › Copiar al portapapeles
    void copyToClipboard()
    {
        auto w = window();
        const BufferId first = w->currentBuffer();
        const QString a = file(QStringLiteral("uno.txt"), "1\n");
        const QString b = file(QStringLiteral("dos.txt"), "2\n");
        w->openFile(a);
        w->openFile(b);
        w->closeBuffer(first);
        action(*w, "actionCopyFullPath")->trigger();
        QCOMPARE(QGuiApplication::clipboard()->text(), b);
        action(*w, "actionCopyFileName")->trigger();
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("dos.txt"));
        action(*w, "actionCopyDirPath")->trigger();
        QCOMPARE(QGuiApplication::clipboard()->text(), QFileInfo(b).absolutePath());
        action(*w, "actionCopyAllNames")->trigger();
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("uno.txt\ndos.txt"));
        action(*w, "actionCopyAllPaths")->trigger();
        QCOMPARE(QGuiApplication::clipboard()->text(), a + QLatin1Char('\n') + b);
    }

    // Selección múltiple: todas, la siguiente, deshacer la última, saltar la actual
    void multiSelect()
    {
        auto w = window();
        EditorView* v = w->editor();
        v->sends(SCI_SETTEXT, 0, "uno Uno uno unos uno");
        v->sci(SCI_GOTOPOS, 1); // en la primera «uno», sin selección
        action(*w, "actionMultiSelectAll")->trigger(); // sin distinguir mayúsculas ni palabra completa
        QCOMPARE(v->sci(SCI_GETSELECTIONS), sptr_t{5});
        v->sci(SCI_GOTOPOS, 1);
        action(*w, "actionMultiSelectAllCaseWord")->trigger();
        QCOMPARE(v->sci(SCI_GETSELECTIONS), sptr_t{3}); // ni «Uno» ni «unos»
        v->sci(SCI_SETSEL, 0, 3);
        action(*w, "actionMultiSelectNextCaseWord")->trigger();
        QCOMPARE(v->sci(SCI_GETSELECTIONS), sptr_t{2});
        QCOMPARE(v->sci(SCI_GETSELECTIONNSTART, 1), sptr_t{8});
        action(*w, "actionMultiSelectSkip")->trigger(); // quita la de 8 y añade la de 17
        QCOMPARE(v->sci(SCI_GETSELECTIONS), sptr_t{2});
        QCOMPARE(v->sci(SCI_GETSELECTIONNSTART, 1), sptr_t{17});
        action(*w, "actionMultiSelectUndo")->trigger();
        QCOMPARE(v->sci(SCI_GETSELECTIONS), sptr_t{1});
    }

    // Ver › Resumen
    void summary()
    {
        auto w = window();
        w->openFile(file(QStringLiteral("resumen.txt"), "Hola, mundo.\r\nñandú y más\r\n"));
        EditorView* v = w->editor();
        v->sci(SCI_SETSEL, 0, 4); // «Hola»
        const QString s = w->documentSummary();
        QVERIFY2(s.contains(QStringLiteral("Full file path: ") + _tmp.filePath("resumen.txt")), qPrintable(s));
        QVERIFY(s.contains(QStringLiteral("Modified: ")));
        QVERIFY2(s.contains(QStringLiteral("Characters (without line endings): 23")), qPrintable(s));
        QVERIFY(s.contains(QStringLiteral("Words: 5")));
        QVERIFY(s.contains(QStringLiteral("Lines: 3")));
        QVERIFY(s.contains(QStringLiteral("Document length: %1").arg(QLocale::system().toString(30))));
        QVERIFY(s.contains(QStringLiteral("4 selected characters (4 bytes) in 1 ranges")));
        action(*w, "actionSummary")->trigger();
        QCOMPARE(_prompter->informations.size(), 1);
    }

    // Ver › Seguimiento (tail -f): solo lectura, se vuelve a leer solo y va al final
    void monitoring()
    {
        auto w = window();
        const QString path = file(QStringLiteral("registro.log"), "uno\n");
        w->openFile(path);
        Buffer* buf = w->documents().buffer(w->currentBuffer());
        action(*w, "actionMonitoring")->trigger();
        QVERIFY(buf->isMonitoring());
        QVERIFY(buf->isUserReadOnly());
        QVERIFY(action(*w, "actionMonitoring")->isChecked());
        QFile f(path);
        QVERIFY(f.open(QIODevice::Append));
        f.write("dos\ntres\n");
        f.close();
        QTRY_COMPARE_WITH_TIMEOUT(text(*w), QStringLiteral("uno\ndos\ntres\n"), 5000);
        QCOMPARE(w->editor()->sci(SCI_GETCURRENTPOS), w->editor()->sci(SCI_GETLENGTH)); // al final
        QVERIFY(_prompter->warnings.isEmpty()); // sin preguntar
        action(*w, "actionMonitoring")->trigger();
        QVERIFY(!buf->isMonitoring());
        QVERIFY(!buf->isUserReadOnly());
        // Un documento sin guardar no se puede seguir
        w->newDocument();
        action(*w, "actionMonitoring")->trigger();
        QVERIFY(!w->documents().buffer(w->currentBuffer())->isMonitoring());
        QVERIFY(_prompter->warnings.last().contains(QStringLiteral("should exist")));
    }

    // Herramientas: MD5, SHA-1, SHA-256 y SHA-512
    void hashes()
    {
        QCOMPARE(HashDialog::digest("abc", QCryptographicHash::Md5), QStringLiteral("900150983cd24fb0d6963f7d28e17f72"));
        QCOMPARE(HashDialog::digest("abc", QCryptographicHash::Sha1), QStringLiteral("a9993e364706816aba3e25717850c26c9cd0d89d"));
        QCOMPARE(HashDialog::digest("abc", QCryptographicHash::Sha256),
                 QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        QCOMPARE(HashDialog::digestText(QStringLiteral("abc\n\nabc"), QCryptographicHash::Md5, true),
                 QStringLiteral("900150983cd24fb0d6963f7d28e17f72\n\n900150983cd24fb0d6963f7d28e17f72"));
        QCOMPARE(HashDialog::digestText(QString(), QCryptographicHash::Md5, false), QString());
        const QString path = file(QStringLiteral("abc.txt"), "abc");
        QCOMPARE(HashDialog::digestFiles({path, _tmp.filePath("no-existe")}, QCryptographicHash::Md5),
                 QStringLiteral("900150983cd24fb0d6963f7d28e17f72  %1\n%2: cannot be read").arg(path, _tmp.filePath("no-existe")));
        FakePrompter prompter;
        HashDialog text(HashDialog::Mode::Text, QCryptographicHash::Sha256, prompter);
        text.setInputText(QStringLiteral("abc"));
        QCOMPARE(text.result(), QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        HashDialog files(HashDialog::Mode::Files, QCryptographicHash::Md5, prompter);
        files.setFiles({path});
        QVERIFY(files.result().startsWith(QStringLiteral("900150983cd24fb0d6963f7d28e17f72  ")));

        // Desde la selección al portapapeles
        auto w = window();
        w->editor()->sends(SCI_SETTEXT, 0, "xabcx");
        w->editor()->sci(SCI_SETSEL, 1, 4);
        action(*w, "actionMd5Selection")->trigger();
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("900150983cd24fb0d6963f7d28e17f72"));
        w->editor()->sci(SCI_SETSEL, 2, 2);
        QVERIFY(!w->hashSelectionToClipboard(QCryptographicHash::Md5));
        for (const char* name : {"actionSha1Text", "actionSha256Files", "actionSha512Selection"})
            QVERIFY(action(*w, name));
    }

    // Ayuda › Información de depuración
    void debugInfo()
    {
        auto w = window();
        const QString info = w->debugInfo();
        QVERIFY(info.startsWith(QStringLiteral("Edita %1").arg(QString::fromUtf8(kAppVersion))));
        QVERIFY(info.contains(QStringLiteral("Built with: ")));
        QVERIFY(info.contains(QStringLiteral("Qt: %1").arg(QString::fromLatin1(qVersion()))));
        QVERIFY(info.contains(QStringLiteral("System: ")));
        QVERIFY(info.contains(QStringLiteral("Plugins: ")));
        QVERIFY(action(*w, "actionDebugInfo"));
    }
};

QTEST_MAIN(TestEditTools)
#include "tst_edittools.moc"
