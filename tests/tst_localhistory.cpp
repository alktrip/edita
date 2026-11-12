// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 83: el historial local. Cada guardado deja una copia fechada del archivo, para
// compararla con el documento o volver a ella (no existe en N++)

#include "FakePrompter.h"
#include "core/ConfigXml.h"
#include "core/LocalHistory.h"
#include "core/Preferences.h"
#include "ui/CompareSession.h"
#include "ui/EditorView.h"
#include "ui/LocalHistoryPanel.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace edita;
using edita::test::FakePrompter;

namespace {

QDateTime at(int day, int hour = 12, int minute = 0)
{
    return QDateTime(QDate(2026, 11, day), QTime(hour, minute), QTimeZone::UTC);
}

void writeFile(const QString& path, const QByteArray& bytes)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class TestLocalHistory : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    QString file(const QString& name) const { return _tmp.filePath(QStringLiteral("files/") + name); }

    FakePrompter* _prompter = nullptr;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        auto fake = std::make_unique<FakePrompter>();
        _prompter = fake.get();
        w->setPrompter(std::move(fake));
        return w;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_UPDATE_URL", "none");
        QDir().mkpath(_tmp.filePath("files"));
    }

    void cleanup() { QDir(LocalHistory::defaultRoot()).removeRecursively(); }

    // --- core/LocalHistory -------------------------------------------------------------

    void defaultRootIsNotInTheCloudFolder()
    {
        QCOMPARE(LocalHistory::defaultRoot(), _tmp.filePath("config/edita/localHistory"));
    }

    void recordsVersionsNewestFirst()
    {
        LocalHistory h(_tmp.filePath("h1"));
        const QString path = file("a.txt");
        QVERIFY(!h.hasHistory(path));
        QVERIFY(h.record(path, "uno", at(10), {}));
        QVERIFY(h.record(path, "dos", at(11), {}));
        QVERIFY(h.record(path, "tres", at(11, 13), {}));
        const auto versions = h.versions(path);
        QCOMPARE(versions.size(), 3);
        QCOMPARE(h.read(path, versions[0]), QByteArray("tres"));
        QCOMPARE(h.read(path, versions[2]), QByteArray("uno"));
        QCOMPARE(versions[0].when, at(11, 13));
        QCOMPARE(versions[1].size, qint64{3});
        // Cada archivo tiene el suyo
        QVERIFY(!h.hasHistory(file("b.txt")));
        // La misma ruta escrita de otra forma
        QCOMPARE(h.versions(_tmp.filePath("files/../files/./a.txt")).size(), 3);
    }

    // Guardar sin cambios no deja otra copia
    void identicalContentIsNotRecordedTwice()
    {
        LocalHistory h(_tmp.filePath("h2"));
        const QString path = file("a.txt");
        QVERIFY(h.record(path, "igual", at(10), {}));
        QVERIFY(!h.record(path, "igual", at(10, 13), {}));
        QVERIFY(h.record(path, "otro", at(10, 14), {}));
        QVERIFY(h.record(path, "igual", at(10, 15), {})); // volver a lo de antes sí es una versión
        QCOMPARE(h.versions(path).size(), 3);
    }

    // Dos guardados en el mismo milisegundo
    void sameInstantKeepsBoth()
    {
        LocalHistory h(_tmp.filePath("h3"));
        const QString path = file("a.txt");
        QVERIFY(h.record(path, "1", at(10), {}));
        QVERIFY(h.record(path, "2", at(10), {}));
        QVERIFY(h.record(path, "3", at(10), {}));
        const auto versions = h.versions(path);
        QCOMPARE(versions.size(), 3);
        QCOMPARE(h.read(path, versions[0]), QByteArray("3"));
        QCOMPARE(h.read(path, versions[2]), QByteArray("1"));
    }

    void limits()
    {
        LocalHistory h(_tmp.filePath("h4"));
        const QString path = file("a.txt");
        LocalHistoryLimits limits;
        limits.maxVersions = 3;
        limits.maxDays = 5;
        limits.maxFileBytes = 10;
        QVERIFY(!h.record(path, "demasiado grande", at(1), limits));
        QVERIFY(!h.hasHistory(path));
        for (int day = 1; day <= 4; ++day)
            QVERIFY(h.record(path, QByteArray::number(day), at(day), limits));
        QCOMPARE(h.versions(path).size(), 3); // la del día 1 sobra
        QCOMPARE(h.read(path, h.versions(path).last()), QByteArray("2"));
        // A los 5 días se borran las viejas…
        h.prune(path, limits, at(9));
        QCOMPARE(h.versions(path).size(), 1);
        // …pero la última se queda siempre
        h.prune(path, limits, at(30));
        QCOMPARE(h.versions(path).size(), 1);
        QCOMPARE(h.read(path, h.versions(path).first()), QByteArray("4"));
    }

    void renameFollowsTheFile()
    {
        LocalHistory h(_tmp.filePath("h5"));
        const QString from = file("viejo.txt");
        const QString to = file("nuevo.txt");
        QVERIFY(h.record(from, "a", at(10), {}));
        QVERIFY(h.record(from, "b", at(11), {}));
        h.rename(from, to);
        QVERIFY(!h.hasHistory(from));
        QCOMPARE(h.versions(to).size(), 2);
        QCOMPARE(readFile(QFileInfo(QDir(h.root()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot).value(0).filePath(),
                                    QStringLiteral("path")).filePath()),
                 to.toUtf8());
    }

    void removeAndClear()
    {
        LocalHistory h(_tmp.filePath("h6"));
        const QString path = file("a.txt");
        QVERIFY(h.record(path, "a", at(10), {}));
        QVERIFY(h.record(path, "b", at(11), {}));
        h.removeVersion(path, h.versions(path).first());
        QCOMPARE(h.versions(path).size(), 1);
        QVERIFY(h.totalSize() > 0);
        h.clear(path);
        QVERIFY(!h.hasHistory(path));
        QCOMPARE(h.totalSize(), qint64{0});
    }

    // --- Preferencias --------------------------------------------------------------------

    void preferencesRoundTrip()
    {
        Preferences p;
        QVERIFY(p.localHistory);
        QCOMPARE(p.localHistoryVersions, 50);
        QCOMPARE(p.localHistoryDays, 30);
        QCOMPARE(p.localHistoryMaxKB, 2048);
        p.localHistory = false;
        p.localHistoryVersions = 7;
        p.localHistoryDays = 3;
        p.localHistoryMaxKB = 100;
        ConfigXml config;
        storePreferences(config, p);
        const Preferences back = loadPreferences(config);
        QVERIFY(!back.localHistory);
        QCOMPARE(back.localHistoryVersions, 7);
        QCOMPARE(back.localHistoryDays, 3);
        QCOMPARE(back.localHistoryMaxKB, 100);
        QCOMPARE(back.localHistoryLimits().maxFileBytes, qint64{100 * 1024});
    }

    // --- En la ventana ---------------------------------------------------------------------

    // Al guardar por primera vez, también queda lo que había en el disco antes
    void savingRecordsTheOriginalAndEachVersion()
    {
        const QString path = file("guardar.txt");
        writeFile(path, "original\n");
        auto w = window();
        QVERIFY(w->openFile(path));
        w->editor()->sci(SCI_DOCUMENTEND);
        w->insertText(QStringLiteral("uno\n"));
        QVERIFY(w->saveBuffer(w->currentBuffer()));
        w->insertText(QStringLiteral("dos\n"));
        QVERIFY(w->saveBuffer(w->currentBuffer()));
        QVERIFY(w->saveBuffer(w->currentBuffer())); // sin cambios: no es una versión
        LocalHistory h(LocalHistory::defaultRoot());
        const auto versions = h.versions(path);
        QCOMPARE(versions.size(), 3);
        QCOMPARE(h.read(path, versions[0]), QByteArray("original\nuno\ndos\n"));
        QCOMPARE(h.read(path, versions[2]), QByteArray("original\n"));
    }

    void disabledRecordsNothing()
    {
        const QString path = file("apagado.txt");
        writeFile(path, "x\n");
        auto w = window();
        Preferences p = w->preferences();
        p.localHistory = false;
        w->setPreferences(p);
        QVERIFY(w->openFile(path));
        w->insertText(QStringLiteral("y"));
        QVERIFY(w->saveBuffer(w->currentBuffer()));
        QVERIFY(!LocalHistory(LocalHistory::defaultRoot()).hasHistory(path));
        p.localHistory = true; // se guarda en config.xml: las demás pruebas lo quieren encendido
        w->setPreferences(p);
    }

    // Archivo › Historial local: el panel con las versiones del documento actual
    void panelListsTheVersions()
    {
        const QString path = file("panel.txt");
        writeFile(path, "a\n");
        auto w = window();
        QVERIFY(w->openFile(path));
        w->insertText(QStringLiteral("b"));
        QVERIFY(w->saveBuffer(w->currentBuffer()));
        auto* action = w->findChild<QAction*>(QStringLiteral("actionLocalHistory"));
        QVERIFY(action);
        action->trigger();
        auto* dock = w->findChild<QDockWidget*>(QStringLiteral("localHistoryDock"));
        QVERIFY(dock && dock->isVisibleTo(w.get()));
        auto* panel = w->findChild<LocalHistoryPanel*>();
        QVERIFY(panel);
        QTRY_COMPARE(panel->tree()->topLevelItemCount(), 2);
        // Lo que se guarda aparece sin volver a abrirlo
        w->insertText(QStringLiteral("c"));
        QVERIFY(w->saveBuffer(w->currentBuffer()));
        QTRY_COMPARE(panel->tree()->topLevelItemCount(), 3);
        // Otro documento, otra lista
        w->newDocument();
        QTRY_COMPARE(panel->tree()->topLevelItemCount(), 0);
        QVERIFY(!panel->findChild<QPushButton*>(QStringLiteral("localHistoryRestore"))->isEnabled());
    }

    // Restaurar cambia el texto (se puede deshacer) y no guarda
    void restoreReplacesTheText()
    {
        const QString path = file("restaurar.txt");
        writeFile(path, "primera\n");
        auto w = window();
        QVERIFY(w->openFile(path));
        w->editor()->setText("segunda\n");
        QVERIFY(w->saveBuffer(w->currentBuffer()));
        w->findChild<QAction*>(QStringLiteral("actionLocalHistory"))->trigger();
        auto* panel = w->findChild<LocalHistoryPanel*>();
        QTRY_COMPARE(panel->tree()->topLevelItemCount(), 2);
        panel->tree()->setCurrentItem(panel->tree()->topLevelItem(1)); // la original
        panel->findChild<QPushButton*>(QStringLiteral("localHistoryRestore"))->click();
        QCOMPARE(w->editor()->text(), std::string("primera\n"));
        QVERIFY(w->documents().buffer(w->currentBuffer())->isDirty());
        QCOMPARE(readFile(path), QByteArray("segunda\n"));
        w->editor()->sci(SCI_UNDO);
        QCOMPARE(w->editor()->text(), std::string("segunda\n"));
    }

    // Comparar abre la versión en la otra vista, como Comparar con lo guardado
    void compareWithAVersion()
    {
        const QString path = file("comparar.txt");
        writeFile(path, "a\nb\n");
        auto w = window();
        QVERIFY(w->openFile(path));
        w->editor()->setText("a\nB\nc\n");
        QVERIFY(w->saveBuffer(w->currentBuffer()));
        w->findChild<QAction*>(QStringLiteral("actionLocalHistory"))->trigger();
        auto* panel = w->findChild<LocalHistoryPanel*>();
        QTRY_COMPARE(panel->tree()->topLevelItemCount(), 2);
        panel->tree()->setCurrentItem(panel->tree()->topLevelItem(1));
        panel->findChild<QPushButton*>(QStringLiteral("localHistoryCompare"))->click();
        QVERIFY(w->compareSession());
        QVERIFY(!w->compareSession()->stats().identical());
        w->stopCompare();
        QCOMPARE(w->editor()->text(), std::string("a\nB\nc\n"));
    }

    // Renombrar desde Edita: el historial sigue al archivo
    void renameKeepsTheHistory()
    {
        const QString from = file("antes.txt");
        const QString to = file("despues.txt");
        writeFile(from, "1\n");
        auto w = window();
        QVERIFY(w->openFile(from));
        w->insertText(QStringLiteral("0"));
        QVERIFY(w->saveBuffer(w->currentBuffer()));
        _prompter->saveNames.enqueue(to);
        QVERIFY(w->renameBuffer(w->currentBuffer()));
        LocalHistory h(LocalHistory::defaultRoot());
        QVERIFY(!h.hasHistory(from));
        QCOMPARE(h.versions(to).size(), 2);
    }
};

QTEST_MAIN(TestLocalHistory)
#include "tst_localhistory.moc"
