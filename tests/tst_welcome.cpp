// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Página de inicio (no existe en N++).

#include "FakePrompter.h"
#include "ui/CommandPalette.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/WelcomePage.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

using namespace edita;
using edita::test::FakePrompter;
using namespace std::string_literals;

namespace {

void writeFile(const QString& path, const QByteArray& data = "texto\n")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

} // namespace

class TestWelcome : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        w->setPrompter(std::make_unique<FakePrompter>());
        QCoreApplication::processEvents();
        return w;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void shownWhileTheOnlyDocumentIsNewAndEmpty()
    {
        auto w = window();
        QVERIFY(w->welcomePage());
        QVERIFY(w->isWelcomeVisible());
        // Escribir empieza a escribir en el documento nuevo
        QTest::keyClicks(w->welcomePage(), QStringLiteral("H"));
        QVERIFY(!w->isWelcomeVisible());
        QTest::keyClicks(w->editor(), QStringLiteral("ola"));
        QCOMPARE(w->documents().text(w->currentBuffer()), "Hola"s);
        // Al cerrar todo, N++ deja un «new» vacío: vuelve la página
        w->editor()->sends(SCI_SETTEXT, 0, "");
        w->editor()->sci(SCI_SETSAVEPOINT);
        QAction* closeAll = w->findChild<QAction*>(QStringLiteral("actionCloseAll"));
        QVERIFY(closeAll);
        closeAll->trigger();
        QTRY_VERIFY(w->isWelcomeVisible());
        // Abrir un archivo la quita
        const QString file = _tmp.filePath("files/nota.txt");
        writeFile(file);
        QVERIFY(w->openFile(file) != kInvalidBuffer);
        QTRY_VERIFY(!w->isWelcomeVisible());
        // Esc también (y Ayuda › Página de inicio la vuelve a mostrar)
        QAction* show = w->findChild<QAction*>(QStringLiteral("actionWelcomePage"));
        QVERIFY(show);
        show->trigger();
        QVERIFY(w->isWelcomeVisible());
        QTest::keyClick(w->welcomePage(), Qt::Key_Escape);
        QVERIFY(!w->isWelcomeVisible());
        QCOMPARE(QFileInfo(w->documents().buffer(w->currentBuffer())->filePath()).fileName(), QStringLiteral("nota.txt"));
    }

    void cardsAndRecentItems()
    {
        const QString file = _tmp.filePath("files/reciente.md");
        writeFile(file, "# Hola\n");
        const QString folder = _tmp.filePath("proyecto");
        QDir().mkpath(folder);
        {
            auto first = window();
            QVERIFY(first->openFile(file) != kInvalidBuffer);
            QVERIFY(first->addFolderToWorkspace(folder));
            // Pasa a recientes al cerrar su pestaña (como en N++)
            first->findChild<QAction*>(QStringLiteral("actionClose"))->trigger();
            first->close();
        }
        auto w = window();
        QTRY_VERIFY(w->isWelcomeVisible());
        WelcomePage* page = w->welcomePage();
        QVERIFY(page->recentFileTexts().contains(file));
        QCOMPARE(page->recentFolderTexts().value(0), QDir::cleanPath(folder));
        // Un reciente lo abre
        QVERIFY(page->clickRecent(file));
        QTRY_VERIFY(!w->isWelcomeVisible());
        QCOMPARE(w->documents().buffer(w->currentBuffer())->filePath(), file);

        // «Nuevo archivo»: se queda en el documento vacío (sin crear otro)
        auto other = window();
        QVERIFY(other->isWelcomeVisible());
        QVERIFY(other->welcomePage()->clickCard(QStringLiteral("welcomeNewFile")));
        QVERIFY(!other->isWelcomeVisible());
        QCOMPARE(other->findChild<EditorPane*>(QStringLiteral("mainPane"))->count(), 1);
        // «Paleta de órdenes» la abre
        other->showWelcomePage();
        QVERIFY(other->welcomePage()->clickCard(QStringLiteral("welcomePalette")));
        QVERIFY(other->commandPalette() && !other->commandPalette()->isHidden());
        QCOMPARE(other->commandPalette()->input()->text(), QStringLiteral(">"));
    }

    void appearanceFromThePage()
    {
        auto w = window();
        QVERIFY(w->isWelcomeVisible());
        WelcomePage* page = w->welcomePage();
        // Oscuro, claro o el del sistema
        page->findChild<QToolButton*>(QStringLiteral("welcomeModeDark"))->click();
        QVERIFY(w->preferences().darkModeEnabled);
        QVERIFY(!w->preferences().darkModeFollowSystem);
        page->findChild<QToolButton*>(QStringLiteral("welcomeModeSystem"))->click();
        QVERIFY(w->preferences().darkModeFollowSystem);
        page->findChild<QToolButton*>(QStringLiteral("welcomeModeLight"))->click();
        QVERIFY(!w->preferences().darkModeEnabled);
        // El tema
        auto* themes = page->findChild<QComboBox*>(QStringLiteral("welcomeTheme"));
        QVERIFY(themes);
        const int dracula = themes->findText(QStringLiteral("Dracula"));
        QVERIFY(dracula > 0);
        themes->setCurrentIndex(dracula);
        emit themes->activated(dracula);
        QCOMPARE(w->preferences().lightThemeName, QStringLiteral("Dracula.xml"));
        // No mostrarla al iniciar
        page->findChild<QCheckBox*>(QStringLiteral("welcomeShowOnStartup"))->setChecked(false);
        QVERIFY(!w->preferences().showWelcome);
        auto again = window();
        QVERIFY(!again->isWelcomeVisible());
        Preferences p = again->preferences();
        p.showWelcome = true;
        p.lightThemeName.clear();
        again->setPreferences(p);
        QTRY_VERIFY(again->isWelcomeVisible());
    }
};

QTEST_MAIN(TestWelcome)
#include "tst_welcome.moc"
