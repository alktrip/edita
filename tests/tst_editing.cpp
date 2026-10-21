// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 8: autocompletado (palabras, API, ayuda de parámetros, rutas, cierre de
// pares) y macros (grabar, reproducir, varias veces, shortcuts.xml).

#include "FakePrompter.h"

#include "core/ApiFile.h"
#include "core/Macro.h"
#include "core/Paths.h"
#include "ui/AutoCompletion.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;
using namespace std::string_literals;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

struct Window
{
    std::unique_ptr<MainWindow> window = std::make_unique<MainWindow>();
    FakePrompter* prompter;

    Window()
    {
        auto fake = std::make_unique<FakePrompter>();
        prompter = fake.get();
        window->setPrompter(std::move(fake));
    }
    MainWindow& w() const { return *window; }
    EditorView* editor() const { return window->editor(); }
    AutoCompletion* ac() const { return window->autoCompletion(window->pane()); }
    std::string text() const { return window->documents().text(window->currentBuffer()); }
    void setText(const char* t) const
    {
        editor()->sends(SCI_SETTEXT, 0, t);
        editor()->sci(SCI_DOCUMENTEND);
    }
    void setPrefs(const std::function<void(Preferences&)>& change) const
    {
        Preferences p = window->preferences();
        change(p);
        window->setPreferences(p);
    }
    QAction* action(const char* name) const { return window->findChild<QAction*>(QString::fromLatin1(name)); }
    QString currentItem() const
    {
        std::string buffer(256, '\0');
        const sptr_t n = editor()->sci(SCI_AUTOCGETCURRENTTEXT, 0, reinterpret_cast<sptr_t>(buffer.data()));
        return QString::fromUtf8(buffer.data(), static_cast<qsizetype>(n));
    }
};

} // namespace

