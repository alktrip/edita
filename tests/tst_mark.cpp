// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pestaña Marcar del diálogo Buscar y Buscar › Copiar texto con estilo
// (FindReplaceDlg::processAll(ProcessMarkAll), clearMarks, markedTextToClipboard).

#include "FakePrompter.h"
#include "syntax/SyntaxStyler.h"
#include "ui/EditCommands.h"
#include "ui/EditorView.h"
#include "ui/FindReplaceDialog.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

namespace {

// Los tramos marcados con ese indicador, como (inicio, fin)
std::vector<std::pair<sptr_t, sptr_t>> ranges(EditorView* v, int indicator)
{
    std::vector<std::pair<sptr_t, sptr_t>> out;
    const sptr_t length = v->sci(SCI_GETLENGTH);
    for (sptr_t pos = 0; pos < length;) {
        sptr_t start = pos;
        if (!v->sci(SCI_INDICATORVALUEAT, static_cast<uptr_t>(indicator), pos))
            start = v->sci(SCI_INDICATOREND, static_cast<uptr_t>(indicator), pos);
        if (start >= length || start < pos)
            break;
        const sptr_t end = v->sci(SCI_INDICATOREND, static_cast<uptr_t>(indicator), start);
        if (end <= start)
            break;
        out.emplace_back(start, end);
        pos = end;
    }
    return out;
}

bool bookmarked(EditorView* v, int line)
{
    return (v->sci(SCI_MARKERGET, static_cast<uptr_t>(line)) & (1 << kBookmarkMarker)) != 0;
}

SearchOptions find(const char* text)
{
    SearchOptions o;
    o.pattern = text;
    o.wrapAround = true;
    return o;
}

} // namespace

