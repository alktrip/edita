// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 73: el corrector ortográfico (core/SpellChecker con Hunspell, ui/SpellCheck), con un
// diccionario de prueba (DICPATH) para no depender de los del sistema

#include "FakePrompter.h"
#include "core/Paths.h"
#include "core/Preferences.h"
#include "core/SpellChecker.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/PreferencesDialog.h"
#include "ui/SpellCheck.h"

#include <QAction>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QListWidget>
#include <QMenu>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

class TestSpelling : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    void write(const QString& name, const QByteArray& data)
    {
        QFile f(QDir(_tmp.filePath(QStringLiteral("dics"))).filePath(name));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }

    std::unique_ptr<MainWindow> window(const QString& dictionaries = QStringLiteral("prueba"))
    {
        auto w = std::make_unique<MainWindow>();
        w->setPrompter(std::make_unique<FakePrompter>());
        Preferences p = w->preferences();
        p.spellCheck = true;
        p.spellDictionaries = dictionaries;
        p.spellOnlyCommentsAndStrings = true;
        w->setPreferences(p);
        w->resize(900, 600);
        w->show();
        QCoreApplication::processEvents();
        return w;
    }

    static bool flagged(MainWindow& w, sptr_t pos) { return w.editor()->sci(SCI_INDICATORVALUEAT, kIndicatorSpelling, pos) != 0; }
    static QStringList words(const QString& text)
    {
        QStringList out;
        for (const WordSpan& w : wordsToCheck(text))
            out << text.mid(w.start, w.length);
        return out;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        QDir().mkpath(_tmp.filePath(QStringLiteral("dics")));
        // Un diccionario de prueba en UTF-8 y otro en ISO-8859-1
        write(QStringLiteral("prueba.aff"), "SET UTF-8\nTRY aeiounrstlcdmhgbpvfyqjxzñáéíóú\n");
        write(QStringLiteral("prueba.dic"), "6\nhola\nmundo\nint\ncanción\nnúmero\ntexto\n");
        write(QStringLiteral("latino.aff"), "SET ISO8859-1\nTRY aeiouns\n");
        write(QStringLiteral("latino.dic"), QByteArray("2\nacci\xF3n\nsue\xF1o\n"));
        write(QStringLiteral("hyph_es.dic"), "no es de palabras\n");
        qputenv("DICPATH", _tmp.filePath(QStringLiteral("dics")).toUtf8());
    }

    // Qué palabras se revisan
    void words()
    {
        QCOMPARE(words(QStringLiteral("Hola, mundo: «canción» número 3.")),
                 (QStringList{"Hola", "mundo", "canción", "número"}));
        QCOMPARE(words(QStringLiteral("don't l'eau ’tis")), (QStringList{"don't", "l'eau", "tis"}));
        // Código, siglas, dígitos, URL, correos, rutas y letras sueltas: no
        QCOMPARE(words(QStringLiteral("getValue my_var HTML TODO x2 v1.0 https://ex.org/a ana@ex.org /usr/lib ~/x.txt a b")),
                 QStringList{});
        QCOMPARE(words(QStringLiteral("Árbol ÑANDÚ")), QStringList{QStringLiteral("Árbol")});
        QCOMPARE(wordsToCheck(QStringLiteral("ÑANDÚ"), false).size(), std::size_t{1});
    }

    void defaultDictionaries()
    {
        const QStringList available{"de_DE", "en_GB", "en_US", "es", "es_AR", "es_MX"};
        QCOMPARE(SpellChecker::defaultDictionaries(QStringLiteral("es_MX"), available), (QStringList{"es_MX", "en_US"}));
        QCOMPARE(SpellChecker::defaultDictionaries(QStringLiteral("es_CL.UTF-8"), available), (QStringList{"es_AR", "en_US"}));
        QCOMPARE(SpellChecker::defaultDictionaries(QStringLiteral("es_CL"), QStringList{"es", "en_GB"}), (QStringList{"es", "en_GB"}));
        QCOMPARE(SpellChecker::defaultDictionaries(QStringLiteral("en_US"), available), QStringList{"en_US"});
        QCOMPARE(SpellChecker::defaultDictionaries(QStringLiteral("fr_FR"), QStringList{}), QStringList{});
        const QStringList found = SpellChecker::availableDictionaries();
        QVERIFY(found.contains(QStringLiteral("prueba")));
        QVERIFY(found.contains(QStringLiteral("latino")));
        QVERIFY(!found.contains(QStringLiteral("hyph_es")));
    }

    // Hunspell: varios diccionarios, otra codificación, sugerencias, el personal y pasar por alto
    void checker()
    {
        if (!SpellChecker::available())
            QSKIP("Edita sin Hunspell");
        SpellChecker spell;
        QVERIFY(spell.spell(QStringLiteral("loquesea"))); // sin diccionarios, nada está mal
        QCOMPARE(spell.load({QStringLiteral("prueba"), QStringLiteral("no-existe"), QStringLiteral("latino")}), 2);
        QCOMPARE(spell.loaded(), (QStringList{"prueba", "latino"}));
        QVERIFY(spell.spell(QStringLiteral("hola")));
        QVERIFY(spell.spell(QStringLiteral("Hola")));
        QVERIFY(spell.spell(QStringLiteral("canción")));
        QVERIFY(spell.spell(QStringLiteral("acción"))); // del de ISO-8859-1
        QVERIFY(spell.spell(QStringLiteral("sueño")));
        QVERIFY(!spell.spell(QStringLiteral("mundx")));
        QVERIFY(spell.suggest(QStringLiteral("mundx")).contains(QStringLiteral("mundo")));
        QVERIFY(spell.suggest(QStringLiteral("accion")).contains(QStringLiteral("acción")));
        // El personal se guarda y se vuelve a leer
        const QString personal = _tmp.filePath(QStringLiteral("personal.txt"));
        spell.setPersonalDictionary(personal);
        QVERIFY(spell.addToPersonal(QStringLiteral("Edita")));
        QVERIFY(spell.spell(QStringLiteral("Edita")));
        SpellChecker other;
        other.load({QStringLiteral("prueba")});
        other.setPersonalDictionary(personal);
        QVERIFY(other.spell(QStringLiteral("Edita")));
        spell.ignore(QStringLiteral("zzz"));
        QVERIFY(spell.spell(QStringLiteral("zzz")));
        QVERIFY(!other.spell(QStringLiteral("zzz")));
    }

    // En el editor: subrayadas; en código, solo comentarios y cadenas
    void underlined()
    {
        if (!SpellChecker::available())
            QSKIP("Edita sin Hunspell");
        auto w = window();
        EditorView* v = w->editor();
        v->sends(SCI_SETTEXT, 0, "hola mundx texto");
        w->checkSpellingNow();
        QVERIFY(!flagged(*w, 1));
        QVERIFY(flagged(*w, 6));
        QVERIFY(!flagged(*w, 12));
        // C: el identificador «mundx» no; en el comentario, sí
        const QString path = _tmp.filePath(QStringLiteral("a.c"));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("int mundx = 1; /* hola mundx */\nchar* s = \"textx\";\n");
        }
        QVERIFY(w->openFile(path) != kInvalidBuffer);
        w->checkSpellingNow();
        QVERIFY(!flagged(*w, 5));   // el identificador
        QVERIFY(!flagged(*w, 18));  // «hola»
        QVERIFY(flagged(*w, 24));   // «mundx» en el comentario
        QVERIFY(flagged(*w, 45));   // «textx» en la cadena
        // Todo el código, si se quiere
        Preferences p = w->preferences();
        p.spellOnlyCommentsAndStrings = false;
        w->setPreferences(p);
        w->checkSpellingNow();
        QVERIFY(flagged(*w, 5));
        // Apagado (Ver › Revisar ortografía): nada
        w->findChild<QAction*>(QStringLiteral("actionSpellCheck"))->trigger();
        QVERIFY(!w->preferences().spellCheck);
        w->checkSpellingNow();
        QVERIFY(!flagged(*w, 24));
    }

    // El clic derecho sobre una palabra subrayada: sugerencias, añadir y pasar por alto
    void contextMenu()
    {
        if (!SpellChecker::available())
            QSKIP("Edita sin Hunspell");
        auto w = window();
        EditorView* v = w->editor();
        w->newDocument();
        v = w->editor();
        v->sends(SCI_SETTEXT, 0, "hola mundx y textx");
        w->checkSpellingNow();
        v->sci(SCI_GOTOPOS, 7);
        std::unique_ptr<QMenu> menu(w->buildEditorContextMenu());
        QCOMPARE(menu->actions().front()->text(), QStringLiteral("mundo"));
        QVERIFY(menu->findChild<QAction*>(QStringLiteral("spellAdd")));
        menu->actions().front()->trigger();
        QCOMPARE(QString::fromStdString(v->text()), QStringLiteral("hola mundo y textx"));
        // Añadir «textx» al diccionario personal
        w->checkSpellingNow();
        v->sci(SCI_GOTOPOS, 15);
        menu.reset(w->buildEditorContextMenu());
        menu->findChild<QAction*>(QStringLiteral("spellAdd"))->trigger();
        QVERIFY(!flagged(*w, 15));
        QFile personal(QDir(paths::configDir()).filePath(QStringLiteral("personalDictionary.txt")));
        QVERIFY(personal.open(QIODevice::ReadOnly));
        QVERIFY(personal.readAll().contains("textx"));
        // Sin palabra subrayada, el menú de siempre
        v->sci(SCI_GOTOPOS, 1);
        menu.reset(w->buildEditorContextMenu());
        QVERIFY(!menu->findChild<QAction*>(QStringLiteral("spellAdd")));
    }

    // La página Ortografía: los diccionarios marcados, y los que no están se conservan
    void preferencesPage()
    {
        Preferences p;
        p.spellDictionaries = QStringLiteral("prueba,quitado");
        PreferencesDialog dialog(p, {{0, QStringLiteral("Normal text")}});
        auto* list = dialog.findChild<QListWidget*>(QStringLiteral("spellDictionaries"));
        QVERIFY(list);
        QListWidgetItem* prueba = list->findItems(QStringLiteral("prueba"), Qt::MatchExactly).value(0);
        QListWidgetItem* latino = list->findItems(QStringLiteral("latino"), Qt::MatchExactly).value(0);
        QVERIFY(prueba && latino);
        QCOMPARE(prueba->checkState(), Qt::Checked);
        latino->setCheckState(Qt::Checked);
        prueba->setCheckState(Qt::Unchecked);
        QCOMPARE(dialog.preferences().spellDictionaries, QStringLiteral("latino,quitado"));
        QVERIFY(dialog.pageNames().contains(QStringLiteral("spelling")));
    }
};

QTEST_MAIN(TestSpelling)
#include "tst_spelling.moc"
