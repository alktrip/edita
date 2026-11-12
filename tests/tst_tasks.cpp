// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 84: las tareas del proyecto (Makefile, CMake, package.json…) y los errores de su salida,
// en el panel Problemas y con un doble clic hasta la línea (no existe en N++)

#include "FakePrompter.h"
#include "core/Tasks.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/ProblemsPanel.h"
#include "ui/QuickPicker.h"
#include "ui/TaskOutputPanel.h"
#include "ui/TaskRunner.h"

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTreeWidget>

using namespace edita;
using namespace edita::tasks;
using edita::test::FakePrompter;

namespace {

void writeFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

QStringList labels(const QList<Task>& tasks)
{
    QStringList result;
    for (const Task& t : tasks)
        result << t.label;
    return result;
}

} // namespace

class TestTasks : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    QString dir(const QString& name) const { return _tmp.filePath(name); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_UPDATE_URL", "none");
    }

    // --- Detectar ----------------------------------------------------------------------

    void projectRootIsTheNearestMarker()
    {
        writeFile(dir("p1/Makefile"), "all:\n");
        writeFile(dir("p1/src/deep/a.c"), "");
        QCOMPARE(findProjectRoot(dir("p1/src/deep/a.c")), QDir::cleanPath(dir("p1")));
        QCOMPARE(findProjectRoot(dir("p1/src")), QDir::cleanPath(dir("p1")));
        // Un proyecto dentro de otro: el de dentro
        writeFile(dir("p1/sub/package.json"), "{}");
        writeFile(dir("p1/sub/index.js"), "");
        QCOMPARE(findProjectRoot(dir("p1/sub/index.js")), QDir::cleanPath(dir("p1/sub")));
    }

    void makeTargets()
    {
        const QString makefile = QStringLiteral(".PHONY: all test\n"
                                                "CC := gcc\n"
                                                "VAR = x\n"
                                                "all: app\n"
                                                "app: main.o util.o\n\t$(CC) -o $@ $^\n"
                                                "%.o: %.c\n\t$(CC) -c $<\n"
                                                "test check: app\n\t./app --test\n"
                                                "clean::\n\trm -f app\n"
                                                "\techo not: a target\n");
        QCOMPARE(tasks::makeTargets(makefile), (QStringList{QStringLiteral("all"), QStringLiteral("app"), QStringLiteral("test"),
                                                           QStringLiteral("check")}));
    }

    void detectsEachKindOfProject()
    {
        writeFile(dir("all/Makefile"), "build:\n\ttrue\n");
        writeFile(dir("all/CMakeLists.txt"), "project(x)\n");
        writeFile(dir("all/build/CMakeCache.txt"), "");
        writeFile(dir("all/package.json"), R"({"scripts": {"lint": "eslint .", "build": "tsc"}})");
        writeFile(dir("all/yarn.lock"), "");
        writeFile(dir("all/Cargo.toml"), "[package]\n");
        writeFile(dir("all/go.mod"), "module x\n");
        writeFile(dir("all/pyproject.toml"), "[project]\n");
        writeFile(dir("all/justfile"), "set shell := [\"bash\"]\ndefault: build\nbuild target='x':\n    echo\n");
        const QStringList found = labels(detectTasks(dir("all")));
        for (const char* expected : {"make", "make build", "cmake --build build", "ctest --test-dir build --output-on-failure",
                                     "yarn run build", "yarn run lint", "cargo build", "cargo test", "go test ./...",
                                     "python3 -m pytest", "just default", "just build"}) {
            QVERIFY2(found.contains(QLatin1String(expected)), expected);
        }
        QVERIFY(!found.contains(QStringLiteral("just set")));
        // CMake sin configurar: primero configurar
        writeFile(dir("fresh/CMakeLists.txt"), "project(x)\n");
        QCOMPARE(labels(detectTasks(dir("fresh"))).value(0), QStringLiteral("cmake -S . -B build"));
        QVERIFY(detectTasks(QString()).isEmpty());
        for (const Task& t : detectTasks(dir("all")))
            QCOMPARE(t.cwd, QDir(dir("all")).absolutePath());
    }

    // --- Leer los errores ----------------------------------------------------------------

    void parsesCompilerOutput()
    {
        writeFile(dir("out/src/main.c"), "");
        writeFile(dir("out/app.ts"), "");
        writeFile(dir("out/lib.rs"), "");
        writeFile(dir("out/tool.py"), "");
        ProblemParser parser(dir("out"));
        // GCC / Clang, con colores
        auto p = parser.feed(QStringLiteral("\x1b[01m\x1b[Ksrc/main.c:3:5:\x1b[m\x1b[K \x1b[01;31m\x1b[Kerror: \x1b[m\x1b[K'x' undeclared"));
        QVERIFY(p);
        QCOMPARE(p->file, QDir::cleanPath(dir("out/src/main.c")));
        QCOMPARE(p->line, 3);
        QCOMPARE(p->column, 5);
        QCOMPARE(p->severity, 1);
        QCOMPARE(p->message, QStringLiteral("'x' undeclared"));
        p = parser.feed(QStringLiteral("src/main.c:7:1: warning: unused variable 'y' [-Wunused-variable]"));
        QVERIFY(p && p->severity == 2 && p->line == 7);
        // Go (sin «error:»)
        p = parser.feed(QStringLiteral("./src/main.c:12:2: undefined: foo"));
        QVERIFY(p && p->severity == 1 && p->message == QStringLiteral("undefined: foo"));
        // tsc
        p = parser.feed(QStringLiteral("app.ts(4,10): error TS2322: Type 'string' is not assignable to type 'number'."));
        QVERIFY(p && p->line == 4 && p->column == 10 && p->code == QStringLiteral("TS2322"));
        // rustc: la cabecera y la flecha
        QVERIFY(!parser.feed(QStringLiteral("error[E0308]: mismatched types")));
        p = parser.feed(QStringLiteral("  --> lib.rs:8:17"));
        QVERIFY(p && p->line == 8 && p->column == 17 && p->code == QStringLiteral("E0308") && p->message == QStringLiteral("mismatched types"));
        // Python: el último marco de la traza con el error
        QVERIFY(!parser.feed(QStringLiteral("Traceback (most recent call last):")));
        QVERIFY(!parser.feed(QStringLiteral("  File \"%1\", line 2, in <module>").arg(dir("out/tool.py"))));
        QVERIFY(!parser.feed(QStringLiteral("  File \"tool.py\", line 9, in run")));
        QVERIFY(!parser.feed(QStringLiteral("    raise ValueError('bad')")));
        p = parser.feed(QStringLiteral("ValueError: bad"));
        QVERIFY(p && p->line == 9 && p->message == QStringLiteral("ValueError: bad"));
        // pytest
        p = parser.feed(QStringLiteral("tool.py:14: AssertionError"));
        QVERIFY(p && p->line == 14 && p->column == 0);
    }

    // Lo que parece una posición pero no es un archivo del proyecto
    void ignoresWhatIsNotAFile()
    {
        ProblemParser parser(dir("out"));
        QVERIFY(!parser.feed(QStringLiteral("Downloading http://example.com:80:1: done")));
        QVERIFY(!parser.feed(QStringLiteral("missing.c:3:1: error: nope")));
        QVERIFY(!parser.feed(QStringLiteral("make: *** [Makefile:3: all] Error 1")));
        QVERIFY(!parser.feed(QStringLiteral("12:30:01 build finished")));
    }

    void stripsAnsi()
    {
        QCOMPARE(stripAnsi(QStringLiteral("\x1b[1;31mred\x1b[0m \x1b]8;;http://x\x07link\x1b]8;;\x07")), QStringLiteral("red link"));
    }

    // --- Ejecutar ----------------------------------------------------------------------

    void runnerReportsLinesProblemsAndExitCode()
    {
        writeFile(dir("run/a.c"), "int main() {}\n");
        TaskRunner runner;
        QSignalSpy lines(&runner, &TaskRunner::line);
        QSignalSpy problems(&runner, &TaskRunner::problem);
        QSignalSpy finished(&runner, &TaskRunner::finished);
        runner.start({QStringLiteral("x"), QStringLiteral("echo hola; printf 'a.c:1:5: error: malo\\n' >&2; printf 'sin salto'; exit 3"),
                      dir("run"), QString()});
        QVERIFY(finished.wait(10000));
        QCOMPARE(finished.first().at(0).toInt(), 3);
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines.at(2).at(0).toString(), QStringLiteral("sin salto"));
        QCOMPARE(problems.size(), 1);
        QVERIFY(!runner.isRunning());
    }

    // Detener termina también lo que lanzó la shell
    void stopKillsTheWholeGroup()
    {
        TaskRunner runner;
        QSignalSpy finished(&runner, &TaskRunner::finished);
        const QString marker = dir("stopped.txt");
        runner.start({QStringLiteral("x"), QStringLiteral("(sleep 3; touch '%1') & sleep 30").arg(marker), _tmp.path(), QString()});
        QTRY_VERIFY(runner.isRunning());
        QTest::qWait(200);
        runner.stop();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(0).toInt(), -1);
        QTest::qWait(3500);
        QVERIFY(!QFileInfo::exists(marker));
    }

    // --- En la ventana -------------------------------------------------------------------

    void runFromTheMenuShowsTheErrors()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("make")).isEmpty())
            QSKIP("sin make");
        const QString project = dir("proj");
        writeFile(project + QStringLiteral("/src/util.c"), "int f() {\n    return x;\n}\n");
        writeFile(project + QStringLiteral("/Makefile"),
                  "build:\n\t@echo compiling\n\t@echo 'src/util.c:2:12: error: x undeclared' >&2\n\t@echo 'src/util.c:1:1: warning: old style' >&2\n\t@exit 1\n");
        auto w = std::make_unique<MainWindow>();
        auto fake = std::make_unique<FakePrompter>();
        FakePrompter* prompter = fake.get();
        w->setPrompter(std::move(fake));
        QVERIFY(w->openFile(project + QStringLiteral("/Makefile")) != kInvalidBuffer);
        QCOMPARE(w->taskRoot(), QDir::cleanPath(project));
        w->findChild<QAction*>(QStringLiteral("actionRunTask"))->trigger();
        QuickPicker* picker = w->statusPicker();
        QVERIFY(picker);
        QVERIFY(picker->visibleTexts().contains(QStringLiteral("make build")));
        QVERIFY(picker->activate(QStringLiteral("make build")));
        QSignalSpy finished(w->taskRunner(), &TaskRunner::finished);
        QVERIFY(finished.wait(15000));
        auto* dock = w->findChild<QDockWidget*>(QStringLiteral("taskOutputDock"));
        QVERIFY(dock && dock->isVisibleTo(w.get()));
        TaskOutputPanel* panel = w->taskOutputPanel();
        QVERIFY(panel->output()->toPlainText().contains(QStringLiteral("compiling")));
        QVERIFY(panel->status()->text().contains(QStringLiteral("1 error")));
        // En Problemas, con los del servidor de lenguaje
        QTRY_COMPARE(w->problemsPanel()->tree()->topLevelItemCount(), 1);
        QCOMPARE(w->problemsPanel()->tree()->topLevelItem(0)->childCount(), 2);
        // Doble clic en la línea del error: el archivo, en esa línea
        int errorLine = -1;
        for (QTextBlock b = panel->output()->document()->begin(); b.isValid(); b = b.next()) {
            if (b.text().contains(QStringLiteral("x undeclared")))
                errorLine = b.blockNumber();
        }
        QVERIFY(errorLine >= 0);
        QVERIFY(panel->activateLine(errorLine));
        QTRY_VERIFY(w->documents().buffer(w->currentBuffer())->filePath().endsWith(QStringLiteral("src/util.c")));
        QCOMPARE(w->editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(w->editor()->sci(SCI_GETCURRENTPOS))), sptr_t{1});
        // Repetir la última: limpia lo anterior
        w->findChild<QAction*>(QStringLiteral("actionRunLastTask"))->trigger();
        QVERIFY(finished.wait(15000));
        QCOMPARE(w->problemsPanel()->tree()->topLevelItem(0)->childCount(), 2);
        QVERIFY(prompter->informations.isEmpty());
    }

    void noProjectSaysSo()
    {
        auto w = std::make_unique<MainWindow>();
        auto fake = std::make_unique<FakePrompter>();
        FakePrompter* prompter = fake.get();
        w->setPrompter(std::move(fake));
        writeFile(dir("lonely/x.txt"), "x");
        w->openFile(dir("lonely/x.txt"));
        if (!w->taskRoot().isEmpty())
            QSKIP("la carpeta temporal está dentro de un proyecto");
        w->findChild<QAction*>(QStringLiteral("actionRunTask"))->trigger();
        QCOMPARE(prompter->informations.size(), 1);
    }
};

QTEST_MAIN(TestTasks)
#include "tst_tasks.moc"
