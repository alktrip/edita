// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Comparar documentos (como el plugin Compare de N++).

#include "FakePrompter.h"
#include "ui/CompareSession.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;
using namespace std::string_literals;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

bool hasMarker(ScintillaEditBase* v, int line, int marker)
{
    return (v->send(SCI_MARKERGET, static_cast<uptr_t>(line)) & (1 << marker)) != 0;
}

} // namespace

class TestCompare : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    static EditorView* view(MainWindow& w, const char* pane) { return w.findChild<EditorPane*>(pane)->editor(); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void twoDocumentsSideBySide()
    {
        // (con 100 líneas iguales al final, para poder desplazar)
        QByteArray tail;
        for (int i = 0; i < 100; ++i)
            tail += "igual " + QByteArray::number(i) + "\n";
        writeFile(_tmp.filePath("a.txt"), "uno\ndos\ntres\ncuatro\ncinco\n" + tail);
        writeFile(_tmp.filePath("b.txt"), "uno\nDOS\ntres\nnueva 1\nnueva 2\ncuatro\n" + tail);
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.resize(1000, 600);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.openFile(_tmp.filePath("a.txt")) != kInvalidBuffer);
        QVERIFY(w.openFile(_tmp.filePath("b.txt")) != kInvalidBuffer);
        // Con una sola vista, el documento actual pasa a la otra
        w.findChild<QAction*>("actionCompareViews")->trigger();
        CompareSession* session = w.compareSession();
        QVERIFY(session);
        ScintillaEditBase* oldView = view(w, "mainPane");
        ScintillaEditBase* newView = view(w, "subPane");
        QCOMPARE(session->oldView(), oldView);
        QCOMPARE(session->newView(), newView);

        // dos→DOS cambiada, dos líneas añadidas, «cinco» quitada
        const CompareSession::Stats s = session->stats();
        QCOMPARE(s.blocks, 3);
        QCOMPARE(s.changed, 1);
        QCOMPARE(s.added, 2);
        QCOMPARE(s.removed, 1);
        QVERIFY(hasMarker(oldView, 1, CompareSession::kMarkerChanged));
        QVERIFY(hasMarker(newView, 1, CompareSession::kMarkerChanged));
        QVERIFY(hasMarker(newView, 3, CompareSession::kMarkerAdded) && hasMarker(newView, 4, CompareSession::kMarkerAdded));
        QVERIFY(hasMarker(oldView, 4, CompareSession::kMarkerRemoved));
        QVERIFY(!hasMarker(oldView, 0, CompareSession::kMarkerChanged));
        // Los caracteres distintos de la línea cambiada (todo «DOS» frente a «dos»)
        const sptr_t dos = newView->send(SCI_POSITIONFROMLINE, 1);
        QVERIFY(newView->send(SCI_INDICATORVALUEAT, CompareSession::kIndicatorChars, dos) != 0);
        // Alineadas: dos líneas en blanco en la vieja tras «tres» y una en la nueva tras «cuatro»
        QCOMPARE(oldView->send(SCI_ANNOTATIONGETLINES, 2), sptr_t{2});
        QCOMPARE(newView->send(SCI_ANNOTATIONGETLINES, 5), sptr_t{1});
        QCOMPARE(oldView->send(SCI_VISIBLEFROMDOCLINE, 3), newView->send(SCI_VISIBLEFROMDOCLINE, 5)); // «cuatro»

        // Se desplazan juntas
        newView->send(SCI_SETFIRSTVISIBLELINE, 40);
        QTRY_COMPARE(oldView->send(SCI_GETFIRSTVISIBLELINE), sptr_t{40}); // (con SCN_UPDATEUI, al pintar)

