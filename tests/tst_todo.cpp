// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 96: el panel TODO (Ver › TODO). Los comentarios con TODO, FIXME, HACK, XXX, BUG o NOTE
// del documento actual o de las carpetas de trabajo.

#include "FakePrompter.h"
#include "core/TodoScan.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/TodoPanel.h"

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace edita;
using edita::test::FakePrompter;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QStringList describe(const std::vector<todo::Item>& items)
{
    QStringList out;
    for (const todo::Item& i : items)
        out << QStringLiteral("%1:%2 %3 %4").arg(i.line).arg(i.column).arg(i.kind, i.text);
    return out;
}

// Los elementos del árbol: «archivo» y, debajo, «línea KIND texto»
QStringList treeText(QTreeWidget* tree)
{
    QStringList out;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* file = tree->topLevelItem(i);
        for (int j = 0; j < file->childCount(); ++j) {
            QTreeWidgetItem* item = file->child(j);
            out << QStringLiteral("%1:%2 %3 %4").arg(file->text(0), item->text(2), item->text(0), item->text(1));
        }
    }
    return out;
}

} // namespace

class TestTodo : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    // --- El núcleo -----------------------------------------------------------------------

    void findsCommentsInManyLanguages()
    {
        const QString text = QStringLiteral(
            "int x = 0; // TODO: usar el valor real\n"
            "# FIXME(ana) esto falla con UTF-16\n"
            "/* HACK: hasta que se arregle */\n"
            " * XXX revisar\n"
            "-- NOTE: en SQL\n"
            "<!-- BUG: en HTML -->\n"
            "; TODO en ini\n"
            "    // TODO\n");
        QCOMPARE(describe(todo::scanText(text)),
                 QStringList({QStringLiteral("0:14 TODO usar el valor real"), QStringLiteral("1:2 FIXME (ana) esto falla con UTF-16"),
                              QStringLiteral("2:3 HACK hasta que se arregle"), QStringLiteral("3:3 XXX revisar"),
                              QStringLiteral("4:3 NOTE en SQL"), QStringLiteral("5:5 BUG en HTML"), QStringLiteral("6:2 TODO en ini"),
                              QStringLiteral("7:7 TODO ")}));
    }

    void ignoresCodeAndProse()
    {
        const QString text = QStringLiteral(
            "TODO_LIST = []\n"              // un identificador
            "todos = load()  # todos\n"     // minúsculas
            "print(\"TODO: no\")\n"         // en una cadena, sin comentario antes
            "Estos son TODOS los casos.\n"  // texto normal
            "x = 1 // TODOS\n"              // no es la palabra entera
            "url = 'http://x/TODO'\n");     // dentro de una URL (// sin espacio después)
        QCOMPARE(describe(todo::scanText(text)), QStringList());
    }

    void scansFiles()
    {
        const QString text = _tmp.filePath(QStringLiteral("a.c"));
        writeFile(text, "// TODO uno\nint a;\n// FIXME dos\n");
        const auto items = todo::scanFile(text);
        QCOMPARE(items.size(), size_t{2});
        QCOMPARE(items[0].path, text);
        QCOMPARE(items[1].line, 2);
        // Binarios y archivos grandes: nada
        const QString binary = _tmp.filePath(QStringLiteral("b.bin"));
        writeFile(binary, QByteArray("// TODO\0\1\2", 10));
        QVERIFY(todo::scanFile(binary).empty());
        QVERIFY(todo::scanFile(text, 10).empty());
        QVERIFY(todo::scanFile(_tmp.filePath(QStringLiteral("no-existe"))).empty());
    }

    // --- El panel ------------------------------------------------------------------------

    void documentScopeFollowsTheText()
    {
        const QString path = _tmp.filePath(QStringLiteral("doc/main.py"));
        writeFile(path, "def f():\n    pass  # TODO: implementar\n# FIXME: nombre\n");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.openFile(path) != kInvalidBuffer);
        QAction* action = w.findChild<QAction*>(QStringLiteral("actionTodo"));
        QVERIFY(action && action->isCheckable());
        action->trigger();
        QVERIFY(w.panelDock(QStringLiteral("todo"))->isVisibleTo(&w));
        auto* tree = w.findChild<QTreeWidget*>(QStringLiteral("todoTree"));
        QVERIFY(tree);
        QTRY_COMPARE(treeText(tree), QStringList({QStringLiteral("main.py:2 TODO implementar"), QStringLiteral("main.py:3 FIXME nombre")}));
        // Al escribir, se actualiza
        w.editor()->sci(SCI_DOCUMENTEND);
        w.editor()->sci(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>("# HACK: provisional\n"));
        QTRY_COMPARE(treeText(tree).size(), 3);
        QVERIFY(treeText(tree).contains(QStringLiteral("main.py:4 HACK provisional")));
        // Doble clic: a esa línea
        w.editor()->sci(SCI_GOTOPOS, 0);
        QTreeWidgetItem* fixme = tree->topLevelItem(0)->child(1);
        emit tree->itemActivated(fixme, 0);
        QCOMPARE(w.editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(w.editor()->sci(SCI_GETCURRENTPOS))), sptr_t{2});
    }

    void workspaceScopeScansTheFolders()
    {
        const QString root = _tmp.filePath(QStringLiteral("proyecto"));
        writeFile(root + QStringLiteral("/a.py"), "# TODO: uno\n");
        writeFile(root + QStringLiteral("/src/b.c"), "int b;\n// FIXME dos\n");
        writeFile(root + QStringLiteral("/node_modules/x.js"), "// TODO no se mira\n");
        writeFile(root + QStringLiteral("/.git/config"), "# TODO tampoco\n");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.addFolderToWorkspace(root));
        // a.py abierto y cambiado sin guardar: cuenta su texto actual
        QVERIFY(w.openFile(root + QStringLiteral("/a.py")) != kInvalidBuffer);
        w.editor()->sci(SCI_DOCUMENTEND);
        w.editor()->sci(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>("# NOTE: tres\n"));
        w.findChild<QAction*>(QStringLiteral("actionTodo"))->trigger();
        auto* scope = w.findChild<QComboBox*>(QStringLiteral("todoScope"));
        auto* tree = w.findChild<QTreeWidget*>(QStringLiteral("todoTree"));
        scope->setCurrentIndex(1); // las carpetas de trabajo
        QTRY_COMPARE(treeText(tree), QStringList({QStringLiteral("a.py:1 TODO uno"), QStringLiteral("a.py:2 NOTE tres"),
                                                  QStringLiteral("src/b.c:2 FIXME dos")}));
        // Doble clic: abre el archivo en esa línea
        emit tree->itemActivated(tree->topLevelItem(1)->child(0), 0);
        QTRY_COMPARE(QFileInfo(w.documents().buffer(w.currentBuffer())->filePath()).fileName(), QStringLiteral("b.c"));
        QCOMPARE(w.editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(w.editor()->sci(SCI_GETCURRENTPOS))), sptr_t{1});
        // El alcance se recuerda
        QCOMPARE(w.syntax().config().guiConfig(QStringLiteral("EditaTodo"), QStringLiteral("scope")), QStringLiteral("workspace"));
    }
};

QTEST_MAIN(TestTodo)
#include "tst_todo.moc"
