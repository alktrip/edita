// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Terminal integrada (no existe en N++).

#include "FakePrompter.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/Terminal.h"
#include "ui/TerminalPanel.h"

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

class TestTerminal : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    static QString canonical(const QString& path) { return QFileInfo(path).canonicalFilePath(); }
    // Una línea con lo que imprimió una orden: sola o tras el prompt («$ », «# »), sin ser
    // la orden misma («echo …»)
    static bool printed(const QString& text, const QString& word)
    {
        for (const QString& line : text.split(QLatin1Char('\n'))) {
            if (!line.contains(QLatin1String("echo")) && line.section(QLatin1Char(' '), -1) == word)
                return true;
        }
        return false;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void environment()
    {
        // Lo del AppImage no llega a la shell; TERM es el de la terminal
        const QStringList env = TerminalSession::environmentFor(
            {"PATH=/usr/bin", "APPDIR=/tmp/.mount_x", "APPIMAGE=/a/Edita.AppImage", "ARGV0=Edita", "TERM=dumb", "HOME=/home/x"});
        QVERIFY(env.contains("PATH=/usr/bin") && env.contains("HOME=/home/x"));
        QVERIFY(env.contains("TERM=xterm-256color") && env.contains("COLORTERM=truecolor"));
        QVERIFY(!env.contains("TERM=dumb"));
        for (const QString& e : env)
            QVERIFY2(!e.startsWith("APP") && !e.startsWith("ARGV0"), qPrintable(e));
    }

    void emulation()
    {
        // Sin proceso: lo que llega se interpreta como xterm
        TerminalView v;
        v.resize(640, 160);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        const int rows = v.rows();
        QVERIFY(v.columns() > 20 && rows >= 3);
        v.feed("hola\r\n\x1b[31mrojo\x1b[0m \x1b[1;32mX\x1b[0m \x1b[38;2;10;20;30mT\x1b[0m \xe6\x97\xa5\xe6\x9c\xac fin");
        QCOMPARE(v.lineText(0), QStringLiteral("hola"));
        QCOMPARE(v.lineText(1), QStringLiteral("rojo X T 日本 fin"));
        const TerminalView::Colors colors = TerminalView::defaultColors(Qt::black, Qt::white);
        QCOMPARE(v.cellForeground(v.cellAt(1, 0)), colors.ansi[1]);
        QVERIFY(v.cellAt(1, 5).attrs.bold);
        QCOMPARE(v.cellForeground(v.cellAt(1, 5)), colors.ansi[10]); // negrita: el verde brillante
        QCOMPARE(v.cellForeground(v.cellAt(1, 7)), QColor(10, 20, 30)); // color real (24 bits)
        QCOMPARE(int(v.cellAt(1, 9).width), 2);                         // 日: dos columnas
        QCOMPARE(v.cellForeground(v.cellAt(1, 14)), v.cellForeground(v.cellAt(0, 0))); // el del tema

        // El título (OSC 0)
        QSignalSpy title(&v, &TerminalView::titleChanged);
        v.feed("\x1b]0;Mi título\x07");
        QCOMPARE(v.title(), QStringLiteral("Mi título"));
        QCOMPARE(title.count(), 1);

        // Historial: lo que sale por arriba se guarda
        v.feed("\x1b[2J\x1b[H");
        for (int i = 0; i < 50; ++i)
            v.feed(QByteArray("linea ") + QByteArray::number(i) + "\r\n");
        QVERIFY(v.scrollbackLines() >= 50 - rows);
        QVERIFY(v.text().contains("linea 0\nlinea 1\n"));
        QCOMPARE(v.lineText(-1), QStringLiteral("linea %1").arg(50 - rows));

        // Pantalla alterna (vim, less…): al salir vuelve lo de antes
        v.feed("\x1b[?1049h\x1b[2J\x1b[Hpantalla completa");
        QVERIFY(v.altScreen());
        QCOMPARE(v.lineText(0), QStringLiteral("pantalla completa"));
        v.feed("\x1b[?1049l");
        QVERIFY(!v.altScreen());
        QCOMPARE(v.lineText(rows - 2), QStringLiteral("linea 49"));
        // Ratón para los programas que lo piden
        v.feed("\x1b[?1000h");
        QVERIFY(v.mouseReporting());
        v.feed("\x1b[?1000l");
        QVERIFY(!v.mouseReporting());

        // Seleccionar todo y copiar
        v.selectAll();
        QVERIFY(v.selectedText().startsWith(QStringLiteral("linea 0\nlinea 1\n"))); // «hola» se borró con ESC [2J
        QVERIFY(v.selectedText().contains(QStringLiteral("linea 49")));
    }

    void shortcutsStayInTheTerminal()
    {
        TerminalView v;
        auto overridden = [&](int key, Qt::KeyboardModifiers mods) {
            QKeyEvent e(QEvent::ShortcutOverride, key, mods);
            e.ignore();
            QCoreApplication::sendEvent(&v, &e);
            return e.isAccepted();
        };
#ifdef Q_OS_MACOS
        // Qt llama Meta a la tecla Control y Control a ⌘
        QVERIFY(overridden(Qt::Key_F, Qt::MetaModifier));   // Control+F: de la shell
        QVERIFY(overridden(Qt::Key_C, Qt::MetaModifier));   // Control+C: interrumpir
        QVERIFY(overridden(Qt::Key_F3, Qt::NoModifier));
        QVERIFY(!overridden(Qt::Key_F, Qt::ControlModifier)); // ⌘F: Buscar
        QVERIFY(!overridden(Qt::Key_QuoteLeft, Qt::ControlModifier));
        QVERIFY(overridden(Qt::Key_C, Qt::ControlModifier));  // ⌘C: copiar
#else
        QVERIFY(overridden(Qt::Key_F, Qt::ControlModifier));   // Ctrl+F: de la shell, no Buscar
        QVERIFY(overridden(Qt::Key_W, Qt::ControlModifier));
        QVERIFY(overridden(Qt::Key_F3, Qt::NoModifier));
        QVERIFY(!overridden(Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier)); // la paleta de órdenes
        QVERIFY(!overridden(Qt::Key_QuoteLeft, Qt::ControlModifier));             // mostrar/ocultar
        QVERIFY(!overridden(Qt::Key_Ntilde, Qt::ControlModifier));
        QVERIFY(overridden(Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier));  // copiar
#endif
    }

    void realShell()
    {
        const QString dir = _tmp.filePath("carpeta con espacios");
        QVERIFY(QDir().mkpath(dir));
        qputenv("APPDIR", "/tmp/.mount_edita");
        TerminalView v;
        v.resize(700, 300);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        QVERIFY(v.start("/bin/sh", {}, dir));
        qunsetenv("APPDIR");
        TerminalSession* session = v.session();
        QVERIFY(session->isRunning());
        QTRY_COMPARE(canonical(session->currentDirectory()), canonical(dir)); // (el hijo hace chdir al arrancar)
        // Lo que se escribe llega a la shell y su salida a la pantalla
        v.sendText("echo hola$((40+2)) [$APPDIR] [$TERM]; pwd\n");
        QTRY_VERIFY2(v.text().contains("hola42 [] [xterm-256color]"), qPrintable(v.text()));
        QTRY_VERIFY(v.text().contains(QDir(dir).absolutePath()));
        // El tamaño de la vista es el de la terminal
        v.sendText("stty size\n");
        QTRY_VERIFY2(v.text().contains(QStringLiteral("%1 %2").arg(v.rows()).arg(v.columns())), qPrintable(v.text()));
        // Ctrl+C interrumpe
        v.sendText("sleep 30; echo nunca\n");
        QTest::qWait(200);
#ifdef Q_OS_MACOS
        QTest::keyClick(&v, Qt::Key_C, Qt::MetaModifier); // la tecla Control
#else
        QTest::keyClick(&v, Qt::Key_C, Qt::ControlModifier);
#endif
        v.sendText("echo despues\n");
        QTRY_VERIFY2(printed(v.text(), "despues"), qPrintable(v.text()));
        QVERIFY(!printed(v.text(), "nunca"));
        // Pegar: con «bracketed paste» (bash 5) lo pegado espera a Intro; dash y el bash 3.2
        // de macOS no lo piden y se ejecuta al momento. En los dos casos, con Intro, sale
        v.paste("echo pegado1\necho pegado2\n");
        QTRY_VERIFY(v.text().contains("echo pegado2"));
        v.sendText("\n");
        QTRY_VERIFY2(printed(v.text(), "pegado2"), qPrintable(v.text()));
        // Al salir avisa con el código
        QSignalSpy finished(&v, &TerminalView::finished);
        v.sendText("exit 3\n");
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(finished.first().first().toInt(), 3);
    }

    void fromTheMainWindow()
    {
        const QString dir = _tmp.filePath("proyecto");
        QVERIFY(QDir().mkpath(dir));
        QFile f(dir + "/run.sh");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("echo desde_edita_$((1+1))\n");
        f.close();
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        Preferences p = w.preferences();
        p.terminalShell = "/bin/sh";
        w.setPreferences(p);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.openFile(dir + "/run.sh") != kInvalidBuffer);
        QDockWidget* dock = w.findChild<QDockWidget*>("terminalDock");
        QVERIFY(dock && !dock->isVisibleTo(&w));
        // Ver › Terminal: una terminal en la carpeta del documento
        w.findChild<QAction*>("actionTerminal")->trigger();
        QVERIFY(dock->isVisibleTo(&w));
        TerminalPanel* panel = w.terminalPanel();
        QCOMPARE(panel->count(), 1);
        TerminalView* view = panel->currentTerminal();
        QTRY_COMPARE(canonical(view->session()->currentDirectory()), canonical(dir));
        // Con la fuente y los colores del editor
        const auto back = w.editor()->sci(SCI_STYLEGETBACK, STYLE_DEFAULT);
        QCOMPARE(view->cellBackground(view->cellAt(0, 0)),
                 QColor(static_cast<int>(back & 0xFF), static_cast<int>((back >> 8) & 0xFF), static_cast<int>((back >> 16) & 0xFF)));
        // Ejecutar la selección (sin selección, la línea del cursor)
        w.editor()->sci(SCI_GOTOPOS, 0);
        w.findChild<QAction*>("actionRunSelectionInTerminal")->trigger();
        QTRY_VERIFY2(printed(view->text(), "desde_edita_2"), qPrintable(view->text()));
        // Otra terminal, en otra pestaña
        w.findChild<QAction*>("actionNewTerminal")->trigger();
        QCOMPARE(panel->count(), 2);
        // Salir de las shells cierra sus pestañas y, con la última, el panel
        panel->terminalAt(1)->sendText("exit\n");
        QTRY_COMPARE(panel->count(), 1);
        panel->terminalAt(0)->sendText("exit\n");
        QTRY_COMPARE(panel->count(), 0);
        QTRY_VERIFY(!dock->isVisibleTo(&w));
    }
};

QTEST_MAIN(TestTerminal)
#include "tst_terminal.moc"
