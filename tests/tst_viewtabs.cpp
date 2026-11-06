// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 67: Ver y Pestañas como en N++: fijar pestañas, sus colores, ir a la pestaña N, moverlas,
// cerrar varias, siempre visible, pantalla completa, Post-It y desplazamiento sincronizado

#include "FakePrompter.h"
#include "core/Session.h"
#include "doc/Buffer.h"
#include "ui/DocStatusBar.h"
#include "ui/DocTabBar.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QMenuBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

using namespace edita;
using edita::test::FakePrompter;

class TestViewTabs : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        w->setPrompter(std::make_unique<FakePrompter>());
        w->resize(900, 600);
        w->show();
        QCoreApplication::processEvents();
        return w;
    }

    QString file(const QString& name, int lines = 3)
    {
        const QString path = _tmp.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return {};
        for (int i = 0; i < lines; ++i)
            f.write(QStringLiteral("%1 línea %2\n").arg(name).arg(i).toUtf8());
        return path;
    }

    // Los nombres de las pestañas de la vista principal, en orden
    static QStringList names(MainWindow& w)
    {
        QStringList out;
        for (const BufferId id : w.mainPane()->buffers())
            out << w.documents().buffer(id)->displayName();
        return out;
    }

    // Abre a, b, c, d (y quita el «nuevo 1» del arranque)
    void openFour(MainWindow& w)
    {
        const BufferId first = w.currentBuffer();
        for (const char* n : {"a.txt", "b.txt", "c.txt", "d.txt"})
            w.openFile(file(QString::fromLatin1(n)));
        w.closeBuffer(first);
    }

    static QAction* action(MainWindow& w, const char* name) { return w.findChild<QAction*>(QString::fromLatin1(name)); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    // Fijar: va con las fijadas, al principio; lleva un alfiler en vez del botón de cerrar
    void pinTabs()
    {
        auto w = window();
        openFour(*w);
        QCOMPARE(names(*w), (QStringList{"a.txt", "b.txt", "c.txt", "d.txt"}));
        DocTabBar* tabs = w->mainPane()->tabBar();
        w->goToTab(2); // c
        action(*w, "actionPinTab")->trigger();
        QCOMPARE(names(*w), (QStringList{"c.txt", "a.txt", "b.txt", "d.txt"}));
        QVERIFY(tabs->isPinnedAt(0));
        QVERIFY(action(*w, "actionPinTab")->isChecked());
        auto* pin = qobject_cast<QToolButton*>(tabs->tabButton(0, QTabBar::LeftSide));
        QVERIFY(pin && pin->objectName() == QStringLiteral("tabPin"));
        QVERIFY(!tabs->tabButton(0, QTabBar::RightSide)); // sin el botón de cerrar, ni su hueco
        // La segunda fijada va detrás de la primera
        w->goToTab(3); // d
        action(*w, "actionPinTab")->trigger();
        QCOMPARE(names(*w), (QStringList{"c.txt", "d.txt", "a.txt", "b.txt"}));
        // Mover sin cruzar la frontera: d no pasa de las fijadas, a no entra en ellas
        w->goToTab(1);
        w->moveCurrentTab(1);
        QCOMPARE(names(*w), (QStringList{"c.txt", "d.txt", "a.txt", "b.txt"}));
        w->moveCurrentTab(-1);
        QCOMPARE(names(*w), (QStringList{"d.txt", "c.txt", "a.txt", "b.txt"}));
        w->goToTab(2);
        action(*w, "actionTabToStart")->trigger();
        QCOMPARE(names(*w), (QStringList{"d.txt", "c.txt", "a.txt", "b.txt"}));
        action(*w, "actionTabToEnd")->trigger();
        QCOMPARE(names(*w), (QStringList{"d.txt", "c.txt", "b.txt", "a.txt"}));
        // Arrastrar una pestaña a la zona de las fijadas: al soltar, vuelve a su lado
        tabs->moveTab(3, 0);
        emit tabs->dragFinished();
        QTRY_COMPARE(names(*w), (QStringList{"d.txt", "c.txt", "a.txt", "b.txt"}));
        // El alfiler desfija: va al final, con su botón de cerrar
        pin = qobject_cast<QToolButton*>(tabs->tabButton(0, QTabBar::LeftSide));
        pin->click();
        QCOMPARE(names(*w), (QStringList{"c.txt", "a.txt", "b.txt", "d.txt"}));
        QVERIFY(!tabs->isPinnedAt(3));
        QTRY_VERIFY(!tabs->tabButton(3, QTabBar::LeftSide));
        QVERIFY(tabs->tabButton(3, QTabBar::RightSide)->isVisible());
        // Cerrar todo menos las fijadas
        action(*w, "actionCloseAllButPinned")->trigger();
        QCOMPARE(names(*w), QStringList{"c.txt"});
        // Sin botones de cerrar y otra vez con ellos: la fijada sigue con su alfiler
        Preferences p = w->preferences();
        p.tabCloseButton = false;
        w->setPreferences(p);
        p.tabCloseButton = true;
        w->setPreferences(p);
        QVERIFY(qobject_cast<QToolButton*>(tabs->tabButton(0, QTabBar::LeftSide)));
        QVERIFY(!tabs->tabButton(0, QTabBar::RightSide));
        // Y al desfijarla, su botón de cerrar
        qobject_cast<QToolButton*>(tabs->tabButton(0, QTabBar::LeftSide))->click();
        QTRY_VERIFY(tabs->tabButton(0, QTabBar::RightSide));
        QVERIFY(tabs->tabButton(0, QTabBar::RightSide)->isVisible());
    }

    // Colores e ir a la pestaña N; la sesión recuerda fijadas y colores (tabPinned, tabColourId)
    void colorsGoToAndSession()
    {
        auto w = window();
        openFour(*w);
        action(*w, "actionTab2")->trigger();
        QCOMPARE(w->mainPane()->tabBar()->currentIndex(), 1);
        QCOMPARE(action(*w, "actionTab2")->shortcut(),
                 QKeySequence(QKeyCombination(Qt::ControlModifier | Qt::KeypadModifier, Qt::Key_2)));
        action(*w, "actionLastTab")->trigger();
        QCOMPARE(w->mainPane()->tabBar()->currentIndex(), 3);
        action(*w, "actionFirstTab")->trigger();
        QCOMPARE(w->mainPane()->tabBar()->currentIndex(), 0);
        action(*w, "actionTab9")->trigger(); // no hay novena: no pasa nada
        QCOMPARE(w->mainPane()->tabBar()->currentIndex(), 0);

        action(*w, "actionTabColor3")->trigger();
        QCOMPARE(w->documents().buffer(w->currentBuffer())->tabColor(), 2);
        QCOMPARE(w->mainPane()->tabBar()->colorAt(0), 2);
        QVERIFY(DocTabBar::tabColor(2, false).isValid());
        QVERIFY(DocTabBar::tabColor(2, false) != DocTabBar::tabColor(2, true));
        w->goToTab(2);
        action(*w, "actionPinTab")->trigger(); // c, fijada
        const QString path = _tmp.filePath("tabs.session");
        QVERIFY(w->saveSessionFile(path, false));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray xml = f.readAll();
        QVERIFY(xml.contains("tabColourId=\"2\""));
        QVERIFY(xml.contains("tabPinned=\"yes\""));
        w->closeAll();
        QVERIFY(w->loadSessionFile(path));
        QCOMPARE(names(*w), (QStringList{"c.txt", "a.txt", "b.txt", "d.txt"}));
        QVERIFY(w->mainPane()->tabBar()->isPinnedAt(0));
        QCOMPARE(w->mainPane()->tabBar()->colorAt(1), 2);
        action(*w, "actionFirstTab")->trigger();
        w->goToTab(1);
        action(*w, "actionTabColorNone")->trigger();
        QCOMPARE(w->mainPane()->tabBar()->colorAt(1), -1);
    }

    // Cerrar todo lo que no tiene cambios
    void closeUnchanged()
    {
        auto w = window();
        openFour(*w);
        w->goToTab(1); // b, con cambios
        w->editor()->sends(SCI_INSERTTEXT, 0, "cambio ");
        QVERIFY(w->documents().buffer(w->currentBuffer())->isDirty());
        action(*w, "actionCloseAllUnchanged")->trigger();
        QCOMPARE(names(*w), QStringList{"b.txt"});
        QVERIFY(w->findChild<QMenu*>(QStringLiteral("menuCloseMultiple")));
    }

    // Siempre visible, pantalla completa y Post-It (que se quita con F12)
    void windowModes()
    {
        auto w = window();
        action(*w, "actionAlwaysOnTop")->trigger();
        QVERIFY(w->windowFlags() & Qt::WindowStaysOnTopHint);
        QVERIFY(action(*w, "actionAlwaysOnTop")->isChecked());
        action(*w, "actionAlwaysOnTop")->trigger();
        QVERIFY(!(w->windowFlags() & Qt::WindowStaysOnTopHint));

        QCOMPARE(action(*w, "actionFullScreen")->shortcut(), QKeySequence(Qt::Key_F11));
        action(*w, "actionFullScreen")->trigger();
        QTRY_VERIFY(w->windowState() & Qt::WindowFullScreen);
        action(*w, "actionFullScreen")->trigger();
        QTRY_VERIFY(!(w->windowState() & Qt::WindowFullScreen));

        QCOMPARE(action(*w, "actionPostIt")->shortcut(), QKeySequence(Qt::Key_F12));
        action(*w, "actionPostIt")->trigger();
        QVERIFY(w->isPostIt());
        QVERIFY(w->menuBar()->isHidden());
        QVERIFY(w->mainPane()->tabBar()->isHidden());
        QVERIFY(w->docStatusBar()->isHidden());
        QVERIFY(w->windowFlags() & Qt::FramelessWindowHint);
        // F12 sigue funcionando sin la barra de menús
        QVERIFY(w->actions().contains(action(*w, "actionPostIt")));
        action(*w, "actionPostIt")->trigger();
        QVERIFY(!w->isPostIt());
        QVERIFY(!w->menuBar()->isHidden());
        QVERIFY(!w->mainPane()->tabBar()->isHidden());
        QVERIFY(!w->docStatusBar()->isHidden());
        QVERIFY(!(w->windowFlags() & Qt::FramelessWindowHint));
    }

    // Las dos vistas se desplazan juntas, con la diferencia que tenían al activarlo
    void synchronizedScrolling()
    {
        auto w = window();
        const BufferId first = w->currentBuffer();
        w->openFile(file(QStringLiteral("largo1.txt"), 400));
        w->openFile(file(QStringLiteral("largo2.txt"), 400));
        w->closeBuffer(first);
        w->moveToOtherView(); // largo2 a la otra vista
        QTRY_VERIFY(w->subPane()->isVisible());
        EditorView* main = w->mainPane()->editor();
        EditorView* sub = w->subPane()->editor();
        main->sci(SCI_SETFIRSTVISIBLELINE, 0);
        sub->sci(SCI_SETFIRSTVISIBLELINE, 10);
        action(*w, "actionSyncScrollV")->trigger();
        main->sci(SCI_SETFIRSTVISIBLELINE, 100);
        QTRY_COMPARE(sub->sci(SCI_GETFIRSTVISIBLELINE), sptr_t{110});
        sub->sci(SCI_SETFIRSTVISIBLELINE, 50);
        QTRY_COMPARE(main->sci(SCI_GETFIRSTVISIBLELINE), sptr_t{40});
        // Desactivado: cada una por su lado
        action(*w, "actionSyncScrollV")->trigger();
        main->sci(SCI_SETFIRSTVISIBLELINE, 200);
        QTest::qWait(50);
        QCOMPARE(sub->sci(SCI_GETFIRSTVISIBLELINE), sptr_t{50});
    }
};

QTEST_MAIN(TestViewTabs)
#include "tst_viewtabs.moc"
