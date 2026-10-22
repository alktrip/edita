// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cambios de Git en el margen y marcas en la barra de desplazamiento (no existen en N++).

#include "FakePrompter.h"
#include "core/LineDiff.h"
#include "syntax/SyntaxStyler.h"
#include "ui/EditCommands.h"
#include "ui/EditorView.h"
#include "ui/GitChanges.h"
#include "ui/MainWindow.h"
#include "ui/ScrollBarMarks.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

namespace {

using Kind = LineChangeKind;

std::vector<LineChange> diff(const char* before, const char* after)
{
    return diffLines(splitLines(before), splitLines(after));
}

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

} // namespace

class TestGitChanges : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    QString repo() const { return _tmp.filePath("repo"); }

    bool git(const QStringList& args)
    {
        QProcess p;
        p.setWorkingDirectory(repo());
        p.start(QStringLiteral("git"), QStringList{"-c", "user.name=Edita", "-c", "user.email=edita@example.org",
                                                   "-c", "commit.gpgsign=false", "-c", "init.defaultBranch=main"} + args);
        return p.waitForFinished(20000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("GIT_CONFIG_GLOBAL", "/dev/null");
        qputenv("GIT_CONFIG_NOSYSTEM", "1");
    }

    void lineDiff()
    {
        QVERIFY(diff("a\nb\nc\n", "a\nb\nc\n").empty());
        // Añadidas, modificadas y borradas
        QCOMPARE(diff("a\nb\nc\n", "a\nx\ny\nb\nc\n"), (std::vector<LineChange>{{Kind::Added, 1, 2, 1, 0}}));
        QCOMPARE(diff("a\nb\nc\n", "a\nB\nc\n"), (std::vector<LineChange>{{Kind::Modified, 1, 1, 1, 1}}));
        QCOMPARE(diff("a\nb\nc\n", "a\nc\n"), (std::vector<LineChange>{{Kind::Deleted, 1, 0, 1, 1}}));
        // Al principio y al final
        QCOMPARE(diff("a\nb\n", "b\n"), (std::vector<LineChange>{{Kind::Deleted, 0, 0, 0, 1}}));
        QCOMPARE(diff("a\nb\n", "a\nb\nc\n"), (std::vector<LineChange>{{Kind::Added, 2, 1, 2, 0}}));
        QCOMPARE(diff("a\nb\nc\n", "a\nb\n"), (std::vector<LineChange>{{Kind::Deleted, 2, 0, 2, 1}}));
        // Varios cambios separados
        QCOMPARE(diff("1\n2\n3\n4\n5\n6\n", "1\nX\n3\n4\n6\nY\n"),
                 (std::vector<LineChange>{{Kind::Modified, 1, 1, 1, 1}, {Kind::Deleted, 4, 0, 4, 1}, {Kind::Added, 5, 1, 6, 0}}));
        // Fines de línea distintos: no son cambios
        QVERIFY(diff("a\r\nb\r\n", "a\nb\n").empty());
        QCOMPARE(splitLines("a\r\nb\rc\n").size(), size_t{3});
        QCOMPARE(splitLines("a\nb").size(), size_t{2});
        // Demasiadas diferencias: un solo cambio con lo del medio
        bool complete = true;
        const auto changes = diffLines(splitLines("x\n1\n2\n3\n4\ny\n"), splitLines("x\na\nb\nc\nd\ny\n"), 2, &complete);
        QVERIFY(!complete);
        QCOMPARE(changes, (std::vector<LineChange>{{Kind::Modified, 1, 4, 1, 4}}));
    }

    void marksFollowTheRepository()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty())
            QSKIP("git no está instalado");
        QDir().mkpath(repo());
        QVERIFY(git({"init", "-q"}));
        const QString file = repo() + "/notas.txt";
        writeFile(file, "uno\ndos\ntres\ncuatro\ncinco\n");
        writeFile(repo() + "/nuevo.txt", "sin commit\n");
        QVERIFY(git({"add", "notas.txt"}));
        QVERIFY(git({"commit", "-q", "-m", "inicio"}));

        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.openFile(file) != kInvalidBuffer);
        GitChanges* tracker = w.gitChangesOfCurrentView();
        QVERIFY(tracker);
        QTRY_VERIFY(tracker->hasBase());
        QVERIFY(tracker->changes().empty());
        EditorView* view = w.editor();
        QCOMPARE(view->sci(SCI_GETMARGINWIDTHN, GitChanges::kMargin), sptr_t{5}); // en un repositorio: el margen

        // Añadir una línea, cambiar otra y borrar otra: las marcas cambian mientras se escribe
        view->sends(SCI_SETTEXT, 0, "uno\nDOS\ntres\nnueva\ncinco\n");
        QTRY_COMPARE(tracker->changes().size(), size_t{2});
        QCOMPARE(tracker->changes()[0], (LineChange{Kind::Modified, 1, 1, 1, 1})); // dos → DOS
        QCOMPARE(tracker->changes()[1], (LineChange{Kind::Modified, 3, 1, 3, 1})); // cuatro → nueva
        view->sends(SCI_SETTEXT, 0, "uno\nDOS\ntres\ncuatro\nnueva\ncinco\n");
        QTRY_VERIFY(tracker->changes().size() == 2 && tracker->changes()[1] == (LineChange{Kind::Added, 4, 1, 4, 0}));
        QVERIFY(view->sci(SCI_MARKERGET, 1) & (1 << GitChanges::kMarkerModified));
        QVERIFY(view->sci(SCI_MARKERGET, 4) & (1 << GitChanges::kMarkerAdded));
        QVERIFY(!(view->sci(SCI_MARKERGET, 0) & ((1 << GitChanges::kMarkerAdded) | (1 << GitChanges::kMarkerModified))));
        view->sends(SCI_SETTEXT, 0, "uno\ntres\ncuatro\ncinco\n");
        QTRY_COMPARE(tracker->changes().size(), size_t{1});
        QCOMPARE(tracker->changes()[0].kind, Kind::Deleted);
        QVERIFY(view->sci(SCI_MARKERGET, 1) & (1 << GitChanges::kMarkerDeleted));
        // El aviso: las líneas originales
        QCOMPARE(tracker->originalLines(tracker->changes()[0]), QStringList{QStringLiteral("dos")});

        // Siguiente y anterior cambio de Git (dando la vuelta)
        view->sends(SCI_SETTEXT, 0, "UNO\ndos\ntres\ncuatro\nCINCO\n");
        QTRY_COMPARE(tracker->changes().size(), size_t{2});
        view->sci(SCI_GOTOLINE, 2);
        QAction* next = w.findChild<QAction*>(QStringLiteral("actionNextGitChange"));
        QAction* previous = w.findChild<QAction*>(QStringLiteral("actionPreviousGitChange"));
        QVERIFY(next && previous);
        const auto caretLine = [&] { return view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(view->sci(SCI_GETCURRENTPOS))); };
        next->trigger();
        QCOMPARE(caretLine(), sptr_t{4});
        next->trigger();
        QCOMPARE(caretLine(), sptr_t{0});
        previous->trigger();
        QCOMPARE(caretLine(), sptr_t{4});

        // Marcas en la barra: Git, un marcador, «Marcar todo» y el cursor
        ScrollBarMarks* marks = w.scrollBarMarksOfCurrentView();
        QVERIFY(marks);
        view->sci(SCI_MARKERADD, 2, kBookmarkMarker);
        view->sci(SCI_SETINDICATORCURRENT, kIndicatorFound);
        view->sci(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(view->sci(SCI_POSITIONFROMLINE, 3)), 3);
        marks->updateNow();
        auto lanes = [&](int lane) {
            QList<int> lines;
            for (const ScrollBarMarks::Mark& m : marks->marks()) {
                if (m.lane == lane)
                    lines << m.line;
            }
            return lines;
        };
        QCOMPARE(lanes(0), (QList<int>{0, 4}));
        QCOMPARE(lanes(1), QList<int>{3});
        QCOMPARE(lanes(2), QList<int>{2});
        QCOMPARE(marks->caretLine(), 4);

        // Un commit nuevo (en una terminal): al volver, las marcas desaparecen
        QVERIFY(w.saveBuffer(w.currentBuffer()));
        QVERIFY(git({"commit", "-q", "-am", "mayúsculas"}));
        GitChanges::clearCache();
        tracker->reload();
        QTRY_VERIFY(tracker->changes().empty());

        // Desactivado en Preferencias: sin marcas ni margen
        Preferences p = w.preferences();
        p.gitMargin = false;
        p.scrollBarMarks = false;
        w.setPreferences(p);
        view->sends(SCI_SETTEXT, 0, "otra cosa\n");
        QTest::qWait(400);
        QVERIFY(tracker->changes().empty());
        QCOMPARE(view->sci(SCI_GETMARGINWIDTHN, GitChanges::kMargin), sptr_t{0});
        QVERIFY(marks->isHidden());
        p.gitMargin = true;
        w.setPreferences(p);
        QTRY_VERIFY(!tracker->changes().empty());

        // Un archivo fuera de HEAD (sin commit): sin margen
        QVERIFY(w.openFile(repo() + "/nuevo.txt") != kInvalidBuffer);
        QTest::qWait(500);
        QVERIFY(!w.gitChangesOfCurrentView()->hasBase());
        QCOMPARE(w.editor()->sci(SCI_GETMARGINWIDTHN, GitChanges::kMargin), sptr_t{0});
    }

    void otherEncodings()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty())
            QSKIP("git no está instalado");
        // Un archivo en Windows-1252 en el repositorio: se compara en la misma codificación
        const QString file = repo() + "/latin1.txt";
        writeFile(file, "a\xF1o\ncanci\xF3n\n");
        QVERIFY(git({"add", "latin1.txt"}));
        QVERIFY(git({"commit", "-q", "-m", "latin1"}));
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.openFile(file) != kInvalidBuffer);
        GitChanges* tracker = w.gitChangesOfCurrentView();
        QTRY_VERIFY(tracker->hasBase());
        QTest::qWait(100);
        QVERIFY2(tracker->changes().empty(), "el texto de HEAD se decodificó con otra codificación");
    }
};

QTEST_MAIN(TestGitChanges)
#include "tst_gitchanges.moc"
