// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 11: plugins nativos (.so) con la API de edita_plugin.h, y el plugin de
// ejemplo EditaDemo (plugins/EditaDemo/EditaDemo.c).

#include "FakePrompter.h"

#include "ui/DocStatusBar.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/PluginManager.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QRegularExpression>
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
    }
    MainWindow& w() const { return *window; }
    EditorView* editor() const { return window->editor(); }
    std::string text() const { return window->documents().text(window->currentBuffer()); }
    const PluginManager::Plugin* demo() const
    {
        for (const auto& plugin : window->plugins()->plugins()) {
            if (plugin->name == QLatin1String("Demo"))
                return plugin.get();
        }
        return nullptr;
    }
    int cmd(int index) const { return demo()->commands.at(static_cast<std::size_t>(index)).cmdId; }
    QAction* action(int index) const
    {
        return window->findChild<QAction*>(QStringLiteral("pluginCommand%1").arg(cmd(index)));
    }
    // messageProc(1, código): notificaciones de ese código que ha recibido el plugin
    long received(unsigned int code) const { return static_cast<long>(demo()->messageProc(1, code, 0)); }
    intptr_t host(unsigned int message, uintptr_t w = 0, intptr_t l = 0) const
    {
        return window->handlePluginMessage(message, w, l);
    }
};

} // namespace

