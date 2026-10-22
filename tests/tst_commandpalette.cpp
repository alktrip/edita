// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Paleta de órdenes, «Ir a archivo» e «Ir a símbolo» (no existen en N++).

#include "FakePrompter.h"
#include "core/ConfigXml.h"
#include "core/FileScan.h"
#include "core/FuzzyMatch.h"
#include "core/Paths.h"
#include "ui/CommandPalette.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;
using namespace std::string_literals;

namespace {

void writeFile(const QString& path, const QByteArray& data = "x")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QAction* action(MainWindow& w, const char* name)
{
    return w.findChild<QAction*>(QString::fromLatin1(name));
}

} // namespace

class TestCommandPalette : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void fuzzyMatching()
    {
        // Palabras en cualquier orden, sin mayúsculas ni acentos
        QVERIFY(fuzzyMatch(QStringLiteral("lin ord"), QStringLiteral("Ordenar líneas")).matched());
        QVERIFY(fuzzyMatch(QStringLiteral("codificacion"), QStringLiteral("Codificación")).matched());
        QVERIFY(fuzzyMatch(QStringLiteral("ÑAN"), QStringLiteral("año nuevo")).matched() == false);
        QVERIFY(fuzzyMatch(QStringLiteral("anu"), QStringLiteral("año nuevo")).matched());
        QVERIFY(!fuzzyMatch(QStringLiteral("xyz"), QStringLiteral("Ordenar líneas")).matched());
        // Las letras, en orden
        QVERIFY(!fuzzyMatch(QStringLiteral("ba"), QStringLiteral("ab")).matched());
        // Las posiciones encontradas (para resaltarlas)
        const FuzzyMatch m = fuzzyMatch(QStringLiteral("mw"), QStringLiteral("MainWindow.cpp"));
        QCOMPARE(m.positions, (QList<int>{0, 4}));
        // Inicio de palabra y letras seguidas puntúan más
        QVERIFY(fuzzyMatch(QStringLiteral("mw"), QStringLiteral("MainWindow.cpp")).score
                > fuzzyMatch(QStringLiteral("mw"), QStringLiteral("somewhere.txt")).score);
        QVERIFY(fuzzyMatch(QStringLiteral("main"), QStringLiteral("main.cpp")).score
                > fuzzyMatch(QStringLiteral("main"), QStringLiteral("domain_info.cpp")).score);
        // camelCase solo en una «joroba» de verdad: «may» encaja mejor en MAYÚSCULAS
        QVERIFY(fuzzyMatch(QStringLiteral("may"), QStringLiteral("MAYÚSCULAS")).score
                > fuzzyMatch(QStringLiteral("may"), QStringLiteral("iNVERTIR mAYÚSCULAS")).score);
        QCOMPARE(fuzzyMatch(QStringLiteral("fb"), QStringLiteral("fooBar")).positions, (QList<int>{0, 3}));
        QVERIFY(fuzzyMatch(QStringLiteral("upper"), QStringLiteral("UPPERCASE")).score
                > fuzzyMatch(QStringLiteral("upper"), QStringLiteral("Unpublished paper")).score);
        // Vacío: coincide con todo
        QVERIFY(fuzzyMatch(QString(), QStringLiteral("algo")).matched());
    }

    void scanningSkipsHiddenFoldersAndHasALimit()
    {
        const QString root = _tmp.filePath("scan");
        writeFile(root + "/a.txt");
        writeFile(root + "/src/b.cpp");
        writeFile(root + "/.git/config");
        writeFile(root + "/node_modules/x/index.js");
        writeFile(root + "/__pycache__/m.pyc");
        writeFile(root + "/src/.hidden/c.txt");
        std::atomic_bool cancel{false};
        QStringList found;
        QVERIFY(scanFiles({root, root + "/src"}, cancel, [&](const QStringList& chunk) { found << chunk; }));
        found.sort();
        QCOMPARE(found, (QStringList{root + "/a.txt", root + "/src/b.cpp"})); // src no se recorre dos veces
        // Con un máximo: la lista queda incompleta, primero lo más cercano a la raíz
        found.clear();
        QVERIFY(!scanFiles({root}, cancel, [&](const QStringList& chunk) { found << chunk; }, {1, 10}));
        QCOMPARE(found, QStringList{root + "/a.txt"});
        // Cancelado
        cancel = true;
        found.clear();
        QVERIFY(!scanFiles({root}, cancel, [&](const QStringList& chunk) { found << chunk; }));
        QVERIFY(found.isEmpty());
    }

    void commandsAreRunFromThePalette()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(action(w, "actionCommandPalette"));
        QCOMPARE(action(w, "actionCommandPalette")->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+A")));
        QCOMPARE(action(w, "actionGoToFile")->shortcut(), QKeySequence(QStringLiteral("Ctrl+E")));
        QCOMPARE(action(w, "actionGoToSymbol")->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+O")));
        w.editor()->sends(SCI_SETTEXT, 0, "hola edita");
        w.editor()->sci(SCI_SELECTALL);

        action(w, "actionCommandPalette")->trigger();
        CommandPalette* palette = w.commandPalette();
        QVERIFY(palette);
        QVERIFY(!palette->isHidden());
        QCOMPARE(palette->input()->text(), QStringLiteral(">"));
        QVERIFY(palette->visibleTexts().size() > 100); // todo lo de los menús
        palette->input()->setText(QStringLiteral(">upper"));
        QCOMPARE(palette->visibleTexts().value(0), QStringLiteral("UPPERCASE"));
        QVERIFY(palette->activateRow(0));
        QVERIFY(palette->isHidden());
        QCOMPARE(w.documents().text(w.currentBuffer()), "HOLA EDITA"s);

        // La orden usada sale la primera la próxima vez, también en otra ventana (config.xml)
        w.showCommandPalette(QStringLiteral(">"));
        QCOMPARE(palette->visibleTexts().value(0), QStringLiteral("UPPERCASE"));
        palette->close();
        MainWindow other;
        other.setPrompter(std::make_unique<FakePrompter>());
        other.showCommandPalette(QStringLiteral(">"));
        QCOMPARE(other.commandPalette()->visibleTexts().value(0), QStringLiteral("UPPERCASE"));

        // Busca también por el menú: «view wrap» → Ver > Ajuste de línea; marca las opciones activas
        palette->input()->setText(QStringLiteral(">word wrap"));
        w.showCommandPalette(QStringLiteral(">word wrap"));
        QCOMPARE(palette->visibleTexts().value(0), QStringLiteral("Word wrap"));
        const bool wrap = action(w, "actionWrap")->isChecked();
        QVERIFY(palette->activateRow(0));
        QCOMPARE(action(w, "actionWrap")->isChecked(), !wrap);

        // Escape cierra
        w.showCommandPalette(QStringLiteral(">"));
        QTest::keyClick(palette->input(), Qt::Key_Escape);
        QVERIFY(palette->isHidden());
    }

    void symbolsAndLines()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.setLanguage(LangType::Cpp);
        w.editor()->sends(SCI_SETTEXT, 0,
                          "int suma(int a, int b)\n{\n    return a + b;\n}\n\nint resta(int a, int b)\n{\n    return a - b;\n}\n");
        w.showCommandPalette(QStringLiteral("@"));
        CommandPalette* palette = w.commandPalette();
        // Sin búsqueda, en el orden del documento
        QCOMPARE(palette->visibleTexts(), (QStringList{"suma", "resta"}));
        palette->input()->setText(QStringLiteral("@res"));
        QCOMPARE(palette->visibleTexts().value(0), QStringLiteral("resta"));
        QVERIFY(palette->activateRow(0));
        QCOMPARE(w.editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(w.editor()->sci(SCI_GETCURRENTPOS))), sptr_t{5});

        // :línea y :línea:columna
        w.showCommandPalette(QStringLiteral(":3:5"));
        QCOMPARE(palette->visibleTexts(), QStringList{QStringLiteral("Go to line 3, column 5")});
        QTest::keyClick(palette->input(), Qt::Key_Return);
        const sptr_t pos = w.editor()->sci(SCI_GETCURRENTPOS);
        QCOMPARE(w.editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos)), sptr_t{2});
        QCOMPARE(w.editor()->sci(SCI_GETCOLUMN, static_cast<uptr_t>(pos)), sptr_t{4});
        w.showCommandPalette(QStringLiteral(":abc"));
        QCOMPARE(palette->visibleTexts(), QStringList{QStringLiteral("Type a line number")});
        QVERIFY(palette->activateRow(0)); // no hace nada
    }

    void filesFromTheWorkspaceAndRecentFiles()
    {
        const QString root = _tmp.filePath("proyecto");
        writeFile(root + "/README.md", "# Proyecto");
        writeFile(root + "/src/MainWindow.cpp", "// ventana");
        writeFile(root + "/src/notas.txt", "notas");
        writeFile(root + "/.git/HEAD", "ref");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.addFolderToWorkspace(root));
        QVERIFY(w.openFile(root + "/README.md") != kInvalidBuffer);
        w.showCommandPalette(QString());
        CommandPalette* palette = w.commandPalette();
        // Los archivos de la carpeta de trabajo llegan desde otro hilo
        QTRY_VERIFY(palette->visibleTexts().contains(QStringLiteral("MainWindow.cpp")));
        QVERIFY(!palette->visibleTexts().contains(QStringLiteral("HEAD")));
        QCOMPARE(palette->visibleTexts().count(QStringLiteral("README.md")), 1); // abierto: una sola vez
        palette->input()->setText(QStringLiteral("mwin"));
        QCOMPARE(palette->visibleTexts().value(0), QStringLiteral("MainWindow.cpp"));
        QTest::keyClick(palette->input(), Qt::Key_Return);
        QCOMPARE(QFileInfo(w.documents().buffer(w.currentBuffer())->filePath()).fileName(), QStringLiteral("MainWindow.cpp"));
        // Las flechas mueven la selección (y dan la vuelta)
        w.showCommandPalette(QStringLiteral("txt"));
        QTRY_COMPARE(palette->visibleTexts().value(0), QStringLiteral("notas.txt"));
        QTest::keyClick(palette->input(), Qt::Key_Up);
        QTest::keyClick(palette->input(), Qt::Key_Down);
        QTest::keyClick(palette->input(), Qt::Key_Return);
        QCOMPARE(QFileInfo(w.documents().buffer(w.currentBuffer())->filePath()).fileName(), QStringLiteral("notas.txt"));
    }
};

QTEST_MAIN(TestCommandPalette)
#include "tst_commandpalette.moc"
