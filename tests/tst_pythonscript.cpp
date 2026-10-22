// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 13: plugin Python Script (plugins/PythonScript), con la API del PythonScript
// de Notepad++. Los scripts se ejecutan como lo haría otro plugin: NPPM_MSGTOPLUGIN
// con PYSCR_EXECSTATEMENT / PYSCR_EXECSCRIPT.

#include "FakePrompter.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/PluginManager.h"

#include "../plugins/PythonScript/NppPythonScript.h"

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
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

struct Window
{
    std::unique_ptr<MainWindow> window = std::make_unique<MainWindow>();
    FakePrompter* prompter;

    Window()
    {
        auto fake = std::make_unique<FakePrompter>();
        prompter = fake.get();
        window->setPrompter(std::move(fake));
        Preferences p = window->preferences();
        p.autocStatus = 0;
        window->setPreferences(p);
        window->notifyPluginsReady();
    }
    MainWindow& w() const { return *window; }
    EditorView* editor() const { return window->editor(); }
    std::string text() const { return window->documents().text(window->currentBuffer()); }

    // PYSCR_EXECSTATEMENT síncrono, como otro plugin
    bool exec(const QByteArray& statement, long message = PYSCR_EXECSTATEMENT) const
    {
        PythonScript_Exec exec{1, statement.constData(), nullptr, PYSCRF_SYNC, 0};
        EditaCommunicationInfo info{message, "tst.so", &exec};
        const bool sent = window->handlePluginMessage(NPPM_MSGTOPLUGIN, reinterpret_cast<uintptr_t>("PythonScript.so"),
                                                      reinterpret_cast<intptr_t>(&info))
            != 0;
        return sent && exec.deliverySuccess == 1;
    }
    ScintillaEditBase* consoleOutput() const
    {
        return window->findChild<ScintillaEditBase*>(QStringLiteral("pythonScriptConsoleOutput"));
    }
    QString consoleText() const
    {
        ScintillaEditBase* out = consoleOutput();
        if (!out)
            return {};
        const auto length = static_cast<std::size_t>(out->send(SCI_GETLENGTH));
        std::string text(length + 1, '\0');
        out->send(SCI_GETTEXT, length + 1, reinterpret_cast<sptr_t>(text.data()));
        text.resize(length);
        return QString::fromStdString(text);
    }
    // repr() de una expresión de Python, escrita en la consola
    QString eval(const QByteArray& expression) const
    {
        exec("console.clear()");
        exec("console.write(repr(" + expression + "))");
        return consoleText();
    }
};

} // namespace

