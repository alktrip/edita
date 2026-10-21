// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 17: interfaz moderna. Modo claro u oscuro (DarkMode de N++, siguiendo al
// sistema), barra de herramientas, iconos, pestañas y fuente del editor.

#include "FakePrompter.h"
#include "core/ConfigXml.h"
#include "core/Preferences.h"
#include "syntax/SyntaxStyler.h"
#include "ui/Appearance.h"
#include "ui/DocTabBar.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/Icons.h"
#include "ui/MainWindow.h"
#include "ui/PreferencesDialog.h"
#include "ui/Translations.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QListWidget>
#include <QStackedWidget>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>

#include <ScintillaMessages.h>

using namespace edita;
using edita::test::FakePrompter;

namespace {

// El color más opaco de un icono (el de su trazo)
QColor strokeColor(const QIcon& icon)
{
    const QImage image = icon.pixmap(24, 24).toImage();
    QColor best(0, 0, 0, 0);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor c = image.pixelColor(x, y);
            if (c.alpha() > best.alpha())
                best = c;
        }
    }
    return best;
}

std::unique_ptr<MainWindow> window(const Preferences* prefs = nullptr)
{
    auto w = std::make_unique<MainWindow>();
    w->setPrompter(std::make_unique<FakePrompter>());
    if (prefs)
        w->setPreferences(*prefs);
    return w;
}

Preferences withMode(bool dark)
{
    Preferences p;
    p.darkModeFollowSystem = false;
    p.darkModeEnabled = dark;
    return p;
}

} // namespace

