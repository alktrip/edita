// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Buscar en línea y mapa del documento mejorado (no existen en N++).

#include "FakePrompter.h"
#include "syntax/SyntaxStyler.h"
#include "ui/EditorView.h"
#include "ui/FindBar.h"
#include "ui/FindReplaceDialog.h"
#include "ui/MainWindow.h"
#include "ui/Panels.h"

#include <QAction>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

using namespace edita;
using edita::test::FakePrompter;
using namespace std::string_literals;

class TestFindBar : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    static QAction* action(MainWindow& w, const char* name) { return w.findChild<QAction*>(QString::fromLatin1(name)); }
    static sptr_t selStart(MainWindow& w) { return w.editor()->sci(SCI_GETSELECTIONSTART); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void findWhileTyping()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.newDocument();
        w.editor()->sends(SCI_SETTEXT, 0, "uno dos tres Dos\ndos cuatro dosis\n");
        w.editor()->sci(SCI_GOTOPOS, 0);
        // Ctrl+F: la barra, no el diálogo
        action(w, "actionFind")->trigger();
        FindBar* bar = w.findBarOfCurrentView();
        QVERIFY(bar && bar->isOpen());
        QVERIFY(!w.findChild<FindReplaceDialog*>() || w.findChild<FindReplaceDialog*>()->isHidden());
        // Mientras se escribe: todas resaltadas, la primera seleccionada, «1 de 5»
        bar->findField()->setText(QStringLiteral("dos"));
        QTRY_COMPARE(bar->countText(), QStringLiteral("1 of 4")); // dos, Dos, dos, dos(is)
        QCOMPARE(selStart(w), sptr_t{4});
        QVERIFY(w.editor()->sci(SCI_INDICATORVALUEAT, kIndicatorIncremental, 18) != 0); // la de la segunda línea
        // Intro siguiente, Mayús+Intro anterior (dando la vuelta)
        QTest::keyClick(bar->findField(), Qt::Key_Return);
        QCOMPARE(bar->countText(), QStringLiteral("2 of 4"));
        QTest::keyClick(bar->findField(), Qt::Key_Return, Qt::ShiftModifier);
        QTest::keyClick(bar->findField(), Qt::Key_Return, Qt::ShiftModifier);
        QCOMPARE(bar->countText(), QStringLiteral("4 of 4"));
        // Mayúsculas (Alt+C) y palabra completa (Alt+W)
        QTest::keyClick(bar->findField(), Qt::Key_C, Qt::AltModifier);
        QTRY_COMPARE(bar->countText(), QStringLiteral("1 of 3"));
        QTest::keyClick(bar->findField(), Qt::Key_W, Qt::AltModifier);
        QTRY_COMPARE(bar->countText(), QStringLiteral("1 of 2"));
        // Expresión regular (Alt+R); una que no vale, en rojo
        bar->setToggles(false, false, false);
        QTest::keyClick(bar->findField(), Qt::Key_R, Qt::AltModifier);
        bar->findField()->setText(QStringLiteral("d.s\\b"));
        QTRY_COMPARE(bar->countText(), QStringLiteral("1 of 3")); // dos, Dos, dos (no «dosis»)
        bar->findField()->setText(QStringLiteral("(dos"));
        QTRY_COMPARE(bar->countText(), QStringLiteral("Invalid regex"));
        bar->findField()->setText(QStringLiteral("zzz"));
        QTRY_COMPARE(bar->countText(), QStringLiteral("No results"));
        // F3 sigue a lo último de la barra
        bar->setToggles(true, true, false);
        bar->findField()->setText(QStringLiteral("dos"));
        QTRY_COMPARE(bar->countText(), QStringLiteral("1 of 2"));
        w.editor()->sci(SCI_GOTOPOS, 0);
        action(w, "actionFindNext")->trigger();
        QCOMPARE(selStart(w), sptr_t{4});
        action(w, "actionFindNext")->trigger();
        QCOMPARE(selStart(w), sptr_t{17});
        // Esc cierra y quita el resaltado
        QTest::keyClick(bar->findField(), Qt::Key_Escape);
        QVERIFY(!bar->isOpen());
        QCOMPARE(w.editor()->sci(SCI_INDICATORVALUEAT, kIndicatorIncremental, 18), sptr_t{0});
    }

    void replaceAndMoreOptions()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.newDocument();
        w.editor()->sends(SCI_SETTEXT, 0, "a-b a-b a-b");
        w.editor()->sci(SCI_GOTOPOS, 0);
        action(w, "actionReplace")->trigger(); // Ctrl+H: con la fila de reemplazar
        FindBar* bar = w.findBarOfCurrentView();
        QVERIFY(bar->isOpen() && bar->replaceField()->isVisible() == bar->isVisible());
        // (abre con la palabra del cursor, «a»; Intro justo después de escribir ya vale)
        bar->findField()->setText(QStringLiteral("a-b"));
        bar->replaceField()->setText(QStringLiteral("X"));
        // Intro en reemplazar: una y pasa a la siguiente
        QTest::keyClick(bar->replaceField(), Qt::Key_Return);
        QCOMPARE(w.documents().text(w.currentBuffer()), "X a-b a-b"s);
        QCOMPARE(bar->replaceAll(), 2);
        QCOMPARE(w.documents().text(w.currentBuffer()), "X X X"s);
        QCOMPARE(bar->countText(), QStringLiteral("2 replaced"));
        // Deshacer «todas» de una vez
        w.editor()->sci(SCI_UNDO);
        QCOMPARE(w.documents().text(w.currentBuffer()), "X a-b a-b"s);
        // «⋯»: el diálogo de N++ con el mismo texto
        bar->findChild<QToolButton*>(QStringLiteral("findBarMore"))->click();
        FindReplaceDialog* dialog = w.findChild<FindReplaceDialog*>();
        QVERIFY(dialog && dialog->isVisible());
        QCOMPARE(dialog->options().pattern, "a-b"s);
        dialog->hide();
        // Sin la preferencia, Ctrl+F abre el diálogo como en N++
        bar->close();
        Preferences p = w.preferences();
        p.inlineFind = false;
        w.setPreferences(p);
        action(w, "actionFind")->trigger();
        QVERIFY(dialog->isVisible());
        QVERIFY(!bar->isOpen());
    }

    void documentMapShowsMarks()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.newDocument();
        QByteArray text;
        for (int i = 0; i < 200; ++i)
            text += "linea " + QByteArray::number(i) + "\n";
        w.editor()->sends(SCI_SETTEXT, 0, text.constData());
        action(w, "actionDocumentMap")->trigger();
        auto* map = w.findChild<DocumentMapPanel*>();
        QVERIFY(map);
        QTRY_VERIFY(map->map()->send(SCI_GETDOCPOINTER) == w.editor()->sci(SCI_GETDOCPOINTER));
        // Un margen con los cambios de Git y los marcadores
        QCOMPARE(map->map()->send(SCI_GETMARGINWIDTHN, 0), sptr_t{4});
        QVERIFY(map->map()->send(SCI_GETMARGINMASKN, 0) & (1 << 20));
        // Las búsquedas, con relleno para que se vean en miniatura
        QCOMPARE(map->map()->send(SCI_INDICGETSTYLE, kIndicatorFound), sptr_t{INDIC_FULLBOX});
        QCOMPARE(map->map()->send(SCI_INDICGETSTYLE, kIndicatorIncremental), sptr_t{INDIC_FULLBOX});
        // La capa de la zona cubre todo el mapa
        QWidget* zone = map->findChild<QWidget*>(QStringLiteral("documentMapZone"));
        QVERIFY(zone);
        QTRY_COMPARE(zone->geometry(), map->map()->viewport()->rect());
    }
};

QTEST_MAIN(TestFindBar)
#include "tst_findbar.moc"