        // Siguiente diferencia (también con Ctrl+Alt+↓ mientras se compara)
        newView->setFocus();
        w.setFocus();
        newView->send(SCI_GOTOPOS, 0);
        QCOMPARE(session->goToDifference(newView, true), 1);
        QCOMPARE(newView->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(newView->send(SCI_GETCURRENTPOS))), sptr_t{1});
        QCOMPARE(session->goToDifference(newView, true), 2);
        QCOMPARE(newView->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(newView->send(SCI_GETCURRENTPOS))), sptr_t{3});
        QCOMPARE(session->goToDifference(newView, false), 1);

        // Al editar se vuelve a comparar
        newView->send(SCI_SETSEL, static_cast<uptr_t>(dos), dos + 3);
        newView->send(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>("dos"));
        QTRY_COMPARE(session->stats().blocks, 2);
        QVERIFY(!hasMarker(oldView, 1, CompareSession::kMarkerChanged));

        // Ignorar mayúsculas y espacios
        newView->send(SCI_SETSEL, static_cast<uptr_t>(dos), dos + 3);
        newView->send(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>("D o S"));
        session->recompute();
        QCOMPARE(session->stats().blocks, 3);
        w.findChild<QAction*>("actionCompareIgnoreCase")->trigger();
        w.findChild<QAction*>("actionCompareIgnoreSpaces")->trigger();
        QCOMPARE(session->stats().blocks, 2);
        w.findChild<QAction*>("actionCompareIgnoreCase")->trigger();
        w.findChild<QAction*>("actionCompareIgnoreSpaces")->trigger();

        // Otra pestaña en una de las vistas termina la comparación y quita todo
        w.findChild<QAction*>("actionCompareClear")->trigger();
        QVERIFY(!w.compareSession());
        QVERIFY(!hasMarker(oldView, 4, CompareSession::kMarkerRemoved));
        QCOMPARE(oldView->send(SCI_ANNOTATIONGETLINES, 2), sptr_t{0});
        QCOMPARE(newView->send(SCI_INDICATORVALUEAT, CompareSession::kIndicatorChars, dos), sptr_t{0});
    }

    void identicalAndSwitchingTabs()
    {
        writeFile(_tmp.filePath("x.txt"), "igual\n");
        writeFile(_tmp.filePath("y.txt"), "igual\n");
        writeFile(_tmp.filePath("z.txt"), "otro\n");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.openFile(_tmp.filePath("z.txt")) != kInvalidBuffer);
        QVERIFY(w.openFile(_tmp.filePath("x.txt")) != kInvalidBuffer);
        QVERIFY(w.openFile(_tmp.filePath("y.txt")) != kInvalidBuffer);
        QVERIFY(w.startCompare(MainWindow::CompareWith::OtherView));
        QVERIFY(w.compareSession()->stats().identical());
        // Cambiar de pestaña en la vista de la izquierda: se acabó
        auto* main = w.findChild<EditorPane*>("mainPane");
        const BufferId z = w.documents().findByPath(_tmp.filePath("z.txt"));
        main->activateBuffer(main->currentBuffer() == z ? w.documents().findByPath(_tmp.filePath("x.txt")) : z);
        QVERIFY(!w.compareSession());
    }

    void withLastSavedVersion()
    {
        const QString file = _tmp.filePath("saved.txt");
        writeFile(file, "a\nb\nc\n");
        MainWindow w;
        auto prompter = std::make_unique<FakePrompter>();
        FakePrompter* asks = prompter.get();
        w.setPrompter(std::move(prompter));
        const BufferId id = w.openFile(file);
        EditorView* v = w.editor();
        v->send(SCI_GOTOPOS, 2);
        v->send(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>("nueva\n"));
        QVERIFY(w.startCompare(MainWindow::CompareWith::Saved));
        CompareSession* session = w.compareSession();
        QCOMPARE(session->stats().added, 1);
        // La referencia: en la otra vista, de solo lectura y sin cambios que guardar
        auto* main = w.findChild<EditorPane*>("mainPane");
        auto* sub = w.findChild<EditorPane*>("subPane");
        QCOMPARE(main->currentBuffer(), id);
        const Buffer* reference = w.documents().buffer(sub->currentBuffer());
        QVERIFY(reference);
        QCOMPARE(reference->displayName(), QStringLiteral("saved.txt (saved)"));
        QVERIFY(reference->isUserReadOnly());
        QCOMPARE(session->oldView(), sub->editor());
        // Las líneas iguales no tienen color, aunque el historial de cambios marque las de la
        // referencia como guardadas (sus marcadores son 21-24: antes coincidían con estos)
        for (const int marker : {CompareSession::kMarkerAdded, CompareSession::kMarkerRemoved, CompareSession::kMarkerChanged}) {
            QVERIFY(marker < SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN || marker > SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED);
            QVERIFY(!hasMarker(sub->editor(), 0, marker));
            QVERIFY(!hasMarker(sub->editor(), 2, marker));
            QVERIFY(!hasMarker(v, 0, marker));
        }
        QVERIFY(hasMarker(v, 1, CompareSession::kMarkerAdded));
        // La referencia no tiene historial de cambios (su texto no es un cambio)
        QCOMPARE(sub->editor()->send(SCI_MARKERNEXT, 0, 1 << SC_MARKNUM_HISTORY_SAVED), -1);
        // Al terminar se cierra la referencia sin preguntar
        const BufferId referenceId = sub->currentBuffer();
        w.stopCompare();
        QVERIFY(!w.documents().buffer(referenceId));
        QVERIFY(asks->asked.isEmpty());
        // Sin archivo no hay versión guardada
        w.newDocument();
        QVERIFY(!w.startCompare(MainWindow::CompareWith::Saved));
        QCOMPARE(asks->warnings.size(), 1);
    }

    void withGitHead()
    {
        if (QStandardPaths::findExecutable("git").isEmpty())
            QSKIP("Sin git");
        const QString repo = _tmp.filePath("repo");
        writeFile(repo + "/f.txt", "uno\ndos\n");
        auto git = [&](const QStringList& args) {
            QProcess p;
            p.setWorkingDirectory(repo);
            p.start("git", QStringList{"-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false"} + args);
            QVERIFY(p.waitForFinished());
            QCOMPARE(p.exitCode(), 0);
        };
        git({"init", "-q"});
        git({"add", "f.txt"});
        git({"commit", "-q", "-m", "x"});
        writeFile(repo + "/f.txt", "uno\ntres\n");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.openFile(repo + "/f.txt") != kInvalidBuffer);
        QVERIFY(w.startCompare(MainWindow::CompareWith::GitHead));
        QCOMPARE(w.compareSession()->stats().changed, 1);
        QCOMPARE(w.documents().buffer(w.findChild<EditorPane*>("subPane")->currentBuffer())->displayName(), QStringLiteral("f.txt (HEAD)"));
        // Un archivo fuera de Git: aviso
        w.stopCompare();
        writeFile(_tmp.filePath("suelto.txt"), "x\n");
        QVERIFY(w.openFile(_tmp.filePath("suelto.txt")) != kInvalidBuffer);
        QVERIFY(!w.startCompare(MainWindow::CompareWith::GitHead));
    }
};

QTEST_MAIN(TestCompare)
#include "tst_compare.moc"
