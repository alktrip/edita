// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Probador de expresiones regulares (no existe en N++).

#include "FakePrompter.h"
#include "search/SearchEngine.h"
#include "search/SearchTarget.h"
#include "ui/EditorView.h"
#include "ui/FindReplaceDialog.h"
#include "ui/MainWindow.h"
#include "ui/RegexTester.h"
#include "ui/Translations.h"

#include <QAction>
#include <QDockWidget>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

using namespace edita;
using edita::test::FakePrompter;

class TestRegexTester : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        i18n::install(QStringLiteral("en")); // las formas de plural
    }

    void groupNames()
    {
        // Sin capturar: (?:…), (?<=…), (?!…), \( y [(]; con nombre: <…>, '…' y P<…>
        const auto names = RegexTesterPanel::groupNames(QStringLiteral(R"((\d+)-(?<mes>\d+)(?:x)[(](?<=a)\((?!b)(?'dia'z)(?P<e>q)\Q(\E)"));
        QCOMPARE(names, (std::vector<QString>{QString(), QString(), QStringLiteral("mes"), QStringLiteral("dia"), QStringLiteral("e")}));
        QCOMPARE(RegexTesterPanel::groupNames(QStringLiteral("[]()](x)")).size(), std::size_t{2});
    }

    void groupPositionsFromTheEngine()
    {
        // La regex de Boost guarda dónde cayó cada grupo; el que no participa, -1
        DocumentSearchTarget doc("xx ac");
        SciPos end = 0;
        const SciPos pos = doc.find(0, doc.length(), "(a)(b)?(c)", SCFIND_REGEXP | SCFIND_POSIX, end);
        QCOMPARE(pos, SciPos{3});
        const auto groups = SearchTarget::lastMatchGroups();
        using G = std::pair<SciPos, SciPos>;
        QCOMPARE(groups, (std::vector<G>{{3, 5}, {3, 4}, {-1, -1}, {4, 5}}));
    }

    void matchesGroupsAndPreview()
    {
        RegexTesterPanel panel;
        panel.setSample(QStringLiteral("2026-10-26 y 1999-01-02\nnada"));
        panel.setPattern(QStringLiteral(R"((\d{4})-(?<mes>\d{2})-(\d{2}))"));
        QCOMPARE(panel.count(), 2);
        QCOMPARE(panel.statusText(), QStringLiteral("2 matches · 3 groups"));
        const auto& m = panel.matches();
        QCOMPARE(m[1].start, SciPos{13});
        QCOMPARE(m[0].groups[1].name, QStringLiteral("mes"));
        QCOMPARE(m[0].groups[1].start, SciPos{5});
        // Coincidencias resaltadas (alternando) y el texto de cada grupo con su color
        ScintillaEditBase* sample = panel.sampleView();
        QVERIFY(sample->send(SCI_INDICATORVALUEAT, 8, 0) != 0);
        QVERIFY(sample->send(SCI_INDICATORVALUEAT, 9, 13) != 0);
        QVERIFY(sample->send(SCI_INDICATORVALUEAT, 11, 5) != 0); // el grupo 2
        QCOMPARE(sample->send(SCI_INDICATORVALUEAT, 11, 0), sptr_t{0});
        // La lista: cada coincidencia con sus grupos; elegir uno lo selecciona en el texto
        QTreeWidget* tree = panel.tree();
        QCOMPARE(tree->topLevelItemCount(), 2);
        QCOMPARE(tree->topLevelItem(0)->childCount(), 3);
        QCOMPARE(tree->topLevelItem(0)->child(1)->text(0), QStringLiteral("Group 2 (mes)"));
        QCOMPARE(tree->topLevelItem(1)->child(2)->text(1), QStringLiteral("02"));
        QCOMPARE(tree->topLevelItem(1)->text(2), QStringLiteral("Ln 1, Col 14"));
        tree->setCurrentItem(tree->topLevelItem(1)->child(0));
        QCOMPARE(sample->send(SCI_GETSELECTIONSTART), sptr_t{13});
        QCOMPARE(sample->send(SCI_GETSELECTIONEND), sptr_t{17});
        // Vista previa del reemplazo, con grupos por número y por nombre
        QVERIFY(panel.previewView()->isHidden());
        panel.replacementField()->setText(QStringLiteral("$3/$+{mes}/$1"));
        panel.evaluate();
        QVERIFY(!panel.previewView()->isHidden());
        QCOMPARE(panel.previewView()->toPlainText(), QStringLiteral("26/10/2026 y 02/01/1999\nnada"));
        // Al editar el texto de prueba se vuelve a evaluar
        sample->send(SCI_APPENDTEXT, 11, reinterpret_cast<sptr_t>(" 2000-02-03"));
        QTRY_COMPARE(panel.count(), 3);
        // Una expresión que no vale
        panel.setPattern(QStringLiteral(R"((\d)"));
        QVERIFY2(panel.statusText().startsWith(QStringLiteral("Invalid regular expression")), qPrintable(panel.statusText()));
        QVERIFY(panel.matches().empty());
        QCOMPARE(tree->topLevelItemCount(), 0);
        QCOMPARE(sample->send(SCI_INDICATORVALUEAT, 8, 0), sptr_t{0});
    }

    void toggles()
    {
        RegexTesterPanel panel;
        panel.setSample(QStringLiteral("abc ABC\na-b"));
        panel.setPattern(QStringLiteral("abc"));
        QCOMPARE(panel.count(), 2);
        panel.findChild<QToolButton*>(QStringLiteral("regexTesterMatchCase"))->click();
        QCOMPARE(panel.count(), 1);
        panel.setPattern(QStringLiteral("C.a"));
        QCOMPARE(panel.statusText(), QStringLiteral("No matches"));
        panel.findChild<QToolButton*>(QStringLiteral("regexTesterDotAll"))->click(); // . con el salto de línea
        QCOMPARE(panel.count(), 1);
        // Referencia rápida: escribe el elemento, con la selección dentro de un grupo
        panel.patternField()->setText(QStringLiteral("ab"));
        panel.patternField()->selectAll();
        QToolButton* reference = panel.findChild<QToolButton*>(QStringLiteral("regexTesterReference"));
        const QList<QAction*> actions = reference->menu()->actions();
        auto it = std::find_if(actions.begin(), actions.end(), [](QAction* a) { return a->text().startsWith(QStringLiteral("(?:)")); });
        QVERIFY(it != actions.end());
        (*it)->trigger();
        QCOMPARE(panel.patternField()->text(), QStringLiteral("(?:ab)"));
    }

    void fromTheMainWindow()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.newDocument();
        w.editor()->sends(SCI_SETTEXT, 0, "uno 12\ndos 345\ntres");
        w.editor()->sci(SCI_SETSEL, 4, 14); // «12\ndos 345»
        // Abrirlo toma la selección como texto de prueba
        w.findChild<QAction*>(QStringLiteral("actionRegexTester"))->trigger();
        RegexTesterPanel* panel = w.regexTesterPanel();
        QVERIFY(panel->parentWidget()->isVisibleTo(&w) || w.findChild<QDockWidget*>(QStringLiteral("regexTesterDock"))->isVisibleTo(&w));
        QCOMPARE(panel->sample(), QStringLiteral("12\ndos 345"));
        // «Del editor» sin selección: todo el documento
        w.editor()->sci(SCI_SETEMPTYSELECTION, 0);
        panel->findChild<QPushButton*>(QStringLiteral("regexTesterFromEditor"))->click();
        QCOMPARE(panel->sample(), QStringLiteral("uno 12\ndos 345\ntres"));
        panel->setPattern(QStringLiteral(R"(\d+)"));
        QCOMPARE(panel->count(), 2);
        // «Usar en Buscar»: el diálogo, en modo regex y con la expresión
        panel->findChild<QPushButton*>(QStringLiteral("regexTesterUseInFind"))->click();
        auto* dialog = w.findChild<FindReplaceDialog*>();
        QVERIFY(dialog && dialog->isVisible());
        QCOMPARE(dialog->options().mode, SearchMode::Regex);
        QCOMPARE(dialog->options().pattern, std::string(R"(\d+)"));
        QCOMPARE(dialog->currentTab(), FindReplaceDialog::Tab::Find);
    }
};

QTEST_MAIN(TestRegexTester)
#include "tst_regextester.moc"
