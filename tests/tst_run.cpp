// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Menú Ejecutar (RunDlg y UserCommand de N++): variables, ejecutar, órdenes guardadas en
// shortcuts.xml y su pestaña en la Asignación de atajos.

#include "FakePrompter.h"
#include "core/Macro.h"
#include "core/UserCommands.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/RunDialog.h"
#include "ui/ShortcutMapperDialog.h"

#include <QAction>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace edita;
using edita::test::FakePrompter;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QString readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString();
}

} // namespace

// Recibe las direcciones que Edita manda al navegador
class UrlCatcher : public QObject
{
    Q_OBJECT
public:
    QList<QUrl> urls;
public slots:
    void open(const QUrl& url) { urls << url; }
};

class TestRun : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    UrlCatcher _urls;

    std::unique_ptr<MainWindow> window(FakePrompter** prompter = nullptr)
    {
        auto w = std::make_unique<MainWindow>();
        auto p = std::make_unique<FakePrompter>();
        if (prompter)
            *prompter = p.get();
        w->setPrompter(std::move(p));
        return w;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        QDesktopServices::setUrlHandler(QStringLiteral("https"), &_urls, "open");
    }

    void cleanupTestCase() { QDesktopServices::unsetUrlHandler(QStringLiteral("https")); }

    void expandsVariables()
    {
        RunVariables v;
        v.fullCurrentPath = QStringLiteral("/home/ana/src/main.cpp");
        v.currentWord = QStringLiteral("total");
        v.currentLineText = QStringLiteral("int total = 0;");
        v.line = 12;
        v.column = 5;
        v.appDirectory = QStringLiteral("/opt/edita");
        v.appPath = QStringLiteral("/opt/edita/edita");
        QCOMPARE(expandRunVariables(QStringLiteral("g++ \"$(FULL_CURRENT_PATH)\" -o $(NAME_PART)"), v),
                 QStringLiteral("g++ \"/home/ana/src/main.cpp\" -o main"));
        QCOMPARE(expandRunVariables(QStringLiteral("$(CURRENT_DIRECTORY)|$(FILE_NAME)|$(EXT_PART)"), v),
                 QStringLiteral("/home/ana/src|main.cpp|.cpp"));
        QCOMPARE(expandRunVariables(QStringLiteral("$(CURRENT_WORD) $(CURRENT_LINE):$(CURRENT_COLUMN) [$(CURRENT_LINESTR)]"), v),
                 QStringLiteral("total 12:5 [int total = 0;]"));
        QCOMPARE(expandRunVariables(QStringLiteral("$(NPP_DIRECTORY) $(NPP_FULL_FILE_PATH)"), v),
                 QStringLiteral("/opt/edita /opt/edita/edita"));
        // Lo que no es una variable se queda (en la shell, $(orden) es otra cosa)
        QCOMPARE(expandRunVariables(QStringLiteral("echo $(date) $(FILE_NAME) $(NOPE"), v),
                 QStringLiteral("echo $(date) main.cpp $(NOPE"));
        QVERIFY(isUrlCommand(QStringLiteral(" https://www.php.net/strlen ")));
        QVERIFY(!isUrlCommand(QStringLiteral("xdg-open https://example.com")));
        QVERIFY(!isUrlCommand(QStringLiteral("make")));
    }

    void shortcutsXmlKeepsTheRest()
    {
        const QString path = _tmp.filePath("shortcuts-test.xml");
        QVERIFY(writeMacros(path, {defaultTrimAndSaveMacro()}));
        const std::vector<UserCommand> commands{
            {QStringLiteral("Make"), true, false, true, 'M', QStringLiteral("make -C \"$(CURRENT_DIRECTORY)\" && echo <ok>")},
            {QStringLiteral("Sin atajo"), false, false, false, 0, QStringLiteral("ls")}};
        QVERIFY(writeUserCommands(path, commands));
        std::vector<UserCommand> read;
        QVERIFY(readUserCommands(path, read));
        QCOMPARE(read, commands);
        std::vector<SavedMacro> macros;
        QVERIFY(readMacros(path, macros));
        QCOMPARE(macros.size(), size_t(1));
        // Las de serie: las tres de N++ con sus teclas (Alt+F1, Alt+F3, Alt+F6)
        const auto defaults = defaultUserCommands(QStringLiteral("/usr/bin/edita"));
        QCOMPARE(defaults.size(), size_t(3));
        QCOMPARE(defaults[1].key, 114);
        QVERIFY(defaults[2].command.contains(QStringLiteral("/usr/bin/edita")));
    }

    void runsCommands()
    {
        auto w = window();
        // En el primer arranque, shortcuts.xml trae las órdenes de N++
        QCOMPARE(w->userCommands().size(), size_t(3));
        QVERIFY(w->findChild<QAction*>(QStringLiteral("actionRunCommand1")));
        const QString file = _tmp.filePath("work/notas.txt");
        writeFile(file, "primera\nsegunda línea\n");
        w->openFile(file);
        w->editor()->send(SCI_GOTOLINE, 1);
        w->editor()->send(SCI_WORDRIGHT);
        QCOMPARE(w->expandRunCommand(QStringLiteral("$(FILE_NAME) $(CURRENT_LINE) $(CURRENT_WORD)")),
                 QStringLiteral("notas.txt 2 línea"));
        // Con /bin/sh, en la carpeta del documento
        QVERIFY(w->runCommand(QStringLiteral("pwd > salida.txt; echo \"$(NAME_PART)\" >> salida.txt")));
        QTRY_COMPARE(readFile(_tmp.filePath("work/salida.txt")).split('\n').size(), 2);
        const QStringList lines = readFile(_tmp.filePath("work/salida.txt")).split('\n');
        QCOMPARE(QFileInfo(lines[0]).canonicalFilePath(), QFileInfo(_tmp.filePath("work")).canonicalFilePath());
        QCOMPARE(lines[1], QStringLiteral("notas"));
        // Una dirección va al navegador, con la palabra del cursor
        w->findChild<QAction*>(QStringLiteral("actionRunCommand1"))->trigger();
        QCOMPARE(_urls.urls.size(), 1);
        QCOMPARE(_urls.urls.front().toString(), QStringLiteral("https://en.wikipedia.org/wiki/Special:Search?search=línea"));
    }

    void savedCommandsAndTheirShortcuts()
    {
        FakePrompter* asks = nullptr;
        auto w = window(&asks);
        const std::size_t before = w->userCommands().size();
        asks->macroNames.enqueue({QStringLiteral("Listar"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L)});
        QVERIFY(w->saveUserCommand(QStringLiteral("ls -l")));
        QCOMPARE(w->userCommands().size(), before + 1);
        QAction* action = w->findChild<QAction*>(QStringLiteral("actionRunCommand%1").arg(before));
        QVERIFY(action);
        QCOMPARE(action->text(), QStringLiteral("Listar"));
        QCOMPARE(action->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
        // Otra ventana las lee de shortcuts.xml
        auto other = window();
        QCOMPARE(other->userCommands().size(), before + 1);
        QCOMPARE(other->userCommands().back().command, QStringLiteral("ls -l"));
        // La pestaña Órdenes de Ejecutar de la Asignación de atajos
        ShortcutMapperDialog dialog(*w, *asks, w.get());
        QCOMPARE(dialog.table(ShortcutEntry::Kind::RunCommands)->rowCount(), static_cast<int>(before + 1));
        QVERIFY(w->setShortcutKeys(ShortcutEntry::Kind::RunCommands, QString::number(before), {{true, true, false, 'R'}}));
        QCOMPARE(w->findChild<QAction*>(QStringLiteral("actionRunCommand%1").arg(before))->shortcut(),
                 QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R));
        QVERIFY(w->deleteUserCommand(static_cast<int>(before)));
        QCOMPARE(w->userCommands().size(), before);
        QVERIFY(!w->findChild<QAction*>(QStringLiteral("actionRunCommand%1").arg(before)));
        std::vector<UserCommand> stored;
        QVERIFY(readUserCommands(_tmp.filePath("config/edita/shortcuts.xml"), stored));
        QCOMPARE(stored.size(), before);
    }

    void runDialog()
    {
        auto w = window();
        w->show();
        const QString file = _tmp.filePath("work/notas.txt");
        w->openFile(file);
        w->findChild<QAction*>(QStringLiteral("actionRun"))->trigger();
        RunDialog* dialog = w->runDialog();
        QVERIFY(dialog && dialog->isVisible());
        dialog->setCommand(QStringLiteral("echo \"$(EXT_PART)\" > dialogo.txt"));
        dialog->findChild<QPushButton*>(QStringLiteral("runButton"))->click();
        QTRY_COMPARE(readFile(_tmp.filePath("work/dialogo.txt")), QStringLiteral(".txt"));
        QVERIFY(!dialog->isVisible());
        // Queda en el historial
        w->showRunDialog();
        QCOMPARE(dialog->combo()->itemText(0), QStringLiteral("echo \"$(EXT_PART)\" > dialogo.txt"));
    }
};

QTEST_MAIN(TestRun)
#include "tst_run.moc"
