// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 7: preferencias en config.xml (nombres de N++), diálogo y su efecto en
// la ventana: vistas, estilos, documentos nuevos, archivos recientes, resaltado
// inteligente y sangría automática.

#include "FakePrompter.h"

#include "core/ConfigXml.h"
#include "core/Paths.h"
#include "core/Preferences.h"
#include "syntax/SyntaxStyler.h"
#include "ui/DocStatusBar.h"
#include "ui/DocTabBar.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/PreferencesDialog.h"

#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
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
    const Buffer* current() const { return window->documents().buffer(window->currentBuffer()); }
    std::string text() const { return window->documents().text(window->currentBuffer()); }
    void setPrefs(const std::function<void(Preferences&)>& change) const
    {
        Preferences p = window->preferences();
        change(p);
        window->setPreferences(p);
    }
    QAction* action(const char* name) const { return window->findChild<QAction*>(QString::fromLatin1(name)); }
};

// Unas preferencias con todo distinto del valor por defecto
Preferences nonDefault()
{
    Preferences p;
    p.terminalShell = QStringLiteral("zsh -l");
    p.lspEnabled = false;
    p.lspServers = QStringLiteral("clangd=clangd --log=error\npython=off");
    p.statusBarShow = false;
    p.tabBarHide = true;
    p.tabCloseButton = false;
    p.tabDoubleClickCloses = true;
    p.quitOnEmpty = true;
    p.currentLineMode = CurrentLineMode::Frame;
    p.currentLineFrameWidth = 3;
    p.caretWidth = 0;
    p.caretBlinkRate = 250;
    p.multiSelection = false;
    p.virtualSpace = true;
    p.scrollBeyondLastLine = false;
    p.lineCopyCutWithoutSelection = false;
    p.smoothFont = true;
    p.whiteSpaceShow = true;
    p.eolShow = true;
    p.indentGuideLine = false;
    p.wrap = true;
    p.wrapIndent = WrapIndent::Indent;
    p.wrapSymbolShow = true;
    p.lineNumberMargin = false;
    p.lineNumberDynamicWidth = false;
    p.folderStyle = FolderStyle::Circle;
    p.edgeColumns = {80, 120};
    p.edgeBackground = true;
    p.bookmarkMargin = false;
    p.changeHistory = 3;
    p.newDocEol = 0;
    p.newDocUnicodeMode = 0;
    p.newDocCodepage = 1251;
    p.newDocLanguage = static_cast<int>(LangType::Python);
    p.openSaveDir = OpenSaveDir::Custom;
    p.defaultDirPath = "/srv/datos";
    p.recentFilesMax = 5;
    p.recentFilesInSubMenu = true;
    p.recentFilesCustomLength = 40;
    p.tabSize = 2;
    p.tabReplacedBySpace = true;
    p.backspaceUnindent = true;
    p.autoIndent = AutoIndent::CLike;
    p.autocStatus = 2;
    p.autocFromLen = 4;
    p.autocIgnoreNumbers = false;
    p.autocBrief = true;
    p.funcParams = false;
    p.insertParentheses = true;
    p.insertBrackets = true;
    p.insertCurlyBrackets = true;
    p.insertQuotes = true;
    p.insertDoubleQuotes = true;
    p.insertHtmlXmlTag = true;
    p.printLineNumber = false;
    p.printFormFeedPageBreak = true;
    p.printColourMode = 2;
    p.printHeaderLeft = "$(FILE_NAME)";
    p.printHeaderMiddle = "centro";
    p.printHeaderRight = "$(SHORT_DATE)";
    p.printFooterLeft = "izq";
    p.printFooterMiddle = "-$(CURRENT_PRINTING_PAGE)-";
    p.printFooterRight = "der";
    p.printHeaderFontName = "Fuente Inexistente";
    p.printFooterFontName = "Otra Inexistente";
    p.printHeaderFontStyle = 1;
    p.printFooterFontStyle = 3;
    p.printHeaderFontSize = 8;
    p.printFooterFontSize = 11;
    p.printMarginLeft = 10;
    p.printMarginTop = 12;
    p.printMarginRight = 14;
    p.printMarginBottom = 16;
    p.smartHighlight = false;
    p.smartHighlightMatchCase = true;
    p.smartHighlightWholeWord = false;
    p.smartHighlightUseFindSettings = true;
    p.smartHighlightOtherView = true;
    p.tagsMatchHighlight = false;
    p.tagAttributesHighlight = false;
    p.markAllMatchCase = true;
    p.markAllWholeWord = false;
    p.fillFindFieldWithSelected = false;
    p.confirmReplaceInAllOpenDocs = false;
    p.monospacedFontFindDlg = true;
    p.rememberLastSession = false;
    p.snapshotMode = false;
    p.snapshotBackupTiming = 30000;
    p.backupOnSave = BackupOnSave::Verbose;
    p.useCustomBackupDir = true;
    p.backupDir = "/copias";
    p.fileAutoDetection = FileAutoDetection::AllFiles | FileAutoDetection::AutoUpdate;
    p.detectEncoding = false;
    p.fallbackCodepage = 1250;
    return p;
}

} // namespace

