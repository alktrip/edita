// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 74: las órdenes cortas que faltaban de N++ (sangría desde el menú, marcar inicio y fin de
// la selección, solo lectura para todos, caracteres no imprimibles y de control, buscar
// caracteres en un rango, las órdenes solo de atajo y Ayuda)

#include "FakePrompter.h"
#include "core/OnSelection.h"
#include "core/Preferences.h"
#include "doc/Buffer.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/Shortcuts.h"

#include <QAction>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

class TestShortCommands : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    FakePrompter* _prompter = nullptr;
    QList<QUrl> _opened;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        auto prompter = std::make_unique<FakePrompter>();
        _prompter = prompter.get();
        w->setPrompter(std::move(prompter));
        _opened.clear();
        w->setUrlOpener([this](const QUrl& url) {
            _opened << url;
            return true;
        });
        QCoreApplication::processEvents();
        return w;
    }

    static QAction* action(MainWindow& w, const char* name) { return w.findChild<QAction*>(QString::fromLatin1(name)); }
    static QString text(MainWindow& w) { return QString::fromStdString(w.editor()->text()); }
    static QString representation(EditorView& v, const char* utf8)
    {
        QByteArray out(32, '\0');
        v.sci(SCI_GETREPRESENTATION, reinterpret_cast<uptr_t>(utf8), reinterpret_cast<sptr_t>(out.data()));
        return QString::fromUtf8(out.constData());
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    // Editar › Sangría: una línea, un ancho de tabulación; varias, como Tab
    void indent()
    {
        auto w = window();
        EditorView* v = w->editor();
        v->sci(SCI_SETUSETABS, 0);
        v->sci(SCI_SETTABWIDTH, 4);
        v->sends(SCI_SETTEXT, 0, "a\nb\n");
        v->sci(SCI_GOTOPOS, 0);
        action(*w, "actionIndentIncrease")->trigger();
        QCOMPARE(text(*w), QStringLiteral("    a\nb\n"));
        action(*w, "actionIndentIncrease")->trigger();
        action(*w, "actionIndentDecrease")->trigger();
        QCOMPARE(text(*w), QStringLiteral("    a\nb\n"));
        action(*w, "actionIndentDecrease")->trigger();
        action(*w, "actionIndentDecrease")->trigger(); // no baja de cero
        QCOMPARE(text(*w), QStringLiteral("a\nb\n"));
        v->sci(SCI_SETSEL, 0, 3); // dos líneas
        action(*w, "actionIndentIncrease")->trigger();
        QCOMPARE(text(*w), QStringLiteral("    a\n    b\n"));
    }

    // Editar › Marcar inicio/fin: la primera vez marca; la segunda selecciona
    void beginEndSelect()
    {
        auto w = window();
        EditorView* v = w->editor();
        v->sends(SCI_SETTEXT, 0, "abcdef\nghijkl\n");
        QAction* stream = action(*w, "actionBeginEndSelect");
        QAction* column = action(*w, "actionBeginEndSelectColumn");
        v->sci(SCI_GOTOPOS, 1);
        stream->trigger();
        QVERIFY(stream->isChecked());
        QVERIFY(!column->isEnabled());
        v->sci(SCI_GOTOPOS, 10);
        stream->trigger();
        QVERIFY(!stream->isChecked());
        QVERIFY(column->isEnabled());
        QCOMPARE(v->sci(SCI_GETSELECTIONSTART), 1);
        QCOMPARE(v->sci(SCI_GETSELECTIONEND), 10);
        // En columna: rectangular
        v->sci(SCI_GOTOPOS, 1);
        column->trigger();
        v->sci(SCI_GOTOPOS, 10);
        column->trigger();
        QCOMPARE(v->sci(SCI_GETSELECTIONMODE), static_cast<sptr_t>(SC_SEL_RECTANGLE));
        QCOMPARE(v->sci(SCI_GETSELECTIONS), 2);
    }

    // Editar › Solo lectura para todos los documentos
    void readOnlyForAll()
    {
        auto w = window();
        const BufferId first = w->currentBuffer();
        const BufferId second = w->newDocument();
        action(*w, "actionReadOnlyAll")->trigger();
        QVERIFY(w->documents().buffer(first)->isUserReadOnly());
        QVERIFY(w->documents().buffer(second)->isUserReadOnly());
        QVERIFY(action(*w, "actionReadOnly")->isChecked());
        action(*w, "actionReadOnlyClearAll")->trigger();
        QVERIFY(!w->documents().buffer(first)->isUserReadOnly());
        QVERIFY(!action(*w, "actionReadOnly")->isChecked());
    }

    // Ver › Mostrar símbolo: los no imprimibles (NBSP…) y los de control
    void nonPrintingCharacters()
    {
        auto w = window();
        EditorView* v = w->editor();
        QCOMPARE(representation(*v, "\xC2\xA0"), QString()); // de serie, no
        QCOMPARE(representation(*v, "\x01"), QStringLiteral("SOH")); // los de control, sí
        action(*w, "actionShowNpc")->trigger();
        QVERIFY(w->preferences().npcShow);
        QCOMPARE(representation(*v, "\xC2\xA0"), QStringLiteral("NBSP"));
        QCOMPARE(representation(*v, "\xE2\x80\x8B"), QStringLiteral("ZWSP"));
        action(*w, "actionShowControlChars")->trigger();
        QVERIFY(!w->preferences().ccShow);
        QCOMPARE(representation(*v, "\x01"), QString::fromUtf8("\xE2\x80\x8B")); // invisible
        QCOMPARE(representation(*v, "\xE2\x80\xA8"), QString::fromUtf8("\xE2\x80\x8B"));
        // Mostrar todos los caracteres: los cuatro
        action(*w, "actionShowAll")->trigger();
        const Preferences& p = w->preferences();
        QVERIFY(p.whiteSpaceShow && p.eolShow && p.npcShow && p.ccShow);
        QVERIFY(action(*w, "actionShowAll")->isChecked());
        action(*w, "actionShowAll")->trigger();
        QVERIFY(!w->preferences().npcShow && !w->preferences().ccShow);
        QCOMPARE(representation(*v, "\xC2\xA0"), QString());
    }

    // Buscar caracteres en un rango (códigos Unicode, con los pares sustitutos)
    void charsInRange()
    {
        const QString text = QStringLiteral("ab ñ c 😀 d");
        QCOMPARE(findCharInRange(text, 0, 128, 0x10FFFF, true, false), (std::pair<qsizetype, qsizetype>{3, 1}));
        QCOMPARE(findCharInRange(text, 4, 128, 0x10FFFF, true, false), (std::pair<qsizetype, qsizetype>{7, 2}));
        QCOMPARE(findCharInRange(text, 9, 128, 0x10FFFF, true, false), (std::pair<qsizetype, qsizetype>{-1, 0}));
        QCOMPARE(findCharInRange(text, 9, 128, 0x10FFFF, true, true), (std::pair<qsizetype, qsizetype>{3, 1}));
        QCOMPARE(findCharInRange(text, 9, 128, 0x10FFFF, false, false), (std::pair<qsizetype, qsizetype>{7, 2}));
        QCOMPARE(findCharInRange(text, 3, 128, 0x10FFFF, false, true), (std::pair<qsizetype, qsizetype>{7, 2}));
        QCOMPARE(findCharInRange(text, 0, 'c', 'd', true, false), (std::pair<qsizetype, qsizetype>{5, 1}));

        auto w = window();
        EditorView* v = w->editor();
        v->sends(SCI_SETTEXT, 0, "ab ñ c 😀 d");
        v->sci(SCI_GOTOPOS, 0);
        QVERIFY(w->findCharInRange(128, 0x10FFFF, true, false));
        QCOMPARE(v->sci(SCI_GETSELECTIONSTART), 3);
        QCOMPARE(v->sci(SCI_GETSELECTIONEND), 5); // «ñ», dos bytes
        QVERIFY(w->findCharInRange(128, 0x10FFFF, true, false));
        QCOMPARE(v->sci(SCI_GETSELECTIONSTART), 8);
        QCOMPARE(v->sci(SCI_GETSELECTIONEND), 12); // el emoji, cuatro
        QVERIFY(!w->findCharInRange(128, 0x10FFFF, true, false));
        QVERIFY(w->findCharInRange(128, 0x10FFFF, false, false));
        QCOMPARE(v->sci(SCI_GETSELECTIONSTART), 3);
        QVERIFY(action(*w, "actionFindCharsInRange"));
    }

    // Las órdenes sin menú: en la Asignación de atajos y funcionan
    void shortcutOnly()
    {
        auto w = window();
        bool listed = false;
        for (const ShortcutEntry& e : w->shortcutEntries())
            listed = listed || (e.id == QLatin1String("actionToggleMacroRecord") && e.category == QLatin1String("Shortcut only"));
        QVERIFY(listed);
        QVERIFY(!w->isRecordingMacro());
        action(*w, "actionToggleMacroRecord")->trigger();
        QVERIFY(w->isRecordingMacro());
        action(*w, "actionToggleMacroRecord")->trigger();
        QVERIFY(!w->isRecordingMacro());
        action(*w, "actionFunctionHintNext")->trigger(); // sin ayuda abierta, nada
        action(*w, "actionFunctionHintPrevious")->trigger();
    }

    // Ayuda: los argumentos de la línea de órdenes y la página de Edita
    void help()
    {
        auto w = window();
        action(*w, "actionCommandLineArguments")->trigger();
        QCOMPARE(_prompter->informations.size(), 1);
        for (const char* option : {"--nosession", "--multiInst", "--openSession", "--line"})
            QVERIFY2(_prompter->informations.front().contains(QLatin1String(option)), option);
        action(*w, "actionHomePage")->trigger();
        QCOMPARE(_opened, QList<QUrl>{QUrl(QStringLiteral("https://github.com/alktrip/edita"))});
    }
};

QTEST_MAIN(TestShortCommands)
#include "tst_shortcommands.moc"
