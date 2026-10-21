// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 21: Editor de columnas de N++ (ColumnEditorDlg, Alt+C).

#include "FakePrompter.h"
#include "core/ColumnEditor.h"
#include "core/ConfigXml.h"
#include "ui/ColumnEditorDialog.h"
#include "ui/EditCommands.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QComboBox>
#include <QLineEdit>
#include <QMenuBar>
#include <QRadioButton>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using namespace std::string_literals;
using Texts = std::vector<std::string>;

class TestColumnEditor : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;

    static std::string text(ScintillaEditBase& view)
    {
        const auto length = static_cast<std::size_t>(view.send(SCI_GETLENGTH));
        std::string s(length + 1, '\0');
        view.send(SCI_GETTEXT, length + 1, reinterpret_cast<sptr_t>(s.data()));
        s.resize(length);
        return s;
    }

    static void setText(ScintillaEditBase& view, const char* t)
    {
        view.send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(t));
        view.send(SCI_EMPTYUNDOBUFFER);
    }

    // Selección rectangular de las líneas [first, last], columnas [from, to)
    static void rectangle(ScintillaEditBase& view, int first, int last, int from, int to)
    {
        view.send(SCI_SETVIRTUALSPACEOPTIONS, SCVS_RECTANGULARSELECTION);
        view.send(SCI_SETRECTANGULARSELECTIONANCHOR, static_cast<uptr_t>(view.send(SCI_FINDCOLUMN, static_cast<uptr_t>(first), from)));
        view.send(SCI_SETRECTANGULARSELECTIONCARET, static_cast<uptr_t>(view.send(SCI_FINDCOLUMN, static_cast<uptr_t>(last), to)));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void numbersLikeNotepadPlusPlus()
    {
        ColumnNumbers p;
        QCOMPARE(columnNumbers(p, 4), (Texts{"1", "2", "3", "4"}));
        p.initial = 0;
        p.increase = 5;
        p.repeat = 2;
        QCOMPARE(columnNumbers(p, 5), (Texts{"0", "0", "5", "5", "10"}));
        p = ColumnNumbers{};
        p.initial = 8;
        p.leading = NumberLeading::Zeros;
        QCOMPARE(columnNumbers(p, 3), (Texts{"08", "09", "10"})); // el ancho del más largo
        p.leading = NumberLeading::Spaces;
        QCOMPARE(columnNumbers(p, 3), (Texts{" 8", " 9", "10"}));
        p = ColumnNumbers{};
        p.initial = 254;
        p.base = NumberBase::Hex;
        QCOMPARE(columnNumbers(p, 3), (Texts{"fe", "ff", "100"}));
        p.base = NumberBase::HexUpper;
        p.leading = NumberLeading::Zeros;
        QCOMPARE(columnNumbers(p, 3), (Texts{"0FE", "0FF", "100"}));
        p = ColumnNumbers{};
        p.initial = 6;
        p.base = NumberBase::Binary;
        QCOMPARE(columnNumbers(p, 3), (Texts{"110", "111", "1000"}));
        p.base = NumberBase::Octal;
        QCOMPARE(columnNumbers(p, 3), (Texts{"6", "7", "10"}));
        // Negativos: los ceros van detrás del signo
        p = ColumnNumbers{};
        p.initial = -10;
        p.increase = 9;
        p.leading = NumberLeading::Zeros;
        QCOMPARE(columnNumbers(p, 2), (Texts{"-10", "-01"}));
        p.repeat = 0; // como 1 (en N++, para no entrar en un bucle infinito)
        QCOMPARE(columnNumbers(p, 2).size(), std::size_t{2});

        long long v = 0;
        QVERIFY(parseNumber("ff", NumberBase::Hex, v) && v == 255);
        QVERIFY(parseNumber("101", NumberBase::Binary, v) && v == 5);
        QVERIFY(!parseNumber("102", NumberBase::Binary, v));
        QVERIFY(!parseNumber("12z", NumberBase::Decimal, v));
        QVERIFY(!parseNumber("", NumberBase::Decimal, v));
        QCOMPARE(formatNumber(-255, NumberBase::HexUpper), "-FF"s);
    }

    void insertsAtTheCaretColumnDownToTheEnd()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<test::FakePrompter>());
        ScintillaEditBase& view = *w.editor();
        setText(view, "abc\nd\n\nefgh");
        view.send(SCI_GOTOPOS, 1); // línea 1, columna 1
        QCOMPARE(columnInsertCount(view), std::size_t{4});
        columnInsert(view, {"1", "2", "3", "4"});
        // Las líneas más cortas se rellenan con espacios hasta la columna
        QCOMPARE(text(view), "a1bc\nd2\n 3\ne4fgh"s);
        // Una sola acción de deshacer
        view.send(SCI_UNDO);
        QCOMPARE(text(view), "abc\nd\n\nefgh"s);
        // Desde una línea del medio
        view.send(SCI_GOTOLINE, 2);
        QCOMPARE(columnInsertCount(view), std::size_t{2});
        columnInsert(view, {"x"}); // un solo texto vale para todos
        QCOMPARE(text(view), "abc\nd\nx\nxefgh"s);
    }

    void replacesRectangularSelections()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<test::FakePrompter>());
        ScintillaEditBase& view = *w.editor();
        setText(view, "aaaa\nbbbb\ncccc\n");
        rectangle(view, 0, 2, 1, 3);
        QCOMPARE(columnInsertCount(view), std::size_t{3});
        columnInsert(view, {"1", "22", "333"});
        QCOMPARE(text(view), "a1a\nb22b\nc333c\n"s);
        // Quedan seleccionados los textos puestos
        QCOMPARE(view.send(SCI_GETSELECTIONS), sptr_t{3});
        QCOMPARE(view.send(SCI_GETSELECTIONNSTART, 2) + 3, view.send(SCI_GETSELECTIONNEND, 2));

        // Con espacio virtual: la selección pasa del final de líneas cortas
        setText(view, "aaaaaa\nb\ncccccc\n");
        rectangle(view, 0, 2, 4, 4);
        QCOMPARE(columnInsertCount(view), std::size_t{3});
        columnInsert(view, {"|"});
        QCOMPARE(text(view), "aaaa|aa\nb   |\ncccc|cc\n"s);
    }

    void dialogRemembersItsSettings()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<test::FakePrompter>());
        ScintillaEditBase& view = *w.editor();
        setText(view, "a\nb\nc");
        view.send(SCI_GOTOPOS, 0);
        {
            ColumnEditorDialog dialog(view, w.syntax().config());
            dialog.findChild<QRadioButton*>(QStringLiteral("numberMode"))->setChecked(true);
            dialog.findChild<QLineEdit*>(QStringLiteral("initial"))->setText(QStringLiteral("9"));
            dialog.findChild<QRadioButton*>(QStringLiteral("hex"))->setChecked(true);
            dialog.findChild<QComboBox*>(QStringLiteral("hexCase"))->setCurrentIndex(1);
            dialog.findChild<QComboBox*>(QStringLiteral("leading"))->setCurrentIndex(1);
            QVERIFY(dialog.insert());
        }
        QCOMPARE(text(view), "9a\nAb\nBc"s);
        // Lo recuerda (config.xml)
        ColumnEditorDialog again(view, w.syntax().config());
        QVERIFY(again.findChild<QRadioButton*>(QStringLiteral("numberMode"))->isChecked());
        QCOMPARE(again.findChild<QLineEdit*>(QStringLiteral("initial"))->text(), QStringLiteral("9"));
        QVERIFY(again.findChild<QRadioButton*>(QStringLiteral("hex"))->isChecked());
        QCOMPARE(again.findChild<QComboBox*>(QStringLiteral("hexCase"))->currentIndex(), 1);
        QCOMPARE(w.syntax().config().guiConfig(QStringLiteral("ColumnEditor"), QStringLiteral("formatChoice")), QStringLiteral("4"));
        // Un número que no vale en la base elegida: no se inserta nada
        again.findChild<QRadioButton*>(QStringLiteral("binary"))->setChecked(true);
        again.findChild<QLineEdit*>(QStringLiteral("initial"))->setText(QStringLiteral("2"));
        QVERIFY(!again.insert());
        QCOMPARE(text(view), "9a\nAb\nBc"s);
        // Texto
        again.findChild<QRadioButton*>(QStringLiteral("textMode"))->setChecked(true);
        again.findChild<QLineEdit*>(QStringLiteral("text"))->setText(QStringLiteral("-"));
        view.send(SCI_GOTOPOS, 0);
        QVERIFY(again.insert());
        QCOMPARE(text(view), "-9a\n-Ab\n-Bc"s);
    }

    void menuHasItWithAltC()
    {
        MainWindow w;
        QAction* action = w.findChild<QAction*>(QStringLiteral("actionColumnEditor"));
        QVERIFY(action);
        QCOMPARE(action->shortcut(), QKeySequence(Qt::ALT | Qt::Key_C));
        // Ningún menú de la barra usa Alt+C como letra (Codificación es E&ncoding, como en N++)
        for (QAction* top : w.menuBar()->actions())
            QVERIFY2(!top->text().contains(QStringLiteral("&c"), Qt::CaseInsensitive), qPrintable(top->text()));
    }
};

QTEST_MAIN(TestColumnEditor)
#include "tst_columneditor.moc"
