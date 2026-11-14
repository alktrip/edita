// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 98: la pestaña Historial del panel Git (Ver › Git): los commits del repositorio o del
// archivo actual, los archivos de cada uno y su diff en un documento de solo lectura.

#include "FakePrompter.h"
#include "ui/EditorView.h"
#include "ui/GitCommitPanel.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QCheckBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QProcess>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace edita;
using edita::test::FakePrompter;

namespace {

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
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

void commit(const QString& dir, const QString& message, const QString& date)
{
    git(dir, {QStringLiteral("add"), QStringLiteral("-A")});
    qputenv("GIT_AUTHOR_DATE", date.toUtf8());
    qputenv("GIT_COMMITTER_DATE", date.toUtf8());
    git(dir, {QStringLiteral("commit"), QStringLiteral("-q"), QStringLiteral("-m"), message});
}

QStringList column(QTreeWidget* tree, int col)
{
    QStringList out;
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        out << tree->topLevelItem(i)->text(col);
    return out;
}

} // namespace

class TestGitHistory : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    QString _repo;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_UPDATE_URL", "none");
        qputenv("GIT_CONFIG_GLOBAL", _tmp.filePath("gitconfig").toUtf8());
        qputenv("GIT_CONFIG_NOSYSTEM", "1");
        qputenv("GIT_AUTHOR_NAME", "Ana Pruebas");
        qputenv("GIT_AUTHOR_EMAIL", "ana@example.com");
        qputenv("GIT_COMMITTER_NAME", "Ana Pruebas");
        qputenv("GIT_COMMITTER_EMAIL", "ana@example.com");
        // Tres commits: a.txt, b.txt, y a.txt cambiado con viejo.txt renombrado
        _repo = _tmp.filePath(QStringLiteral("repo"));
        QDir().mkpath(_repo);
        git(_repo, {QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), QStringLiteral("main")});
        writeFile(_repo + QStringLiteral("/a.txt"), "uno\n");
        writeFile(_repo + QStringLiteral("/viejo.txt"), "contenido largo que se conserva\nal renombrar el archivo\n");
        commit(_repo, QStringLiteral("Primero: a.txt"), QStringLiteral("2026-11-01T10:00:00"));
        writeFile(_repo + QStringLiteral("/b.txt"), "b\n");
        commit(_repo, QStringLiteral("Segundo: b.txt"), QStringLiteral("2026-11-02T10:00:00"));
        writeFile(_repo + QStringLiteral("/a.txt"), "uno\ndos\n");
        QVERIFY(QFile::rename(_repo + QStringLiteral("/viejo.txt"), _repo + QStringLiteral("/nuevo.txt")));
        commit(_repo, QStringLiteral("Tercero: a.txt y renombrar"), QStringLiteral("2026-11-03T10:00:00"));
    }

    // --- Lo que se lee de git ------------------------------------------------------------

    void parsesLogAndFiles()
    {
        const QByteArray log = "1111111111111111111111111111111111111111\x1f" "1111111\x1f" "Ana\x1f" "2026-11-03\x1f"
                               "Tercero\x1f" "0000000000000000000000000000000000000000\x1e\n"
                               "0000000000000000000000000000000000000000\x1f" "0000000\x1f" "Beto\x1f" "2026-11-01\x1f"
                               "Primero: con \x1f raro\x1f\x1e\n";
        const QList<GitCommitPanel::Commit> commits = GitCommitPanel::parseLog(log);
        QCOMPARE(commits.size(), 2);
        QCOMPARE(commits[0].shortHash, QStringLiteral("1111111"));
        QCOMPARE(commits[0].author, QStringLiteral("Ana"));
        QCOMPARE(commits[0].date, QStringLiteral("2026-11-03"));
        QCOMPARE(commits[0].subject, QStringLiteral("Tercero"));
        QCOMPARE(commits[0].parents, QStringList{QString(40, QLatin1Char('0'))});
        QVERIFY(commits[1].parents.isEmpty());

        const QByteArray files("M\0a.txt\0R100\0viejo.txt\0nuevo.txt\0A\0b c.txt\0", 43);
        const QList<GitCommitPanel::FileChange> changes = GitCommitPanel::parseNameStatus(files);
        QCOMPARE(changes.size(), 3);
        QCOMPARE(changes[0].status, QLatin1Char('M'));
        QCOMPARE(changes[1].status, QLatin1Char('R'));
        QCOMPARE(changes[1].path, QStringLiteral("nuevo.txt"));
        QCOMPARE(changes[1].oldPath, QStringLiteral("viejo.txt"));
        QCOMPARE(changes[2].path, QStringLiteral("b c.txt"));
    }

    // --- El panel ------------------------------------------------------------------------

    void historyTab()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.openFile(_repo + QStringLiteral("/a.txt")) != kInvalidBuffer);
        w.findChild<QAction*>(QStringLiteral("actionGitCommit"))->trigger();
        auto* tabs = w.findChild<QTabWidget*>(QStringLiteral("gitTabs"));
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        tabs->setCurrentIndex(1); // Historial
        auto* history = w.findChild<QTreeWidget*>(QStringLiteral("gitHistoryTree"));
        QTRY_COMPARE(column(history, 0), QStringList({QStringLiteral("Tercero: a.txt y renombrar"), QStringLiteral("Segundo: b.txt"),
                                                      QStringLiteral("Primero: a.txt")}));
        QCOMPARE(history->topLevelItem(0)->text(1), QStringLiteral("Ana Pruebas"));
        QCOMPARE(history->topLevelItem(0)->text(2), QStringLiteral("2026-11-03"));
        QCOMPARE(history->topLevelItem(0)->text(3).size(), 7);

        // Solo el archivo actual (a.txt): los commits que lo tocaron
        auto* onlyFile = w.findChild<QCheckBox*>(QStringLiteral("gitHistoryCurrentFile"));
        onlyFile->setChecked(true);
        QTRY_COMPARE(column(history, 0), QStringList({QStringLiteral("Tercero: a.txt y renombrar"), QStringLiteral("Primero: a.txt")}));
        onlyFile->setChecked(false);
        QTRY_COMPARE(history->topLevelItemCount(), 3);

        // Un commit: sus archivos, con el renombrado
        auto* files = w.findChild<QTreeWidget*>(QStringLiteral("gitHistoryFiles"));
        history->setCurrentItem(history->topLevelItem(0));
        QTRY_COMPARE(files->topLevelItemCount(), 2);
        QCOMPARE(column(files, 1), QStringList({QStringLiteral("a.txt"), QStringLiteral("viejo.txt → nuevo.txt")}));
        QCOMPARE(column(files, 0), QStringList({QStringLiteral("M"), QStringLiteral("R")}));
        // El primer commit (sin padre) también
        history->setCurrentItem(history->topLevelItem(2));
        QTRY_COMPARE(column(files, 1), QStringList({QStringLiteral("a.txt"), QStringLiteral("viejo.txt")}));

        // Doble clic en un archivo: su diff, en un documento de solo lectura coloreado como Diff
        history->setCurrentItem(history->topLevelItem(0));
        QTRY_COMPARE(files->topLevelItemCount(), 2);
        const int before = static_cast<int>(w.documents().count());
        emit files->itemActivated(files->topLevelItem(0), 0);
        QTRY_COMPARE(static_cast<int>(w.documents().count()), before + 1);
        const Buffer* diff = w.documents().buffer(w.currentBuffer());
        QVERIFY(diff->isReadOnly());
        QVERIFY(!diff->isDirty());
        QCOMPARE(diff->language(), LangType::Diff);
        QVERIFY(diff->displayName().contains(history->topLevelItem(0)->text(3)));
        QVERIFY(diff->displayName().contains(QLatin1String("a.txt")));
        const QString text = QString::fromStdString(w.documents().text(w.currentBuffer()));
        QVERIFY(text.contains(QLatin1String("+dos")));
        QVERIFY(!text.contains(QLatin1String("nuevo.txt")));
        // Doble clic en el commit: el diff entero
        emit history->itemActivated(history->topLevelItem(0), 0);
        QTRY_COMPARE(static_cast<int>(w.documents().count()), before + 2);
        const QString all = QString::fromStdString(w.documents().text(w.currentBuffer()));
        QVERIFY(all.contains(QLatin1String("Tercero: a.txt y renombrar")));
        QVERIFY(all.contains(QLatin1String("nuevo.txt")));
    }

    // Fuera de un repositorio: el historial vacío, sin errores
    void outsideARepository()
    {
        const QString plain = _tmp.filePath(QStringLiteral("suelto/x.txt"));
        writeFile(plain, "x\n");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.openFile(plain) != kInvalidBuffer);
        w.findChild<QAction*>(QStringLiteral("actionGitCommit"))->trigger();
        w.findChild<QTabWidget*>(QStringLiteral("gitTabs"))->setCurrentIndex(1);
        QTest::qWait(100);
        QCOMPARE(w.findChild<QTreeWidget*>(QStringLiteral("gitHistoryTree"))->topLevelItemCount(), 0);
    }
};

QTEST_MAIN(TestGitHistory)
#include "tst_githistory.moc"