class TestPlugins : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    QString file(const QString& name) const { return _tmp.filePath(QStringLiteral("files/") + name); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        QVERIFY2(QFileInfo::exists(EDITA_PLUGIN_BUILD_DIR "/EditaDemo/EditaDemo.so"), "falta el plugin de ejemplo");
        qputenv("EDITA_PLUGIN_PATH", EDITA_PLUGIN_BUILD_DIR);
    }

    void cleanup() { QDir(_tmp.filePath("files")).removeRecursively(); }

    void loadsTheDemoPluginAndBuildsItsMenu()
    {
        Window x;
        QVERIFY(x.w().pluginWarnings().isEmpty());
        QVERIFY(x.w().pluginDirectories().first() == QStringLiteral(EDITA_PLUGIN_BUILD_DIR));
        const PluginManager::Plugin* demo = x.demo();
        QVERIFY(demo);
        QVERIFY(demo->path.endsWith(QStringLiteral("EditaDemo/EditaDemo.so")));
        QCOMPARE(demo->commands.size(), std::size_t{6});
        QCOMPARE(demo->commands[0].name, QStringLiteral("Hello, Edita"));
        QVERIFY(!demo->commands[4].function); // separador
        QCOMPARE(demo->commands[1].shortcut, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_D));
        QVERIFY(demo->commands[0].cmdId >= PluginManager::kFirstCommandId);

        auto* menu = x.w().findChild<QMenu*>(QStringLiteral("menuPlugins"));
        QVERIFY(menu);
        // El primero es el administrador de plugins; después, un submenú por plugin
        QCOMPARE(menu->actions().first()->objectName(), QStringLiteral("actionPluginsAdmin"));
        QMenu* sub = nullptr;
        for (QAction* a : menu->actions()) {
            if (a->menu() && a->text() == QLatin1String("Demo"))
                sub = a->menu();
        }
        QVERIFY(sub);
        QCOMPARE(sub->title(), QStringLiteral("Demo"));
        QCOMPARE(sub->actions().size(), 6);
        QVERIFY(sub->actions()[4]->isSeparator());
        QCOMPARE(x.action(1)->shortcut(), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_D));
    }

    void pluginCommandsUseScintillaAndHostMessages()
    {
        Window x;
        // NPPM_MENUCOMMAND(IDM_FILE_NEW) + SCI_REPLACESEL
        x.action(0)->trigger();
        QCOMPARE(x.w().mainPane()->count(), 2);
        QCOMPARE(x.text(), "Hello from an Edita plugin!\n"s);

        x.action(1)->trigger();
        QVERIFY(QRegularExpression(QStringLiteral("\\d{4}-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d$"))
                    .match(QString::fromStdString(x.text())).hasMatch());

        x.editor()->sends(SCI_SETTEXT, 0, "pera\nmanzana\nuva");
        x.action(2)->trigger();
        QCOMPARE(x.text(), "manzana\npera\nuva"s);
        // Un solo deshacer (el plugin usa SCI_REPLACETARGET)
        x.editor()->sci(SCI_UNDO);
        QCOMPARE(x.text(), "pera\nmanzana\nuva"s);

        // NPPM_SETSTATUSBAR
        x.action(3)->trigger();
        QCOMPARE(x.w().docStatusBar()->languageText(), QStringLiteral("Words: 3"));

        // Funciona igual en la vista secundaria (NPPM_GETCURRENTSCINTILLA)
        x.w().cloneToOtherView();
        QCOMPARE(x.w().pane(), x.w().subPane());
        x.editor()->sci(SCI_DOCUMENTEND);
        x.action(1)->trigger();
        QVERIFY(x.w().subPane()->editor()->sci(SCI_GETLENGTH) > 20);
    }

    void checkedItemsAndSaveNotification()
    {
        writeFile(file("guardar.txt"), "a\nb\n");
        Window x;
        x.w().openFile(file("guardar.txt"));
        QVERIFY(!x.action(5)->isChecked());
        x.action(5)->trigger(); // el plugin llama a NPPM_SETMENUITEMCHECK
        QVERIFY(x.action(5)->isCheckable());
        QVERIFY(x.action(5)->isChecked());
        QVERIFY(x.demo()->commands[5].checked);

        x.editor()->sends(SCI_REPLACESEL, 0, "c\n");
        QVERIFY(x.w().saveBuffer(x.w().currentBuffer()));
        QCOMPARE(x.w().docStatusBar()->languageText(), QStringLiteral("Saved: %1 (4 lines)").arg(file("guardar.txt")));

        x.action(5)->trigger();
        QVERIFY(!x.action(5)->isChecked());
    }

    void notificationsReachThePlugin()
    {
        writeFile(file("n1.txt"), "uno\n");
        writeFile(file("n2.cpp"), "int x;\n");
        Window x;
        const long opened = x.received(NPPN_FILEOPENED);
        const long activated = x.received(NPPN_BUFFERACTIVATED);
        const long before = x.received(NPPN_FILEBEFORECLOSE);
        const long closed = x.received(NPPN_FILECLOSED);
        const long saved = x.received(NPPN_FILESAVED);
        const long beforeSave = x.received(NPPN_FILEBEFORESAVE);
        const long lang = x.received(NPPN_LANGCHANGED);
        const long ready = x.received(NPPN_READY);
        const long chars = x.received(63);

        x.w().notifyPluginsReady();
        QCOMPARE(x.received(NPPN_READY), ready + 1);
        const BufferId a = x.w().openFile(file("n1.txt"));
        x.w().openFile(file("n2.cpp"));
        QCOMPARE(x.received(NPPN_FILEOPENED), opened + 2);
        QVERIFY(x.received(NPPN_BUFFERACTIVATED) >= activated + 2);
        x.w().setLanguage(LangType::Python);
        QCOMPARE(x.received(NPPN_LANGCHANGED), lang + 1);
        QTest::keyClicks(x.editor(), "abc");
        QCOMPARE(x.received(63), chars + 3); // SCN_CHARADDED de Scintilla
        QVERIFY(x.w().saveBuffer(x.w().currentBuffer()));
        QCOMPARE(x.received(NPPN_FILEBEFORESAVE), beforeSave + 1);
        QCOMPARE(x.received(NPPN_FILESAVED), saved + 1);
        QVERIFY(x.w().closeBuffer(a));
        QCOMPARE(x.received(NPPN_FILEBEFORECLOSE), before + 1);
        QCOMPARE(x.received(NPPN_FILECLOSED), closed + 1);

        const long shutdown = x.received(NPPN_SHUTDOWN);
        const long beforeShutdown = x.received(NPPN_BEFORESHUTDOWN);
        x.w().setSessionEnabled(false);
        QVERIFY(x.w().close());
        QCOMPARE(x.received(NPPN_BEFORESHUTDOWN), beforeShutdown + 1);
        QCOMPARE(x.received(NPPN_SHUTDOWN), shutdown + 1);
    }

    void hostMessages()
    {
        writeFile(file("ruta/datos.tar.gz"), "primera\nsegunda línea\n");
        Window x;
        const BufferId id = x.w().openFile(file("ruta/datos.tar.gz"));
        x.editor()->sci(SCI_GOTOLINE, 1);
        x.editor()->sci(SCI_CHARRIGHT);
        x.editor()->sci(SCI_CHARRIGHT);

        char buffer[512] = {};
        QCOMPARE(x.host(NPPM_GETFULLCURRENTPATH, sizeof buffer, reinterpret_cast<intptr_t>(buffer)), intptr_t{1});
        QCOMPARE(QString::fromUtf8(buffer), file("ruta/datos.tar.gz"));
        QCOMPARE(x.host(NPPM_GETFULLCURRENTPATH, 5, reinterpret_cast<intptr_t>(buffer)), intptr_t{0}); // búfer pequeño
        x.host(NPPM_GETFILENAME, sizeof buffer, reinterpret_cast<intptr_t>(buffer));
        QCOMPARE(QString::fromUtf8(buffer), QStringLiteral("datos.tar.gz"));
        x.host(NPPM_GETNAMEPART, sizeof buffer, reinterpret_cast<intptr_t>(buffer));
        QCOMPARE(QString::fromUtf8(buffer), QStringLiteral("datos.tar"));
        x.host(NPPM_GETEXTPART, sizeof buffer, reinterpret_cast<intptr_t>(buffer));
        QCOMPARE(QString::fromUtf8(buffer), QStringLiteral(".gz"));
        x.host(NPPM_GETCURRENTDIRECTORY, sizeof buffer, reinterpret_cast<intptr_t>(buffer));
        QCOMPARE(QString::fromUtf8(buffer), file("ruta"));
        x.host(NPPM_GETCURRENTWORD, sizeof buffer, reinterpret_cast<intptr_t>(buffer));
        QCOMPARE(QString::fromUtf8(buffer), QStringLiteral("segunda"));
        QCOMPARE(x.host(NPPM_GETCURRENTLINE), intptr_t{1});
        QCOMPARE(x.host(NPPM_GETCURRENTCOLUMN), intptr_t{2});

        QCOMPARE(x.host(NPPM_GETCURRENTBUFFERID), static_cast<intptr_t>(id));
        QCOMPARE(x.host(NPPM_GETFULLPATHFROMBUFFERID, id, 0), static_cast<intptr_t>(file("ruta/datos.tar.gz").toUtf8().size()));
        QCOMPARE(x.host(NPPM_GETFULLPATHFROMBUFFERID, 999999, 0), intptr_t{-1});
        QCOMPARE(x.host(NPPM_GETNBOPENFILES, 0, ALL_OPEN_FILES), intptr_t{1});
        int view = -1;
        x.host(NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<intptr_t>(&view));
        QCOMPARE(view, 0);

        x.host(NPPM_SETCURRENTLANGTYPE, 0, static_cast<intptr_t>(LangType::Python));
        int lang = -1;
        x.host(NPPM_GETCURRENTLANGTYPE, 0, reinterpret_cast<intptr_t>(&lang));
        QCOMPARE(lang, static_cast<int>(LangType::Python));
        QCOMPARE(x.host(NPPM_GETBUFFERLANGTYPE, id), static_cast<intptr_t>(LangType::Python));

        // NPPM_MENUCOMMAND con un IDM de N++ (IDM_EDIT_UPPERCASE)
        x.editor()->sci(SCI_SETSEL, 0, 7);
        QCOMPARE(x.host(NPPM_MENUCOMMAND, 0, 42016), intptr_t{1});
        QVERIFY(x.text().starts_with("PRIMERA"));

        writeFile(file("otro.txt"), "x");
        QCOMPARE(x.host(NPPM_DOOPEN, 0, reinterpret_cast<intptr_t>(file("otro.txt").toUtf8().constData())), intptr_t{1});
        QCOMPARE(x.host(NPPM_GETNBOPENFILES, 0, PRIMARY_VIEW), intptr_t{2});
        QCOMPARE(x.host(NPPM_SWITCHTOFILE, 0, reinterpret_cast<intptr_t>(file("ruta/datos.tar.gz").toUtf8().constData())), intptr_t{1});
        QCOMPARE(x.w().currentBuffer(), id);
        QCOMPARE(x.host(NPPM_SAVECURRENTFILE), intptr_t{1});
        QCOMPARE(x.host(NPPM_SAVECURRENTFILE), intptr_t{0}); // ya no hay nada que guardar
        x.host(NPPM_GETPLUGINSCONFIGDIR, sizeof buffer, reinterpret_cast<intptr_t>(buffer));
        QVERIFY(QFileInfo(QString::fromUtf8(buffer)).isDir());
        QCOMPARE(x.host(0xFFFF), intptr_t{0}); // desconocido
    }

    void pluginCommandsCanBeRecordedInMacros()
    {
        Window x;
        x.editor()->sends(SCI_SETTEXT, 0, "b\na");
        x.w().toggleMacroRecording();
        x.action(2)->trigger(); // Ordenar líneas
        x.w().toggleMacroRecording();
        QCOMPARE(x.w().currentMacro().size(), std::size_t{1}); // la orden, no lo que hizo con Scintilla
        QCOMPARE(x.w().currentMacro()[0].wParam, static_cast<std::uintptr_t>(x.cmd(2)));
        x.editor()->sends(SCI_SETTEXT, 0, "z\ny\nx");
        x.w().playMacro(x.w().currentMacro());
        QCOMPARE(x.text(), "x\ny\nz"s);
    }

    void brokenAndDuplicatePluginsAreReported()
    {
        const QString dir = _tmp.filePath("files/plugins");
        writeFile(dir + "/Roto/Roto.so", "esto no es una biblioteca");
        QDir().mkpath(dir + "/Copia");
        QVERIFY(QFile::copy(EDITA_PLUGIN_BUILD_DIR "/EditaDemo/EditaDemo.so", dir + "/Copia/Copia.so"));
        writeFile(dir + "/SinBiblioteca/otra-cosa.txt", "");
        qputenv("EDITA_PLUGIN_PATH", QByteArray(EDITA_PLUGIN_BUILD_DIR) + ":" + dir.toUtf8());
        Window x;
        qputenv("EDITA_PLUGIN_PATH", EDITA_PLUGIN_BUILD_DIR);
        // Solo se cargan los del build (EditaDemo y, si se compiló, PythonScript)
        for (const auto& plugin : x.w().plugins()->plugins())
            QVERIFY2(plugin->path.startsWith(QStringLiteral(EDITA_PLUGIN_BUILD_DIR)), qPrintable(plugin->path));
        const QStringList warnings = x.w().pluginWarnings();
        QCOMPARE(warnings.size(), 2);
        QVERIFY(warnings.filter(QStringLiteral("Roto.so")).size() == 1);
        QVERIFY(warnings.filter(QStringLiteral("Copia.so")).first().contains(QStringLiteral("\"Demo\"")));
    }
};

QTEST_MAIN(TestPlugins)
#include "tst_plugins.moc"