class TestEditing : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    QString file(const QString& name) const { return _tmp.filePath(QStringLiteral("files/") + name); }
    QString configDir() const { return _tmp.filePath(QStringLiteral("config/edita")); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void cleanup()
    {
        QFile::remove(QDir(configDir()).filePath("config.xml"));
        QFile::remove(QDir(configDir()).filePath("session.xml"));
        QFile::remove(QDir(configDir()).filePath("shortcuts.xml"));
        QDir(_tmp.filePath("files")).removeRecursively();
    }

    // --- Archivos de API ------------------------------------------------------------

    void loadsNotepadPlusPlusApiFiles()
    {
        QCOMPARE(apiFileName(LangType::Python), QStringLiteral("python"));
        QCOMPARE(apiFileName(LangType::JavaScript), QStringLiteral("javascript"));
        const QString path = findApiFile(LangType::Python);
        QVERIFY2(path.endsWith(QStringLiteral("autoCompletion/python.xml")), qPrintable(path));
        ApiLanguage api;
        QVERIFY(loadApiFile(path, api));
        QVERIFY(!api.ignoreCase);
        QCOMPARE(api.additionalWordChars, "."s);
        const ApiKeyword* abs = api.find("abs");
        QVERIFY(abs && abs->isFunction);
        QVERIFY(!abs->overloads.empty());
        QVERIFY(!api.find("ABS")); // distingue mayúsculas
        QVERIFY(!loadApiFile(findApiFile(LangType::Text), api)); // sin archivo

        // Uno propio en ~/.config/edita/autoCompletion tiene prioridad
        writeFile(QDir(configDir()).filePath("autoCompletion/python.xml"),
                  R"XML(<NotepadPlus><AutoComplete><Environment ignoreCase="yes" startFunc="[" stopFunc="]" paramSeparator=";" terminal="."/>
<KeyWord name="mia" func="yes"><Overload retVal="int" descr="propia"><Param name="a"/><Param name="b"/></Overload></KeyWord>
<KeyWord name="clave"/></AutoComplete></NotepadPlus>)XML");
        QVERIFY(loadApiFile(findApiFile(LangType::Python), api));
        QVERIFY(api.ignoreCase);
        QCOMPARE(api.startFunc, '[');
        QCOMPARE(api.paramSeparator, ';');
        QCOMPARE(api.keywords.size(), std::size_t{2});
        QVERIFY(api.find("MIA"));
        QCOMPARE(api.find("mia")->overloads[0].params, (std::vector<std::string>{"a", "b"}));
        QFile::remove(QDir(configDir()).filePath("autoCompletion/python.xml"));
    }

    // --- Autocompletado --------------------------------------------------------------

    void wordCompletionFromTheDocument()
    {
        Window x;
        x.setPrefs([](Preferences& p) { p.autocStatus = 0; });
        x.setText("alpha alphabet beta\nal");
        QVERIFY(x.ac()->showWordComplete(false));
        QVERIFY(x.editor()->sci(SCI_AUTOCACTIVE));
        QCOMPARE(x.currentItem(), QStringLiteral("alpha"));
        x.editor()->sci(SCI_AUTOCCOMPLETE);
        QCOMPARE(x.text(), "alpha alphabet beta\nalpha"s);

        // Una sola palabra posible: se escribe sin lista (Ctrl+Intro)
        x.editor()->sends(SCI_REPLACESEL, 0, " be");
        QVERIFY(x.ac()->showWordComplete(true));
        QVERIFY(!x.editor()->sci(SCI_AUTOCACTIVE));
        QCOMPARE(x.text(), "alpha alphabet beta\nalpha beta"s);

        // Los números no se completan (autoCIgnoreNumbers)
        x.setText("12345 12\n12");
        QVERIFY(!x.ac()->showWordComplete(false));
    }

    void listAppearsWhileTyping()
    {
        Window x;
        x.setText("variable valor\n");
        QTest::keyClicks(x.editor(), "va");
        QVERIFY(x.editor()->sci(SCI_AUTOCACTIVE));
        x.editor()->sci(SCI_AUTOCCANCEL);

        // Desde 3 caracteres
        x.setPrefs([](Preferences& p) { p.autocFromLen = 3; });
        x.setText("variable valor\n");
        QTest::keyClicks(x.editor(), "va");
        QVERIFY(!x.editor()->sci(SCI_AUTOCACTIVE));
        QTest::keyClicks(x.editor(), "r");
        QVERIFY(x.editor()->sci(SCI_AUTOCACTIVE));
        x.editor()->sci(SCI_AUTOCCANCEL);

        // Desactivado
        x.setPrefs([](Preferences& p) { p.autocStatus = 0; });
        x.setText("variable valor\n");
        QTest::keyClicks(x.editor(), "var");
        QVERIFY(!x.editor()->sci(SCI_AUTOCACTIVE));
    }

    void apiCompletionAndCallTips()
    {
        writeFile(file("a.py"), "");
        Window x;
        x.w().openFile(file("a.py"));
        QVERIFY(x.ac()->hasApi());
        QTest::keyClicks(x.editor(), "isinst");
        QVERIFY(x.editor()->sci(SCI_AUTOCACTIVE));
        QCOMPARE(x.currentItem(), QStringLiteral("isinstance"));
        x.editor()->sci(SCI_AUTOCCANCEL);

        // "(" muestra los parámetros; "," pasa al siguiente; ")" cierra la ayuda
        QTest::keyClicks(x.editor(), "ance(");
        QVERIFY(x.editor()->sci(SCI_CALLTIPACTIVE));
        QTest::keyClicks(x.editor(), "obj,");
        QVERIFY(x.editor()->sci(SCI_CALLTIPACTIVE));
        QTest::keyClicks(x.editor(), " int)");
        QVERIFY(!x.editor()->sci(SCI_CALLTIPACTIVE));

        // Ctrl+Mayús+Espacio dentro de los paréntesis la vuelve a mostrar
        x.editor()->sci(SCI_CHARLEFT);
        QVERIFY(x.ac()->showFunctionComplete());
        QVERIFY(x.editor()->sci(SCI_CALLTIPACTIVE));

        // Sin "ayuda de parámetros" no aparece sola
        x.editor()->sci(SCI_CALLTIPCANCEL);
        x.setPrefs([](Preferences& p) { p.funcParams = false; });
        x.editor()->sci(SCI_DOCUMENTEND);
        QTest::keyClick(x.editor(), Qt::Key_Return);
        QTest::keyClicks(x.editor(), "abs(");
        QVERIFY(!x.editor()->sci(SCI_CALLTIPACTIVE));
    }

    void pathCompletion()
    {
        writeFile(file("dir/uno.txt"), "");
        writeFile(file("dir/otro.txt"), "");
        QDir().mkpath(file("dir/sub"));
        Window x;
        x.setPrefs([](Preferences& p) { p.autocStatus = 0; });
        const QByteArray typed = "ver \"" + file("dir/").toUtf8() + "u";
        x.setText(typed.constData());
        x.ac()->showPathCompletion();
        QVERIFY(x.editor()->sci(SCI_AUTOCACTIVE));
        QCOMPARE(x.currentItem(), file("dir/uno.txt"));
        x.editor()->sci(SCI_AUTOCCOMPLETE);
        QCOMPARE(QString::fromStdString(x.text()), QStringLiteral("ver \"") + file("dir/uno.txt"));

        // Sin ruta delante no hace nada
        x.setText("nada");
        x.ac()->showPathCompletion();
        QVERIFY(!x.editor()->sci(SCI_AUTOCACTIVE));
    }

    void matchedPairsAreInsertedAndSkipped()
    {
        Window x;
        x.setPrefs([](Preferences& p) {
            p.autocStatus = 0;
            p.insertParentheses = true;
            p.insertDoubleQuotes = true;
            p.insertCurlyBrackets = true;
        });
        QTest::keyClicks(x.editor(), "f(");
        QCOMPARE(x.text(), "f()"s);
        QCOMPARE(x.editor()->sci(SCI_GETCURRENTPOS), sptr_t{2});
        QTest::keyClicks(x.editor(), "\"");
        QCOMPARE(x.text(), "f(\"\")"s);
        QTest::keyClicks(x.editor(), "a\"");   // la comilla de cierre se salta
        QCOMPARE(x.text(), "f(\"a\")"s);
        QTest::keyClicks(x.editor(), ")");     // y el paréntesis también
        QCOMPARE(x.text(), "f(\"a\")"s);
        QCOMPARE(x.editor()->sci(SCI_GETCURRENTPOS), sptr_t{6});
        // Delante de una palabra no se cierra
        x.setText("x");
        x.editor()->sci(SCI_GOTOPOS, 0);
        QTest::keyClicks(x.editor(), "(");
        QCOMPARE(x.text(), "(x"s);
    }

    void htmlTagsAreClosed()
    {
        writeFile(file("p.html"), "");
        Window x;
        x.setPrefs([](Preferences& p) {
            p.autocStatus = 0;
            p.insertHtmlXmlTag = true;
        });
        x.w().openFile(file("p.html"));
        QTest::keyClicks(x.editor(), "<div class=\"a\">");
        QCOMPARE(x.text(), "<div class=\"a\"></div>"s);
        x.setText("");
        QTest::keyClicks(x.editor(), "<br>"); // elemento vacío
        QCOMPARE(x.text(), "<br>"s);
    }

    // --- Macros -------------------------------------------------------------------------

    void macrosFileRoundTripAndNotepadPlusPlusFormat()
    {
        const QString path = _tmp.filePath("shortcuts.xml");
        // shortcuts.xml de N++ (en un archivo: moc no entiende bien ese texto en una cadena)
        QFile::remove(path);
        QVERIFY(QFile::copy(QStringLiteral(EDITA_FIXTURES_DIR "/macros/shortcuts.xml"), path));
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
        std::vector<SavedMacro> macros;
        QVERIFY(readMacros(path, macros));
        QCOMPARE(macros.size(), std::size_t{2});
        QVERIFY(macros[0] == defaultTrimAndSaveMacro());
        const SavedMacro& old = macros[1];
        QVERIFY(old.ctrl && !old.alt);
        QCOMPARE(qtKeyFromVirtualKey(old.key), int(Qt::Key_F1));
        // "\r" + "\n" de una macro antigua quedan en un solo SCI_NEWLINE; el tipo 9 se ignora
        QCOMPARE(old.steps.size(), std::size_t{3});
        QCOMPARE(old.steps[0].sParam, "a"s);
        QCOMPARE(old.steps[1].message, SCI_NEWLINE);
        QCOMPARE(old.steps[2].message, SCI_LINEDOWN);

        macros[1].name = "Cambiada";
        QVERIFY(writeMacros(path, macros));
        std::vector<SavedMacro> again;
        QVERIFY(readMacros(path, again));
        QVERIFY(again == macros);
        const QByteArray xml = readAll(path);
        QVERIFY(xml.contains("<Shortcut id=\"41001\"")); // lo demás se conserva
        QVERIFY(xml.contains("<Command name=\"x\""));
        QVERIFY(xml.contains("<Macro name=\"Cambiada\" Ctrl=\"yes\" Alt=\"no\" Shift=\"no\" Key=\"112\">"));

        QCOMPARE(virtualKeyFromQtKey(Qt::Key_M), 77);
        QCOMPARE(virtualKeyFromQtKey(Qt::Key_F5), 0x74);
        QCOMPARE(qtKeyFromVirtualKey(0x28), int(Qt::Key_Down));
    }

    void recordAndPlayBack()
    {
        Window x;
        x.setPrefs([](Preferences& p) { p.autocStatus = 0; });
        QVERIFY(!x.action("actionPlayMacro")->isEnabled());
        x.w().toggleMacroRecording();
        QVERIFY(x.w().isRecordingMacro());
        QCOMPARE(x.action("actionRecordMacro")->text(), QStringLiteral("S&top Recording"));
        QTest::keyClicks(x.editor(), "hola");
        QTest::keyClick(x.editor(), Qt::Key_Return);
        x.action("actionDuplicateLine")->trigger(); // orden de menú
        x.w().toggleMacroRecording();
        QCOMPARE(x.text(), "hola\n\n"s);

        const Macro& macro = x.w().currentMacro();
        QCOMPARE(macro.size(), std::size_t{6}); // 4 letras, salto, orden
        QCOMPARE(macro[0].type, MacroStep::UseSParameter);
        QCOMPARE(macro[4].message, SCI_NEWLINE);
        QCOMPARE(macro[5].type, MacroStep::MenuCommand);
        QCOMPARE(macro[5].wParam, std::uintptr_t{42010}); // IDM_EDIT_DUP_LINE, sin el SCI_LINEDUPLICATE
        QVERIFY(x.action("actionPlayMacro")->isEnabled());

        x.editor()->sci(SCI_DOCUMENTEND);
        x.w().playMacro(macro);
        QCOMPARE(x.text(), "hola\n\nhola\n\n"s);
        // Un solo deshacer quita toda la reproducción
        x.editor()->sci(SCI_UNDO);
        QCOMPARE(x.text(), "hola\n\n"s);
    }

    void autoInsertedTextIsNotRecordedTwice()
    {
        Window x;
        x.setPrefs([](Preferences& p) {
            p.autocStatus = 0;
            p.insertParentheses = true;
        });
        x.w().toggleMacroRecording();
        QTest::keyClicks(x.editor(), "f(");
        x.w().toggleMacroRecording();
        QCOMPARE(x.text(), "f()"s);
        QCOMPARE(x.w().currentMacro().size(), std::size_t{2});
        x.setText("");
        x.w().playMacro(x.w().currentMacro());
        QCOMPARE(x.text(), "f()"s);
    }

    void runTimesAndUntilEndOfFile()
    {
        Window x;
        x.setPrefs([](Preferences& p) { p.autocStatus = 0; });
        x.setText("a\nb\nc\nd\ne");
        x.editor()->sci(SCI_DOCUMENTSTART);
        x.w().toggleMacroRecording();
        QTest::keyClick(x.editor(), Qt::Key_Home);
        QTest::keyClicks(x.editor(), "- ");
        QTest::keyClick(x.editor(), Qt::Key_Down);
        x.w().toggleMacroRecording();
        QCOMPARE(x.text(), "- a\nb\nc\nd\ne"s);

        x.w().runMacro(x.w().currentMacro(), 2);
        QCOMPARE(x.text(), "- a\n- b\n- c\nd\ne"s);
        x.w().runMacro(x.w().currentMacro(), -1);
        QCOMPARE(x.text(), "- a\n- b\n- c\n- d\n- e"s);
        // Todas las ejecuciones se deshacen juntas
        x.editor()->sci(SCI_UNDO);
        QCOMPARE(x.text(), "- a\n- b\n- c\nd\ne"s);
    }

    void savedMacrosLiveInShortcutsXml()
    {
        writeFile(file("t.txt"), "uno   \ndos\t\n");
        QString shortcuts;
        {
            Window x;
            shortcuts = x.w().shortcutsPath();
            // Primer arranque: shortcuts.xml con la macro de N++
            QVERIFY(QFileInfo::exists(shortcuts));
            QCOMPARE(x.w().savedMacros().size(), std::size_t{1});
            QAction* trim = x.action("actionMacro0");
            QVERIFY(trim);
            QCOMPARE(trim->shortcut(), QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_S));
            // Y funciona: quitar espacios del final y guardar
            x.w().openFile(file("t.txt"));
            trim->trigger();
            QCOMPARE(readAll(file("t.txt")), QByteArray("uno\ndos\n"));

            x.setPrefs([](Preferences& p) { p.autocStatus = 0; });
            x.w().toggleMacroRecording();
            QTest::keyClicks(x.editor(), "xy");
            x.w().toggleMacroRecording();
            x.prompter->macroNames.enqueue({QString(), QKeySequence()}); // cancelar
            QVERIFY(!x.w().saveCurrentMacro());
            x.prompter->macroNames.enqueue({QStringLiteral("Mi macro"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_M)});
            QVERIFY(x.w().saveCurrentMacro());
            QCOMPARE(x.w().savedMacros().size(), std::size_t{2});
            QVERIFY(readAll(shortcuts).contains("<Macro name=\"Mi macro\" Ctrl=\"yes\" Alt=\"yes\" Shift=\"no\" Key=\"77\">"));
        }
        Window y;
        QCOMPARE(y.w().savedMacros().size(), std::size_t{2});
        QAction* mine = y.action("actionMacro1");
        QVERIFY(mine);
        QCOMPARE(mine->text(), QStringLiteral("Mi macro"));
        QCOMPARE(mine->shortcut(), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_M));
        mine->trigger();
        QCOMPARE(y.text(), "xy"s);

        QVERIFY(y.w().deleteSavedMacro(0));
        QCOMPARE(y.w().savedMacros().size(), std::size_t{1});
        QCOMPARE(y.action("actionMacro0")->text(), QStringLiteral("Mi macro"));
        QVERIFY(!readAll(shortcuts).contains("Trim Trailing"));
    }

    void editCommands()
    {
        Window x;
        x.setPrefs([](Preferences& p) { p.autocStatus = 0; });
        x.setText("uno  \ndos\t \ntres");
        x.w().trimTrailingSpaces();
        QCOMPARE(x.text(), "uno\ndos\ntres"s);
        x.editor()->sci(SCI_UNDO);
        QCOMPARE(x.text(), "uno  \ndos\t \ntres"s);

        x.setText("abc\ndef");
        x.editor()->sci(SCI_DOCUMENTSTART);
        x.action("actionLineDown")->trigger();
        QCOMPARE(x.text(), "def\nabc"s);
        x.editor()->sci(SCI_SETSEL, 0, 3);
        x.action("actionUpperCase")->trigger();
        QCOMPARE(x.text(), "DEF\nabc"s);
        x.editor()->sci(SCI_SELECTALL);
        x.action("actionJoinLines")->trigger();
        QCOMPARE(x.text(), "DEF abc"s);
    }
};

QTEST_MAIN(TestEditing)
#include "tst_editing.moc"
