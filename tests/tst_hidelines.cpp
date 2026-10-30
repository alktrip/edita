// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ver › Ocultar líneas (ScintillaEditView::hideLines, hidelineMarkerClicked,
// restoreHiddenLines de N++).

#include "FakePrompter.h"
#include "ui/EditCommands.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

bool visible(EditorView* v, int line)
{
    return v->sci(SCI_GETLINEVISIBLE, static_cast<uptr_t>(line)) != 0;
}

bool hasMarker(EditorView* v, int line, int marker)
{
    return (v->sci(SCI_MARKERGET, static_cast<uptr_t>(line)) & (1 << marker)) != 0;
}

QList<int> markedLines(EditorView* v, int marker)
{
    QList<int> lines;
    for (sptr_t l = v->sci(SCI_MARKERNEXT, 0, 1 << marker); l >= 0; l = v->sci(SCI_MARKERNEXT, static_cast<uptr_t>(l + 1), 1 << marker))
        lines << static_cast<int>(l);
    return lines;
}

void selectLines(EditorView* v, int first, int last)
{
    v->sci(SCI_SETSEL, static_cast<uptr_t>(v->sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(first))),
           v->sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(last)));
}

} // namespace

class TestHideLines : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    std::unique_ptr<MainWindow> window(const char* text)
    {
        auto w = std::make_unique<MainWindow>();
        w->setPrompter(std::make_unique<FakePrompter>());
        w->editor()->sends(SCI_SETTEXT, 0, text);
        return w;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void hidesTheSelectedLines()
    {
        auto w = window("0\n1\n2\n3\n4\n5\n");
        EditorView* v = w->editor();
        selectLines(v, 1, 3);
        w->findChild<QAction*>(QStringLiteral("actionHideLines"))->trigger();
        QVERIFY(visible(v, 0) && !visible(v, 1) && !visible(v, 2) && !visible(v, 3) && visible(v, 4));
        QCOMPARE(markedLines(v, kHideLinesBeginMarker), QList<int>{0});
        QCOMPARE(markedLines(v, kHideLinesEndMarker), QList<int>{4});
        // Ni la primera ni la última línea
        auto edges = window("0\n1\n2\n3");
        EditorView* e = edges->editor();
        selectLines(e, 0, 3);
        hideLines(*e);
        QVERIFY(visible(e, 0) && !visible(e, 1) && !visible(e, 2) && visible(e, 3));
        // Con menos de tres líneas, nada
        auto tiny = window("a\nb");
        selectLines(tiny->editor(), 0, 1);
        hideLines(*tiny->editor());
        QVERIFY(markedLines(tiny->editor(), kHideLinesBeginMarker).isEmpty());
    }

    void adjacentSectionsMerge()
    {
        auto w = window("0\n1\n2\n3\n4\n5\n6\n7\n");
        EditorView* v = w->editor();
        selectLines(v, 1, 2);
        hideLines(*v);
        selectLines(v, 3, 4);
        hideLines(*v);
        QCOMPARE(markedLines(v, kHideLinesBeginMarker), QList<int>{0});
        QCOMPARE(markedLines(v, kHideLinesEndMarker), QList<int>{5});
        for (int line = 1; line <= 4; ++line)
            QVERIFY(!visible(v, line));
        // Dos secciones separadas por una línea, y luego esa línea: una sola
        auto two = window("0\n1\n2\n3\n4\n5\n6\n");
        EditorView* t = two->editor();
        selectLines(t, 1, 1);
        hideLines(*t);
        selectLines(t, 3, 3);
        hideLines(*t);
        QCOMPARE(markedLines(t, kHideLinesBeginMarker), (QList<int>{0, 2}));
        selectLines(t, 2, 2);
        hideLines(*t);
        QCOMPARE(markedLines(t, kHideLinesBeginMarker), QList<int>{0});
        QCOMPARE(markedLines(t, kHideLinesEndMarker), QList<int>{4});
    }

    void clickingAMarkerShowsTheSection()
    {
        auto w = window("0\n1\n2\n3\n4\n5\n");
        EditorView* v = w->editor();
        selectLines(v, 1, 2);
        hideLines(*v);
        // Un clic en el margen de los marcadores, en la marca de abajo
        emit v->marginClicked(v->sci(SCI_POSITIONFROMLINE, 3), Scintilla::KeyMod::Norm, static_cast<int>(kMarginSymbol));
        QVERIFY(visible(v, 1) && visible(v, 2));
        QVERIFY(markedLines(v, kHideLinesBeginMarker).isEmpty() && markedLines(v, kHideLinesEndMarker).isEmpty());
        QVERIFY(!hasMarker(v, 3, kBookmarkMarker)); // no pone un marcador
        // Con la de arriba
        selectLines(v, 2, 3);
        hideLines(*v);
        QVERIFY(showHiddenSection(*v, 1));
        QVERIFY(visible(v, 2) && visible(v, 3));
        QVERIFY(!showHiddenSection(*v, 1));
    }

    void survivesSwitchingDocumentsAndUnfolding()
    {
        const QString a = _tmp.filePath("a.c");
        const QString b = _tmp.filePath("b.c");
        writeFile(a, "int f()\n{\n    int x = 1;\n    int y = 2;\n    return x + y;\n}\n");
        writeFile(b, "otro\n");
        auto w = window("");
        w->openFile(a);
        EditorView* v = w->editor();
        v->send(SCI_COLOURISE, 0, -1);
        selectLines(v, 2, 3);
        hideLines(*v);
        QVERIFY(!visible(v, 2) && !visible(v, 3));
        // Otro documento en la vista y de vuelta: siguen ocultas
        w->openFile(b);
        w->openFile(a);
        QVERIFY(!visible(v, 2) && !visible(v, 3) && visible(v, 4));
        // Plegar y desplegar la función con el margen: lo oculto sigue oculto
        const sptr_t header = 1;
        QVERIFY(v->sci(SCI_GETFOLDLEVEL, header) & SC_FOLDLEVELHEADERFLAG);
        emit v->marginClicked(v->sci(SCI_POSITIONFROMLINE, header), Scintilla::KeyMod::Norm, static_cast<int>(kMarginFolder));
        QVERIFY(!v->sci(SCI_GETFOLDEXPANDED, header));
        QVERIFY(!visible(v, 4));
        emit v->marginClicked(v->sci(SCI_POSITIONFROMLINE, header), Scintilla::KeyMod::Norm, static_cast<int>(kMarginFolder));
        QVERIFY(v->sci(SCI_GETFOLDEXPANDED, header));
        QVERIFY(visible(v, 4) && !visible(v, 2) && !visible(v, 3));
        // Y con Plegar todo / Desplegar todo
        w->findChild<QAction*>(QStringLiteral("actionFoldAll"))->trigger();
        w->findChild<QAction*>(QStringLiteral("actionUnfoldAll"))->trigger();
        QVERIFY(visible(v, 4) && !visible(v, 2));
    }

    void altHShortcut()
    {
        auto w = window("0\n1\n2\n3\n");
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.get()));
        EditorView* v = w->editor();
        v->setFocus();
        selectLines(v, 1, 2);
        QTest::keyClick(v, Qt::Key_H, Qt::AltModifier);
        QVERIFY(!visible(v, 1) && !visible(v, 2));
    }
};

QTEST_MAIN(TestHideLines)
#include "tst_hidelines.moc"
