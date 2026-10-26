// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Barra de estado interactiva, paréntesis por colores y fuentes del escritorio.

#include "FakePrompter.h"
#include "syntax/DesktopFonts.h"
#include "ui/CommandPalette.h"
#include "ui/DocStatusBar.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/QuickPicker.h"
#include "ui/RainbowBrackets.h"

#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

using namespace edita;
using edita::test::FakePrompter;
using namespace std::string_literals;

class TestStatusBar : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    static QToolButton* fieldButton(MainWindow& w, DocStatusBar::Field field)
    {
        return qobject_cast<QToolButton*>(w.docStatusBar()->field(field));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void desktopFontsInAnyQtFormat()
    {
        // Qt 6.12 (19 campos, lo que guarda Plasma 6), Qt 6 antiguo y Qt 5
        const auto font = desktopfonts::parseFontDescription(QStringLiteral("Ubuntu,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1,,0,0"));
        QVERIFY(font);
        QCOMPARE(font->family(), QStringLiteral("Ubuntu"));
        QCOMPARE(font->pointSizeF(), 10.0);
        QCOMPARE(font->weight(), QFont::Normal);
        const auto bold = desktopfonts::parseFontDescription(QStringLiteral("Noto Sans,11.5,-1,5,700,1,0,0,0,0"));
        QVERIFY(bold && bold->bold() && bold->italic());
        QCOMPARE(bold->pointSizeF(), 11.5);
        QCOMPARE(desktopfonts::parseFontDescription(QStringLiteral("Hack,9,-1,5,50,0,0,0,0,0"))->weight(), QFont::Normal); // Qt 5
        QVERIFY(!desktopfonts::parseFontDescription(QString()));
        // kdeglobals: [General] font=…, con comas (QSettings las tomaría como una lista)
        const QString file = _tmp.filePath("kdeglobals");
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[Colors:View]\nfont=No,1\n\n[General]\nfixed=Ubuntu Mono,11,-1,5,400,0,0,0,0,0,0,0,0,0,0,1,,0,0\n"
                "font=Ubuntu,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1,,0,0\n");
        f.close();
        QCOMPARE(desktopfonts::kdeglobalsValue(QStringLiteral("font"), {QStringLiteral("/no/existe"), file}),
                 QStringLiteral("Ubuntu,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1,,0,0"));
        QCOMPARE(desktopfonts::kdeglobalsValue(QStringLiteral("fixed"), {file}).section(QLatin1Char(','), 0, 0),
                 QStringLiteral("Ubuntu Mono"));
    }

    void statusBarFieldsOpenPickers()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.newDocument(); // fuera de la página de inicio
        w.editor()->sends(SCI_SETTEXT, 0, "print(1)\n");
        // Lenguaje: lista con buscador; escribir «python» y elegir
        fieldButton(w, DocStatusBar::Field::Language)->click();
        QuickPicker* picker = w.statusPicker();
        QVERIFY(picker && picker->isVisible());
        QVERIFY(picker->visibleTexts().size() > 50);
        picker->input()->setText(QStringLiteral("python"));
        QCOMPARE(picker->visibleTexts().value(0), QStringLiteral("Python"));
        QVERIFY(picker->activate(QStringLiteral("Python")));
        QCOMPARE(w.documents().buffer(w.currentBuffer())->language(), LangType::Python);
        QVERIFY(!picker->isVisible());
        // Fin de línea: convertir
        fieldButton(w, DocStatusBar::Field::Eol)->click();
        QCOMPARE(picker->visibleTexts().size(), 3);
        picker->input()->setText(QStringLiteral("windows"));
        QTest::keyClick(picker->input(), Qt::Key_Return);
        QCOMPARE(w.documents().buffer(w.currentBuffer())->eol(), EolType::Windows);
        // Codificación: con sus juegos de caracteres agrupados
        fieldButton(w, DocStatusBar::Field::Encoding)->click();
        QVERIFY(!picker->visibleTexts().filter(QStringLiteral("UTF-8")).isEmpty());
        picker->input()->setText(QStringLiteral("1252"));
        QVERIFY(!picker->visibleTexts().isEmpty());
        QTest::keyClick(picker->input(), Qt::Key_Escape);
        QVERIFY(!picker->isVisible());
        // Posición: ir a una línea (la paleta con «:»)
        fieldButton(w, DocStatusBar::Field::Position)->click();
        QVERIFY(w.commandPalette() && !w.commandPalette()->isHidden());
        QCOMPARE(w.commandPalette()->input()->text(), QStringLiteral(":"));
        w.commandPalette()->close();
        // INS/SOB
        QCOMPARE(w.docStatusBar()->insertModeText(), QStringLiteral("INS"));
        fieldButton(w, DocStatusBar::Field::InsertMode)->click();
        QCOMPARE(w.docStatusBar()->insertModeText(), QStringLiteral("OVR"));
        QCOMPARE(w.editor()->sci(SCI_GETOVERTYPE), sptr_t{1});
    }

    void indentationOfTheDocument()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        const BufferId doc = w.newDocument();
        // De serie, con tabulaciones (como N++)
        QVERIFY(w.docStatusBar()->indentationText().startsWith(QStringLiteral("Tabs: ")));
        fieldButton(w, DocStatusBar::Field::Indentation)->click();
        QuickPicker* picker = w.statusPicker();
        QVERIFY(picker->activate(QStringLiteral("Indent using spaces")));
        QCOMPARE(w.editor()->sci(SCI_GETUSETABS), sptr_t{0});
        fieldButton(w, DocStatusBar::Field::Indentation)->click();
        QVERIFY(picker->activate(QStringLiteral("Indent using tabs")));
        fieldButton(w, DocStatusBar::Field::Indentation)->click();
        QVERIFY(picker->activate(QStringLiteral("Width: 8")));
        QCOMPARE(w.editor()->sci(SCI_GETUSETABS), sptr_t{1});
        QCOMPARE(w.editor()->sci(SCI_GETTABWIDTH), sptr_t{8});
        QCOMPARE(w.docStatusBar()->indentationText(), QStringLiteral("Tabs: 8"));
        // Se mantiene al volver al documento (cada documento con la suya)
        w.newDocument();
        QCOMPARE(w.editor()->sci(SCI_GETTABWIDTH), sptr_t{4}); // el otro, con la de serie
        w.findChild<EditorPane*>(QStringLiteral("mainPane"))->activateBuffer(doc);
        QCOMPARE(w.editor()->sci(SCI_GETUSETABS), sptr_t{1});
        QCOMPARE(w.docStatusBar()->indentationText(), QStringLiteral("Tabs: 8"));
    }

    void bracketsByLevel()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.newDocument();
        w.setLanguage(LangType::Cpp);
        const char* text = "f(a[b{c}]); // (no)\nx = \"(\"; g());\n";
        w.editor()->sends(SCI_SETTEXT, 0, text);
        RainbowBrackets* rainbow = w.rainbowBracketsOfCurrentView();
        QVERIFY(rainbow);
        rainbow->updateNow();
        const std::string s(text);
        auto at = [&](size_t pos) { return rainbow->colorAt(static_cast<intptr_t>(pos)); };
        const intptr_t level0 = at(s.find('('));
        const intptr_t level1 = at(s.find('['));
        const intptr_t level2 = at(s.find('{'));
        QVERIFY(level0 >= 0 && level1 >= 0 && level2 >= 0);
        QVERIFY(level0 != level1 && level1 != level2 && level0 != level2);
        QCOMPARE(at(s.find('}')), level2);
        QCOMPARE(at(s.find(']')), level1);
        QCOMPARE(at(s.find(')')), level0);
        // En comentarios y cadenas no cuentan
        QCOMPARE(at(s.find("(no)")), intptr_t{-1});
        QCOMPARE(at(s.find("\"(\"") + 1), intptr_t{-1});
        // «g()» se empareja; el «)» de más va en rojo (distinto de los tres niveles)
        const size_t extra = s.find("g())") + 3;
        QCOMPARE(at(extra - 1), level0);
        const intptr_t error = at(extra);
        QVERIFY(error >= 0 && error != level0 && error != level1 && error != level2);
        // Quieto: sin cambios de indicadores (paréntesis y colores volvían a calcular por
        // sus propios indicadores, en un bucle sin fin)
        w.setLanguage(LangType::Css);
        w.editor()->sends(SCI_SETTEXT, 0, "a { color: #3fb950; b: rgb(1, 2, 3); }\n");
        QTest::qWait(300);
        int changes = 0;
        connect(w.editor(), &ScintillaEditBase::modified, this, [&changes](Scintilla::ModificationFlags type) {
            if (static_cast<int>(type) & SC_MOD_CHANGEINDICATOR)
                ++changes;
        });
        QTest::qWait(500);
        QCOMPARE(changes, 0);
        // Desactivado
        w.setLanguage(LangType::Cpp);
        w.editor()->sends(SCI_SETTEXT, 0, text);
        rainbow->updateNow();
        Preferences p = w.preferences();
        p.rainbowBrackets = false;
        w.setPreferences(p);
        QCOMPARE(at(s.find('[')), intptr_t{-1});
    }
};

QTEST_MAIN(TestStatusBar)
#include "tst_statusbar.moc"
