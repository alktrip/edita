// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Git en el explorador de carpetas y Git blame (no existen en N++).

#include "FakePrompter.h"
#include "ui/EditorView.h"
#include "ui/GitBlame.h"
#include "ui/GitStatus.h"
#include "ui/MainWindow.h"
#include "ui/Panels.h"
#include "ui/Translations.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace edita;
using edita::test::FakePrompter;
using State = GitStatus::State;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QTreeWidgetItem* findItem(QTreeWidgetItem* item, const QString& name)
{
    if (item->text(0) == name)
        return item;
    for (int i = 0; i < item->childCount(); ++i) {
        if (QTreeWidgetItem* found = findItem(item->child(i), name))
            return found;
    }
    return nullptr;
}

} // namespace

class TestGitExplorer : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    QString _repo;

    void git(const QStringList& args, const QString& author = QStringLiteral("Ana López"), const QString& date = QString())
    {
        QProcess p;
        p.setWorkingDirectory(_repo);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (!date.isEmpty()) {
            env.insert("GIT_AUTHOR_DATE", date);
            env.insert("GIT_COMMITTER_DATE", date);
        }
        p.setProcessEnvironment(env);
        p.start("git", QStringList{"-c", "user.name=" + author, "-c", "user.email=" + author.left(3).toLower() + "@x.mx", "-c",
                                   "commit.gpgsign=false", "-c", "init.defaultBranch=principal"} + args);
        QVERIFY(p.waitForFinished());
        QVERIFY2(p.exitCode() == 0, p.readAllStandardError().constData());
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        i18n::install(QStringLiteral("en")); // las formas de plural
        if (QStandardPaths::findExecutable("git").isEmpty())
            QSKIP("Sin git");
        // Un repositorio: dos commits de dos personas y archivos en todos los estados
        _repo = QDir(_tmp.filePath("repo")).absolutePath();
        writeFile(_repo + "/src/main.cpp", "int main()\n{\n    return 0;\n}\n");
        writeFile(_repo + "/src/viejo.txt", "sin cambios\n");
        writeFile(_repo + "/.gitignore", "build/\n*.log\n");
        git({"init", "-q"});
        git({"add", "."});
        git({"commit", "-q", "-m", "Primera versión"}, "Ana López", "2020-01-02T10:00:00");
        writeFile(_repo + "/src/main.cpp", "int main()\n{\n    return 1;\n}\n");
        git({"commit", "-q", "-am", "Devuelve 1"}, "Beto Ruiz");
        writeFile(_repo + "/src/viejo.txt", "cambiado\n");           // modificado
        writeFile(_repo + "/docs/nuevo.md", "# nuevo\n");            // sin seguimiento
        writeFile(_repo + "/src/indice.h", "#pragma once\n");        // añadido al índice
        git({"add", "src/indice.h"});
        writeFile(_repo + "/build/salida.o", "x");                   // ignorado (carpeta)
        writeFile(_repo + "/registro.log", "x");                     // ignorado (patrón)
    }

    void parsing()
    {
        static const char raw[] = "M  a.txt\0 M b/c.txt\0?? d/\0R  nuevo.txt\0viejo.txt\0UU e.txt\0!! build/\0A  f.h\0";
        const QByteArray out(raw, sizeof(raw) - 1);
        const auto entries = GitStatus::parsePorcelain(out, "/r");
        QCOMPARE(entries.size(), std::size_t{7});
        QCOMPARE(entries[0], (std::pair<QString, State>{"/r/a.txt", State::Modified}));
        QCOMPARE(entries[2], (std::pair<QString, State>{"/r/d/", State::Untracked}));
        QCOMPARE(entries[3], (std::pair<QString, State>{"/r/nuevo.txt", State::Renamed}));
        QCOMPARE(entries[4].second, State::Conflict);
        QCOMPARE(entries[5], (std::pair<QString, State>{"/r/build/", State::Ignored}));
        QCOMPARE(entries[6].second, State::Added);

        QHash<QString, GitBlame::Commit> commits;
        const QByteArray blame = "1111111111111111111111111111111111111111 1 1 2\nauthor Ana\nauthor-mail <ana@x>\nauthor-time 1577959200\n"
                                 "summary Primera\nfilename f\n\tuno\n1111111111111111111111111111111111111111 2 2\nfilename f\n\tdos\n"
                                 "0000000000000000000000000000000000000000 3 3 1\nauthor Not Committed Yet\nfilename f\n\ttres\n";
        const auto lines = GitBlame::parsePorcelain(blame, commits);
        QCOMPARE(lines.size(), std::size_t{3});
        QCOMPARE(lines[1], QString(40, '1'));
        QCOMPARE(commits[lines[0]].author, QStringLiteral("Ana"));
        QCOMPARE(commits[lines[0]].mail, QStringLiteral("ana@x"));
        QCOMPARE(commits[lines[0]].time.toSecsSinceEpoch(), qint64{1577959200});
        QVERIFY(commits[lines[2]].uncommitted);

        const QDateTime now = QDateTime::currentDateTime();
        QCOMPARE(GitBlame::relativeTime(now.addSecs(-10), now), QStringLiteral("just now"));
        QCOMPARE(GitBlame::relativeTime(now.addSecs(-3 * 3600), now), QStringLiteral("3 hours ago"));
        QCOMPARE(GitBlame::relativeTime(now.addDays(-1), now), QStringLiteral("1 day ago"));
        QCOMPARE(GitBlame::relativeTime(now.addDays(-800), now), QStringLiteral("2 years ago"));
    }

    void statusOfTheRepository()
    {
        GitStatus status;
        QSignalSpy updated(&status, &GitStatus::updated);
        status.refresh({_repo + "/src", _tmp.filePath("no-es-repo")});
        QTRY_COMPARE(updated.count(), 1);
        QCOMPARE(status.stateOf(_repo + "/src/viejo.txt"), State::Modified);
        QCOMPARE(status.stateOf(_repo + "/src/indice.h"), State::Added);
        QCOMPARE(status.stateOf(_repo + "/docs/nuevo.md"), State::Untracked);
        QCOMPARE(status.stateOf(_repo + "/src/main.cpp"), State::None);
        QCOMPARE(status.stateOf(_repo + "/build/salida.o"), State::Ignored);
        QCOMPARE(status.stateOf(_repo + "/registro.log"), State::Ignored);
        // Las carpetas, lo más importante de dentro: src (modificado y añadido) → modificado
        QCOMPARE(status.stateOf(_repo + "/src"), State::Modified);
        QCOMPARE(status.stateOf(_repo + "/docs"), State::Untracked);
        QCOMPARE(status.stateOf(_repo), State::Modified);
        QCOMPARE(status.branchOf(_repo + "/src"), QStringLiteral("principal"));
        QVERIFY(status.branchOf(_tmp.filePath("no-es-repo")).isEmpty());
    }

    void explorerShowsTheStatus()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.addFolderToWorkspace(_repo));
        QSignalSpy updated(w.gitStatus(), &GitStatus::updated);
        QTRY_VERIFY(updated.count() >= 1);
        QTreeWidget* tree = w.findChild<FileBrowserPanel*>()->tree();
        QTreeWidgetItem* root = tree->topLevelItem(0);
        QCOMPARE(root->text(1), QStringLiteral("•"));
        QVERIFY(root->toolTip(0).contains("principal"));
        root->setExpanded(true);
        QTreeWidgetItem* src = findItem(root, "src");
        QVERIFY(src);
        src->setExpanded(true);
        QCOMPARE(findItem(root, "viejo.txt")->text(1), QStringLiteral("M"));
        QCOMPARE(findItem(root, "indice.h")->text(1), QStringLiteral("A"));
        QCOMPARE(findItem(root, "main.cpp")->text(1), QString());
        QVERIFY(findItem(root, "viejo.txt")->foreground(0).color() != findItem(root, "main.cpp")->foreground(0).color());
        // Al guardar se vuelve a leer: main.cpp pasa a modificado
        QVERIFY(w.openFile(_repo + "/src/main.cpp") != kInvalidBuffer);
        w.editor()->send(SCI_APPENDTEXT, 3, reinterpret_cast<sptr_t>("// \n"));
        const int before = static_cast<int>(updated.count());
        w.findChild<QAction*>("actionSave")->trigger();
        QTRY_VERIFY(updated.count() > before);
        QCOMPARE(findItem(root, "main.cpp")->text(1), QStringLiteral("M"));
        // Restaurar
        writeFile(_repo + "/src/main.cpp", "int main()\n{\n    return 1;\n}\n");
    }

    void blame()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.openFile(_repo + "/src/main.cpp") != kInvalidBuffer);
        GitBlame* blame = w.gitBlameOfCurrentView();
        QVERIFY(blame);
        QTRY_VERIFY(blame->hasData());
        QCOMPARE(blame->commitAt(0)->author, QStringLiteral("Ana López"));
        QCOMPARE(blame->commitAt(2)->author, QStringLiteral("Beto Ruiz"));
        QCOMPARE(blame->commitAt(2)->summary, QStringLiteral("Devuelve 1"));
        // Al final de la línea del cursor
        EditorView* v = w.editor();
        v->send(SCI_GOTOLINE, 2);
        auto eol = [v](int line) {
            QByteArray text(static_cast<qsizetype>(v->send(SCI_EOLANNOTATIONGETTEXT, static_cast<uptr_t>(line))) + 1, '\0');
            v->send(SCI_EOLANNOTATIONGETTEXT, static_cast<uptr_t>(line), reinterpret_cast<sptr_t>(text.data()));
            return QString::fromUtf8(text.constData());
        };
        QTRY_VERIFY2(eol(2).contains("Beto Ruiz") && eol(2).contains("Devuelve 1"), qPrintable(eol(2)));
        v->send(SCI_GOTOLINE, 0);
        QTRY_VERIFY(eol(0).contains("Ana López"));
        QVERIFY(eol(2).isEmpty()); // solo en la línea del cursor
        QVERIFY2(eol(0).contains("years ago"), qPrintable(eol(0)));
        // Un cambio sin guardar: esa línea, «sin confirmar»
        v->send(SCI_GOTOLINE, 1);
        v->send(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>("// nuevo\n"));
        QTRY_VERIFY_WITH_TIMEOUT(blame->commitAt(1) && blame->commitAt(1)->uncommitted, 5000);
        QCOMPARE(blame->commitAt(3)->author, QStringLiteral("Beto Ruiz")); // las demás, desplazadas
        // El margen: hash, autor y fecha en la primera línea de cada bloque
        w.findChild<QAction*>("actionGitBlame")->trigger();
        QVERIFY(v->send(SCI_GETMARGINWIDTHN, GitBlame::kMargin) > 0);
        QByteArray margin(200, '\0');
        v->send(SCI_MARGINGETTEXT, 0, reinterpret_cast<sptr_t>(margin.data()));
        QVERIFY2(QString::fromUtf8(margin.constData()).contains(blame->commitAt(0)->hash.left(7)), margin.constData());
        QVERIFY(QString::fromUtf8(margin.constData()).contains("2020-01-02"));
        // El commit de una línea, en un documento
        v->send(SCI_GOTOLINE, 3);
        const QString hash = blame->commitAt(3)->hash;
        QVERIFY(w.showCommitOfCurrentLine());
        const QString shown = QString::fromStdString(w.documents().text(w.currentBuffer()));
        QVERIFY2(shown.startsWith("commit " + hash) && shown.contains("Devuelve 1"), qPrintable(shown.left(200)));
        QCOMPARE(w.documents().buffer(w.currentBuffer())->displayName(), hash.left(7));
        QVERIFY(w.documents().buffer(w.currentBuffer())->isUserReadOnly());
        w.findChild<QAction*>("actionGitBlame")->trigger();
    }
};

QTEST_MAIN(TestGitExplorer)
#include "tst_gitexplorer.moc"