class TestPythonScript : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    QString file(const QString& name) const { return _tmp.filePath(QStringLiteral("files/") + name); }
    QString userScripts() const { return _tmp.filePath(QStringLiteral("config/edita/plugins/config/PythonScript/scripts")); }
    QString cnf() const { return _tmp.filePath(QStringLiteral("config/edita/plugins/config/PythonScriptStartup.cnf")); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        QVERIFY2(QFileInfo::exists(EDITA_PLUGIN_BUILD_DIR "/PythonScript/PythonScript.so"), "falta el plugin PythonScript");
        qputenv("EDITA_PLUGIN_PATH", EDITA_PLUGIN_BUILD_DIR);
    }

    void cleanup()
    {
        QDir(_tmp.filePath("files")).removeRecursively();
        QDir(userScripts()).removeRecursively();
        QFile::remove(cnf());
    }

    void pluginLoadsWithItsMenu()
    {
        Window x;
        QVERIFY2(x.w().pluginWarnings().isEmpty(), qPrintable(x.w().pluginWarnings().join('\n')));
        auto* plugins = x.w().findChild<QMenu*>(QStringLiteral("menuPlugins"));
        QMenu* menu = nullptr;
        for (QAction* a : plugins->actions()) {
            if (a->menu() && a->text() == QLatin1String("Python Script"))
                menu = a->menu();
        }
        QVERIFY(menu);
        QStringList texts;
        for (QAction* a : menu->actions())
            texts << (a->isSeparator() ? QStringLiteral("--") : a->text());
        // Mismo orden que PythonScript; Scripts se añade en NPPN_READY
        QCOMPARE(texts, (QStringList{"New Script", "Show Console", "--", "Stop Script", "--", "Run Previous Script", "--",
                                     "Scripts", "--", "Configuration...", "--", "Context-Help", "About"}));
        auto* scripts = menu->findChild<QMenu*>(QStringLiteral("pythonScriptScriptsMenu"));
        QVERIFY(scripts);
        QStringList samples;
        for (QAction* a : scripts->actions()) {
            if (a->menu())
                samples << a->text();
        }
        QVERIFY(samples.contains(QStringLiteral("Samples"))); // los de la máquina, sin startup.py
    }

    void editorObjectDrivesScintilla()
    {
        Window x;
        QVERIFY(x.exec("editor.setText('uno\\ndos\\ntres')"));
        QCOMPARE(x.text(), "uno\ndos\ntres"s);
        QCOMPARE(x.eval("editor.getText()"), QStringLiteral("'uno\\ndos\\ntres'"));
        QCOMPARE(x.eval("editor.getLineCount()"), QStringLiteral("3"));
        QCOMPARE(x.eval("editor.getLine(1)"), QStringLiteral("'dos\\n'"));
        QCOMPARE(x.eval("editor.getTextRange(4, 7)"), QStringLiteral("'dos'"));
        x.exec("editor.gotoPos(0); editor.addText('¡ñ ')");
        QCOMPARE(x.text(), "¡ñ uno\ndos\ntres"s);
        QCOMPARE(x.eval("editor.getCurrentPos()"), QStringLiteral("5")); // bytes UTF-8: ¡ y ñ ocupan 2
        x.exec("editor.setSel(0, 5)");
        QCOMPARE(x.eval("editor.getSelText()"), QStringLiteral("'¡ñ '"));
        QCOMPARE(x.eval("editor.getModify()"), QStringLiteral("True"));
        // Colores como tuplas y propiedades con clave de texto
        x.exec("editor.styleSetFore(1, (255, 0, 128))");
        QCOMPARE(x.eval("editor.styleGetFore(1)"), QStringLiteral("(255, 0, 128)"));
        // Helpers de PythonScript
        x.exec("editor.replaceLine(1, 'DOS')");
        x.exec("editor.deleteLine(0)");
        QCOMPARE(x.text(), "DOS\ntres"s);
        x.exec("editor.forEachLine(lambda text, n, total: editor.replaceLine(n, text.lower()))");
        QCOMPARE(x.text(), "dos\ntres"s);
        QCOMPARE(x.eval("editor.getUserLineSelection()"), QStringLiteral("(0, 1)"));
        x.exec("editor.gotoPos(1)");
        QCOMPARE(x.eval("editor.getCurrentWord()"), QStringLiteral("'dos'"));
        QCOMPARE(x.eval("editor.findText(0, 0, editor.getLength(), 'tres')"), QStringLiteral("(4, 8)"));
        // editor1 es la vista principal
        QCOMPARE(x.eval("editor1.getText() == editor.getText()"), QStringLiteral("True"));
    }

    void searchAndReplace()
    {
        Window x;
        x.exec("editor.setText('X56 X39 X999\\r\\nñX1 fin')");
        x.editor()->sci(SCI_SETEOLMODE, SC_EOL_CRLF);
        // rereplace con función: Y + número + 1
        x.exec("editor.rereplace(r'X([0-9]+)', lambda m: 'Y' + str(int(m.group(1)) + 1))");
        QCOMPARE(x.text(), "Y57 Y40 Y1000\r\nñY2 fin"s);
        x.exec("editor.undo()"); // una sola acción
        QCOMPARE(x.text(), "X56 X39 X999\r\nñX1 fin"s);
        // Referencias de Boost ($1) y de Python (\\1); ^ y $ por línea con CRLF
        x.exec("editor.rereplace(r'^X(\\d+)', r'[$1]')");
        QCOMPARE(x.text(), "[56] X39 X999\r\nñX1 fin"s);
        x.exec("editor.rereplace(r'(\\w+)$', r'<\\1>')");
        QCOMPARE(x.text(), "[56] X39 <X999>\r\nñX1 <fin>"s);
        // research: posiciones del documento en bytes (la ñ ocupa 2)
        x.exec("m = []; editor.research(r'X1', lambda r: m.append(r.span(0)))");
        QCOMPARE(x.eval("m"), QStringLiteral("[(19, 21)]")); // 15 + CRLF + ñ
        // replace literal, sin distinguir mayúsculas y con máximo
        x.exec("editor.setText('a.b A.B a.b')");
        x.exec("import re; editor.replace('a.b', 'c', re.IGNORECASE, 0, -1, 2)");
        QCOMPARE(x.text(), "c c a.b"s);
        x.exec("found = []; editor.search('c', lambda r: found.append(r.start()))");
        QCOMPARE(x.eval("found"), QStringLiteral("[0, 2]"));
        // Errores como en PythonScript
        x.exec("editor.replace('', 'x')");
        QVERIFY(x.consoleText().contains(QStringLiteral("TypeError")));
    }

    void notepadObject()
    {
        writeFile(file("a.txt"), "alfa\n");
        writeFile(file("b.py"), "print(1)\n");
        Window x;
        QVERIFY(x.exec(QByteArray("notepad.open(r'") + file("a.txt").toUtf8() + "')"));
        x.exec(QByteArray("notepad.open(r'") + file("b.py").toUtf8() + "')");
        QCOMPARE(x.eval("notepad.getCurrentFilename()"), QStringLiteral("'%1'").arg(file("b.py")));
        QCOMPARE(x.eval("notepad.getLangType() == LANGTYPE.PYTHON"), QStringLiteral("True"));
        QCOMPARE(x.eval("editor.getLexerLanguage()"), QStringLiteral("'python'"));
        x.exec("notepad.new(); notepad.setLangType(LANGTYPE.CPP)");
        QCOMPARE(x.eval("editor.getLexerLanguage()"), QStringLiteral("'cpp'"));
        x.exec("notepad.setLangType(LANGTYPE.RUST)"); // por encima de L_USER (15)
        QCOMPARE(x.eval("editor.getLexerLanguage()"), QStringLiteral("'rust'"));
        QCOMPARE(x.eval("notepad.setLangType(LANGTYPE.USER)"), QStringLiteral("False"));
        x.exec("notepad.close()");
        QCOMPARE(x.eval("notepad.getLanguageName(LANGTYPE.PYTHON)"), QStringLiteral("'Python'"));
        QCOMPARE(x.eval("[f[0].split('/')[-1] for f in notepad.getFiles()]"), QStringLiteral("['a.txt', 'b.py']"));
        QCOMPARE(x.eval("notepad.getFiles()[0][2:]"), QStringLiteral("(0, 0)"));
        x.exec(QByteArray("notepad.activateFile(r'") + file("a.txt").toUtf8() + "')");
        QCOMPARE(x.text(), "alfa\n"s);
        x.exec("notepad.activateBufferID(notepad.getFiles()[1][1])");
        QCOMPARE(x.text(), "print(1)\n"s);
        QCOMPARE(x.eval("notepad.getFormatType() == FORMATTYPE.UNIX"), QStringLiteral("True"));
        QCOMPARE(x.eval("notepad.getEncoding() == BUFFERENCODING.UTF8_NOBOM"), QStringLiteral("True"));
        // menuCommand con los IDM de N++
        x.exec("editor.setSel(0, 5); notepad.menuCommand(MENUCOMMAND.EDIT_UPPERCASE)");
        QCOMPARE(x.text(), "PRINT(1)\n"s);
        x.exec("notepad.new()");
        QCOMPARE(x.eval("len(notepad.getFiles())"), QStringLiteral("3"));
        x.exec("editor.setText('nuevo')");
        x.exec(QByteArray("notepad.saveAs(r'") + file("c.txt").toUtf8() + "')");
        QVERIFY(QFileInfo::exists(file("c.txt")));
        QCOMPARE(x.eval("notepad.getCurrentFilename().endswith('c.txt')"), QStringLiteral("True"));
        // runMenuCommand por el texto del menú; versión y carpetas
        x.exec("notepad.runMenuCommand('Edit', 'Select All')");
        QCOMPARE(x.editor()->sci(SCI_GETSELECTIONEND), sptr_t{5});
        QVERIFY(x.eval("notepad.getVersion()").startsWith('('));
        QVERIFY(x.eval("notepad.getPluginConfigDir()").contains(QStringLiteral("plugins/config")));
        QCOMPARE(x.eval("notepad.allocateMarker(2)"), QStringLiteral("1"));
        x.exec("notepad.setStatusBar(STATUSBARSECTION.DOCTYPE, 'desde Python')");
    }

    void consoleAndErrors()
    {
        Window x;
        x.exec("console.clear(); print('hola')");
        QCOMPARE(x.consoleText(), QStringLiteral("hola\n"));
        // Los errores van a la consola en rojo y la abren (OPENCONSOLEONERROR)
        x.exec("console.clear(); 1/0");
        QVERIFY(x.consoleText().contains(QStringLiteral("ZeroDivisionError")));
        QCOMPARE(x.consoleOutput()->send(SCI_GETSTYLEAT, 0), sptr_t{1});
        auto* dock = x.w().findChild<QDockWidget*>(QStringLiteral("pluginDock_PythonScript.so_1"));
        QVERIFY(dock);
        QVERIFY(!dock->isHidden());
        // console.editor es un Editor de la consola
        x.exec("console.write('abc'); n = console.editor.getLength()");
        QCOMPARE(x.eval("n > 0"), QStringLiteral("True"));
        // console.run: un programa externo
        x.exec("console.clear(); console.run('echo desde-fuera')");
        QCOMPARE(x.consoleText(), QStringLiteral("desde-fuera\n"));
        // Una línea escrita en la consola interactiva, también de varias líneas
        auto* input = x.w().findChild<QLineEdit*>(QStringLiteral("pythonScriptConsoleInput"));
        QVERIFY(input);
        x.exec("console.clear()");
        input->setText(QStringLiteral("for i in range(2):"));
        QTest::keyClick(input, Qt::Key_Return);
        input->setText(QStringLiteral("    print(i * 10)"));
        QTest::keyClick(input, Qt::Key_Return);
        input->setText(QString());
        QTest::keyClick(input, Qt::Key_Return);
        // La consola ejecuta en el hilo de los scripts
        QTRY_VERIFY2(x.consoleText().contains(QStringLiteral("0\n10\n")), qPrintable(x.consoleText()));
    }

    void callbacks()
    {
        writeFile(file("cb.txt"), "x");
        Window x;
        x.w().show();
        QVERIFY(QTest::qWaitForWindowExposed(&x.w()));
        // notepad.callback: síncrono, con bufferID
        x.exec("saved = []\n"
               "notepad.callback(lambda a: saved.append(notepad.getBufferFilename(a['bufferID'])), [NOTIFICATION.FILESAVED])");
        x.w().openFile(file("cb.txt"));
        x.editor()->sends(SCI_APPENDTEXT, 1, "y");
        QVERIFY(x.w().saveBuffer(x.w().currentBuffer()));
        QCOMPARE(x.eval("[s.split('/')[-1] for s in saved]"), QStringLiteral("['cb.txt']"));
        // editor.callback: asíncrono (al volver al bucle de eventos); callbackSync: en el acto
        x.exec("chars = []; sync = []\n"
               "editor.callback(lambda a: chars.append(chr(a['ch'])), [SCINTILLANOTIFICATION.CHARADDED])\n"
               "editor.callbackSync(lambda a: sync.append(a['ch']), [SCINTILLANOTIFICATION.CHARADDED])");
        x.editor()->setFocus();
        QTest::keyClicks(x.editor(), QStringLiteral("ab"));
        QCOMPARE(x.eval("len(sync)"), QStringLiteral("2"));
        QTRY_COMPARE(x.eval("chars"), QStringLiteral("['a', 'b']"));
        // clearCallbacks con la lista de eventos
        x.exec("editor.clearCallbacks([SCINTILLANOTIFICATION.CHARADDED])");
        QTest::keyClicks(x.editor(), QStringLiteral("c"));
        QCoreApplication::processEvents();
        QCOMPARE(x.eval("len(sync)"), QStringLiteral("2"));
        // Un hilo de Python puede usar el editor (las llamadas pasan al hilo de la interfaz)
        x.exec("import threading\n"
               "t = threading.Thread(target=lambda: editor.appendText('+hilo'))\n"
               "t.start()");
        QTRY_VERIFY(QString::fromStdString(x.text()).endsWith(QStringLiteral("+hilo")));
    }

    void scriptsRunInTheirOwnThread()
    {
        Window x;
        x.w().show();
        QVERIFY(QTest::qWaitForWindowExposed(&x.w()));
        // Un script al estilo de los tests de PythonScript: espera con sleep a que llegue
        // su callback asíncrono, que se ejecuta mientras el script sigue
        const QByteArray script =
            "import time\n"
            "called = []\n"
            "editor.callback(lambda a: called.append(editor.getLength()), [SCINTILLANOTIFICATION.MODIFIED])\n"
            "editor.setText('12345')\n"
            "for _ in range(50):\n"
            "    if called: break\n"
            "    time.sleep(0.02)\n"
            "editor.appendText(' ok' if called else ' sin callback')\n";
        PythonScript_Exec exec{1, script.constData(), nullptr, 0, 0};
        EditaCommunicationInfo info{PYSCR_EXECSTATEMENT, "tst.so", &exec};
        x.w().handlePluginMessage(NPPM_MSGTOPLUGIN, reinterpret_cast<uintptr_t>("PythonScript.so"),
                                  reinterpret_cast<intptr_t>(&info));
        QTRY_COMPARE(x.text(), "12345 ok"s);
        x.exec("editor.clearCallbacks()");

        // Stop Script: KeyboardInterrupt en un bucle sin fin
        PythonScript_Exec loop{1, "import time\nwhile True:\n    time.sleep(0.01)\n", nullptr, 0, 0};
        EditaCommunicationInfo loopInfo{PYSCR_EXECSTATEMENT, "tst.so", &loop};
        x.exec("console.clear()");
        x.w().handlePluginMessage(NPPM_MSGTOPLUGIN, reinterpret_cast<uintptr_t>("PythonScript.so"),
                                  reinterpret_cast<intptr_t>(&loopInfo));
        QTRY_COMPARE(x.eval("__import__('Npp')._script_running()"), QStringLiteral("True"));
        QAction* stop = nullptr;
        for (QAction* a : x.w().findChildren<QAction*>()) {
            if (a->text() == QLatin1String("Stop Script"))
                stop = a;
        }
        QVERIFY(stop);
        stop->trigger();
        QTRY_VERIFY2(x.consoleText().contains(QStringLiteral("KeyboardInterrupt")), qPrintable(x.consoleText()));
        QTRY_COMPARE(x.eval("__import__('Npp')._script_running()"), QStringLiteral("False"));

        // Stop Script justo al lanzar el script (antes de que empiece o al empezar): el
        // script no se queda ejecutando y el hilo de los scripts sigue vivo
        for (int i = 0; i < 30; ++i) {
            x.w().handlePluginMessage(NPPM_MSGTOPLUGIN, reinterpret_cast<uintptr_t>("PythonScript.so"),
                                      reinterpret_cast<intptr_t>(&loopInfo));
            if (i % 3 == 1)
                QTest::qWait(1);
            stop->trigger();
            QTRY_COMPARE(x.eval("__import__('Npp')._script_running()"), QStringLiteral("False"));
        }
        x.editor()->sends(SCI_SETTEXT, 0, "");
        PythonScript_Exec after{1, "editor.setText('sigue vivo')", nullptr, 0, 0};
        EditaCommunicationInfo afterInfo{PYSCR_EXECSTATEMENT, "tst.so", &after};
        x.w().handlePluginMessage(NPPM_MSGTOPLUGIN, reinterpret_cast<uintptr_t>("PythonScript.so"),
                                  reinterpret_cast<intptr_t>(&afterInfo));
        QTRY_COMPARE(x.text(), "sigue vivo"s);
    }

    void enumsMatchNotepadPlusPlus()
    {
        Window x;
        QCOMPARE(x.eval("(LANGTYPE.TXT, LANGTYPE.PYTHON, LANGTYPE.USER)"), QStringLiteral("(0, 22, 15)"));
        QCOMPARE(x.eval("(NOTIFICATION.READY, NOTIFICATION.FILESAVED)"), QStringLiteral("(1001, 1008)"));
        QCOMPARE(x.eval("(MENUCOMMAND.FILE_NEW, MENUCOMMAND.EDIT_BLOCK_COMMENT)"), QStringLiteral("(41001, 42022)"));
        QCOMPARE(x.eval("(SCINTILLANOTIFICATION.CHARADDED, SCINTILLANOTIFICATION.MODIFIED)"), QStringLiteral("(2001, 2008)"));
        QCOMPARE(x.eval("(FINDOPTION.MATCHCASE, MODIFICATIONFLAGS.INSERTTEXT, WRAP.WORD)"), QStringLiteral("(4, 1, 1)"));
        QCOMPARE(x.eval("MESSAGEBOXFLAGS.RESULTYES"), QStringLiteral("6"));
    }

    void scriptsFromMenusAndOtherPlugins()
    {
        writeFile(userScripts() + "/Mio.py", "editor.setText('desde script')\n");
        writeFile(userScripts() + "/startup.py", "editor.setText('startup del usuario')\n");
        writeFile(cnf(), "ITEM/Mio.py\nSETTING/STARTUP/ATSTARTUP\n");
        Window x;
        // ATSTARTUP: Python y el startup.py del usuario al terminar de arrancar
        QCOMPARE(x.text(), "startup del usuario"s);
        // El script configurado está en el menú del plugin y en Scripts
        QAction* item = nullptr;
        for (QAction* a : x.w().findChildren<QAction*>()) {
            if (a->text() == QLatin1String("Mio") && a->toolTip().endsWith(QLatin1String("Mio.py")))
                item = a;
        }
        QVERIFY(item);
        item->trigger(); // en el hilo de los scripts, como en PythonScript
        QTRY_COMPARE(x.text(), "desde script"s);
        // Run Previous Script
        x.exec("editor.setText('')");
        QAction* previous = nullptr;
        for (QAction* a : x.w().findChildren<QAction*>()) {
            if (a->text().startsWith(QLatin1String("Run Previous Script")))
                previous = a;
        }
        QVERIFY(previous);
        QCOMPARE(previous->text(), QStringLiteral("Run Previous Script (Mio)"));
        QTRY_COMPARE(x.eval("__import__('Npp')._script_running()"), QStringLiteral("False"));
        previous->trigger();
        QTRY_COMPARE(x.text(), "desde script"s);
        // PYSCR_EXECSCRIPT con la ruta de un archivo
        writeFile(file("otro.py"), "editor.appendText(' + otro')\n");
        QVERIFY(x.exec(file("otro.py").toUtf8(), PYSCR_EXECSCRIPT));
        QCOMPARE(x.text(), "desde script + otro"s);
        // Sin PYSCRF_SYNC se ejecuta al volver al bucle de eventos
        PythonScript_Exec later{1, "editor.appendText('!')", nullptr, 0, 0};
        EditaCommunicationInfo info{PYSCR_EXECSTATEMENT, "tst.so", &later};
        x.w().handlePluginMessage(NPPM_MSGTOPLUGIN, reinterpret_cast<uintptr_t>("PythonScript.so"),
                                  reinterpret_cast<intptr_t>(&info));
        QCOMPARE(later.deliverySuccess, 1);
        QCOMPARE(x.text(), "desde script + otro"s);
        QTRY_COMPARE(x.text(), "desde script + otro!"s);
        // Un módulo que no existe no se puede mandar
        EditaCommunicationInfo none{PYSCR_EXECSTATEMENT, "tst.so", &later};
        QCOMPARE(x.w().handlePluginMessage(NPPM_MSGTOPLUGIN, reinterpret_cast<uintptr_t>("NoExiste.so"),
                                           reinterpret_cast<intptr_t>(&none)),
                 intptr_t{0});
    }

    void secondWindowStartsFresh()
    {
        // El intérprete sigue vivo, pero Npp se vuelve a importar para la ventana nueva
        {
            Window x;
            x.exec("editor.callback(lambda a: None, [SCINTILLANOTIFICATION.MODIFIED])");
        }
        Window y;
        y.exec("editor.setText('segunda')");
        QCOMPARE(y.text(), "segunda"s);
        QCOMPARE(y.eval("len(editor._callbacks.entries)"), QStringLiteral("0"));
    }
};

QTEST_MAIN(TestPythonScript)
#include "tst_pythonscript.moc"