class TestMark : public QObject
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

    void marksEveryMatch()
    {
        auto w = window("uno dos\ntres uno\ncuatro\nuno\n");
        EditorView* v = w->editor();
        QCOMPARE(w->markAllInCurrent(find("uno"), false, false), 3);
        const auto marked = ranges(v, kIndicatorFound);
        QCOMPARE(marked.size(), size_t(3));
        QCOMPARE(marked[0], (std::pair<sptr_t, sptr_t>(0, 3)));
        QCOMPARE(marked[1], (std::pair<sptr_t, sptr_t>(13, 16)));
        QVERIFY(!bookmarked(v, 0));
        // Sin «borrar en cada búsqueda», las marcas se acumulan
        QCOMPARE(w->markAllInCurrent(find("cuatro"), false, false), 1);
        QCOMPARE(ranges(v, kIndicatorFound).size(), size_t(4));
        // Con él, solo quedan las nuevas
        QCOMPARE(w->markAllInCurrent(find("dos"), false, true), 1);
        QCOMPARE(ranges(v, kIndicatorFound).size(), size_t(1));
        // Las órdenes «Marcado al buscar» ahora tienen a dónde ir
        v->send(SCI_GOTOPOS, 0);
        w->findChild<QAction*>(QStringLiteral("actionNextFoundMark"))->trigger();
        QCOMPARE(v->sci(SCI_GETSELECTIONSTART), sptr_t(4));
        // Una expresión inválida no marca nada
        SearchOptions bad = find("(");
        bad.mode = SearchMode::Regex;
        QCOMPARE(w->markAllInCurrent(bad, false, false), -1);
    }

    void bookmarksAndSelection()
    {
        auto w = window("a x\nb\nc x\nd x\n");
        EditorView* v = w->editor();
        QCOMPARE(w->markAllInCurrent(find("x"), true, false), 3);
        QVERIFY(bookmarked(v, 0) && !bookmarked(v, 1) && bookmarked(v, 2) && bookmarked(v, 3));
        // Borrar las marcas de la selección (líneas 2 y 3) con sus marcadores
        v->send(SCI_SETSEL, 6, v->sci(SCI_GETLENGTH));
        w->clearMarks(true, true);
        QCOMPARE(ranges(v, kIndicatorFound).size(), size_t(1));
        QVERIFY(bookmarked(v, 0) && !bookmarked(v, 2) && !bookmarked(v, 3));
        // Marcar solo dentro de la selección
        SearchOptions inSelection = find("x");
        inSelection.inSelection = true;
        QCOMPARE(w->markAllInCurrent(inSelection, false, false), 2);
        // Todo, sin tocar los marcadores
        w->clearMarks(false, false);
        QVERIFY(ranges(v, kIndicatorFound).empty());
        QVERIFY(bookmarked(v, 0));
        w->clearMarks(false, true);
        QVERIFY(!bookmarked(v, 0));
        // «^$»: coincidencias vacías, que no se pintan pero marcan su línea
        v->sends(SCI_SETTEXT, 0, "a\n\nb\n");
        SearchOptions empty = find("^$");
        empty.mode = SearchMode::Regex;
        QVERIFY(w->markAllInCurrent(empty, true, false) >= 1);
        QVERIFY(bookmarked(v, 1));
    }

    void copiesStyledText()
    {
        auto w = window("uno dos uno\ntres\n");
        EditorView* v = w->editor();
        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(QStringLiteral("antes"));
        w->copyStyledText({kIndicatorFound});
        QCOMPARE(clipboard->text(), QStringLiteral("antes")); // nada marcado: no cambia
        w->markAllInCurrent(find("uno"), false, false);
        w->copyStyledText({kIndicatorFound});
        QCOMPARE(clipboard->text(), QStringLiteral("uno\nuno\n"));
        // Un solo tramo, sin salto final
        w->markAllInCurrent(find("tres"), false, true);
        w->findChild<QAction*>(QStringLiteral("actionCopyFoundMark"))->trigger();
        QCOMPARE(clipboard->text(), QStringLiteral("tres"));
        // Varios estilos de token, en el orden del documento; con varias líneas, «----» entre ellos
        v->send(SCI_SETSEL, 8, 11);
        w->findChild<QAction*>(QStringLiteral("actionStyleOne1"))->trigger();
        v->send(SCI_SETSEL, 0, 7);
        w->findChild<QAction*>(QStringLiteral("actionStyleOne2"))->trigger();
        w->findChild<QAction*>(QStringLiteral("actionCopyAllStyles"))->trigger();
        QCOMPARE(clipboard->text(), QStringLiteral("uno dos\nuno\n"));
        w->findChild<QAction*>(QStringLiteral("actionCopyStyle1"))->trigger();
        QCOMPARE(clipboard->text(), QStringLiteral("uno"));
        v->send(SCI_SETSEL, 8, 16);
        w->findChild<QAction*>(QStringLiteral("actionStyleOne1"))->trigger();
        w->findChild<QAction*>(QStringLiteral("actionCopyAllStyles"))->trigger();
        QCOMPARE(clipboard->text(), QStringLiteral("uno dos\n----\nuno\ntres\n"));
    }

    void markTabOfTheDialog()
    {
        auto w = window("hola mundo hola\n");
        w->show();
        w->findChild<QAction*>(QStringLiteral("actionMark"))->trigger();
        FindReplaceDialog* dialog = w->findDialog();
        QCOMPARE(dialog->currentTab(), FindReplaceDialog::Tab::Mark);
        auto* markAll = dialog->findChild<QPushButton*>(QStringLiteral("markAllButton"));
        QVERIFY(markAll->isVisible());
        QVERIFY(!dialog->findChild<QPushButton*>(QStringLiteral("findNextButton"))->isVisible());
        QVERIFY(dialog->findChild<QCheckBox*>(QStringLiteral("bookmarkLineCheck"))->isVisible());
        auto* combo = dialog->findChild<QComboBox*>(QStringLiteral("findCombo"));
        combo->setEditText(QStringLiteral("hola"));
        dialog->findChild<QCheckBox*>(QStringLiteral("bookmarkLineCheck"))->setChecked(true);
        markAll->click();
        QCOMPARE(ranges(w->editor(), kIndicatorFound).size(), size_t(2));
        QVERIFY(bookmarked(w->editor(), 0));
        QVERIFY(dialog->statusText().contains(QLatin1Char('2')));
        dialog->findChild<QPushButton*>(QStringLiteral("clearMarksButton"))->click();
        QVERIFY(ranges(w->editor(), kIndicatorFound).empty());
        QVERIFY(!bookmarked(w->editor(), 0));
        // Intro en el campo marca
        QTest::keyClick(combo->lineEdit(), Qt::Key_Return);
        QCOMPARE(ranges(w->editor(), kIndicatorFound).size(), size_t(2));
        // En otra pestaña no se ven los botones de Marcar
        dialog->showTab(FindReplaceDialog::Tab::Find);
        QVERIFY(!markAll->isVisible());
        // Las casillas se guardan en config.xml (bookmarkLine, purge) como en N++
        dialog->setMarkOptions(true, true);
        w->close();
        w.reset();
        auto again = window("");
        QVERIFY(again->findDialog()->bookmarkLine());
        QVERIFY(again->findDialog()->purge());
    }
};

QTEST_MAIN(TestMark)
#include "tst_mark.moc"
