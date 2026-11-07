// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 71: Configuración › Importar (plugins y temas), el menú contextual del editor desde
// contextMenu.xml y Macro › Modificar atajo/Borrar macro

#include "FakePrompter.h"
#include "core/ContextMenuXml.h"
#include "core/Paths.h"
#include "core/PluginInstaller.h"
#include "doc/Buffer.h"
#include "syntax/SyntaxManager.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace edita;
using edita::test::FakePrompter;

class TestImportMenu : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    FakePrompter* _prompter = nullptr;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        auto prompter = std::make_unique<FakePrompter>();
        _prompter = prompter.get();
        w->setPrompter(std::move(prompter));
        QCoreApplication::processEvents();
        return w;
    }

    QString file(const QString& name, const QByteArray& data)
    {
        const QString path = _tmp.filePath(name);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return {};
        f.write(data);
        return path;
    }

    // Lo que tiene un menú, por objectName de la acción («-» separador, «>Título» submenú)
    static QStringList contents(const QMenu* menu)
    {
        QStringList out;
        for (QAction* a : menu->actions()) {
            if (a->isSeparator())
                out << QStringLiteral("-");
            else if (a->menu())
                out << QStringLiteral(">") + a->text();
            else
                out << (a->objectName().isEmpty() ? a->text() : a->objectName());
        }
        return out;
    }

    void writeContextMenu(const QByteArray& items)
    {
        QFile f(QDir(paths::configDir()).filePath(QStringLiteral("contextMenu.xml")));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<NotepadPlus><ScintillaContextMenu>\n" + items
                + "\n</ScintillaContextMenu></NotepadPlus>\n");
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        QDir().mkpath(paths::configDir());
    }

    // Los nombres en inglés de N++ (english.xml), sin distinguir mayúsculas, primero en su menú
    void commandNames()
    {
        QCOMPARE(nppCommandIdFromName(QStringLiteral("Edit"), QStringLiteral("Cut")), 42001);
        QCOMPARE(nppCommandIdFromName(QStringLiteral("Edit"), QStringLiteral("Select all")), 42007);
        QCOMPARE(nppCommandIdFromName(QStringLiteral("View"), QStringLiteral("Hide lines")), 44042);
        QCOMPARE(nppCommandIdFromName(QStringLiteral("&Edit"), QStringLiteral("Cu&t")), 42001);
        QCOMPARE(nppCommandIdFromName(QStringLiteral("Search"), QStringLiteral("Find...")), 43001);
        QCOMPARE(nppCommandIdFromName(QStringLiteral("Edit"), QStringLiteral("No existe")), -1);
        QCOMPARE(contextMenuText(QStringLiteral("Save &As…\tCtrl+Alt+S")), QStringLiteral("Save As"));
        QCOMPARE(contextMenuText(QStringLiteral("Find && Replace...")), QStringLiteral("Find & Replace"));
    }

    // El contextMenu.xml de serie (el de N++): lo que Edita tiene, en su orden y con submenús
    void defaultContextMenu()
    {
        QFile::remove(QDir(paths::configDir()).filePath(QStringLiteral("contextMenu.xml")));
        auto w = window();
        const QString path = w->contextMenuPath();
        QVERIFY(QFile::exists(path)); // se crea con el de serie
        bool ok = false;
        QVERIFY(!readContextMenu(path, &ok).empty());
        QVERIFY(ok);
        const std::unique_ptr<QMenu> menu(w->buildEditorContextMenu());
        const QStringList items = contents(menu.get());
        QCOMPARE(items.mid(0, 6), (QStringList{"actionCut", "actionCopy", "actionPaste", "actionDelete", "actionSelectAll", "-"}));
        QVERIFY(items.contains(QStringLiteral(">Style all occurrences of token")));
        QVERIFY(items.contains(QStringLiteral("actionUpperCase")));
        QVERIFY(items.contains(QStringLiteral("actionOpenSelectedFile")));
        QVERIFY(items.contains(QStringLiteral("actionSearchOnInternet")));
        QCOMPARE(items.back(), QStringLiteral("actionHideLines"));
        QVERIFY(!items.contains(QStringLiteral(">Plugin commands"))); // sin MIME Tools ni NppExport
        QVERIFY(!items.join(QLatin1Char(',')).contains(QStringLiteral("-,-"))); // sin separadores seguidos
        for (QAction* a : menu->actions()) {
            if (a->menu() && a->text() == QLatin1String("Style all occurrences of token"))
                QCOMPARE(contents(a->menu()), (QStringList{"actionStyleAll1", "actionStyleAll2", "actionStyleAll3",
                                                           "actionStyleAll4", "actionStyleAll5"}));
        }
    }

    // Uno propio: ids, nombres de N++ y de Edita, ItemNameAs, carpetas, lo que no existe
    void customContextMenu()
    {
        auto w = window();
        writeContextMenu("<Item id=\"0\"/>\n"
            "<Item id=\"42003\"/>\n"
            "<Item id=\"99999\"/>\n"
            "<Item MenuEntryName=\"Edit\" MenuItemName=\"Redo\" ItemNameAs=\"Otra vez\"/>\n"
            "<Item id=\"0\"/>\n"
            "<Item FolderName=\"Mío\" MenuEntryName=\"Search\" MenuItemName=\"Find...\"/>\n"
            "<Item FolderName=\"Mío\" PluginEntryName=\"No instalado\" PluginCommandItemName=\"Algo\"/>\n"
            "<Item MenuEntryName=\"Edit\" MenuItemName=\"Select all\"/>\n");
        const std::unique_ptr<QMenu> menu(w->buildEditorContextMenu());
        QCOMPARE(contents(menu.get()), (QStringList{"actionUndo", "Otra vez", "-", ">Mío", "actionSelectAll"}));
        // «Otra vez» hace lo que hace Rehacer
        w->editor()->sends(SCI_SETTEXT, 0, "a");
        w->editor()->sci(SCI_UNDO);
        menu->actions().at(1)->trigger();
        QCOMPARE(QString::fromStdString(w->editor()->text()), QStringLiteral("a"));
        // Vacío o roto: el menú de siempre de Scintilla
        writeContextMenu("");
        const std::unique_ptr<QMenu> fallback(w->buildEditorContextMenu());
        QCOMPARE(contents(fallback.get()), (QStringList{"actionUndo", "actionRedo", "-", "actionCut", "actionCopy", "actionPaste",
                                                        "actionDelete", "-", "actionSelectAll"}));
    }

    // El clic derecho en el texto abre ese menú (y no el de Scintilla)
    void rightClickOpensIt()
    {
        auto w = window();
        writeContextMenu("<Item id=\"42001\"/>\n");
        w->resize(900, 600);
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.get()));
        EditorView* view = w->editor();
        view->sends(SCI_SETTEXT, 0, "hola mundo");
        QCoreApplication::processEvents();
        sptr_t margins = 0;
        for (sptr_t i = 0; i < view->sci(SCI_GETMARGINS); ++i)
            margins += view->sci(SCI_GETMARGINWIDTHN, static_cast<uptr_t>(i));
        QStringList seen;
        QTimer::singleShot(200, [&seen] {
            if (auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
                seen = contents(popup);
                popup->close();
            }
        });
        const QPoint at(static_cast<int>(margins) + 20, 4);
        QContextMenuEvent event(QContextMenuEvent::Mouse, at, view->viewport()->mapToGlobal(at));
        QApplication::sendEvent(view->viewport(), &event);
        QCOMPARE(seen, QStringList{QStringLiteral("actionCut")});
    }

    // Configuración › Importar › Plugins: un .so a plugins/<nombre>/, un paquete .zip, y lo que no vale
    void importPlugins()
    {
        auto w = window();
        const QString so = file(QStringLiteral("descargas/MiPlugin.so"), QByteArray("\x7f" "ELF") + QByteArray(64, '\0'));
        const QString fake = file(QStringLiteral("descargas/Falso.so"), "no es un ELF");
        // Un paquete como los del catálogo
        const QString inner = file(QStringLiteral("paquete/OtroPlugin.so"), QByteArray("\x7f" "ELF") + QByteArray(8, '\1'));
        const QString zip = _tmp.filePath(QStringLiteral("descargas/OtroPlugin-1.0.zip"));
        QString error;
        QVERIFY2(plugin_installer::createZip(zip, {{QStringLiteral("OtroPlugin.so"), inner}}, &error), qPrintable(error));
        QCOMPARE(w->importPlugins({so, fake, zip}), 2);
        const QString root = QDir(paths::baseConfigDir()).filePath(QStringLiteral("plugins"));
        QVERIFY(QFile::exists(root + QStringLiteral("/MiPlugin/MiPlugin.so")));
        QVERIFY(QFile::exists(root + QStringLiteral("/OtroPlugin/OtroPlugin.so")));
        QVERIFY(!QFile::exists(root + QStringLiteral("/Falso")));
        QCOMPARE(_prompter->warnings.size(), 1);
        QVERIFY(_prompter->warnings.front().contains(QStringLiteral("Falso.so")));
        QCOMPARE(_prompter->informations.size(), 1); // hay que reiniciar
        // Otra vez: se sustituye
        QCOMPARE(w->importPlugins({so}), 1);
    }

    // Configuración › Importar › Temas: a themes/ y ya en la lista
    void importThemes()
    {
        auto w = window();
        const QString theme = file(QStringLiteral("descargas/Mi tema.xml"),
                                   "<?xml version=\"1.0\"?><NotepadPlus><LexerStyles/><GlobalStyles/></NotepadPlus>");
        const QString notTheme = file(QStringLiteral("descargas/otro.xml"), "<?xml version=\"1.0\"?><Algo/>");
        QCOMPARE(w->importThemes({theme, notTheme}), 1);
        QVERIFY(QFile::exists(QDir(paths::configDir()).filePath(QStringLiteral("themes/Mi tema.xml"))));
        bool listed = false;
        for (const auto& t : w->syntax().themes())
            listed = listed || t.name == QLatin1String("Mi tema");
        QVERIFY(listed);
        QCOMPARE(_prompter->warnings.size(), 1);
        // Ya existe: pregunta
        _prompter->confirmations.enqueue(false);
        QCOMPARE(w->importThemes({theme}), 0);
        _prompter->confirmations.enqueue(true);
        QCOMPARE(w->importThemes({theme}), 1);
    }

    // Las órdenes de los menús
    void menuActions()
    {
        auto w = window();
        auto* import = w->findChild<QMenu*>(QStringLiteral("menuImport"));
        QVERIFY(import);
        QCOMPARE(contents(import), (QStringList{"actionImportPlugins", "actionImportThemes"}));
        QVERIFY(w->findChild<QAction*>(QStringLiteral("actionEditMacros")));
        // Editar el menú contextual: avisa y abre contextMenu.xml
        w->findChild<QAction*>(QStringLiteral("actionEditContextMenu"))->trigger();
        QCOMPARE(_prompter->informations.size(), 1);
        const Buffer* buf = w->documents().buffer(w->currentBuffer());
        QVERIFY(buf);
        QCOMPARE(buf->filePath(), w->contextMenuPath());
    }
};

QTEST_MAIN(TestImportMenu)
#include "tst_importmenu.moc"