class TestPreferences : public QObject
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
        QDir(_tmp.filePath("files")).removeRecursively();
    }

    // --- config.xml -------------------------------------------------------------

    void everyPreferenceRoundTripsThroughConfigXml()
    {
        const QString path = _tmp.filePath("todo.xml");
        const Preferences p = nonDefault();
        {
            ConfigXml config;
            config.load(path);
            storePreferences(config, p);
            QVERIFY(config.save());
        }
        ConfigXml config;
        QVERIFY(config.load(path));
        QVERIFY(loadPreferences(config) == p);

        // Nombres y valores de N++
        const QByteArray xml = readAll(path);
        for (const char* expected : {
                 "<GUIConfig name=\"StatusBar\">hide</GUIConfig>",
                 "doubleClick2Close=\"yes\"",
                 "lineNumberMargin=\"hide\"",
                 "folderMarkStyle=\"circle\"",
                 "lineWrapMethod=\"indent\"",
                 "currentLineIndicator=\"2\"",
                 "edgeMultiColumnPos=\"80 120\"",
                 "whiteSpaceShow=\"show\"",
                 "<GUIConfig name=\"Caret\" width=\"0\" blinkRate=\"250\"",
                 "<GUIConfig name=\"TabSetting\" replaceBySpace=\"yes\" size=\"2\" backspaceUnindent=\"yes\"",
                 "<GUIConfig name=\"MaintainIndent\">2</GUIConfig>",
                 "<GUIConfig name=\"NewDocDefaultSettings\" format=\"0\" encoding=\"0\" codepage=\"1251\" lang=\"22\"",
                 "<GUIConfig name=\"openSaveDir\" value=\"2\" defaultDirPath=\"/srv/datos\"",
                 "<History nbMaxFile=\"5\" inSubMenu=\"yes\" customLength=\"40\"",
                 "<GUIConfig name=\"SmartHighLight\" matchCase=\"yes\" wholeWordOnly=\"no\" useFindSettings=\"yes\" onAnotherView=\"yes\">no</GUIConfig>",
                 "<GUIConfig name=\"Auto-detection\">autoOld</GUIConfig>",
             })
            QVERIFY2(xml.contains(expected), expected);
    }

    void readsAConfigWrittenByNotepadPlusPlus()
    {
        const QString path = _tmp.filePath("npp-config.xml");
        writeFile(path, R"XML(<?xml version="1.0" encoding="UTF-8" ?>
<NotepadPlus>
    <FindHistory nbMaxFindHistoryPath="10" />
    <History nbMaxFile="15" inSubMenu="no" customLength="-1">
        <File filename="C:\Users\u\a.txt" />
        <File filename="/home/u/b.txt" />
    </History>
    <GUIConfigs>
        <GUIConfig name="ToolBar" visible="yes">small</GUIConfig>
        <GUIConfig name="StatusBar">show</GUIConfig>
        <GUIConfig name="TabBar" dragAndDrop="yes" drawTopBar="yes" drawInactiveTab="yes" reduce="yes" closeButton="yes" pinButton="yes" showOnlyPinnedButton="no" buttonsOninactiveTabs="no" doubleClick2Close="no" vertical="no" multiLine="no" hide="no" quitOnEmpty="no" iconSetNumber="0" />
        <GUIConfig name="ScintillaPrimaryView" lineNumberMargin="show" lineNumberDynamicWidth="yes" bookMarkMargin="show" indentGuideLine="show" folderMarkStyle="arrow" isChangeHistoryEnabled="1" lineWrapMethod="aligned" currentLineIndicator="1" currentLineFrameWidth="1" virtualSpace="no" scrollBeyondLastLine="yes" rightClickKeepsSelection="no" selectedTextForegroundSingleColor="no" disableAdvancedScrolling="no" wrapSymbolShow="hide" Wrap="no" borderEdge="yes" isEdgeBgMode="no" edgeMultiColumnPos="100" zoom="0" zoom2="0" whiteSpaceShow="hide" eolShow="hide" eolMode="1" npcShow="hide" npcMode="1" npcCustomColor="no" npcIncludeCcUniEOL="no" npcNoInputC0="yes" ccShow="yes" borderWidth="2" smoothFont="no" paddingLeft="0" paddingRight="0" distractionFreeDivPart="4" lineCopyCutWithoutSelection="yes" multiSelection="yes" columnSel2MultiEdit="yes" />
        <GUIConfig name="TabSetting" replaceBySpace="yes" size="8" backspaceUnindent="no" />
        <GUIConfig name="MaitainIndent">yes</GUIConfig>
        <GUIConfig name="NewDocDefaultSettings" format="0" encoding="4" lang="0" codepage="-1" openAnsiAsUTF8="yes" addNewDocumentOnStartup="no" />
    </GUIConfigs>
</NotepadPlus>
)XML");
        ConfigXml config;
        QVERIFY(config.load(path));
        const Preferences p = loadPreferences(config);
        QCOMPARE(p.folderStyle, FolderStyle::Arrow);
        QCOMPARE(p.edgeColumns, QList<int>{100});
        QCOMPARE(p.tabSize, 8);
        QVERIFY(p.tabReplacedBySpace);
        QCOMPARE(p.autoIndent, AutoIndent::Basic); // errata "MaitainIndent" y valor antiguo "yes"
        QCOMPARE(p.recentFilesMax, 15);
        QCOMPARE(p.newDocEol, 0);
        QCOMPARE(config.recentFiles().files.size(), 2);

        // Guardar conserva lo que Edita no usa
        storePreferences(config, p);
        QVERIFY(config.save());
        const QByteArray xml = readAll(path);
        QVERIFY(xml.contains("<GUIConfig name=\"ToolBar\" visible=\"yes\">small</GUIConfig>"));
        QVERIFY(xml.contains("pinButton=\"yes\""));
        QVERIFY(xml.contains("<FindHistory"));
        QVERIFY(xml.contains("filename=\"/home/u/b.txt\""));
    }

    void edgeColumnsParsing()
    {
        QCOMPARE(parseEdgeColumns("120 80  80 x -3"), (QList<int>{80, 120}));
        QCOMPARE(edgeColumnsToText({80, 120}), QStringLiteral("80 120"));
        QVERIFY(parseEdgeColumns("").isEmpty());
    }

    // --- Diálogo ---------------------------------------------------------------------

    void dialogShowsAndReturnsEveryPreference()
    {
        const Preferences p = nonDefault();
        PreferencesDialog dialog(Preferences{}, {{0, "Normal text"}, {22, "Python"}});
        QCOMPARE(dialog.pageNames(), (QStringList{"general", "editing", "margins", "newDocument", "recentFiles",
                                                  "indentation", "autoCompletion", "highlighting", "searching", "printing", "backup", "misc",
                                                  "languageServers"}));
        QVERIFY(dialog.preferences() == Preferences{});
        dialog.setPreferences(p);
        QVERIFY(dialog.preferences() == p);

        // Editar un control cambia la preferencia
        dialog.setPreferences(Preferences{});
        dialog.findChild<QCheckBox*>("whiteSpaceShow")->setChecked(true);
        dialog.findChild<QSpinBox*>("tabSize")->setValue(3);
        dialog.findChild<QLineEdit*>("edgeColumns")->setText("72");
        auto* folder = dialog.findChild<QComboBox*>("folderStyle");
        folder->setCurrentIndex(folder->findData(static_cast<int>(FolderStyle::None)));
        dialog.findChild<QCheckBox*>("fileAutoDetection")->setChecked(false);
        const Preferences edited = dialog.preferences();
        QVERIFY(edited.whiteSpaceShow);
        QCOMPARE(edited.tabSize, 3);
        QCOMPARE(edited.edgeColumns, QList<int>{72});
        QCOMPARE(edited.folderStyle, FolderStyle::None);
        QCOMPARE(edited.fileAutoDetection, int(FileAutoDetection::Disabled));

        // Controles que dependen de otros
        QVERIFY(!dialog.findChild<QCheckBox*>("detectAllFiles")->isEnabled());
        QVERIFY(!dialog.findChild<QSpinBox*>("currentLineFrameWidth")->isEnabled());
        auto* line = dialog.findChild<QComboBox*>("currentLineMode");
        line->setCurrentIndex(line->findData(2));
        QVERIFY(dialog.findChild<QSpinBox*>("currentLineFrameWidth")->isEnabled());

        // "Aplicar" emite las preferencias sin cerrar
        Preferences applied;
        connect(&dialog, &PreferencesDialog::applyRequested, this, [&](const Preferences& a) { applied = a; });
        dialog.findChild<QPushButton*>("apply")->click();
        QCOMPARE(applied.currentLineMode, CurrentLineMode::Frame);
    }

    // --- Efecto en la ventana -----------------------------------------------------------

    void viewPreferencesApplyToBothViews()
    {
        writeFile(file("a.cpp"), "int main()\n{\n    return 0;\n}\n");
        Window x;
        x.w().openFile(file("a.cpp"));
        x.w().cloneToOtherView();
        x.setPrefs([](Preferences& p) {
            p.wrap = true;
            p.whiteSpaceShow = true;
            p.eolShow = true;
            p.caretWidth = 0;
            p.virtualSpace = true;
            p.scrollBeyondLastLine = false;
            p.edgeColumns = {80, 100};
            p.currentLineMode = CurrentLineMode::Frame;
            p.currentLineFrameWidth = 2;
            p.folderStyle = FolderStyle::None;
            p.tabSize = 3;
            p.tabReplacedBySpace = true;
            p.lineNumberMargin = false;
        });
        for (EditorPane* pane : {x.w().mainPane(), x.w().subPane()}) {
            EditorView* v = pane->editor();
            QCOMPARE(v->sci(SCI_GETWRAPMODE), sptr_t{SC_WRAP_WORD});
            QCOMPARE(v->sci(SCI_GETVIEWWS), sptr_t{SCWS_VISIBLEALWAYS});
            QCOMPARE(v->sci(SCI_GETVIEWEOL), sptr_t{1});
            QCOMPARE(v->sci(SCI_GETCARETSTYLE) & CARETSTYLE_BLOCK, sptr_t{CARETSTYLE_BLOCK});
            QVERIFY(v->sci(SCI_GETVIRTUALSPACEOPTIONS) & SCVS_USERACCESSIBLE);
            QCOMPARE(v->sci(SCI_GETENDATLASTLINE), sptr_t{1});
            QCOMPARE(v->sci(SCI_GETEDGEMODE), sptr_t{EDGE_MULTILINE});
            QCOMPARE(v->sci(SCI_GETCARETLINEFRAME), sptr_t{2});
            QCOMPARE(v->sci(SCI_GETMARGINWIDTHN, 3), sptr_t{0}); // sin margen de plegado
            QCOMPARE(v->sci(SCI_GETTABWIDTH), sptr_t{3});
            QCOMPARE(v->sci(SCI_GETUSETABS), sptr_t{0});
            QCOMPARE(v->sci(SCI_GETMARGINWIDTHN, 0), sptr_t{0});
        }
        // Una sola columna con fondo
        x.setPrefs([](Preferences& p) {
            p.edgeColumns = {72};
            p.edgeBackground = true;
            p.folderStyle = FolderStyle::Circle;
        });
        QCOMPARE(x.editor()->sci(SCI_GETEDGEMODE), sptr_t{EDGE_BACKGROUND});
        QCOMPARE(x.editor()->sci(SCI_GETEDGECOLUMN), sptr_t{72});
        QCOMPARE(x.editor()->sci(SCI_MARKERSYMBOLDEFINED, SC_MARKNUM_FOLDER), sptr_t{SC_MARK_CIRCLEPLUS});
        QVERIFY(x.editor()->sci(SCI_GETMARGINWIDTHN, 3) > 0);

        // Se guardan en config.xml y la próxima ventana arranca con ellas
        Window y;
        QVERIFY(y.w().preferences().wrap);
        QCOMPARE(y.editor()->sci(SCI_GETWRAPMODE), sptr_t{SC_WRAP_WORD});
    }

    void generalPreferencesAffectTheWindow()
    {
        Window x;
        x.w().show();
        x.setPrefs([](Preferences& p) {
            p.statusBarShow = false;
            p.tabCloseButton = false;
            p.tabBarHide = true;
        });
        QVERIFY(x.w().docStatusBar()->isHidden());
        QVERIFY(!x.w().mainPane()->tabBar()->tabsClosable());
        QVERIFY(x.w().mainPane()->tabBar()->isHidden());
        x.setPrefs([](Preferences& p) { p = Preferences{}; });
        QVERIFY(!x.w().docStatusBar()->isHidden());
        QVERIFY(x.w().mainPane()->tabBar()->tabsClosable());
    }

    void viewMenuSymbolsArePreferences()
    {
        Window x;
        x.action("actionShowWhiteSpace")->trigger();
        QVERIFY(x.w().preferences().whiteSpaceShow);
        QCOMPARE(x.editor()->sci(SCI_GETVIEWWS), sptr_t{SCWS_VISIBLEALWAYS});
        x.action("actionShowAll")->trigger();
        QVERIFY(x.w().preferences().eolShow);
        QVERIFY(x.action("actionShowEol")->isChecked());
        x.action("actionWrap")->trigger();
        QVERIFY(x.w().preferences().wrap);
        ConfigXml config;
        QVERIFY(config.load(QDir(configDir()).filePath("config.xml")));
        QVERIFY(loadPreferences(config).wrap);
    }

    void newDocumentSettings()
    {
        Window x;
        x.setPrefs([](Preferences& p) {
            p.newDocEol = 0;
            p.newDocUnicodeMode = static_cast<int>(UniMode::Utf8Bom);
            p.newDocLanguage = static_cast<int>(LangType::Python);
        });
        x.w().newDocument();
        QCOMPARE(x.current()->eol(), EolType::Windows);
        QCOMPARE(x.current()->encoding(), TextEncoding::unicode(UniMode::Utf8Bom));
        QCOMPARE(x.current()->language(), LangType::Python);
        QCOMPARE(x.editor()->sci(SCI_GETEOLMODE), sptr_t{SC_EOL_CRLF});
        x.setPrefs([](Preferences& p) {
            p.newDocUnicodeMode = 0;
            p.newDocCodepage = 1251;
        });
        x.w().newDocument();
        QCOMPARE(x.current()->encoding().codepage, 1251);
    }

    void openSaveDirectoryFollowsThePreference()
    {
        writeFile(file("sub/doc.txt"), "x");
        Window x;
        x.w().openFile(file("sub/doc.txt"));
        x.prompter->openNames.enqueue({});
        x.w().openWithDialog();
        // FakePrompter no recibe el directorio; se comprueba con "Guardar como" de un documento nuevo
        x.w().newDocument();
        x.setPrefs([this](Preferences& p) {
            p.openSaveDir = OpenSaveDir::Custom;
            p.defaultDirPath = _tmp.filePath("files");
        });
        x.w().saveBufferAs(x.w().currentBuffer());
        QCOMPARE(x.prompter->asked.last(), QStringLiteral("saveAs:%1").arg(QDir(_tmp.filePath("files")).filePath("new 1")));
    }

    void copyWithoutSelectionCopiesTheLine()
    {
        Window x;
        x.editor()->sends(SCI_SETTEXT, 0, "primera\nsegunda\n");
        x.editor()->sci(SCI_GOTOPOS, 10);
        QGuiApplication::clipboard()->clear();
        x.action("actionCopy")->trigger();
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("segunda\n"));
        x.action("actionCut")->trigger();
        QCOMPARE(x.text(), "primera\n"s);

        x.setPrefs([](Preferences& p) { p.lineCopyCutWithoutSelection = false; });
        QGuiApplication::clipboard()->setText(QStringLiteral("intacto"));
        x.editor()->sci(SCI_GOTOPOS, 2);
        x.action("actionCopy")->trigger();
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("intacto"));
    }

    void quitOnEmptyClosesTheWindow()
    {
        Window x;
        x.w().show();
        x.setPrefs([](Preferences& p) { p.quitOnEmpty = true; });
        QVERIFY(x.w().isVisible());
        QVERIFY(x.w().closeBuffer(x.w().currentBuffer()));
        QTRY_VERIFY(!x.w().isVisible());
    }

    // --- Archivos recientes ---------------------------------------------------------------

    void recentFilesList()
    {
        writeFile(file("r1.txt"), "1");
        writeFile(file("r2.txt"), "2");
        writeFile(file("r3.txt"), "3");
        Window x;
        x.setPrefs([](Preferences& p) { p.recentFilesMax = 2; });
        const BufferId a = x.w().openFile(file("r1.txt"));
        const BufferId b = x.w().openFile(file("r2.txt"));
        const BufferId c = x.w().openFile(file("r3.txt"));
        QVERIFY(x.w().recentFiles().isEmpty());
        QVERIFY(x.w().closeBuffer(a));
        QVERIFY(x.w().closeBuffer(b));
        QVERIFY(x.w().closeBuffer(c));
        // El más reciente primero, y como mucho nbMaxFile
        QCOMPARE(x.w().recentFiles(), (QStringList{file("r3.txt"), file("r2.txt")}));
        QAction* entry = x.action("actionRecent0");
        QVERIFY(entry);
        QVERIFY(entry->text().contains(QStringLiteral("r3.txt")));
        QVERIFY(x.action("actionClearRecent"));

        // Abrir uno lo quita de la lista; Ctrl+Mayús+T abre el último cerrado
        entry->trigger();
        QCOMPARE(x.current()->filePath(), file("r3.txt"));
        QCOMPARE(x.w().recentFiles(), QStringList{file("r2.txt")});
        const BufferId reopened = x.w().reopenLastClosed();
        QCOMPARE(reopened, x.w().documents().findByPath(file("r2.txt")));
        QVERIFY(reopened != kInvalidBuffer);
        QVERIFY(x.w().recentFiles().isEmpty());
        QVERIFY(!x.action("actionRecent0"));

        // En un submenú, con el nombre solo
        QVERIFY(x.w().closeBuffer(x.w().currentBuffer()));
        x.setPrefs([](Preferences& p) {
            p.recentFilesInSubMenu = true;
            p.recentFilesCustomLength = 0;
        });
        auto* sub = x.w().findChild<QMenu*>("menuRecentFiles");
        QVERIFY(sub);
        QCOMPARE(sub->actions().first()->text(), QStringLiteral("&1: r2.txt"));

        // Se guarda en <History> al salir; con la sesión recordada, los abiertos no se añaden
        x.w().setSessionEnabled(true);
        QVERIFY(x.w().close());
        ConfigXml config;
        QVERIFY(config.load(QDir(configDir()).filePath("config.xml")));
        QCOMPARE(config.recentFiles().files, QStringList{file("r2.txt")});
        Window y;
        QCOMPARE(y.w().recentFiles(), QStringList{file("r2.txt")});
    }

    void missingRecentFileOffersToRemoveIt()
    {
        writeFile(file("se-va.txt"), "x");
        Window x;
        QVERIFY(x.w().closeBuffer(x.w().openFile(file("se-va.txt"))));
        QFile::remove(file("se-va.txt"));
        x.prompter->confirmations.enqueue(true);
        x.action("actionRecent0")->trigger();
        QCOMPARE(x.prompter->asked, QStringList{"confirm:Recent file"});
        QVERIFY(x.w().recentFiles().isEmpty());
    }

    // --- Resaltado inteligente y sangría -------------------------------------------------

    void smartHighlightMarksOtherOccurrences()
    {
        Window x;
        x.w().show();
        QVERIFY(QTest::qWaitForWindowExposed(&x.w()));
        x.editor()->sends(SCI_SETTEXT, 0, "uno dos Uno uno-tres unos\n");
        auto smartAt = [&](sptr_t pos) { return x.editor()->sci(SCI_INDICATORVALUEAT, kIndicatorSmart, pos); };
        x.setPrefs([](Preferences& p) { p.smartHighlightMatchCase = true; }); // N++: no distingue por defecto
        x.editor()->sci(SCI_SETSEL, 0, 3);
        QTRY_COMPARE(smartAt(12), sptr_t{1});  // "uno" de "uno-tres"
        QCOMPARE(smartAt(8), sptr_t{0});       // "Uno": distingue mayúsculas
        QCOMPARE(smartAt(21), sptr_t{0});      // "unos": palabra completa

        x.setPrefs([](Preferences& p) {
            p.smartHighlightMatchCase = false;
            p.smartHighlightWholeWord = false;
        });
        x.editor()->sci(SCI_SETSEL, 0, 0);
        x.editor()->sci(SCI_SETSEL, 0, 3);
        QTRY_COMPARE(smartAt(8), sptr_t{1});
        QCOMPARE(smartAt(21), sptr_t{1});

        // Sin selección no queda nada marcado; desactivado, tampoco
        x.editor()->sci(SCI_SETSEL, 5, 5);
        QTRY_COMPARE(smartAt(12), sptr_t{0});
        x.setPrefs([](Preferences& p) { p.smartHighlight = false; });
        x.editor()->sci(SCI_SETSEL, 0, 3);
        QTest::qWait(50);
        QCOMPARE(smartAt(12), sptr_t{0});
    }

    void basicAutoIndentKeepsThePreviousIndentation()
    {
        Window x;
        x.editor()->sends(SCI_SETTEXT, 0, "\tuno");
        x.editor()->sci(SCI_DOCUMENTEND);
        QTest::keyClick(x.editor(), Qt::Key_Return);
        QTest::keyClicks(x.editor(), "dos");
        QCOMPARE(x.text(), "\tuno\n\tdos"s);

        x.setPrefs([](Preferences& p) { p.autoIndent = AutoIndent::None; });
        QTest::keyClick(x.editor(), Qt::Key_Return);
        QCOMPARE(x.text(), "\tuno\n\tdos\n"s);
    }

    void cLikeAutoIndentFollowsBraces()
    {
        writeFile(file("c.cpp"), "");
        Window x;
        x.setPrefs([](Preferences& p) {
            p.autoIndent = AutoIndent::CLike;
            p.tabReplacedBySpace = true;
            p.tabSize = 4;
        });
        x.w().openFile(file("c.cpp"));
        QTest::keyClicks(x.editor(), "if (a) {");
        QTest::keyClick(x.editor(), Qt::Key_Return);
        QTest::keyClicks(x.editor(), "b;");
        QTest::keyClick(x.editor(), Qt::Key_Return);
        QTest::keyClicks(x.editor(), "}");
        QCOMPARE(x.text(), "if (a) {\n    b;\n}"s);

        // Condición sin llave: solo la línea siguiente se sangra
        QTest::keyClick(x.editor(), Qt::Key_Return);
        QTest::keyClicks(x.editor(), "while (c)");
        QTest::keyClick(x.editor(), Qt::Key_Return);
        QTest::keyClicks(x.editor(), "d;");
        QTest::keyClick(x.editor(), Qt::Key_Return);
        QTest::keyClicks(x.editor(), "e;");
        QCOMPARE(x.text(), "if (a) {\n    b;\n}\nwhile (c)\n    d;\ne;"s);
    }

    void pythonAutoIndentAfterColon()
    {
        writeFile(file("p.py"), "");
        Window x;
        x.setPrefs([](Preferences& p) {
            p.autoIndent = AutoIndent::CLike;
            p.tabReplacedBySpace = true;
        });
        x.w().openFile(file("p.py"));
        QTest::keyClicks(x.editor(), "def f():");
        QTest::keyClick(x.editor(), Qt::Key_Return);
        QTest::keyClicks(x.editor(), "x = ':'");
        QTest::keyClick(x.editor(), Qt::Key_Return);
        QTest::keyClicks(x.editor(), "y");
        QCOMPARE(x.text(), "def f():\n    x = ':'\n    y"s);
    }

    void replaceAllInOpenDocumentsAsksFirst()
    {
        Window x;
        x.editor()->sends(SCI_SETTEXT, 0, "aaa");
        FindReplaceDialog* dialog = x.w().findDialog();
        SearchOptions o;
        o.pattern = "a";
        o.replacement = "b";
        dialog->setOptions(o);
        x.prompter->confirmations.enqueue(false);
        emit dialog->replaceAllInOpenRequested();
        QCOMPARE(x.text(), "aaa"s);
        x.prompter->confirmations.enqueue(true);
        emit dialog->replaceAllInOpenRequested();
        QCOMPARE(x.text(), "bbb"s);
        x.setPrefs([](Preferences& p) { p.confirmReplaceInAllOpenDocs = false; });
        o.pattern = "b";
        o.replacement = "c";
        dialog->setOptions(o);
        emit dialog->replaceAllInOpenRequested();
        QCOMPARE(x.text(), "ccc"s);
        QCOMPARE(x.prompter->asked.size(), 2);
    }
};

QTEST_MAIN(TestPreferences)
#include "tst_preferences.moc"
