// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 75: abrir o mover el documento en otra ventana de Edita (Open/Move in New Instance),
// Editar › Pegado especial (el HTML y el RTF del portapapeles, y el contenido binario) y el
// menú de las pestañas desde tabContextMenu.xml

#include "FakePrompter.h"
#include "core/CommandLine.h"
#include "core/ContextMenuXml.h"
#include "core/Paths.h"
#include "doc/Buffer.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/SingleInstance.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

class TestTransfer : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    QList<QStringList> _launched;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        w->setPrompter(std::make_unique<FakePrompter>());
        _launched.clear();
        w->setInstanceLauncher([this](const QStringList& arguments) {
            _launched << arguments;
            return true;
        });
        QCoreApplication::processEvents();
        return w;
    }

    QString file(const QString& name, const QByteArray& data)
    {
        const QString path = _tmp.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return {};
        f.write(data);
        return path;
    }

    static QAction* action(MainWindow& w, const char* name) { return w.findChild<QAction*>(QString::fromLatin1(name)); }
    static QByteArray bytes(MainWindow& w)
    {
        EditorView* v = w.editor();
        const auto length = static_cast<qsizetype>(v->sci(SCI_GETLENGTH));
        QByteArray out(length + 1, '\0');
        v->sci(SCI_GETTEXT, static_cast<uptr_t>(length + 1), reinterpret_cast<sptr_t>(out.data()));
        out.truncate(length);
        return out;
    }
    static void setClipboard(const QString& mime, const QByteArray& data)
    {
        auto* mimeData = new QMimeData;
        mimeData->setData(mime, data);
        QApplication::clipboard()->setMimeData(mimeData);
    }
    static QStringList titles(const QMenu* menu)
    {
        QStringList out;
        for (QAction* a : menu->actions())
            out << (a->isSeparator() ? QStringLiteral("-") : a->menu() ? QStringLiteral(">") + a->text() : a->text());
        return out;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    // La línea de órdenes de la otra ventana: el archivo, sin sesión, otra instancia, el
    // lenguaje (-l, como N++) y el cursor
    void newInstanceArguments()
    {
        QCOMPARE(edita::newInstanceArguments(QStringLiteral("/tmp/a.c"), QStringLiteral("cpp"), 3, 5),
                 (QStringList{QStringLiteral("/tmp/a.c"), QStringLiteral("--multiInst"), QStringLiteral("--nosession"),
                              QStringLiteral("-l"), QStringLiteral("cpp"), QStringLiteral("-n"), QStringLiteral("3"),
                              QStringLiteral("-c"), QStringLiteral("5")}));
        QCOMPARE(edita::newInstanceArguments(QStringLiteral("/tmp/a"), QString(), 1, 1).size(), 7); // sin -l

        // -l en la línea de órdenes y en lo que se pasa a la instancia abierta
        QCommandLineParser parser;
        const CommandLineOptions options = setupCommandLine(parser);
        QVERIFY(parser.parse({QStringLiteral("edita"), QStringLiteral("-l"), QStringLiteral("python"), QStringLiteral("x.txt")}));
        QCOMPARE(parser.value(options.language), QStringLiteral("python"));
        QVERIFY(commandLineHelp().contains(QLatin1String("--language")));
        InstanceRequest request;
        request.files << QStringLiteral("/tmp/x.txt");
        request.language = QStringLiteral("python");
        InstanceRequest back;
        QVERIFY(InstanceRequest::fromJson(request.toJson(), back));
        QCOMPARE(back.language, QStringLiteral("python"));
    }

    // La ventana que recibe el lenguaje se lo pone al archivo
    void languageFromRequest()
    {
        auto w = window();
        InstanceRequest request;
        request.files << file(QStringLiteral("notes.txt"), "print(1)\n");
        request.language = QStringLiteral("python");
        w->openFromOtherInstance(request, false);
        QCOMPARE(w->documents().buffer(w->currentBuffer())->language(), LangType::Python);
        request.files = QStringList{file(QStringLiteral("other.txt"), "x\n")};
        request.language = QStringLiteral("no-such-language"); // se pasa por alto
        w->openFromOtherInstance(request, false);
        QCOMPARE(w->documents().buffer(w->currentBuffer())->language(), LangType::Text);
    }

    // Ver › Abrir en otra ventana: lanza Edita con el archivo; el documento sigue aquí
    void openInNewInstance()
    {
        auto w = window();
        const QString path = file(QStringLiteral("a.cpp"), "int a;\nint b;\n");
        const BufferId id = w->openFile(path);
        w->editor()->sci(SCI_GOTOPOS, 9); // línea 2, columna 3
        QAction* open = action(*w, "actionOpenInNewInstance");
        QAction* move = action(*w, "actionMoveToNewInstance");
        QVERIFY(open->isEnabled() && move->isEnabled());
        open->trigger();
        QCOMPARE(_launched.size(), 1);
        QCOMPARE(_launched.front(), edita::newInstanceArguments(path, QStringLiteral("cpp"), 2, 3));
        QVERIFY(w->documents().buffer(id));

        // Mover: el documento se cierra aquí
        w->newDocument();
        const BufferId keep = w->currentBuffer();
        w->openFile(path);
        move->trigger();
        QCOMPARE(_launched.size(), 2);
        QVERIFY(!w->documents().buffer(id));
        QCOMPARE(w->currentBuffer(), keep);
    }

    // Sin guardar o sin título no se puede (la otra ventana leería el archivo del disco)
    void notForUnsavedDocuments()
    {
        auto w = window();
        QAction* open = action(*w, "actionOpenInNewInstance");
        QAction* move = action(*w, "actionMoveToNewInstance");
        QVERIFY(!open->isEnabled() && !move->isEnabled()); // «nuevo 1»
        w->openFile(file(QStringLiteral("b.txt"), "b\n"));
        QVERIFY(open->isEnabled());
        w->editor()->sends(SCI_INSERTTEXT, 0, "x");
        QVERIFY(!open->isEnabled() && !move->isEnabled());
        QVERIFY(!w->openInNewInstance(false));
        QVERIFY(_launched.isEmpty());
    }

    // Mover el único documento cierra la ventana (docOpenInNewInstance: noOpenedDoc)
    void movingTheLastDocumentClosesTheWindow()
    {
        auto w = window();
        w->openFile(file(QStringLiteral("only.txt"), "only\n"));
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.get()));
        action(*w, "actionMoveToNewInstance")->trigger();
        QCOMPARE(_launched.size(), 1);
        QTRY_VERIFY(!w->isVisible());
    }

    // Editar › Pegado especial › Pegar contenido HTML: el código, no el texto que se ve
    void pasteHtml()
    {
        auto w = window();
        auto* mime = new QMimeData;
        mime->setHtml(QStringLiteral("<b>año</b>"));
        mime->setText(QStringLiteral("año"));
        QApplication::clipboard()->setMimeData(mime);
        QVERIFY(action(*w, "actionPasteHtml")->isEnabled());
        action(*w, "actionPasteHtml")->trigger();
        QCOMPARE(bytes(*w), QByteArray("<b>a\xC3\xB1o</b>"));
        // El de Firefox viene en UTF-16 con BOM
        QByteArray utf16("\xFF\xFE", 2);
        for (const char c : QByteArray("<i>x</i>"))
            utf16 += QByteArray(1, c) + QByteArray(1, '\0');
        setClipboard(QStringLiteral("text/html"), utf16);
        w->editor()->sends(SCI_SETTEXT, 0, "");
        action(*w, "actionPasteHtml")->trigger();
        QCOMPARE(bytes(*w), QByteArray("<i>x</i>"));
    }

    // Pegar contenido RTF: el código RTF (text/rtf o application/rtf)
    void pasteRtf()
    {
        auto w = window();
        setClipboard(QStringLiteral("text/rtf"), "{\\rtf1 hola}");
        action(*w, "actionPasteRtf")->trigger();
        QCOMPARE(bytes(*w), QByteArray("{\\rtf1 hola}"));
        setClipboard(QStringLiteral("application/rtf"), "{\\rtf1 b}");
        w->editor()->sends(SCI_SETTEXT, 0, "");
        action(*w, "actionPasteRtf")->trigger();
        QCOMPARE(bytes(*w), QByteArray("{\\rtf1 b}"));
        // Sin RTF en el portapapeles, nada
        QApplication::clipboard()->setText(QStringLiteral("plain"));
        w->editor()->sends(SCI_SETTEXT, 0, "");
        action(*w, "actionPasteRtf")->trigger();
        QCOMPARE(bytes(*w), QByteArray());
    }

    // Copiar, cortar y pegar contenido binario: con los NUL
    void binaryContent()
    {
        auto w = window();
        EditorView* v = w->editor();
        const QByteArray data("a\0b\0c", 5);
        v->sci(SCI_ADDTEXT, static_cast<uptr_t>(data.size()), reinterpret_cast<sptr_t>(data.constData()));
        v->sci(SCI_SETSEL, 0, 5);
        action(*w, "actionCopyBinary")->trigger();
        QCOMPARE(bytes(*w), data);
        v->sci(SCI_SETSEL, 5, 5);
        action(*w, "actionPasteBinary")->trigger();
        QCOMPARE(bytes(*w), data + data);
        // Cortar: se quita
        v->sci(SCI_SETSEL, 0, 3);
        action(*w, "actionCutBinary")->trigger();
        QCOMPARE(bytes(*w), QByteArray("\0c", 2) + data);
        v->sci(SCI_SETSEL, 0, 0);
        action(*w, "actionPasteBinary")->trigger();
        QCOMPARE(bytes(*w), data + data);
        // Lo que copió otro programa: el texto sin más
        QApplication::clipboard()->setText(QStringLiteral("txt"));
        v->sends(SCI_SETTEXT, 0, "");
        action(*w, "actionPasteBinary")->trigger();
        QCOMPARE(bytes(*w), QByteArray("txt"));
    }

    // tabContextMenu.xml: <TabContextMenu> con las mismas reglas que contextMenu.xml
    void readTabContextMenu()
    {
        const QString path = file(QStringLiteral("tab.xml"), "<NotepadPlus><TabContextMenu>"
                                                            "<Item MenuEntryName=\"File\" MenuItemName=\"Close\"/>"
                                                            "<Item id=\"0\"/>"
                                                            "</TabContextMenu></NotepadPlus>");
        bool ok = false;
        QCOMPARE(readContextMenu(path, &ok, ContextMenuKind::Tab).size(), std::size_t(2));
        QVERIFY(ok);
        QVERIFY(readContextMenu(path, &ok).empty()); // no es un <ScintillaContextMenu>
        QVERIFY(!ok);
    }

    // Sin tabContextMenu.xml, el menú de siempre (con las dos órdenes nuevas)
    void defaultTabMenu()
    {
        QFile::remove(QDir(paths::configDir()).filePath(QStringLiteral("tabContextMenu.xml")));
        auto w = window();
        w->openFile(file(QStringLiteral("t.txt"), "t\n"));
        const std::unique_ptr<QMenu> menu(w->buildTabContextMenu(w->currentBuffer()));
        const QStringList items = titles(menu.get());
        QVERIFY2(items.contains(QStringLiteral("Close")), qPrintable(items.join(u'|')));
        QVERIFY(items.contains(QStringLiteral("Move to New Instance")));
        QVERIFY(items.contains(QStringLiteral("Open in New Instance")));
    }

    // Con tabContextMenu.xml: sus órdenes, submenús y separadores; lo que no existe se salta
    void customTabMenu()
    {
        paths::ensureConfigDir();
        QFile f(QDir(paths::configDir()).filePath(QStringLiteral("tabContextMenu.xml")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("<NotepadPlus><TabContextMenu>"
                "<Item MenuEntryName=\"File\" MenuItemName=\"Close\"/>"
                "<Item id=\"44048\"/>"
                "<Item id=\"0\"/>"
                "<Item FolderName=\"Move Document\" MenuEntryName=\"View\" MenuItemName=\"Move to New Instance\"/>"
                "<Item FolderName=\"Move Document\" MenuEntryName=\"View\" MenuItemName=\"Open in New Instance\" ItemNameAs=\"Other window\"/>"
                "<Item MenuEntryName=\"File\" MenuItemName=\"PowerShell\"/>"
                "</TabContextMenu></NotepadPlus>");
        f.close();
        auto w = window();
        w->openFile(file(QStringLiteral("u.txt"), "u\n"));
        const std::unique_ptr<QMenu> menu(w->buildTabContextMenu(w->currentBuffer()));
        const QList<QAction*> actions = menu->actions();
        QCOMPARE(actions.size(), 4); // Cerrar, Fijar, -, >Move Document
        QCOMPARE(actions.at(0), action(*w, "actionClose"));
        QCOMPARE(actions.at(1), action(*w, "actionPinTab"));
        QVERIFY(actions.at(2)->isSeparator());
        QMenu* folder = actions.at(3)->menu();
        QVERIFY(folder);
        QCOMPARE(folder->actions().size(), 2);
        QCOMPARE(folder->actions().at(0), action(*w, "actionMoveToNewInstance"));
        QCOMPARE(folder->actions().at(1)->text(), QStringLiteral("Other window"));
        folder->actions().at(1)->trigger();
        QCOMPARE(_launched.size(), 1);

        // Configuración › Editar el menú de las pestañas: abre el archivo
        action(*w, "actionEditTabContextMenu")->trigger();
        QCOMPARE(w->documents().buffer(w->currentBuffer())->filePath(), w->tabContextMenuPath());
        QFile::remove(w->tabContextMenuPath());
    }

    // La primera vez, Editar el menú de las pestañas crea el de N++ (el de serie)
    void editTabMenuCreatesDefault()
    {
        QFile::remove(QDir(paths::configDir()).filePath(QStringLiteral("tabContextMenu.xml")));
        auto w = window();
        action(*w, "actionEditTabContextMenu")->trigger();
        bool ok = false;
        QVERIFY(!readContextMenu(w->tabContextMenuPath(), &ok, ContextMenuKind::Tab).empty());
        QVERIFY(ok);
        const std::unique_ptr<QMenu> menu(w->buildTabContextMenu(w->currentBuffer()));
        QVERIFY(menu->actions().contains(action(*w, "actionClose")));
        QFile::remove(w->tabContextMenuPath());
    }
};

QTEST_MAIN(TestTransfer)
#include "tst_transfer.moc"
