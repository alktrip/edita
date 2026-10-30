// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fragmentos de código (no existen en N++).

#include "FakePrompter.h"
#include "core/Snippet.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/QuickPicker.h"
#include "ui/Snippets.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;
using Range = std::pair<int, int>;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QString text(MainWindow& w)
{
    return QString::fromStdString(w.documents().text(w.currentBuffer()));
}

QString selected(ScintillaEditBase* v, int n = 0)
{
    const sptr_t a = v->send(SCI_GETSELECTIONNSTART, static_cast<uptr_t>(n));
    const sptr_t b = v->send(SCI_GETSELECTIONNEND, static_cast<uptr_t>(n));
    const auto* raw = reinterpret_cast<const char*>(v->send(SCI_GETRANGEPOINTER, static_cast<uptr_t>(a), b - a));
    return raw ? QString::fromUtf8(raw, static_cast<qsizetype>(b - a)) : QString();
}

} // namespace

class TestSnippets : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void parsing()
    {
        // Marcadores, espejos, anidados, el final, elecciones, variables y escapes
        auto p = snippet::parse("for (${1:i} = 0; $1 < ${2:n}; ++$1) {\n\t$0\n}");
        QCOMPARE(p.text, QStringLiteral("for (i = 0; i < n; ++i) {\n\t\n}"));
        QCOMPARE(p.tabstops.size(), std::size_t{3});
        QCOMPARE(p.tabstops[0].index, 1);
        QCOMPARE(p.tabstops[0].ranges, (std::vector<Range>{{5, 6}, {12, 13}, {21, 22}}));
        QCOMPARE(p.tabstops[1].ranges, (std::vector<Range>{{16, 17}}));
        QCOMPARE(p.tabstops[2].index, 0);
        QCOMPARE(p.tabstops[2].ranges, (std::vector<Range>{{27, 27}}));

        p = snippet::parse("${1:a ${2:b} c}$3");
        QCOMPARE(p.text, QStringLiteral("a b c"));
        QCOMPARE(p.tabstops[0].ranges, (std::vector<Range>{{0, 5}}));
        QCOMPARE(p.tabstops[1].ranges, (std::vector<Range>{{2, 3}}));
        QCOMPARE(p.tabstops[2].ranges, (std::vector<Range>{{5, 5}}));
        QCOMPARE(p.tabstops.back().ranges, (std::vector<Range>{{5, 5}})); // sin $0: al final

        const snippet::Variables vars = [](const QString& name) -> std::optional<QString> {
            if (name == "TM_FILENAME_BASE")
                return QStringLiteral("pila");
            return std::nullopt;
        };
        p = snippet::parse("${1|uno,dos|} \\$x ${TM_FILENAME_BASE}_H ${DESCONOCIDA:defecto} $NADA.", vars);
        QCOMPARE(p.text, QStringLiteral("uno $x pila_H defecto ."));

        // Archivos con comentarios, comas sobrantes, body como lista y varios prefijos
        const QString file = _tmp.filePath("f.json");
        writeFile(file, "{\n  // comentario\n  \"Uno\": {\"prefix\": [\"a\", \"b\"], \"body\": [\"x\", \"\\ty\"], \"description\": \"d\",},\n"
                        "  /* otro */ \"Dos\": {\"prefix\": \"c\", \"body\": \"z // no es comentario\"}\n}\n");
        const auto defs = snippet::loadFile(file);
        QCOMPARE(defs.size(), std::size_t{3}); // (por nombre: Dos, Uno, Uno)
        QCOMPARE(defs[0].body, QStringLiteral("z // no es comentario"));
        QCOMPARE(defs[1].body, QStringLiteral("x\n\ty"));
        QCOMPARE(defs[2].prefix, QStringLiteral("b"));
    }

    void tabExpandsAndNavigates()
    {
        writeFile(_tmp.filePath("a.cpp"), "int f()\n{\n\t\n}\n"); // (con tabuladores, los de Edita de serie)
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.openFile(_tmp.filePath("a.cpp")) != kInvalidBuffer);
        EditorView* v = w.editor();
        SnippetController* c = w.snippetControllerOfCurrentView();
        QCOMPARE(c->language(), QStringLiteral("cpp"));
        v->setFocus();
        // En la línea con sangría: «for» + Tab
        v->send(SCI_GOTOPOS, static_cast<uptr_t>(v->send(SCI_GETLINEENDPOSITION, 2)));
        QTest::keyClicks(v, "for");
        QTest::keyClick(v, Qt::Key_Tab);
        QCOMPARE(text(w), QStringLiteral("int f()\n{\n\tfor (int i = 0; i < n; ++i) {\n\t\t\n\t}\n}\n"));
        QVERIFY(c->isActive());
        QCOMPARE(c->currentIndex(), 1);
        QCOMPARE(selected(v), QStringLiteral("int"));
        // Tab: el siguiente (los tres espejos de «i» seleccionados a la vez)
        QTest::keyClick(v, Qt::Key_Tab);
        QCOMPARE(c->currentIndex(), 2);
        QCOMPARE(v->send(SCI_GETSELECTIONS), sptr_t{3});
        QTest::keyClicks(v, "k");
        QCOMPARE(text(w), QStringLiteral("int f()\n{\n\tfor (int k = 0; k < n; ++k) {\n\t\t\n\t}\n}\n"));
        QTest::keyClicks(v, "x");
        QVERIFY(text(w).contains("for (int kx = 0; kx < n; ++kx)"));
        // Mayús+Tab vuelve; Tab, Tab llega al final ($0, dentro del bucle) y se acaba
        QTest::keyClick(v, Qt::Key_Backtab);
        QCOMPARE(c->currentIndex(), 1);
        QTest::keyClick(v, Qt::Key_Tab);
        QTest::keyClick(v, Qt::Key_Tab);
        QCOMPARE(c->currentIndex(), 3);
        QCOMPARE(selected(v), QStringLiteral("n"));
        QTest::keyClick(v, Qt::Key_Tab);
        QVERIFY(!c->isActive());
        const sptr_t caret = v->send(SCI_GETCURRENTPOS);
        QCOMPARE(v->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(caret)), sptr_t{3});
        QCOMPARE(caret, v->send(SCI_GETLINEENDPOSITION, 3)); // tras los dos tabuladores
        // Un Tab sin prefijo es un Tab normal
        QTest::keyClick(v, Qt::Key_Tab);
        QVERIFY(text(w).contains("\n\t\t\t\n"));
        // Deshacer: el fragmento de una vez
        v->send(SCI_UNDO);
        v->send(SCI_UNDO);
        v->send(SCI_UNDO);
        v->send(SCI_UNDO);
        QVERIFY(!text(w).contains("for ("));
        // Queda el «for» que se escribió: «forwhile» no expande «while» (el prefijo, entero)
        v->send(SCI_GOTOPOS, static_cast<uptr_t>(v->send(SCI_GETLINEENDPOSITION, 2)));
        QTest::keyClicks(v, "while");
        QTest::keyClick(v, Qt::Key_Tab);
        QVERIFY2(!c->isActive() && text(w).contains("forwhile"), qPrintable(text(w)));
        // Esc termina antes del final
        v->send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>("int f()\n{\n\t\n}\n"));
        v->send(SCI_GOTOPOS, static_cast<uptr_t>(v->send(SCI_GETLINEENDPOSITION, 2)));
        QTest::keyClicks(v, "while");
        QTest::keyClick(v, Qt::Key_Tab);
        QVERIFY2(c->isActive(), qPrintable(text(w)));
        QTest::keyClick(v, Qt::Key_Escape);
        QVERIFY(!c->isActive());
    }

    void userSnippetsAndPicker()
    {
        // Los del usuario mandan; y la lista para elegir, con la selección dentro
        writeFile(_tmp.filePath("config/edita/snippets/python.json"),
                  "{ \"Imprimir\": {\"prefix\": \"pr\", \"body\": \"print(\\\"${1:${TM_SELECTED_TEXT}}\\\") # $TM_FILENAME\"} }");
        writeFile(_tmp.filePath("b.py"), "hola\n");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.snippetLibrary()->reload();
        QVERIFY(w.openFile(_tmp.filePath("b.py")) != kInvalidBuffer);
        EditorView* v = w.editor();
        const auto defs = w.snippetLibrary()->forLanguage("python");
        const auto pr = std::find_if(defs.begin(), defs.end(), [](const auto& d) { return d.prefix == "pr"; });
        QVERIFY(pr != defs.end());
        QCOMPARE(pr->name, QStringLiteral("Imprimir")); // en lugar del de Edita
        QVERIFY(std::any_of(defs.begin(), defs.end(), [](const auto& d) { return d.prefix == "hoy"; })); // los globales
        v->send(SCI_SETSEL, 0, 4);
        w.findChild<QAction*>("actionInsertSnippet")->trigger();
        QuickPicker* picker = w.findChild<QuickPicker*>();
        QVERIFY(picker && picker->isVisible());
        QVERIFY(picker->activate(picker->visibleTexts().filter("pr  —").value(0)));
        QCOMPARE(text(w), QStringLiteral("print(\"hola\") # b.py\n"));
        QCOMPARE(selected(v), QStringLiteral("hola"));
        // Editar los fragmentos: el archivo del usuario de ese lenguaje
        w.findChild<QAction*>("actionEditSnippets")->trigger();
        QVERIFY(w.documents().buffer(w.currentBuffer())->filePath().endsWith("snippets/python.json"));
        // Sin la preferencia, Tab no expande
        Preferences p = w.preferences();
        p.snippetsTab = false;
        w.setPreferences(p);
        QVERIFY(w.openFile(_tmp.filePath("b.py")) != kInvalidBuffer);
        v = w.editor();
        v->send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>("pr"));
        v->send(SCI_GOTOPOS, 2);
        v->setFocus();
        QTest::keyClick(v, Qt::Key_Tab);
        QVERIFY(!text(w).contains("print"));
    }
};

QTEST_MAIN(TestSnippets)
#include "tst_snippets.moc"