class TestAppearance : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void cleanup()
    {
        QDir(_tmp.filePath("config")).removeRecursively();
    }

    void preferencesUseTheNamesOfNotepadPlusPlus()
    {
        ConfigXml config;
        config.load(_tmp.filePath("a.xml"));
        // Por defecto: seguir al sistema, DarkModeDefault.xml en oscuro, barra visible
        const Preferences defaults = loadPreferences(config);
        QVERIFY(defaults.darkModeFollowSystem);
        QVERIFY(!defaults.darkModeEnabled);
        QCOMPARE(defaults.darkThemeName, QStringLiteral("DarkModeDefault.xml"));
        QVERIFY(defaults.lightThemeName.isEmpty());
        QVERIFY(defaults.toolbarVisible);
        QCOMPARE(defaults.toolbarIcons, ToolbarIcons::Standard);
        QVERIFY(defaults.systemMonospaceFont);

        Preferences p;
        p.darkModeEnabled = true;
        p.darkModeFollowSystem = false;
        p.darkThemeName = QStringLiteral("Monokai.xml");
        p.lightThemeName = QStringLiteral("Solarized-light.xml");
        p.toolbarVisible = false;
        p.toolbarIcons = ToolbarIcons::Large;
        p.systemMonospaceFont = false;
        storePreferences(config, p);
        // <GUIConfig name="DarkMode" enable="yes" enableWindowsMode="no" darkThemeName=… lightThemeName=…/>
        QCOMPARE(config.guiConfig("DarkMode", "enable"), QStringLiteral("yes"));
        QCOMPARE(config.guiConfig("DarkMode", "enableWindowsMode"), QStringLiteral("no"));
        QCOMPARE(config.guiConfig("DarkMode", "darkThemeName"), QStringLiteral("Monokai.xml"));
        // <GUIConfig name="ToolBar" visible="no">large</GUIConfig>
        QCOMPARE(config.guiConfig("ToolBar", "visible"), QStringLiteral("no"));
        QCOMPARE(config.guiConfigText("ToolBar"), QStringLiteral("large"));
        const Preferences back = loadPreferences(config);
        QCOMPARE(back.darkThemeName, p.darkThemeName);
        QCOMPARE(back.lightThemeName, p.lightThemeName);
        QCOMPARE(back.toolbarIcons, ToolbarIcons::Large);
        QVERIFY(!back.toolbarVisible && back.darkModeEnabled && !back.darkModeFollowSystem && !back.systemMonospaceFont);

        // Un config.xml de antes, con un tema elegido en el menú: es el tema del modo claro
        ConfigXml old;
        old.load(_tmp.filePath("b.xml"));
        old.setGuiConfig("stylerTheme", "path", "/algo/themes/Zenburn.xml");
        QCOMPARE(loadPreferences(old).lightThemeName, QStringLiteral("Zenburn.xml"));
    }

    void darkModeFollowsTheSystemOrTheChoice()
    {
        Preferences p;
        QVERIFY(appearance::isDark(p, Qt::ColorScheme::Dark));
        QVERIFY(!appearance::isDark(p, Qt::ColorScheme::Light));
        QVERIFY(!appearance::isDark(p, Qt::ColorScheme::Unknown)); // el escritorio no lo dice: lo elegido
        p.darkModeEnabled = true;
        QVERIFY(appearance::isDark(p, Qt::ColorScheme::Unknown));
        p.darkModeFollowSystem = false;
        QVERIFY(appearance::isDark(p, Qt::ColorScheme::Light));
        p.darkModeEnabled = false;
        QVERIFY(!appearance::isDark(p, Qt::ColorScheme::Dark));

        // Las paletas: fondo oscuro con texto claro y al revés; el mismo acento verde
        const QPalette dark = appearance::palette(true);
        const QPalette light = appearance::palette(false);
        QVERIFY(dark.color(QPalette::Window).lightness() < 60 && dark.color(QPalette::WindowText).lightness() > 180);
        QVERIFY(light.color(QPalette::Window).lightness() > 220 && light.color(QPalette::WindowText).lightness() < 60);
        QVERIFY(appearance::accent(true).green() > appearance::accent(true).red());
        QVERIFY(appearance::styleSheet(true).contains(QLatin1String("tab-close-dark.svg")));
        QVERIFY(!appearance::styleSheet(false).contains(QLatin1Char('@'))); // todos los colores sustituidos
    }

    void eachModeHasItsTheme()
    {
        const Preferences darkPrefs = withMode(true);
        auto w = window(&darkPrefs);
        // Modo oscuro: paleta oscura, hoja de estilos y DarkModeDefault.xml
        QVERIFY(appearance::isDarkApplied());
        QVERIFY(QApplication::palette().color(QPalette::Window).lightness() < 60);
        QCOMPARE(qApp->styleSheet(), appearance::styleSheet(true));
        QCOMPARE(QFileInfo(w->syntax().activeThemePath()).fileName(), QStringLiteral("DarkModeDefault.xml"));

        // Un tema elegido en el menú queda como el del modo oscuro…
        QString monokai;
        for (const auto& theme : w->syntax().themes()) {
            if (theme.name == QLatin1String("Monokai"))
                monokai = theme.path;
        }
        QVERIFY(!monokai.isEmpty());
        QVERIFY(w->setTheme(monokai));
        QCOMPARE(w->preferences().darkThemeName, QStringLiteral("Monokai.xml"));
        // …al pasar a claro vuelve el de serie, y al volver a oscuro, Monokai
        Preferences p = w->preferences();
        p.darkModeEnabled = false;
        w->setPreferences(p);
        QVERIFY(!appearance::isDarkApplied());
        QVERIFY(w->syntax().activeThemePath().isEmpty());
        QVERIFY(QApplication::palette().color(QPalette::Window).lightness() > 220);
        p.darkModeEnabled = true;
        w->setPreferences(p);
        QCOMPARE(w->syntax().activeThemePath(), monokai);

        // Se recuerda al volver a abrir
        auto again = window();
        QCOMPARE(again->syntax().activeThemePath(), monokai);
        QVERIFY(appearance::isDarkApplied());
    }

    void followsTheSystemLive()
    {
        auto w = window(); // por defecto, sigue al sistema
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
        if (QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Dark)
            QSKIP("esta plataforma no cambia el esquema de colores");
        QTRY_VERIFY(appearance::isDarkApplied());
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        QTRY_VERIFY(!appearance::isDarkApplied());
        QGuiApplication::styleHints()->unsetColorScheme();
    }

    void toolbarHasTheButtonsOfNotepadPlusPlus()
    {
        const Preferences light = withMode(false);
        auto w = window(&light);
        QToolBar* bar = w->toolBar();
        QVERIFY(bar && bar->isVisibleTo(w.get()));
        QStringList names;
        for (QAction* action : bar->actions())
            names << (action->isSeparator() ? QStringLiteral("|") : action->objectName());
        QCOMPARE(names.join(QLatin1Char(' ')),
                 QStringLiteral("actionNew actionOpen actionSave actionSaveAll actionClose actionCloseAll actionPrint | "
                                "actionCut actionCopy actionPaste | actionUndo actionRedo | actionFind actionReplace | "
                                "actionZoomIn actionZoomOut | actionWrap actionShowAll actionShowIndentGuides | "
                                "actionDefineLanguage actionDocumentMap actionDocumentList actionFunctionList actionFolderAsWorkspace | "
                                "actionRecordMacro actionPlayMacro actionRunMacro actionSaveMacro"));
        // Cada botón con su icono (que también se ve en el menú)
        for (QAction* action : bar->actions()) {
            if (!action->isSeparator())
                QVERIFY2(!action->icon().isNull(), qPrintable(action->objectName()));
        }
        QCOMPARE(bar->iconSize(), QSize(20, 20));

        // Preferencias: iconos grandes; ocultarla
        Preferences p = w->preferences();
        p.toolbarIcons = ToolbarIcons::Large;
        w->setPreferences(p);
        QCOMPARE(bar->iconSize(), QSize(24, 24));
        p.toolbarVisible = false;
        w->setPreferences(p);
        QVERIFY(!bar->isVisibleTo(w.get()));
        QCOMPARE(w->createPopupMenu(), nullptr); // sin el menú de QMainWindow que la oculta
    }

    void iconsFollowThePalette()
    {
        const Preferences light = withMode(false);
        auto w = window(&light);
        QAction* save = w->findChild<QAction*>(QStringLiteral("actionSave"));
        QCOMPARE(icons::iconName(save), QStringLiteral("save"));
        QVERIFY(strokeColor(save->icon()).lightness() < 80); // trazo oscuro sobre fondo claro
        Preferences p = w->preferences();
        p.darkModeEnabled = true;
        w->setPreferences(p);
        QTRY_VERIFY(strokeColor(save->icon()).lightness() > 170); // y claro sobre oscuro
        QVERIFY(icons::icon(QStringLiteral("no-existe")).isNull());
    }

    void tabsShowTheStateWithoutFloppies()
    {
        const Preferences light = withMode(false);
        auto w = window(&light);
        DocTabBar* tabs = w->mainPane()->tabBar();
        // Guardado: sin indicador (un hueco transparente, para que el texto no salte)
        QCOMPARE(tabs->stateAt(0), DocTabBar::TabState::Saved);
        QCOMPARE(strokeColor(tabs->tabIcon(0)).alpha(), 0);
        // Con cambios: un punto del color de "sin guardar"
        w->editor()->sends(SCI_REPLACESEL, 0, "x");
        QTRY_COMPARE(tabs->stateAt(0), DocTabBar::TabState::Dirty);
        const QColor dot = strokeColor(tabs->tabIcon(0));
        QCOMPARE(dot.rgb(), appearance::unsavedColor(false).rgb());
        QVERIFY(tabs->tabsClosable());
    }

    void userLanguagesHaveALightAndADarkVersion()
    {
        // markdown._preinstalled(_DM).udl.xml: N++ elige el del modo (darkModeTheme)
        const QString md = _tmp.filePath("notas.md");
        QFile f(md);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("# Hola\n");
        f.close();
        const Preferences light = withMode(false);
        auto w = window(&light);
        const BufferId id = w->openFile(md);
        QCOMPARE(w->documents().buffer(id)->userLanguage(), QStringLiteral("Markdown (preinstalled)"));
        Preferences p = w->preferences();
        p.darkModeEnabled = true;
        w->setPreferences(p);
        QCOMPARE(w->documents().buffer(id)->userLanguage(), QStringLiteral("Markdown (preinstalled dark mode)"));
        // Uno elegido a mano no se toca
        w->setUserLanguage(QStringLiteral("Markdown (preinstalled)"));
        p.darkModeEnabled = false;
        w->setPreferences(p);
        p.darkModeEnabled = true;
        w->setPreferences(p);
        QCOMPARE(w->documents().buffer(id)->userLanguage(), QStringLiteral("Markdown (preinstalled)"));
    }

    void preferencesShowEverythingWithoutResizing_data()
    {
        QTest::addColumn<QString>("language");
        QTest::addColumn<bool>("dark");
        QTest::newRow("español, claro") << "es" << false;
        QTest::newRow("español, oscuro") << "es" << true;
        QTest::newRow("inglés, claro") << "en" << false;
    }

    void preferencesShowEverythingWithoutResizing()
    {
        // Al abrir Preferencias se ven enteros los nombres de las páginas (con su icono)
        // y cabe la página más grande, sin tener que agrandar el diálogo
        QFETCH(QString, language);
        QFETCH(bool, dark);
        i18n::install(language);
        appearance::apply(dark);
        PreferencesDialog dialog(Preferences{}, {});
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* list = dialog.findChild<QListWidget*>(QStringLiteral("categories"));
        QVERIFY(list);
        const QRect viewport = list->viewport()->rect();
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* item = list->item(i);
            const QRect rect = list->visualItemRect(item);
            const int needed = list->iconSize().width() + list->fontMetrics().horizontalAdvance(item->text());
            QVERIFY2(viewport.contains(rect), qPrintable(item->text()));
            QVERIFY2(rect.width() > needed, qPrintable(item->text()));
        }
        auto* pages = dialog.findChild<QStackedWidget*>();
        for (int i = 0; i < pages->count(); ++i) {
            const QSize page = pages->widget(i)->sizeHint();
            QVERIFY2(page.width() <= pages->width() && page.height() <= pages->height(),
                     qPrintable(pages->widget(i)->objectName()));
        }
        i18n::install(QStringLiteral("en"));
    }

    void editorUsesTheSystemMonospaceFont()
    {
        // Los temas de N++ piden Courier New (stylers.xml) o Consolas (DarkModeDefault.xml)
        const std::string system = QFontDatabase::systemFont(QFontDatabase::FixedFont).family().toStdString();
        const Preferences light = withMode(false);
        auto w = window(&light);
        char font[256] = {};
        w->editor()->send(SCI_STYLEGETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>(font));
        QCOMPARE(std::string(font), system);
        if (!QFontDatabase::hasFamily(QStringLiteral("Courier New")))
            QSKIP("Courier New no está instalada: no se puede ver la diferencia");
        Preferences p = w->preferences();
        p.systemMonospaceFont = false;
        w->setPreferences(p);
        w->editor()->send(SCI_STYLEGETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>(font));
        QCOMPARE(std::string(font), std::string("Courier New"));
    }
};

QTEST_MAIN(TestAppearance)
#include "tst_appearance.moc"
