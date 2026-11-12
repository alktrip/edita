// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 85: Git desde el margen. Descartar un bloque en el documento, pasarlo al índice sin
// tocar los demás y el panel Ver › Git para confirmar (no existe en N++)

#include "FakePrompter.h"
#include "core/GitHunks.h"
#include "ui/EditorView.h"
#include "ui/GitChanges.h"
#include "ui/GitCommitPanel.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace edita;
using namespace edita::githunks;
using edita::test::FakePrompter;

namespace {

LineChange change(LineChangeKind kind, int line, int count, int oldLine, int oldCount)
{
    return {kind, line, count, oldLine, oldCount};
}

QByteArray git(const QString& dir, const QStringList& args)
{
    QProcess p;
    p.setWorkingDirectory(dir);
    p.start(QStringLiteral("git"), args);
    p.waitForFinished(15000);
    return p.readAllStandardOutput();
}

void writeFile(const QString& path, const QByteArray& bytes)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

} // namespace

class TestGitHunks : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    FakePrompter* _prompter = nullptr;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        auto fake = std::make_unique<FakePrompter>();
        _prompter = fake.get();
        w->setPrompter(std::move(fake));
        return w;
    }

    // Un repositorio con a.txt de diez líneas en el primer commit
    QString repo(const QString& name)
    {
        const QString dir = _tmp.filePath(name);
        QDir().mkpath(dir);
        git(dir, {QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), QStringLiteral("main")});
        writeFile(dir + QStringLiteral("/a.txt"), "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n");
        git(dir, {QStringLiteral("add"), QStringLiteral("a.txt")});
        git(dir, {QStringLiteral("commit"), QStringLiteral("-q"), QStringLiteral("-m"), QStringLiteral("primero")});
        return dir;
    }

    // Abre el archivo y espera a que GitChanges tenga la versión de HEAD
    GitChanges* openAndWait(MainWindow& w, const QString& path)
    {
        w.openFile(path);
        GitChanges* g = w.gitChangesOfCurrentView();
        if (g)
            (void)QTest::qWaitFor([g] { return g->hasBase(); }, 10000);
        return g && g->hasBase() ? g : nullptr;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_UPDATE_URL", "none");
        // git sin la configuración del usuario
        qputenv("GIT_CONFIG_GLOBAL", _tmp.filePath("gitconfig").toUtf8());
        qputenv("GIT_CONFIG_NOSYSTEM", "1");
        qputenv("GIT_AUTHOR_NAME", "Prueba");
        qputenv("GIT_AUTHOR_EMAIL", "prueba@example.com");
        qputenv("GIT_COMMITTER_NAME", "Prueba");
        qputenv("GIT_COMMITTER_EMAIL", "prueba@example.com");
    }

    // --- Funciones puras -------------------------------------------------------------------

    void splitKeepsTheEol()
    {
        const auto lines = splitKeepingEol("a\nb\r\nc");
        QCOMPARE(lines.size(), std::size_t{3});
        QCOMPARE(std::string(lines[1]), std::string("b\r\n"));
        QCOMPARE(std::string(lines[2]), std::string("c"));
        QVERIFY(splitKeepingEol("").empty());
    }

    void applyEachKind()
    {
        const std::string head = "1\n2\n3\n4\n";
        // Modificada: la línea 2 (índice 1)
        QCOMPARE(applyHunk(head, "1\nDOS\n3\n4\n", change(LineChangeKind::Modified, 1, 1, 1, 1)), std::string("1\nDOS\n3\n4\n"));
        // Añadida tras la 3
        QCOMPARE(applyHunk(head, "1\n2\n3\nx\ny\n4\n", change(LineChangeKind::Added, 3, 2, 3, 0)), std::string("1\n2\n3\nx\ny\n4\n"));
        // Borradas la 2 y la 3
        QCOMPARE(applyHunk(head, "1\n4\n", change(LineChangeKind::Deleted, 1, 0, 1, 2)), std::string("1\n4\n"));
        // Solo ese bloque: lo demás del texto nuevo no entra
        QCOMPARE(applyHunk(head, "UNO\n2\n3\nCUATRO\n", change(LineChangeKind::Modified, 0, 1, 0, 1)), std::string("UNO\n2\n3\n4\n"));
    }

    // Descartar = aplicar el bloque al revés
    void invertedUndoesTheHunk()
    {
        const std::string head = "1\n2\n3\n4\n";
        const std::string now = "1\nDOS\n3\nCUATRO\nCINCO\n";
        const auto changes = diffLines(splitLines(head), splitLines(now));
        QCOMPARE(changes.size(), std::size_t{2});
        QCOMPARE(applyHunk(now, head, inverted(changes[0])), std::string("1\n2\n3\nCUATRO\nCINCO\n"));
        QCOMPARE(applyHunk(now, head, inverted(changes[1])), std::string("1\nDOS\n3\n4\n"));
        // Un borrado se repone
        const std::string cut = "1\n4\n";
        const auto del = diffLines(splitLines(head), splitLines(cut));
        QCOMPARE(applyHunk(cut, head, inverted(del[0])), head);
    }

    void endOfFileWithoutNewline()
    {
        // HEAD termina sin \n y se añade una línea detrás
        QCOMPARE(applyHunk("1\n2", "1\n2\n3", change(LineChangeKind::Added, 2, 1, 2, 0)), std::string("1\n2\n3"));
        // Se cambia la última línea (sin \n) y sigue habiendo líneas en el texto viejo: no
        QCOMPARE(applyHunk("a\nb\nc\n", "a\nB", change(LineChangeKind::Modified, 1, 1, 1, 1)), std::string("a\nB\nc\n"));
    }

    void keepsCrLf()
    {
        QCOMPARE(applyHunk("1\r\n2\r\n3\r\n", "1\r\nX\r\n3\r\n", change(LineChangeKind::Modified, 1, 1, 1, 1)),
                 std::string("1\r\nX\r\n3\r\n"));
    }

    void hunkAtLine()
    {
        const std::vector<LineChange> changes{change(LineChangeKind::Modified, 2, 2, 2, 1), change(LineChangeKind::Deleted, 8, 0, 7, 3)};
        QCOMPARE(hunkAt(changes, 3), &changes[0]);
        QVERIFY(!hunkAt(changes, 4));
        QCOMPARE(hunkAt(changes, 8), &changes[1]);
        QCOMPARE(hunkAt(changes, 7), &changes[1]);
    }

    void changedSpanIsMinimal()
    {
        const Span s = changedSpan("hola mundo", "hola gente mundo");
        QCOMPARE(s.start, std::size_t{5});
        QCOMPARE(s.endA, std::size_t{5});
        QCOMPARE(s.endB, std::size_t{11});
        // Sin partir un carácter de varios bytes («ñ» y «é» comparten el primer byte)
        const Span u = changedSpan("añb", "aéb");
        QCOMPARE(u.start, std::size_t{1});
        QCOMPARE(u.endA, std::size_t{3});
    }

    void parsesPorcelain()
    {
        const QByteArray out("M  staged.txt\0 M changed.txt\0MM both.txt\0?? new dir/x.txt\0R  nuevo.txt\0viejo.txt\0!! ignored\0", 81);
        const auto entries = GitCommitPanel::parseStatus(out);
        QCOMPARE(entries.size(), 5);
        QCOMPARE(entries[3].path, QStringLiteral("new dir/x.txt"));
        QCOMPARE(entries[3].index, QLatin1Char('?'));
        QCOMPARE(entries[4].path, QStringLiteral("nuevo.txt"));
        QCOMPARE(entries[2].worktree, QLatin1Char('M'));
    }

    // --- Con un repositorio de verdad ----------------------------------------------------

    void revertOneHunkInTheEditor()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty())
            QSKIP("sin git");
        const QString dir = repo(QStringLiteral("revert"));
        writeFile(dir + QStringLiteral("/a.txt"), "1\nDOS\n3\n4\n5\n6\n7\n8\nNUEVE\n10\n");
        auto w = window();
        GitChanges* g = openAndWait(*w, dir + QStringLiteral("/a.txt"));
        QVERIFY(g);
        QTRY_COMPARE(g->changes().size(), std::size_t{2});
        QVERIFY(w->revertGitHunk(1));
        QCOMPARE(w->editor()->text(), std::string("1\n2\n3\n4\n5\n6\n7\n8\nNUEVE\n10\n"));
        QVERIFY(w->documents().buffer(w->currentBuffer())->isDirty());
        // Un solo paso de deshacer
        w->editor()->sci(SCI_UNDO);
        QCOMPARE(w->editor()->text(), std::string("1\nDOS\n3\n4\n5\n6\n7\n8\nNUEVE\n10\n"));
        QVERIFY(!w->revertGitHunk(4)); // sin cambios en esa línea
    }

    // Cada bloque va al índice por separado, y lo que ya estaba en el stage se queda
    void stageHunksOneByOne()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty())
            QSKIP("sin git");
        const QString dir = repo(QStringLiteral("stage"));
        writeFile(dir + QStringLiteral("/a.txt"), "1\nDOS\n3\n4\n5\n6\n7\n8\nNUEVE\n10\n");
        auto w = window();
        GitChanges* g = openAndWait(*w, dir + QStringLiteral("/a.txt"));
        QTRY_COMPARE(g->changes().size(), std::size_t{2});
        QVERIFY(w->stageGitHunk(8));
        QByteArray cached = git(dir, {QStringLiteral("diff"), QStringLiteral("--cached"), QStringLiteral("-U0")});
        QVERIFY(cached.contains("+NUEVE"));
        QVERIFY(!cached.contains("+DOS"));
        QVERIFY(git(dir, {QStringLiteral("diff"), QStringLiteral("-U0")}).contains("+DOS"));
        // El segundo, con el primero ya en el índice (las líneas no se mueven, pero el índice sí cambió)
        QVERIFY(w->stageGitHunk(1));
        cached = git(dir, {QStringLiteral("diff"), QStringLiteral("--cached"), QStringLiteral("-U0")});
        QVERIFY(cached.contains("+NUEVE") && cached.contains("+DOS"));
        QVERIFY(git(dir, {QStringLiteral("diff")}).isEmpty());
        // Ya está todo: no hay nada que pasar
        QVERIFY(!w->stageGitHunk(1));
    }

    // Con cambios sin guardar, pregunta y guarda primero
    void stageAsksToSave()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty())
            QSKIP("sin git");
        const QString dir = repo(QStringLiteral("dirty"));
        auto w = window();
        openAndWait(*w, dir + QStringLiteral("/a.txt"));
        w->editor()->sci(SCI_DOCUMENTEND);
        w->insertText(QStringLiteral("11\n"));
        _prompter->confirmations.enqueue(true);
        QVERIFY(w->stageGitHunk(10));
        QVERIFY(!w->documents().buffer(w->currentBuffer())->isDirty());
        QVERIFY(git(dir, {QStringLiteral("diff"), QStringLiteral("--cached")}).contains("+11"));
    }

    void commitFromThePanel()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty())
            QSKIP("sin git");
        const QString dir = repo(QStringLiteral("commit"));
        writeFile(dir + QStringLiteral("/a.txt"), "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n");
        writeFile(dir + QStringLiteral("/b.txt"), "nuevo\n");
        auto w = window();
        w->openFile(dir + QStringLiteral("/a.txt"));
        w->findChild<QAction*>(QStringLiteral("actionGitCommit"))->trigger();
        auto* dock = w->findChild<QDockWidget*>(QStringLiteral("gitCommitDock"));
        QVERIFY(dock && dock->isVisibleTo(w.get()));
        GitCommitPanel* panel = w->gitCommitPanel();
        QTRY_COMPARE(panel->repository(), QDir::cleanPath(dir));
        QVERIFY(panel->header()->text().contains(QStringLiteral("main")));
        QTreeWidget* tree = panel->tree();
        QTreeWidgetItem* staged = tree->topLevelItem(0);
        QTreeWidgetItem* changes = tree->topLevelItem(1);
        QCOMPARE(staged->childCount(), 0);
        QCOMPARE(changes->childCount(), 2);
        // Sin nada en el índice no se puede confirmar
        panel->message()->setPlainText(QStringLiteral("Segundo commit"));
        auto* commit = panel->findChild<QPushButton*>(QStringLiteral("gitCommit"));
        QVERIFY(!commit->isEnabled());
        panel->findChild<QPushButton*>(QStringLiteral("gitStageAll"))->click();
        QTRY_COMPARE(tree->topLevelItem(0)->childCount(), 2);
        QCOMPARE(tree->topLevelItem(1)->childCount(), 0);
        // Quitar uno del índice
        tree->topLevelItem(0)->child(1)->setSelected(true);
        panel->findChild<QPushButton*>(QStringLiteral("gitUnstage"))->click();
        QTRY_COMPARE(tree->topLevelItem(0)->childCount(), 1);
        QVERIFY(commit->isEnabled());
        commit->click();
        QVERIFY(_prompter->warnings.isEmpty());
        QCOMPARE(git(dir, {QStringLiteral("log"), QStringLiteral("-1"), QStringLiteral("--format=%s")}).trimmed(), QByteArray("Segundo commit"));
        QVERIFY(panel->message()->toPlainText().isEmpty());
        QCOMPARE(tree->topLevelItem(0)->childCount(), 0);
        QCOMPARE(tree->topLevelItem(1)->childCount(), 1);
    }

    void outsideARepository()
    {
        const QString dir = _tmp.filePath(QStringLiteral("norepo"));
        QDir().mkpath(dir);
        writeFile(dir + QStringLiteral("/x.txt"), "x\n");
        auto w = window();
        w->openFile(dir + QStringLiteral("/x.txt"));
        if (!w->gitTopLevel().isEmpty())
            QSKIP("la carpeta temporal está dentro de un repositorio");
        w->refreshGitCommitPanel();
        QVERIFY(w->gitCommitPanel()->repository().isEmpty());
        QVERIFY(!w->stageGitHunk(0));
        QVERIFY(!w->revertGitHunk(0));
    }
};

QTEST_MAIN(TestGitHunks)
#include "tst_githunks.moc"
