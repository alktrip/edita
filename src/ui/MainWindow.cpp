// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
//
// Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team).
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "ui/MainWindow.h"

#include "AppInfo.h"
#include "core/Backup.h"
#include "core/Codec.h"
#include "core/CrashReport.h"
#include "core/DateTimeFormat.h"
#include "core/OnSelection.h"
#include "core/CommandLine.h"
#include "core/ContextMenuXml.h"
#include "core/Formatters.h"
#include "lsp/LspServers.h"
#include "core/PluginInstaller.h"
#include "core/IssueReport.h"
#include "core/UpdateCheck.h"
#include "core/Paths.h"
#include "ui/Appearance.h"
#include "ui/AutoCompletion.h"
#include "ui/ColumnEditorDialog.h"
#include "ui/DocStatusBar.h"
#include "ui/DocTabBar.h"
#include "ui/HashDialog.h"
#include "ui/FindCharsDialog.h"
#include "ui/EditPanels.h"
#include "ui/UrlLinks.h"
#include "ui/SpellCheck.h"
#include "core/SpellChecker.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/FileIcons.h"
#include "ui/FunctionParser.h"
#include "ui/ColorPreview.h"
#include "ui/GitChanges.h"
#include "ui/NotificationBar.h"
#include "ui/QuickPicker.h"
#include "ui/RegexTester.h"
#include "ui/RunDialog.h"
#include "ui/TerminalPanel.h"
#include "ui/LspManager.h"
#include "ui/ManualWindow.h"
#include "ui/ProblemsPanel.h"
#include "ui/CompareSession.h"
#include "ui/GitBlame.h"
#include "ui/GitStatus.h"
#include "ui/Snippets.h"
#include "ui/EolAnnotations.h"
#include "ui/RainbowBrackets.h"
#include "ui/Breadcrumbs.h"
#include "ui/StickyScroll.h"
#include "ui/FindBar.h"
#include "ui/WelcomePage.h"
#include "ui/ScrollBarMarks.h"
#include "ui/Icons.h"
#include "ui/Indentation.h"
#include "ui/MacroSteps.h"
#include "ui/Panels.h"
#include "core/RemoteCatalog.h"
#include "ui/PluginManager.h"
#include "ui/PreferencesDialog.h"
#include "ui/Printer.h"
#include "ui/Prompter.h"
#include "ui/SearchResultsPanel.h"
#include "ui/ShortcutMapperDialog.h"
#include "ui/SingleInstance.h"
#include "ui/SmartHighlighter.h"
#include "ui/StyleConfiguratorDialog.h"
#include "ui/Translations.h"
#include "ui/UserLanguageDialog.h"
#include "ui/XmlTagMatcher.h"
#include "search/FileSearch.h"
#include "core/FileScan.h"

#include <ILexer.h>
#include <Lexilla.h>

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDate>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDockWidget>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QTemporaryDir>
#include <QFontDatabase>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeySequence>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QMimeData>
#include <QPdfWriter>
#include <QPixmap>
#include <QProcess>
#include <QStandardPaths>
#include <QPrintDialog>
#include <QPrinter>
#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QScopedValueRollback>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStringDecoder>
#include <QSplitter>
#include <QStyleHints>
#include <QSysInfo>
#include <QToolBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <pugixml.hpp>

#include <algorithm>
#include <functional>
#include <limits>
#include <functional>
#include <cstring>
#include <vector>

namespace edita {

namespace {

// Ruta de navegación: hasta este tamaño se rehacen los símbolos al escribir (y se calculan
// los bloques con el plegado de todo el documento)
constexpr sptr_t kMaxSymbolsWhileTyping = 2 * 1024 * 1024;

// Submenús de "Juegos de caracteres", en el orden del menú de N++ (Notepad_plus.rc)
struct CharsetCategory
{
    const char* name;
    std::vector<int> codepages;
};

const std::vector<CharsetCategory>& charsetCategories()
{
    static const std::vector<CharsetCategory> categories{
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Arabic"), {28596, 720, 1256}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Baltic"), {28594, 28603, 775, 1257}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Celtic"), {28604}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Cyrillic"), {28595, 20866, 21866, 10007, 855, 866, 1251}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Central European"), {852, 1250}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Chinese"), {950, 936}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Eastern European"), {28592}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Greek"), {28597, 737, 869, 1253}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Hebrew"), {28598, 862, 1255}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Japanese"), {932}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Korean"), {949, 51949}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "North European"), {861, 865}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Thai"), {874}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Turkish"), {28593, 28599, 857, 1254}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Western European"), {28591, 28605, 850, 858, 860, 863, 437, 1252}},
        {QT_TRANSLATE_NOOP("edita::MainWindow", "Vietnamese"), {1258}},
    };
    return categories;
}

constexpr UniMode kMenuUnicodeModes[] = {UniMode::Utf8, UniMode::Utf8Bom, UniMode::Utf16BeBom, UniMode::Utf16LeBom};

QString displayName(const TextEncoding& encoding)
{
    return QString::fromStdString(encodingDisplayName(encoding));
}

// IDM_* de N++ de las órdenes que no se graban en las macros (para shortcuts.xml)
int shortcutOnlyCommandFor(const QString& actionName)
{
    static const std::pair<const char*, int> kCommands[] = {
        {"actionOpen", 41002},           {"actionReload", 41014},         {"actionSaveCopy", 41015},
        {"actionPrint", 41010},          {"actionPrintNow", 1001},        {"actionReopenClosed", 41021},
        {"actionQuit", 41011},           {"actionCompleteFunction", 50000}, {"actionCompleteWord", 50001},
        {"actionFunctionHint", 50002},   {"actionCompletePath", 50006},   {"actionReadOnly", 42028},
        {"actionFind", 43001},           {"actionFindInFiles", 43013},    {"actionReplace", 43003},
        {"actionMark", 43054},
        {"actionGoTo", 43004},           {"actionNextResult", 43046},     {"actionPreviousResult", 43047},
        {"actionWrap", 44022},           {"actionShowWhiteSpace", 44025}, {"actionShowEol", 44026},
        {"actionShowIndentGuides", 44020}, {"actionShowWrapSymbol", 44041}, {"actionShowAll", 44019},
        {"actionMoveToOtherView", 10001}, {"actionCloneToOtherView", 10002},
        {"actionMoveToNewInstance", 10003}, {"actionOpenInNewInstance", 10004}, {"actionPasteHtml", 42038},
        {"actionPasteRtf", 42039}, {"actionCopyBinary", 42048}, {"actionCutBinary", 42049}, {"actionPasteBinary", 42050}, {"actionSwitchView", 44072},
        {"actionPreferences", 48011},    {"actionStyleConfigurator", 46001}, {"actionShortcutMapper", 48009},
        {"actionRecordMacro", 42018},    {"actionPlayMacro", 42021},      {"actionSaveMacro", 42025},
        // Fases 67 y 68 (Ver, Pestaña, Cerrar varios, Copiar al portapapeles, Herramientas, Ayuda)
        {"actionCloseAllButPinned", 41026}, {"actionCloseAllToLeft", 41009}, {"actionCloseAllToRight", 41018},
        {"actionCloseAllUnchanged", 41024}, {"actionAlwaysOnTop", 44034}, {"actionFullScreen", 44032},
        {"actionPostIt", 44009}, {"actionSyncScrollV", 44035}, {"actionSyncScrollH", 44036},
        {"actionTab1", 44086}, {"actionTab2", 44087}, {"actionTab3", 44088}, {"actionTab4", 44089},
        {"actionTab5", 44090}, {"actionTab6", 44091}, {"actionTab7", 44092}, {"actionTab8", 44093},
        {"actionTab9", 44094}, {"actionFirstTab", 44116}, {"actionLastTab", 44117}, {"actionTabToStart", 10005},
        {"actionTabToEnd", 10006}, {"actionTabForward", 44098}, {"actionTabBackward", 44099}, {"actionPinTab", 44048},
        {"actionTabColor1", 44111}, {"actionTabColor2", 44112}, {"actionTabColor3", 44113}, {"actionTabColor4", 44114},
        {"actionTabColor5", 44115}, {"actionTabColorNone", 44110}, {"actionSummary", 44049}, {"actionMonitoring", 44097},
        {"actionCopyFullPath", 42029}, {"actionCopyFileName", 42030}, {"actionCopyDirPath", 42031},
        {"actionCopyAllNames", 42087}, {"actionCopyAllPaths", 42088}, {"actionDebugInfo", 47012},
        {"actionMd5Text", 48501}, {"actionMd5Files", 48502}, {"actionMd5Selection", 48503},
        {"actionSha256Text", 48504}, {"actionSha256Files", 48505}, {"actionSha256Selection", 48506},
        {"actionSha1Text", 48507}, {"actionSha1Files", 48508}, {"actionSha1Selection", 48509},
        {"actionSha512Text", 48510}, {"actionSha512Files", 48511}, {"actionSha512Selection", 48512},
        // Fase 69 (Archivo, Editar › Sobre la selección, los paneles de Editar)
        {"actionOpenFolderInFileManager", 41019}, {"actionOpenFolderInTerminal", 41020},
        {"actionContainingFolderAsWorkspace", 41025}, {"actionOpenDefaultViewer", 41023}, {"actionRename", 41017},
        {"actionMoveToTrash", 41016}, {"actionOpenSelectedFile", 42073}, {"actionOpenSelectedFolder", 42074},
        {"actionSearchOnInternet", 42075}, {"actionChangeSearchEngine", 42076}, {"actionCharPanel", 42051},
        {"actionClipboardHistory", 42052},
        // Fase 71 (Configuración › Importar, el menú contextual, Macro)
        {"actionImportPlugins", 48005}, {"actionImportThemes", 48006}, {"actionEditContextMenu", 48018},
        {"actionEditMacros", 48016},
        // Fase 74
        {"actionBeginEndSelect", 42020}, {"actionBeginEndSelectColumn", 42089}, {"actionReadOnlyAll", 42102},
        {"actionReadOnlyClearAll", 42103}, {"actionShowNpc", 44130}, {"actionShowControlChars", 44131},
        {"actionFindCharsInRange", 43052}, {"actionCommandLineArguments", 47010}, {"actionHomePage", 47001},
        {"actionUserManual", 47003}, {"actionToggleMacroRecord", 50005}, {"actionFunctionHintPrevious", 50010},
        {"actionFunctionHintNext", 50011},
        {"actionRunMacro", 42032},       {"actionAbout", 47000},          {"actionCheckForUpdates", 47006},          {"actionEol0", 45001},
        {"actionEol1", 45003},           {"actionEol2", 45002},           {"actionDefineLanguage", 46250},
        {"actionPluginsAdmin", 48015},   {"actionColumnEditor", 42034},   {"actionDocumentMap", 44080},
        {"actionDocumentList", 44070},   {"actionFunctionList", 44084},   {"actionFolderAsWorkspace", 44085},
        {"actionOpenFolderAsWorkspace", 41022}, {"actionWindowsList", 11001},
        {"actionSortTabsNameAsc", 11002}, {"actionSortTabsNameDesc", 11003}, {"actionSortTabsPathAsc", 11004},
        {"actionSortTabsPathDesc", 11005}, {"actionSortTabsTypeAsc", 11006}, {"actionSortTabsTypeDesc", 11007},
        {"actionSortTabsSizeAsc", 11008}, {"actionSortTabsSizeDesc", 11009}, {"actionSortTabsModifiedAsc", 11010},
        {"actionSortTabsModifiedDesc", 11011}, {"actionLoadSession", 41012}, {"actionSaveSession", 41013},
    };
    for (const auto& [name, idm] : kCommands) {
        if (actionName == QLatin1String(name))
            return idm;
    }
    return 0;
}

// Órdenes de Scintilla del Shortcut Mapper (scintKeyDefs de N++)
struct ScintillaKeyDef
{
    const char* name;
    int command;
    bool ctrl;
    bool alt;
    bool shift;
    int key;
};

constexpr ScintillaKeyDef kScintillaKeys[] = {
#include "ui/ScintillaKeyTable.inc"
};

struct ScintillaCommand
{
    QString name;
    int command = 0;
    std::vector<KeyCombo> defaults;
};

const std::vector<ScintillaCommand>& scintillaCommands()
{
    static const std::vector<ScintillaCommand> commands = [] {
        std::vector<ScintillaCommand> list;
        for (const ScintillaKeyDef& def : kScintillaKeys) {
            if (def.name[0] || list.empty())
                list.push_back({QString::fromLatin1(def.name), def.command, {}});
            if (def.key)
                list.back().defaults.push_back(KeyCombo{def.ctrl, def.alt, def.shift, def.key});
        }
        return list;
    }();
    return commands;
}

QString plainMenuText(QString text)
{
    text.remove(QRegularExpression(QStringLiteral("&(?!&)")));
    text.replace(QStringLiteral("&&"), QStringLiteral("&"));
    if (text.endsWith(QStringLiteral("…")))
        text.chop(1);
    return text;
}

QString absolutePath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

// Órdenes de menú que se graban en las macros, con su IDM_* de N++ (menuCmdID.h)
// para que las macros de shortcuts.xml sean intercambiables
int nppCommandFor(const char* actionName)
{
    static constexpr std::pair<const char*, int> kCommands[] = {
        {"actionNew", 41001},         {"actionClose", 41003},      {"actionCloseAll", 41004},
        {"actionCloseAllBut", 41005}, {"actionSave", 41006},       {"actionSaveAll", 41007},
        {"actionSaveAs", 41008},      {"actionCut", 42001},        {"actionCopy", 42002},
        {"actionUndo", 42003},        {"actionRedo", 42004},       {"actionPaste", 42005},
        {"actionDelete", 42006},      {"actionSelectAll", 42007},  {"actionDuplicateLine", 42010},
        // Fase 68: insertar la fecha y la selección múltiple (IDM_EDIT_INSERT_DATETIME_*, IDM_EDIT_MULTISELECT*)
        {"actionDateTimeShort", 42084}, {"actionDateTimeLong", 42085}, {"actionDateTimeCustom", 42086},
        {"actionMultiSelectAll", 42090}, {"actionMultiSelectAllCase", 42091}, {"actionMultiSelectAllWord", 42092},
        {"actionMultiSelectAllCaseWord", 42093}, {"actionMultiSelectNext", 42094}, {"actionMultiSelectNextCase", 42095},
        {"actionMultiSelectNextWord", 42096}, {"actionMultiSelectNextCaseWord", 42097}, {"actionMultiSelectUndo", 42098},
        {"actionMultiSelectSkip", 42099},
        {"actionRedactSelection", 42106}, // IDM_EDIT_REDACT_SELECTION (fase 69)
        {"actionIndentIncrease", 42008}, {"actionIndentDecrease", 42009}, // IDM_EDIT_INS_TAB, RMV_TAB (fase 74)
        {"actionJoinLines", 42013},   {"actionLineUp", 42014},     {"actionLineDown", 42015},
        {"actionUpperCase", 42016},   {"actionLowerCase", 42017},  {"actionTrimTrailing", 42024},
        {"actionFindNext", 43002},    {"actionFindPrevious", 43010}, {"actionFindSelectedNext", 43014},
        {"actionFindSelectedPrevious", 43015}, {"actionNextTab", 44095}, {"actionPreviousTab", 44096},
        // Fase 12: líneas, mayúsculas, comentarios y espacios
        {"actionRemoveDuplicateLines", 42079}, {"actionRemoveConsecutiveDuplicates", 42077},
        {"actionSplitLines", 42012},  {"actionRemoveEmptyLines", 42055}, {"actionRemoveBlankLines", 42056},
        {"actionBlankLineAbove", 42057}, {"actionBlankLineBelow", 42058}, {"actionSortReverse", 42083},
        {"actionSortRandom", 42078},  {"actionSortLexAsc", 42059}, {"actionSortLexDesc", 42060},
        {"actionSortIntAsc", 42061},  {"actionSortIntDesc", 42062}, {"actionSortCommaAsc", 42063},
        {"actionSortCommaDesc", 42064}, {"actionSortDotAsc", 42065}, {"actionSortDotDesc", 42066},
        {"actionSortCaseInsAsc", 42080}, {"actionSortCaseInsDesc", 42081}, {"actionSortLocaleAsc", 42100},
        {"actionSortLocaleDesc", 42101}, {"actionSortLengthAsc", 42104}, {"actionSortLengthDesc", 42105},
        {"actionProperCase", 42067},  {"actionProperCaseBlend", 42068}, {"actionSentenceCase", 42069},
        {"actionSentenceCaseBlend", 42070}, {"actionInvertCase", 42071}, {"actionRandomCase", 42072},
        {"actionToggleComment", 42022}, {"actionLineComment", 42035}, {"actionLineUncomment", 42036},
        {"actionStreamComment", 42023}, {"actionStreamUncomment", 42047}, {"actionTrimLeading", 42042},
        {"actionTrimBoth", 42043},    {"actionEolToSpace", 42044}, {"actionTrimAll", 42045},
        {"actionTabToSpace", 42046},  {"actionSpaceToTabLeading", 42053}, {"actionSpaceToTabAll", 42054},
        // Buscar: llaves, estilos de token, marcadores e historial de cambios
        {"actionGoToMatchingBrace", 43009}, {"actionSelectMatchingBraces", 43053},
        {"actionCopyStyle1", 43055}, {"actionCopyStyle2", 43056}, {"actionCopyStyle3", 43057},
        {"actionCopyStyle4", 43058}, {"actionCopyStyle5", 43059}, {"actionCopyAllStyles", 43060},
        {"actionCopyFoundMark", 43061},
        {"actionStyleAll1", 43022},   {"actionStyleAll2", 43024}, {"actionStyleAll3", 43026},
        {"actionStyleAll4", 43028},   {"actionStyleAll5", 43030}, {"actionUnstyle1", 43023},
        {"actionUnstyle2", 43025},    {"actionUnstyle3", 43027}, {"actionUnstyle4", 43029},
        {"actionUnstyle5", 43031},    {"actionUnstyleAll", 43032}, {"actionPreviousStyle1", 43033},
        {"actionPreviousStyle2", 43034}, {"actionPreviousStyle3", 43035}, {"actionPreviousStyle4", 43036},
        {"actionPreviousStyle5", 43037}, {"actionPreviousFoundMark", 43038}, {"actionNextStyle1", 43039},
        {"actionNextStyle2", 43040},  {"actionNextStyle3", 43041}, {"actionNextStyle4", 43042},
        {"actionNextStyle5", 43043},  {"actionNextFoundMark", 43044}, {"actionStyleOne1", 43062},
        {"actionStyleOne2", 43063},   {"actionStyleOne3", 43064}, {"actionStyleOne4", 43065},
        {"actionStyleOne5", 43066},   {"actionToggleBookmark", 43005}, {"actionNextBookmark", 43006},
        {"actionPreviousBookmark", 43007}, {"actionClearBookmarks", 43008}, {"actionCutMarkedLines", 43018},
        {"actionCopyMarkedLines", 43019}, {"actionPasteMarkedLines", 43020}, {"actionDeleteMarkedLines", 43021},
        {"actionInverseBookmarks", 43050}, {"actionDeleteUnmarkedLines", 43051}, {"actionNextChange", 43067},
        {"actionPreviousChange", 43068}, {"actionClearChangeHistory", 43069},
        // Ver: plegado y zoom
        {"actionHideLines", 44042}, {"actionFoldAll", 44010},     {"actionUnfoldAll", 44029}, {"actionFoldCurrent", 44030},
        {"actionUnfoldCurrent", 44031}, {"actionZoomIn", 44023}, {"actionZoomOut", 44024},
        {"actionZoomRestore", 44033},
    };
    for (const auto& [name, idm] : kCommands) {
        if (std::strcmp(name, actionName) == 0)
            return idm;
    }
    // IDM_VIEW_FOLD_1…8 e IDM_VIEW_UNFOLD_1…8
    for (int level = 1; level <= 8; ++level) {
        if (QByteArray("actionFoldLevel") + QByteArray::number(level) == actionName)
            return 44050 + level;
        if (QByteArray("actionUnfoldLevel") + QByteArray::number(level) == actionName)
            return 44060 + level;
    }
    return 0;
}

} // namespace

QIcon MainWindow::applicationIcon()
{
    QIcon icon;
    for (int size : {16, 22, 24, 32, 48, 64, 128, 256, 512})
        icon.addFile(QStringLiteral(":/edita/icons/hicolor/%1x%1/apps/edita.png").arg(size), QSize(size, size));
    return icon;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , _syntax(paths::configDir())
    , _prompter(std::make_unique<DialogPrompter>())
    , _splitter(new QSplitter(Qt::Horizontal, this))
    , _statusBar(new DocStatusBar(this))
{
    i18n::ensureInstalled(); // main ya lo hizo; si no (pruebas, otros usos), inglés
    setWindowIcon(applicationIcon());
    // Sintaxis antes de crear documentos: decide su lenguaje y sus estilos
    const QStringList syntaxWarnings = _syntax.initialize();
    _documents.setUserLanguageDetector([this](const QString& path) { return _syntax.detectUserLanguage(path); });
    _documents.setExternalLanguageDetector([this](const QString& path) { return _syntax.detectExternalLanguage(path); });
    _documents.setLanguageDetector([this](const QString& path, std::string_view text) {
        return _syntax.detect(path, text);
    });

    // Servidores de lenguaje (antes de las vistas: las vistas les avisan de los cambios)
    _lsp = std::make_unique<LspManager>(_documents);
    connect(_lsp.get(), &LspManager::diagnosticsChanged, this, [this] { updateDiagnostics(); });
    connect(_lsp.get(), &LspManager::statusChanged, this, [this] {
        updateProblemsField();
        scheduleLspDecorations(0); // un servidor que acaba de arrancar: pistas y bombilla
        scheduleLspDecorations(1);
    });
    connect(_lsp.get(), &LspManager::applyEditRequested, this,
            [this](const QJsonObject& edit, lsp::PositionEncoding encoding, const std::function<void(bool)>& reply) {
                reply(applyWorkspaceEdit(edit, encoding) >= 0);
            });

    // Fragmentos de código (los de Edita y los del usuario)
    _snippets = new SnippetLibrary(this);

    // Las dos vistas de N++ (_mainEditView/_subEditView) en un divisor; la
    // secundaria se oculta mientras no tenga documentos
    _mainPane = new EditorPane(_documents, _splitter);
    _subPane = new EditorPane(_documents, _splitter);
    _mainPane->setObjectName(QStringLiteral("mainPane"));
    _subPane->setObjectName(QStringLiteral("subPane"));
    _splitter->addWidget(_mainPane);
    _splitter->addWidget(_subPane);
    _splitter->setChildrenCollapsible(false);
    _activePane = _mainPane;
    connectPane(_mainPane);
    connectPane(_subPane);

    // Encima del editor, la barra de avisos (sin ventanas modales)
    auto* central = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    _notifications = new NotificationBar(central);
    centralLayout->addWidget(_notifications);
    // El editor o, mientras no hay documentos, la página de inicio
    _centerStack = new QStackedWidget(central);
    _centerStack->addWidget(_splitter);
    centralLayout->addWidget(_centerStack, 1);
    setCentralWidget(central);
    setStatusBar(_statusBar);
    connect(_statusBar, &DocStatusBar::fieldClicked, this, [this](DocStatusBar::Field field) { showStatusPicker(static_cast<int>(field)); });
    setAcceptDrops(true);

    // Panel de resultados (Finder) abajo, oculto hasta la primera búsqueda
    _results = new SearchResultsPanel(this);
    addDockWidget(Qt::BottomDockWidgetArea, _results);
    _results->hide();
    _results->applyStyles(_syntax);
    connect(&_syntax, &SyntaxManager::stylesChanged, _results, [this] { _results->applyStyles(_syntax); });
    connect(_results, &SearchResultsPanel::resultActivated, this, &MainWindow::openSearchResult);

    // Vista invisible para buscar en documentos que no se muestran
    _searchView = std::make_unique<ScintillaEditBase>();
    _searchViewDefaultDoc = _searchView->send(SCI_GETDOCPOINTER);
    _searchView->send(SCI_ADDREFDOCUMENT, 0, _searchViewDefaultDoc);

    _prefs = loadPreferences(_syntax.config());
    _recentFiles = _syntax.config().recentFiles().files;
    _lastDirectory = _syntax.config().guiConfig(QStringLiteral("openSaveDir"), QStringLiteral("lastUsedDirPath"));
    // shortcuts.xml: en el primer arranque, con la macro que trae N++
    if (!QFileInfo::exists(shortcutsPath())) {
        _savedMacros = {defaultTrimAndSaveMacro()};
        writeMacros(shortcutsPath(), _savedMacros);
        // Y las órdenes de Ejecutar que trae N++
        const QString appImage = qEnvironmentVariable("APPIMAGE");
        _userCommands = defaultUserCommands(appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage);
        writeUserCommands(shortcutsPath(), _userCommands);
    } else if (!readMacros(shortcutsPath(), _savedMacros) || !readUserCommands(shortcutsPath(), _userCommands)) {
        QTimer::singleShot(0, this, [this] {
            _prompter->warn(this, tr("Macros"), tr("Could not read %1").arg(shortcutsPath()));
        });
    }
    createPanels();
    createMenus();
    resize(1000, 700);

    connect(&_documents, &DocumentManager::bufferChanged, this, [this](BufferId id, int mask) {
        if (id == currentBuffer())
            refreshCurrentDocument();
        if (mask & BufferChange::Language)
            notifyPlugins(NPPN_LANGCHANGED, id);
        if (mask & BufferChange::ReadOnly)
            notifyPlugins(NPPN_READONLYCHANGED, id);
        if (mask & BufferChange::Language) {
            for (EditorPane* pane : {_mainPane, _subPane}) {
                if (pane->currentBuffer() == id && autoCompletion(pane))
                    autoCompletion(pane)->setLanguage(_documents.buffer(id)->language());
            }
        }
        // Guardar con QSaveFile sustituye el archivo: hay que volver a vigilarlo
        if (mask & (BufferChange::Filename | BufferChange::Timestamp | BufferChange::Status))
            syncWatchedFiles();
        // Otro lenguaje u otro nombre: quizá otro servidor de lenguaje y otros fragmentos
        if (mask & (BufferChange::Language | BufferChange::Filename)) {
            attachToLsp(id);
            for (EditorPane* pane : {_mainPane, _subPane}) {
                if (pane->currentBuffer() == id)
                    updateSnippetContext(pane);
            }
        }
    });

    // Cambios externos: N++ comprueba al activar la ventana o una pestaña; además
    // se vigilan los archivos para enterarse mientras la ventana está activa
    _fileWatcher = new QFileSystemWatcher(this);
    connect(_fileWatcher, &QFileSystemWatcher::fileChanged, this, [this] { scheduleFileCheck(); });
    connect(&_documents, &DocumentManager::bufferAdded, this, [this](BufferId id) {
        scheduleWelcomeUpdate();
        syncWatchedFiles();
        // Rendimiento › Desactivar el ajuste de línea: al abrir un archivo grande, para todos
        if (const Buffer* buf = _documents.buffer(id); buf && buf->isLargeFile() && _prefs.largeFileNoWordWrap && _prefs.wrap)
            QTimer::singleShot(0, this, [this] { toggleViewOption(&Preferences::wrap, false); });
        // Un archivo abierto deja de estar en "recientes" (como N++)
        if (const Buffer* buf = _documents.buffer(id); buf && !buf->isUntitled()) {
            removeRecentFile(buf->filePath());
            notifyPlugins(NPPN_FILEOPENED, id);
            // Un archivo abierto: la página de inicio deja paso al editor, aunque se hubiera
            // abierto desde Ayuda o tras tocar «Mostrar al iniciar» (seguía encima)
            _welcomeForced = false;
            QTimer::singleShot(0, this, [this, id] { attachToLsp(id); }); // ya con su lenguaje
        }
    });
    connect(&_documents, &DocumentManager::bufferClosed, this, [this](BufferId id) {
        _indentOverrides.erase(id);
        _symbolCache.erase(id);
        _lsp->detach(id);
        if (_compare && (id == _compareBuffers[0] || id == _compareBuffers[1]))
            stopCompare();
        scheduleWelcomeUpdate();
        syncWatchedFiles();
        QTimer::singleShot(0, this, [this] { updatePanels(); });
    });
    connect(&_documents, &DocumentManager::bufferChanged, this, [this](BufferId id, int mask) {
        if (mask & (BufferChange::Dirty | BufferChange::Filename | BufferChange::ReadOnly | BufferChange::Language))
            updatePanels();
        if (mask & (BufferChange::Dirty | BufferChange::Filename))
            scheduleWelcomeUpdate();
        // Guardado con otro nombre o con otra codificación: otra versión de Git
        if (mask & (BufferChange::Filename | BufferChange::Unicode)) {
            for (EditorPane* pane : {_mainPane, _subPane}) {
                if (pane->currentBuffer() == id)
                    updateGitDocument(pane);
            }
        }
        // Otro lenguaje: la vista previa de colores y qué paréntesis cuentan
        if (mask & BufferChange::Language) {
            for (int i = 0; i < 2; ++i) {
                if ((i ? _subPane : _mainPane)->currentBuffer() == id)
                    updateLanguageDecorations(i);
            }
        }
    });

    // Modo "snapshot": copias periódicas de los cambios sin guardar
    _backupTimer = new QTimer(this);
    connect(_backupTimer, &QTimer::timeout, this, &MainWindow::backupNow);
    // "Seguir al sistema": el escritorio cambia entre claro y oscuro
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { applyAppearance(); });
    applyPreferences();

    // Notepad++ arranca siempre con un documento ("new 1")
    newDocument();
    updatePaneVisibility();
    editor()->setFocus();
    createWelcomePage();

    if (!syntaxWarnings.isEmpty())
        QTimer::singleShot(0, this, [this, syntaxWarnings] { showWarnings(tr("Settings"), syntaxWarnings); });
    // ¿Se cerró por un fallo la vez anterior?
    QTimer::singleShot(0, this, [this] { offerCrashReport(crash_report::defaultDirectory()); });
    // Versiones nuevas: unos segundos después de abrir, para no retrasar el arranque
    QTimer::singleShot(3000, this, [this] { checkForUpdatesIfDue(QDate::currentDate()); });
}

void MainWindow::connectPane(EditorPane* pane)
{
    // Git en el margen y marcas en la barra de desplazamiento (no existen en N++)
    const int index = pane == _subPane ? 1 : 0;
    _git[index] = new GitChanges(pane->editor());
    _blame[index] = new GitBlame(pane->editor());
    _snippetControllers[index] = new SnippetController(pane->editor(), *_snippets);
    // La bombilla de las acciones de código y las pistas, un momento después de moverse o editar
    _lspDecorationTimer[index] = new QTimer(this);
    _lspDecorationTimer[index]->setSingleShot(true);
    _lspDecorationTimer[index]->setInterval(400);
    connect(_lspDecorationTimer[index], &QTimer::timeout, this, [this, index] { updateLspDecorations(index); });
    connect(pane->editor(), &ScintillaEditBase::updateUi, this, [this, index](Scintilla::Update updated) {
        if (static_cast<int>(updated) & (SC_UPDATE_SELECTION | SC_UPDATE_V_SCROLL))
            scheduleLspDecorations(index);
    });
    connect(pane->editor(), &ScintillaEditBase::modified, this, [this, index](Scintilla::ModificationFlags type) {
        if (static_cast<int>(type) & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
            scheduleLspDecorations(index);
    });
    _scrollMarks[index] = new ScrollBarMarks(pane->editor(), _git[index]);
    _colorPreview[index] = new ColorPreview(pane->editor());
    _rainbow[index] = new RainbowBrackets(pane->editor());
    // Ruta de navegación entre las pestañas y el editor, y encabezado fijo en el editor
    _breadcrumbs[index] = new Breadcrumbs(pane);
    if (auto* paneLayout = qobject_cast<QVBoxLayout*>(pane->layout()))
        paneLayout->insertWidget(1, _breadcrumbs[index]);
    _sticky[index] = new StickyScroll(pane->editor());
    _findBar[index] = new FindBar(pane->editor(), pane);
    // Desplazamiento sincronizado de las dos vistas (IDM_VIEW_SYNSCROLLV/H)
    connect(pane->editor(), &ScintillaEditBase::verticalScrolled, this, [this, pane] { syncScroll(pane->editor()); });
    connect(pane->editor(), &ScintillaEditBase::horizontalScrolled, this, [this, pane] { syncScroll(pane->editor()); });
    // El alfiler de una pestaña fijada
    connect(pane, &EditorPane::unpinRequested, this, [this](BufferId id) {
        if (Buffer* buf = _documents.buffer(id))
            buf->setPinned(false);
    });
    connect(_findBar[index], &FindBar::searched, this, [this](const SearchOptions& options) {
        _lastOptions = options;
        _findBarUsedLast = true;
    });
    connect(_findBar[index], &FindBar::moreOptionsRequested, this, [this](bool replace, const QString& text) {
        // El diálogo completo de N++, con el texto y las opciones de la barra
        FindReplaceDialog* dialog = findDialog();
        if (FindBar* bar = findBarOfCurrentView())
            dialog->setOptions(bar->options());
        dialog->showTab(replace ? FindReplaceDialog::Tab::Replace : FindReplaceDialog::Tab::Find, text);
        _findBarUsedLast = false;
    });
    connect(_breadcrumbs[index], &Breadcrumbs::openFileRequested, this, [this](const QString& path) {
        openFile(path);
        editor()->setFocus();
    });
    connect(_breadcrumbs[index], &Breadcrumbs::goToLineRequested, this, [this, pane](int line) {
        setActivePane(pane);
        goToLine(line + 1);
        pane->editor()->setFocus();
    });
    _symbolTimer[index] = new QTimer(this);
    _symbolTimer[index]->setSingleShot(true);
    _symbolTimer[index]->setInterval(500);
    connect(_symbolTimer[index], &QTimer::timeout, this, [this, index] { updateBreadcrumbs(index, true); });
    connect(pane->editor(), &ScintillaEditBase::modified, _symbolTimer[index],
            [this, index, pane](Scintilla::ModificationFlags type) {
                // En un documento grande, analizarlo en cada pausa al escribir bloquearía la
                // ventana (segundos con 14 MB): se rehace al volver a su pestaña
                if ((static_cast<int>(type) & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
                    && pane->editor()->sci(SCI_GETLENGTH) <= kMaxSymbolsWhileTyping)
                    _symbolTimer[index]->start();
            });
    connect(pane->editor(), &ScintillaEditBase::updateUi, _breadcrumbs[index], [this, index, pane](Scintilla::Update updated) {
        if ((static_cast<int>(updated) & SC_UPDATE_SELECTION) && _breadcrumbs[index])
            _breadcrumbs[index]->setCaretLine(static_cast<int>(
                pane->editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pane->editor()->sci(SCI_GETCURRENTPOS)))));
    });
    pane->setStyler([this](EditorView& view, const Buffer& buffer) { _syntax.apply(view, buffer.language(), buffer.userLanguage()); });
    connect(&_syntax, &SyntaxManager::stylesChanged, pane, &EditorPane::restyle);
    // Después de restyle: los colores de las marcas según el tema ya aplicado
    connect(&_syntax, &SyntaxManager::stylesChanged, pane, [this, pane] { applyIndentOverride(pane); });
    connect(&_syntax, &SyntaxManager::stylesChanged, _git[index], [this, index] {
        if (_git[index])
            _git[index]->applyColors();
        if (_scrollMarks[index])
            _scrollMarks[index]->scheduleUpdate();
    });
    connect(pane, &EditorPane::currentBufferChanged, this, [this, pane, index] {
        // Otra pestaña en una de las vistas que se comparan: se acabó la comparación
        if (_compare && pane->currentBuffer() != _compareBuffers[0] && pane->currentBuffer() != _compareBuffers[1])
            stopCompare();
        applyIndentOverride(pane);
        applyDocumentOptions(pane);
        _lsp->paintDiagnostics(*pane->editor(), pane->currentBuffer());
        updateProblemsField();
        updateSnippetContext(pane);
        _lightbulbLine[index] = -1;
        scheduleLspDecorations(index);
        scheduleWelcomeUpdate();
        updateGitDocument(pane);
        updateLanguageDecorations(index);
        updateBreadcrumbs(index, true);
        if (_findBar[index])
            _findBar[index]->refresh();
        if (pane == _activePane)
            refreshCurrentDocument();
        else
            updateFindDialogHighlight();
        if (const Buffer* buf = _documents.buffer(pane->currentBuffer()); buf && autoCompletion(pane))
            autoCompletion(pane)->setLanguage(buf->language());
        if (pane == _activePane)
            notifyPlugins(NPPN_BUFFERACTIVATED, pane->currentBuffer());
        // Notepad_plus::notifyBufferActivated: con detección "solo el documento
        // actual", se comprueba al activarlo (fuera de la señal, por si pregunta)
        if (_prefs.detectsChanges() && !_prefs.detectsAllFiles())
            scheduleFileCheck();
    });
    connect(pane, &EditorPane::closeRequested, this, [this, pane](BufferId id) {
        setActivePane(pane);
        closeBuffer(id);
    });
    connect(pane, &EditorPane::newDocumentRequested, this, [this, pane] {
        setActivePane(pane);
        newDocument();
    });
    connect(pane, &EditorPane::tabContextMenuRequested, this, [this, pane](BufferId id, const QPoint& pos) {
        setActivePane(pane);
        showTabContextMenu(id, pos);
    });
    EditorView* view = pane->editor();
    // El menú del clic derecho, de contextMenu.xml (como N++)
    view->setContextMenuHandler([this, pane](const QPoint& globalPos) {
        setActivePane(pane);
        const std::unique_ptr<QMenu> menu(buildEditorContextMenu());
        menu->exec(globalPos);
    });
    // La vista que recibe el foco pasa a ser la activa (como en N++)
    connect(view, &ScintillaEditBase::focusChanged, this, [this, pane](bool focused) {
        if (focused)
            setActivePane(pane);
    });
    // Los enlaces de lo que se ve, en las dos vistas (addHotSpot al desplazarse o al cambiar)
    connect(view, &ScintillaEditBase::updateUi, this, [this, pane](Scintilla::Update updated) {
        if (static_cast<int>(updated) & (SC_UPDATE_CONTENT | SC_UPDATE_V_SCROLL)) {
            refreshUrls(pane);
            scheduleSpelling(pane);
        }
    });
    connect(view, &ScintillaEditBase::updateUi, this, [this, pane](Scintilla::Update updated) {
        if (pane != _activePane)
            return;
        _statusBar->updateCaret(pane->editor());
        // XmlMatchedTagsHighlighter (tras braceMatch, como en NppNotification.cpp)
        if (static_cast<int>(updated) & (SC_UPDATE_SELECTION | SC_UPDATE_CONTENT)) {
            if (const Buffer* buf = _documents.buffer(pane->currentBuffer())) {
                XmlTagMatchOptions options;
                options.highlightAttributes = _prefs.tagAttributesHighlight;
                options.caseSensitive = buf->language() == LangType::Xml
                    || (buf->language() == LangType::Html && buf->filePath().endsWith(QLatin1String(".xhtml"), Qt::CaseInsensitive));
                options.indentGuides = _prefs.indentGuideLine;
                matchXmlTags(*pane->editor(), _prefs.tagsMatchHighlight ? buf->language() : LangType::Text, options);
            }
        }
        // SmartHighlighter: al cambiar la selección o desplazarse (solo se marcan las líneas visibles)
        if (static_cast<int>(updated) & (SC_UPDATE_SELECTION | SC_UPDATE_CONTENT | SC_UPDATE_V_SCROLL)) {
            SmartHighlightOptions options;
            options.enabled = _prefs.smartHighlight && (!isLargeFileIn(pane) || _prefs.largeFileSmartHighlight);
            options.otherView = _prefs.smartHighlightOtherView;
            if (_prefs.smartHighlightUseFindSettings) {
                const SearchOptions find = _findDialog ? _findDialog->options() : _lastOptions;
                options.matchCase = find.matchCase;
                options.wholeWord = find.wholeWord;
            } else {
                options.matchCase = _prefs.smartHighlightMatchCase;
                options.wholeWord = _prefs.smartHighlightWholeWord;
            }
            smartHighlight(*pane->editor(), otherPane(pane)->editor(), options);
        }
        // Lo que coincide con el diálogo de búsqueda sigue a las líneas que se ven
        if ((static_cast<int>(updated) & SC_UPDATE_V_SCROLL) && _findDialog && _findDialog->highlightMatchesActive())
            updateFindDialogHighlight();
    });
    (pane == _mainPane ? _mainAuto : _subAuto) = std::make_unique<AutoCompletion>(*view, _prefs);
    connect(view, &ScintillaEditBase::charAdded, this, [this, pane](int ch) {
        // SCN_CHARADDED: sangría, cierre de pares y después autocompletado. Lo que
        // hacen no se graba en la macro: al reproducirla se vuelve a hacer
        const QScopedValueRollback suppress(_suppressRecording, _suppressRecording + 1);
        if (const Buffer* buf = _documents.buffer(pane->currentBuffer()))
            maintainIndentation(*pane->editor(), buf->language(), _prefs.autoIndent, ch);
        AutoCompletion* ac = autoCompletion(pane);
        if (pane->editor()->sci(SCI_GETSELECTIONS) == 1)
            ac->insertMatchedChars(ch);
        // Con servidor de lenguaje, su lista; si no, la de N++. En un archivo grande, solo si
        // Rendimiento lo permite
        const bool autocAllowed = !isLargeFileIn(pane) || _prefs.largeFileAutoCompletion;
        if (!_playingMacro && autocAllowed && !_lsp->complete(*pane->editor(), pane->currentBuffer(), ch))
            ac->update(ch);
        if (!_playingMacro)
            _lsp->signatureHelp(*pane->editor(), pane->currentBuffer(), ch);
    });
    // El texto que ve el servidor de lenguaje, al día; y lo que dice al dejar el ratón
    connect(view, &ScintillaEditBase::modified, this, [this, pane](Scintilla::ModificationFlags type) {
        if (static_cast<int>(type) & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) {
            _lsp->scheduleSync();
            // Y lo que coincide con el diálogo de búsqueda, un momento después
            if (pane == _activePane && _findDialog && _findDialog->highlightMatchesActive())
                _findDialog->scheduleMatchHighlight();
        }
    });
    view->send(SCI_SETMOUSEDWELLTIME, 500);
    connect(view, &ScintillaEditBase::notify, this, [this, pane](Scintilla::NotificationData* n) {
        switch (static_cast<int>(n->nmhdr.code)) {
        case SCN_CALLTIPCLICK:
            autoCompletion(pane)->callTipClick(static_cast<int>(n->position));
            break;
        case SCN_DOUBLECLICK:
            // Ctrl + doble clic: entre los delimitadores; sin teclas, sobre un enlace: abrirlo
            if (pane != _activePane)
                setActivePane(pane);
            if (static_cast<int>(n->modifiers) == SCMOD_CTRL)
                selectDelimitedAt(n->position >= 0 ? n->position : pane->editor()->sci(SCI_GETCURRENTPOS));
            else if (static_cast<int>(n->modifiers) == SCMOD_NORM && n->position >= 0)
                openLinkAt(n->position);
            break;
        case SCN_AUTOCSELECTION:
            if (n->text)
                _lsp->completionSelected(*pane->editor(), QString::fromUtf8(n->text));
            break;
        case SCN_AUTOCCANCELLED:
        case SCN_AUTOCCOMPLETED:
            _lsp->completionClosed(*pane->editor());
            break;
        case SCN_DWELLSTART:
            if (n->position >= 0) {
                const QPoint global = pane->editor()->viewport()->mapToGlobal(QPoint(static_cast<int>(n->x), static_cast<int>(n->y)));
                _lsp->hover(*pane->editor(), pane->currentBuffer(), n->position, global);
            }
            break;
        case SCN_DWELLEND:
            _lsp->hideHover();
            break;
        default:
            break;
        }
        // Todas las notificaciones de Scintilla llegan también a los plugins
        if (_plugins && !_plugins->plugins().empty()) {
            SCNotification copy = *reinterpret_cast<SCNotification*>(n);
            copy.nmhdr.hwndFrom = pane->editor();
            _plugins->notify(copy);
        }
    });
    connect(view, &ScintillaEditBase::macroRecord, this,
            [this](Scintilla::Message message, Scintilla::uptr_t wParam, Scintilla::sptr_t lParam) {
        // SCN_MACRORECORD (lo que hacen las órdenes de menú ya está grabado como orden)
        if (!_recordingMacro || _suppressRecording != 0)
            return;
        MacroStep step = recordedStep(static_cast<int>(message), wParam, lParam);
        // Intro llega como texto ("\r" en ScintillaQt): se graba como SCI_NEWLINE, que
        // al reproducir usa el fin de línea del documento (como getActions de N++)
        if (step.message == SCI_REPLACESEL && (step.sParam == "\r" || step.sParam == "\n" || step.sParam == "\r\n")) {
            const bool secondHalfOfCrLf = step.sParam == "\n" && _lastStepWasCr;
            _lastStepWasCr = step.sParam == "\r";
            if (secondHalfOfCrLf)
                return;
            step = recordedStep(SCI_NEWLINE, 0, 0);
        } else {
            _lastStepWasCr = false;
        }
        _macro.push_back(step);
    });
    // Clic en el margen de símbolos: pone o quita un marcador (ScintillaEditView::marginClick)
    connect(view, &ScintillaEditBase::marginClicked, this, [this, pane](Scintilla::Position position, Scintilla::KeyMod, int margin) {
        if (margin != static_cast<int>(kMarginSymbol))
            return;
        setActivePane(pane);
        EditorView* v = pane->editor();
        // En la línea de la bombilla: las acciones de código, no un marcador
        const sptr_t clicked = v->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(position));
        // En una marca de líneas ocultas: se muestran (hidelineMarkerClicked)
        if (showHiddenSection(*v, clicked))
            return;
        if (v->sci(SCI_MARKERGET, static_cast<uptr_t>(clicked)) & (1 << kLightbulbMarker)) {
            v->sci(SCI_GOTOLINE, static_cast<uptr_t>(clicked));
            showCodeActions();
            return;
        }
        toggleBookmark(*v, v->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(position)));
    });
    connect(view, &ScintillaEditBase::uriDropped, this, [this, pane](const QString& uri) {
        const QUrl url(uri);
        if (!url.isLocalFile())
            return;
        setActivePane(pane);
        openFile(url.toLocalFile());
    });
}

void MainWindow::showWarnings(const QString& title, const QStringList& warnings)
{
    // Avisos que no piden nada: en la barra, sin interrumpir
    if (!warnings.isEmpty())
        _notifications->show(NotificationBar::Kind::Warning, title, warnings.join(QLatin1Char('\n')));
}

MainWindow::~MainWindow()
{
    // Al cerrar, QWidget oculta los paneles cuando los miembros de esta clase ya no
    // existen: que no avisen
    for (const QPointer<QDockWidget>& dock : _panelDocks) {
        if (dock)
            dock->disconnect(this);
    }
    if (_fileJob) {
        _fileJob->cancel();
        _fileJob->wait();
    }
    stopPaletteFileScan();
    _lsp->shutdownAll(); // shutdown y exit a cada servidor
    // Las vistas sueltan sus documentos antes de que se destruya el DocumentManager
    _searchView->send(SCI_RELEASEDOCUMENT, 0, _searchViewDefaultDoc);
    _searchView.reset();
    // Los paneles de los plugins son código de los plugins: se destruyen antes de descargarlos
    const std::vector<PluginDock> docks = std::move(_pluginDocks);
    _pluginDocks.clear();
    for (const PluginDock& dock : docks)
        delete dock.dock;
    _plugins.reset();
    _mainAuto.reset();
    _subAuto.reset();
    delete _splitter;
    _splitter = nullptr;
}

void MainWindow::setPrompter(std::unique_ptr<Prompter> prompter)
{
    _prompter = std::move(prompter);
}

EditorView* MainWindow::editor() const
{
    return _activePane->editor();
}

BufferId MainWindow::currentBuffer() const
{
    return _activePane->currentBuffer();
}

EditorPane* MainWindow::otherPane(const EditorPane* pane) const
{
    return pane == _mainPane ? _subPane : _mainPane;
}

EditorPane* MainWindow::paneContaining(BufferId id) const
{
    if (_activePane->contains(id))
        return _activePane;
    EditorPane* other = otherPane(_activePane);
    return other->contains(id) ? other : nullptr;
}

std::vector<BufferId> MainWindow::allBuffers() const
{
    std::vector<BufferId> ids = _mainPane->buffers();
    for (const BufferId id : _subPane->buffers()) {
        if (std::find(ids.begin(), ids.end(), id) == ids.end())
            ids.push_back(id);
    }
    return ids;
}

void MainWindow::setActivePane(EditorPane* pane)
{
    if (!pane || pane == _activePane || pane->count() == 0)
        return;
    _activePane = pane;
    refreshCurrentDocument();
}

void MainWindow::updatePaneVisibility()
{
    // showView/hideView: la secundaria solo con documentos; la principal se
    // oculta si se queda vacía mientras la secundaria tiene
    _subPane->setVisible(_subPane->count() > 0);
    _mainPane->setVisible(_mainPane->count() > 0 || _subPane->count() == 0);
    if (_activePane->count() == 0 && otherPane(_activePane)->count() > 0) {
        _activePane = otherPane(_activePane);
        refreshCurrentDocument();
    }
}

// --- Menús ----------------------------------------------------------------------

void MainWindow::createMenus()
{
    auto addAction = [this](QMenu* menu, const QString& text, const char* name, const QKeySequence& shortcut,
                            auto&& slot) {
        QAction* action = menu->addAction(text);
        action->setObjectName(QString::fromLatin1(name));
        if (!shortcut.isEmpty())
            action->setShortcut(shortcut);
        if constexpr (std::is_invocable_v<decltype(slot)>) {
            if (const int idm = nppCommandFor(name)) {
                // Grabable en macros
                action->setProperty("nppCommand", idm);
                connect(action, &QAction::triggered, this, [this, idm, slot] { runMenuCommand(idm, slot); });
                return action;
            }
        }
        connect(action, &QAction::triggered, this, slot);
        return action;
    };
    auto sci = [this](unsigned int message) { return [this, message] { editor()->sci(message); }; };

    // Archivo
    QMenu* file = menuBar()->addMenu(tr("&File"));
    addAction(file, tr("&New"), "actionNew", QKeySequence::New, [this] { newDocument(); });
    addAction(file, tr("&Open…"), "actionOpen", QKeySequence::Open, [this] { openWithDialog(); });
    addAction(file, tr("Open Folder as Wor&kspace…"), "actionOpenFolderAsWorkspace", QKeySequence(), [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Folder as Workspace"));
        if (!dir.isEmpty())
            addFolderToWorkspace(dir);
    });
    // IDM_FILE_OPEN_FOLDER, IDM_FILE_OPEN_CMD, IDM_FILE_CONTAININGFOLDERASWORKSPACE: la carpeta del
    // documento en el administrador de archivos, en una terminal o en «Carpeta como espacio de trabajo»
    QMenu* containing = file->addMenu(tr("Open Containing &Folder"));
    containing->setObjectName(QStringLiteral("menuOpenContainingFolder"));
    auto currentDir = [this] {
        const Buffer* buf = _documents.buffer(currentBuffer());
        return buf && !buf->isUntitled() ? QFileInfo(buf->filePath()).absolutePath() : QString();
    };
    addAction(containing, tr("File &Manager"), "actionOpenFolderInFileManager", QKeySequence(), [this, currentDir] {
        if (const QString dir = currentDir(); !dir.isEmpty())
            _openUrl(QUrl::fromLocalFile(dir));
    });
    addAction(containing, tr("&Terminal"), "actionOpenFolderInTerminal", QKeySequence(), [this, currentDir] {
        if (const QString dir = currentDir(); !dir.isEmpty())
            openTerminal(dir);
    });
    containing->addSeparator();
    addAction(containing, tr("Folder as &Workspace"), "actionContainingFolderAsWorkspace", QKeySequence(), [this, currentDir] {
        if (const QString dir = currentDir(); !dir.isEmpty())
            addFolderToWorkspace(dir);
    });
    addAction(file, tr("Open in &Default Viewer"), "actionOpenDefaultViewer", QKeySequence(),
              [this] { openInDefaultViewer(currentBuffer()); });
    addAction(file, tr("Re&load from Disk"), "actionReload", QKeySequence(), [this] { reloadBuffer(currentBuffer()); });
    file->addSeparator();
    addAction(file, tr("&Save"), "actionSave", QKeySequence::Save, [this] { saveBuffer(currentBuffer()); });
    addAction(file, tr("Save &As…"), "actionSaveAs", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S),
              [this] { saveBufferAs(currentBuffer()); });
    addAction(file, tr("Save a Cop&y As…"), "actionSaveCopy", QKeySequence(),
              [this] { saveBufferAs(currentBuffer(), true); });
    addAction(file, tr("Sa&ve All"), "actionSaveAll", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S),
              [this] { saveAll(); });
    addAction(file, tr("Re&name…"), "actionRename", QKeySequence(), [this] { renameBuffer(currentBuffer()); });
    file->addSeparator();
    addAction(file, tr("&Close"), "actionClose", QKeySequence(Qt::CTRL | Qt::Key_W), [this] { closeBuffer(currentBuffer()); });
    addAction(file, tr("Clos&e All"), "actionCloseAll", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W), [this] { closeAll(); });
    // IDM_FILE_CLOSEALL_*: el submenú «Cerrar varios documentos» de N++
    QMenu* closeMultiple = file->addMenu(tr("Close &Multiple Documents"));
    closeMultiple->setObjectName(QStringLiteral("menuCloseMultiple"));
    addAction(closeMultiple, tr("Close All but Acti&ve Document"), "actionCloseAllBut", QKeySequence(),
              [this] { closeAllBut(currentBuffer()); });
    addAction(closeMultiple, tr("Close All but &Pinned Documents"), "actionCloseAllButPinned", QKeySequence(),
              [this] { closeAllButPinned(); });
    addAction(closeMultiple, tr("Close All to the &Left"), "actionCloseAllToLeft", QKeySequence(),
              [this] { closeAllToLeft(currentBuffer()); });
    addAction(closeMultiple, tr("Close All to the &Right"), "actionCloseAllToRight", QKeySequence(),
              [this] { closeAllToRight(currentBuffer()); });
    addAction(closeMultiple, tr("Close All &Unchanged"), "actionCloseAllUnchanged", QKeySequence(),
              [this] { closeAllUnchanged(); });
    addAction(file, tr("Move to &Trash"), "actionMoveToTrash", QKeySequence(), [this] { moveBufferToTrash(currentBuffer()); });
    file->addSeparator();
    // IDM_FILE_LOADSESSION, IDM_FILE_SAVESESSION
    auto sessionFilters = [this] {
        const QString all = tr("All types (*)");
        return _prefs.sessionExt.isEmpty() ? all : tr("Session file (*.%1)").arg(_prefs.sessionExt) + QStringLiteral(";;") + all;
    };
    addAction(file, tr("Load Sess&ion…"), "actionLoadSession", QKeySequence(), [this, sessionFilters] {
        const QStringList names = _prompter->openFileNames(this, suggestedDirectory(), sessionFilters());
        if (!names.isEmpty())
            loadSessionFile(names.front());
    });
    addAction(file, tr("Save Sess&ion…"), "actionSaveSession", QKeySequence(), [this, sessionFilters] {
        const QString suggested = QDir(suggestedDirectory()).filePath(
            _prefs.sessionExt.isEmpty() ? QStringLiteral("session.xml") : QStringLiteral("session.") + _prefs.sessionExt);
        QString name = _prompter->saveFileName(this, suggested, sessionFilters(), QString());
        if (name.isEmpty())
            return;
        if (!_prefs.sessionExt.isEmpty() && QFileInfo(name).suffix().isEmpty())
            name += QLatin1Char('.') + _prefs.sessionExt;
        // Las carpetas del explorador van si el panel está a la vista (la casilla de N++
        // «Guardar Carpeta como espacio de trabajo» está marcada en ese caso)
        QDockWidget* folders = panelDock(QStringLiteral("folderAsWorkspace"));
        saveSessionFile(name, folders && folders->isVisible());
    });
    file->addSeparator();
    addAction(file, tr("&Print…"), "actionPrint", QKeySequence::Print, [this] { printWithDialog(); });
    addAction(file, tr("Print No&w"), "actionPrintNow", QKeySequence(), [this] { printNow(); });
    addAction(file, tr("E&xport to PDF…"), "actionExportPdf", QKeySequence(), [this] { exportToPdf(); });
    file->addSeparator();
    addAction(file, tr("&Restore Recent Closed File"), "actionReopenClosed",
              QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T), [this] { reopenLastClosed(); });
    // Aquí van los archivos recientes (rebuildRecentFilesMenu)
    _fileMenu = file;
    _recentAnchor = file->addSeparator();
    addAction(file, tr("E&xit"), "actionQuit", QKeySequence(Qt::ALT | Qt::Key_F4), [this] { close(); });

    // Editar
    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    addAction(edit, tr("&Undo"), "actionUndo", QKeySequence::Undo, sci(SCI_UNDO));
    addAction(edit, tr("&Redo"), "actionRedo", QKeySequence(Qt::CTRL | Qt::Key_Y), sci(SCI_REDO));
    edit->addSeparator();
    addAction(edit, tr("Cu&t"), "actionCut", QKeySequence::Cut, [this] { copyOrCut(true); });
    addAction(edit, tr("&Copy"), "actionCopy", QKeySequence::Copy, [this] { copyOrCut(false); });
    addAction(edit, tr("&Paste"), "actionPaste", QKeySequence::Paste, sci(SCI_PASTE));
    // Editar › Pegado especial (IDM_EDIT_PASTE_AS_HTML/RTF, IDM_EDIT_COPY/CUT/PASTE_BINARY)
    QMenu* pasteSpecial = edit->addMenu(tr("Paste &Special"));
    QAction* pasteHtml = addAction(pasteSpecial, tr("Paste &HTML Content"), "actionPasteHtml", QKeySequence(),
                                   [this] { pasteClipboardMarkup(true); });
    QAction* pasteRtf = addAction(pasteSpecial, tr("Paste &RTF Content"), "actionPasteRtf", QKeySequence(),
                                  [this] { pasteClipboardMarkup(false); });
    pasteSpecial->addSeparator();
    addAction(pasteSpecial, tr("&Copy Binary Content"), "actionCopyBinary", QKeySequence(), [this] { copyBinary(false); });
    addAction(pasteSpecial, tr("Cu&t Binary Content"), "actionCutBinary", QKeySequence(), [this] { copyBinary(true); });
    addAction(pasteSpecial, tr("&Paste Binary Content"), "actionPasteBinary", QKeySequence(), [this] { pasteBinary(); });
    // Solo con HTML o RTF en el portapapeles
    auto refreshPasteSpecial = [pasteHtml, pasteRtf] {
        const QMimeData* clip = QApplication::clipboard()->mimeData();
        pasteHtml->setEnabled(clip && clip->hasHtml());
        pasteRtf->setEnabled(clip && (clip->hasFormat(QStringLiteral("text/rtf")) || clip->hasFormat(QStringLiteral("application/rtf"))
                                      || clip->hasFormat(QStringLiteral("text/richtext"))));
    };
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, refreshPasteSpecial);
    refreshPasteSpecial();
    addAction(edit, tr("&Delete"), "actionDelete", QKeySequence(), sci(SCI_CLEAR));
    addAction(edit, tr("Select A&ll"), "actionSelectAll", QKeySequence::SelectAll, sci(SCI_SELECTALL));
    // IDM_EDIT_BEGINENDSELECT[_COLUMNMODE]: marcada mientras se espera el final; la otra, sin poder usarse
    for (const bool column : {false, true}) {
        QAction* select = addAction(edit, column ? tr("Begin/End Select in Column &Mode") : tr("&Begin/End Select"),
                                    column ? "actionBeginEndSelectColumn" : "actionBeginEndSelect", QKeySequence(), [this, column] {
                                        editor()->beginOrEndSelect(column);
                                        refreshBeginEndSelect(column);
                                    });
        select->setCheckable(true);
    }
    edit->addSeparator();
    // Editar › Insertar (IDM_EDIT_INSERT_DATETIME_*)
    QMenu* insert = edit->addMenu(tr("I&nsert"));
    addAction(insert, tr("Date Time (&short)"), "actionDateTimeShort", QKeySequence(), [this] {
        insertText(localDateTime(QDateTime::currentDateTime(), false, _prefs.dateTimeReverseOrder));
    });
    addAction(insert, tr("Date Time (&long)"), "actionDateTimeLong", QKeySequence(), [this] {
        insertText(localDateTime(QDateTime::currentDateTime(), true, _prefs.dateTimeReverseOrder));
    });
    addAction(insert, tr("Date Time (&customized)"), "actionDateTimeCustom", QKeySequence(), [this] {
        insertText(formatDateTime(_prefs.dateTimeFormat, QDateTime::currentDateTime()));
    });
    // Editar › Copiar al portapapeles (IDM_EDIT_*TOCLIP, IDM_EDIT_COPY_ALL_*)
    QMenu* copyTo = edit->addMenu(tr("Cop&y to Clipboard"));
    auto copyText = [](const QString& text) { QApplication::clipboard()->setText(text); };
    addAction(copyTo, tr("Copy Current Full File &Path"), "actionCopyFullPath", QKeySequence(), [this, copyText] {
        if (const Buffer* buf = _documents.buffer(currentBuffer()))
            copyText(buf->isUntitled() ? buf->displayName() : buf->filePath());
    });
    addAction(copyTo, tr("Copy Current &Filename"), "actionCopyFileName", QKeySequence(), [this, copyText] {
        if (const Buffer* buf = _documents.buffer(currentBuffer()))
            copyText(buf->displayName());
    });
    addAction(copyTo, tr("Copy Current &Dir. Path"), "actionCopyDirPath", QKeySequence(), [this, copyText] {
        if (const Buffer* buf = _documents.buffer(currentBuffer()); buf && !buf->isUntitled())
            copyText(QFileInfo(buf->filePath()).absolutePath());
    });
    copyTo->addSeparator();
    addAction(copyTo, tr("Copy All File&names"), "actionCopyAllNames", QKeySequence(), [this, copyText] {
        copyText(openDocumentNames(false).join(QLatin1Char('\n')));
    });
    addAction(copyTo, tr("Copy All File P&aths"), "actionCopyAllPaths", QKeySequence(), [this, copyText] {
        copyText(openDocumentNames(true).join(QLatin1Char('\n')));
    });
    edit->addSeparator();
    // Editar › Sangría (IDM_EDIT_INS_TAB, IDM_EDIT_RMV_TAB): con varias líneas o selecciones, como
    // Tab y Mayús+Tab; con una, un ancho de tabulación más o menos (como N++)
    QMenu* indent = edit->addMenu(tr("&Indent"));
    for (const bool forward : {true, false}) {
        addAction(indent, forward ? tr("&Increase Line Indent") : tr("&Decrease Line Indent"),
                  forward ? "actionIndentIncrease" : "actionIndentDecrease", QKeySequence(), [this, forward] {
                      EditorView* v = editor();
                      const sptr_t start = v->sci(SCI_GETSELECTIONSTART);
                      const sptr_t end = v->sci(SCI_GETSELECTIONEND);
                      const sptr_t line = v->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(start));
                      if (v->sci(SCI_GETSELECTIONS) > 1 || line != v->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(end))) {
                          v->sci(forward ? SCI_TAB : SCI_BACKTAB);
                          return;
                      }
                      const sptr_t width = v->sci(SCI_GETTABWIDTH);
                      const sptr_t current = v->sci(SCI_GETLINEINDENTATION, static_cast<uptr_t>(line));
                      setLineIndent(*v, line, std::max<sptr_t>(0, current + (forward ? width : -width)));
                  });
    }
    // Operaciones con líneas (Editar > Line Operations de N++)
    QMenu* lines = edit->addMenu(tr("&Line Operations"));
    addAction(lines, tr("&Duplicate Current Line"), "actionDuplicateLine", QKeySequence(Qt::CTRL | Qt::Key_D), sci(SCI_LINEDUPLICATE));
    addAction(lines, tr("Remove Duplicate &Lines"), "actionRemoveDuplicateLines", QKeySequence(),
              [this] { removeDuplicateLines(*editor(), false); });
    addAction(lines, tr("Remove &Consecutive Duplicate Lines"), "actionRemoveConsecutiveDuplicates", QKeySequence(),
              [this] { removeDuplicateLines(*editor(), true); });
    addAction(lines, tr("Spl&it Lines"), "actionSplitLines", QKeySequence(Qt::CTRL | Qt::Key_I),
              [this] { splitLines(*editor()); });
    addAction(lines, tr("&Join Lines"), "actionJoinLines", QKeySequence(Qt::CTRL | Qt::Key_J),
              [this] { joinLines(*editor()); });
    addAction(lines, tr("Move Up Current Li&ne"), "actionLineUp", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Up),
              sci(SCI_MOVESELECTEDLINESUP));
    addAction(lines, tr("Move Down Current Lin&e"), "actionLineDown", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down),
              sci(SCI_MOVESELECTEDLINESDOWN));
    addAction(lines, tr("Remove &Empty Lines"), "actionRemoveEmptyLines", QKeySequence(),
              [this] { removeEmptyLines(*editor(), false); });
    addAction(lines, tr("Remove Empty Lines (Containing &Blank characters)"), "actionRemoveBlankLines", QKeySequence(),
              [this] { removeEmptyLines(*editor(), true); });
    addAction(lines, tr("Insert Blank Line &Above Current"), "actionBlankLineAbove",
              QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Return), [this] { insertBlankLine(*editor(), true); });
    addAction(lines, tr("Insert Blank Line &Below Current"), "actionBlankLineBelow",
              QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_Return), [this] { insertBlankLine(*editor(), false); });
    lines->addSeparator();
    // Ordenar (IDM_EDIT_SORTLINES_*): una entrada por ordenador y sentido
    struct SortEntry
    {
        const char* text;
        const char* name;
        SortKind kind;
        bool descending;
    };
    static constexpr SortEntry kSortEntries[] = {
        {QT_TR_NOOP("&Reverse Line Order"), "actionSortReverse", SortKind::Reverse, false},
        {QT_TR_NOOP("&Randomize Line Order"), "actionSortRandom", SortKind::Random, false},
        {nullptr, nullptr, SortKind::Lexicographic, false},
        {QT_TR_NOOP("Sort Lines Lexicographically Ascending"), "actionSortLexAsc", SortKind::Lexicographic, false},
        {QT_TR_NOOP("Sort Lines Lexicographically Descending"), "actionSortLexDesc", SortKind::Lexicographic, true},
        {QT_TR_NOOP("Sort Lines Lex. Ascending Ignoring Case"), "actionSortCaseInsAsc", SortKind::LexicographicCaseInsensitive, false},
        {QT_TR_NOOP("Sort Lines Lex. Descending Ignoring Case"), "actionSortCaseInsDesc", SortKind::LexicographicCaseInsensitive, true},
        {QT_TR_NOOP("Sort Lines In Locale Order Ascending"), "actionSortLocaleAsc", SortKind::Locale, false},
        {QT_TR_NOOP("Sort Lines In Locale Order Descending"), "actionSortLocaleDesc", SortKind::Locale, true},
        {QT_TR_NOOP("Sort Lines As Integers Ascending"), "actionSortIntAsc", SortKind::Integer, false},
        {QT_TR_NOOP("Sort Lines As Integers Descending"), "actionSortIntDesc", SortKind::Integer, true},
        {QT_TR_NOOP("Sort Lines As Decimals (Comma) Ascending"), "actionSortCommaAsc", SortKind::DecimalComma, false},
        {QT_TR_NOOP("Sort Lines As Decimals (Comma) Descending"), "actionSortCommaDesc", SortKind::DecimalComma, true},
        {QT_TR_NOOP("Sort Lines As Decimals (Dot) Ascending"), "actionSortDotAsc", SortKind::DecimalDot, false},
        {QT_TR_NOOP("Sort Lines As Decimals (Dot) Descending"), "actionSortDotDesc", SortKind::DecimalDot, true},
        {QT_TR_NOOP("Sort Lines By Length Ascending"), "actionSortLengthAsc", SortKind::Length, false},
        {QT_TR_NOOP("Sort Lines By Length Descending"), "actionSortLengthDesc", SortKind::Length, true},
    };
    for (const SortEntry& entry : kSortEntries) {
        if (!entry.text) {
            lines->addSeparator();
            continue;
        }
        addAction(lines, tr(entry.text), entry.name, QKeySequence(),
                  [this, kind = entry.kind, descending = entry.descending] { sortCurrentLines(kind, descending); });
    }

    // Convertir (IDM_EDIT_UPPERCASE… IDM_EDIT_RANDOMCASE)
    QMenu* convert = edit->addMenu(tr("Con&vert Case to"));
    struct CaseEntry
    {
        const char* text;
        const char* name;
        TextCase textCase;
        QKeyCombination key;
    };
    const CaseEntry caseEntries[] = {
        {QT_TR_NOOP("&UPPERCASE"), "actionUpperCase", TextCase::Upper, Qt::CTRL | Qt::SHIFT | Qt::Key_U},
        {QT_TR_NOOP("&lowercase"), "actionLowerCase", TextCase::Lower, Qt::CTRL | Qt::Key_U},
        {QT_TR_NOOP("&Proper Case"), "actionProperCase", TextCase::ProperForce, Qt::ALT | Qt::Key_U},
        {QT_TR_NOOP("Proper Case (&blend)"), "actionProperCaseBlend", TextCase::ProperBlend, Qt::ALT | Qt::SHIFT | Qt::Key_U},
        {QT_TR_NOOP("&Sentence case"), "actionSentenceCase", TextCase::SentenceForce, Qt::CTRL | Qt::ALT | Qt::Key_U},
        {QT_TR_NOOP("Sentence case (b&lend)"), "actionSentenceCaseBlend", TextCase::SentenceBlend,
         Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_U},
        {QT_TR_NOOP("&iNVERT cASE"), "actionInvertCase", TextCase::Invert, QKeyCombination()},
        {QT_TR_NOOP("ra&NdOm CasE"), "actionRandomCase", TextCase::Random, QKeyCombination()},
    };
    for (const CaseEntry& entry : caseEntries) {
        addAction(convert, tr(entry.text), entry.name,
                  entry.key == QKeyCombination() ? QKeySequence() : QKeySequence(entry.key),
                  [this, textCase = entry.textCase] {
                      convertCase(*editor(), textCase, static_cast<unsigned>(QDateTime::currentMSecsSinceEpoch()));
                  });
    }

    // Comentar (IDM_EDIT_BLOCK_COMMENT…): Ctrl+Q es el de N++ (Salir queda en Alt+F4)
    QMenu* comment = edit->addMenu(tr("C&omment/Uncomment"));
    addAction(comment, tr("&Toggle Single Line Comment"), "actionToggleComment", QKeySequence(Qt::CTRL | Qt::Key_Q),
              [this] { commentCurrent(CommentKind::Toggle); });
    addAction(comment, tr("Single Line &Comment"), "actionLineComment", QKeySequence(Qt::CTRL | Qt::Key_K),
              [this] { commentCurrent(CommentKind::Comment); });
    addAction(comment, tr("Single Line &Uncomment"), "actionLineUncomment", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K),
              [this] { commentCurrent(CommentKind::Uncomment); });
    addAction(comment, tr("&Block Comment"), "actionStreamComment", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q),
              [this] { commentCurrent(CommentKind::Stream); });
    addAction(comment, tr("Block Unc&omment"), "actionStreamUncomment", QKeySequence(),
              [this] { commentCurrent(CommentKind::StreamUncomment); });

    // Operaciones con espacios (Editar > Blank Operations de N++)
    QMenu* blanks = edit->addMenu(tr("&Blank Operations"));
    addAction(blanks, tr("Trim &Trailing Space"), "actionTrimTrailing", QKeySequence(),
              [this] { trimTrailingSpaces(); });
    addAction(blanks, tr("Trim &Leading Space"), "actionTrimLeading", QKeySequence(),
              [this] { trimLines(*editor(), true, false); });
    addAction(blanks, tr("Trim Leading a&nd Trailing Space"), "actionTrimBoth", QKeySequence(),
              [this] { trimLines(*editor(), true, true); });
    addAction(blanks, tr("&EOL to Space"), "actionEolToSpace", QKeySequence(),
              [this] { eolToSpace(*editor()); });
    addAction(blanks, tr("Trim Both and EOL to &Space"), "actionTrimAll", QKeySequence(),
              [this] { trimAll(*editor()); });
    blanks->addSeparator();
    addAction(blanks, tr("&TAB to Space"), "actionTabToSpace", QKeySequence(),
              [this] { convertTabs(*editor(), TabConversion::TabsToSpaces); });
    addAction(blanks, tr("Space to TAB (&All)"), "actionSpaceToTabAll", QKeySequence(),
              [this] { convertTabs(*editor(), TabConversion::SpacesToTabsAll); });
    addAction(blanks, tr("Space to TAB (L&eading)"), "actionSpaceToTabLeading", QKeySequence(),
              [this] { convertTabs(*editor(), TabConversion::SpacesToTabsLeading); });
    // Autocompletado (los atajos de N++)
    QMenu* complete = edit->addMenu(tr("&Auto-Completion"));
    addAction(complete, tr("&Function Completion"), "actionCompleteFunction", QKeySequence(Qt::CTRL | Qt::Key_Space),
              [this] {
                  if (!_lsp->complete(*editor(), currentBuffer(), 0))
                      autoCompletion(_activePane)->showApiComplete();
              });
    addAction(complete, tr("&Word Completion"), "actionCompleteWord", QKeySequence(Qt::CTRL | Qt::Key_Return),
              [this] { autoCompletion(_activePane)->showWordComplete(true); });
    addAction(complete, tr("Function &Parameters Hint"), "actionFunctionHint", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space),
              [this] {
                  if (!_lsp->signatureHelp(*editor(), currentBuffer(), 0))
                      autoCompletion(_activePane)->showFunctionComplete();
              });
    addAction(complete, tr("P&ath Completion"), "actionCompletePath", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Space),
              [this] { autoCompletion(_activePane)->showPathCompletion(); });
    // Con el servidor de lenguaje (no existen en N++)
    addAction(edit, tr("Rena&me Symbol…"), "actionRenameSymbol", QKeySequence(), [this] { renameSymbol(); });
    addAction(edit, tr("F&ormat Document"), "actionFormatDocument", QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_F), [this] { formatDocument(); });
    // Fragmentos de código (no existen en N++)
    addAction(edit, tr("Insert S&nippet…"), "actionInsertSnippet", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_J), [this] { showSnippetPicker(); });
    addAction(edit, tr("Code &Actions…"), "actionCodeActions", QKeySequence(Qt::CTRL | Qt::Key_Period), [this] { showCodeActions(); });
    edit->addSeparator();
    // IDM_EDIT_COLUMNMODE: Editor de columnas (Alt+C)
    // Editar › Sobre la selección (IDM_EDIT_OPENSELECTEDFILE*, IDM_EDIT_REDACT_SELECTION,
    // IDM_EDIT_SEARCHONINTERNET, IDM_EDIT_CHANGESEARCHENGINE)
    QMenu* onSelection = edit->addMenu(tr("&On Selection"));
    onSelection->setObjectName(QStringLiteral("menuOnSelection"));
    addAction(onSelection, tr("Open &File"), "actionOpenSelectedFile", QKeySequence(), [this] { openSelectedPath(false); });
    addAction(onSelection, tr("Open Containing F&older"), "actionOpenSelectedFolder", QKeySequence(),
              [this] { openSelectedPath(true); });
    onSelection->addSeparator();
    addAction(onSelection, tr("&Redact Selection █ (Shift: ●)"), "actionRedactSelection", QKeySequence(), [this] {
        redactSelection(QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier));
    });
    onSelection->addSeparator();
    addAction(onSelection, tr("&Search on Internet"), "actionSearchOnInternet", QKeySequence(), [this] { searchOnInternet(); });
    addAction(onSelection, tr("&Change Search Engine…"), "actionChangeSearchEngine", QKeySequence(),
              [this] { showPreferences(QStringLiteral("searchEngine")); });
    edit->addSeparator();
    // Selección múltiple (IDM_EDIT_MULTISELECT*): la palabra o la selección, en todo el documento o la siguiente
    QMenu* multiAll = edit->addMenu(tr("Multi-select &All"));
    QMenu* multiNext = edit->addMenu(tr("Multi-select Ne&xt"));
    const std::pair<const char*, int> multiFlags[] = {
        {"", 0}, {"Case", SCFIND_MATCHCASE}, {"Word", SCFIND_WHOLEWORD}, {"CaseWord", SCFIND_MATCHCASE | SCFIND_WHOLEWORD}};
    const QString multiTexts[] = {tr("Ignore Case && Whole Word"), tr("Match Case Only"), tr("Match Whole Word Only"),
                                  tr("Match Case && Whole Word")};
    for (int i = 0; i < 4; ++i) {
        const int flags = multiFlags[i].second;
        addAction(multiAll, multiTexts[i], QByteArray(QByteArray("actionMultiSelectAll") + multiFlags[i].first).constData(),
                  QKeySequence(), [this, flags] { multiSelect(flags, true); });
        addAction(multiNext, multiTexts[i], QByteArray(QByteArray("actionMultiSelectNext") + multiFlags[i].first).constData(),
                  QKeySequence(), [this, flags] { multiSelect(flags, false); });
    }
    addAction(edit, tr("&Undo the Latest Added Multi-Select"), "actionMultiSelectUndo", QKeySequence(), [this] {
        // IDM_EDIT_MULTISELECTUNDO: quita la última añadida
        EditorView* view = editor();
        if (const sptr_t n = view->sci(SCI_GETSELECTIONS); n > 0)
            view->sci(SCI_DROPSELECTIONN, static_cast<uptr_t>(n - 1));
    });
    addAction(edit, tr("S&kip Current && Go to Next Multi-select"), "actionMultiSelectSkip", QKeySequence(), [this] {
        // IDM_EDIT_MULTISELECTSSKIP: añade la siguiente con las mismas opciones y quita la actual
        EditorView* view = editor();
        view->sci(SCI_TARGETWHOLEDOCUMENT);
        view->sci(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(_multiSelectFlags));
        view->sci(SCI_MULTIPLESELECTADDNEXT);
        if (const sptr_t n = view->sci(SCI_GETSELECTIONS); n > 1)
            view->sci(SCI_DROPSELECTIONN, static_cast<uptr_t>(n - 2));
    });
    edit->addSeparator();
    addAction(edit, tr("Colu&mn Editor…"), "actionColumnEditor", QKeySequence(Qt::ALT | Qt::Key_C), [this] {
        ColumnEditorDialog dialog(*editor(), _syntax.config(), this);
        dialog.exec();
    });
    // IDM_EDIT_CHAR_PANEL, IDM_EDIT_CLIPBOARDHISTORY_PANEL: los paneles se abren y se cierran
    const std::tuple<const char*, const char*, QString> editPanels[] = {
        {"charPanel", "actionCharPanel", tr("Character &Panel")},
        {"clipboardHistory", "actionClipboardHistory", tr("Clipboard &History")},
    };
    for (const auto& [name, actionName, text] : editPanels) {
        QDockWidget* dock = panelDock(QString::fromLatin1(name));
        QAction* toggle = edit->addAction(text);
        toggle->setObjectName(QString::fromLatin1(actionName));
        toggle->setCheckable(true);
        toggle->setChecked(dock->isVisibleTo(this));
        connect(toggle, &QAction::toggled, dock, [dock](bool on) {
            dock->setVisible(on);
            if (on)
                dock->raise();
        });
        connect(dock, &QDockWidget::visibilityChanged, toggle, [this, toggle, dock] {
            toggle->setChecked(dock->isVisibleTo(this));
        });
    }
    edit->addSeparator();
    edit->addMenu(createEolMenu());
    edit->addSeparator();
    _readOnlyAction = addAction(edit, tr("Read-&Only"), "actionReadOnly", QKeySequence(), [this](bool checked) {
        if (Buffer* buf = _documents.buffer(currentBuffer()))
            buf->setUserReadOnly(checked);
    });
    _readOnlyAction->setCheckable(true);
    // IDM_EDIT_SETREADONLYFORALLDOCS, IDM_EDIT_CLEARREADONLYFORALLDOCS
    for (const bool on : {true, false}) {
        addAction(edit, on ? tr("Read-Only for &All Documents") : tr("Clear Read-Only for All Doc&uments"),
                  on ? "actionReadOnlyAll" : "actionReadOnlyClearAll", QKeySequence(), [this, on] {
                      for (std::size_t i = 0; i < _documents.count(); ++i)
                          _documents.bufferAt(i)->setUserReadOnly(on);
                      refreshMenuChecks();
                  });
    }

    createSearchMenu();

    // Ver
    QMenu* view = menuBar()->addMenu(tr("&View"));
    addAction(view, tr("Command &Palette…"), "actionCommandPalette", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A),
              [this] { showCommandPalette(QStringLiteral(">")); });
    view->addSeparator();
    // IDM_VIEW_ALWAYSONTOP, IDM_VIEW_FULLSCREENTOGGLE, IDM_VIEW_POSTIT
    QAction* onTop = addAction(view, tr("Always on &Top"), "actionAlwaysOnTop", QKeySequence(), [this] { toggleAlwaysOnTop(); });
    onTop->setCheckable(true);
    QAction* fullScreen = addAction(view, tr("To&ggle Full Screen Mode"), "actionFullScreen", QKeySequence(Qt::Key_F11),
                                    [this] { toggleFullScreen(); });
    fullScreen->setCheckable(true);
    QAction* postIt = addAction(view, tr("Post-&It"), "actionPostIt", QKeySequence(Qt::Key_F12), [this] { togglePostIt(); });
    postIt->setCheckable(true);
    // Siguen respondiendo con la barra de menús oculta (Post-It)
    QWidget::addAction(fullScreen);
    QWidget::addAction(postIt);
    view->addSeparator();
    // IDM_VIEW_DOC_MAP, IDM_VIEW_DOCLIST, IDM_VIEW_FUNC_LIST, IDM_VIEW_FILEBROWSER
    const std::pair<const char*, QString> panelEntries[] = {
        {"documentMap", tr("Document &Map")}, {"documentList", tr("Document &List")},
        {"functionList", tr("&Function List")}, {"folderAsWorkspace", tr("Folder as &Workspace")},
        {"problems", tr("P&roblems")},
    };
    for (const auto& [name, text] : panelEntries) {
        QDockWidget* dock = panelDock(QString::fromLatin1(name));
        QAction* toggle = view->addAction(text);
        QString actionName = QString::fromLatin1(name);
        actionName[0] = actionName[0].toUpper();
        toggle->setObjectName(QStringLiteral("action") + actionName);
        toggle->setCheckable(true);
        toggle->setChecked(dock->isVisibleTo(this));
        connect(toggle, &QAction::toggled, dock, &QDockWidget::setVisible);
        connect(dock, &QDockWidget::visibilityChanged, toggle, [this, toggle, dock] {
            toggle->setChecked(dock->isVisibleTo(this));
        });
    }
    // Terminal integrada (no existe en N++): Ctrl+` o, en los teclados en español, Ctrl+Ñ
    QAction* terminal = addAction(view, tr("&Terminal"), "actionTerminal", QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft),
                                  [this] { toggleTerminal(); });
    terminal->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft), QKeySequence(Qt::CTRL | Qt::Key_Ntilde)});
    addAction(view, tr("New Te&rminal"), "actionNewTerminal", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_QuoteLeft),
              [this] { openTerminal(terminalDirectory()); });
    addAction(view, tr("Run Selection in Terminal"), "actionRunSelectionInTerminal", QKeySequence(),
              [this] { runSelectionInTerminal(); });
    // Comparar (como el plugin Compare de N++)
    QMenu* compare = view->addMenu(tr("Co&mpare"));
    addAction(compare, tr("Compare with &Other View"), "actionCompareViews", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C),
              [this] { startCompare(CompareWith::OtherView); });
    addAction(compare, tr("Compare with Last &Saved Version"), "actionCompareSaved", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_D),
              [this] { startCompare(CompareWith::Saved); });
    addAction(compare, tr("Compare with &Git (HEAD)"), "actionCompareGit", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H),
              [this] { startCompare(CompareWith::GitHead); });
    compare->addSeparator();
    addAction(compare, tr("&Next Difference"), "actionNextDifference", QKeySequence(), [this] {
        if (_compare)
            statusBar()->showMessage(tr("Difference %1 of %2").arg(_compare->goToDifference(editor(), true)).arg(_compare->changes().size()), 4000);
    });
    addAction(compare, tr("&Previous Difference"), "actionPreviousDifference", QKeySequence(), [this] {
        if (_compare)
            statusBar()->showMessage(tr("Difference %1 of %2").arg(_compare->goToDifference(editor(), false)).arg(_compare->changes().size()), 4000);
    });
    compare->addSeparator();
    QAction* ignoreSpaces = addAction(compare, tr("Ignore &Spaces"), "actionCompareIgnoreSpaces", QKeySequence(), [this](bool on) {
        _compareIgnoreSpaces = on;
        if (_compare)
            _compare->setOptions({_compareIgnoreSpaces, _compareIgnoreCase});
    });
    ignoreSpaces->setCheckable(true);
    QAction* ignoreCase = addAction(compare, tr("Ignore &Case"), "actionCompareIgnoreCase", QKeySequence(), [this](bool on) {
        _compareIgnoreCase = on;
        if (_compare)
            _compare->setOptions({_compareIgnoreSpaces, _compareIgnoreCase});
    });
    ignoreCase->setCheckable(true);
    compare->addSeparator();
    addAction(compare, tr("&Clear Comparison"), "actionCompareClear", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_X), [this] { stopCompare(); });
    view->addSeparator();
    _wrapAction = addAction(view, tr("&Word wrap"), "actionWrap", QKeySequence(),
                            [this](bool checked) { toggleViewOption(&Preferences::wrap, checked); });
    _wrapAction->setCheckable(true);
    _viewOptionActions.emplace_back(&Preferences::wrap, _wrapAction);
    // Ver > Mostrar símbolo (IDM_VIEW_TAB_SPACE, IDM_VIEW_EOL, IDM_VIEW_ALL_CHARACTERS…)
    QMenu* symbols = view->addMenu(tr("Show S&ymbol"));
    const std::pair<bool Preferences::*, std::pair<QString, const char*>> symbolOptions[] = {
        {&Preferences::whiteSpaceShow, {tr("Show &Space and TAB"), "actionShowWhiteSpace"}},
        {&Preferences::eolShow, {tr("Show &End of Line"), "actionShowEol"}},
        {&Preferences::indentGuideLine, {tr("Show &Indent Guide"), "actionShowIndentGuides"}},
        {&Preferences::wrapSymbolShow, {tr("Show &Wrap Symbol"), "actionShowWrapSymbol"}},
        {&Preferences::npcShow, {tr("Show &Non-Printing Characters"), "actionShowNpc"}},
        {&Preferences::ccShow, {tr("Show &Control Characters && Unicode EOL"), "actionShowControlChars"}},
    };
    for (const auto& [option, label] : symbolOptions) {
        QAction* action = addAction(symbols, label.first, label.second, QKeySequence(),
                                    [this, member = option](bool checked) { toggleViewOption(member, checked); });
        action->setCheckable(true);
        _viewOptionActions.emplace_back(option, action);
    }
    _showAllAction = addAction(symbols, tr("Show &All Characters"), "actionShowAll", QKeySequence(), [this](bool checked) {
        Preferences p = _prefs;
        p.whiteSpaceShow = checked;
        p.eolShow = checked;
        p.npcShow = checked; // como IDM_VIEW_ALL_CHARACTERS
        p.ccShow = checked;
        setPreferences(p);
    });
    _showAllAction->setCheckable(true);
    QMenu* themeMenu = view->addMenu(tr("&Theme"));
    connect(themeMenu, &QMenu::aboutToShow, this, [this, themeMenu] { populateThemeMenu(themeMenu); });
    populateThemeMenu(themeMenu);
    // Zoom (IDM_VIEW_ZOOMIN/ZOOMOUT/ZOOMRESTORE)
    QMenu* zoom = view->addMenu(tr("&Zoom"));
    addAction(zoom, tr("Zoom &In"), "actionZoomIn", QKeySequence(Qt::CTRL | Qt::Key_Plus), sci(SCI_ZOOMIN));
    addAction(zoom, tr("Zoom &Out"), "actionZoomOut", QKeySequence(Qt::CTRL | Qt::Key_Minus), sci(SCI_ZOOMOUT));
    addAction(zoom, tr("Res&tore Default Zoom"), "actionZoomRestore", QKeySequence(Qt::CTRL | Qt::Key_Slash),
              [this] { editor()->sci(SCI_SETZOOM, 0); });
    view->addSeparator();
    // Plegado (IDM_VIEW_FOLDALL, UNFOLDALL, FOLD_CURRENT, FOLD_1…8, UNFOLD_1…8)
    addAction(view, tr("&Fold All"), "actionFoldAll", QKeySequence(Qt::ALT | Qt::Key_0),
              [this] { foldAll(*editor(), false); });
    addAction(view, tr("&Unfold All"), "actionUnfoldAll", QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_0),
              [this] { foldAll(*editor(), true); });
    addAction(view, tr("Fold &Current Level"), "actionFoldCurrent", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F),
              [this] { foldCurrent(*editor(), false); });
    addAction(view, tr("Unfold Current &Level"), "actionUnfoldCurrent",
              QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_F), [this] { foldCurrent(*editor(), true); });
    QMenu* foldLevels = view->addMenu(tr("Fold Le&vel"));
    QMenu* unfoldLevels = view->addMenu(tr("Unfold Lev&el"));
    for (int level = 1; level <= 8; ++level) {
        const auto key = static_cast<Qt::Key>(Qt::Key_0 + level);
        const QByteArray n = QByteArray::number(level);
        addAction(foldLevels, tr("Level &%1").arg(level), QByteArray("actionFoldLevel" + n).constData(),
                  QKeySequence(Qt::ALT | key), [this, level] { foldLevel(*editor(), level - 1, false); });
        addAction(unfoldLevels, tr("Level &%1").arg(level), QByteArray("actionUnfoldLevel" + n).constData(),
                  QKeySequence(Qt::ALT | Qt::SHIFT | key), [this, level] { foldLevel(*editor(), level - 1, true); });
    }
    // IDM_VIEW_HIDELINES
    addAction(view, tr("&Hide Lines"), "actionHideLines", QKeySequence(Qt::ALT | Qt::Key_H), [this] {
        hideLines(*editor());
        // La otra vista, si muestra el mismo documento (un clon)
        EditorPane* other = otherPane(_activePane);
        if (other && other->currentBuffer() == currentBuffer())
            restoreHiddenLines(*other->editor());
    });
    view->addSeparator();
    addAction(view, tr("&Move to Other View"), "actionMoveToOtherView", QKeySequence(), [this] { moveToOtherView(); });
    addAction(view, tr("&Clone to Other View"), "actionCloneToOtherView", QKeySequence(), [this] { cloneToOtherView(); });
    // IDM_VIEW_GOTO_NEW_INSTANCE, IDM_VIEW_LOAD_IN_NEW_INSTANCE
    addAction(view, tr("Mo&ve to New Instance"), "actionMoveToNewInstance", QKeySequence(), [this] { openInNewInstance(true); });
    addAction(view, tr("&Open in New Instance"), "actionOpenInNewInstance", QKeySequence(), [this] { openInNewInstance(false); });
    addAction(view, tr("Switch to &Other View"), "actionSwitchView", QKeySequence(Qt::Key_F8), [this] { switchToOtherView(); });
    addAction(view, tr("&Rotate Split"), "actionRotateSplit", QKeySequence(), [this] { toggleSplitOrientation(); });
    view->addSeparator();
    // IDM_VIEW_SYNSCROLLV, IDM_VIEW_SYNSCROLLH
    QAction* syncV = addAction(view, tr("Sy&nchronize Vertical Scrolling"), "actionSyncScrollV", QKeySequence(),
                               [this] { setScrollSync(true); });
    syncV->setCheckable(true);
    QAction* syncH = addAction(view, tr("Syn&chronize Horizontal Scrolling"), "actionSyncScrollH", QKeySequence(),
                               [this] { setScrollSync(false); });
    syncH->setCheckable(true);
    view->addSeparator();
    // IDM_VIEW_SUMMARY, IDM_VIEW_MONITORING
    addAction(view, tr("S&ummary…"), "actionSummary", QKeySequence(), [this] {
        _prompter->inform(this, tr("Summary"), documentSummary());
    });
    // El corrector ortográfico (no existe en N++): Preferencias › Ortografía
    QAction* spelling = addAction(view, tr("Check S&pelling"), "actionSpellCheck", QKeySequence(), [this] {
        toggleViewOption(&Preferences::spellCheck, !_prefs.spellCheck);
    });
    spelling->setCheckable(true);
    QAction* monitoring = addAction(view, tr("Monitoring (tail -&f)"), "actionMonitoring", QKeySequence(),
                                    [this] { toggleMonitoring(); });
    monitoring->setCheckable(true);
    view->addSeparator();
    // El submenú «Pestaña» de N++ (IDM_VIEW_TAB*)
    QMenu* tab = view->addMenu(tr("Ta&b"));
    tab->setObjectName(QStringLiteral("menuTab"));
    const QString ordinals[] = {tr("1st Tab"), tr("2nd Tab"), tr("3rd Tab"), tr("4th Tab"), tr("5th Tab"),
                                tr("6th Tab"), tr("7th Tab"), tr("8th Tab"), tr("9th Tab")};
    for (int n = 1; n <= 9; ++n) {
        // Ctrl + el número del teclado numérico, como N++
        addAction(tab, ordinals[n - 1],
                  QByteArray("actionTab" + QByteArray::number(n)).constData(),
                  QKeySequence(QKeyCombination(Qt::ControlModifier | Qt::KeypadModifier, static_cast<Qt::Key>(Qt::Key_0 + n))),
                  [this, n] { goToTab(n - 1); });
    }
    tab->addSeparator();
    addAction(tab, tr("&First Tab"), "actionFirstTab", QKeySequence(), [this] { goToTab(0); });
    addAction(tab, tr("&Last Tab"), "actionLastTab", QKeySequence(), [this] { goToTab(_activePane->count() - 1); });
    addAction(tab, tr("&Next Tab"), "actionNextTab", QKeySequence(Qt::CTRL | Qt::Key_PageDown), [this] {
        DocTabBar* tabs = _activePane->tabBar();
        if (tabs->count() > 1)
            tabs->setCurrentIndex((tabs->currentIndex() + 1) % tabs->count());
    });
    addAction(tab, tr("&Previous Tab"), "actionPreviousTab", QKeySequence(Qt::CTRL | Qt::Key_PageUp), [this] {
        DocTabBar* tabs = _activePane->tabBar();
        if (tabs->count() > 1)
            tabs->setCurrentIndex((tabs->currentIndex() + tabs->count() - 1) % tabs->count());
    });
    tab->addSeparator();
    addAction(tab, tr("Move to &Start"), "actionTabToStart", QKeySequence(), [this] { moveCurrentTab(-1000000); });
    addAction(tab, tr("Move to &End"), "actionTabToEnd", QKeySequence(), [this] { moveCurrentTab(1000000); });
    addAction(tab, tr("Move Tab &Forward"), "actionTabForward", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_PageDown),
              [this] { moveCurrentTab(1); });
    addAction(tab, tr("Move Tab &Backward"), "actionTabBackward", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_PageUp),
              [this] { moveCurrentTab(-1); });
    tab->addSeparator();
    QAction* pin = addAction(tab, tr("Pi&n Tab"), "actionPinTab", QKeySequence(), [this] {
        if (Buffer* buf = _documents.buffer(currentBuffer()))
            buf->setPinned(!buf->isPinned());
    });
    pin->setCheckable(true);
    tab->addSeparator();
    for (int c = 0; c < 5; ++c) {
        QAction* color = addAction(tab, tr("Apply Color &%1").arg(c + 1),
                                   QByteArray("actionTabColor" + QByteArray::number(c + 1)).constData(), QKeySequence(),
                                   [this, c] {
                                       if (Buffer* buf = _documents.buffer(currentBuffer()))
                                           buf->setTabColor(c);
                                   });
        QPixmap swatch(12, 12);
        swatch.fill(Qt::transparent);
        QPainter painter(&swatch);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(DocTabBar::tabColor(c, false));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(1, 1, 10, 10), 2, 2);
        color->setIcon(QIcon(swatch));
    }
    addAction(tab, tr("&Remove Color"), "actionTabColorNone", QKeySequence(), [this] {
        if (Buffer* buf = _documents.buffer(currentBuffer()))
            buf->setTabColor(-1);
    });

    // Codificación
    QMenu* encoding = createEncodingMenu();
    menuBar()->addMenu(encoding);
    _encodingMenu = encoding;

    _languageMenu = createLanguageMenu();
    menuBar()->addMenu(_languageMenu);

    // Configuración
    QMenu* settings = menuBar()->addMenu(tr("Se&ttings"));
    addAction(settings, tr("&Preferences…"), "actionPreferences", QKeySequence(), [this] { showPreferences(); });
    // IDM_LANGSTYLE_CONFIG_DLG
    addAction(settings, tr("&Style Configurator…"), "actionStyleConfigurator", QKeySequence(),
              [this] { showStyleConfigurator(); });
    addAction(settings, tr("Shortcut &Mapper…"), "actionShortcutMapper", QKeySequence(), [this] { showShortcutMapper(); });
    addAction(settings, tr("Edit S&nippets…"), "actionEditSnippets", QKeySequence(), [this] { editUserSnippets(); });
    // IDM_SETTING_IMPORTPLUGIN, IDM_SETTING_IMPORTSTYLETHEMES, IDM_SETTING_EDITCONTEXTMENU
    settings->addSeparator();
    QMenu* import = settings->addMenu(tr("I&mport"));
    import->setObjectName(QStringLiteral("menuImport"));
    addAction(import, tr("Import &plugin(s)…"), "actionImportPlugins", QKeySequence(), [this] {
        importPlugins(_prompter->openFileNames(this, QDir::homePath(), tr("Edita plugin (*.so *.zip)")));
    });
    addAction(import, tr("Import style &theme(s)…"), "actionImportThemes", QKeySequence(), [this] {
        importThemes(_prompter->openFileNames(this, QDir::homePath(), tr("Style theme (*.xml)")));
    });
    addAction(settings, tr("Edit Popup &ContextMenu"), "actionEditContextMenu", QKeySequence(), [this] {
        _prompter->inform(this, tr("Editing contextMenu"),
                          tr("Editing contextMenu.xml allows you to modify the context menu of the edit zone. "
                             "The changes are used the next time the menu opens, after saving the file."));
        openFile(contextMenuPath());
    });
    // tabContextMenu.xml (N++ lo lee si existe, pero no tiene orden para editarlo): la primera
    // vez, con el de serie
    addAction(settings, tr("Edit &Tab ContextMenu"), "actionEditTabContextMenu", QKeySequence(), [this] {
        const QString path = tabContextMenuPath();
        if (!QFileInfo::exists(path)) {
            paths::ensureConfigDir();
            if (const QString model = paths::findDataFile(QStringLiteral("tabContextMenu.xml")); !model.isEmpty()) {
                QFile::copy(model, path);
                QFile(path).setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
            }
        }
        _prompter->inform(this, tr("Editing tabContextMenu"),
                          tr("While tabContextMenu.xml exists, the menu of the tabs is built from it. The changes are used "
                             "the next time the menu opens, after saving the file. Delete the file to get the usual menu back."));
        openFile(path);
    });
    // Servidores de lenguaje (no existen en N++)
    settings->addSeparator();
    QMenu* lspMenu = settings->addMenu(tr("&Language Server"));
    addAction(lspMenu, tr("&Restart Language Server"), "actionLspRestart", QKeySequence(), [this] {
        _lsp->restart(currentBuffer());
        updateProblemsField();
    });
    addAction(lspMenu, tr("Show Language Server &Log"), "actionLspLog", QKeySequence(), [this] {
        const QString log = _lsp->log(currentBuffer());
        if (log.isEmpty()) {
            statusBar()->showMessage(tr("This document has no language server"), 5000);
            return;
        }
        newDocument();
        editor()->sends(SCI_SETTEXT, 0, log.toUtf8().constData());
        editor()->sci(SCI_SETSAVEPOINT);
    });

    // Herramientas (IDM_TOOL_*): MD5, SHA-1, SHA-256 y SHA-512
    QMenu* tools = menuBar()->addMenu(tr("T&ools"));
    tools->setObjectName(QStringLiteral("menuTools"));
    const std::tuple<const char*, QCryptographicHash::Algorithm> hashes[] = {
        {"Md5", QCryptographicHash::Md5}, {"Sha1", QCryptographicHash::Sha1},
        {"Sha256", QCryptographicHash::Sha256}, {"Sha512", QCryptographicHash::Sha512}};
    for (const auto& [key, algorithm] : hashes) {
        QMenu* sub = tools->addMenu(HashDialog::algorithmName(algorithm));
        const QByteArray base = QByteArray("action") + key;
        addAction(sub, tr("&Generate…"), QByteArray(base + "Text").constData(), QKeySequence(), [this, a = algorithm] {
            HashDialog dialog(HashDialog::Mode::Text, a, *_prompter, this);
            dialog.exec();
        });
        addAction(sub, tr("Generate from &files…"), QByteArray(base + "Files").constData(), QKeySequence(), [this, a = algorithm] {
            HashDialog dialog(HashDialog::Mode::Files, a, *_prompter, this);
            dialog.exec();
        });
        addAction(sub, tr("Generate from &selection into clipboard"), QByteArray(base + "Selection").constData(),
                  QKeySequence(), [this, a = algorithm] { hashSelectionToClipboard(a); });
    }

    createMacroMenu();
    createRunMenu();
    createWindowMenu();
    loadPlugins();

    // Ayuda
    // Sin tecla de acceso en inglés: Alt+H es Ver › Ocultar líneas, como en N++ (cuyo menú
    // de ayuda es «?»)
    QMenu* help = menuBar()->addMenu(tr("Help"));
    addAction(help, tr("User &Manual"), "actionUserManual", QKeySequence::HelpContents, [this] { showManual(); });
    addAction(help, tr("&Welcome Page"), "actionWelcomePage", QKeySequence(), [this] { showWelcomePage(); });
    if (kUpdateCheck) // un paquete de una distribución se actualiza con su gestor de paquetes
        addAction(help, tr("Check for &Updates…"), "actionCheckForUpdates", QKeySequence(), [this] { checkForUpdates(true); });
    addAction(help, tr("&Report a Problem…"), "actionReportProblem", QKeySequence(), [this] {
        if (!_openUrl(issueReportUrl()))
            _prompter->warn(this, tr("Report a Problem"),
                            tr("Could not open the browser. Report the problem at\n%1").arg(issueReportUrl().toString()));
    });
    addAction(help, tr("&Debug Info…"), "actionDebugInfo", QKeySequence(), [this] { showDebugInfo(); });
    help->addSeparator();
    // IDM_CMDLINEARGUMENTS e IDM_HOMESWEETHOME (el sitio de Edita es su repositorio)
    addAction(help, tr("Command &Line Arguments…"), "actionCommandLineArguments", QKeySequence(), [this] {
        _prompter->inform(this, tr("Command Line Arguments"), commandLineHelp());
    });
    addAction(help, tr("Edita &Home Page"), "actionHomePage", QKeySequence(), [this] {
        _openUrl(QUrl(QStringLiteral("https://github.com/alktrip/edita")));
    });
    help->addSeparator();
    QAction* about = help->addAction(tr("&About %1").arg(QString::fromUtf8(kAppName)), this, [this] {
        QMessageBox box(this);
        box.setObjectName(QStringLiteral("aboutBox"));
        box.setWindowTitle(tr("About %1").arg(QString::fromUtf8(kAppName)));
        box.setIconPixmap(QPixmap(QStringLiteral(":/edita/logo.png"))
                              .scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        box.setText(tr("<b>%1 %2</b><br>Text editor inspired by Notepad++.<br>Based on Scintilla and "
                       "Lexilla.<br>GNU GPL v3 or later license.")
                        .arg(QString::fromUtf8(kAppName), QString::fromUtf8(kAppVersion)));
        box.exec();
    });
    about->setObjectName(QStringLiteral("actionAbout"));
    help->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
    // Órdenes sin menú, solo para darles un atajo (las «shortcut only» de N++)
    const std::tuple<QString, const char*, std::function<void()>> shortcutOnly[] = {
        {tr("Toggle Macro Recording"), "actionToggleMacroRecord", [this] { toggleMacroRecording(); }},
        {tr("Function Parameters Previous Hint"), "actionFunctionHintPrevious",
         [this] { autoCompletion(_activePane)->callTipClick(1); }},
        {tr("Function Parameters Next Hint"), "actionFunctionHintNext", [this] { autoCompletion(_activePane)->callTipClick(2); }},
    };
    for (const auto& [text, name, slot] : shortcutOnly) {
        auto* action = new QAction(text, this);
        action->setObjectName(QString::fromLatin1(name));
        action->setProperty("defaultShortcut", QKeySequence());
        connect(action, &QAction::triggered, this, slot);
        QWidget::addAction(action);
    }
    createToolBar();
    applyShortcuts();
}

void MainWindow::checkForUpdatesIfDue(const QDate& today)
{
    update_check::State state = update_check::readState(_syntax.config());
    if (!kUpdateCheck || !update_check::isDue(_prefs.updateOnStartup, state, today)
        || update_check::releaseUrl(QString::fromUtf8(kHomepageUrl)).isEmpty())
        return;
    // Como launchUpdater: la próxima fecha se fija al comprobar, salga bien o no
    update_check::scheduleNext(state, _prefs.updateIntervalDays, today);
    update_check::writeState(_syntax.config(), state);
    _syntax.config().save();
    checkForUpdates(false);
}

void MainWindow::checkForUpdates(bool manual)
{
    const QUrl url = update_check::releaseUrl(QString::fromUtf8(kHomepageUrl));
    if (_updateReply)
        return;
    if (url.isEmpty()) {
        if (manual)
            _prompter->warn(this, tr("Check for Updates"), tr("Checking for new versions is disabled."));
        return;
    }
    if (!_updateNetwork)
        _updateNetwork = new QNetworkAccessManager(this);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);
    // GitHub pide un User-Agent; solo va la versión
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Edita/%1").arg(QString::fromUtf8(kAppVersion)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    _updateReply = _updateNetwork->get(request);
    connect(_updateReply, &QNetworkReply::finished, this, [this, reply = _updateReply.data(), manual] {
        updateCheckFinished(reply, manual);
    });
}

void MainWindow::updateCheckFinished(QNetworkReply* reply, bool manual)
{
    reply->deleteLater();
    _updateReply = nullptr;
    update_check::Release release;
    QString error = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
    if (error.isEmpty())
        update_check::parseReleases(reply->readAll(), kAppPrerelease, release, &error); // una estable no avisa de betas
    if (!error.isEmpty()) {
        if (manual) // al arrancar, sin red no se molesta a nadie
            _prompter->warn(this, tr("Check for Updates"), tr("Could not check for a new version: %1").arg(error));
        return;
    }
    const QString current = QString::fromUtf8(kAppVersion);
    if (update_check::compareVersions(release.version, current) <= 0) {
        if (manual)
            _notifications->show(NotificationBar::Kind::Info, tr("Edita is up to date"),
                                 tr("Edita %1 is the latest version.").arg(current));
        return;
    }
    update_check::State state = update_check::readState(_syntax.config());
    if (!manual && state.skippedVersion == release.version)
        return;
    const QUrl page = release.page.isValid() ? release.page : QUrl(QString::fromUtf8(kHomepageUrl) + QStringLiteral("/releases"));
    _notifications->show(
        NotificationBar::Kind::Info, tr("Edita %1 is available").arg(release.version),
        tr("You have version %1.").arg(current),
        {{tr("Download Page"), [this, page] { _openUrl(page); }},
         {tr("Skip This Version"), [this, version = release.version] {
              update_check::State s = update_check::readState(_syntax.config());
              s.skippedVersion = version;
              update_check::writeState(_syntax.config(), s);
              _syntax.config().save(); // la barra se cierra sola al pulsar
          }}});
}

QUrl MainWindow::crashReportUrl(const QString& report) const
{
    // El formulario cabe en una URL de unos 8 KB: el principio de la traza, que es lo que importa
    QString details = crash_report::readable(report).trimmed();
    constexpr qsizetype kMax = 4000;
    if (details.size() > kMax)
        details = details.left(kMax) + QStringLiteral("\n…");
    const QString signal = crash_report::signalName(report);
    return issue_report::formUrl(
        QString::fromUtf8(kHomepageUrl), QStringLiteral("bug.yml"), QString::fromUtf8(kAppVersion),
        issue_report::describe(issue_report::currentSystem(QGuiApplication::platformName())),
        {{QStringLiteral("what"), tr("Edita closed unexpectedly (%1). What I was doing:").arg(signal)},
         {QStringLiteral("extra"), QStringLiteral("```\n%1\n```").arg(details)}});
}

bool MainWindow::offerCrashReport(const QString& directory)
{
    const QString report = crash_report::pending(directory);
    if (report.isEmpty())
        return false;
    const QString signal = crash_report::signalName(report);
    _notifications->show(
        NotificationBar::Kind::Warning, tr("Edita closed unexpectedly"),
        tr("The last time, Edita closed because of an error (%1). Reporting it helps to fix it: the report "
           "has the version, your system and where it failed, and you can review it before sending it.")
            .arg(signal.isEmpty() ? tr("unknown") : signal),
        {{tr("Report…"), [this, directory, report] {
              _openUrl(crashReportUrl(report));
              crash_report::clear(directory);
          }},
         {tr("Copy Details"), [directory, report] {
              QGuiApplication::clipboard()->setText(crash_report::readable(report));
              crash_report::clear(directory);
          }},
         {tr("Discard"), [directory] { crash_report::clear(directory); }}});
    return true;
}

QUrl MainWindow::issueReportUrl() const
{
    return issue_report::formUrl(QString::fromUtf8(kHomepageUrl), QStringLiteral("bug.yml"), QString::fromUtf8(kAppVersion),
                                 issue_report::describe(issue_report::currentSystem(QGuiApplication::platformName())));
}

void MainWindow::applyShortcuts()
{
    for (QAction* action : findChildren<QAction*>()) {
        if (action->objectName().startsWith(QLatin1String("action")) && !action->property("defaultShortcut").isValid())
            action->setProperty("defaultShortcut", action->shortcut());
    }
    if (!readShortcuts(shortcutsPath(), _shortcutConfig))
        return; // un shortcuts.xml que no se entiende: los de serie
    for (QAction* action : findChildren<QAction*>()) {
        const QString name = action->objectName();
        if (action->property("pluginModule").isValid()) {
            for (const PluginShortcut& p : _shortcutConfig.plugins) {
                if (p.module == action->property("pluginModule").toString() && p.index == action->property("pluginIndex").toInt())
                    action->setShortcut(p.key.toKeySequence());
            }
            continue;
        }
        if (!name.startsWith(QLatin1String("action")))
            continue;
        const int nppCommand = action->property("nppCommand").toInt();
        const int id = nppCommand ? nppCommand : shortcutOnlyCommandFor(name);
        if (const auto it = _shortcutConfig.internal.find(id); id && it != _shortcutConfig.internal.end())
            action->setShortcut(it->second.toKeySequence());
        else if (const auto e = _shortcutConfig.edita.find(name); e != _shortcutConfig.edita.end())
            action->setShortcut(e->second.toKeySequence());
    }
    for (const ScintillaCommand& command : scintillaCommands()) {
        if (const auto it = _shortcutConfig.scintilla.find(command.command); it != _shortcutConfig.scintilla.end())
            applyScintillaKeys(command.command, command.defaults, it->second);
    }
}

void MainWindow::applyScintillaKeys(int sciCommand, const std::vector<KeyCombo>& before, const std::vector<KeyCombo>& after)
{
    for (EditorPane* pane : {_mainPane, _subPane}) {
        for (const KeyCombo& key : before) {
            if (const int def = scintillaKeyDefinition(key))
                pane->editor()->send(SCI_CLEARCMDKEY, static_cast<uptr_t>(def));
        }
        for (const KeyCombo& key : after) {
            if (const int def = scintillaKeyDefinition(key))
                pane->editor()->send(SCI_ASSIGNCMDKEY, static_cast<uptr_t>(def), sciCommand);
        }
    }
}

std::vector<ShortcutEntry> MainWindow::shortcutEntries() const
{
    using Kind = ShortcutEntry::Kind;
    std::vector<ShortcutEntry> entries;
    auto combos = [](const QKeySequence& sequence) {
        std::vector<KeyCombo> list;
        if (const KeyCombo combo = KeyCombo::fromKeySequence(sequence); !combo.isEmpty())
            list.push_back(combo);
        return list;
    };
    // Menú principal: las órdenes de cada menú, con el menú como categoría
    QSet<const QAction*> seen;
    std::function<void(const QMenu*, const QString&)> walk = [&](const QMenu* menu, const QString& category) {
        for (QAction* action : menu->actions()) {
            if (action->menu() && !action->menu()->objectName().startsWith(QLatin1String("plugin"))) {
                walk(action->menu(), category);
                continue;
            }
            const QString name = action->objectName();
            if (!name.startsWith(QLatin1String("action")) || name.startsWith(QLatin1String("actionMacro"))
                || name.startsWith(QLatin1String("actionRunCommand"))
                || seen.contains(action))
                continue;
            seen.insert(action);
            entries.push_back({Kind::Menu, name, plainMenuText(action->text()), category, combos(action->shortcut()),
                               combos(action->property("defaultShortcut").value<QKeySequence>())});
        }
    };
    for (QAction* top : menuBar()->actions()) {
        if (top->menu())
            walk(top->menu(), plainMenuText(top->text()));
    }
    // Las que no están en un menú (solo atajo)
    for (QAction* action : actions()) {
        const QString name = action->objectName();
        if (!name.startsWith(QLatin1String("action")) || seen.contains(action))
            continue;
        seen.insert(action);
        entries.push_back({Kind::Menu, name, plainMenuText(action->text()), tr("Shortcut only"), combos(action->shortcut()),
                           combos(action->property("defaultShortcut").value<QKeySequence>())});
    }
    // Macros
    for (std::size_t i = 0; i < _savedMacros.size(); ++i) {
        const SavedMacro& m = _savedMacros[i];
        std::vector<KeyCombo> keys;
        if (m.key)
            keys.push_back(KeyCombo{m.ctrl, m.alt, m.shift, m.key});
        entries.push_back({Kind::Macros, QString::number(i), m.name, tr("Macro"), keys, {}});
    }
    // Órdenes del menú Ejecutar
    for (std::size_t i = 0; i < _userCommands.size(); ++i) {
        const UserCommand& c = _userCommands[i];
        std::vector<KeyCombo> keys;
        if (c.key)
            keys.push_back(KeyCombo{c.ctrl, c.alt, c.shift, c.key});
        entries.push_back({Kind::RunCommands, QString::number(i), c.name, c.command, keys, {}});
    }
    // Órdenes de los plugins
    for (QAction* action : findChildren<QAction*>()) {
        if (!action->property("pluginModule").isValid())
            continue;
        const QString id = action->property("pluginModule").toString() + QLatin1Char(':')
                         + QString::number(action->property("pluginIndex").toInt());
        const auto* menu = qobject_cast<QMenu*>(action->parent());
        entries.push_back({Kind::Plugin, id, action->text(), menu ? menu->title() : QString(), combos(action->shortcut()),
                           combos(action->property("defaultShortcut").value<QKeySequence>())});
    }
    // Scintilla
    for (const ScintillaCommand& command : scintillaCommands()) {
        const auto it = _shortcutConfig.scintilla.find(command.command);
        entries.push_back({Kind::Scintilla, QString::number(command.command), command.name, QString(),
                           it != _shortcutConfig.scintilla.end() ? it->second : command.defaults, command.defaults});
    }
    return entries;
}

bool MainWindow::setShortcutKeys(ShortcutEntry::Kind kind, const QString& id, const std::vector<KeyCombo>& keys)
{
    using Kind = ShortcutEntry::Kind;
    const KeyCombo combo = keys.empty() ? KeyCombo{} : keys.front();
    switch (kind) {
    case Kind::Macros: {
        const int index = id.toInt();
        if (index < 0 || static_cast<std::size_t>(index) >= _savedMacros.size())
            return false;
        SavedMacro& macro = _savedMacros[static_cast<std::size_t>(index)];
        macro.ctrl = combo.ctrl;
        macro.alt = combo.alt;
        macro.shift = combo.shift;
        macro.key = combo.key;
        const bool ok = writeMacros(shortcutsPath(), _savedMacros);
        rebuildMacroMenu();
        return ok;
    }
    case Kind::RunCommands: {
        const int index = id.toInt();
        if (index < 0 || static_cast<std::size_t>(index) >= _userCommands.size())
            return false;
        UserCommand& command = _userCommands[static_cast<std::size_t>(index)];
        command.ctrl = combo.ctrl;
        command.alt = combo.alt;
        command.shift = combo.shift;
        command.key = combo.key;
        const bool ok = writeUserCommands(shortcutsPath(), _userCommands);
        rebuildRunMenu();
        return ok;
    }
    case Kind::Menu: {
        QAction* action = findChild<QAction*>(id);
        if (!action)
            return false;
        action->setShortcut(combo.toKeySequence());
        const bool isDefault = combo == KeyCombo::fromKeySequence(action->property("defaultShortcut").value<QKeySequence>());
        const int nppCommand = action->property("nppCommand").toInt();
        if (const int idm = nppCommand ? nppCommand : shortcutOnlyCommandFor(id)) {
            if (isDefault)
                _shortcutConfig.internal.erase(idm);
            else
                _shortcutConfig.internal[idm] = combo;
        } else if (isDefault) {
            _shortcutConfig.edita.erase(id);
        } else {
            _shortcutConfig.edita[id] = combo;
        }
        break;
    }
    case Kind::Plugin: {
        const QString module = id.section(QLatin1Char(':'), 0, 0);
        const int index = id.section(QLatin1Char(':'), 1).toInt();
        QAction* target = nullptr;
        for (QAction* action : findChildren<QAction*>()) {
            if (action->property("pluginModule").toString() == module && action->property("pluginIndex").toInt() == index)
                target = action;
        }
        if (!target)
            return false;
        target->setShortcut(combo.toKeySequence());
        auto& plugins = _shortcutConfig.plugins;
        plugins.erase(std::remove_if(plugins.begin(), plugins.end(),
                                     [&](const PluginShortcut& p) { return p.module == module && p.index == index; }),
                      plugins.end());
        if (combo != KeyCombo::fromKeySequence(target->property("defaultShortcut").value<QKeySequence>()))
            plugins.push_back({module, index, combo});
        break;
    }
    case Kind::Scintilla: {
        const int command = id.toInt();
        const auto commands = scintillaCommands();
        const auto def = std::find_if(commands.begin(), commands.end(), [&](const ScintillaCommand& c) { return c.command == command; });
        if (def == commands.end())
            return false;
        const auto it = _shortcutConfig.scintilla.find(command);
        const std::vector<KeyCombo> before = it != _shortcutConfig.scintilla.end() ? it->second : def->defaults;
        if (keys == def->defaults)
            _shortcutConfig.scintilla.erase(command);
        else
            _shortcutConfig.scintilla[command] = keys;
        applyScintillaKeys(command, before, keys);
        break;
    }
    }
    return writeShortcuts(shortcutsPath(), _shortcutConfig);
}

void MainWindow::createToolBar()
{
    // Iconos de las órdenes con botón y de algunas más (en los menús)
    static const std::pair<const char*, const char*> kIcons[] = {
        {"actionNew", "new"}, {"actionOpen", "open"}, {"actionSave", "save"}, {"actionSaveAll", "save-all"},
        {"actionClose", "close"}, {"actionCloseAll", "close-all"}, {"actionPrint", "print"}, {"actionCut", "cut"},
        {"actionCopy", "copy"}, {"actionPaste", "paste"}, {"actionUndo", "undo"}, {"actionRedo", "redo"},
        {"actionFind", "find"}, {"actionReplace", "replace"}, {"actionZoomIn", "zoom-in"}, {"actionZoomOut", "zoom-out"},
        {"actionWrap", "wrap"}, {"actionShowAll", "show-all"}, {"actionShowIndentGuides", "indent-guide"},
        {"actionDefineLanguage", "language"}, {"actionRecordMacro", "record"}, {"actionPlayMacro", "play"},
        {"actionRunMacro", "run-multiple"}, {"actionSaveMacro", "save-macro"}, {"actionPreferences", "preferences"},
        {"actionPluginsAdmin", "plugins"}, {"actionStyleConfigurator", "highlighting"}, {"actionShortcutMapper", "keyboard"},
        {"actionDocumentMap", "doc-map"}, {"actionDocumentList", "doc-list"}, {"actionFunctionList", "function-list"},
        {"actionFolderAsWorkspace", "folder-workspace"}, {"actionOpenFolderAsWorkspace", "folder-add"}, {"actionAbout", "about"}, {"actionFindInFiles", "find"},
    };
    for (const auto& [name, icon] : kIcons) {
        if (QAction* action = findChild<QAction*>(QString::fromLatin1(name)))
            icons::setIcon(action, QString::fromLatin1(icon));
    }

    // toolBarIcons de Notepad_plus.cpp: los mismos botones y grupos
    static const char* const kButtons[] = {
        "actionNew", "actionOpen", "actionSave", "actionSaveAll", "actionClose", "actionCloseAll", "actionPrint", nullptr,
        "actionCut", "actionCopy", "actionPaste", nullptr, "actionUndo", "actionRedo", nullptr, "actionFind",
        "actionReplace", nullptr, "actionZoomIn", "actionZoomOut", nullptr, "actionWrap", "actionShowAll",
        "actionShowIndentGuides", nullptr, "actionDefineLanguage", "actionDocumentMap", "actionDocumentList",
        "actionFunctionList", "actionFolderAsWorkspace", nullptr, "actionRecordMacro", "actionPlayMacro",
        "actionRunMacro", "actionSaveMacro",
    };
    _toolBar = addToolBar(tr("Toolbar"));
    _toolBar->setObjectName(QStringLiteral("mainToolBar"));
    _toolBar->setMovable(false);
    _toolBar->setFloatable(false);
    for (const char* name : kButtons) {
        if (!name)
            _toolBar->addSeparator();
        else if (QAction* action = findChild<QAction*>(QString::fromLatin1(name)))
            _toolBar->addAction(action);
    }
}

void MainWindow::createPanels()
{
    _functionParsers = std::make_unique<FunctionParsers>();
    _docListPanel = new DocumentListPanel;
    _docMapPanel = new DocumentMapPanel;
    _functionListPanel = new FunctionListPanel(*_functionParsers);
    _functionListPanel->setFunctionSource([this](std::vector<FunctionInfo>& found) {
        const SymbolCacheEntry* entry = functionsOf(currentBuffer(), editor());
        if (!entry || !entry->parsed)
            return false;
        found = entry->found;
        return true;
    });
    _folderPanel = new FileBrowserPanel;
    _regexTester = new RegexTesterPanel;
    _terminal = new TerminalPanel;
    _problems = new ProblemsPanel;
    _charPanel = new CharacterPanel;
    _clipboardPanel = new ClipboardHistoryPanel;
    // Dónde los pone N++: la lista de documentos y las carpetas a la izquierda; el
    // mapa y las funciones a la derecha
    const std::tuple<QWidget*, const char*, QString, Qt::DockWidgetArea> panels[] = {
        {_docMapPanel, "documentMap", tr("Document Map"), Qt::RightDockWidgetArea},
        {_docListPanel, "documentList", tr("Document List"), Qt::LeftDockWidgetArea},
        {_functionListPanel, "functionList", tr("Function List"), Qt::RightDockWidgetArea},
        {_folderPanel, "folderAsWorkspace", tr("Folder as Workspace"), Qt::LeftDockWidgetArea},
        {_regexTester, "regexTester", tr("Regex Tester"), Qt::BottomDockWidgetArea}, // no existe en N++
        {_terminal, "terminal", tr("Terminal"), Qt::BottomDockWidgetArea},           // no existe en N++
        {_problems, "problems", tr("Problems"), Qt::BottomDockWidgetArea},           // no existe en N++
        {_charPanel, "charPanel", tr("Character Panel"), Qt::RightDockWidgetArea},
        {_clipboardPanel, "clipboardHistory", tr("Clipboard History"), Qt::RightDockWidgetArea},
    };
    // <GUIConfig name="multiInst" … documentList="yes" documentMap="no" fuctionList="no" folderAsWorkspace="no">
    // (los nombres de N++, con su "fuctionList")
    auto configKey = [](const char* name) {
        return std::strcmp(name, "functionList") == 0 ? QStringLiteral("fuctionList") : QString::fromLatin1(name);
    };
    for (const auto& [widget, name, title, area] : panels) {
        auto* dock = new QDockWidget(title, this);
        dock->setObjectName(QString::fromLatin1(name) + QStringLiteral("Dock"));
        dock->setWidget(widget);
        addDockWidget(area, dock);
        dock->hide();
        dock->setVisible(_syntax.config().guiConfig(QStringLiteral("multiInst"), configKey(name)) == QLatin1String("yes"));
        connect(dock, &QDockWidget::visibilityChanged, this, [this, dock, key = configKey(name)] {
            if (isVisible()) {
                _syntax.config().setGuiConfig(QStringLiteral("multiInst"), key,
                                              dock->isVisibleTo(this) ? QStringLiteral("yes") : QStringLiteral("no"));
                _syntax.config().save();
            }
            updatePanels();
        });
        _panelDocks.push_back(dock);
    }
    connect(_docListPanel, &DocumentListPanel::activated, this, [this](BufferId id, int view) {
        EditorPane* pane = view == 1 ? _subPane : _mainPane;
        pane->activateBuffer(id);
        setActivePane(pane);
        editor()->setFocus();
    });
    connect(_docListPanel, &DocumentListPanel::closeRequested, this, [this](BufferId id, int view) {
        EditorPane* pane = view == 1 ? _subPane : _mainPane;
        pane->activateBuffer(id);
        setActivePane(pane);
        closeBuffer(id);
    });
    // Editar › Panel de caracteres y Historial del portapapeles: lo elegido va al documento
    for (QObject* panel : {static_cast<QObject*>(_charPanel), static_cast<QObject*>(_clipboardPanel)}) {
        auto insert = [this](const QString& text) {
            insertText(text);
            editor()->setFocus();
        };
        if (auto* chars = qobject_cast<CharacterPanel*>(panel))
            connect(chars, &CharacterPanel::insertText, this, insert);
        else
            connect(static_cast<ClipboardHistoryPanel*>(panel), &ClipboardHistoryPanel::insertText, this, insert);
    }
    // El historial sigue el portapapeles desde que se abre por primera vez (como N++)
    connect(panelDock(QStringLiteral("clipboardHistory")), &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible)
            _clipboardPanel->startTracking();
    });
    if (!panelDock(QStringLiteral("clipboardHistory"))->isHidden())
        _clipboardPanel->startTracking();
    connect(_functionListPanel, &FunctionListPanel::goToPosition, this, [this](sptr_t pos) {
        editor()->send(SCI_GOTOPOS, static_cast<uptr_t>(pos));
        editor()->send(SCI_SCROLLCARET);
        editor()->setFocus();
    });
    connect(_folderPanel, &FileBrowserPanel::openFile, this, [this](const QString& path) { openFile(path); });
    connect(_folderPanel, &FileBrowserPanel::findInFolder, this, [this](const QString& dir) {
        findDialog()->setDirectory(dir);
        showFindDialog(FindReplaceDialog::Tab::FindInFiles);
    });
    connect(_folderPanel, &FileBrowserPanel::addFolderRequested, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Folder as Workspace"));
        if (!dir.isEmpty())
            addFolderToWorkspace(dir);
    });
    connect(_folderPanel, &FileBrowserPanel::locateRequested, this, [this] {
        if (const Buffer* buf = _documents.buffer(currentBuffer()); buf && !buf->isUntitled())
            _folderPanel->locate(buf->filePath());
    });
    connect(&_syntax, &SyntaxManager::stylesChanged, _docMapPanel, &DocumentMapPanel::restyle);

    // Terminal: al mostrarla sin ninguna (p. ej. al arrancar con el panel abierto), una nueva
    connect(panelDock(QStringLiteral("terminal")), &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible && _terminal->count() == 0)
            QTimer::singleShot(0, this, [this] {
                if (_terminal->count() == 0 && panelDock(QStringLiteral("terminal"))->isVisibleTo(this))
                    openTerminal();
            });
    });
    connect(_terminal, &TerminalPanel::newTerminalRequested, this, [this] { openTerminal(terminalDirectory()); });
    connect(_terminal, &TerminalPanel::lastTerminalClosed, this, [this] {
        panelDock(QStringLiteral("terminal"))->hide();
        editor()->setFocus();
    });
    connect(&_syntax, &SyntaxManager::stylesChanged, this, [this] { QTimer::singleShot(0, this, [this] { applyTerminalStyle(); }); });
    connect(&_syntax, &SyntaxManager::stylesChanged, this, [this] {
        _spellStyles.clear(); // otros nombres de estilo
        for (EditorPane* pane : {_mainPane, _subPane})
            scheduleSpelling(pane);
    });
    connect(_folderPanel, &FileBrowserPanel::openTerminal, this, [this](const QString& dir) { openTerminal(dir); });
    connect(_problems, &ProblemsPanel::activated, this, &MainWindow::goToLspPosition);
    // Git en el explorador: el estado de cada archivo, al cambiar algo
    _gitStatus = new GitStatus(this);
    _gitStatusTimer = new QTimer(this);
    _gitStatusTimer->setSingleShot(true);
    _gitStatusTimer->setInterval(400);
    connect(_gitStatusTimer, &QTimer::timeout, this, [this] {
        _gitStatus->refresh(_prefs.gitExplorer ? _folderPanel->roots() : QStringList());
    });
    _folderPanel->setGitStatus(_gitStatus);
    connect(_folderPanel, &FileBrowserPanel::folderChanged, this, &MainWindow::scheduleGitStatus);

    // Probador de regex: el texto de prueba sale de la selección o del documento
    _regexTester->applyStyles(_syntax);
    connect(&_syntax, &SyntaxManager::stylesChanged, _regexTester, [this] { _regexTester->applyStyles(_syntax); });
    connect(_regexTester, &RegexTesterPanel::sampleFromEditorRequested, this, [this] { fillRegexSample(); });
    connect(_regexTester, &RegexTesterPanel::useInFindRequested, this, [this](const SearchOptions& o) {
        _findBarUsedLast = false;
        FindReplaceDialog* dialog = findDialog();
        dialog->setOptions(o);
        dialog->showTab(o.replacement.empty() ? FindReplaceDialog::Tab::Find : FindReplaceDialog::Tab::Replace,
                        QString::fromStdString(o.pattern));
    });
}

// --- Comparar --------------------------------------------------------------------

BufferId MainWindow::openReference(EditorPane* pane, const QString& name, const std::string& utf8, const Buffer& like)
{
    // Un documento sin archivo, de solo lectura y con el lenguaje del original
    const LangType lang = like.language();
    const QString userLanguage = like.userLanguage();
    const BufferId id = _documents.newEmptyDocument();
    pane->addBuffer(id); // en esa vista aunque esté vacía (setActivePane no cambia a una vacía)
    pane->activateBuffer(id);
    updatePaneVisibility();
    _documents.setUntitledName(id, name);
    EditorView* v = pane->editor();
    v->send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(utf8.c_str()));
    v->sci(SCI_SETSAVEPOINT);
    clearChangeHistory(*v); // el texto no es un cambio: sin marcas de «guardado» en el margen
    if (Buffer* buf = _documents.buffer(id)) {
        if (lang == LangType::User)
            buf->setUserLanguage(userLanguage);
        else if (lang == LangType::External)
            buf->setExternalLanguage(userLanguage);
        else
            buf->setLanguage(lang);
        buf->setUserReadOnly(true);
    }
    return id;
}

bool MainWindow::startCompare(CompareWith with)
{
    stopCompare();
    EditorPane* current = _activePane;
    EditorPane* other = otherPane(current);
    const BufferId id = current->currentBuffer();
    const Buffer* buf = _documents.buffer(id);
    if (!buf)
        return false;
    EditorPane* oldPane = nullptr;
    EditorPane* newPane = nullptr;
    BufferId temp = kInvalidBuffer;
    if (with == CompareWith::OtherView) {
        // Con una sola vista: el documento pasa a la otra y se compara con la pestaña que queda
        if (other->count() == 0) {
            if (current->count() < 2) {
                _prompter->warn(this, tr("Compare"), tr("Open two documents (or one in each view) to compare them."));
                return false;
            }
            moveToOtherView();
        }
        oldPane = _mainPane;
        newPane = _subPane;
    } else {
        if (buf->isUntitled()) {
            _prompter->warn(this, tr("Compare"), tr("The document has not been saved to a file yet."));
            return false;
        }
        const QString path = buf->filePath();
        QByteArray bytes;
        QString suffix;
        if (with == CompareWith::Saved) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                _prompter->warn(this, tr("Compare"), tr("Could not read %1").arg(path));
                return false;
            }
            bytes = file.readAll();
            suffix = tr("saved");
        } else {
            QProcess git;
            git.start(QStringLiteral("git"), {QStringLiteral("-C"), QFileInfo(path).absolutePath(), QStringLiteral("cat-file"),
                                              QStringLiteral("blob"), QStringLiteral("HEAD:./") + QFileInfo(path).fileName()});
            if (!git.waitForFinished(10000) || git.exitStatus() != QProcess::NormalExit || git.exitCode() != 0) {
                _prompter->warn(this, tr("Compare"), tr("%1 has no version in Git (HEAD).").arg(QFileInfo(path).fileName()));
                return false;
            }
            bytes = git.readAllStandardOutput();
            suffix = QStringLiteral("HEAD");
        }
        std::string utf8 = decodeAll(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())), buf->encoding());
        if (utf8.starts_with("\xEF\xBB\xBF"))
            utf8.erase(0, 3);
        temp = openReference(other, QStringLiteral("%1 (%2)").arg(buf->displayName(), suffix), utf8, *buf);
        oldPane = other;
        newPane = current;
        setActivePane(current);
    }
    _compareBuffers[0] = oldPane->currentBuffer();
    _compareBuffers[1] = newPane->currentBuffer();
    _compareTemp = temp;
    _compare = std::make_unique<CompareSession>(oldPane->editor(), newPane->editor());
    connect(_compare.get(), &CompareSession::updated, this, &MainWindow::compareChanged);
    // Otro tema: los colores y las máscaras de los márgenes, después de aplicarlo a las vistas
    connect(&_syntax, &SyntaxManager::stylesChanged, _compare.get(), [session = _compare.get()] {
        QTimer::singleShot(0, session, &CompareSession::recompute);
    });
    _compare->setOptions({_compareIgnoreSpaces, _compareIgnoreCase}); // (compara)
    newPane->editor()->setFocus();
    return true;
}

void MainWindow::compareChanged()
{
    if (!_compare)
        return;
    const CompareSession::Stats s = _compare->stats();
    if (s.identical())
        statusBar()->showMessage(tr("Compare: the documents are identical"), 8000);
    else
        statusBar()->showMessage(tr("Compare: %1 changed, %2 added, %3 removed lines in %n block(s)", nullptr, s.blocks)
                                     .arg(s.changed).arg(s.added).arg(s.removed),
                                 8000);
    for (const QPointer<ScrollBarMarks>& marks : _scrollMarks) {
        if (marks)
            marks->scheduleUpdate();
    }
}

void MainWindow::stopCompare()
{
    if (!_compare)
        return;
    std::unique_ptr<CompareSession> session = std::move(_compare);
    session->disconnect(this);
    // Las marcas viven en los documentos: se quitan aunque ya no estén a la vista
    for (const BufferId id : _compareBuffers) {
        if (const Buffer* buf = _documents.buffer(id))
            withSearchView(*buf, [](ScintillaEditBase& view) { CompareSession::clearDecorations(view); });
    }
    session.reset();
    const BufferId temp = _compareTemp;
    _compareBuffers[0] = _compareBuffers[1] = _compareTemp = kInvalidBuffer;
    if (temp != kInvalidBuffer && _documents.buffer(temp))
        closeBuffer(temp); // la referencia (sin cambios: no pregunta)
    for (const QPointer<ScrollBarMarks>& marks : _scrollMarks) {
        if (marks)
            marks->scheduleUpdate();
    }
}

QuickPicker* MainWindow::ensurePicker()
{
    if (!_picker)
        _picker = new QuickPicker(this);
    // La lista es de todos: lo que escuchaba la de símbolos del proyecto, fuera
    disconnect(_symbolQuery);
    delete _workspaceSymbolTimer;
    return _picker;
}

void MainWindow::scheduleLspDecorations(int index)
{
    if (_lspDecorationTimer[index])
        _lspDecorationTimer[index]->start();
}

void MainWindow::updateLspDecorations(int index)
{
    EditorPane* pane = index == 0 ? _mainPane : _subPane;
    EditorView* v = pane->editor();
    const BufferId id = pane->currentBuffer();
    if (!v || id == kInvalidBuffer)
        return;
    // La bombilla: si hay acciones de código en la línea del cursor
    const auto caretLine = static_cast<int>(v->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(v->sci(SCI_GETCURRENTPOS))));
    QPointer<EditorView> target(v);
    const bool asked = _lsp->codeActions(*v, id, v->sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(caretLine)),
                                         v->sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(caretLine)),
                                         [this, target, index, caretLine, id](const QJsonArray& actions, lsp::LspClient*) {
        if (!target || (index == 0 ? _mainPane : _subPane)->currentBuffer() != id)
            return;
        target->sci(SCI_MARKERDELETEALL, kLightbulbMarker);
        _lightbulbLine[index] = actions.isEmpty() ? -1 : caretLine;
        if (actions.isEmpty())
            return;
        // Una bombilla amarilla (del tamaño de la línea)
        const int size = std::max(12, static_cast<int>(target->sci(SCI_TEXTHEIGHT, 0)) - 2);
        QImage image(size, size, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0xF2, 0xC2, 0x1B));
        painter.drawEllipse(QRectF(size * 0.2, size * 0.05, size * 0.6, size * 0.6));
        painter.setBrush(QColor(0x80, 0x80, 0x80));
        painter.drawRoundedRect(QRectF(size * 0.35, size * 0.62, size * 0.3, size * 0.3), 1.5, 1.5);
        painter.end();
        target->sci(SCI_RGBAIMAGESETWIDTH, static_cast<uptr_t>(size));
        target->sci(SCI_RGBAIMAGESETHEIGHT, static_cast<uptr_t>(size));
        target->sci(SCI_MARKERDEFINERGBAIMAGE, kLightbulbMarker, reinterpret_cast<sptr_t>(image.constBits()));
        target->sci(SCI_MARKERADD, static_cast<uptr_t>(caretLine), kLightbulbMarker);
    });
    if (!asked) {
        v->sci(SCI_MARKERDELETEALL, kLightbulbMarker);
        _lightbulbLine[index] = -1;
    }
    // Las pistas de las líneas a la vista (y unas cuantas más), al final de cada línea
    EolAnnotations* eol = EolAnnotations::of(v);
    if (!_prefs.inlayHints) {
        eol->clear(EolAnnotations::Hints);
        return;
    }
    const auto first = static_cast<int>(v->sci(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(v->sci(SCI_GETFIRSTVISIBLELINE))));
    const int last = first + static_cast<int>(v->sci(SCI_LINESONSCREEN)) + 5;
    _lsp->inlayHints(*v, id, first, last, [target](const QMap<int, QString>& hints) {
        if (!target)
            return;
        EolAnnotations* annotations = EolAnnotations::of(target);
        annotations->clear(EolAnnotations::Hints);
        for (auto it = hints.begin(); it != hints.end(); ++it)
            annotations->set(EolAnnotations::Hints, it.key(), it.value());
    });
}

void MainWindow::showCodeActions()
{
    EditorView* v = editor();
    sptr_t start = v->sci(SCI_GETSELECTIONSTART);
    sptr_t end = v->sci(SCI_GETSELECTIONEND);
    if (start == end) { // sin selección: la línea
        const sptr_t line = v->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(start));
        start = v->sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
        end = v->sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line));
    }
    const bool asked = _lsp->codeActions(*v, currentBuffer(), start, end, [this](const QJsonArray& actions, lsp::LspClient* client) {
        if (actions.isEmpty()) {
            statusBar()->showMessage(tr("No code actions here"), 4000);
            return;
        }
        // Las preferidas primero (y marcadas)
        std::vector<QuickPicker::Item> items;
        for (int pass = 0; pass < 2; ++pass) {
            for (const QJsonValue& value : actions) {
                const QJsonObject action = value.toObject();
                const bool preferred = action.value(QStringLiteral("isPreferred")).toBool();
                if (preferred != (pass == 0))
                    continue;
                items.push_back({action.value(QStringLiteral("title")).toString(), action.value(QStringLiteral("kind")).toString(), preferred,
                                 [this, client, action] { _lsp->runCodeAction(client, action); }});
            }
        }
        ensurePicker()->popup(editor(), tr("Code actions"), items);
    });
    if (!asked)
        statusBar()->showMessage(tr("The language server of this document has no code actions"), 5000);
}

void MainWindow::showWorkspaceSymbols()
{
    const BufferId id = currentBuffer();
    QuickPicker* picker = ensurePicker();
    auto* timer = new QTimer(picker);
    _workspaceSymbolTimer = timer;
    timer->setSingleShot(true);
    timer->setInterval(200);
    auto query = [this, picker, id] {
        const bool asked = _lsp->workspaceSymbols(id, picker->input()->text().trimmed(), [this, picker](const std::vector<LspManager::SymbolHit>& hits) {
            if (!picker->isVisible())
                return;
            std::vector<QuickPicker::Item> items;
            for (const LspManager::SymbolHit& h : hits) {
                QString detail = QStringLiteral("%1:%2").arg(QFileInfo(h.path).fileName()).arg(h.line + 1);
                if (!h.container.isEmpty())
                    detail = h.container + QStringLiteral("  ·  ") + detail;
                items.push_back({h.name, detail, false, [this, h] { goToLocation(h.path, h.line, h.character, h.encoding); }});
            }
            picker->setItems(std::move(items));
        });
        if (!asked)
            statusBar()->showMessage(tr("No language server can search the symbols of the project"), 5000);
    };
    connect(timer, &QTimer::timeout, picker, query);
    _symbolQuery = connect(picker, &QuickPicker::queryChanged, timer, [timer] { timer->start(); });
    picker->popup(editor(), tr("Symbol in the project"), {});
    query();
}

void MainWindow::updateSnippetContext(EditorPane* pane)
{
    SnippetController* controller = _snippetControllers[pane == _mainPane ? 0 : 1];
    const Buffer* buf = _documents.buffer(pane->currentBuffer());
    if (!controller)
        return;
    if (!buf)
        controller->setContext(QString(), QString());
    else
        controller->setContext(SnippetLibrary::languageKey(static_cast<int>(buf->language()), buf->userLanguage()),
                               buf->isUntitled() ? QString() : buf->filePath());
}

SnippetController* MainWindow::snippetControllerOfCurrentView() const
{
    return _snippetControllers[_activePane == _mainPane ? 0 : 1];
}

void MainWindow::showSnippetPicker()
{
    SnippetController* controller = snippetControllerOfCurrentView();
    if (!controller)
        return;
    const auto definitions = _snippets->forLanguage(controller->language());
    if (definitions.empty()) {
        statusBar()->showMessage(tr("There are no snippets for this language"), 5000);
        return;
    }
    // Lo seleccionado se sustituye por el fragmento (y es $TM_SELECTED_TEXT)
    std::vector<QuickPicker::Item> items;
    for (const snippet::Definition& d : definitions) {
        items.push_back({QStringLiteral("%1  —  %2").arg(d.prefix, d.description.isEmpty() ? d.name : d.description), d.name, false,
                         [this, body = d.body] {
                             EditorView* v = editor();
                             const sptr_t start = v->sci(SCI_GETSELECTIONSTART);
                             const sptr_t end = v->sci(SCI_GETSELECTIONEND);
                             const auto* raw = reinterpret_cast<const char*>(v->sci(SCI_GETRANGEPOINTER, static_cast<uptr_t>(start), end - start));
                             const QString selected = raw ? QString::fromUtf8(raw, static_cast<qsizetype>(end - start)) : QString();
                             if (SnippetController* c = snippetControllerOfCurrentView())
                                 c->insert(body, start, end, selected);
                             v->setFocus();
                         }});
    }
    ensurePicker()->popup(editor(), tr("Insert snippet"), items);
}

void MainWindow::editUserSnippets()
{
    SnippetController* controller = snippetControllerOfCurrentView();
    const QString language = controller && !controller->language().isEmpty() ? controller->language() : QStringLiteral("global");
    openFile(_snippets->ensureUserFile(language));
}

void MainWindow::scheduleGitStatus()
{
    if (_gitStatusTimer)
        _gitStatusTimer->start();
}

void MainWindow::attachToLsp(BufferId id)
{
    _lsp->setWorkspaceRoots(_folderPanel ? _folderPanel->roots() : QStringList());
    _lsp->attach(id);
    updateProblemsField();
}

void MainWindow::updateDiagnostics()
{
    for (EditorPane* pane : {_mainPane, _subPane}) {
        if (pane->currentBuffer() != kInvalidBuffer)
            _lsp->paintDiagnostics(*pane->editor(), pane->currentBuffer());
    }
    if (_problems)
        _problems->setDiagnostics(_lsp->allDiagnostics());
    updateProblemsField();
}

void MainWindow::updateProblemsField()
{
    const int errors = _lsp->count(1);
    const int warnings = _lsp->count(2);
    QString tip = _lsp->statusText(currentBuffer());
    tip = tip.isEmpty() ? tr("Problems (no language server for this document)") : tr("Problems — language server: %1").arg(tip);
    _statusBar->setProblems(_lsp->isEnabled() && (_lsp->hasClients() || errors + warnings > 0), errors, warnings, tip);
}

void MainWindow::goToLspPosition(const QString& path, int line, int character)
{
    goToLocation(path, line, character, _lsp->encodingForPath(path));
}

void MainWindow::goToLocation(const QString& path, int line, int character, lsp::PositionEncoding encoding)
{
    if (openFile(path) == kInvalidBuffer)
        return;
    EditorView* v = editor();
    const sptr_t pos = _lsp->scintillaPosition(*v, encoding, line, character);
    // La línea, a un tercio de la altura (como al saltar a un resultado)
    const sptr_t visible = v->sci(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(line));
    v->sci(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(std::max<sptr_t>(0, visible - v->sci(SCI_LINESONSCREEN) / 3)));
    v->sci(SCI_GOTOPOS, static_cast<uptr_t>(pos));
    v->setFocus();
}

void MainWindow::goToDefinition()
{
    EditorView* v = editor();
    const bool asked = _lsp->findLocations(*v, currentBuffer(), v->sci(SCI_GETCURRENTPOS), QStringLiteral("textDocument/definition"),
                                           [this](const std::vector<LspManager::Location>& locations, lsp::PositionEncoding encoding) {
        if (locations.empty()) {
            statusBar()->showMessage(tr("No definition found"), 5000);
            return;
        }
        if (locations.size() == 1) {
            goToLocation(locations.front().path, locations.front().line, locations.front().character, encoding);
            return;
        }
        // Varias: una lista para elegir
        std::vector<QuickPicker::Item> items;
        for (const LspManager::Location& l : locations) {
            items.push_back({QStringLiteral("%1:%2").arg(QFileInfo(l.path).fileName()).arg(l.line + 1), QDir::toNativeSeparators(l.path), false,
                             [this, l, encoding] { goToLocation(l.path, l.line, l.character, encoding); }});
        }
        ensurePicker()->popup(editor(), tr("Go to definition"), items);
    });
    if (!asked)
        statusBar()->showMessage(tr("The language server of this document cannot find definitions"), 5000);
}

void MainWindow::findReferences()
{
    EditorView* v = editor();
    const sptr_t caret = v->sci(SCI_GETCURRENTPOS);
    const sptr_t wordStart = v->sci(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(caret), 1);
    const sptr_t wordEnd = v->sci(SCI_WORDENDPOSITION, static_cast<uptr_t>(caret), 1);
    const auto* raw = reinterpret_cast<const char*>(v->sci(SCI_GETRANGEPOINTER, static_cast<uptr_t>(wordStart), wordEnd - wordStart));
    const QString word = raw ? QString::fromUtf8(raw, static_cast<qsizetype>(wordEnd - wordStart)) : QString();
    const bool asked = _lsp->findLocations(*v, currentBuffer(), caret, QStringLiteral("textDocument/references"),
                                           [this, word](const std::vector<LspManager::Location>& locations, lsp::PositionEncoding encoding) {
        // En el panel de resultados de búsqueda, por archivo y línea (como «Buscar todo»)
        std::map<QString, std::vector<LspManager::Location>> byFile;
        for (const LspManager::Location& l : locations)
            byFile[l.path].push_back(l);
        SearchOptions options;
        options.pattern = word.toStdString();
        _results->beginSearch(tr("References to %1").arg(word));
        int total = 0;
        for (auto& [path, list] : byFile) {
            std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return std::tie(a.line, a.character) < std::tie(b.line, b.character); });
            // El texto: el del documento abierto o el del archivo
            const BufferId id = _documents.findByPath(path);
            QByteArray text;
            if (id != kInvalidBuffer) {
                text = QByteArray::fromStdString(_documents.text(id));
            } else {
                QFile file(path);
                if (file.open(QIODevice::ReadOnly))
                    text = file.readAll();
            }
            std::vector<SciPos> lineStarts{0};
            for (qsizetype i = 0; i < text.size(); ++i) {
                if (text[i] == '\n')
                    lineStarts.push_back(static_cast<SciPos>(i + 1));
            }
            auto lineText = [&](int line) {
                if (line < 0 || line >= static_cast<int>(lineStarts.size()))
                    return QByteArray();
                const SciPos start = lineStarts[static_cast<std::size_t>(line)];
                const SciPos end = line + 1 < static_cast<int>(lineStarts.size()) ? lineStarts[static_cast<std::size_t>(line) + 1] - 1 : text.size();
                QByteArray t = text.mid(start, end - start);
                if (t.endsWith('\r'))
                    t.chop(1);
                return t;
            };
            std::vector<LineHit> hits;
            for (const LspManager::Location& l : list) {
                const QByteArray t = lineText(l.line);
                const int from = lsp::byteColumn(t, l.character, encoding);
                const int to = l.endLine == l.line ? lsp::byteColumn(t, l.endCharacter, encoding) : static_cast<int>(t.size());
                if (hits.empty() || hits.back().line != l.line) {
                    LineHit hit;
                    hit.line = l.line;
                    hit.text = t.left(static_cast<qsizetype>(kResultLineMaxLength)).toStdString();
                    hits.push_back(std::move(hit));
                }
                const SciPos lineStart = l.line < static_cast<int>(lineStarts.size()) ? lineStarts[static_cast<std::size_t>(l.line)] : 0;
                hits.back().segments.emplace_back(from, std::max(from, to));
                hits.back().ranges.emplace_back(lineStart + from, lineStart + std::max(from, to));
            }
            _results->addFile(QDir::toNativeSeparators(path), path, id, hits, static_cast<int>(list.size()));
            total += static_cast<int>(list.size());
        }
        _results->endSearch(total, static_cast<int>(byFile.size()), options);
        _results->show();
        statusBar()->showMessage(tr("%n reference(s)", nullptr, total), 5000);
    });
    if (!asked)
        statusBar()->showMessage(tr("The language server of this document cannot find references"), 5000);
}

void MainWindow::renameSymbol()
{
    EditorView* v = editor();
    const BufferId id = currentBuffer();
    if (!_lsp->supports(id, QStringLiteral("renameProvider"))) {
        statusBar()->showMessage(tr("The language server of this document cannot rename"), 5000);
        return;
    }
    const sptr_t caret = v->sci(SCI_GETCURRENTPOS);
    const sptr_t wordStart = v->sci(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(caret), 1);
    const sptr_t wordEnd = v->sci(SCI_WORDENDPOSITION, static_cast<uptr_t>(caret), 1);
    const auto* raw = reinterpret_cast<const char*>(v->sci(SCI_GETRANGEPOINTER, static_cast<uptr_t>(wordStart), wordEnd - wordStart));
    const QString old = raw ? QString::fromUtf8(raw, static_cast<qsizetype>(wordEnd - wordStart)) : QString();
    QString name = old;
    if (!_prompter->askText(this, tr("Rename Symbol"), tr("New name for %1:").arg(old), name) || name.trimmed().isEmpty() || name == old)
        return;
    _lsp->rename(*v, id, caret, name.trimmed(), [this](const QJsonObject& edit, lsp::PositionEncoding encoding, const QString& error) {
        if (!error.isEmpty()) {
            _prompter->warn(this, tr("Rename Symbol"), error);
            return;
        }
        const int changed = applyWorkspaceEdit(edit, encoding);
        if (changed >= 0)
            statusBar()->showMessage(tr("Renamed in %n file(s)", nullptr, changed), 5000);
    });
}

void MainWindow::formatDocument()
{
    // Primero la herramienta del lenguaje (Preferencias › Formato); si no hay, el servidor
    const BufferId id = currentBuffer();
    QString error;
    switch (formatWithTool(*editor(), id, true, &error)) {
    case FormatOutcome::Formatted:
    case FormatOutcome::Unchanged:
        return;
    case FormatOutcome::Failed:
        _prompter->warn(this, tr("Format Document"), error);
        return;
    case FormatOutcome::NoTool:
        break;
    }
    if (_lsp->format(*editor(), id))
        return;
    const Buffer* buf = _documents.buffer(id);
    const FormatterSpec* spec = buf ? formatterForLanguage(lsp::languageIdFor(buf->language(), buf->userLanguage(), buf->filePath()))
                                    : nullptr;
    statusBar()->showMessage(
        spec ? tr("No formatter for this document: install %1 or choose one in Preferences › Formatting")
                   .arg(QProcess::splitCommand(spec->candidates.front()).value(0))
             : tr("No formatter for this document"),
        8000);
}

MainWindow::FormatOutcome MainWindow::formatWithTool(EditorView& view, BufferId id, bool useSelection, QString* error)
{
    const Buffer* buf = _documents.buffer(id);
    if (!buf)
        return FormatOutcome::NoTool;
    const FormatterSpec* spec = formatterForLanguage(lsp::languageIdFor(buf->language(), buf->userLanguage(), buf->filePath()));
    const auto formatter = spec ? resolveFormatter(*spec, lsp::parseOverrides(_prefs.formatters)) : std::nullopt;
    if (!formatter)
        return FormatOutcome::NoTool;
    FormatRequest request;
    request.filePath = buf->isUntitled() ? QString() : buf->filePath();
    request.tabSize = static_cast<int>(view.sci(SCI_GETTABWIDTH));
    request.useTabs = view.sci(SCI_GETUSETABS) != 0;
    // Solo las líneas seleccionadas, si la herramienta sabe (clang-format)
    const sptr_t selStart = view.sci(SCI_GETSELECTIONSTART);
    const sptr_t selEnd = view.sci(SCI_GETSELECTIONEND);
    if (useSelection && selEnd > selStart && formatter->supportsLines) {
        request.firstLine = static_cast<int>(view.sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(selStart))) + 1;
        request.lastLine = static_cast<int>(view.sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(selEnd))) + 1;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const FormatResult result = runFormatter(*formatter, QByteArray::fromStdString(_documents.text(id)), request);
    QApplication::restoreOverrideCursor();
    const QString tool = QFileInfo(formatter->program).fileName();
    if (!result.ok) {
        if (error)
            *error = tr("%1 could not format the document:\n%2").arg(tool, result.error);
        return FormatOutcome::Failed;
    }
    // El cursor, en la misma línea y columna (lo más parecido); solo cambian las líneas distintas
    const sptr_t caret = view.sci(SCI_GETCURRENTPOS);
    const sptr_t line = view.sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(caret));
    const sptr_t column = view.sci(SCI_GETCOLUMN, static_cast<uptr_t>(caret));
    const sptr_t firstVisible = view.sci(SCI_GETFIRSTVISIBLELINE);
    const int changed = replaceChangedLines(view, std::string_view(result.output.constData(), static_cast<std::size_t>(result.output.size())));
    if (changed == 0) {
        statusBar()->showMessage(tr("Already formatted (%1)").arg(tool), 5000);
        return FormatOutcome::Unchanged;
    }
    view.sci(SCI_GOTOPOS, static_cast<uptr_t>(view.sci(SCI_FINDCOLUMN, static_cast<uptr_t>(line), column)));
    view.sci(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(firstVisible));
    statusBar()->showMessage(tr("Formatted with %1").arg(tool), 5000);
    return FormatOutcome::Formatted;
}

void MainWindow::formatBeforeSave(BufferId id)
{
    // Preferencias › Formato › Al guardar: con la herramienta, si el documento está a la vista
    // (el de una vista que no se ve se guarda tal cual); si falla, se guarda igual
    if (!_prefs.formatOnSave)
        return;
    for (EditorPane* pane : {_mainPane, _subPane}) {
        if (pane->currentBuffer() != id)
            continue;
        QString error;
        if (formatWithTool(*pane->editor(), id, false, &error) == FormatOutcome::Failed)
            statusBar()->showMessage(error.section(QLatin1Char('\n'), 0, 0), 8000);
        return;
    }
}

int MainWindow::applyWorkspaceEdit(const QJsonObject& edit, lsp::PositionEncoding encoding)
{
    // «documentChanges» (TextDocumentEdit) si viene; si no, «changes» (uri → TextEdit[])
    std::vector<std::pair<QString, QJsonArray>> files;
    int unsupported = 0;
    if (edit.contains(QStringLiteral("documentChanges"))) {
        for (const QJsonValue& value : edit.value(QStringLiteral("documentChanges")).toArray()) {
            const QJsonObject change = value.toObject();
            if (change.contains(QStringLiteral("kind"))) { // crear, renombrar o borrar archivos
                ++unsupported;
                continue;
            }
            files.emplace_back(lsp::pathFromUri(change.value(QStringLiteral("textDocument")).toObject().value(QStringLiteral("uri")).toString()),
                               change.value(QStringLiteral("edits")).toArray());
        }
    } else {
        const QJsonObject changes = edit.value(QStringLiteral("changes")).toObject();
        for (auto it = changes.begin(); it != changes.end(); ++it)
            files.emplace_back(lsp::pathFromUri(it.key()), it.value().toArray());
    }
    if (unsupported > 0)
        showWarnings(tr("Language Server"), {tr("%n file operation(s) not applied (create, rename or delete files)", nullptr, unsupported)});
    // Los abiertos se cambian sin cambiar de pestaña; los demás se abren (quedan sin guardar)
    const BufferId original = currentBuffer();
    EditorPane* originalPane = _activePane;
    int changed = 0;
    for (const auto& [path, edits] : files) {
        if (path.isEmpty() || edits.isEmpty())
            continue;
        BufferId id = _documents.findByPath(path);
        if (id == kInvalidBuffer)
            id = openFile(path);
        const Buffer* buf = _documents.buffer(id);
        if (!buf)
            continue;
        withSearchView(*buf, [&](ScintillaEditBase& view) { _lsp->applyTextEdits(view, encoding, edits); });
        ++changed;
    }
    if (original != kInvalidBuffer && currentBuffer() != original) {
        setActivePane(originalPane);
        originalPane->activateBuffer(original);
    }
    return changed;
}

QString MainWindow::terminalDirectory() const
{
    // La carpeta del documento; sin ella, la primera de trabajo; si no, la personal
    if (const Buffer* buf = _documents.buffer(currentBuffer()); buf && !buf->isUntitled())
        return QFileInfo(buf->filePath()).absolutePath();
    const QStringList roots = _folderPanel->roots();
    return roots.isEmpty() ? QDir::homePath() : roots.first();
}

void MainWindow::applyTerminalStyle()
{
    // La fuente y los colores del texto normal del editor (el tema activo)
    EditorView* v = editor();
    if (!v || !_terminal)
        return;
    QByteArray family(static_cast<qsizetype>(v->sci(SCI_STYLEGETFONT, STYLE_DEFAULT)) + 1, '\0');
    v->sci(SCI_STYLEGETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>(family.data()));
    QFont font(QString::fromUtf8(family.constData()));
    font.setPointSizeF(std::max(6.0, static_cast<double>(v->sci(SCI_STYLEGETSIZEFRACTIONAL, STYLE_DEFAULT)) / SC_FONT_SIZE_MULTIPLIER));
    _terminal->setTerminalFont(font);
    auto color = [](sptr_t c) {
        return QColor(static_cast<int>(c & 0xFF), static_cast<int>((c >> 8) & 0xFF), static_cast<int>((c >> 16) & 0xFF));
    };
    _terminal->setColors(color(v->sci(SCI_STYLEGETFORE, STYLE_DEFAULT)), color(v->sci(SCI_STYLEGETBACK, STYLE_DEFAULT)));
}

void MainWindow::openTerminal(const QString& directory)
{
    QDockWidget* dock = panelDock(QStringLiteral("terminal"));
    if (!directory.isEmpty() || _terminal->count() == 0) {
        _terminal->setShell(_prefs.terminalShell);
        applyTerminalStyle();
        _terminal->newTerminal(directory.isEmpty() ? terminalDirectory() : directory);
    }
    dock->show();
    dock->raise();
    if (TerminalView* view = _terminal->currentTerminal())
        view->setFocus();
}

void MainWindow::toggleTerminal()
{
    // Como en VS Code: abre, pasa el foco a la terminal o, si ya lo tiene, la oculta
    QDockWidget* dock = panelDock(QStringLiteral("terminal"));
    TerminalView* view = _terminal->currentTerminal();
    if (dock->isVisibleTo(this) && view && view->hasFocus()) {
        dock->hide();
        editor()->setFocus();
    } else {
        openTerminal();
    }
}

void MainWindow::runSelectionInTerminal()
{
    // La selección (o la línea del cursor), como si se tecleara, con Intro al final
    EditorView* v = editor();
    sptr_t start = v->sci(SCI_GETSELECTIONSTART);
    sptr_t end = v->sci(SCI_GETSELECTIONEND);
    if (start == end) {
        const sptr_t line = v->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(start));
        start = v->sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
        end = v->sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line));
    }
    const auto* range = reinterpret_cast<const char*>(v->sci(SCI_GETRANGEPOINTER, static_cast<uptr_t>(start), end - start));
    QString text = range ? QString::fromUtf8(range, static_cast<qsizetype>(end - start)) : QString();
    if (text.trimmed().isEmpty())
        return;
    if (!text.endsWith(QLatin1Char('\n')))
        text += QLatin1Char('\n');
    openTerminal();
    if (TerminalView* view = _terminal->currentTerminal())
        view->sendText(text);
    v->setFocus();
}

void MainWindow::fillRegexSample()
{
    EditorView* v = editor();
    constexpr sptr_t kMaxSample = 1024 * 1024;
    sptr_t start = v->sci(SCI_GETSELECTIONSTART);
    sptr_t end = v->sci(SCI_GETSELECTIONEND);
    if (start == end) {
        start = 0;
        end = v->sci(SCI_GETLENGTH);
    }
    if (end - start > kMaxSample) // hasta el final de la línea en la que se pasa del límite
        end = v->sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(v->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(start + kMaxSample))));
    const auto* text = reinterpret_cast<const char*>(v->sci(SCI_GETRANGEPOINTER, static_cast<uptr_t>(start), end - start));
    _regexTester->setSample(text ? QString::fromUtf8(text, static_cast<qsizetype>(end - start)) : QString());
}

void MainWindow::showRegexTester()
{
    QDockWidget* dock = panelDock(QStringLiteral("regexTester"));
    const bool wasEmpty = _regexTester->sample().isEmpty();
    dock->show();
    dock->raise();
    // La primera vez, con lo seleccionado (o el documento) y la expresión del diálogo Buscar
    if (wasEmpty)
        fillRegexSample();
    if (_regexTester->patternField()->text().isEmpty() && _findDialog && _findDialog->options().mode == SearchMode::Regex)
        _regexTester->setPattern(QString::fromStdString(_findDialog->options().pattern));
    _regexTester->patternField()->setFocus();
}

QDockWidget* MainWindow::panelDock(const QString& name) const
{
    for (const QPointer<QDockWidget>& dock : _panelDocks) {
        if (dock && dock->objectName() == name + QStringLiteral("Dock"))
            return dock;
    }
    return nullptr;
}

bool MainWindow::addFolderToWorkspace(const QString& folder)
{
    const bool added = _folderPanel->addRoot(folder);
    scheduleGitStatus();
    panelDock(QStringLiteral("folderAsWorkspace"))->show();
    // Carpetas recientes (para la página de inicio), en config.xml
    const QString clean = absolutePath(folder);
    _recentFolders.removeAll(clean);
    _recentFolders.prepend(clean);
    while (_recentFolders.size() > 10)
        _recentFolders.removeLast();
    _syntax.config().setGuiConfig(QStringLiteral("EditaRecentFolders"), QStringLiteral("paths"), _recentFolders.join(QLatin1Char('\x1f')));
    _syntax.config().save();
    if (isWelcomeVisible())
        fillWelcomePage();
    for (int i = 0; i < 2; ++i)
        updateBreadcrumbs(i, false);
    return added;
}

void MainWindow::updatePanels()
{
    const auto shown = [this](const char* name) {
        const QDockWidget* dock = panelDock(QString::fromLatin1(name));
        return dock && dock->isVisibleTo(this);
    };
    if (!_docListPanel)
        return;
    // Lista de documentos: las pestañas de las dos vistas
    if (shown("documentList")) {
        std::vector<DocumentListPanel::Document> docs;
        for (EditorPane* pane : {_mainPane, _subPane}) {
            for (const BufferId id : pane->buffers()) {
                if (const Buffer* buf = _documents.buffer(id)) {
                    const DocTabBar::TabState state = buf->isReadOnly() ? DocTabBar::TabState::ReadOnly
                                                    : buf->isDirty()    ? DocTabBar::TabState::Dirty
                                                                        : DocTabBar::TabState::Saved;
                    docs.push_back({id, buf->displayName(), buf->isUntitled() ? QString() : buf->filePath(), state,
                                    pane == _subPane ? 1 : 0});
                }
            }
        }
        _docListPanel->setDocuments(docs, currentBuffer(), _activePane == _subPane ? 1 : 0);
    }
    const Buffer* buf = _documents.buffer(currentBuffer());
    // Mapa: sigue al editor activo
    if (shown("documentMap") && buf) {
        _docMapPanel->setEditor(editor(), [this, lang = buf->language(), udl = buf->userLanguage()](ScintillaEditBase& view) {
            _syntax.apply(view, lang, udl);
            // Las marcas en el margen del mapa (cambios de Git y marcadores) y las búsquedas
            // («Marcar todo», resaltado inteligente, buscar en línea) con un relleno que se ve
            // aunque el mapa sea tan pequeño
            auto bgr = [](const QColor& c) { return static_cast<sptr_t>(c.red() | (c.green() << 8) | (c.blue() << 16)); };
            if (GitChanges* git = gitChanges(_activePane)) {
                const std::pair<int, LineChangeKind> marks[] = {{GitChanges::kMarkerAdded, LineChangeKind::Added},
                                                                {GitChanges::kMarkerModified, LineChangeKind::Modified},
                                                                {GitChanges::kMarkerDeleted, LineChangeKind::Deleted}};
                for (const auto& [marker, kind] : marks) {
                    view.send(SCI_MARKERDEFINE, static_cast<uptr_t>(marker), SC_MARK_FULLRECT);
                    view.send(SCI_MARKERSETBACK, static_cast<uptr_t>(marker), bgr(git->color(kind)));
                }
            }
            view.send(SCI_MARKERDEFINE, kBookmarkMarker, SC_MARK_FULLRECT);
            view.send(SCI_MARKERSETBACK, kBookmarkMarker, bgr(QColor(0x2E, 0x8B, 0xE6)));
            for (const int indicator : {kIndicatorFound, kIndicatorSmart, kIndicatorIncremental}) {
                view.send(SCI_INDICSETSTYLE, static_cast<uptr_t>(indicator), INDIC_FULLBOX);
                view.send(SCI_INDICSETFORE, static_cast<uptr_t>(indicator), editor()->sci(SCI_INDICGETFORE, static_cast<uptr_t>(indicator)));
                view.send(SCI_INDICSETALPHA, static_cast<uptr_t>(indicator), 220);
                view.send(SCI_INDICSETOUTLINEALPHA, static_cast<uptr_t>(indicator), 255);
            }
        });
    }
    // Funciones del documento activo
    if (shown("functionList") && buf)
        _functionListPanel->setSource(editor(), buf->language(), buf->userLanguage(), buf->displayName());
}

void MainWindow::openFromOtherInstance(const InstanceRequest& request, bool raise)
{
    if (!request.session.isEmpty())
        loadSessionFile(request.session);
    openFiles(request.files, /*askToCreate*/ true);
    // -l: el lenguaje del último archivo (si se conoce)
    if (!request.language.isEmpty() && !request.files.isEmpty() && isKnownLanguageName(request.language.toStdString()))
        setLanguage(langTypeFromName(request.language.toStdString()));
    if (request.line > 0)
        goToLine(request.line, request.column > 0 ? request.column : 1);
    if (raise) {
        if (isMinimized())
            showNormal();
        show();
        this->raise();
        activateWindow();
    }
}

QString MainWindow::themePathFor(const QString& name, bool dark) const
{
    // DarkMode de N++ (sin nombre, DarkModeDefault.xml o stylers.xml); en Edita, sin
    // nombre son sus temas, que combinan con la interfaz
    const QString file = !name.isEmpty() ? name : dark ? QStringLiteral("Edita Dark.xml") : QStringLiteral("Edita Light.xml");
    if (file == QLatin1String("stylers.xml"))
        return {};
    for (const SyntaxManager::Theme& theme : _syntax.themes()) {
        if (!theme.path.isEmpty() && QFileInfo(theme.path).fileName() == file)
            return theme.path;
    }
    return {};
}

void MainWindow::applyAppearance()
{
    const bool dark = appearance::isDark(_prefs, appearance::systemScheme());
    static bool applied = false;
    if (!applied || dark != appearance::isDarkApplied()) {
        appearance::apply(dark);
        applied = true;
        // La página de inicio tiene hoja de estilo propia: no recibe el cambio de paleta
        if (_welcome)
            _welcome->refreshStyle();
    }
    const QString theme = themePathFor(dark ? _prefs.darkThemeName : _prefs.lightThemeName, dark);
    if (theme != _syntax.activeThemePath())
        _syntax.setTheme(theme);

    // Los lenguajes del usuario con versión clara y oscura (Markdown): la del modo
    _syntax.setDarkMode(dark);
    for (std::size_t i = 0; i < _documents.count(); ++i) {
        Buffer* buf = _documents.bufferAt(i);
        if (buf->language() != LangType::User || buf->isLanguageSetFromMenu() || buf->isUntitled())
            continue;
        const QString udl = _syntax.detectUserLanguage(buf->filePath());
        if (!udl.isEmpty() && udl != buf->userLanguage())
            buf->setUserLanguage(udl);
    }
}

QMenu* MainWindow::createEolMenu()
{
    auto* menu = new QMenu(tr("&EOL Conversion"), this);
    _eolGroup = new QActionGroup(this);
    const std::pair<EolType, QString> entries[] = {
        {EolType::Windows, tr("&Windows (CR LF)")},
        {EolType::Unix, tr("&Unix (LF)")},
        {EolType::MacOs, tr("&Macintosh (CR)")},
    };
    for (const auto& [eol, text] : entries) {
        QAction* action = menu->addAction(text);
        action->setCheckable(true);
        action->setActionGroup(_eolGroup);
        action->setObjectName(QStringLiteral("actionEol%1").arg(static_cast<int>(eol)));
        connect(action, &QAction::triggered, this, [this, eol] { convertEol(eol); });
        _eolActions[static_cast<int>(eol)] = action;
    }
    _eolMenu = menu;
    return menu;
}

QMenu* MainWindow::createEncodingMenu()
{
    auto* menu = new QMenu(tr("E&ncoding"), this); // como N++: Alt+C es el editor de columnas
    _encodingGroup = new QActionGroup(this);
    _encodingGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);

    // "Codificar en…" (en un sistema UTF-8, N++ no ofrece "ANSI")
    for (const UniMode mode : kMenuUnicodeModes) {
        QAction* action = menu->addAction(tr("Encode in %1").arg(displayName(TextEncoding::unicode(mode))));
        action->setCheckable(true);
        action->setActionGroup(_encodingGroup);
        action->setObjectName(QStringLiteral("actionEncode%1").arg(static_cast<int>(mode)));
        connect(action, &QAction::triggered, this, [this, mode] { encodeAs(mode); });
        _unicodeModeActions[static_cast<int>(mode)] = action;
    }

    QMenu* charsets = menu->addMenu(tr("Character &sets"));
    for (const CharsetCategory& category : charsetCategories()) {
        QMenu* sub = charsets->addMenu(tr(category.name));
        for (const int cp : category.codepages) {
            QAction* action = sub->addAction(displayName(TextEncoding::fromCodepage(cp)));
            action->setCheckable(true);
            action->setActionGroup(_encodingGroup);
            action->setObjectName(QStringLiteral("actionCharset%1").arg(cp));
            action->setEnabled(iconvNameForCodepage(cp) != nullptr);
            connect(action, &QAction::triggered, this, [this, cp] { reinterpretAs(cp); });
            _codepageActions[cp] = action;
        }
    }

    menu->addSeparator();
    for (const UniMode mode : kMenuUnicodeModes) {
        QAction* action = menu->addAction(tr("Convert to %1").arg(displayName(TextEncoding::unicode(mode))));
        action->setObjectName(QStringLiteral("actionConvert%1").arg(static_cast<int>(mode)));
        connect(action, &QAction::triggered, this, [this, mode] { convertTo(TextEncoding::unicode(mode)); });
    }
    // Añadido de Edita: N++ solo convierte a "ANSI" (el codepage del sistema),
    // que en Linux no existe; aquí se elige el juego de caracteres
    QMenu* convertCharsets = menu->addMenu(tr("Convert to another character set"));
    for (const CharsetCategory& category : charsetCategories()) {
        QMenu* sub = convertCharsets->addMenu(tr(category.name));
        for (const int cp : category.codepages) {
            QAction* action = sub->addAction(displayName(TextEncoding::fromCodepage(cp)));
            action->setEnabled(iconvNameForCodepage(cp) != nullptr);
            connect(action, &QAction::triggered, this, [this, cp] { convertTo(TextEncoding::fromCodepage(cp)); });
        }
    }
    return menu;
}

QMenu* MainWindow::createLanguageMenu()
{
    auto* menu = new QMenu(tr("&Language"), this);
    _languageMenu = menu;
    _languageGroup = new QActionGroup(this);
    _languageGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    populateLanguageMenu();
    return menu;
}

void MainWindow::populateLanguageMenu()
{
    // El menú Lenguaje de N++: compacto (agrupado por inicial) o con todos seguidos, y sin
    // los lenguajes ocultos en Preferencias › Lenguaje (langsExcluded)
    QMenu* menu = _languageMenu;
    for (QAction* action : _languageGroup->actions())
        _languageGroup->removeAction(action);
    _languageActions.clear();
    menu->clear();
    for (QMenu* sub : menu->findChildren<QMenu*>(Qt::FindDirectChildrenOnly))
        delete sub; // las letras y el de los lenguajes de usuario: se vuelven a hacer
    auto addLanguage = [this](QMenu* target, LangType lang, const QString& text) {
        QAction* action = target->addAction(text);
        action->setCheckable(true);
        action->setActionGroup(_languageGroup);
        action->setObjectName(QStringLiteral("actionLang%1").arg(static_cast<int>(lang)));
        connect(action, &QAction::triggered, this, [this, lang] { setLanguage(lang); });
        _languageActions[static_cast<int>(lang)] = action;
    };
    addLanguage(menu, LangType::Text, tr("None (Normal Text)"));
    menu->addSeparator();

    std::vector<std::pair<QString, LangType>> entries;
    for (int i = 0; i < kLangTypeCount; ++i) {
        const auto lang = static_cast<LangType>(i);
        const LanguageInfo& info = languageInfo(lang);
        // Los lenguajes definidos por el usuario (UDL) van en su submenú
        if (info.menuName && lang != LangType::Text && lang != LangType::User && !_prefs.excludedLanguages.contains(i))
            entries.emplace_back(lang == LangType::Pascal ? QStringLiteral("Pascal / Delphi") // su lexer es el de Delphi
                                                          : QString::fromUtf8(info.menuName),
                                 lang);
    }
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.first.compare(b.first, Qt::CaseInsensitive) < 0; });
    QMenu* letterMenu = nullptr;
    QChar letter;
    for (const auto& [name, lang] : entries) {
        if (!_prefs.langMenuCompact) {
            addLanguage(menu, lang, name);
            continue;
        }
        const QChar first = name.at(0).toUpper();
        if (!letterMenu || first != letter) {
            letter = first;
            letterMenu = menu->addMenu(QString(letter));
        }
        addLanguage(letterMenu, lang, name);
    }
    menu->addSeparator();
    _userLanguageMenu = menu->addMenu(tr("&User Defined Language"));
    _userLanguageMenu->setObjectName(QStringLiteral("menuUserLanguages"));
    connect(_userLanguageMenu, &QMenu::aboutToShow, this, &MainWindow::populateUserLanguageMenu);
    populateUserLanguageMenu();
    addExternalLanguageActions();
}

void MainWindow::populateThemeMenu(QMenu* menu)
{
    menu->clear();
    auto* group = new QActionGroup(menu);
    const QString active = _syntax.activeThemePath();
    for (const SyntaxManager::Theme& theme : _syntax.themes()) {
        QAction* action = menu->addAction(theme.name);
        action->setCheckable(true);
        action->setChecked(theme.path == active);
        action->setActionGroup(group);
        const QString path = theme.path;
        connect(action, &QAction::triggered, this, [this, path] { setTheme(path); });
    }
    menu->addSeparator();
    QAction* reload = menu->addAction(tr("&Reload langs.xml and Styles"), this, [this] { reloadSyntax(); });
    reload->setObjectName(QStringLiteral("actionReloadSyntax"));
    menu->addAction(tr("Edit the &Styles File"), this, [this] { openFile(_syntax.activeStylePath()); });
    menu->addAction(tr("Edit &langs.xml"), this, [this] { openFile(_syntax.langsPath()); });
}

QString MainWindow::fileFilterFor(LangType lang) const
{
    const LanguageDefinition* def = _syntax.languages().find(lang);
    if (!def || def->extensions.empty() || lang == LangType::Text)
        return {};
    QStringList patterns;
    for (const QString& ext : QString::fromStdString(def->extensions).split(QLatin1Char(' '), Qt::SkipEmptyParts))
        patterns << QStringLiteral("*.") + ext;
    return QStringLiteral("%1 (%2)").arg(QCoreApplication::translate("LangType", languageInfo(lang).longName),
                                         patterns.join(QLatin1Char(' ')));
}

QString MainWindow::fileFilters() const
{
    QStringList filters{tr("All files (*)")};
    std::vector<std::pair<QString, QString>> byName;
    for (const LanguageDefinition& def : _syntax.languages().languages()) {
        const QString filter = fileFilterFor(def.type);
        if (!filter.isEmpty())
            byName.emplace_back(QCoreApplication::translate("LangType", languageInfo(def.type).longName), filter);
    }
    std::sort(byName.begin(), byName.end(),
              [](const auto& a, const auto& b) { return a.first.compare(b.first, Qt::CaseInsensitive) < 0; });
    for (const auto& entry : byName)
        filters << entry.second;
    filters.removeDuplicates();
    return filters.join(QStringLiteral(";;"));
}

void MainWindow::setLanguage(LangType lang)
{
    if (Buffer* buf = _documents.buffer(currentBuffer()))
        buf->setLanguage(lang, /*fromMenu*/ true);
}

int MainWindow::langTypeForPlugins(const Buffer& buffer) const
{
    // L_EXTERNAL + el índice del lenguaje, como en N++
    if (buffer.language() == LangType::External) {
        const int i = _syntax.externalLexers().indexOf(buffer.userLanguage().toStdString());
        if (i >= 0)
            return static_cast<int>(LangType::External) + i;
    }
    return static_cast<int>(buffer.language());
}

void MainWindow::setExternalLanguage(const QString& name)
{
    if (Buffer* buf = _documents.buffer(currentBuffer()); buf && _syntax.externalLexers().find(name.toStdString()))
        buf->setExternalLanguage(name, /*fromMenu*/ true);
}

void MainWindow::registerExternalLexers()
{
    const QString configDir = QDir(paths::configDir()).filePath(QStringLiteral("plugins/config"));
    for (const auto& plugin : _plugins->plugins()) {
        if (plugin->lexerNames.empty() || !plugin->createLexer)
            continue;
        _pluginWarnings << _syntax.addExternalLexers(plugin->name, plugin->lexerNames,
                                                      reinterpret_cast<CreateLexerFunction>(plugin->createLexer),
                                                      findExternalLexerXml(configDir, plugin->path));
    }
    if (_syntax.externalLexers().empty())
        return;
    addExternalLanguageActions();
    // Los estilos del plugin ya están en la hoja: los documentos abiertos los usan
    _syntax.notifyUserLanguagesChanged();
}

void MainWindow::addExternalLanguageActions()
{
    // Como en N++, después de los lenguajes de Edita, por orden de carga
    _externalLanguageActions.clear();
    auto* menu = qobject_cast<QMenu*>(_userLanguageMenu->parent()); // el menú Lenguaje
    if (!menu || _syntax.externalLexers().empty())
        return;
    QAction* before = _userLanguageMenu->menuAction();
    for (const ExternalLanguage& lang : _syntax.externalLexers().languages()) {
        const QString name = QString::fromStdString(lang.name);
        auto* action = new QAction(name, menu);
        action->setCheckable(true);
        action->setActionGroup(_languageGroup);
        action->setObjectName(QStringLiteral("actionExternalLang:") + name); // su texto es del plugin
        action->setToolTip(tr("Language of the %1 plugin").arg(lang.pluginName));
        connect(action, &QAction::triggered, this, [this, name] { setExternalLanguage(name); });
        menu->insertAction(before, action);
        _externalLanguageActions[name] = action;
    }
    menu->insertSeparator(before);
}

void MainWindow::setUserLanguage(const QString& name)
{
    if (Buffer* buf = _documents.buffer(currentBuffer()); buf && _syntax.userLanguages().find(name))
        buf->setUserLanguage(name, /*fromMenu*/ true);
}

void MainWindow::populateUserLanguageMenu()
{
    _userLanguageMenu->clear();
    const Buffer* buf = _documents.buffer(currentBuffer());
    for (const UserLanguage& lang : _syntax.userLanguages().languages()) {
        QAction* action = _userLanguageMenu->addAction(lang.name);
        action->setCheckable(true);
        action->setChecked(buf && buf->language() == LangType::User && buf->userLanguage() == lang.name);
        connect(action, &QAction::triggered, this, [this, name = lang.name] { setUserLanguage(name); });
    }
    if (!_syntax.userLanguages().languages().empty())
        _userLanguageMenu->addSeparator();
    QAction* define = _userLanguageMenu->addAction(tr("&Define your language…"), this, &MainWindow::showUserLanguageDialog);
    define->setObjectName(QStringLiteral("actionDefineLanguage"));
    _userLanguageMenu->addAction(tr("&Import…"), this, [this] {
        for (const QString& file : _prompter->openFileNames(this, QDir::homePath(), tr("Notepad++ languages (*.xml)")))
            importUserLanguage(file);
    });
    _userLanguageMenu->addAction(tr("Open User Defined Language &folder"), this, [] {
        const QString dir = QDir(paths::configDir()).filePath(QStringLiteral("userDefineLangs"));
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
}

void MainWindow::showUserLanguageDialog()
{
    UserLanguageDialog dialog(_syntax.userLanguages(), *_prompter, this);
    if (const Buffer* buf = _documents.buffer(currentBuffer()); buf && buf->language() == LangType::User)
        dialog.selectLanguage(buf->userLanguage());
    connect(&dialog, &UserLanguageDialog::languagesChanged, this, &MainWindow::userLanguagesChanged);
    dialog.exec();
}

QStringList MainWindow::importUserLanguage(const QString& path)
{
    QStringList imported;
    QString error;
    if (!_syntax.userLanguages().importFile(path, &imported, &error))
        _prompter->warn(this, tr("Import language"), error);
    else
        userLanguagesChanged();
    return imported;
}

void MainWindow::userLanguagesChanged()
{
    // Los documentos sin lenguaje elegido a mano vuelven a detectarse (una extensión
    // nueva puede apuntar ahora a un lenguaje del usuario); los demás, que tengan
    // un lenguaje que ya no existe, pasan a texto normal
    for (std::size_t i = 0; i < _documents.count(); ++i) {
        Buffer* buf = _documents.bufferAt(i);
        const bool gone = buf->language() == LangType::User && !_syntax.userLanguages().find(buf->userLanguage());
        if (gone) {
            buf->setLanguage(LangType::Text);
        } else if (!buf->isLanguageSetFromMenu() && !buf->isUntitled()) {
            const QString udl = _syntax.detectUserLanguage(buf->filePath());
            if (!udl.isEmpty())
                buf->setUserLanguage(udl);
            else if (buf->language() == LangType::User)
                buf->setLanguage(_syntax.detect(buf->filePath(), _documents.text(buf->id()).substr(0, 1024)));
        }
    }
    _syntax.notifyUserLanguagesChanged();
}

void MainWindow::reloadSyntax()
{
    showWarnings(tr("Reload styles"), _syntax.reload());
}

bool MainWindow::setTheme(const QString& path)
{
    QString error;
    if (!_syntax.setTheme(path, &error)) {
        _prompter->warn(this, tr("Theme"), tr("Cannot load theme %1:\n%2").arg(path, error));
        return false;
    }
    // Como N++ en modo oscuro: el tema elegido es el de ese modo (darkThemeName/lightThemeName)
    const QString name = path.isEmpty() ? QStringLiteral("stylers.xml") : QFileInfo(path).fileName();
    if (appearance::isDarkApplied())
        _prefs.darkThemeName = name;
    else
        _prefs.lightThemeName = name;
    storePreferences(_syntax.config(), _prefs);
    _syntax.config().save();
    return true;
}

void MainWindow::afterSave(const QString& path)
{
    // Guardar langs.xml, stylers.xml o el tema activo desde el propio editor los recarga
    if (_syntax.isActiveConfigFile(path))
        reloadSyntax();
}

void MainWindow::createSearchMenu()
{
    using Tab = FindReplaceDialog::Tab;
    QMenu* search = menuBar()->addMenu(tr("&Search"));
    auto addIn = [this](QMenu* menu, const QString& text, const char* name, const QKeySequence& key, auto&& slot) {
        QAction* action = menu->addAction(text);
        action->setObjectName(QString::fromLatin1(name));
        action->setShortcut(key);
        if (const int idm = nppCommandFor(name)) {
            action->setProperty("nppCommand", idm);
            connect(action, &QAction::triggered, this, [this, idm, slot] { runMenuCommand(idm, slot); });
        } else {
            connect(action, &QAction::triggered, this, slot);
        }
        return action;
    };
    auto add = [&addIn, search](const QString& text, const char* name, const QKeySequence& key, auto&& slot) {
        return addIn(search, text, name, key, slot);
    };
    add(tr("&Find…"), "actionFind", QKeySequence::Find, [this] {
        if (_prefs.inlineFind)
            showFindBar(false);
        else
            showFindDialog(Tab::Find);
    });
    add(tr("Find in &Files…"), "actionFindInFiles", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F),
        [this] { showFindDialog(Tab::FindInFiles); });
    add(tr("Find Characters in &Range…"), "actionFindCharsInRange", QKeySequence(), [this] {
        if (!_findCharsDialog) {
            _findCharsDialog = new FindCharsDialog(this);
            connect(_findCharsDialog, &FindCharsDialog::findRequested, this, [this] {
                if (!findCharInRange(_findCharsDialog->low(), _findCharsDialog->high(), _findCharsDialog->forward(),
                                     _findCharsDialog->wrap()))
                    statusBar()->showMessage(tr("Find Characters in Range: no character in that range"), 5000);
            });
        }
        _findCharsDialog->show();
        _findCharsDialog->raise();
        _findCharsDialog->activateWindow();
    });
    add(tr("Find &Next"), "actionFindNext", QKeySequence(Qt::Key_F3), [this] {
        SearchOptions o = _findDialog && !_findBarUsedLast ? _findDialog->options() : _lastOptions;
        o.searchUp = false;
        findNextInCurrent(o);
    });
    add(tr("Find &Previous"), "actionFindPrevious", QKeySequence(Qt::SHIFT | Qt::Key_F3), [this] {
        SearchOptions o = _findDialog && !_findBarUsedLast ? _findDialog->options() : _lastOptions;
        o.searchUp = true;
        findNextInCurrent(o);
    });
    add(tr("Select and Find Next"), "actionFindSelectedNext", QKeySequence(Qt::CTRL | Qt::Key_F3),
        [this] { findSelectedText(false); });
    add(tr("Select and Find Previous"), "actionFindSelectedPrevious",
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F3), [this] { findSelectedText(true); });
    add(tr("&Replace…"), "actionReplace", QKeySequence(Qt::CTRL | Qt::Key_H), [this] {
        if (_prefs.inlineFind)
            showFindBar(true);
        else
            showFindDialog(Tab::Replace);
    });
    search->addSeparator();
    add(tr("&Go to…"), "actionGoTo", QKeySequence(Qt::CTRL | Qt::Key_G), [this] {
        const sptr_t lines = editor()->sci(SCI_GETLINECOUNT);
        const sptr_t current = editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(editor()->sci(SCI_GETCURRENTPOS))) + 1;
        bool ok = false;
        const int line = QInputDialog::getInt(this, tr("Go To Line"), tr("Line (1 - %1):").arg(lines),
                                              static_cast<int>(current), 1, static_cast<int>(lines), 1, &ok);
        if (ok)
            goToLine(line);
    });
    add(tr("Go to &File…"), "actionGoToFile", QKeySequence(Qt::CTRL | Qt::Key_E), [this] { showCommandPalette(QString()); });
    add(tr("Go to S&ymbol…"), "actionGoToSymbol", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O),
        [this] { showCommandPalette(QStringLiteral("@")); });
    // Con el servidor de lenguaje (no existen en N++)
    add(tr("Go to &Definition"), "actionGoToDefinition", QKeySequence(Qt::Key_F12), [this] { goToDefinition(); });
    add(tr("Find Re&ferences"), "actionFindReferences", QKeySequence(Qt::SHIFT | Qt::Key_F12), [this] { findReferences(); });
    add(tr("Go to Symbol in &Project…"), "actionWorkspaceSymbol", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_O), [this] { showWorkspaceSymbols(); });
    search->addSeparator();
    add(tr("Next Search Result"), "actionNextResult", QKeySequence(Qt::Key_F4),
        [this] { _results->activateAdjacentResult(true); });
    add(tr("Previous Search Result"), "actionPreviousResult", QKeySequence(Qt::SHIFT | Qt::Key_F4),
        [this] { _results->activateAdjacentResult(false); });
    add(tr("Rege&x Tester"), "actionRegexTester", QKeySequence(), [this] { showRegexTester(); });
    QAction* panel = _results->toggleViewAction();
    panel->setText(tr("Search &Results Window"));
    panel->setShortcut(QKeySequence(Qt::Key_F7));
    search->addAction(panel);
    search->addSeparator();

    // Llaves (IDM_SEARCH_GOTOMATCHINGBRACE, IDM_SEARCH_SELECTMATCHINGBRACES)
    add(tr("Go to &Matching Brace"), "actionGoToMatchingBrace", QKeySequence(Qt::CTRL | Qt::Key_B),
        [this] { goToMatchingBrace(*editor(), false); });
    add(tr("Select All In-between &Braces"), "actionSelectMatchingBraces", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B),
        [this] { goToMatchingBrace(*editor(), true); });
    add(tr("Mar&k…"), "actionMark", QKeySequence(Qt::CTRL | Qt::Key_M), [this] { showFindDialog(FindReplaceDialog::Tab::Mark); });

    // Estilo de token (IDM_SEARCH_MARKALLEXT*, MARKONEEXT*, UNMARKALLEXT*, GONEXTMARKER*…)
    QMenu* styleAll = search->addMenu(tr("S&tyle All Occurrences of Token"));
    QMenu* styleOne = search->addMenu(tr("Style &One Token"));
    QMenu* unstyle = search->addMenu(tr("&Clear Style"));
    QMenu* nextStyle = search->addMenu(tr("Jump to Next Style"));
    QMenu* previousStyle = search->addMenu(tr("Jump to Previous Style"));
    static constexpr const char* kStyleNames[] = {QT_TR_NOOP("Using &1st Style"), QT_TR_NOOP("Using &2nd Style"), QT_TR_NOOP("Using &3rd Style"),
                                                  QT_TR_NOOP("Using &4th Style"), QT_TR_NOOP("Using &5th Style")};
    for (int i = 0; i < 5; ++i) {
        const int indicator = kIndicatorTokenStyles[i];
        const QByteArray n = QByteArray::number(i + 1);
        addIn(styleAll, tr(kStyleNames[i]), QByteArray("actionStyleAll" + n).constData(), QKeySequence(), [this, indicator] {
            markAllOccurrences(*editor(), selectedTextOrWord(*editor()), indicator, _prefs.markAllMatchCase,
                               _prefs.markAllWholeWord);
        });
        addIn(styleOne, tr(kStyleNames[i]), QByteArray("actionStyleOne" + n).constData(), QKeySequence(),
              [this, indicator] { markSelection(*editor(), indicator); });
        addIn(unstyle, tr(kStyleNames[i]), QByteArray("actionUnstyle" + n).constData(), QKeySequence(),
              [this, indicator] { clearIndicator(*editor(), indicator); });
        addIn(nextStyle, tr(kStyleNames[i]), QByteArray("actionNextStyle" + n).constData(), QKeySequence(),
              [this, indicator] { goToIndicator(*editor(), indicator, true); });
        addIn(previousStyle, tr(kStyleNames[i]), QByteArray("actionPreviousStyle" + n).constData(), QKeySequence(),
              [this, indicator] { goToIndicator(*editor(), indicator, false); });
    }
    unstyle->addSeparator();
    addIn(unstyle, tr("Clear &All Styles"), "actionUnstyleAll", QKeySequence(), [this] {
        for (const int indicator : kIndicatorTokenStyles)
            clearIndicator(*editor(), indicator);
    });
    nextStyle->addSeparator();
    addIn(nextStyle, tr("&Mark All on Search"), "actionNextFoundMark", QKeySequence(),
          [this] { goToIndicator(*editor(), kIndicatorFound, true); });
    previousStyle->addSeparator();
    addIn(previousStyle, tr("&Mark All on Search"), "actionPreviousFoundMark", QKeySequence(),
          [this] { goToIndicator(*editor(), kIndicatorFound, false); });
    // IDM_SEARCH_STYLE*TOCLIP, ALLSTYLESTOCLIP, MARKEDTOCLIP
    QMenu* copyStyled = search->addMenu(tr("Cop&y Styled Text"));
    for (int i = 0; i < 5; ++i) {
        const int indicator = kIndicatorTokenStyles[i];
        addIn(copyStyled, tr(kStyleNames[i]), QByteArray("actionCopyStyle" + QByteArray::number(i + 1)).constData(),
              QKeySequence(), [this, indicator] { copyStyledText({indicator}); });
    }
    addIn(copyStyled, tr("All S&tyles"), "actionCopyAllStyles", QKeySequence(), [this] {
        copyStyledText(std::vector<int>(std::begin(kIndicatorTokenStyles), std::end(kIndicatorTokenStyles)));
    });
    addIn(copyStyled, tr("&Mark All on Search"), "actionCopyFoundMark", QKeySequence(),
          [this] { copyStyledText({kIndicatorFound}); });
    search->addSeparator();

    // Marcadores (IDM_SEARCH_*BOOKMARK*, *MARKEDLINES)
    QMenu* bookmarks = search->addMenu(tr("&Bookmark"));
    addIn(bookmarks, tr("&Toggle Bookmark"), "actionToggleBookmark", QKeySequence(Qt::CTRL | Qt::Key_F2),
          [this] { toggleBookmark(*editor(), -1); });
    addIn(bookmarks, tr("&Next Bookmark"), "actionNextBookmark", QKeySequence(Qt::Key_F2),
          [this] { goToBookmark(*editor(), true); });
    addIn(bookmarks, tr("&Previous Bookmark"), "actionPreviousBookmark", QKeySequence(Qt::SHIFT | Qt::Key_F2),
          [this] { goToBookmark(*editor(), false); });
    addIn(bookmarks, tr("&Clear All Bookmarks"), "actionClearBookmarks", QKeySequence(),
          [this] { clearBookmarks(*editor()); });
    bookmarks->addSeparator();
    addIn(bookmarks, tr("C&ut Bookmarked Lines"), "actionCutMarkedLines", QKeySequence(), [this] {
        QApplication::clipboard()->setText(QString::fromStdString(markedLinesText(*editor())));
        deleteMarkedLines(*editor(), true);
    });
    addIn(bookmarks, tr("C&opy Bookmarked Lines"), "actionCopyMarkedLines", QKeySequence(),
          [this] { QApplication::clipboard()->setText(QString::fromStdString(markedLinesText(*editor()))); });
    addIn(bookmarks, tr("&Paste to (Replace) Bookmarked Lines"), "actionPasteMarkedLines", QKeySequence(), [this] {
        const QString text = QApplication::clipboard()->text();
        if (!text.isEmpty())
            replaceMarkedLines(*editor(), text.toStdString());
    });
    addIn(bookmarks, tr("&Remove Bookmarked Lines"), "actionDeleteMarkedLines", QKeySequence(),
          [this] { deleteMarkedLines(*editor(), true); });
    addIn(bookmarks, tr("Remove &Non-Bookmarked Lines"), "actionDeleteUnmarkedLines", QKeySequence(),
          [this] { deleteMarkedLines(*editor(), false); });
    addIn(bookmarks, tr("&Inverse Bookmark"), "actionInverseBookmarks", QKeySequence(),
          [this] { inverseBookmarks(*editor()); });

    // Historial de cambios (IDM_SEARCH_CHANGED_NEXT/PREV, IDM_SEARCH_CLEAR_CHANGE_HISTORY)
    QMenu* changes = search->addMenu(tr("Change &History"));
    addIn(changes, tr("Go to &Next Change"), "actionNextChange", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Down),
          [this] {
              if (_compare) // comparando: las diferencias
                  findChild<QAction*>(QStringLiteral("actionNextDifference"))->trigger();
              else
                  goToChange(*editor(), true);
          });
    addIn(changes, tr("Go to &Previous Change"), "actionPreviousChange", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Up),
          [this] {
              if (_compare)
                  findChild<QAction*>(QStringLiteral("actionPreviousDifference"))->trigger();
              else
                  goToChange(*editor(), false);
          });
    addIn(changes, tr("&Clear All Changes"), "actionClearChangeHistory", QKeySequence(), [this] {
        clearChangeHistory(*editor());
        refreshCurrentDocument(); // deshacer ya no está disponible
    });
    // Cambios respecto al último commit de Git (no existe en N++)
    changes->addSeparator();
    addIn(changes, tr("Go to Next &Git Change"), "actionNextGitChange", QKeySequence(), [this] { goToGitChange(true); });
    addIn(changes, tr("Go to Previous G&it Change"), "actionPreviousGitChange", QKeySequence(), [this] { goToGitChange(false); });
    changes->addSeparator();
    QAction* blameAction = addIn(changes, tr("Show Git &Blame"), "actionGitBlame", QKeySequence(), [this] {
        const bool on = findChild<QAction*>(QStringLiteral("actionGitBlame"))->isChecked();
        for (const QPointer<GitBlame>& blame : _blame) {
            if (blame)
                blame->setMarginVisible(on);
        }
    });
    blameAction->setCheckable(true);
    addIn(changes, tr("Show C&ommit of the Current Line"), "actionShowLineCommit", QKeySequence(), [this] { showCommitOfCurrentLine(); });
}

void MainWindow::showTabContextMenu(BufferId id, const QPoint& globalPos)
{
    if (!_documents.buffer(id))
        return;
    _activePane->activateBuffer(id);
    const std::unique_ptr<QMenu> menu(buildTabContextMenu(id));
    menu->exec(globalPos);
}

QString MainWindow::tabContextMenuPath() const
{
    return QDir(paths::configDir()).filePath(QStringLiteral("tabContextMenu.xml"));
}

QMenu* MainWindow::buildTabContextMenu(BufferId id)
{
    auto* menuPtr = new QMenu(this);
    menuPtr->setObjectName(QStringLiteral("tabContextMenu"));
    const Buffer* buf = _documents.buffer(id);
    if (!buf)
        return menuPtr;
    // NppParameters::hasCustomTabContextMenu: con tabContextMenu.xml, sus órdenes (las del menú
    // principal, que actúan sobre el documento activo, el de la pestaña)
    refreshTransferActions();
    if (QFileInfo::exists(tabContextMenuPath())) {
        fillMenuFromXml(menuPtr, readContextMenu(tabContextMenuPath(), nullptr, ContextMenuKind::Tab));
        if (!menuPtr->actions().isEmpty())
            return menuPtr;
    }
    QMenu& menu = *menuPtr;
    menu.addAction(tr("Close"), this, [this, id] { closeBuffer(id); });
    menu.addAction(tr("Close All but This"), this, [this, id] { closeAllBut(id); });
    menu.addAction(tr("Close All to the Left"), this, [this, id] { closeAllToLeft(id); });
    menu.addAction(tr("Close All to the Right"), this, [this, id] { closeAllToRight(id); });
    menu.addSeparator();
    // IDM_PINTAB y los colores (el menú contextual de la pestaña de N++)
    menu.addAction(buf->isPinned() ? tr("Unpin Tab") : tr("Pin Tab"), this, [this, id] {
        if (Buffer* b = _documents.buffer(id))
            b->setPinned(!b->isPinned());
    });
    QMenu* colors = menu.addMenu(tr("Apply Color"));
    for (int c = 0; c < 5; ++c) {
        if (QAction* source = findChild<QAction*>(QStringLiteral("actionTabColor%1").arg(c + 1))) {
            QAction* a = colors->addAction(source->icon(), tr("Color %1").arg(c + 1), this, [this, id, c] {
                if (Buffer* b = _documents.buffer(id))
                    b->setTabColor(c);
            });
            a->setCheckable(true);
            a->setChecked(buf->tabColor() == c);
        }
    }
    colors->addAction(tr("Remove Color"), this, [this, id] {
        if (Buffer* b = _documents.buffer(id))
            b->setTabColor(-1);
    })->setEnabled(buf->tabColor() >= 0);
    menu.addSeparator();
    menu.addAction(tr("Move to Other View"), this, [this] { moveToOtherView(); });
    menu.addAction(tr("Clone to Other View"), this, [this] { cloneToOtherView(); });
    menu.addAction(tr("Move to New Instance"), this, [this] { openInNewInstance(true); })->setEnabled(
        !buf->isUntitled() && !buf->isDirty());
    menu.addAction(tr("Open in New Instance"), this, [this] { openInNewInstance(false); })->setEnabled(
        !buf->isUntitled() && !buf->isDirty());
    menu.addSeparator();
    menu.addAction(tr("Save"), this, [this, id] { saveBuffer(id); });
    menu.addAction(tr("Save As…"), this, [this, id] { saveBufferAs(id); });
    QAction* reload = menu.addAction(tr("Reload"), this, [this, id] { reloadBuffer(id); });
    reload->setEnabled(!buf->isUntitled());
    menu.addSeparator();
    const QString path = buf->filePath();
    QAction* copyPath = menu.addAction(tr("Copy Full File Path"), this, [path] { QApplication::clipboard()->setText(path); });
    const QString name = buf->displayName();
    menu.addAction(tr("Copy Filename"), this, [name] { QApplication::clipboard()->setText(name); });
    const QString dir = QFileInfo(path).absolutePath();
    QAction* copyDir = menu.addAction(tr("Copy Current Dir. Path"), this, [dir] { QApplication::clipboard()->setText(dir); });
    QAction* openDir = menu.addAction(tr("Open Containing Folder"), this,
                                      [dir] { QDesktopServices::openUrl(QUrl::fromLocalFile(dir)); });
    QAction* openTerminalHere = menu.addAction(tr("Open Containing Folder in Terminal"), this, [this, dir] { openTerminal(dir); });
    for (QAction* a : {copyPath, copyDir, openDir, openTerminalHere})
        a->setEnabled(!buf->isUntitled());
    menu.addSeparator();
    QAction* readOnly = menu.addAction(tr("Read-Only"));
    readOnly->setCheckable(true);
    readOnly->setChecked(buf->isUserReadOnly());
    connect(readOnly, &QAction::toggled, this, [this, id](bool on) {
        if (Buffer* b = _documents.buffer(id))
            b->setUserReadOnly(on);
    });
    return menuPtr;
}

// --- Archivo --------------------------------------------------------------------

BufferId MainWindow::newDocument()
{
    const BufferId id = _documents.newEmptyDocument();
    _activePane->addBuffer(id);
    _activePane->activateBuffer(id);
    updatePaneVisibility();
    return id;
}

void MainWindow::showLoadedBuffer(BufferId id, EditorPane* pane)
{
    if (!pane)
        pane = _activePane;
    // loadBufferIntoView: si la única pestaña es un documento sin título y sin
    // cambios (y la otra vista no lo muestra), el archivo ocupa su lugar
    if (pane->count() == 1) {
        const BufferId only = pane->buffers().front();
        const Buffer* buf = _documents.buffer(only);
        if (buf && buf->isUntitled() && !buf->isDirty() && !otherPane(pane)->contains(only)) {
            pane->replaceBuffer(only, id);
            _documents.closeBuffer(only);
            return;
        }
    }
    pane->addBuffer(id);
    pane->activateBuffer(id);
    updatePaneVisibility();
}

BufferId MainWindow::openFile(const QString& path, bool askToCreate)
{
    if (path.isEmpty())
        return kInvalidBuffer;
    const QString absPath = absolutePath(path);
    const QFileInfo info(absPath);
    // Notepad_plus::doOpen: un archivo de sesión abre sus archivos
    if (isSessionFile(absPath) && info.isFile()) {
        loadSessionFile(absPath);
        return kInvalidBuffer;
    }

    if (const BufferId existing = _documents.findByPath(absPath); existing != kInvalidBuffer) {
        // switchToFile: en la vista que lo muestre (primero la activa)
        if (EditorPane* pane = paneContaining(existing)) {
            pane->activateBuffer(existing);
            setActivePane(pane);
        }
        return existing;
    }

    if (info.isDir()) {
        // N++ abre los archivos que contiene
        BufferId last = kInvalidBuffer;
        const QDir dir(absPath);
        for (const QString& name : dir.entryList(QDir::Files, QDir::Name)) {
            if (const BufferId id = openFile(dir.filePath(name)); id != kInvalidBuffer)
                last = id;
        }
        return last;
    }

    if (!info.exists()) {
        if (!askToCreate) {
            _prompter->warn(this, tr("Open"), tr("The file does not exist:\n%1").arg(absPath));
            return kInvalidBuffer;
        }
        if (!_prompter->confirm(this, tr("Create new file"), tr("\"%1\" doesn't exist. Create it?").arg(absPath)))
            return kInvalidBuffer;
        QFile file(absPath);
        if (!file.open(QIODevice::WriteOnly)) {
            _prompter->warn(this, tr("Create new file"), tr("Cannot create %1:\n%2").arg(absPath, file.errorString()));
            return kInvalidBuffer;
        }
    }

    QString error;
    const BufferId id = _documents.loadFile(absPath, -1, &error);
    if (id == kInvalidBuffer) {
        _prompter->warn(this, tr("Open"), error);
        return kInvalidBuffer;
    }
    showLoadedBuffer(id);
    _lastDirectory = info.absolutePath();
    return id;
}

void MainWindow::openFiles(const QStringList& paths, bool askToCreate)
{
    for (const QString& path : paths)
        openFile(path, askToCreate);
}

QString MainWindow::suggestedDirectory() const
{
    // openSaveDir de las preferencias
    if (_prefs.openSaveDir == OpenSaveDir::Custom && QFileInfo(_prefs.defaultDirPath).isDir())
        return _prefs.defaultDirPath;
    const Buffer* buf = _documents.buffer(currentBuffer());
    if (_prefs.openSaveDir == OpenSaveDir::FollowCurrent && buf && !buf->isUntitled())
        return QFileInfo(buf->filePath()).absolutePath();
    return _lastDirectory.isEmpty() ? QDir::homePath() : _lastDirectory;
}

void MainWindow::openWithDialog()
{
    openFiles(_prompter->openFileNames(this, suggestedDirectory(), fileFilters()));
}

bool MainWindow::confirmEncodable(BufferId id)
{
    Buffer* buf = _documents.buffer(id);
    if (!buf || buf->encoding().isUnicode())
        return true;
    const std::size_t unmappable = countUnmappable(_documents.text(id), buf->encoding());
    if (unmappable == 0)
        return true;
    switch (_prompter->askUnmappable(this, buf->displayName(), unmappable, displayName(buf->encoding()))) {
    case Prompter::UnmappableChoice::ConvertToUtf8:
        buf->setEncoding(TextEncoding::utf8());
        return true;
    case Prompter::UnmappableChoice::SaveAnyway:
        return true;
    case Prompter::UnmappableChoice::Cancel:
        break;
    }
    return false;
}

bool MainWindow::saveBuffer(BufferId id)
{
    const Buffer* buf = _documents.buffer(id);
    if (!buf)
        return false;
    if (buf->isUntitled())
        return saveBufferAs(id);
    if (!confirmEncodable(id))
        return false;
    const QString path = buf->filePath();
    if (!backupBeforeSave(path))
        return false;
    formatBeforeSave(id);
    notifyPlugins(NPPN_FILEBEFORESAVE, id);
    const SaveResult result = _documents.saveBuffer(id, path);
    if (result.ok()) {
        notifyPlugins(NPPN_FILESAVED, id);
        _lsp->documentSaved(id);
        scheduleGitStatus();
        // Un archivo de fragmentos del usuario: se vuelven a leer
        if (absolutePath(path).startsWith(QDir(SnippetLibrary::userDirectory()).absolutePath() + QLatin1Char('/')))
            _snippets->reload();
        for (const QPointer<GitBlame>& blame : _blame) {
            if (blame)
                blame->reload();
        }
    }
    if (!result.ok()) {
        _prompter->warn(this, tr("Save"), result.errorString);
        return false;
    }
    afterSave(path);
    return true;
}

bool MainWindow::saveBufferAs(BufferId id, bool isCopy)
{
    const Buffer* buf = _documents.buffer(id);
    if (!buf)
        return false;
    const QString suggested = buf->isUntitled() ? QDir(suggestedDirectory()).filePath(buf->displayName())
                                                : buf->filePath();
    const QString chosen = _prompter->saveFileName(this, suggested, fileFilters(), fileFilterFor(buf->language()));
    if (chosen.isEmpty())
        return false;
    return saveBufferTo(id, absolutePath(chosen), isCopy);
}

bool MainWindow::saveBufferTo(BufferId id, const QString& absPath, bool isCopy)
{
    // N++ no deja guardar encima de otro documento abierto
    const BufferId other = _documents.findByPath(absPath);
    if (other != kInvalidBuffer && other != id) {
        _prompter->warn(this, tr("Save As"),
                        tr("\"%1\" is already open in another tab. Close it before overwriting it.").arg(absPath));
        return false;
    }
    if (!confirmEncodable(id) || !backupBeforeSave(absPath))
        return false;
    if (!isCopy)
        formatBeforeSave(id);
    notifyPlugins(NPPN_FILEBEFORESAVE, id);
    const SaveResult result = _documents.saveBuffer(id, absPath, isCopy);
    if (result.ok())
        notifyPlugins(NPPN_FILESAVED, id);
    if (!result.ok()) {
        _prompter->warn(this, tr("Save As"), result.errorString);
        return false;
    }
    _lastDirectory = QFileInfo(absPath).absolutePath();
    afterSave(absPath);
    return true;
}

bool MainWindow::saveAll()
{
    bool ok = true;
    for (const BufferId id : allBuffers()) {
        const Buffer* buf = _documents.buffer(id);
        if (buf && buf->isDirty()) {
            if (EditorPane* pane = buf->isUntitled() ? paneContaining(id) : nullptr) {
                pane->activateBuffer(id); // que se vea cuál se va a nombrar
                setActivePane(pane);
            }
            ok = saveBuffer(id) && ok;
        }
    }
    return ok;
}

bool MainWindow::backupBeforeSave(const QString& path)
{
    // Notepad_plus::doSave: copia de la versión que hay en disco
    if (_prefs.backupOnSave == BackupOnSave::None || !QFileInfo::exists(path))
        return true;
    const QString backup = saveBackupPath(path, _prefs, QDateTime::currentDateTime());
    QDir().mkpath(QFileInfo(backup).absolutePath());
    QFile::remove(backup);
    if (QFile::copy(path, backup))
        return true;
    return _prompter->confirm(this, tr("Backup"),
                              tr("The previous version of the file could not be copied to \"%1\".\n\nDo you want to save "
                                 "the file anyway?").arg(backup));
}

bool MainWindow::reloadBuffer(BufferId id)
{
    const Buffer* buf = _documents.buffer(id);
    if (!buf || buf->isUntitled())
        return false;
    if (buf->isDirty()
        && !_prompter->confirm(this, tr("Reload"),
                               tr("Reload \"%1\" and lose the unsaved changes?").arg(buf->displayName())))
        return false;
    return reloadFromDisk(id);
}

bool MainWindow::reloadFromDisk(BufferId id)
{
    const Buffer* buf = _documents.buffer(id);
    if (!buf)
        return false;
    // Cada vista que lo muestra conserva su cursor y su scroll (doReload)
    std::vector<std::pair<EditorPane*, Position>> positions;
    for (EditorPane* pane : {_mainPane, _subPane}) {
        if (pane->currentBuffer() == id)
            positions.emplace_back(pane, pane->editor()->currentPosition());
    }
    const int codepage = buf->encoding().isUnicode() ? -1 : buf->encoding().codepage;
    QString error;
    if (!_documents.reloadBuffer(id, codepage, &error)) {
        _prompter->warn(this, tr("Reload"), error);
        return false;
    }
    for (const auto& [pane, position] : positions) {
        EditorView* view = pane->editor();
        view->restorePosition(position);
        if (_prefs.goesToEnd()) // cdGo2end: como tail -f
            view->sci(SCI_DOCUMENTEND);
    }
    return true;
}

// --- Imprimir -------------------------------------------------------------------------------

int MainWindow::printCurrent(QPagedPaintDevice& device, bool selectionOnly, int fromPage, int toPage)
{
    const Buffer* buf = _documents.buffer(currentBuffer());
    EditorView* view = editor();
    PrintRequest request;
    request.documentPath = buf ? (buf->isUntitled() ? buf->displayName() : buf->filePath()) : QString();
    request.selectionOnly = selectionOnly;
    request.selectionStart = view->sci(SCI_GETSELECTIONSTART);
    request.selectionEnd = view->sci(SCI_GETSELECTIONEND);
    request.fromPage = fromPage;
    request.toPage = toPage;
    return printDocument(*view, device, _prefs, request);
}

void MainWindow::printWithDialog()
{
    QPrinter printer(QPrinter::HighResolution);
    const Buffer* buf = _documents.buffer(currentBuffer());
    if (buf)
        printer.setDocName(buf->displayName());
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print"));
    // Como N++: "Selección" solo si hay algo seleccionado
    dialog.setOption(QAbstractPrintDialog::PrintSelection, editor()->sci(SCI_GETSELECTIONEMPTY) == 0);
    dialog.setOption(QAbstractPrintDialog::PrintPageRange, true);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const bool selection = printer.printRange() == QPrinter::Selection;
    const bool pages = printer.printRange() == QPrinter::PageRange;
    if (printCurrent(printer, selection, pages ? printer.fromPage() : 0, pages ? printer.toPage() : 0) == 0)
        _prompter->warn(this, tr("Print"), tr("Printing could not be started."));
}

void MainWindow::printNow()
{
    // IDM_FILE_PRINTNOW: impresora por defecto y todo el documento
    QPrinter printer(QPrinter::HighResolution);
    if (const Buffer* buf = _documents.buffer(currentBuffer()))
        printer.setDocName(buf->displayName());
    if (!printer.isValid() || printCurrent(printer) == 0)
        _prompter->warn(this, tr("Print"), tr("No printer is available."));
}

int MainWindow::exportToPdf()
{
    const Buffer* buf = _documents.buffer(currentBuffer());
    if (!buf)
        return 0;
    const QString base = buf->isUntitled() ? QDir(suggestedDirectory()).filePath(buf->displayName())
                                           : QFileInfo(buf->filePath()).absoluteDir().filePath(QFileInfo(buf->filePath()).completeBaseName());
    const QString filter = tr("PDF (*.pdf)");
    QString path = _prompter->saveFileName(this, base + QStringLiteral(".pdf"), filter, filter);
    if (path.isEmpty())
        return 0;
    if (!path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        path += QStringLiteral(".pdf");
    const int pages = exportToPdf(path, editor()->sci(SCI_GETSELECTIONEMPTY) == 0
                                            && _prompter->confirm(this, tr("Export to PDF"), tr("Export the selection only?")));
    if (pages == 0)
        _prompter->warn(this, tr("Export to PDF"), tr("Could not write %1").arg(path));
    return pages;
}

int MainWindow::exportToPdf(const QString& path, bool selectionOnly)
{
    QPdfWriter writer(path);
    writer.setResolution(300);
    writer.setPageSize(QPageSize(QLocale().measurementSystem() == QLocale::ImperialUSSystem ? QPageSize::Letter
                                                                                             : QPageSize::A4));
    const Buffer* buf = _documents.buffer(currentBuffer());
    writer.setTitle(buf ? buf->displayName() : QString());
    writer.setCreator(QString::fromUtf8(kAppName));
    return printCurrent(writer, selectionOnly);
}

// --- Preferencias -------------------------------------------------------------------------

void MainWindow::showShortcutMapper()
{
    ShortcutMapperDialog dialog(*this, *_prompter, this);
    dialog.exec();
    if (_manual)
        _manual->reload(); // los atajos que cita el manual
}

void MainWindow::showStyleConfigurator()
{
    StyleConfiguratorDialog dialog(_syntax, *_prompter, [this](const QString& path) { return setTheme(path); }, this);
    dialog.exec();
}

void MainWindow::showPreferences(const QString& page)
{
    std::vector<std::pair<int, QString>> languages;
    for (int i = 0; i < static_cast<int>(LangType::External); ++i) {
        const auto lang = static_cast<LangType>(i);
        if (lang == LangType::SearchResult || lang == LangType::User)
            continue;
        const LanguageInfo& info = languageInfo(lang);
        languages.emplace_back(i, lang == LangType::Text ? tr("None (Normal Text)")
                                                     : QString::fromUtf8(info.menuName ? info.menuName : info.name));
    }
    PreferencesDialog dialog(_prefs, std::move(languages), this);
    dialog.setCloudFolder(paths::cloudChoice());
    // Nube y enlaces: la carpeta va en cloud/choice y vale al reiniciar (como en N++)
    auto applyCloud = [this, &dialog] {
        const QString folder = dialog.cloudFolder();
        if (folder == paths::cloudChoice())
            return;
        QString error;
        if (!paths::setCloudChoice(folder, &error)) {
            _prompter->warn(this, tr("Settings on cloud"), error);
            dialog.setCloudFolder(paths::cloudChoice());
            return;
        }
        _prompter->inform(this, tr("Settings on cloud"), tr("Please restart Edita to take effect."));
    };
    connect(&dialog, &PreferencesDialog::applyRequested, this, [this, applyCloud](const Preferences& prefs) {
        setPreferences(prefs);
        applyCloud();
    });
    if (!page.isEmpty())
        dialog.showPage(page);
    if (dialog.exec() == QDialog::Accepted) {
        setPreferences(dialog.preferences());
        applyCloud();
    }
}

void MainWindow::applyPreferences()
{
    const Preferences& p = _prefs;
    // Servidores de lenguaje: con otras órdenes, cada documento vuelve a buscar el suyo
    _lsp->setAutoComplete(p.autocStatus != 0, p.autocFromLen);
    _lsp->setOverrides(lsp::parseOverrides(p.lspServers));
    _lsp->setEnabled(p.lspEnabled);
    scheduleGitStatus();
    for (std::size_t i = 0; i < _documents.count(); ++i)
        attachToLsp(_documents.bufferAt(i)->id());
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, p.qtFileDialog);
    applyAppearance();
    if (_toolBar) {
        _toolBar->setVisible(p.toolbarVisible);
        const int size = appearance::toolbarIconSize(p.toolbarIcons);
        _toolBar->setIconSize(QSize(size, size));
    }
    _statusBar->setVisible(p.statusBarShow);
    for (EditorPane* pane : {_mainPane, _subPane}) {
        pane->tabBar()->setVisible(!p.tabBarHide);
        pane->tabBar()->setTabsClosable(p.tabCloseButton);
        // setTabsClosable rehace los botones de cerrar: las fijadas, otra vez con el alfiler
        for (const BufferId id : pane->buffers())
            if (const Buffer* buf = _documents.buffer(id))
                pane->tabBar()->updateBuffer(*buf);
        pane->tabBar()->setDoubleClickCloses(p.tabDoubleClickCloses);
        pane->editor()->applyPreferences(p);
        applyDocumentOptions(pane);
    }
    scheduleWelcomeUpdate();
    // Iconos por tipo de archivo: las pestañas, los paneles y la paleta
    if (fileicons::enabled() != p.fileTypeIcons) {
        fileicons::setEnabled(p.fileTypeIcons);
        for (EditorPane* pane : {_mainPane, _subPane})
            pane->tabBar()->refreshIcons();
        if (_folderPanel)
            _folderPanel->refreshIcons();
        updatePanels();
    }
    for (int i = 0; i < 2; ++i) {
        if (_colorPreview[i])
            _colorPreview[i]->setEnabled(p.colorPreview);
        if (_rainbow[i])
            _rainbow[i]->setEnabled(p.rainbowBrackets);
        if (_breadcrumbs[i])
            _breadcrumbs[i]->setVisible(p.breadcrumbs);
        if (_sticky[i])
            _sticky[i]->setEnabled(p.stickyScroll);
        if (_git[i])
            _git[i]->setEnabled(p.gitMargin);
        if (_blame[i])
            _blame[i]->setInlineEnabled(p.gitBlameInline);
        if (_snippetControllers[i])
            _snippetControllers[i]->setTabExpansion(p.snippetsTab);
        scheduleLspDecorations(i);
        if (_scrollMarks[i])
            _scrollMarks[i]->setEnabled(p.scrollBarMarks);
    }

    // Lo que depende de los colores del tema lo aplica SyntaxStyler
    StylerOptions options = _syntax.options();
    options.indentGuides = p.indentGuideLine;
    options.currentLine = p.currentLineMode;
    options.currentLineFrameWidth = p.currentLineFrameWidth;
    options.folderStyle = p.folderStyle;
    options.edgeColumns = p.edgeColumns;
    options.edgeBackground = p.edgeBackground;
    options.tabSize = p.tabSize;
    options.replaceTabsBySpaces = p.tabReplacedBySpace;
    options.backspaceUnindent = p.backspaceUnindent;
    options.bookmarkMargin = p.bookmarkMargin;
    options.changeHistory = static_cast<ChangeHistoryMode>(std::clamp(p.changeHistory, 0, 3));
    options.systemMonospaceFont = p.systemMonospaceFont;
    options.sqlBackslashEscapes = p.sqlBackslashEscapes;
    options.showControlChars = p.ccShow;
    _syntax.setOptions(options);
    loadSpellDictionaries();
    _spellStyles.clear();
    if (QAction* spelling = findChild<QAction*>(QStringLiteral("actionSpellCheck"))) {
        const QSignalBlocker blocker(spelling);
        spelling->setChecked(p.spellCheck);
    }

    // Documentos nuevos y cómo se abren
    DocumentSettings& docs = _documents.settings();
    // Preferencias › Rendimiento: desde qué tamaño un archivo es grande (al abrirlo)
    docs.largeFileThreshold = p.largeFileRestriction ? static_cast<std::int64_t>(p.largeFileSizeMB) * 1024 * 1024
                                                     : std::numeric_limits<std::int64_t>::max();
    docs.load.defaultEol = static_cast<EolType>(std::clamp(p.newDocEol, 0, 2));
    const auto mode = static_cast<UniMode>(p.newDocUnicodeMode);
    docs.newDocEncoding = (mode == UniMode::Ansi && p.newDocCodepage > 0) ? TextEncoding::fromCodepage(p.newDocCodepage)
                        : mode == UniMode::Ansi ? TextEncoding::fromCodepage(p.fallbackCodepage)
                                                : TextEncoding::unicode(mode == UniMode::Ascii7 ? UniMode::Utf8 : mode);
    docs.newDocLanguage = static_cast<LangType>(std::clamp(p.newDocLanguage, 0, static_cast<int>(LangType::External) - 1));
    // L_USER sin el nombre del lenguaje: texto normal (FileManager::newEmptyDocument de N++ 8.9.9)
    if (docs.newDocLanguage == LangType::User)
        docs.newDocLanguage = LangType::Text;
    docs.load.detection.autoDetect = p.detectEncoding;
    docs.load.detection.fallbackCodepage = p.fallbackCodepage;

    if (_findDialog) {
        _findDialog->setMonospacedFont(p.monospacedFontFindDlg);
        _findDialog->setReplaceStops(p.replaceStopsWithoutFindingNext);
    }

    for (const auto& [option, action] : _viewOptionActions)
        action->setChecked(p.*option);
    _showAllAction->setChecked(p.whiteSpaceShow && p.eolShow && p.npcShow && p.ccShow);
    rebuildRecentFilesMenu();
    updateBackupTimer();
}

void MainWindow::toggleViewOption(bool Preferences::* option, bool on)
{
    // Opciones de Ver: se guardan como preferencias, igual que en N++ (svp)
    Preferences p = _prefs;
    p.*option = on;
    setPreferences(p);
}

void MainWindow::copyOrCut(bool cut)
{
    // IDM_EDIT_COPY/CUT: sin selección, la línea entera si está activada la opción
    EditorView* view = editor();
    const bool empty = view->sci(SCI_GETSELECTIONEMPTY) != 0;
    if (empty && _prefs.lineCopyCutWithoutSelection) {
        view->sci(SCI_COPYALLOWLINE);
        if (cut)
            view->sci(SCI_LINEDELETE);
        return;
    }
    view->sci(cut ? SCI_CUT : SCI_COPY);
}

void MainWindow::pasteClipboardMarkup(bool html)
{
    // IDM_EDIT_PASTE_AS_HTML/RTF: el código tal cual (CF_HTML, CF_RTF), no el texto que se ve
    const QMimeData* clip = QApplication::clipboard()->mimeData();
    if (!clip)
        return;
    QByteArray bytes;
    if (html) {
        bytes = clip->data(QStringLiteral("text/html"));
    } else {
        for (const char* mime : {"text/rtf", "application/rtf", "text/richtext"}) {
            if (clip->hasFormat(QString::fromLatin1(mime))) {
                bytes = clip->data(QString::fromLatin1(mime));
                break;
            }
        }
    }
    if (bytes.isEmpty())
        return;
    // Firefox pone el HTML en UTF-16 con BOM; los demás, en UTF-8
    QString text;
    if (bytes.startsWith("\xFF\xFE") || bytes.startsWith("\xFE\xFF"))
        text = QStringDecoder(bytes.startsWith("\xFF\xFE") ? QStringConverter::Utf16LE : QStringConverter::Utf16BE)
                   .decode(bytes.mid(2));
    else
        text = QString::fromUtf8(bytes);
    while (text.endsWith(QChar(0)))
        text.chop(1);
    insertText(text);
}

namespace {
// El formato propio para el contenido binario (CF_NPPTEXTLEN de N++: el texto con su longitud)
const QString kBinaryMime = QStringLiteral("application/x-edita-binary");
} // namespace

void MainWindow::copyBinary(bool cut)
{
    // IDM_EDIT_COPY/CUT_BINARY: los bytes de la selección, NUL incluidos
    // (SCI_GETSELTEXT cambia los NUL por espacios: cada selección con SCI_GETTEXTRANGEFULL; varias,
    // una por línea)
    EditorView* view = editor();
    QByteArray bytes;
    const auto selections = static_cast<int>(view->sci(SCI_GETSELECTIONS));
    for (int i = 0; i < selections; ++i) {
        const sptr_t start = view->sci(SCI_GETSELECTIONNSTART, static_cast<uptr_t>(i));
        const sptr_t end = view->sci(SCI_GETSELECTIONNEND, static_cast<uptr_t>(i));
        QByteArray part(static_cast<qsizetype>(end - start) + 1, '\0');
        Sci_TextRangeFull range{{start, end}, part.data()};
        view->sci(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&range));
        part.truncate(static_cast<qsizetype>(end - start));
        if (i > 0)
            bytes += view->sci(SCI_GETEOLMODE) == SC_EOL_CRLF ? QByteArray("\r\n")
                     : view->sci(SCI_GETEOLMODE) == SC_EOL_CR ? QByteArray("\r")
                                                              : QByteArray("\n");
        bytes += part;
    }
    if (bytes.isEmpty())
        return;
    auto* clip = new QMimeData;
    clip->setData(kBinaryMime, bytes);
    clip->setText(QString::fromUtf8(bytes)); // para los demás programas
    QApplication::clipboard()->setMimeData(clip);
    if (cut)
        view->sends(SCI_REPLACESEL, 0, "");
}

void MainWindow::pasteBinary()
{
    // IDM_EDIT_PASTE_BINARY: con el formato propio, todos los bytes; si no, el texto
    const QMimeData* clip = QApplication::clipboard()->mimeData();
    if (!clip)
        return;
    const QByteArray bytes = clip->hasFormat(kBinaryMime) ? clip->data(kBinaryMime) : clip->text().toUtf8();
    EditorView* view = editor();
    view->sci(SCI_BEGINUNDOACTION);
    view->sends(SCI_REPLACESEL, 0, "");
    view->sci(SCI_ADDTEXT, static_cast<uptr_t>(bytes.size()), reinterpret_cast<sptr_t>(bytes.constData()));
    view->sci(SCI_ENDUNDOACTION);
}

void MainWindow::refreshTransferActions()
{
    const Buffer* buf = _documents.buffer(currentBuffer());
    const bool can = buf && !buf->isUntitled() && !buf->isDirty();
    for (const char* name : {"actionMoveToNewInstance", "actionOpenInNewInstance"}) {
        if (QAction* a = findChild<QAction*>(QString::fromLatin1(name)))
            a->setEnabled(can);
    }
}

bool MainWindow::openInNewInstance(bool move)
{
    // docOpenInNewInstance: la otra ventana lee el archivo del disco, así que sin cambios
    const BufferId id = currentBuffer();
    const Buffer* buf = _documents.buffer(id);
    if (!buf || buf->isUntitled() || buf->isDirty())
        return false;
    // El lenguaje, si no es uno del usuario (N++ deja que la otra lo detecte por la extensión)
    const QString lang = Buffer::isNamedLanguage(buf->language()) ? QString()
                                                                   : QString::fromLatin1(languageInfo(buf->language()).name);
    EditorView* view = editor();
    const sptr_t pos = view->sci(SCI_GETCURRENTPOS);
    const int line = static_cast<int>(view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos))) + 1;
    const int column = static_cast<int>(view->sci(SCI_GETCOLUMN, static_cast<uptr_t>(pos))) + 1;
    const QStringList arguments = newInstanceArguments(buf->filePath(), lang, line, column);
    bool launched = false;
    if (_launchInstance) {
        launched = _launchInstance(arguments);
    } else {
        // Dentro de un AppImage, el AppImage (su carpeta montada se va al cerrar esta ventana)
        const QString appImage = qEnvironmentVariable("APPIMAGE");
        launched = QProcess::startDetached(appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage, arguments);
    }
    if (!launched) {
        _prompter->warn(this, tr("Open in New Instance"), tr("Could not start a new instance of Edita."));
        return false;
    }
    if (move) {
        closeBuffer(id);
        // Si solo queda el documento vacío de siempre, la ventana también se cierra
        const Buffer* only = _documents.count() == 1 ? _documents.bufferAt(0) : nullptr;
        if (_documents.count() == 0 || (only && only->isUntitled() && !only->isDirty() && _documents.text(only->id()).empty()))
            QTimer::singleShot(0, this, &QWidget::close);
    }
    return true;
}

// --- Autocompletado ------------------------------------------------------------------------

AutoCompletion* MainWindow::autoCompletion(const EditorPane* pane) const
{
    return pane == _subPane ? _subAuto.get() : _mainAuto.get();
}

// --- Edición ---------------------------------------------------------------------------------

void MainWindow::trimTrailingSpaces()
{
    trimLines(*editor(), false, true);
}

void MainWindow::sortCurrentLines(SortKind kind, bool descending)
{
    SortRequest request;
    request.kind = kind;
    request.descending = descending;
    request.seed = static_cast<unsigned>(QDateTime::currentMSecsSinceEpoch());
    if (const sptr_t failedLine = sortLines(*editor(), request))
        _prompter->warn(this, tr("Sorting error"),
                        tr("Unable to sort numerically by line %1.").arg(failedLine));
}

CommentSymbols MainWindow::commentSymbols(BufferId id) const
{
    CommentSymbols symbols;
    const Buffer* buf = _documents.buffer(id);
    if (!buf)
        return symbols;
    const LangType lang = buf->language();
    if (lang == LangType::User) {
        // Casillas 00 (línea), 03 y 04 (bloque) de la lista "Comments" (extractSymbol)
        if (const UserLanguage* user = _syntax.userLanguages().find(buf->userLanguage())) {
            const std::string& list = user->keywordLists[UserKeywordList::Comments];
            auto first = [&list](int slot) {
                const QStringList words = userSlotWords(list, slot).split(QLatin1Char(' '), Qt::SkipEmptyParts);
                return words.isEmpty() ? std::string() : words.front().toStdString();
            };
            symbols.line = first(0);
            symbols.start = first(3);
            symbols.end = first(4);
        }
        return symbols;
    }
    if (lang == LangType::External) {
        // Los comentarios del <Language> del XML del plugin
        if (const ExternalLanguage* external = _syntax.externalLexers().find(buf->userLanguage().toStdString())) {
            symbols.line = external->definition.commentLine;
            symbols.start = external->definition.commentStart;
            symbols.end = external->definition.commentEnd;
        }
        return symbols;
    }
    if (const LanguageDefinition* def = _syntax.languages().find(lang)) {
        symbols.line = def->commentLine;
        symbols.start = def->commentStart;
        symbols.end = def->commentEnd;
    }
    symbols.avoidIndent = lang == LangType::Fortran77 || lang == LangType::BaanC;
    symbols.commentEmptyLines = lang == LangType::BaanC;
    symbols.noSpaceAfterLine = lang == LangType::BaanC;
    return symbols;
}

void MainWindow::commentCurrent(CommentKind kind)
{
    const CommentSymbols symbols = commentSymbols(currentBuffer());
    EditorView& view = *editor();
    switch (kind) {
    case CommentKind::Toggle: lineComment(view, symbols, CommentMode::Toggle); break;
    case CommentKind::Comment: lineComment(view, symbols, CommentMode::Comment); break;
    case CommentKind::Uncomment: lineComment(view, symbols, CommentMode::Uncomment); break;
    case CommentKind::Stream: streamComment(view, symbols); break;
    case CommentKind::StreamUncomment: undoStreamComment(view, symbols); break;
    }
}

// --- Macros --------------------------------------------------------------------------------

QString MainWindow::shortcutsPath() const
{
    return QDir(paths::configDir()).filePath(QStringLiteral("shortcuts.xml"));
}

void MainWindow::runMenuCommand(int idm, const std::function<void()>& command)
{
    if (_recordingMacro) {
        MacroStep step;
        step.type = MacroStep::MenuCommand;
        step.wParam = static_cast<std::uintptr_t>(idm);
        _macro.push_back(step);
    }
    // Lo que la orden envíe a Scintilla ya queda representado por la orden
    const QScopedValueRollback suppress(_suppressRecording, _suppressRecording + 1);
    command();
}

QAction* MainWindow::actionForCommand(int idm) const
{
    for (QAction* action : findChildren<QAction*>()) {
        if (action->property("nppCommand").toInt() == idm)
            return action;
    }
    return nullptr;
}

void MainWindow::createMacroMenu()
{
    // Menú Macro de N++ (IDM_MACRO_*)
    _macroMenu = menuBar()->addMenu(tr("&Macro"));
    _recordAction = _macroMenu->addAction(tr("Start &Recording"), this, &MainWindow::toggleMacroRecording);
    _recordAction->setObjectName(QStringLiteral("actionRecordMacro"));
    _recordAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    _playAction = _macroMenu->addAction(tr("&Playback"), this, [this] { playMacro(_macro); });
    _playAction->setObjectName(QStringLiteral("actionPlayMacro"));
    _playAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    _saveMacroAction = _macroMenu->addAction(tr("&Save Current Recorded Macro…"), this, [this] { saveCurrentMacro(); });
    _saveMacroAction->setObjectName(QStringLiteral("actionSaveMacro"));
    _runMacroAction = _macroMenu->addAction(tr("Run a Macro &Multiple Times…"), this, &MainWindow::showRunMacroDialog);
    _runMacroAction->setObjectName(QStringLiteral("actionRunMacro"));
    // IDM_SETTING_SHORTCUT_MAPPER_MACRO: las macros guardadas, en la Asignación de atajos
    QAction* editMacros = _macroMenu->addAction(tr("Modify Shortcut/&Delete Macro…"), this, [this] {
        ShortcutMapperDialog dialog(*this, *_prompter, this);
        dialog.showKind(ShortcutEntry::Kind::Macros);
        dialog.exec();
        if (_manual)
            _manual->reload();
    });
    editMacros->setObjectName(QStringLiteral("actionEditMacros"));
    _savedMacrosAnchor = _macroMenu->addSeparator();
    rebuildMacroMenu();
}

void MainWindow::createRunMenu()
{
    // Menú Ejecutar de N++: Ejecutar… (F5), las órdenes guardadas y la Asignación de atajos
    _runMenu = menuBar()->addMenu(tr("&Run"));
    _runMenu->setObjectName(QStringLiteral("menuRun"));
    QAction* run = _runMenu->addAction(tr("&Run…"), this, &MainWindow::showRunDialog);
    run->setObjectName(QStringLiteral("actionRun"));
    run->setShortcut(QKeySequence(Qt::Key_F5));
    _runMenu->addSeparator();
    _runCommandsEnd = _runMenu->addSeparator();
    QAction* edit = _runMenu->addAction(tr("Modify Shortcut/Delete Command…"), this, [this] {
        ShortcutMapperDialog dialog(*this, *_prompter, this);
        dialog.showKind(ShortcutEntry::Kind::RunCommands);
        dialog.exec();
        if (_manual)
            _manual->reload();
    });
    edit->setObjectName(QStringLiteral("actionEditRunCommands"));
    rebuildRunMenu();
}

void MainWindow::rebuildRunMenu()
{
    for (QAction* action : _userCommandActions) {
        _runMenu->removeAction(action);
        delete action;
    }
    _userCommandActions.clear();
    for (std::size_t i = 0; i < _userCommands.size(); ++i) {
        const UserCommand& command = _userCommands[i];
        auto* action = new QAction(command.name, _runMenu);
        action->setObjectName(QStringLiteral("actionRunCommand%1").arg(i));
        action->setToolTip(command.command);
        if (const int key = qtKeyFromVirtualKey(command.key)) {
            Qt::KeyboardModifiers modifiers;
            modifiers.setFlag(Qt::ControlModifier, command.ctrl);
            modifiers.setFlag(Qt::AltModifier, command.alt);
            modifiers.setFlag(Qt::ShiftModifier, command.shift);
            action->setShortcut(QKeySequence(QKeyCombination(modifiers, static_cast<Qt::Key>(key))));
        }
        connect(action, &QAction::triggered, this, [this, i] {
            if (i < _userCommands.size())
                runCommand(_userCommands[i].command);
        });
        _runMenu->insertAction(_runCommandsEnd, action);
        _userCommandActions.push_back(action);
    }
    _runCommandsEnd->setVisible(!_userCommands.empty());
}

void MainWindow::createWindowMenu()
{
    _windowMenu = menuBar()->addMenu(tr("&Window"));
    _windowMenu->setObjectName(QStringLiteral("menuWindow"));
    QMenu* sortBy = _windowMenu->addMenu(tr("Sort By"));
    struct SortItem
    {
        const char* text;
        const char* name;
        TabSort by;
        bool ascending;
    };
    static constexpr SortItem kSorts[] = {
        {QT_TR_NOOP("Name A to Z"), "actionSortTabsNameAsc", TabSort::Name, true},
        {QT_TR_NOOP("Name Z to A"), "actionSortTabsNameDesc", TabSort::Name, false},
        {QT_TR_NOOP("Path A to Z"), "actionSortTabsPathAsc", TabSort::Path, true},
        {QT_TR_NOOP("Path Z to A"), "actionSortTabsPathDesc", TabSort::Path, false},
        {QT_TR_NOOP("Type A to Z"), "actionSortTabsTypeAsc", TabSort::Type, true},
        {QT_TR_NOOP("Type Z to A"), "actionSortTabsTypeDesc", TabSort::Type, false},
        {QT_TR_NOOP("Content Length Ascending"), "actionSortTabsSizeAsc", TabSort::Size, true},
        {QT_TR_NOOP("Content Length Descending"), "actionSortTabsSizeDesc", TabSort::Size, false},
        {QT_TR_NOOP("Modified Time Ascending"), "actionSortTabsModifiedAsc", TabSort::Modified, true},
        {QT_TR_NOOP("Modified Time Descending"), "actionSortTabsModifiedDesc", TabSort::Modified, false},
    };
    for (const SortItem& item : kSorts) {
        QAction* action = sortBy->addAction(tr(item.text), this, [this, item] { sortTabs(item.by, item.ascending); });
        action->setObjectName(QString::fromLatin1(item.name));
    }
    QAction* windows = _windowMenu->addAction(tr("&Windows…"), this, &MainWindow::showWindowsDialog);
    windows->setObjectName(QStringLiteral("actionWindowsList"));
    _windowDocumentsStart = _windowMenu->addSeparator();
    // Los documentos de la vista actual, al abrir el menú (IDM_WINDOW_MRU_FIRST…)
    connect(_windowMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildWindowMenu);
    rebuildWindowMenu();
}

void MainWindow::rebuildWindowMenu()
{
    for (QAction* action : _windowDocumentActions) {
        _windowMenu->removeAction(action);
        delete action;
    }
    _windowDocumentActions.clear();
    EditorPane* pane = _activePane ? _activePane : _mainPane;
    if (!pane)
        return;
    const std::vector<BufferId> ids = pane->buffers();
    constexpr std::size_t kMax = 40; // IDM_WINDOW_MRU_LIMIT - IDM_WINDOW_MRU_FIRST + 1
    for (std::size_t i = 0; i < ids.size() && i < kMax; ++i) {
        const Buffer* buf = _documents.buffer(ids[i]);
        if (!buf)
            continue;
        // BuildMenuFileName: «&1: nombre», con «*» si tiene cambios sin guardar
        QString name = buf->displayName();
        name.replace(QLatin1Char('&'), QStringLiteral("&&"));
        const QString number = i < 9 ? QStringLiteral("&%1").arg(i + 1) : QString::number(i + 1);
        auto* action = new QAction(QStringLiteral("%1: %2%3").arg(number, name, buf->isDirty() ? QStringLiteral("*") : QString()),
                                   _windowMenu);
        action->setObjectName(QStringLiteral("actionWindowDocument%1").arg(i));
        action->setCheckable(true);
        action->setChecked(ids[i] == pane->currentBuffer());
        const BufferId id = ids[i];
        connect(action, &QAction::triggered, this, [this, pane, id] {
            if (pane->contains(id)) {
                pane->activateBuffer(id);
                setActivePane(pane);
                pane->editor()->setFocus();
            }
        });
        _windowMenu->addAction(action);
        _windowDocumentActions.push_back(action);
    }
    _windowDocumentsStart->setVisible(!_windowDocumentActions.empty());
}

std::vector<WindowsDialog::Row> MainWindow::windowRows() const
{
    std::vector<WindowsDialog::Row> rows;
    EditorPane* pane = _activePane ? _activePane : _mainPane;
    for (const BufferId id : pane->buffers()) {
        const Buffer* buf = _documents.buffer(id);
        if (!buf)
            continue;
        WindowsDialog::Row row;
        row.id = id;
        row.name = buf->displayName();
        row.path = buf->isUntitled() ? QString() : buf->filePath();
        row.type = buf->hasNamedLanguage()
            ? buf->userLanguage()
            : QCoreApplication::translate("LangType", languageInfo(buf->language()).longName);
        row.size = static_cast<qint64>(_documents.text(id).size());
        row.modified = buf->isDirty();
        rows.push_back(row);
    }
    return rows;
}

void MainWindow::sortTabs(TabSort by, bool ascending)
{
    // WindowsDlg::sort + doSortToTabs, sobre la vista actual
    EditorPane* pane = _activePane ? _activePane : _mainPane;
    std::vector<WindowsDialog::Row> rows = windowRows();
    auto modified = [this](BufferId id) {
        const Buffer* buf = _documents.buffer(id);
        return buf && !buf->isUntitled() ? buf->lastModified() : QDateTime();
    };
    auto less = [&](const WindowsDialog::Row& a, const WindowsDialog::Row& b) {
        switch (by) {
        case TabSort::Name: return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
        case TabSort::Path: return QString::compare(a.path, b.path, Qt::CaseInsensitive) < 0;
        case TabSort::Type: return QString::compare(a.type, b.type, Qt::CaseInsensitive) < 0;
        case TabSort::Size: return a.size < b.size;
        case TabSort::Modified: return modified(a.id) < modified(b.id);
        }
        return false;
    };
    std::stable_sort(rows.begin(), rows.end(), [&](const auto& a, const auto& b) { return ascending ? less(a, b) : less(b, a); });
    std::vector<BufferId> order;
    for (const auto& row : rows)
        order.push_back(row.id);
    pane->reorder(order);
}

void MainWindow::showWindowsDialog()
{
    if (!_windowsDialog) {
        _windowsDialog = new WindowsDialog(this);
        auto paneNow = [this] { return _activePane ? _activePane : _mainPane; };
        connect(_windowsDialog, &WindowsDialog::activateRequested, this, [paneNow](BufferId id) {
            EditorPane* pane = paneNow();
            if (pane->contains(id)) {
                pane->activateBuffer(id);
                pane->editor()->setFocus();
            }
        });
        connect(_windowsDialog, &WindowsDialog::saveRequested, this, [this](const std::vector<BufferId>& ids) {
            for (const BufferId id : ids) {
                if (const Buffer* buf = _documents.buffer(id); buf && buf->isDirty())
                    saveBuffer(id);
            }
            _windowsDialog->setRows(windowRows(), currentBuffer());
        });
        connect(_windowsDialog, &WindowsDialog::closeRequested, this, [this, paneNow](const std::vector<BufferId>& ids) {
            closeBuffers(ids, false, paneNow());
            _windowsDialog->setRows(windowRows(), currentBuffer());
        });
        connect(_windowsDialog, &WindowsDialog::sortTabsRequested, this,
                [paneNow](const std::vector<BufferId>& order) { paneNow()->reorder(order); });
    }
    _windowsDialog->setRows(windowRows(), currentBuffer());
    _windowsDialog->show();
    _windowsDialog->raise();
    _windowsDialog->activateWindow();
}

void MainWindow::showRunDialog()
{
    if (!_runDialog) {
        _runDialog = new RunDialog(this);
        connect(_runDialog, &RunDialog::runRequested, this, [this](const QString& command) {
            // Como N++: si se lanzó, queda en el historial y el diálogo se cierra
            if (runCommand(command)) {
                _runHistory.removeAll(command);
                _runHistory.prepend(command);
                _runDialog->setHistory(_runHistory);
                _runDialog->accept();
            }
        });
        connect(_runDialog, &RunDialog::saveRequested, this, [this](const QString& command) { saveUserCommand(command); });
    }
    _runDialog->setHistory(_runHistory);
    _runDialog->show();
    _runDialog->raise();
    _runDialog->activateWindow();
    _runDialog->combo()->setFocus();
}

QString MainWindow::expandRunCommand(const QString& command) const
{
    RunVariables v;
    EditorView* view = _activePane ? _activePane->editor() : nullptr;
    if (const Buffer* buf = _documents.buffer(_activePane ? _activePane->currentBuffer() : kInvalidBuffer))
        v.fullCurrentPath = buf->isUntitled() ? buf->displayName() : buf->filePath();
    if (view) {
        v.currentWord = QString::fromStdString(selectedTextOrWord(*view));
        const sptr_t pos = view->sci(SCI_GETCURRENTPOS);
        const sptr_t line = view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos));
        v.line = line + 1;
        v.column = view->sci(SCI_GETCOLUMN, static_cast<uptr_t>(pos)) + 1;
        const sptr_t start = view->sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
        const sptr_t end = view->sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line));
        const auto* text = reinterpret_cast<const char*>(view->sci(SCI_GETRANGEPOINTER, static_cast<uptr_t>(start), end - start));
        v.currentLineText = text ? QString::fromUtf8(text, static_cast<qsizetype>(end - start)) : QString();
    }
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    v.appPath = appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage;
    v.appDirectory = QFileInfo(v.appPath).absolutePath();
    return expandRunVariables(command, v);
}

bool MainWindow::runCommand(const QString& command)
{
    const QString expanded = expandRunCommand(command).trimmed();
    if (expanded.isEmpty())
        return false;
    if (isUrlCommand(expanded)) {
        if (QDesktopServices::openUrl(QUrl(expanded)))
            return true;
        _prompter->warn(this, tr("Run"), tr("Could not open %1").arg(expanded));
        return false;
    }
    // En la carpeta del documento (o en la personal); lo que pone el AppImage no pasa
    QString directory = QDir::homePath();
    if (const Buffer* buf = _documents.buffer(currentBuffer()); buf && !buf->isUntitled())
        directory = QFileInfo(buf->filePath()).absolutePath();
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    for (const char* name : {"APPIMAGE", "APPDIR", "ARGV0", "OWD", "APPIMAGE_EXTRACT_AND_RUN"})
        environment.remove(QString::fromLatin1(name));
    QProcess process;
    process.setProgram(QStringLiteral("/bin/sh"));
    process.setArguments({QStringLiteral("-c"), expanded});
    process.setWorkingDirectory(directory);
    process.setProcessEnvironment(environment);
    process.setStandardInputFile(QProcess::nullDevice());
    if (process.startDetached())
        return true;
    _prompter->warn(this, tr("Run"), tr("Could not run %1").arg(expanded));
    return false;
}

bool MainWindow::saveUserCommand(const QString& command)
{
    QString name;
    QKeySequence shortcut;
    if (command.trimmed().isEmpty() || !_prompter->askNameAndShortcut(this, tr("Save Command"), name, shortcut))
        return false;
    UserCommand c;
    c.name = name;
    c.command = command.trimmed();
    if (!shortcut.isEmpty()) {
        const QKeyCombination combo = shortcut[0];
        c.ctrl = combo.keyboardModifiers().testFlag(Qt::ControlModifier);
        c.alt = combo.keyboardModifiers().testFlag(Qt::AltModifier);
        c.shift = combo.keyboardModifiers().testFlag(Qt::ShiftModifier);
        c.key = virtualKeyFromQtKey(combo.key());
    }
    _userCommands.push_back(c);
    const bool ok = writeUserCommands(shortcutsPath(), _userCommands);
    rebuildRunMenu();
    if (_manual)
        _manual->reload();
    return ok;
}

bool MainWindow::deleteUserCommand(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= _userCommands.size())
        return false;
    _userCommands.erase(_userCommands.begin() + index);
    const bool ok = writeUserCommands(shortcutsPath(), _userCommands);
    rebuildRunMenu();
    return ok;
}

void MainWindow::rebuildMacroMenu()
{
    for (QAction* action : _savedMacroActions) {
        _macroMenu->removeAction(action);
        delete action;
    }
    _savedMacroActions.clear();
    for (std::size_t i = 0; i < _savedMacros.size(); ++i) {
        const SavedMacro& macro = _savedMacros[i];
        auto* action = new QAction(macro.name, _macroMenu);
        action->setObjectName(QStringLiteral("actionMacro%1").arg(i));
        if (const int key = qtKeyFromVirtualKey(macro.key)) {
            Qt::KeyboardModifiers modifiers;
            modifiers.setFlag(Qt::ControlModifier, macro.ctrl);
            modifiers.setFlag(Qt::AltModifier, macro.alt);
            modifiers.setFlag(Qt::ShiftModifier, macro.shift);
            action->setShortcut(QKeySequence(QKeyCombination(modifiers, static_cast<Qt::Key>(key))));
        }
        connect(action, &QAction::triggered, this, [this, i] {
            if (i < _savedMacros.size())
                playMacro(_savedMacros[i].steps);
        });
        _macroMenu->addAction(action);
        _savedMacroActions.push_back(action);
    }
    if (!_savedMacros.empty()) {
        auto* remove = new QMenu(tr("&Delete a Macro"), _macroMenu);
        remove->setObjectName(QStringLiteral("menuDeleteMacro"));
        for (std::size_t i = 0; i < _savedMacros.size(); ++i) {
            remove->addAction(_savedMacros[i].name, this, [this, i] {
                if (_prompter->confirm(this, tr("Delete macro"), tr("Delete the macro \"%1\"?").arg(_savedMacros[i].name)))
                    deleteSavedMacro(static_cast<int>(i));
            });
        }
        QAction* separator = _macroMenu->addSeparator();
        _savedMacroActions.push_back(separator);
        _macroMenu->addMenu(remove);
        _savedMacroActions.push_back(remove->menuAction());
    }
    refreshMacroActions();
}

void MainWindow::refreshMacroActions()
{
    // checkMacroState
    _recordAction->setText(_recordingMacro ? tr("S&top Recording") : tr("Start &Recording"));
    _playAction->setEnabled(!_recordingMacro && !_macro.empty());
    _saveMacroAction->setEnabled(!_recordingMacro && !_macro.empty());
    _runMacroAction->setEnabled(!_recordingMacro && (!_macro.empty() || !_savedMacros.empty()));
}

void MainWindow::toggleMacroRecording()
{
    // IDM_MACRO_STARTRECORDINGMACRO / STOPRECORDINGMACRO, en las dos vistas
    _recordingMacro = !_recordingMacro;
    if (_recordingMacro)
        _macro.clear();
    for (EditorPane* pane : {_mainPane, _subPane})
        pane->editor()->sci(_recordingMacro ? SCI_STARTRECORD : SCI_STOPRECORD);
    statusBar()->showMessage(_recordingMacro ? tr("Recording macro…") : tr("Macro recorded: %1 steps").arg(_macro.size()), 3000);
    refreshMacroActions();
}

void MainWindow::playMacroSteps(const Macro& macro, std::vector<SciDocument>& undoDocs)
{
    for (const MacroStep& step : macro) {
        EditorView* view = editor();
        // Un grupo de deshacer por documento tocado (se cierran al final)
        const SciDocument doc = view->sci(SCI_GETDOCPOINTER);
        if (std::find(undoDocs.begin(), undoDocs.end(), doc) == undoDocs.end()) {
            view->sci(SCI_BEGINUNDOACTION);
            undoDocs.push_back(doc);
        }
        if (step.type == MacroStep::MenuCommand) {
            if (QAction* action = actionForCommand(static_cast<int>(step.wParam)))
                action->trigger();
            continue;
        }
        playScintillaStep(*view, step);
        // Como al escribir: sangría automática y cierre de pares
        const bool insertsText = step.message == SCI_REPLACESEL || step.message == SCI_ADDTEXT
                              || step.message == SCI_INSERTTEXT || step.message == SCI_APPENDTEXT;
        if (insertsText && step.type == MacroStep::UseSParameter && !step.sParam.empty())
            emit view->charAdded(static_cast<unsigned char>(step.sParam.back()));
    }
}

void MainWindow::endMacroUndo(const std::vector<SciDocument>& undoDocs)
{
    for (const SciDocument doc : undoDocs) {
        bool exists = false;
        for (std::size_t i = 0; i < _documents.count(); ++i)
            exists = exists || _documents.bufferAt(i)->document() == doc;
        if (!exists)
            continue; // una orden de la macro cerró el documento
        _searchView->send(SCI_SETDOCPOINTER, 0, doc);
        _searchView->send(SCI_ENDUNDOACTION);
        _searchView->send(SCI_SETDOCPOINTER, 0, _searchViewDefaultDoc);
    }
}

void MainWindow::playMacro(const Macro& macro)
{
    if (_recordingMacro || macro.empty())
        return;
    const QScopedValueRollback playing(_playingMacro, true);
    std::vector<SciDocument> undoDocs;
    playMacroSteps(macro, undoDocs);
    endMacroUndo(undoDocs);
}

void MainWindow::runMacro(const Macro& macro, int times)
{
    if (_recordingMacro || macro.empty())
        return;
    const QScopedValueRollback playing(_playingMacro, true);
    std::vector<SciDocument> undoDocs;
    auto currentLine = [this] {
        return editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(editor()->sci(SCI_GETCURRENTPOS)));
    };
    // WM_MACRODLGRUNMACRO: "hasta el final del archivo" mientras la línea avance
    // (o retroceda) siempre en el mismo sentido y no se quede quieta
    int counter = 0;
    sptr_t lastLine = editor()->sci(SCI_GETLINECOUNT) - 1;
    sptr_t currLine = currentLine();
    sptr_t deltaLastLine = 0;
    sptr_t deltaCurrLine = 0;
    bool cursorMovedUp = false;
    for (;;) {
        playMacroSteps(macro, undoDocs);
        ++counter;
        if (times >= 0) {
            if (counter >= times)
                break;
            continue;
        }
        if (counter > 2 && cursorMovedUp != (deltaCurrLine < 0) && deltaLastLine >= 0)
            break;
        cursorMovedUp = deltaCurrLine < 0;
        deltaLastLine = editor()->sci(SCI_GETLINECOUNT) - 1 - lastLine;
        deltaCurrLine = currentLine() - currLine;
        if (deltaCurrLine == 0 && deltaLastLine >= 0)
            break;
        if (deltaLastLine < deltaCurrLine)
            lastLine += deltaLastLine;
        currLine += deltaCurrLine;
        if (currLine > lastLine || currLine < 0
            || (deltaCurrLine == 0 && currLine == 0 && (deltaLastLine >= 0 || cursorMovedUp)))
            break;
    }
    endMacroUndo(undoDocs);
}

bool MainWindow::saveCurrentMacro()
{
    // IDM_MACRO_SAVECURRENTMACRO → addCurrentMacro
    if (_macro.empty() || _recordingMacro)
        return false;
    QString name;
    QKeySequence shortcut;
    if (!_prompter->askNameAndShortcut(this, tr("Save Macro"), name, shortcut))
        return false;
    SavedMacro macro;
    macro.name = name;
    macro.steps = _macro;
    if (!shortcut.isEmpty()) {
        const QKeyCombination combo = shortcut[0];
        macro.ctrl = combo.keyboardModifiers().testFlag(Qt::ControlModifier);
        macro.alt = combo.keyboardModifiers().testFlag(Qt::AltModifier);
        macro.shift = combo.keyboardModifiers().testFlag(Qt::ShiftModifier);
        macro.key = virtualKeyFromQtKey(combo.key());
    }
    _savedMacros.push_back(std::move(macro));
    if (!writeMacros(shortcutsPath(), _savedMacros)) {
        _prompter->warn(this, tr("Macros"), tr("Could not save %1").arg(shortcutsPath()));
        return false;
    }
    rebuildMacroMenu();
    return true;
}

bool MainWindow::deleteSavedMacro(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= _savedMacros.size())
        return false;
    _savedMacros.erase(_savedMacros.begin() + index);
    const bool ok = writeMacros(shortcutsPath(), _savedMacros);
    rebuildMacroMenu();
    return ok;
}

void MainWindow::showRunMacroDialog()
{
    // RunMacroDlg: qué macro, y cuántas veces o hasta el final del archivo
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Run a Macro Multiple Times"));
    auto* macros = new QComboBox(&dialog);
    if (!_macro.empty())
        macros->addItem(tr("Current recorded macro"), -1);
    for (std::size_t i = 0; i < _savedMacros.size(); ++i)
        macros->addItem(_savedMacros[i].name, static_cast<int>(i));
    auto* timesRadio = new QRadioButton(tr("Run"), &dialog);
    auto* times = new QSpinBox(&dialog);
    times->setRange(1, 1000000);
    times->setSuffix(tr(" times"));
    auto* eofRadio = new QRadioButton(tr("Run until the end of file"), &dialog);
    timesRadio->setChecked(true);
    auto* timesRow = new QHBoxLayout;
    timesRow->addWidget(timesRadio);
    timesRow->addWidget(times, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Run"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto* form = new QFormLayout(&dialog);
    form->addRow(tr("Macro:"), macros);
    form->addRow(timesRow);
    form->addRow(eofRadio);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted || macros->count() == 0)
        return;
    const int index = macros->currentData().toInt();
    const Macro macro = index < 0 ? _macro : _savedMacros[static_cast<std::size_t>(index)].steps;
    runMacro(macro, eofRadio->isChecked() ? -1 : times->value());
}

// --- Plugins -------------------------------------------------------------------------------

QStringList MainWindow::pluginDirectories() const
{
    QStringList dirs;
    for (const QString& dir : qEnvironmentVariable("EDITA_PLUGIN_PATH").split(QLatin1Char(':'), Qt::SkipEmptyParts))
        dirs << dir;
    dirs << QDir(paths::baseConfigDir()).filePath(QStringLiteral("plugins"));
    // Junto al binario (build de desarrollo) y en <prefijo>/<libdir>/<id>/plugins (instalado)
    const QDir app(QCoreApplication::applicationDirPath());
    dirs << app.filePath(QStringLiteral("plugins"));
    for (const char* lib : {kInstallLibDir, "lib", "lib64"})
        dirs << app.filePath(QStringLiteral("../%1/%2/plugins").arg(QLatin1String(lib), QString::fromUtf8(kAppId)));
    dirs << app.filePath(QStringLiteral("../PlugIns/%1/plugins").arg(QString::fromUtf8(kAppId))); // paquete .app de macOS
    dirs.removeDuplicates();
    return dirs;
}

void MainWindow::loadPlugins()
{
    _plugins = std::make_unique<PluginManager>(this, _mainPane->editor(), _subPane->editor(),
        [this](unsigned int message, uintptr_t wParam, intptr_t lParam) { return handlePluginMessage(message, wParam, lParam); });
    _pluginWarnings = _plugins->loadFrom(pluginDirectories());
    registerExternalLexers();

    // Menú Plugins: un submenú por plugin con sus órdenes (PluginsManager::setMenu)
    _pluginsMenu = new QMenu(tr("&Plugins"), this);
    _pluginsMenu->setObjectName(QStringLiteral("menuPlugins"));
    menuBar()->insertMenu(menuBar()->actions().last(), _pluginsMenu); // antes de Ayuda
    // Como en N++, el administrador de plugins va el primero
    QAction* admin = _pluginsMenu->addAction(tr("Plugins &Admin…"), this, [this] { showPluginsAdmin(); });
    admin->setObjectName(QStringLiteral("actionPluginsAdmin"));
    _pluginsMenu->addSeparator();
    for (const auto& plugin : _plugins->plugins()) {
        QMenu* sub = _pluginsMenu->addMenu(plugin->name);
        sub->setObjectName(QStringLiteral("plugin:") + plugin->name); // sus textos son del plugin
        int index = -1;
        for (const PluginManager::Command& command : plugin->commands) {
            ++index; // internalID de <PluginCommand>: la posición en getFuncsArray
            if (!command.function) {
                sub->addSeparator();
                continue;
            }
            QAction* action = sub->addAction(command.name);
            action->setProperty("pluginModule", QFileInfo(plugin->path).fileName());
            action->setProperty("pluginIndex", index);
            action->setProperty("defaultShortcut", command.shortcut);
            action->setObjectName(QStringLiteral("pluginCommand%1").arg(command.cmdId));
            action->setProperty("nppCommand", command.cmdId); // grabable en macros, como en N++
            action->setShortcut(command.shortcut);
            action->setCheckable(command.checked);
            action->setChecked(command.checked);
            connect(action, &QAction::triggered, this, [this, id = command.cmdId] {
                runMenuCommand(id, [this, id] { _plugins->runCommand(id); });
            });
        }
    }
    connect(_plugins.get(), &PluginManager::checkChanged, this, [this](int cmdId, bool checked) {
        if (auto* action = findChild<QAction*>(QStringLiteral("pluginCommand%1").arg(cmdId))) {
            action->setCheckable(true);
            action->setChecked(checked);
        }
    });
    if (!_plugins->plugins().empty())
        _pluginsMenu->addSeparator();
    _pluginsMenu->addAction(tr("Open Plugins &Folder"), this, [] {
        const QString dir = QDir(paths::baseConfigDir()).filePath(QStringLiteral("plugins"));
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    _pluginsMenu->addAction(tr("Plugins &Information…"), this, [this] {
        QStringList lines;
        for (const auto& plugin : _plugins->plugins())
            lines << QStringLiteral("%1 — %2").arg(plugin->name, plugin->path);
        if (lines.isEmpty())
            lines << tr("No plugins are loaded.");
        if (!_pluginWarnings.isEmpty())
            lines << QString() << tr("Could not be loaded:") << _pluginWarnings;
        lines << QString() << tr("Folders:") << pluginDirectories();
        QMessageBox::information(this, tr("Plugins"), lines.join(QLatin1Char('\n')));
    });
}

void MainWindow::notifyPlugins(unsigned int code, BufferId id)
{
    if (_plugins)
        _plugins->notifyHost(code, static_cast<uintptr_t>(id));
}

void MainWindow::notifyPluginsReady()
{
    notifyPlugins(NPPN_READY, currentBuffer());
}

intptr_t MainWindow::handlePluginMessage(unsigned int message, uintptr_t wParam, intptr_t lParam)
{
    // Subconjunto de los NPPM_* de Notepad_plus::process (NppBigSwitch.cpp), con cadenas UTF-8
    auto copyString = [&](const QString& text) -> intptr_t {
        const QByteArray utf8 = text.toUtf8();
        auto* buffer = reinterpret_cast<char*>(lParam);
        if (!buffer || wParam <= static_cast<uintptr_t>(utf8.size()))
            return 0; // como N++: FALSE si el búfer no basta
        std::memcpy(buffer, utf8.constData(), static_cast<std::size_t>(utf8.size()) + 1);
        return 1;
    };
    auto string = [&]() { return QString::fromUtf8(reinterpret_cast<const char*>(lParam)); };
    auto intOut = [&](int value) {
        if (auto* out = reinterpret_cast<int*>(lParam))
            *out = value;
        return intptr_t{1};
    };
    const Buffer* current = _documents.buffer(currentBuffer());
    const QString path = current ? (current->isUntitled() ? current->displayName() : current->filePath()) : QString();
    const QFileInfo info(path);

    switch (message) {
    case NPPM_GETCURRENTSCINTILLA:
        return intOut(_activePane == _subPane ? 1 : 0);
    case NPPM_GETCURRENTVIEW:
        return _activePane == _subPane ? 1 : 0;
    case NPPM_GETCURRENTLANGTYPE:
        return intOut(current ? langTypeForPlugins(*current) : 0);
    case NPPM_SETCURRENTLANGTYPE:
        if (lParam >= 0 && lParam < static_cast<intptr_t>(LangType::External))
            setLanguage(static_cast<LangType>(lParam));
        else if (const auto i = static_cast<std::size_t>(lParam - static_cast<intptr_t>(LangType::External));
                 lParam >= static_cast<intptr_t>(LangType::External) && i < _syntax.externalLexers().languages().size())
            setExternalLanguage(QString::fromStdString(_syntax.externalLexers().languages()[i].name));
        return 1;
    case NPPM_GETNBOPENFILES:
        if (lParam == PRIMARY_VIEW)
            return _mainPane->count();
        if (lParam == SECOND_VIEW)
            return _subPane->count();
        return static_cast<intptr_t>(allBuffers().size());
    case NPPM_SETSTATUSBAR:
        _statusBar->setFieldText(static_cast<int>(wParam), string());
        return 1;
    case NPPM_RELOADFILE: {
        const BufferId id = _documents.findByPath(string());
        if (id == kInvalidBuffer)
            return 0;
        return wParam ? reloadBuffer(id) : reloadFromDisk(id);
    }
    case NPPM_SWITCHTOFILE: {
        const BufferId id = _documents.findByPath(string());
        if (EditorPane* pane = id == kInvalidBuffer ? nullptr : paneContaining(id)) {
            pane->activateBuffer(id);
            setActivePane(pane);
            return 1;
        }
        return 0;
    }
    case NPPM_SAVECURRENTFILE:
        return current && current->isDirty() && saveBuffer(currentBuffer());
    case NPPM_SAVEALLFILES: {
        bool any = false;
        for (const BufferId id : allBuffers())
            any = any || (_documents.buffer(id) && _documents.buffer(id)->isDirty());
        return any && saveAll();
    }
    case NPPM_SETMENUITEMCHECK:
        return _plugins && _plugins->setChecked(static_cast<int>(wParam), lParam != 0);
    case NPPM_GETNATIVELANGFILENAME: {
        // Como N++: el nativeLang.xml en uso ("spanish.xml"); 0 si es el inglés de serie
        const QString code = i18n::currentLanguage();
        if (code == QLatin1String("en"))
            return 0;
        return copyString(QLocale::languageToString(QLocale(code).language()).toLower() + QStringLiteral(".xml"));
    }
    case NPPM_GETPLUGINSCONFIGDIR: {
        const QString dir = QDir(paths::configDir()).filePath(QStringLiteral("plugins/config"));
        QDir().mkpath(dir);
        return copyString(dir);
    }
    case NPPM_MENUCOMMAND: {
        const int id = static_cast<int>(lParam);
        if (_plugins && _plugins->command(id))
            return _plugins->runCommand(id);
        if (QAction* action = actionForCommand(id)) {
            action->trigger();
            return 1;
        }
        return 0;
    }
    case NPPM_GETNPPVERSION: {
        const QStringList parts = QString::fromUtf8(kAppVersionNumber).split(QLatin1Char('.'));
        const int major = parts.value(0).toInt();
        const int minor = parts.value(1).toInt() * 100 + parts.value(2).toInt() * 10;
        return (major << 16) | minor;
    }
    case NPPM_GETFULLPATHFROMBUFFERID: {
        const Buffer* buf = _documents.buffer(static_cast<BufferId>(wParam));
        if (!buf)
            return -1;
        const QByteArray utf8 = (buf->isUntitled() ? buf->displayName() : buf->filePath()).toUtf8();
        if (auto* out = reinterpret_cast<char*>(lParam))
            std::memcpy(out, utf8.constData(), static_cast<std::size_t>(utf8.size()) + 1);
        return utf8.size();
    }
    case NPPM_GETCURRENTBUFFERID:
        return static_cast<intptr_t>(currentBuffer());
    case NPPM_GETBUFFERLANGTYPE: {
        const Buffer* buf = _documents.buffer(static_cast<BufferId>(wParam));
        return buf ? static_cast<intptr_t>(langTypeForPlugins(*buf)) : -1;
    }
    case NPPM_SETBUFFERLANGTYPE: {
        Buffer* buf = _documents.buffer(static_cast<BufferId>(wParam));
        // Como N++: todos menos L_USER y L_EXTERNAL
        if (!buf || lParam < 0 || lParam == static_cast<intptr_t>(LangType::User) || lParam >= static_cast<intptr_t>(LangType::External))
            return 0;
        buf->setLanguage(static_cast<LangType>(lParam), true);
        return 1;
    }
    case NPPM_DOOPEN:
        return openFile(string()) != kInvalidBuffer;
    case NPPM_GETFULLCURRENTPATH:
        return copyString(path);
    case NPPM_GETCURRENTDIRECTORY:
        return copyString(current && !current->isUntitled() ? info.absolutePath() : QString());
    case NPPM_GETFILENAME:
        return copyString(info.fileName());
    case NPPM_GETNAMEPART:
        return copyString(info.completeBaseName());
    case NPPM_GETEXTPART:
        return copyString(info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix());
    case NPPM_GETCURRENTWORD: {
        EditorView* view = editor();
        const sptr_t start = view->sci(SCI_GETSELECTIONSTART);
        const sptr_t end = view->sci(SCI_GETSELECTIONEND);
        if (end > start)
            return copyString(QString::fromStdString(ViewSearchTarget(*view).text(start, end)));
        return copyString(textForFindField());
    }
    case NPPM_GETCURRENTLINE:
        return editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(editor()->sci(SCI_GETCURRENTPOS)));
    case NPPM_GETCURRENTCOLUMN:
        return editor()->sci(SCI_GETCOLUMN, static_cast<uptr_t>(editor()->sci(SCI_GETCURRENTPOS)));
    case NPPM_GETNPPDIRECTORY:
        return copyString(QCoreApplication::applicationDirPath());

    // --- Documentos y vistas -------------------------------------------------------------
    case NPPM_GETCURRENTDOCINDEX: {
        EditorPane* pane = lParam == SUB_VIEW ? _subPane : _mainPane;
        if (pane->count() == 0)
            return -1;
        const std::vector<BufferId> ids = pane->buffers();
        const auto it = std::find(ids.begin(), ids.end(), pane->currentBuffer());
        return it == ids.end() ? -1 : static_cast<intptr_t>(it - ids.begin());
    }
    case NPPM_ACTIVATEDOC: {
        EditorPane* pane = wParam == SUB_VIEW ? _subPane : _mainPane;
        const std::vector<BufferId> ids = pane->buffers();
        if (lParam < 0 || static_cast<std::size_t>(lParam) >= ids.size())
            return 0;
        pane->activateBuffer(ids[static_cast<std::size_t>(lParam)]);
        setActivePane(pane);
        return 1;
    }
    case NPPM_GETBUFFERIDFROMPOS: {
        const std::vector<BufferId> ids = (lParam == SUB_VIEW ? _subPane : _mainPane)->buffers();
        return wParam < ids.size() ? static_cast<intptr_t>(ids[wParam]) : 0;
    }
    case NPPM_GETPOSFROMBUFFERID: {
        // (vista << 30) | índice, buscando antes en la vista preferida
        EditorPane* panes[] = {lParam == SUB_VIEW ? _subPane : _mainPane, lParam == SUB_VIEW ? _mainPane : _subPane};
        for (EditorPane* pane : panes) {
            const std::vector<BufferId> ids = pane->buffers();
            const auto it = std::find(ids.begin(), ids.end(), static_cast<BufferId>(wParam));
            if (it != ids.end())
                return (intptr_t{pane == _subPane ? 1 : 0} << 30) | static_cast<intptr_t>(it - ids.begin());
        }
        return -1;
    }
    case NPPM_RELOADBUFFERID: {
        const auto id = static_cast<BufferId>(wParam);
        if (!_documents.buffer(id))
            return 0;
        return lParam ? reloadBuffer(id) : reloadFromDisk(id);
    }
    case NPPM_MAKECURRENTBUFFERDIRTY:
        if (Buffer* buf = _documents.buffer(currentBuffer()))
            buf->setDirty(true);
        return 1;
    case NPPM_GETBUFFERENCODING: {
        const Buffer* buf = _documents.buffer(static_cast<BufferId>(wParam));
        return buf ? static_cast<intptr_t>(buf->encoding().isUnicode() ? buf->encoding().unicodeMode : UniMode::Ansi) : -1;
    }
    case NPPM_SETBUFFERENCODING: {
        // Como N++: solo en documentos nuevos sin modificar
        Buffer* buf = _documents.buffer(static_cast<BufferId>(wParam));
        if (!buf || buf->isDirty() || !buf->isUntitled() || lParam < 0 || lParam > 7)
            return 0;
        const auto mode = static_cast<UniMode>(lParam);
        buf->setEncoding(mode == UniMode::Ansi ? TextEncoding::fromCodepage(_prefs.fallbackCodepage) : TextEncoding::unicode(mode));
        buf->setDirty(false); // Buffer::setUnicodeMode de N++ no lo deja pendiente de guardar
        return 1;
    }
    case NPPM_GETBUFFERFORMAT: {
        const Buffer* buf = _documents.buffer(static_cast<BufferId>(wParam));
        return buf ? static_cast<intptr_t>(buf->eol()) : -1;
    }
    case NPPM_SETBUFFERFORMAT: {
        const auto id = static_cast<BufferId>(wParam);
        EditorPane* pane = paneContaining(id);
        if (!pane || lParam < 0 || lParam > 2)
            return 0;
        EditorPane* previous = _activePane;
        const BufferId previousBuffer = currentBuffer();
        pane->activateBuffer(id);
        setActivePane(pane);
        convertEol(static_cast<EolType>(lParam));
        previous->activateBuffer(previousBuffer);
        setActivePane(previous);
        return 1;
    }
    case NPPM_SAVECURRENTFILEAS: {
        return current && saveBufferTo(currentBuffer(), absolutePath(string()), wParam != 0);
    }
    case NPPM_SAVEFILE: {
        const BufferId id = _documents.findByPath(absolutePath(string()));
        return id != kInvalidBuffer && saveBuffer(id);
    }
    case NPPM_GETLANGUAGENAME:
    case NPPM_GETLANGUAGEDESC: {
        if (wParam >= static_cast<uintptr_t>(LangType::External)) {
            // getLangDesc: de un lenguaje externo, su nombre
            const std::size_t i = wParam - static_cast<uintptr_t>(LangType::External);
            if (i >= _syntax.externalLexers().languages().size())
                return 0;
            const std::string& name = _syntax.externalLexers().languages()[i].name;
            if (auto* out = reinterpret_cast<char*>(lParam))
                std::memcpy(out, name.c_str(), name.size() + 1);
            return static_cast<intptr_t>(name.size());
        }
        const LanguageInfo& lang = languageInfo(static_cast<LangType>(wParam));
        const QByteArray text = QByteArray(message == NPPM_GETLANGUAGENAME ? lang.shortName : lang.longName);
        if (auto* out = reinterpret_cast<char*>(lParam))
            std::memcpy(out, text.constData(), static_cast<std::size_t>(text.size()) + 1);
        return text.size();
    }
    case NPPM_GETNBUSERLANG:
        if (auto* out = reinterpret_cast<int*>(lParam))
            *out = 0;
        return static_cast<intptr_t>(_syntax.userLanguages().languages().size());
    case NPPM_LAUNCHFINDINFILESDLG:
        showFindDialog(FindReplaceDialog::Tab::FindInFiles);
        return 1;

    // --- Sesiones --------------------------------------------------------------------------
    case NPPM_SAVECURRENTSESSION: {
        QString error;
        return writeSessionFile(string(), captureSession(), &error);
    }
    case NPPM_LOADSESSION: {
        Session session;
        if (!readSessionFile(string(), session))
            return 0;
        restoreSession(session);
        return 1;
    }
    case NPPM_GETNBSESSIONFILES: {
        Session session;
        if (!readSessionFile(string(), session))
            return 0;
        return static_cast<intptr_t>(session.mainView.size() + session.subView.size());
    }

    // --- Interfaz --------------------------------------------------------------------------
    case NPPM_GETMENUHANDLE:
        // En Edita el HMENU es un QMenu* (menú Plugins) o el QMenuBar*
        return wParam == NPPMAINMENU ? reinterpret_cast<intptr_t>(menuBar()) : reinterpret_cast<intptr_t>(_pluginsMenu);
    case NPPM_HIDETABBAR: {
        const bool previous = _prefs.tabBarHide;
        Preferences p = _prefs;
        p.tabBarHide = lParam != 0;
        setPreferences(p);
        return previous;
    }
    case NPPM_ISTABBARHIDDEN:
        return _prefs.tabBarHide;
    case NPPM_HIDESTATUSBAR: {
        const bool previous = !_prefs.statusBarShow;
        Preferences p = _prefs;
        p.statusBarShow = lParam == 0;
        setPreferences(p);
        return previous;
    }
    case NPPM_ISSTATUSBARHIDDEN:
        return !_prefs.statusBarShow;
    case NPPM_HIDEMENU: {
        const bool previous = menuBar()->isHidden();
        menuBar()->setVisible(lParam == 0);
        return previous;
    }
    case NPPM_ISMENUHIDDEN:
        return menuBar()->isHidden();
    case NPPM_GETEDITORDEFAULTFOREGROUNDCOLOR:
        return editor()->sci(SCI_STYLEGETFORE, STYLE_DEFAULT);
    case NPPM_GETEDITORDEFAULTBACKGROUNDCOLOR:
        return editor()->sci(SCI_STYLEGETBACK, STYLE_DEFAULT);
    case NPPM_ISDARKMODEENABLED:
        return 0;
    case NPPM_ISAUTOINDENTON:
        return _prefs.autoIndent != AutoIndent::None;
    case NPPM_GETCURRENTCMDLINE: {
        const QByteArray line = QCoreApplication::arguments().join(QLatin1Char(' ')).toUtf8();
        if (auto* out = reinterpret_cast<char*>(lParam)) {
            if (wParam <= static_cast<uintptr_t>(line.size()))
                return 0;
            std::memcpy(out, line.constData(), static_cast<std::size_t>(line.size()) + 1);
        }
        return line.size();
    }
    case NPPM_GETBOOKMARKID:
        return kBookmarkMarker;

    // --- Recursos para los plugins ---------------------------------------------------------
    case NPPM_ALLOCATECMDID:
    case NPPM_ALLOCATEMARKER:
    case NPPM_ALLOCATEINDICATOR: {
        // Mismos rangos que N++; se agotan como en N++ (FALSE y 0)
        int& next = message == NPPM_ALLOCATECMDID ? _nextDynamicCommand
                  : message == NPPM_ALLOCATEMARKER ? _nextMarker : _nextIndicator;
        // Marcadores 1–10 (N++: 1–14; Edita usa 11–14); indicadores 9–19 (N++: 9–20; Edita usa el 20, la ortografía)
        const int limit = message == NPPM_ALLOCATECMDID ? 25000 : message == NPPM_ALLOCATEMARKER ? 11 : 20;
        auto* out = reinterpret_cast<int*>(lParam);
        const int requested = static_cast<int>(wParam);
        if (!out || requested <= 0 || next + requested > limit) {
            if (out)
                *out = 0;
            return 0;
        }
        *out = next;
        next += requested;
        return 1;
    }
    case NPPM_GETPLUGINHOMEPATH: {
        // La carpeta del usuario donde se instalan los plugins
        const QByteArray dir = QDir(paths::baseConfigDir()).filePath(QStringLiteral("plugins")).toUtf8();
        if (auto* out = reinterpret_cast<char*>(lParam)) {
            if (wParam <= static_cast<uintptr_t>(dir.size()))
                return 0;
            std::memcpy(out, dir.constData(), static_cast<std::size_t>(dir.size()) + 1);
        }
        return dir.size();
    }
    case NPPM_CREATELEXER:
        // CreateLexer de Lexilla: el plugin lo pasa con SCI_SETILEXER a su editor
        return reinterpret_cast<intptr_t>(CreateLexer(reinterpret_cast<const char*>(lParam)));
    case NPPM_ADDSCNMODIFIEDFLAGS:
        for (EditorPane* pane : {_mainPane, _subPane}) {
            EditorView* view = pane->editor();
            view->sci(SCI_SETMODEVENTMASK, static_cast<uptr_t>(view->sci(SCI_GETMODEVENTMASK) | lParam));
        }
        return 1;
    case NPPM_GETSETTINGSONCLOUDPATH:
        return 0; // Edita no tiene ajustes en la nube
    case NPPM_CREATESCINTILLAHANDLE:
        return _plugins ? reinterpret_cast<intptr_t>(_plugins->createEditor(reinterpret_cast<QWidget*>(lParam))) : 0;
    case NPPM_DESTROYSCINTILLAHANDLE:
        return _plugins && _plugins->destroyEditor(reinterpret_cast<void*>(lParam));
    case NPPM_MSGTOPLUGIN:
        return _plugins && _plugins->sendToPlugin(QString::fromUtf8(reinterpret_cast<const char*>(wParam)), message, wParam, lParam);

    // --- Paneles acoplables ----------------------------------------------------------------
    case NPPM_DMMREGASDCKDLG:
        return lParam ? registerPluginDock(*reinterpret_cast<const EditaDockData*>(lParam)) : 0;
    case NPPM_DMMSHOW:
    case NPPM_DMMHIDE:
        if (PluginDock* dock = pluginDock(reinterpret_cast<QWidget*>(lParam))) {
            dock->dock->setVisible(message == NPPM_DMMSHOW);
            if (message == NPPM_DMMSHOW)
                dock->dock->raise();
            return 1;
        }
        return 0;
    case NPPM_DMMUPDATEDISPINFO:
        if (PluginDock* dock = pluginDock(reinterpret_cast<QWidget*>(lParam))) {
            dock->dock->setWindowTitle(dock->client->windowTitle().isEmpty() ? dock->name : dock->client->windowTitle());
            return 1;
        }
        return 0;
    case NPPM_DMMGETPLUGINHWNDBYNAME: {
        const QString name = QString::fromUtf8(reinterpret_cast<const char*>(wParam));
        const QString module = lParam ? QString::fromUtf8(reinterpret_cast<const char*>(lParam)) : QString();
        for (const PluginDock& dock : _pluginDocks) {
            if (dock.name == name && (module.isEmpty() || dock.module == module))
                return reinterpret_cast<intptr_t>(dock.client);
        }
        return 0;
    }
    default:
        return 0;
    }
}

MainWindow::PluginDock* MainWindow::pluginDock(const QWidget* client)
{
    for (PluginDock& dock : _pluginDocks) {
        if (dock.client == client)
            return &dock;
    }
    return nullptr;
}

intptr_t MainWindow::registerPluginDock(const EditaDockData& dockData)
{
    // DockingManager::createDockableDlg: el panel en un QDockWidget, en la zona por defecto
    auto* client = static_cast<QWidget*>(dockData.hClient);
    if (!client || pluginDock(client))
        return 0;
    PluginDock entry;
    entry.client = client;
    entry.name = QString::fromUtf8(dockData.pszName ? dockData.pszName : "");
    entry.module = QString::fromUtf8(dockData.pszModuleName ? dockData.pszModuleName : "");
    QString title = entry.name;
    if ((dockData.uMask & DWS_ADDINFO) && dockData.pszAddInfo && *dockData.pszAddInfo)
        title += QStringLiteral(" - ") + QString::fromUtf8(dockData.pszAddInfo);
    entry.dock = new QDockWidget(title, this);
    entry.dock->setObjectName(QStringLiteral("pluginDock_%1_%2").arg(entry.module).arg(dockData.dlgID));
    entry.dock->setWidget(client);
    const unsigned container = (dockData.uMask >> 28) & 0x3;
    static constexpr Qt::DockWidgetArea kAreas[] = {Qt::LeftDockWidgetArea, Qt::RightDockWidgetArea, Qt::TopDockWidgetArea,
                                                    Qt::BottomDockWidgetArea};
    addDockWidget(kAreas[container], entry.dock);
    entry.dock->setFloating((dockData.uMask & DWS_DF_FLOATING) != 0);
    entry.dock->hide(); // se muestra con NPPM_DMMSHOW, como en N++
    // Si el plugin destruye su ventana (DestroyWindow en la capa de N++), el panel se va con ella
    QDockWidget* dock = entry.dock;
    connect(client, &QObject::destroyed, dock, [this, client, dock] {
        const auto it = std::find_if(_pluginDocks.begin(), _pluginDocks.end(), [client](const PluginDock& d) { return d.client == client; });
        if (it == _pluginDocks.end())
            return; // Edita está cerrando sus paneles
        _pluginDocks.erase(it);
        dock->deleteLater();
    });
    _pluginDocks.push_back(entry);
    return 1;
}

// --- Administrador de plugins --------------------------------------------------------------

PluginsAdminContext MainWindow::pluginsAdminContext() const
{
    PluginsAdminContext context;
    context.pluginDirectories = pluginDirectories();
    context.userPluginsDir = QDir(paths::baseConfigDir()).filePath(QStringLiteral("plugins"));
    context.appVersion = PluginVersion::parse(QString::fromUtf8(kAppVersionNumber));

    // La lista de plugins (nppPluginList.json en N++): la de pruebas, la del usuario o la de Edita
    QString listPath = qEnvironmentVariable("EDITA_PLUGIN_LIST");
    if (listPath.isEmpty()) {
        const QString user = QDir(context.userPluginsDir).filePath(QStringLiteral("Config/editaPluginList.json"));
        listPath = QFileInfo::exists(user) ? user : paths::findDataFile(QStringLiteral("plugins/editaPluginList.json"));
    }
    QFile list(listPath);
    if (listPath.isEmpty() || !list.open(QIODevice::ReadOnly))
        context.catalogError = tr("editaPluginList.json not found");
    else if (!PluginCatalog::parse(list.readAll(), context.catalog, &context.catalogError))
        context.catalogError = QStringLiteral("%1: %2").arg(listPath, context.catalogError);
    context.catalog.sourcePath = listPath;
    // El catálogo de Edita: la última lista firmada que se aceptó (el diálogo descarga la nueva)
    context.bundledCatalog = context.catalog;
    context.catalogSettings = remote_catalog::settings(context.userPluginsDir);
    if (PluginCatalog cached; context.catalogSettings.enabled() && remote_catalog::loadCached(context.catalogSettings, cached))
        PluginsAdminDialog::useRemoteCatalog(context, std::move(cached));

    // Recetas locales: las del usuario y las de la distribución (share/edita/plugins/recipes).
    // Las del catálogo de Edita llegan con su lista firmada (recipes.zip)
    QStringList recipeDirs{QDir(context.userPluginsDir).filePath(QStringLiteral("recipes"))};
    for (const QString& dataDir : paths::dataDirs())
        recipeDirs << QDir(dataDir).filePath(QStringLiteral("plugins/recipes"));
    if (const QString extra = qEnvironmentVariable("EDITA_PLUGIN_RECIPES"); !extra.isEmpty())
        recipeDirs.prepend(extra);
    context.recipes = loadRecipes(recipeDirs);
    context.sdk = findPluginSdk();
    context.buildRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation))
                            .filePath(QString::fromUtf8(kAppId) + QStringLiteral("/plugin-builds"));
    context.localPackagesDir = QDir(context.userPluginsDir).filePath(QStringLiteral("local"));

    if (_plugins) {
        for (const auto& plugin : _plugins->plugins())
            context.loadedVersions.insert(plugin->path, plugin->version);
        const QString userDir = QDir(context.userPluginsDir).absolutePath() + QLatin1Char('/');
        for (const PluginManager::Failure& failure : _plugins->failures()) {
            PluginInfo info;
            info.folderName = QFileInfo(failure.path).completeBaseName();
            info.displayName = QFileInfo(failure.path).fileName();
            info.fullFilePath = failure.path;
            info.problem = failure.reason;
            info.isSystem = !QFileInfo(failure.path).absoluteFilePath().startsWith(userDir);
            context.loadFailures.push_back(info);
        }
    }
    return context;
}

void MainWindow::showPluginsAdmin()
{
    PluginsAdminDialog dialog(pluginsAdminContext(), *_prompter, this);
    dialog.exec();
    // N++ reinicia para aplicar los cambios; aquí se ofrece
    if (dialog.hasChanges()
        && _prompter->confirm(this, tr("Plugins Admin"),
                              tr("Plugin changes take effect when Edita restarts.\nRestart now?")))
        restartApplication();
}

bool MainWindow::restartApplication()
{
    // Como al salir: guarda la sesión y pregunta por los cambios; si se cancela, no se reinicia
    if (!close())
        return false;
    return QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
}

// --- Archivos recientes (LastRecentFileList) ------------------------------------------

void MainWindow::addRecentFile(const QString& path)
{
    if (_prefs.recentFilesMax <= 0)
        return;
    _recentFiles.removeAll(path);
    _recentFiles.prepend(path);
    while (_recentFiles.size() > _prefs.recentFilesMax)
        _recentFiles.removeLast();
    rebuildRecentFilesMenu();
}

void MainWindow::removeRecentFile(const QString& path)
{
    if (_recentFiles.removeAll(path) > 0)
        rebuildRecentFilesMenu();
}

void MainWindow::clearRecentFiles()
{
    _recentFiles.clear();
    rebuildRecentFilesMenu();
}

BufferId MainWindow::reopenLastClosed()
{
    while (!_recentFiles.isEmpty()) {
        const QString path = _recentFiles.front();
        if (QFileInfo::exists(path))
            return openFile(path);
        _recentFiles.removeFirst(); // ya no existe: fuera de la lista
    }
    rebuildRecentFilesMenu();
    return kInvalidBuffer;
}

QString MainWindow::recentFileLabel(const QString& path) const
{
    // customLength: -1 ruta completa, 0 solo el nombre, n acortada a n caracteres
    const QString native = QDir::toNativeSeparators(path);
    const int length = _prefs.recentFilesCustomLength;
    if (length == 0)
        return QFileInfo(path).fileName();
    if (length > 0 && native.size() > length) {
        const int head = std::max(1, length / 3);
        const int tail = std::max(1, length - head - 1);
        return native.left(head) + QStringLiteral("…") + native.right(tail);
    }
    return native;
}

void MainWindow::rebuildRecentFilesMenu()
{
    // La página de inicio también los muestra
    if (isWelcomeVisible())
        fillWelcomePage();
    if (!_fileMenu)
        return;
    for (QAction* action : _recentEntries) {
        _fileMenu->removeAction(action);
        delete action;
    }
    _recentEntries.clear();
    const QStringList files = _recentFiles.mid(0, std::max(0, _prefs.recentFilesMax));
    if (files.isEmpty())
        return;

    // Como N++: en la parte de abajo del menú Archivo, o en un submenú
    QMenu* target = _fileMenu;
    QAction* before = _recentAnchor;
    if (_prefs.recentFilesInSubMenu) {
        auto* sub = new QMenu(tr("&Recent Files"), _fileMenu);
        sub->setObjectName(QStringLiteral("menuRecentFiles"));
        QAction* subAction = sub->menuAction();
        _fileMenu->insertAction(_recentAnchor, subAction);
        _recentEntries.push_back(subAction);
        target = sub;
        before = nullptr;
    } else {
        QAction* separator = new QAction(_fileMenu);
        separator->setSeparator(true);
        _fileMenu->insertAction(_recentAnchor, separator);
        _recentEntries.push_back(separator);
    }
    auto add = [&](QAction* action) {
        target->insertAction(before, action);
        if (target == _fileMenu)
            _recentEntries.push_back(action);
    };
    for (int i = 0; i < files.size(); ++i) {
        const QString number = i < 9 ? QStringLiteral("&%1: ").arg(i + 1) : QStringLiteral("%1: ").arg(i + 1);
        auto* action = new QAction(number + recentFileLabel(files[i]), target);
        action->setObjectName(QStringLiteral("actionRecent%1").arg(i));
        action->setToolTip(files[i]);
        connect(action, &QAction::triggered, this, [this, path = files[i]] {
            if (!QFileInfo::exists(path)) {
                // N++ ofrece quitarlo de la lista
                if (_prompter->confirm(this, tr("Recent file"),
                                       tr("\"%1\" no longer exists.\nRemove it from the list?").arg(QDir::toNativeSeparators(path))))
                    removeRecentFile(path);
                return;
            }
            openFile(path);
        });
        add(action);
    }
    auto* separator = new QAction(target);
    separator->setSeparator(true);
    add(separator);
    auto* openAll = new QAction(tr("Open All Recent Files"), target);
    openAll->setObjectName(QStringLiteral("actionOpenAllRecent"));
    connect(openAll, &QAction::triggered, this, [this] {
        const QStringList all = _recentFiles;
        for (const QString& path : all) {
            if (QFileInfo::exists(path))
                openFile(path);
        }
    });
    add(openAll);
    auto* clear = new QAction(tr("Empty Recent Files List"), target);
    clear->setObjectName(QStringLiteral("actionClearRecent"));
    connect(clear, &QAction::triggered, this, &MainWindow::clearRecentFiles);
    add(clear);
}

// --- Cambios externos y copias (Seguridad de los datos) --------------------------------

void MainWindow::setSessionEnabled(bool enabled)
{
    _sessionEnabled = enabled;
    updateBackupTimer();
}

void MainWindow::setPreferences(const Preferences& prefs)
{
    const bool languageChanged = prefs.language != _prefs.language;
    const bool languageMenuChanged = prefs.langMenuCompact != _prefs.langMenuCompact
                                  || prefs.excludedLanguages != _prefs.excludedLanguages;
    _prefs = prefs;
    if (languageMenuChanged && _languageMenu) {
        populateLanguageMenu();
        refreshMenuChecks();
    }
    storePreferences(_syntax.config(), _prefs);
    _syntax.config().save();
    applyPreferences();
    // Como N++ con nativeLang.xml: el idioma nuevo se ve al reiniciar
    if (languageChanged
        && i18n::resolveLanguage(_prefs.language, QLocale::system().uiLanguages()) != i18n::currentLanguage()
        && _prompter->confirm(this, tr("Localization"),
                              tr("The new language will be used when Edita restarts.\nRestart now?")))
        QTimer::singleShot(0, this, &MainWindow::restartApplication);
}

bool MainWindow::isSnapshotMode() const
{
    return _prefs.snapshotMode && _prefs.rememberLastSession && _sessionEnabled;
}

void MainWindow::updateBackupTimer()
{
    if (!_backupTimer)
        return;
    if (isSnapshotMode()) {
        _backupTimer->start(std::max(1000, _prefs.snapshotBackupTiming));
    } else {
        _backupTimer->stop();
    }
}

void MainWindow::backupNow()
{
    if (!isSnapshotMode())
        return;
    // N++ solo copia el documento actual, porque es el único que se edita; aquí se
    // recorren todos (buscar y reemplazar en todos los abiertos también los cambia)
    const QString dir = snapshotBackupDir();
    bool sessionChanged = false;
    for (std::size_t i = 0; i < _documents.count(); ++i) {
        bool pathChanged = false;
        _documents.backupBuffer(_documents.bufferAt(i)->id(), dir, &pathChanged);
        sessionChanged = sessionChanged || pathChanged;
    }
    // La sesión apunta a las copias: si cambian, se guarda ya (por si hay un corte)
    if (sessionChanged)
        writeSessionFile(sessionPath(), captureSession());
}

std::vector<BufferId> MainWindow::visibleBuffers() const
{
    // FileManager::checkFilesystemChanges(true): el actual y el de la otra vista si se ve
    std::vector<BufferId> ids;
    if (const BufferId id = _activePane->currentBuffer(); id != kInvalidBuffer)
        ids.push_back(id);
    const EditorPane* other = otherPane(_activePane);
    if (other->isVisible() && other->count() > 0 && other->currentBuffer() != ids.front())
        ids.push_back(other->currentBuffer());
    return ids;
}

void MainWindow::scheduleFileCheck()
{
    if (_fileCheckPending)
        return;
    _fileCheckPending = true;
    // Fuera de la señal que lo provoca: la comprobación puede abrir diálogos
    QTimer::singleShot(0, this, [this] {
        _fileCheckPending = false;
        if (isActiveWindow())
            checkModifiedDocuments();
    });
}

void MainWindow::checkModifiedDocuments()
{
    if (_checkingFiles || !_prefs.detectsChanges())
        return;
    const QScopedValueRollback guard(_checkingFiles, true);
    const std::vector<BufferId> ids = _prefs.detectsAllFiles() ? allBuffers() : visibleBuffers();
    for (const BufferId id : ids) {
        if (_documents.buffer(id)) // una respuesta anterior pudo cerrarlo
            handleFileState(id, _documents.checkFileState(id));
    }
    syncWatchedFiles();
}

void MainWindow::handleFileState(BufferId id, DocumentManager::FileState state)
{
    // Notepad_plus::notifyBufferChanged (DOC_MODIFIED / DOC_DELETED)
    const Buffer* buf = _documents.buffer(id);
    if (!buf || state == DocumentManager::FileState::Unchanged)
        return;
    const QString path = QDir::toNativeSeparators(buf->filePath());
    auto showIt = [this, id] {
        if (EditorPane* pane = paneContaining(id)) {
            pane->activateBuffer(id);
            setActivePane(pane);
        }
    };

    if (state == DocumentManager::FileState::Modified) {
        if (buf->isMonitoring()) { // tail -f: sin preguntar, y al final
            reloadFromDisk(id);
            for (EditorPane* pane : {_mainPane, _subPane}) {
                if (pane->currentBuffer() == id)
                    pane->editor()->sci(SCI_DOCUMENTEND);
            }
            return;
        }
        if (_prefs.autoUpdates() && !buf->isDirty()) {
            reloadFromDisk(id); // sin preguntar
            return;
        }
        showIt();
        const QString question = buf->isDirty()
            ? tr("\"%1\"\n\nThis file has been modified by another program.\nDo you want to reload it and "
                 "lose the changes made in %2?").arg(path, QString::fromUtf8(kAppName))
            : tr("\"%1\"\n\nThis file has been modified by another program.\nDo you want to reload it?").arg(path);
        if (_prompter->confirm(this, tr("Reload"), question))
            reloadFromDisk(id);
        else
            _documents.markDirtyUntilSaved(id); // ya no coincide con el disco
        return;
    }

    // Borrado: mantenerlo (modificado, para poder guardarlo de nuevo) o cerrarlo
    showIt();
    if (_prompter->confirm(this, tr("Keep non existing file"),
                           tr("The file \"%1\" doesn't exist anymore.\nKeep this file in editor?").arg(path)))
        return;
    discardBuffer(id);
    if (_mainPane->count() == 0 && _subPane->count() == 0) {
        _activePane = _mainPane;
        newDocument();
    }
    updatePaneVisibility();
}

void MainWindow::syncWatchedFiles()
{
    if (!_fileWatcher)
        return;
    QStringList wanted;
    for (std::size_t i = 0; i < _documents.count(); ++i) {
        const Buffer* buf = _documents.bufferAt(i);
        if (!buf->isUntitled() && QFileInfo::exists(buf->filePath()))
            wanted << buf->filePath();
    }
    const QStringList watched = _fileWatcher->files();
    QStringList toRemove;
    for (const QString& path : watched) {
        if (!wanted.contains(path))
            toRemove << path;
    }
    QStringList toAdd;
    for (const QString& path : wanted) {
        if (!watched.contains(path))
            toAdd << path;
    }
    if (!toRemove.isEmpty())
        _fileWatcher->removePaths(toRemove);
    if (!toAdd.isEmpty())
        _fileWatcher->addPaths(toAdd);
}

bool MainWindow::maybeSave(BufferId id, bool offerAll, bool& saveRest, bool& discardRest)
{
    const Buffer* buf = _documents.buffer(id);
    if (!buf || !buf->isDirty() || discardRest || _discardOnClose)
        return true;
    if (saveRest)
        return saveBuffer(id);
    if (EditorPane* pane = paneContaining(id)) { // como N++: se muestra el documento sobre el que se pregunta
        pane->activateBuffer(id);
        setActivePane(pane);
    }
    switch (_prompter->askSaveChanges(this, buf->displayName(), offerAll)) {
    case Prompter::SaveChoice::Save:
        return saveBuffer(id);
    case Prompter::SaveChoice::SaveAll:
        saveRest = true;
        return saveBuffer(id);
    case Prompter::SaveChoice::Discard:
        return true;
    case Prompter::SaveChoice::DiscardAll:
        discardRest = true;
        return true;
    case Prompter::SaveChoice::Cancel:
        break;
    }
    return false;
}

void MainWindow::discardBuffer(BufferId id, bool keepBackup)
{
    _mainPane->removeBuffer(id);
    _subPane->removeBuffer(id);
    releaseDocument(id, keepBackup);
}

void MainWindow::removeFromPane(BufferId id, EditorPane* pane)
{
    pane->removeBuffer(id);
    if (!otherPane(pane)->contains(id))
        releaseDocument(id, false);
}

void MainWindow::releaseDocument(BufferId id, bool keepBackup)
{
    // doClose: el archivo pasa a "recientes" (salvo al salir recordando la sesión)
    if (const Buffer* buf = _documents.buffer(id); buf && !buf->isUntitled() && !_lockRecentFiles
                                                     && QFileInfo::exists(buf->filePath()))
        addRecentFile(buf->filePath());
    notifyPlugins(NPPN_FILEBEFORECLOSE, id);
    _documents.closeBuffer(id, keepBackup);
    notifyPlugins(NPPN_FILECLOSED, id);
}

bool MainWindow::closeBuffer(BufferId id)
{
    // fileClose: en la vista activa; si el documento está clonado en la otra,
    // solo se quita de esta vista y no se pregunta nada
    EditorPane* pane = paneContaining(id);
    const Buffer* buf = _documents.buffer(id);
    if (!pane || !buf)
        return false;
    EditorPane* other = otherPane(pane);
    if (!other->contains(id)) {
        bool saveRest = false;
        bool discardRest = false;
        if (!maybeSave(id, false, saveRest, discardRest))
            return false;
    }

    if (pane->count() == 1) {
        if (other->count() > 0) {
            // La vista se queda vacía: se oculta (hideView) y la otra pasa a ser la activa
            removeFromPane(id, pane);
            updatePaneVisibility();
            setActivePane(other);
            return true;
        }
        if (_prefs.quitOnEmpty) { // TabBar quitOnEmpty: cerrar el último documento cierra la ventana
            removeFromPane(id, pane);
            QTimer::singleShot(0, this, &QWidget::close);
            return true;
        }
        // N++ no cierra el último documento si ya es un "nuevo" vacío
        buf = _documents.buffer(id);
        if (buf->isUntitled() && !buf->isDirty() && _documents.text(id).empty())
            return true;
        removeFromPane(id, pane);
        newDocument();
        return true;
    }
    removeFromPane(id, pane);
    return true;
}

bool MainWindow::closeBuffers(const std::vector<BufferId>& ids, bool exiting, EditorPane* onlyPane)
{
    // Se pregunta por los modificados que dejan de estar abiertos en cualquier vista.
    // Al salir en modo snapshot no se pregunta por los que tienen copia
    // (fileCloseAll(false, isSnapshotMode)): se restaurarán en el próximo arranque.
    const bool snapshotExit = exiting && isSnapshotMode();
    auto mustAsk = [this, onlyPane, snapshotExit](BufferId id) {
        const Buffer* buf = _documents.buffer(id);
        if (!buf || !buf->isDirty() || (onlyPane && otherPane(onlyPane)->contains(id)))
            return false;
        return !(snapshotExit && !buf->backupPath().isEmpty() && QFileInfo::exists(buf->backupPath()));
    };
    const auto dirty = std::count_if(ids.begin(), ids.end(), mustAsk);
    bool saveRest = false;
    bool discardRest = false;
    for (const BufferId id : ids) {
        if (mustAsk(id) && !maybeSave(id, dirty > 1, saveRest, discardRest))
            return false;
    }
    for (const BufferId id : ids) {
        if (onlyPane)
            removeFromPane(id, onlyPane);
        else
            discardBuffer(id, snapshotExit);
    }
    if (!exiting && _mainPane->count() == 0 && _subPane->count() == 0) {
        if (_prefs.quitOnEmpty) {
            QTimer::singleShot(0, this, &QWidget::close);
            return true;
        }
        _activePane = _mainPane;
        newDocument();
    }
    updatePaneVisibility();
    return true;
}

bool MainWindow::closeAll()
{
    return closeBuffers(allBuffers(), false);
}

bool MainWindow::closeAllBut(BufferId keep)
{
    std::vector<BufferId> ids = allBuffers();
    std::erase(ids, keep);
    return closeBuffers(ids, false);
}

bool MainWindow::closeAllButPinned()
{
    // fileCloseAllButPinned: en las dos vistas, las que no están fijadas
    std::vector<BufferId> ids;
    for (EditorPane* pane : {_mainPane, _subPane}) {
        for (const BufferId id : pane->buffers()) {
            const Buffer* buf = _documents.buffer(id);
            if (buf && !buf->isPinned() && std::find(ids.begin(), ids.end(), id) == ids.end())
                ids.push_back(id);
        }
    }
    return closeBuffers(ids, false);
}

bool MainWindow::closeAllUnchanged()
{
    // fileCloseAllUnchanged: en la vista activa, las que no tienen cambios (y las nuevas vacías)
    std::vector<BufferId> ids;
    for (const BufferId id : _activePane->buffers()) {
        const Buffer* buf = _documents.buffer(id);
        if (buf && (!buf->isDirty() || (buf->isUntitled() && _documents.text(id).empty())))
            ids.push_back(id);
    }
    return closeBuffers(ids, false, _activePane);
}

void MainWindow::goToTab(int index)
{
    DocTabBar* tabs = _activePane->tabBar();
    if (index >= 0 && index < tabs->count())
        tabs->setCurrentIndex(index);
}

void MainWindow::moveCurrentTab(int delta)
{
    // IDM_VIEW_TAB_MOVEFORWARD/BACKWARD, IDM_VIEW_GOTO_START/END: sin cruzar la frontera de las fijadas
    DocTabBar* tabs = _activePane->tabBar();
    const int from = tabs->currentIndex();
    if (from < 0)
        return;
    // Las fijadas van primero: [0, fijadas) y [fijadas, total)
    int pinnedCount = 0;
    for (int i = 0; i < tabs->count(); ++i)
        pinnedCount += tabs->isPinnedAt(i) ? 1 : 0;
    const bool pinned = tabs->isPinnedAt(from);
    const int first = pinned ? 0 : pinnedCount;
    const int last = pinned ? pinnedCount - 1 : tabs->count() - 1;
    const int to = std::clamp(from + delta, first, last);
    if (to != from)
        tabs->moveTab(from, to);
}

void MainWindow::toggleAlwaysOnTop()
{
    const bool on = !(windowFlags() & Qt::WindowStaysOnTopHint);
    setWindowFlag(Qt::WindowStaysOnTopHint, on);
    show(); // cambiar las banderas oculta la ventana
    if (QAction* a = findChild<QAction*>(QStringLiteral("actionAlwaysOnTop")))
        a->setChecked(on);
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen()) {
        if (_wasMaximized)
            showMaximized();
        else
            showNormal();
    } else {
        _wasMaximized = isMaximized();
        showFullScreen();
    }
    if (QAction* a = findChild<QAction*>(QStringLiteral("actionFullScreen")))
        a->setChecked(isFullScreen());
}

void MainWindow::togglePostIt()
{
    // IDM_VIEW_POSTIT: solo el texto, sin marco, menús, barras ni pestañas; F12 vuelve
    _postIt = !_postIt;
    if (_postIt) {
        // isHidden y no isVisible: con la página de inicio delante, las pestañas no se ven pero no están ocultas
        _postItState = {!menuBar()->isHidden(), _toolBar && !_toolBar->isHidden(), !_statusBar->isHidden(),
                        !_mainPane->tabBar()->isHidden(), !_subPane->tabBar()->isHidden()};
        menuBar()->hide();
        if (_toolBar)
            _toolBar->hide();
        _statusBar->hide();
        _mainPane->tabBar()->hide();
        _subPane->tabBar()->hide();
    } else {
        menuBar()->setVisible(_postItState.menu);
        if (_toolBar)
            _toolBar->setVisible(_postItState.toolBar);
        _statusBar->setVisible(_postItState.statusBar);
        _mainPane->tabBar()->setVisible(_postItState.mainTabs);
        _subPane->tabBar()->setVisible(_postItState.subTabs);
    }
    setWindowFlag(Qt::FramelessWindowHint, _postIt);
    show();
    if (QAction* a = findChild<QAction*>(QStringLiteral("actionPostIt")))
        a->setChecked(_postIt);
}

void MainWindow::setScrollSync(bool vertical)
{
    // IDM_VIEW_SYNSCROLLV/H: la diferencia de ahora entre las dos vistas se mantiene al desplazar una
    QAction* action = findChild<QAction*>(QString::fromLatin1(vertical ? "actionSyncScrollV" : "actionSyncScrollH"));
    const bool on = action && action->isChecked();
    EditorView* main = _mainPane->editor();
    EditorView* sub = _subPane->editor();
    if (vertical) {
        _syncScrollV = on;
        _syncLineOffset = sub->sci(SCI_GETFIRSTVISIBLELINE) - main->sci(SCI_GETFIRSTVISIBLELINE);
    } else {
        _syncScrollH = on;
        _syncXOffset = sub->sci(SCI_GETXOFFSET) - main->sci(SCI_GETXOFFSET);
    }
}

void MainWindow::syncScroll(EditorView* source)
{
    if (_syncingScroll || (!_syncScrollV && !_syncScrollH) || !_subPane->isVisible() || !_mainPane->isVisible())
        return;
    EditorView* main = _mainPane->editor();
    EditorView* sub = _subPane->editor();
    EditorView* target = source == main ? sub : main;
    const sptr_t sign = source == main ? 1 : -1;
    _syncingScroll = true;
    if (_syncScrollV)
        target->sci(SCI_SETFIRSTVISIBLELINE,
                    static_cast<uptr_t>(std::max<sptr_t>(0, source->sci(SCI_GETFIRSTVISIBLELINE) + sign * _syncLineOffset)));
    if (_syncScrollH)
        target->sci(SCI_SETXOFFSET, static_cast<uptr_t>(std::max<sptr_t>(0, source->sci(SCI_GETXOFFSET) + sign * _syncXOffset)));
    _syncingScroll = false;
}

void MainWindow::insertText(const QString& text)
{
    EditorView* view = editor();
    view->sci(SCI_BEGINUNDOACTION);
    view->sends(SCI_REPLACESEL, 0, text.toUtf8().constData());
    view->sci(SCI_ENDUNDOACTION);
}

QStringList MainWindow::openDocumentNames(bool paths) const
{
    QStringList names;
    std::vector<BufferId> seen;
    for (EditorPane* pane : {_mainPane, _subPane}) {
        for (const BufferId id : pane->buffers()) {
            const Buffer* buf = _documents.buffer(id);
            if (!buf || std::find(seen.begin(), seen.end(), id) != seen.end())
                continue;
            seen.push_back(id);
            names << (paths && !buf->isUntitled() ? buf->filePath() : buf->displayName());
        }
    }
    return names;
}

void MainWindow::multiSelect(int searchFlags, bool all)
{
    EditorView* view = editor();
    _multiSelectFlags = searchFlags;
    view->sci(SCI_TARGETWHOLEDOCUMENT);
    view->sci(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(searchFlags));
    if (!all) {
        view->sci(SCI_MULTIPLESELECTADDNEXT);
        return;
    }
    // Sin selección, la palabra del cursor (expandWordSelection). Con GETSELECTIONSTART/END y no
    // con «hay selección»: si la principal está vacía, SCI_MULTIPLESELECTADDEACH no termina nunca
    if (view->sci(SCI_GETSELECTIONSTART) == view->sci(SCI_GETSELECTIONEND)) {
        const sptr_t caret = view->sci(SCI_GETCURRENTPOS);
        const sptr_t start = view->sci(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(caret), 1);
        const sptr_t end = view->sci(SCI_WORDENDPOSITION, static_cast<uptr_t>(caret), 1);
        if (start == end)
            return;
        view->sci(SCI_SETSEL, static_cast<uptr_t>(start), end);
    }
    view->sci(SCI_MULTIPLESELECTADDEACH);
}

QString MainWindow::documentSummary() const
{
    // IDM_VIEW_SUMMARY de N++, con sus mismas cuentas
    const Buffer* buf = _documents.buffer(currentBuffer());
    EditorView* view = editor();
    if (!buf)
        return {};
    const QLocale locale = QLocale::system();
    auto number = [&locale](qint64 n) { return locale.toString(n); };
    QStringList lines;
    if (!buf->isUntitled() && QFileInfo::exists(buf->filePath())) {
        const QFileInfo info(buf->filePath());
        lines << tr("Full file path: %1").arg(buf->filePath());
        const QDateTime created = info.birthTime();
        lines << tr("Created: %1").arg(created.isValid() ? locale.toString(created, QLocale::ShortFormat) : tr("unknown"));
        lines << tr("Modified: %1").arg(locale.toString(info.lastModified(), QLocale::ShortFormat));
    }
    const QString text = QString::fromStdString(_documents.text(currentBuffer()));
    qint64 chars = 0;
    for (const QChar c : text) {
        if (c != QLatin1Char('\r') && c != QLatin1Char('\n') && !c.isLowSurrogate())
            ++chars;
    }
    // wordCount: lo que no es espacio ni puntuación
    static const QRegularExpression word(QStringLiteral("[^ \\t\\\\.,;:!?()+\\r\\n\\-*/=\\][{}&~\"'`|@$%<>^]+"));
    qint64 words = 0;
    for (QRegularExpressionMatchIterator it = word.globalMatch(text); it.hasNext(); it.next())
        ++words;
    lines << tr("Characters (without line endings): %1").arg(number(chars));
    lines << tr("Words: %1").arg(number(words));
    lines << tr("Lines: %1").arg(number(view->sci(SCI_GETLINECOUNT)));
    lines << tr("Document length: %1").arg(number(view->sci(SCI_GETLENGTH)));
    qint64 selChars = 0, selBytes = 0, ranges = 0;
    const sptr_t selections = view->sci(SCI_GETSELECTIONS);
    for (sptr_t i = 0; i < selections; ++i) {
        const sptr_t start = view->sci(SCI_GETSELECTIONNSTART, static_cast<uptr_t>(i));
        const sptr_t end = view->sci(SCI_GETSELECTIONNEND, static_cast<uptr_t>(i));
        if (start == end)
            continue;
        ++ranges;
        selBytes += end - start;
        selChars += view->sci(SCI_COUNTCHARACTERS, static_cast<uptr_t>(start), end);
    }
    lines << tr("%1 selected characters (%2 bytes) in %3 ranges").arg(number(selChars), number(selBytes), number(ranges));
    return lines.join(QLatin1Char('\n'));
}

void MainWindow::toggleMonitoring()
{
    // IDM_VIEW_MONITORING: el archivo, en solo lectura, se vuelve a leer al cambiar y va al final
    Buffer* buf = _documents.buffer(currentBuffer());
    if (!buf)
        return;
    if (buf->isMonitoring()) {
        buf->setMonitoring(false);
        buf->setUserReadOnly(false);
    } else if (buf->isUntitled() || !QFileInfo::exists(buf->filePath())) {
        _prompter->warn(this, tr("Monitoring problem"), tr("The file should exist to be monitored."));
    } else if (buf->isDirty()) {
        _prompter->warn(this, tr("Monitoring problem"),
                        tr("The document is dirty. Please save the modification before monitoring it."));
    } else {
        buf->setMonitoring(true);
        buf->setUserReadOnly(true);
        editor()->sci(SCI_DOCUMENTEND);
        checkMonitoredDocuments(); // por si cambió justo ahora
    }
    if (!_monitorTimer) {
        // Además del aviso del sistema de archivos: una comprobación por segundo de los seguidos,
        // también con Edita en segundo plano (un registro sigue creciendo)
        _monitorTimer = new QTimer(this);
        _monitorTimer->setInterval(1000);
        connect(_monitorTimer, &QTimer::timeout, this, &MainWindow::checkMonitoredDocuments);
    }
    bool any = false;
    for (std::size_t i = 0; i < _documents.count(); ++i)
        any = any || _documents.bufferAt(i)->isMonitoring();
    if (any)
        _monitorTimer->start();
    else
        _monitorTimer->stop();
    refreshMenuChecks();
}

void MainWindow::checkMonitoredDocuments()
{
    for (std::size_t i = 0; i < _documents.count(); ++i) {
        const Buffer* buf = _documents.bufferAt(i);
        if (!buf->isMonitoring())
            continue;
        const BufferId id = buf->id();
        if (_documents.checkFileState(id) != DocumentManager::FileState::Modified)
            continue;
        reloadFromDisk(id);
        for (EditorPane* pane : {_mainPane, _subPane}) {
            if (pane->currentBuffer() == id)
                pane->editor()->sci(SCI_DOCUMENTEND);
        }
    }
}

bool MainWindow::hashSelectionToClipboard(QCryptographicHash::Algorithm algorithm)
{
    // IDM_TOOL_*_GENERATEINTOCLIPBOARD: con una sola selección, no vacía
    EditorView* view = editor();
    if (view->sci(SCI_GETSELECTIONS) != 1 || view->sci(SCI_GETSELECTIONEMPTY))
        return false;
    const sptr_t start = view->sci(SCI_GETSELECTIONSTART);
    const sptr_t end = view->sci(SCI_GETSELECTIONEND);
    const std::string text = _documents.text(currentBuffer()).substr(static_cast<std::size_t>(start),
                                                                     static_cast<std::size_t>(end - start));
    QApplication::clipboard()->setText(HashDialog::digest(QByteArray::fromStdString(text), algorithm));
    return true;
}

bool MainWindow::renameBuffer(BufferId id)
{
    Buffer* buf = _documents.buffer(id);
    if (!buf)
        return false;
    if (!buf->isUntitled()) {
        // fileRename: el archivo, con el diálogo de guardar en su carpeta y con su nombre
        if (!QFileInfo::exists(buf->filePath())) {
            _prompter->warn(this, tr("Rename"), tr("The file %1 no longer exists.").arg(QDir::toNativeSeparators(buf->filePath())));
            return false;
        }
        const QString target = _prompter->saveFileName(this, buf->filePath(), tr("All types (*)"), QString());
        if (target.isEmpty() || QFileInfo(target).absoluteFilePath() == buf->filePath())
            return false;
        if (const BufferId other = _documents.findByPath(target); other != kInvalidBuffer && other != id) {
            _prompter->warn(this, tr("Rename"), tr("%1 is open in Edita. Close it first.").arg(QDir::toNativeSeparators(target)));
            return false;
        }
        notifyPlugins(NPPN_FILEBEFORERENAME, id);
        QString error;
        const bool ok = _documents.moveFile(id, target, &error);
        notifyPlugins(ok ? NPPN_FILERENAMED : NPPN_FILERENAMECANCEL, id);
        if (!ok)
            _prompter->warn(this, tr("Rename"), tr("Cannot rename %1:\n%2").arg(QDir::toNativeSeparators(buf->filePath()), error));
        return ok;
    }
    // Sin archivo, solo la pestaña («Rename Current Tab» de N++)
    QString name = buf->displayName();
    if (!_prompter->askText(this, tr("Rename Current Tab"), tr("New name:"), name))
        return false;
    name = name.trimmed();
    if (name == buf->displayName())
        return false;
    QString problem;
    if (name.isEmpty())
        problem = tr("The specified name cannot be empty, or it cannot contain only space(s) or TAB(s).");
    else if (name.contains(QLatin1Char('/')))
        problem = tr("The name cannot contain a slash (/).");
    for (std::size_t i = 0; problem.isEmpty() && i < _documents.count(); ++i) {
        const Buffer* other = _documents.bufferAt(i);
        if (other->id() != id && other->displayName() == name)
            problem = tr("The specified name is already in use on another tab.");
    }
    if (!problem.isEmpty()) {
        _prompter->warn(this, tr("Rename failed"), problem);
        return false;
    }
    notifyPlugins(NPPN_FILEBEFORERENAME, id);
    _documents.setUntitledName(id, name);
    notifyPlugins(NPPN_FILERENAMED, id);
    return true;
}

bool MainWindow::moveBufferToTrash(BufferId id)
{
    // fileDelete: pregunta, lo manda a la papelera y lo cierra sin preguntar por los cambios
    const Buffer* buf = _documents.buffer(id);
    if (!buf || buf->isUntitled() || !QFileInfo::exists(buf->filePath()))
        return false;
    const QString path = buf->filePath();
    if (!_prompter->confirm(this, tr("Move to Trash"),
                            tr("Are you sure you want to move %1 to the trash?").arg(QDir::toNativeSeparators(path))))
        return false;
    notifyPlugins(NPPN_FILEBEFOREDELETE, id);
    if (!_trashFile(path)) {
        notifyPlugins(NPPN_FILEDELETEFAILED, id);
        _prompter->warn(this, tr("Move to Trash"), tr("Could not move %1 to the trash.").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    notifyPlugins(NPPN_FILEDELETED, id);
    _discardOnClose = true;
    while (_documents.buffer(id) && paneContaining(id)) { // también si está clonado en la otra vista
        if (!closeBuffer(id))
            break;
    }
    _discardOnClose = false;
    return true;
}

bool MainWindow::openInDefaultViewer(BufferId id)
{
    // IDM_FILE_OPEN_DEFAULT_VIEWER: como hacer doble clic en el administrador de archivos
    const Buffer* buf = _documents.buffer(id);
    if (!buf || buf->isUntitled() || !QFileInfo::exists(buf->filePath()))
        return false;
    if (_openUrl(QUrl::fromLocalFile(buf->filePath())))
        return true;
    _prompter->warn(this, tr("Open in Default Viewer"),
                    tr("There is no application to open %1.").arg(QDir::toNativeSeparators(buf->filePath())));
    return false;
}

QString MainWindow::selectedPath() const
{
    EditorView* view = editor();
    if (view->sci(SCI_GETSELECTIONS) != 1) // varias selecciones o en columna: nada (como N++)
        return {};
    const sptr_t start = view->sci(SCI_GETSELECTIONSTART);
    const sptr_t end = view->sci(SCI_GETSELECTIONEND);
    ViewSearchTarget target(*view);
    QString text;
    if (end > start) {
        text = QString::fromStdString(target.text(start, end));
    } else {
        // Sin selección: lo que parece una ruta alrededor del cursor
        const sptr_t caret = view->sci(SCI_GETCURRENTPOS);
        const sptr_t line = view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(caret));
        const sptr_t lineStart = view->sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
        const sptr_t lineEnd = view->sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line));
        const QString before = QString::fromStdString(target.text(lineStart, caret));
        text = fileNameAtColumn(QString::fromStdString(target.text(lineStart, lineEnd)), static_cast<int>(before.size()));
    }
    const Buffer* buf = _documents.buffer(currentBuffer());
    const QString dir = buf && !buf->isUntitled() ? QFileInfo(buf->filePath()).absolutePath() : QDir::currentPath();
    return resolveSelectedPath(text, dir);
}

bool MainWindow::openSelectedPath(bool folder)
{
    const QString path = selectedPath();
    if (path.isEmpty())
        return false;
    const QFileInfo info(path);
    if (folder ? !info.exists() : !info.isFile()) {
        _prompter->warn(this, tr("Open Path"), tr("The path you're trying to open doesn't exist.\n%1").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    if (folder) // la carpeta que la contiene (o ella misma, si es una carpeta)
        return _openUrl(QUrl::fromLocalFile(info.isDir() ? info.absoluteFilePath() : info.absolutePath()));
    return openFile(info.absoluteFilePath()) != kInvalidBuffer;
}

void MainWindow::redactSelection(bool bullet)
{
    EditorView* view = editor();
    if (view->sci(SCI_GETREADONLY))
        return;
    ViewSearchTarget target(*view);
    view->sci(SCI_BEGINUNDOACTION);
    const sptr_t count = view->sci(SCI_GETSELECTIONS);
    for (sptr_t i = 0; i < count; ++i) {
        // Las selecciones se mueven con cada cambio: se leen cada vez
        const sptr_t start = view->sci(SCI_GETSELECTIONNSTART, static_cast<uptr_t>(i));
        const sptr_t end = view->sci(SCI_GETSELECTIONNEND, static_cast<uptr_t>(i));
        if (end <= start)
            continue;
        const QByteArray masked = redactText(QString::fromStdString(target.text(start, end)), bullet).toUtf8();
        view->sci(SCI_SETTARGETRANGE, static_cast<uptr_t>(start), end);
        view->sends(SCI_REPLACETARGET, static_cast<uptr_t>(masked.size()), masked.constData());
    }
    view->sci(SCI_ENDUNDOACTION);
}

bool MainWindow::searchOnInternet()
{
    // IDM_EDIT_SEARCHONINTERNET: la selección o, sin ella, la palabra del cursor
    EditorView* view = editor();
    if (view->sci(SCI_GETSELECTIONS) != 1)
        return false;
    const sptr_t start = view->sci(SCI_GETSELECTIONSTART);
    const sptr_t end = view->sci(SCI_GETSELECTIONEND);
    const QString text = end > start ? QString::fromStdString(ViewSearchTarget(*view).text(start, end)) : textForFindField();
    if (text.trimmed().isEmpty())
        return false;
    return _openUrl(searchOnInternetUrl(static_cast<SearchEngine>(_prefs.searchEngineChoice), _prefs.searchEngineCustom, text));
}

bool MainWindow::isLargeFileIn(const EditorPane* pane) const
{
    const Buffer* buf = pane ? _documents.buffer(pane->currentBuffer()) : nullptr;
    return buf && buf->isLargeFile();
}

void MainWindow::applyDocumentOptions(EditorPane* pane)
{
    EditorView* view = pane->editor();
    // Preferencias › Delimitadores: los caracteres de palabra son del documento (CharClassify),
    // así que van cada vez que la vista cambia de documento. Los de serie de Scintilla son las
    // letras, los dígitos, _ y todo lo que no es ASCII (sin lista, SCI_SETWORDCHARS no los
    // deja así sino sin ninguno: se dan siempre), más los ASCII elegidos
    QByteArray chars = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int c = 0x80; c < 0x100; ++c)
        chars += static_cast<char>(c);
    if (!_prefs.wordCharsDefault) {
        for (const QChar c : _prefs.wordCharsAdded) {
            if (c.unicode() > ' ' && c.unicode() < 0x7F && !chars.contains(static_cast<char>(c.unicode())))
                chars += static_cast<char>(c.unicode());
        }
    }
    view->sends(SCI_SETWORDCHARS, 0, chars.constData());
    // Preferencias › Rendimiento: en un archivo grande, los paréntesis y los enlaces si se permiten
    const bool large = isLargeFileIn(pane);
    view->setBraceMatchEnabled(!large || _prefs.largeFileBraceMatch);
    refreshUrls(pane);
    scheduleSpelling(pane);
}

void MainWindow::loadSpellDictionaries()
{
    // Preferencias › Ortografía: los elegidos o, sin elegir, los del idioma del sistema y el inglés
    if (!_spell)
        _spell = std::make_unique<SpellChecker>();
    QStringList codes = _prefs.spellDictionaries.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (codes.isEmpty())
        codes = SpellChecker::defaultDictionaries(QLocale::system().name(), SpellChecker::availableDictionaries());
    if (codes != _spellCodes || !_spell->isLoaded()) {
        _spellCodes = codes;
        _spell->load(codes);
    }
    _spell->setPersonalDictionary(QDir(paths::configDir()).filePath(QStringLiteral("personalDictionary.txt")));
}

void MainWindow::scheduleSpelling(EditorPane* pane)
{
    // Al escribir o desplazarse, un momento después (no en cada tecla)
    const int index = pane == _subPane ? 1 : 0;
    if (!_spellTimer[index]) {
        _spellTimer[index] = new QTimer(this);
        _spellTimer[index]->setSingleShot(true);
        _spellTimer[index]->setInterval(200);
        connect(_spellTimer[index], &QTimer::timeout, this, [this, pane] { checkSpelling(pane); });
    }
    _spellTimer[index]->start();
}

void MainWindow::checkSpelling(EditorPane* pane)
{
    if (!_spell)
        loadSpellDictionaries();
    const Buffer* buf = _documents.buffer(pane->currentBuffer());
    const bool enabled = _prefs.spellCheck && buf && !buf->isLargeFile();
    std::array<bool, 256> styles{};
    if (enabled) {
        const QString key = QString::fromLatin1(languageInfo(buf->language()).name) + QLatin1Char('/') + buf->userLanguage()
                          + (_prefs.spellOnlyCommentsAndStrings ? QStringLiteral("/c") : QStringLiteral("/a"));
        auto it = _spellStyles.find(key);
        if (it == _spellStyles.end())
            it = _spellStyles.emplace(key, spellCheckedStyles(*pane->editor(), _syntax.styles().lexer(languageInfo(buf->language()).name),
                                                             _prefs.spellOnlyCommentsAndStrings)).first;
        styles = it->second;
    }
    highlightMisspellings(*pane->editor(), *_spell, styles, enabled);
}

void MainWindow::checkSpellingNow()
{
    for (EditorPane* pane : {_mainPane, _subPane})
        checkSpelling(pane);
}

void MainWindow::refreshUrls(EditorPane* pane)
{
    UrlLinkOptions options;
    options.mode = _prefs.urlMode;
    options.extraSchemes = _prefs.uriSchemes;
    options.allowed = !isLargeFileIn(pane) || _prefs.largeFileClickableLink;
    highlightUrls(*pane->editor(), options);
}

bool MainWindow::openLinkAt(sptr_t position)
{
    // SCN_DOUBLECLICK sobre URL_INDIC (NppNotification.cpp): se quita la palabra que seleccionó
    // el doble clic y se abre la dirección
    EditorView* view = editor();
    const QString url = urlAt(*view, position);
    if (url.isEmpty())
        return false;
    view->sci(SCI_SETSEL, static_cast<uptr_t>(position), position);
    return _openUrl(QUrl(url));
}

bool MainWindow::selectDelimitedAt(sptr_t position)
{
    // Ctrl + doble clic: en la línea del clic o, con «Varias líneas», en todo el documento
    EditorView* view = editor();
    const char left = static_cast<char>(_prefs.leftDelimiter.unicode());
    const char right = static_cast<char>(_prefs.rightDelimiter.unicode());
    sptr_t base = 0;
    std::string text;
    if (_prefs.delimiterSeveralLines) {
        text = _documents.text(currentBuffer());
    } else {
        const sptr_t line = view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(position));
        base = view->sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
        text = ViewSearchTarget(*view).text(base, view->sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line)));
    }
    if (position < base)
        return false;
    const auto [open, close] = delimiterRange(text, static_cast<std::size_t>(position - base), left, right);
    if (open < 0)
        return false;
    view->sci(SCI_SETSEL, static_cast<uptr_t>(base + open + 1), base + close);
    return true;
}

QString MainWindow::contextMenuPath() const
{
    // contextMenu.xml del usuario; la primera vez, el de serie (NppParameters::load)
    const QString path = QDir(paths::configDir()).filePath(QStringLiteral("contextMenu.xml"));
    if (!QFileInfo::exists(path)) {
        paths::ensureConfigDir();
        if (const QString model = paths::findDataFile(QStringLiteral("contextMenu.xml")); !model.isEmpty()) {
            QFile::copy(model, path);
            QFile(path).setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
        }
    }
    return path;
}

void MainWindow::fillMenuFromXml(QMenu* menu, const std::vector<ContextMenuItem>& items)
{
    // NppParameters::getContextMenuFromXmlTree + ContextMenu::create: cada elemento es una orden
    // del menú principal (por id o por su nombre en inglés), de un plugin o un separador; con
    // FolderName, en un submenú. Lo que no existe en Edita se salta
    std::map<int, QAction*> byId;
    for (QAction* action : findChildren<QAction*>()) {
        const QString name = action->objectName();
        if (!name.startsWith(QLatin1String("action")))
            continue;
        int id = nppCommandFor(name.toLatin1().constData());
        if (id <= 0)
            id = shortcutOnlyCommandFor(name);
        if (id > 0 && !byId.count(id))
            byId[id] = action;
    }
    // Por el texto del menú en el idioma de ahora (sin & ni «…»), dentro de un menú
    std::function<QAction*(const QMenu*, const QString&)> findByText = [&findByText](const QMenu* in, const QString& text) -> QAction* {
        for (QAction* action : in->actions()) {
            if (action->menu()) {
                if (QAction* found = findByText(action->menu(), text))
                    return found;
            } else if (!action->isSeparator() && contextMenuText(action->text()).compare(text, Qt::CaseInsensitive) == 0) {
                return action;
            }
        }
        return nullptr;
    };
    auto topMenu = [this](const QString& title) -> QMenu* {
        for (QAction* action : menuBar()->actions()) {
            if (action->menu() && contextMenuText(action->text()).compare(contextMenuText(title), Qt::CaseInsensitive) == 0)
                return action->menu();
        }
        return nullptr;
    };
    // Los TranslateID de los submenús del contextMenu.xml de serie
    auto folderTitle = [](const QString& folder) {
        if (folder == QLatin1String("Style all occurrences of token"))
            return tr("Style all occurrences of token");
        if (folder == QLatin1String("Style one token"))
            return tr("Style one token");
        if (folder == QLatin1String("Clear style"))
            return tr("Clear style");
        if (folder == QLatin1String("Plugin commands"))
            return tr("Plugin commands");
        // Los de tabContextMenu_example.xml
        if (folder == QLatin1String("Close Multiple Tabs"))
            return tr("Close Multiple Tabs");
        if (folder == QLatin1String("Open into"))
            return tr("Open into");
        if (folder == QLatin1String("Copy to Clipboard"))
            return tr("Copy to Clipboard");
        if (folder == QLatin1String("Move Document"))
            return tr("Move Document");
        if (folder == QLatin1String("Apply Color to Tab"))
            return tr("Apply Color to Tab");
        return folder;
    };
    std::map<QString, QMenu*> folders;
    bool pendingSeparator = false;
    for (const ContextMenuItem& item : items) {
        if (item.isSeparator()) {
            if (item.folder.isEmpty())
                pendingSeparator = !menu->actions().isEmpty();
            else if (const auto it = folders.find(item.folder); it != folders.end())
                it->second->addSeparator();
            continue;
        }
        QAction* action = nullptr;
        if (item.id > 0) {
            if (const auto it = byId.find(item.id); it != byId.end())
                action = it->second;
        } else if (!item.menuEntry.isEmpty() && !item.menuItem.isEmpty()) {
            if (const int id = nppCommandIdFromName(item.menuEntry, item.menuItem); id > 0)
                if (const auto it = byId.find(id); it != byId.end())
                    action = it->second;
            if (!action) // el nombre en el idioma de Edita
                if (const QMenu* top = topMenu(item.menuEntry))
                    action = findByText(top, contextMenuText(item.menuItem));
        } else if (!item.pluginEntry.isEmpty() && !item.pluginCommand.isEmpty() && _pluginsMenu) {
            for (QAction* entry : _pluginsMenu->actions()) {
                if (entry->menu() && contextMenuText(entry->text()).compare(contextMenuText(item.pluginEntry), Qt::CaseInsensitive) == 0) {
                    action = findByText(entry->menu(), contextMenuText(item.pluginCommand));
                    break;
                }
            }
        }
        if (!action)
            continue;
        QMenu* target = menu;
        if (!item.folder.isEmpty()) {
            auto it = folders.find(item.folder);
            if (it == folders.end()) {
                if (pendingSeparator)
                    menu->addSeparator();
                pendingSeparator = false;
                it = folders.emplace(item.folder, menu->addMenu(folderTitle(item.folder))).first;
            }
            target = it->second;
        } else if (pendingSeparator) {
            menu->addSeparator();
            pendingSeparator = false;
        }
        if (item.displayAs.isEmpty()) {
            target->addAction(action);
        } else {
            // ItemNameAs: otro texto para la misma orden
            QAction* alias = target->addAction(item.displayAs);
            alias->setEnabled(action->isEnabled());
            connect(alias, &QAction::triggered, action, &QAction::trigger);
        }
    }
}

QMenu* MainWindow::buildEditorContextMenu()
{
    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("editorContextMenu"));
    // Una palabra mal escrita bajo el cursor: las sugerencias, añadirla al diccionario o pasarla por alto
    if (_prefs.spellCheck && _spell) {
        EditorView* view = editor();
        sptr_t start = 0;
        sptr_t end = 0;
        const QString word = misspelledWordAt(*view, view->sci(SCI_GETCURRENTPOS), &start, &end);
        if (!word.isEmpty()) {
            const QStringList suggestions = _spell->suggest(word);
            for (const QString& suggestion : suggestions) {
                QAction* replace = menu->addAction(suggestion);
                QFont bold = replace->font();
                bold.setBold(true);
                replace->setFont(bold);
                connect(replace, &QAction::triggered, this, [view, start, end, suggestion] {
                    const QByteArray text = suggestion.toUtf8();
                    view->sci(SCI_SETTARGETRANGE, static_cast<uptr_t>(start), end);
                    view->sends(SCI_REPLACETARGET, static_cast<uptr_t>(text.size()), text.constData());
                });
            }
            if (suggestions.isEmpty())
                menu->addAction(tr("(No suggestions)"))->setEnabled(false);
            QAction* add = menu->addAction(tr("Add «%1» to the Dictionary").arg(word), this, [this, word] {
                _spell->addToPersonal(word);
                checkSpellingNow();
            });
            add->setObjectName(QStringLiteral("spellAdd"));
            QAction* ignore = menu->addAction(tr("Ignore «%1»").arg(word), this, [this, word] {
                _spell->ignore(word);
                checkSpellingNow();
            });
            ignore->setObjectName(QStringLiteral("spellIgnore"));
            menu->addSeparator();
        }
    }
    fillMenuFromXml(menu, readContextMenu(contextMenuPath()));
    if (menu->actions().isEmpty()) {
        // Sin contextMenu.xml que valga: el de Scintilla de siempre
        for (const char* name : {"actionUndo", "actionRedo", "", "actionCut", "actionCopy", "actionPaste", "actionDelete", "",
                                 "actionSelectAll"}) {
            if (!*name)
                menu->addSeparator();
            else if (QAction* action = findChild<QAction*>(QString::fromLatin1(name)))
                menu->addAction(action);
        }
    }
    return menu;
}

int MainWindow::importPlugins(const QStringList& files)
{
    // IDM_SETTING_IMPORTPLUGIN (addNppPlugins): cada plugin en plugins/<nombre>/<nombre>.so; un
    // paquete .zip (el del catálogo o el de una receta) se instala como desde el administrador
    const QString root = QDir(paths::baseConfigDir()).filePath(QStringLiteral("plugins"));
    QStringList imported;
    QStringList problems;
    for (const QString& file : files) {
        QFile in(file);
        if (!in.open(QIODevice::ReadOnly)) {
            problems << tr("%1: cannot read it").arg(QFileInfo(file).fileName());
            continue;
        }
        const QByteArray bytes = in.readAll();
        QString error;
        if (file.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive)) {
            // El nombre del plugin es el de su .so
            QTemporaryDir temp;
            QString folder;
            if (temp.isValid() && plugin_installer::extractZip(bytes, temp.path(), &error)) {
                QDirIterator it(temp.path(), {QStringLiteral("*.so")}, QDir::Files, QDirIterator::Subdirectories);
                if (it.hasNext())
                    folder = QFileInfo(it.next()).completeBaseName();
            }
            if (folder.isEmpty()) {
                problems << tr("%1: there is no plugin (.so) in the package %2").arg(QFileInfo(file).fileName(), error);
                continue;
            }
            if (!plugin_installer::installPackage(bytes, folder, QString(), root, true, &error)) {
                problems << QStringLiteral("%1: %2").arg(QFileInfo(file).fileName(), error);
                continue;
            }
            imported << folder;
        } else {
            if (!bytes.startsWith("\x7f" "ELF")) {
                problems << tr("%1: it is not a Linux plugin (.so)").arg(QFileInfo(file).fileName());
                continue;
            }
            const QString folder = QFileInfo(file).completeBaseName();
            const QString dir = QDir(root).filePath(folder);
            const QString target = QDir(dir).filePath(folder + QStringLiteral(".so"));
            QDir().mkpath(dir);
            QFile::remove(target);
            QFile out(target);
            if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size()) {
                problems << QStringLiteral("%1: %2").arg(QFileInfo(file).fileName(), out.errorString());
                continue;
            }
            imported << folder;
        }
    }
    if (!problems.isEmpty())
        _prompter->warn(this, tr("Import plugin(s)"), problems.join(QLatin1Char('\n')));
    if (!imported.isEmpty())
        _prompter->inform(this, tr("Edita needs to be relaunched"),
                          tr("You have to restart Edita to load the plugins you imported:\n%1").arg(imported.join(QStringLiteral(", "))));
    return static_cast<int>(imported.size());
}

int MainWindow::importThemes(const QStringList& files)
{
    // IDM_SETTING_IMPORTSTYLETHEMES: a themes/ de la configuración, donde ya los ven el menú de
    // temas y el Configurador de estilos
    const QString dir = QDir(paths::configDir()).filePath(QStringLiteral("themes"));
    QDir().mkpath(dir);
    QStringList imported;
    QStringList problems;
    for (const QString& file : files) {
        pugi::xml_document doc;
        if (!doc.load_file(file.toUtf8().constData()) || !doc.child("NotepadPlus").child("LexerStyles")) {
            problems << tr("%1: it is not a style theme").arg(QFileInfo(file).fileName());
            continue;
        }
        const QString target = QDir(dir).filePath(QFileInfo(file).fileName());
        if (QFileInfo(target) == QFileInfo(file))
            continue;
        if (QFileInfo::exists(target)
            && !_prompter->confirm(this, tr("Import style theme(s)"),
                                   tr("The theme %1 already exists. Replace it?").arg(QFileInfo(file).completeBaseName())))
            continue;
        QFile::remove(target);
        if (!QFile::copy(file, target)) {
            problems << tr("%1: cannot copy it").arg(QFileInfo(file).fileName());
            continue;
        }
        imported << QFileInfo(file).completeBaseName();
    }
    if (!problems.isEmpty())
        _prompter->warn(this, tr("Import style theme(s)"), problems.join(QLatin1Char('\n')));
    if (!imported.isEmpty())
        statusBar()->showMessage(tr("Imported themes: %1").arg(imported.join(QStringLiteral(", "))), 5000);
    return static_cast<int>(imported.size());
}

void MainWindow::refreshBeginEndSelect(bool column)
{
    // N++: la orden empezada, marcada; la otra no se puede usar hasta terminar
    QAction* used = findChild<QAction*>(column ? QStringLiteral("actionBeginEndSelectColumn") : QStringLiteral("actionBeginEndSelect"));
    QAction* other = findChild<QAction*>(column ? QStringLiteral("actionBeginEndSelect") : QStringLiteral("actionBeginEndSelectColumn"));
    const bool started = editor()->beginEndSelectStarted();
    if (used)
        used->setChecked(started);
    if (other)
        other->setEnabled(!started);
}

bool MainWindow::findCharInRange(char32_t low, char32_t high, bool forward, bool wrap)
{
    // FindCharsInRangeDlg::findCharInRange: desde la selección; lo encontrado queda seleccionado
    EditorView* view = editor();
    const bool utf8 = view->sci(SCI_GETCODEPAGE) == SC_CP_UTF8;
    const std::string bytes = _documents.text(currentBuffer());
    const QString text = utf8 ? QString::fromUtf8(bytes) : QString::fromLatin1(bytes);
    const sptr_t fromByte = forward ? view->sci(SCI_GETSELECTIONEND) : view->sci(SCI_GETSELECTIONSTART);
    auto unitsOf = [&](sptr_t byte) {
        return utf8 ? QString::fromUtf8(bytes.data(), static_cast<qsizetype>(byte)).size() : static_cast<qsizetype>(byte);
    };
    const auto [at, length] = edita::findCharInRange(text, unitsOf(fromByte), low, high, forward, wrap);
    if (at < 0)
        return false;
    auto bytesOf = [&](qsizetype units) {
        return utf8 ? static_cast<sptr_t>(QStringView(text).left(units).toUtf8().size()) : static_cast<sptr_t>(units);
    };
    const sptr_t start = bytesOf(at);
    const sptr_t end = bytesOf(at + length);
    view->sci(SCI_SETSEL, static_cast<uptr_t>(start), end);
    view->sci(SCI_SCROLLCARET);
    return true;
}

QString MainWindow::debugInfo() const
{
    // AboutDlg de N++ (Debug Info): lo que hace falta para reproducir un problema
    QStringList lines;
    lines << QStringLiteral("Edita %1 (%2)").arg(QString::fromUtf8(kAppVersion), QSysInfo::buildCpuArchitecture());
#if defined(__clang__)
    lines << QStringLiteral("Built with: Clang %1").arg(QString::fromLatin1(__clang_version__));
#elif defined(__GNUC__)
    lines << QStringLiteral("Built with: GCC %1").arg(QString::fromLatin1(__VERSION__));
#endif
    lines << QStringLiteral("Qt: %1 (built with %2)").arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR));
    lines << QStringLiteral("Path: %1").arg(QCoreApplication::applicationFilePath());
    if (const QString appImage = qEnvironmentVariable("APPIMAGE"); !appImage.isEmpty())
        lines << QStringLiteral("AppImage: %1").arg(appImage);
    lines << QStringLiteral("Command line: %1").arg(QCoreApplication::arguments().mid(1).join(QLatin1Char(' ')));
    lines << QStringLiteral("Configuration: %1").arg(paths::configDir());
    lines << QStringLiteral("Session: %1, snapshot mode: %2")
                 .arg(_prefs.rememberLastSession ? QStringLiteral("ON") : QStringLiteral("OFF"),
                      isSnapshotMode() ? QStringLiteral("ON") : QStringLiteral("OFF"));
    lines << QStringLiteral("System: %1")
                 .arg(issue_report::describe(issue_report::currentSystem(QGuiApplication::platformName())));
    lines << QStringLiteral("Locale: %1, interface: %2").arg(QLocale::system().name(), QLocale().name());
    QStringList plugins;
    if (_plugins) {
        for (const auto& p : _plugins->plugins())
            plugins << (p->version.isEmpty() ? p->name : QStringLiteral("%1 (%2)").arg(p->name, p->version));
    }
    lines << QStringLiteral("Plugins: %1").arg(plugins.isEmpty() ? QStringLiteral("none") : plugins.join(QStringLiteral(", ")));
    return lines.join(QLatin1Char('\n'));
}

void MainWindow::showDebugInfo()
{
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("debugInfo"));
    dialog.setWindowTitle(tr("Debug Info"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* text = new QPlainTextEdit(debugInfo(), &dialog);
    text->setReadOnly(true);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(text);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QPushButton* copy = buttons->addButton(tr("&Copy Debug Info into Clipboard"), QDialogButtonBox::ActionRole);
    connect(copy, &QPushButton::clicked, &dialog, [text] { QApplication::clipboard()->setText(text->toPlainText()); });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.resize(640, 380);
    dialog.exec();
}

bool MainWindow::closeAllToLeft(BufferId id)
{
    std::vector<BufferId> ids = _activePane->buffers();
    const auto it = std::find(ids.begin(), ids.end(), id);
    return closeBuffers(std::vector<BufferId>(ids.begin(), it), false, _activePane);
}

bool MainWindow::closeAllToRight(BufferId id)
{
    std::vector<BufferId> ids = _activePane->buffers();
    auto it = std::find(ids.begin(), ids.end(), id);
    if (it != ids.end())
        ++it;
    return closeBuffers(std::vector<BufferId>(it, ids.end()), false, _activePane);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    notifyPlugins(NPPN_BEFORESHUTDOWN, 0);
    // Última copia y sesión antes de cerrar nada (WM_CLOSE de N++)
    if (isSnapshotMode())
        backupNow();
    const Session session = captureSession();
    // Con la sesión recordada, los archivos abiertos no pasan a "recientes" (N++ bloquea la lista)
    _lockRecentFiles = _sessionEnabled && _prefs.rememberLastSession;
    const bool closed = closeBuffers(allBuffers(), true);
    _lockRecentFiles = false;
    if (!closed) {
        notifyPlugins(NPPN_CANCELSHUTDOWN, 0);
        event->ignore();
        return;
    }
    notifyPlugins(NPPN_SHUTDOWN, 0);
    if (_sessionEnabled && _prefs.rememberLastSession)
        writeSessionFile(sessionPath(), session);
    saveWindowState();
    event->accept();
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    // Claro u oscuro: los iconos con el color nuevo
    if (event->type() == QEvent::PaletteChange)
        icons::refresh(this);
    // WM_ACTIVATEAPP → NPPM_INTERNAL_CHECKDOCSTATUS
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        scheduleFileCheck();
        // Al volver a la ventana puede haber un commit nuevo (hecho en una terminal)
        GitChanges::clearCache();
        for (const QPointer<GitChanges>& git : _git) {
            if (git)
                git->reload();
        }
        for (const QPointer<GitBlame>& blame : _blame) {
            if (blame)
                blame->reload();
        }
        scheduleGitStatus();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            openFile(url.toLocalFile());
    }
    event->acceptProposedAction();
}

// --- Codificación ---------------------------------------------------------------

bool MainWindow::prepareReinterpret(BufferId id)
{
    // NppCommands.cpp, IDM_FORMAT_*: guardar primero y aceptar perder el deshacer
    const Buffer* buf = _documents.buffer(id);
    if (buf->isDirty()) {
        if (!_prompter->confirm(this, tr("Save modifications"),
                                tr("You should save the current modifications.\nAll the saved modifications cannot be "
                                   "undone.\n\nContinue?")))
            return false;
        if (!saveBuffer(id))
            return false;
        editor()->sci(SCI_EMPTYUNDOBUFFER);
    }
    if (editor()->sci(SCI_CANUNDO)
        && !_prompter->confirm(this, tr("Lose undo ability"),
                               tr("Changing the encoding will lose the undo history.\n\nContinue?")))
        return false;
    return true;
}

void MainWindow::reinterpretAs(int codepage)
{
    const BufferId id = currentBuffer();
    Buffer* buf = _documents.buffer(id);
    if (!buf)
        return;
    if (buf->isUntitled()) {
        buf->setEncoding(TextEncoding::fromCodepage(codepage)); // no hay bytes que reinterpretar
        return;
    }
    if (!prepareReinterpret(id)) {
        refreshMenuChecks();
        return;
    }
    QString error;
    if (!_documents.reloadBuffer(id, codepage, &error))
        _prompter->warn(this, tr("Encoding"), error);
}

void MainWindow::encodeAs(UniMode mode)
{
    const BufferId id = currentBuffer();
    Buffer* buf = _documents.buffer(id);
    if (!buf)
        return;
    // Documento Unicode (o sin archivo): solo cambia cómo se guarda
    if (buf->encoding().isUnicode() || buf->isUntitled()) {
        buf->setEncoding(TextEncoding::unicode(mode));
        refreshMenuChecks();
        return;
    }
    // Documento con juego de caracteres: se vuelven a leer los bytes como UTF-8
    if (!prepareReinterpret(id)) {
        refreshMenuChecks();
        return;
    }
    QString error;
    if (!_documents.reloadBuffer(id, kCodepageUtf8, &error)) {
        _prompter->warn(this, tr("Encoding"), error);
        return;
    }
    if (mode != UniMode::Utf8)
        buf->setEncoding(TextEncoding::unicode(mode));
}

void MainWindow::convertTo(const TextEncoding& encoding)
{
    if (Buffer* buf = _documents.buffer(currentBuffer()))
        buf->setEncoding(encoding);
}

// --- Edición --------------------------------------------------------------------

void MainWindow::convertEol(EolType eol)
{
    _documents.convertEol(currentBuffer(), eol);
}

void MainWindow::goToLine(int line, int column)
{
    EditorView* view = editor();
    const auto docLine = static_cast<uptr_t>(std::max(line, 1) - 1);
    view->sci(SCI_ENSUREVISIBLEENFORCEPOLICY, docLine);
    const sptr_t pos = view->sci(SCI_FINDCOLUMN, docLine, std::max(column, 1) - 1);
    view->sci(SCI_GOTOPOS, static_cast<uptr_t>(pos));
    view->sci(SCI_SCROLLCARET);
}

// --- Barra de estado interactiva -------------------------------------------------------

void MainWindow::applyIndentOverride(EditorPane* pane)
{
    const auto it = _indentOverrides.find(pane->currentBuffer());
    if (it == _indentOverrides.end())
        return;
    EditorView* view = pane->editor();
    view->sci(SCI_SETUSETABS, it->second.useTabs ? 1 : 0);
    view->sci(SCI_SETTABWIDTH, static_cast<uptr_t>(it->second.width));
    view->sci(SCI_SETINDENT, static_cast<uptr_t>(it->second.width));
}

// La sangría de este documento (desde la barra de estado): se mantiene mientras esté abierto
void MainWindow::setDocumentIndentation(bool useTabs, int width)
{
    _indentOverrides[currentBuffer()] = {useTabs, width};
    applyIndentOverride(_activePane);
    _statusBar->updateCaret(editor());
}

// Una lista con buscador encima del campo pulsado
void MainWindow::showStatusPicker(int which)
{
    using Field = DocStatusBar::Field;
    const auto field = static_cast<Field>(which);
    if (field == Field::Problems) {
        QDockWidget* dock = panelDock(QStringLiteral("problems"));
        dock->setVisible(!dock->isVisibleTo(this));
        return;
    }
    if (field == Field::Position) {
        showCommandPalette(QStringLiteral(":"));
        return;
    }
    if (field == Field::InsertMode) {
        editor()->sci(SCI_EDITTOGGLEOVERTYPE);
        _statusBar->updateCaret(editor());
        editor()->setFocus();
        return;
    }
    // Las órdenes de un menú (con sus submenús como detalle), tal cual: elegir una es
    // lo mismo que elegirla en el menú
    auto fromMenu = [](QMenu* menu) {
        std::vector<QuickPicker::Item> items;
        std::function<void(QMenu*, const QString&)> walk = [&](QMenu* m, const QString& group) {
            emit m->aboutToShow(); // los que se rellenan o marcan al abrirse
            for (QAction* action : m->actions()) {
                if (action->isSeparator() || !action->isVisible())
                    continue;
                const QString text = plainMenuText(action->text());
                if (action->menu()) {
                    walk(action->menu(), group.isEmpty() ? text : group + QStringLiteral(" › ") + text);
                    continue;
                }
                if (!action->isEnabled() || text.isEmpty())
                    continue;
                QPointer<QAction> target(action);
                items.push_back({text, group, action->isCheckable() && action->isChecked(), [target] {
                                     if (target)
                                         target->trigger();
                                 }});
            }
        };
        if (menu)
            walk(menu, QString());
        return items;
    };
    auto actionItem = [this](const char* name, const QString& group) -> std::optional<QuickPicker::Item> {
        QAction* action = findChild<QAction*>(QString::fromLatin1(name));
        if (!action)
            return std::nullopt;
        QPointer<QAction> target(action);
        return QuickPicker::Item{plainMenuText(action->text()), group, false, [target] {
                                     if (target)
                                         target->trigger();
                                 }};
    };
    std::vector<QuickPicker::Item> items;
    QString placeholder;
    switch (field) {
    case Field::Language:
        items = fromMenu(_languageMenu);
        placeholder = tr("Select a language");
        break;
    case Field::Encoding:
        items = fromMenu(_encodingMenu);
        placeholder = tr("Select an encoding or convert to it");
        break;
    case Field::Eol:
        items = fromMenu(_eolMenu);
        placeholder = tr("Convert the end of line");
        break;
    case Field::Indentation: {
        placeholder = tr("Indentation of this document");
        const bool tabs = editor()->sci(SCI_GETUSETABS) != 0;
        const sptr_t indent = editor()->sci(SCI_GETINDENT);
        const int width = static_cast<int>(indent > 0 ? indent : editor()->sci(SCI_GETTABWIDTH));
        items.push_back({tr("Indent using spaces"), QString(), !tabs, [this, width] { setDocumentIndentation(false, width); }});
        items.push_back({tr("Indent using tabs"), QString(), tabs, [this, width] { setDocumentIndentation(true, width); }});
        for (int w : {2, 3, 4, 8})
            items.push_back({tr("Width: %1").arg(w), tr("Indentation"), w == width, [this, tabs, w] { setDocumentIndentation(tabs, w); }});
        for (const char* name : {"actionTabToSpace", "actionSpaceToTabLeading", "actionSpaceToTabAll"}) {
            if (auto item = actionItem(name, tr("Convert")))
                items.push_back(*item);
        }
        if (auto item = actionItem("actionPreferences", tr("Preferences"))) {
            item->text = tr("Indentation settings…");
            items.push_back(*item);
        }
        break;
    }
    default:
        return;
    }
    ensurePicker()->popup(_statusBar->field(field), placeholder, std::move(items));
}

// --- Página de inicio -----------------------------------------------------------------

void MainWindow::createWelcomePage()
{
    _recentFolders = _syntax.config().guiConfig(QStringLiteral("EditaRecentFolders"), QStringLiteral("paths"))
                         .split(QLatin1Char('\x1f'), Qt::SkipEmptyParts);
    _welcome = new WelcomePage(_centerStack);
    _centerStack->addWidget(_welcome);
    _welcome->setVersion(QString::fromUtf8(kAppVersion));
    auto trigger = [this](const char* name) {
        if (QAction* action = findChild<QAction*>(QString::fromLatin1(name)))
            action->trigger();
    };
    connect(_welcome, &WelcomePage::newFileRequested, this, [this] { hideWelcome(true); });
    connect(_welcome, &WelcomePage::openFileRequested, this, [trigger] { trigger("actionOpen"); });
    connect(_welcome, &WelcomePage::openFolderRequested, this, [trigger] { trigger("actionOpenFolderAsWorkspace"); });
    connect(_welcome, &WelcomePage::commandPaletteRequested, this, [this] {
        hideWelcome(true);
        showCommandPalette(QStringLiteral(">"));
    });
    connect(_welcome, &WelcomePage::recentFileRequested, this, [this](const QString& path) {
        if (openFile(path) == kInvalidBuffer)
            fillWelcomePage(); // ya no existe: fuera de la lista
    });
    connect(_welcome, &WelcomePage::recentFolderRequested, this, [this](const QString& path) {
        if (QFileInfo(path).isDir()) {
            addFolderToWorkspace(path);
        } else {
            _recentFolders.removeAll(path);
            fillWelcomePage();
        }
    });
    connect(_welcome, &WelcomePage::themeRequested, this, [this](int index) {
        const std::vector<SyntaxManager::Theme> themes = _syntax.themes();
        if (index >= 0 && index < static_cast<int>(themes.size()))
            setTheme(themes[static_cast<size_t>(index)].path);
    });
    connect(_welcome, &WelcomePage::modeRequested, this, [this](WelcomePage::Mode mode) {
        Preferences p = _prefs;
        p.darkModeFollowSystem = mode == WelcomePage::Mode::System;
        p.darkModeEnabled = mode == WelcomePage::Mode::Dark;
        setPreferences(p);
        fillWelcomePage(); // el tema de ese modo
    });
    connect(_welcome, &WelcomePage::showOnStartupChanged, this, [this](bool show) {
        Preferences p = _prefs;
        p.showWelcome = show;
        _welcomeForced = isWelcomeVisible(); // sigue a la vista hasta que se cierre
        setPreferences(p);
    });
    connect(_welcome, &WelcomePage::typingStarted, this, [this](const QString& text) {
        hideWelcome(true);
        if (!text.isEmpty())
            editor()->sends(SCI_REPLACESEL, 0, text.toUtf8().constData());
    });
    connect(_welcome, &WelcomePage::dismissRequested, this, [this] { hideWelcome(true); });
    updateWelcome();
}

bool MainWindow::isWelcomeVisible() const
{
    return _centerStack && _welcome && _centerStack->currentWidget() == _welcome;
}

void MainWindow::showWelcomePage()
{
    _welcomeForced = true;
    updateWelcome();
}

Manual::Commands MainWindow::manualCommands() const
{
    // Las órdenes que cita el manual ({{actionFind}}): su menú y su atajo, como están ahora
    Manual::Commands commands;
    std::function<void(QMenu*, const QString&)> walk = [&](QMenu* menu, const QString& path) {
        for (QAction* action : menu->actions()) {
            if (action->isSeparator())
                continue;
            const QString text = QString(action->text()).remove(QLatin1Char('&'));
            const QString here = path.isEmpty() ? text : path + QStringLiteral(" › ") + text;
            if (action->menu())
                walk(action->menu(), here);
            else if (!action->objectName().isEmpty())
                commands.insert(action->objectName(),
                                {here, action->shortcut().toString(QKeySequence::NativeText), static_cast<int>(commands.size())});
        }
    };
    for (QAction* top : menuBar()->actions()) {
        if (top->menu())
            walk(top->menu(), QString(top->text()).remove(QLatin1Char('&')));
    }
    return commands;
}

void MainWindow::showManual(const QString& chapter, const QString& anchor)
{
    if (!_manual)
        _manual = new ManualWindow([this] { return manualCommands(); }, this);
    _manual->open(chapter, anchor);
}

void MainWindow::hideWelcome(bool focusEditor)
{
    _welcomeForced = false;
    _welcomeDismissed = currentBuffer();
    updateWelcome();
    if (focusEditor)
        editor()->setFocus();
}

void MainWindow::scheduleWelcomeUpdate()
{
    if (_welcomeUpdatePending || !_welcome)
        return;
    _welcomeUpdatePending = true;
    QTimer::singleShot(0, this, [this] {
        _welcomeUpdatePending = false;
        updateWelcome();
    });
}

// Solo un documento, nuevo, sin cambios y vacío, en la vista principal (como al arrancar
// o al cerrar todo): la página de inicio en lugar del editor
void MainWindow::updateWelcome()
{
    if (!_welcome)
        return;
    bool show = _welcomeForced;
    if (!show && _prefs.showWelcome) {
        const std::vector<BufferId> buffers = allBuffers();
        const Buffer* buf = buffers.size() == 1 ? _documents.buffer(buffers.front()) : nullptr;
        show = buf && buf->isUntitled() && !buf->isDirty() && _documents.text(buf->id()).empty() && _subPane->count() == 0
            && buf->id() != _welcomeDismissed;
    }
    if (show == isWelcomeVisible())
        return;
    if (show) {
        fillWelcomePage();
        _centerStack->setCurrentWidget(_welcome);
        _welcome->setFocus();
    } else {
        _centerStack->setCurrentWidget(_splitter);
    }
}

void MainWindow::fillWelcomePage()
{
    QStringList files;
    for (const QString& path : _recentFiles) {
        if (QFileInfo::exists(path))
            files << path;
    }
    _welcome->setRecentFiles(files);
    QStringList folders;
    for (const QString& path : _recentFolders) {
        if (QFileInfo(path).isDir())
            folders << path;
    }
    _welcome->setRecentFolders(folders);
    QStringList names;
    int current = 0;
    const QString active = _syntax.activeThemePath();
    for (const SyntaxManager::Theme& theme : _syntax.themes()) {
        if (theme.path == active)
            current = static_cast<int>(names.size());
        names << theme.name;
    }
    _welcome->setThemes(names, current);
    _welcome->setMode(_prefs.darkModeFollowSystem ? WelcomePage::Mode::System
                      : _prefs.darkModeEnabled    ? WelcomePage::Mode::Dark
                                                  : WelcomePage::Mode::Light);
    _welcome->setShowOnStartup(_prefs.showWelcome);
    auto keys = [this](const char* name) {
        const QAction* action = findChild<QAction*>(QString::fromLatin1(name));
        return action ? action->shortcut() : QKeySequence();
    };
    auto text = [this](const char* name) {
        const QAction* action = findChild<QAction*>(QString::fromLatin1(name));
        return action ? plainMenuText(action->text()) : QString();
    };
    _welcome->setActionShortcuts(keys("actionNew"), keys("actionOpen"), keys("actionOpenFolderAsWorkspace"), keys("actionCommandPalette"));
    std::vector<WelcomePage::Shortcut> shortcuts;
    for (const char* name : {"actionGoToFile", "actionGoToSymbol", "actionFind", "actionFindInFiles", "actionReplace",
                             "actionColumnEditor", "actionToggleComment", "actionDuplicateLine"})
        shortcuts.push_back({text(name), keys(name)});
    _welcome->setShortcuts(shortcuts);
}

// --- Git ------------------------------------------------------------------------------

GitChanges* MainWindow::gitChanges(const EditorPane* pane) const
{
    return _git[pane == _subPane ? 1 : 0];
}

GitChanges* MainWindow::gitChangesOfCurrentView() const
{
    return gitChanges(_activePane);
}

ScrollBarMarks* MainWindow::scrollBarMarksOfCurrentView() const
{
    return _scrollMarks[_activePane == _subPane ? 1 : 0];
}

ColorPreview* MainWindow::colorPreviewOfCurrentView() const
{
    return _colorPreview[_activePane == _subPane ? 1 : 0];
}

RainbowBrackets* MainWindow::rainbowBracketsOfCurrentView() const
{
    return _rainbow[_activePane == _subPane ? 1 : 0];
}

Breadcrumbs* MainWindow::breadcrumbsOfCurrentView() const
{
    return _breadcrumbs[_activePane == _subPane ? 1 : 0];
}

StickyScroll* MainWindow::stickyScrollOfCurrentView() const
{
    return _sticky[_activePane == _subPane ? 1 : 0];
}

FindBar* MainWindow::findBarOfCurrentView() const
{
    return _findBar[_activePane == _subPane ? 1 : 0];
}

// Buscar en línea: la barra de la vista activa, con la selección o la palabra del cursor
void MainWindow::showFindBar(bool replace)
{
    FindBar* bar = findBarOfCurrentView();
    if (!bar)
        return;
    hideWelcome(false);
    bar->open(replace, _prefs.fillFindFieldWithSelected ? textForFindField() : QString());
}

const MainWindow::SymbolCacheEntry* MainWindow::functionsOf(BufferId id, const EditorView* view) const
{
    const Buffer* buf = _documents.buffer(id);
    if (!buf || !view || !_functionParsers)
        return nullptr;
    SymbolCacheEntry key;
    key.language = static_cast<int>(buf->language());
    key.userLanguage = buf->userLanguage();
    key.length = view->sci(SCI_GETLENGTH);
    key.undoActions = view->sci(SCI_GETUNDOACTIONS);
    key.undoCurrent = view->sci(SCI_GETUNDOCURRENT);
    auto cached = _symbolCache.find(id);
    if (cached == _symbolCache.end() || cached->second.language != key.language
        || cached->second.userLanguage != key.userLanguage || cached->second.length != key.length
        || cached->second.undoActions != key.undoActions || cached->second.undoCurrent != key.undoCurrent) {
        ScintillaEditBase scanner;
        scanner.send(SCI_SETDOCPOINTER, 0, view->sci(SCI_GETDOCPOINTER));
        key.parsed = _functionParsers->parse(scanner, buf->language(), buf->userLanguage(), key.found);
        scanner.send(SCI_SETDOCPOINTER, 0, 0);
        cached = _symbolCache.insert_or_assign(id, std::move(key)).first;
    }
    return &cached->second;
}

// La ruta de navegación: el archivo y, si `symbols`, los símbolos del documento con el
// bloque que ocupa cada uno (el plegado: del encabezado a su último hijo)
void MainWindow::updateBreadcrumbs(int index, bool symbols)
{
    EditorPane* pane = index ? _subPane : _mainPane;
    Breadcrumbs* crumbs = _breadcrumbs[index];
    const Buffer* buf = _documents.buffer(pane->currentBuffer());
    if (!crumbs || !buf)
        return;
    crumbs->setFile(buf->isUntitled() ? QString() : absolutePath(buf->filePath()), buf->displayName(),
                    _folderPanel ? _folderPanel->roots() : QStringList());
    if (!symbols || !_prefs.breadcrumbs)
        return;
    EditorView* view = pane->editor();
    const sptr_t caretLine = view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(view->sci(SCI_GETCURRENTPOS)));
    const bool folds = view->sci(SCI_GETLENGTH) <= kMaxSymbolsWhileTyping;
    if (folds)
        view->sci(SCI_COLOURISE, 0, -1); // el plegado de todo el documento
    const sptr_t lineCount = view->sci(SCI_GETLINECOUNT);
    auto blockEnd = [&](sptr_t line) {
        if (!folds)
            return static_cast<int>(line);
        // El encabezado puede estar en la línea siguiente (la llave en su propia línea)
        for (sptr_t h = line; h < std::min(line + 3, lineCount); ++h) {
            if (view->sci(SCI_GETFOLDLEVEL, static_cast<uptr_t>(h)) & SC_FOLDLEVELHEADERFLAG)
                return static_cast<int>(view->sci(SCI_GETLASTCHILD, static_cast<uptr_t>(h), -1));
        }
        return static_cast<int>(line);
    };
    std::vector<Breadcrumbs::Symbol> list;
    if (_functionParsers) {
        // Lo que tarda es el análisis, que se guarda por documento. Los bloques salen del
        // plegado, que puede no estar hecho todavía la primera vez: se calculan siempre
        const SymbolCacheEntry* entry = functionsOf(pane->currentBuffer(), view);
        const bool parsed = entry && entry->parsed;
        static const std::vector<FunctionInfo> kNone;
        const std::vector<FunctionInfo>& found = entry ? entry->found : kNone;
        if (parsed) {
            QSet<QString> classes;
            for (const FunctionInfo& fi : found) {
                const QString className = QString::fromStdString(fi.className);
                if (!className.isEmpty() && fi.classPos >= 0 && !classes.contains(className)) {
                    classes.insert(className);
                    const sptr_t line = view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(fi.classPos));
                    list.push_back({className, true, static_cast<int>(line), blockEnd(line)});
                }
                if (fi.name.empty() || fi.pos < 0)
                    continue;
                const sptr_t line = view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(fi.pos));
                list.push_back({QString::fromStdString(fi.name), false, static_cast<int>(line), blockEnd(line)});
            }
        }
    }
    bool fallback = false;
    if (list.empty() && folds) {
        // Sin analizador: los bloques plegables de los primeros niveles (encabezados de
        // Markdown, etiquetas de XML…), con el texto de su primera línea
        fallback = true;
        for (sptr_t line = 0; line < lineCount && list.size() < 2000; ++line) {
            const sptr_t level = view->sci(SCI_GETFOLDLEVEL, static_cast<uptr_t>(line));
            if (!(level & SC_FOLDLEVELHEADERFLAG) || (level & SC_FOLDLEVELNUMBERMASK) - SC_FOLDLEVELBASE >= 4)
                continue;
            const sptr_t start = view->sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
            const sptr_t end = std::min(view->sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line)), start + 200);
            QString text;
            if (end > start) {
                const auto* chars = reinterpret_cast<const char*>(view->sci(SCI_GETRANGEPOINTER, static_cast<uptr_t>(start), end - start));
                text = QString::fromUtf8(chars, static_cast<qsizetype>(end - start)).simplified();
            }
            if (text.size() > 60)
                text = text.left(60) + QStringLiteral("…");
            if (!text.isEmpty())
                list.push_back({text, false, static_cast<int>(line), blockEnd(line)});
        }
    }
    std::stable_sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.line < b.line; });
    crumbs->setSymbols(std::move(list), fallback);
    crumbs->setCaretLine(static_cast<int>(caretLine));
}

void MainWindow::updateLanguageDecorations(int index)
{
    const Buffer* buf = _documents.buffer((index ? _subPane : _mainPane)->currentBuffer());
    if (!buf)
        return;
    if (_colorPreview[index])
        _colorPreview[index]->setLanguage(buf->language());
    if (_rainbow[index]) {
        // Los paréntesis de comentarios, cadenas, caracteres y expresiones regulares no
        // cuentan: los estilos del lexer con esos nombres (stylers.xml)
        std::bitset<256> ignored;
        if (const LexerStyles* lexer = _syntax.styles().lexer(languageInfo(buf->language()).name)) {
            static const char* const kWords[] = {"COMMENT", "STRING", "CHAR", "REGEX", "VERBATIM", "TRIPLE", "LITERAL",
                                                 "HEREDOC", "BACKTICK", "DOCUMENT"};
            for (const Style& style : lexer->styles) {
                const QString name = QString::fromStdString(style.name).toUpper();
                for (const char* word : kWords) {
                    if (style.id >= 0 && style.id < 256 && name.contains(QLatin1String(word)))
                        ignored.set(static_cast<size_t>(style.id));
                }
            }
        }
        _rainbow[index]->setIgnoredStyles(ignored);
    }
}

void MainWindow::updateGitDocument(EditorPane* pane)
{
    GitChanges* git = gitChanges(pane);
    if (!git)
        return;
    git->applyColors(); // el estilo del documento recién activado
    const Buffer* buf = _documents.buffer(pane->currentBuffer());
    if (!buf || buf->isUntitled())
        git->setDocument(QString(), TextEncoding::utf8());
    else
        git->setDocument(absolutePath(buf->filePath()), buf->encoding());
    if (GitBlame* blame = _blame[pane == _mainPane ? 0 : 1]) {
        if (!buf || buf->isUntitled())
            blame->setDocument(QString(), TextEncoding::utf8());
        else
            blame->setDocument(absolutePath(buf->filePath()), buf->encoding());
    }
}

GitBlame* MainWindow::gitBlameOfCurrentView() const
{
    return _blame[_activePane == _mainPane ? 0 : 1];
}

bool MainWindow::showCommitOfCurrentLine()
{
    GitBlame* blame = gitBlameOfCurrentView();
    const Buffer* buf = _documents.buffer(currentBuffer());
    if (!blame || !buf || buf->isUntitled())
        return false;
    const auto line = static_cast<int>(editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(editor()->sci(SCI_GETCURRENTPOS))));
    const GitBlame::Commit* commit = blame->commitAt(line);
    if (!commit || commit->uncommitted) {
        statusBar()->showMessage(commit ? tr("This line is not committed yet") : tr("No Git history for this line"), 5000);
        return false;
    }
    // (copia: al abrir el documento nuevo, el blame de esta vista se vacía)
    const QString hash = commit->hash;
    QProcess git;
    git.start(QStringLiteral("git"), {QStringLiteral("-C"), QFileInfo(buf->filePath()).absolutePath(), QStringLiteral("show"),
                                      QStringLiteral("--no-color"), hash});
    if (!git.waitForFinished(15000) || git.exitCode() != 0)
        return false;
    // En un documento de solo lectura, como diff
    const BufferId id = newDocument();
    _documents.setUntitledName(id, hash.left(7));
    editor()->send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(git.readAllStandardOutput().constData()));
    editor()->sci(SCI_EMPTYUNDOBUFFER);
    editor()->sci(SCI_SETSAVEPOINT);
    editor()->sci(SCI_GOTOPOS, 0);
    if (Buffer* doc = _documents.buffer(id)) {
        doc->setLanguage(LangType::Diff);
        doc->setUserReadOnly(true);
    }
    return true;
}

// El siguiente (o anterior) cambio respecto a Git desde el cursor, dando la vuelta
void MainWindow::goToGitChange(bool forward)
{
    GitChanges* git = gitChanges(_activePane);
    if (!git || git->changes().empty())
        return;
    EditorView* view = editor();
    const int line = static_cast<int>(view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(view->sci(SCI_GETCURRENTPOS))));
    const std::vector<LineChange>& changes = git->changes();
    const LineChange* target = nullptr;
    if (forward) {
        for (const LineChange& c : changes) {
            if (c.line > line) {
                target = &c;
                break;
            }
        }
        if (!target)
            target = &changes.front();
    } else {
        for (auto it = changes.rbegin(); it != changes.rend(); ++it) {
            const int end = it->kind == LineChangeKind::Deleted ? it->line : it->line + it->count - 1;
            if (end < line && it->line < line) {
                target = &*it;
                break;
            }
        }
        if (!target)
            target = &changes.back();
    }
    goToLine(std::min(target->line, static_cast<int>(view->sci(SCI_GETLINECOUNT)) - 1) + 1);
}

// --- Paleta de órdenes e «Ir a archivo» ------------------------------------------------

void MainWindow::showCommandPalette(const QString& prefix)
{
    if (!_palette) {
        _palette = new CommandPalette(this);
        // El historial de órdenes, en config.xml (<GUIConfig name="EditaCommandPalette" history="…">)
        _palette->setHistory(_syntax.config().guiConfig(QStringLiteral("EditaCommandPalette"), QStringLiteral("history"))
                                 .split(QLatin1Char('\x1f'), Qt::SkipEmptyParts));
        connect(_palette, &CommandPalette::historyChanged, this, [this](const QStringList& ids) {
            _syntax.config().setGuiConfig(QStringLiteral("EditaCommandPalette"), QStringLiteral("history"),
                                          ids.join(QLatin1Char('\x1f')));
            _syntax.config().save();
        });
        connect(_palette, &CommandPalette::itemsRequested, this, [this](CommandPalette::Mode mode) { fillPalette(mode); });
        connect(_palette, &CommandPalette::goToLineRequested, this, [this](int line, int column) {
            goToLine(line, column);
            editor()->setFocus();
        });
    }
    _palette->open(prefix);
}

void MainWindow::fillPalette(CommandPalette::Mode mode)
{
    switch (mode) {
    case CommandPalette::Mode::Commands:
        _palette->setItems(mode, paletteCommands());
        break;
    case CommandPalette::Mode::Symbols:
        _palette->setItems(mode, paletteSymbols());
        break;
    case CommandPalette::Mode::Files:
        startPaletteFileScan();
        break;
    case CommandPalette::Mode::Line:
        break;
    }
}

// Todo lo de los menús (también los plugins, las macros, los lenguajes y los temas) que
// se puede ejecutar ahora, con su menú como detalle («Editar › Convertir»)
std::vector<CommandPalette::Item> MainWindow::paletteCommands() const
{
    std::vector<CommandPalette::Item> items;
    QSet<const QAction*> seen;
    const QSet<const QAction*> recent(_recentEntries.begin(), _recentEntries.end());
    std::function<void(const QMenu*, const QString&)> walk = [&](const QMenu* menu, const QString& path) {
        for (QAction* action : menu->actions()) {
            if (action->isSeparator() || !action->isVisible() || seen.contains(action) || recent.contains(action))
                continue;
            seen.insert(action);
            const QString text = plainMenuText(action->text());
            if (text.isEmpty())
                continue;
            if (action->menu()) {
                walk(action->menu(), path + QStringLiteral(" › ") + text);
                continue;
            }
            if (!action->isEnabled() || action->objectName() == QLatin1String("actionCommandPalette"))
                continue;
            CommandPalette::Item item;
            item.text = text;
            item.detail = path;
            item.shortcut = action->shortcut().toString(QKeySequence::NativeText);
            item.icon = action->icon();
            item.checked = action->isCheckable() && action->isChecked();
            item.id = action->objectName().startsWith(QLatin1String("action")) ? action->objectName() : path + QLatin1Char('/') + text;
            item.order = static_cast<int>(items.size());
            QPointer<QAction> target(action);
            item.run = [target] {
                if (target)
                    target->trigger();
            };
            items.push_back(std::move(item));
        }
    };
    for (QAction* top : menuBar()->actions()) {
        if (top->menu())
            walk(top->menu(), plainMenuText(top->text()));
    }
    return items;
}

// Las funciones y clases del documento (los analizadores de la Lista de funciones)
std::vector<CommandPalette::Item> MainWindow::paletteSymbols() const
{
    std::vector<CommandPalette::Item> items;
    const Buffer* buf = _documents.buffer(currentBuffer());
    if (!buf || !_functionParsers)
        return items;
    const SymbolCacheEntry* entry = functionsOf(currentBuffer(), editor());
    if (!entry || !entry->parsed)
        return items;
    const std::vector<FunctionInfo>& found = entry->found;
    QSet<QString> classes;
    for (const FunctionInfo& fi : found) {
        const QString className = QString::fromStdString(fi.className);
        if (!className.isEmpty() && !classes.contains(className) && fi.classPos >= 0) {
            classes.insert(className);
            CommandPalette::Item cls;
            cls.text = className;
            cls.icon = icons::icon(QStringLiteral("function-list"));
            cls.order = static_cast<int>(fi.classPos);
            const sptr_t pos = fi.classPos;
            QPointer<EditorView> view(editor());
            cls.run = [view, pos] {
                if (!view)
                    return;
                view->sci(SCI_ENSUREVISIBLEENFORCEPOLICY, static_cast<uptr_t>(view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos))));
                view->sci(SCI_GOTOPOS, static_cast<uptr_t>(pos));
                view->setFocus();
            };
            items.push_back(std::move(cls));
        }
        if (fi.name.empty() || fi.pos < 0)
            continue;
        CommandPalette::Item item;
        item.text = QString::fromStdString(fi.name);
        item.detail = className;
        item.order = static_cast<int>(fi.pos);
        const sptr_t pos = fi.pos;
        QPointer<EditorView> view(editor());
        item.run = [view, pos] {
            if (!view)
                return;
            view->sci(SCI_ENSUREVISIBLEENFORCEPOLICY, static_cast<uptr_t>(view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos))));
            view->sci(SCI_GOTOPOS, static_cast<uptr_t>(pos));
            view->setFocus();
        };
        items.push_back(std::move(item));
    }
    return items;
}

void MainWindow::stopPaletteFileScan()
{
    if (!_paletteScan)
        return;
    _paletteScanCancel->store(true);
    _paletteScan->wait();
    delete _paletteScan;
    _paletteScan = nullptr;
}

// «Ir a archivo»: los abiertos, los recientes y los de las carpetas de trabajo (el
// Explorador de carpetas o, sin él, la carpeta del documento), que se recorren en otro hilo
void MainWindow::startPaletteFileScan()
{
    stopPaletteFileScan();
    _paletteFiles.clear();
    std::vector<CommandPalette::Item> items;
    auto stateIcon = [](const Buffer& buf) {
        using State = DocTabBar::TabState;
        return DocTabBar::iconFor(buf.isUntitled() ? QString() : QFileInfo(buf.filePath()).fileName(),
                                  buf.isReadOnly() ? State::ReadOnly : buf.isDirty() ? State::Dirty : State::Saved);
    };
    auto fileItem = [this](const QString& path, const QString& folder, int order) {
        // La carpeta personal como «~», para que las rutas largas quepan
        QString detail = folder;
        const QString home = QDir::toNativeSeparators(QDir::homePath());
        if (detail == home || detail.startsWith(home + QDir::separator()))
            detail = QLatin1Char('~') + detail.mid(home.size());
        CommandPalette::Item item;
        item.text = QFileInfo(path).fileName();
        item.detail = detail;
        item.order = order;
        item.icon = fileicons::enabled() ? fileicons::forFile(path) : QIcon();
        item.run = [this, path] {
            openFile(path);
            editor()->setFocus();
        };
        return item;
    };
    // Abiertos: el actual al final, para que Enter cambie al anterior como Ctrl+Tab
    std::vector<BufferId> open = allBuffers();
    const BufferId current = currentBuffer();
    std::stable_partition(open.begin(), open.end(), [current](BufferId id) { return id != current; });
    for (BufferId id : open) {
        const Buffer* buf = _documents.buffer(id);
        if (!buf)
            continue;
        if (buf->isUntitled()) {
            CommandPalette::Item item;
            item.text = buf->displayName();
            item.detail = tr("open");
            item.order = static_cast<int>(items.size());
            item.icon = stateIcon(*buf);
            item.run = [this, id] {
                if (EditorPane* pane = paneContaining(id)) {
                    pane->activateBuffer(id);
                    setActivePane(pane);
                    editor()->setFocus();
                }
            };
            items.push_back(std::move(item));
            continue;
        }
        const QString path = absolutePath(buf->filePath());
        _paletteFiles.insert(path);
        CommandPalette::Item item = fileItem(path, QDir::toNativeSeparators(QFileInfo(path).absolutePath()), static_cast<int>(items.size()));
        item.icon = stateIcon(*buf);
        items.push_back(std::move(item));
    }
    for (const QString& recentPath : _recentFiles) {
        const QString path = absolutePath(recentPath);
        if (_paletteFiles.contains(path) || !QFileInfo::exists(path))
            continue;
        _paletteFiles.insert(path);
        items.push_back(fileItem(path, QDir::toNativeSeparators(QFileInfo(path).absolutePath()), 1000 + static_cast<int>(items.size())));
    }
    _palette->setItems(CommandPalette::Mode::Files, std::move(items));

    QStringList roots = _folderPanel ? _folderPanel->roots() : QStringList();
    if (roots.isEmpty()) {
        const Buffer* buf = _documents.buffer(current);
        const QString dir = buf && !buf->isUntitled() ? QFileInfo(buf->filePath()).absolutePath() : QString();
        // Sin carpeta de trabajo, la del documento (no la carpeta personal ni la raíz enteras)
        if (!dir.isEmpty() && dir != QDir::homePath() && dir != QDir::rootPath())
            roots << dir;
    }
    if (roots.isEmpty())
        return;
    _palette->setBusy(CommandPalette::Mode::Files, true);
    _paletteScanCancel = std::make_shared<std::atomic_bool>(false);
    auto cancel = _paletteScanCancel;
    QPointer<MainWindow> self(this);
    _paletteScan = QThread::create([self, roots, cancel] {
        scanFiles(roots, *cancel, [self, cancel](const QStringList& chunk) {
            QMetaObject::invokeMethod(self, [self, cancel, chunk] {
                if (!self || cancel->load() || !self->_palette)
                    return;
                std::vector<CommandPalette::Item> found;
                for (const QString& path : chunk) {
                    if (self->_paletteFiles.contains(path))
                        continue;
                    self->_paletteFiles.insert(path);
                    // El detalle: la carpeta, relativa a la carpeta de trabajo
                    QString detail = QFileInfo(path).absolutePath();
                    for (const QString& root : self->_folderPanel ? self->_folderPanel->roots() : QStringList()) {
                        if (detail == root || detail.startsWith(root + QLatin1Char('/'))) {
                            detail = QFileInfo(root).fileName() + detail.mid(root.size());
                            break;
                        }
                    }
                    if (const QString home = QDir::homePath(); detail == home || detail.startsWith(home + QLatin1Char('/')))
                        detail = QLatin1Char('~') + detail.mid(home.size());
                    CommandPalette::Item item;
                    item.text = QFileInfo(path).fileName();
                    item.detail = QDir::toNativeSeparators(detail);
                    item.icon = fileicons::enabled() ? fileicons::forFile(path) : QIcon();
                    item.order = 100000 + static_cast<int>(self->_paletteFiles.size());
                    QPointer<MainWindow> window = self;
                    item.run = [window, path] {
                        if (window) {
                            window->openFile(path);
                            window->editor()->setFocus();
                        }
                    };
                    found.push_back(std::move(item));
                }
                self->_palette->addItems(CommandPalette::Mode::Files, std::move(found));
            }, Qt::QueuedConnection);
        });
        QMetaObject::invokeMethod(self, [self, cancel] {
            if (self && !cancel->load() && self->_palette)
                self->_palette->setBusy(CommandPalette::Mode::Files, false);
        }, Qt::QueuedConnection);
    });
    _paletteScan->start();
}

// --- Buscar ------------------------------------------------------------------------

FindReplaceDialog* MainWindow::findDialog()
{
    if (_findDialog)
        return _findDialog;
    using Tab = FindReplaceDialog::Tab;
    _findDialog = new FindReplaceDialog(this);
    _findDialog->setMonospacedFont(_prefs.monospacedFontFindDlg);
    // Historial y opciones de la sesión anterior (<FindHistory> de config.xml)
    const FindHistory history = _syntax.config().findHistory();
    _lastOptions.matchCase = history.matchCase;
    _lastOptions.wholeWord = history.matchWord;
    _lastOptions.wrapAround = history.wrap;
    _lastOptions.searchUp = !history.directionDown;
    _lastOptions.dotMatchesNewline = history.dotMatchesNewline;
    _lastOptions.mode = static_cast<SearchMode>(std::clamp(history.searchMode, 0, 2));
    _findDialog->setHistories(history.finds, history.replaces, history.filters, history.paths);
    if (!history.finds.isEmpty())
        _lastOptions.pattern = history.finds.first().toStdString();
    if (!history.replaces.isEmpty())
        _lastOptions.replacement = history.replaces.first().toStdString();
    _findDialog->setOptions(_lastOptions);
    _findDialog->setFindInFilesOptions(history.recursive, history.hidden, history.folderFollowsDoc);
    _findDialog->setMarkOptions(history.bookmarkLine, history.purge);
    _findDialog->setHighlightMatches(history.findDlgHighlightMatches);
    _findDialog->setReplaceStops(_prefs.replaceStopsWithoutFindingNext);
    connect(_findDialog, &FindReplaceDialog::matchHighlightChanged, this, &MainWindow::updateFindDialogHighlight);
    connect(_findDialog, &FindReplaceDialog::replaceStopsChanged, this, [this](bool on) {
        if (_prefs.replaceStopsWithoutFindingNext == on)
            return;
        _prefs.replaceStopsWithoutFindingNext = on;
        storePreferences(_syntax.config(), _prefs);
        _syntax.config().save();
    });
    if (history.filters.isEmpty())
        _findDialog->setFilters(QStringLiteral("*.*"));
    auto run = [this](auto&& action) {
        return [this, action] {
            _findDialog->rememberCurrentTexts();
            action(_findDialog->options());
        };
    };
    connect(_findDialog, &FindReplaceDialog::findNextRequested, this, [this](bool) {
        _findDialog->rememberCurrentTexts();
        findNextInCurrent(_findDialog->options());
    });
    connect(_findDialog, &FindReplaceDialog::countRequested, this, run([this](const SearchOptions& o) { countInCurrent(o); }));
    connect(_findDialog, &FindReplaceDialog::markAllRequested, this, run([this](const SearchOptions& o) {
        markAllInCurrent(o, _findDialog->bookmarkLine(), _findDialog->purge());
    }));
    connect(_findDialog, &FindReplaceDialog::clearMarksRequested, this,
            [this] { clearMarks(_findDialog->options().inSelection, _findDialog->bookmarkLine()); });
    connect(_findDialog, &FindReplaceDialog::copyMarkedTextRequested, this, [this] { copyStyledText({kIndicatorFound}); });
    connect(_findDialog, &FindReplaceDialog::findAllInCurrentRequested, this,
            run([this](const SearchOptions& o) { findAllInCurrent(o); }));
    connect(_findDialog, &FindReplaceDialog::findAllInOpenRequested, this,
            run([this](const SearchOptions& o) { findAllInOpenDocuments(o); }));
    connect(_findDialog, &FindReplaceDialog::replaceRequested, this, run([this](const SearchOptions& o) { replaceInCurrent(o); }));
    connect(_findDialog, &FindReplaceDialog::replaceAllRequested, this,
            run([this](const SearchOptions& o) { replaceAllInCurrent(o); }));
    connect(_findDialog, &FindReplaceDialog::replaceAllInOpenRequested, this, run([this](const SearchOptions& o) {
        if (_prefs.confirmReplaceInAllOpenDocs
            && !_prompter->confirm(_findDialog, tr("Replace All"),
                                   tr("Replace all occurrences in all opened documents?")))
            return;
        replaceAllInOpenDocuments(o);
    }));
    connect(_findDialog, &FindReplaceDialog::findInFilesRequested, this, run([this](const SearchOptions& o) {
        findInFiles(_findDialog->directory(), _findDialog->filters(), _findDialog->recursive(), _findDialog->includeHidden(), o);
    }));
    connect(_findDialog, &FindReplaceDialog::replaceInFilesRequested, this, run([this](const SearchOptions& o) {
        replaceInFiles(_findDialog->directory(), _findDialog->filters(), _findDialog->recursive(),
                       _findDialog->includeHidden(), o);
    }));
    connect(_findDialog, &FindReplaceDialog::cancelFileSearchRequested, this, &MainWindow::cancelFileSearch);
    (void)Tab::Find;
    return _findDialog;
}

QString MainWindow::textForFindField() const
{
    // Como N++: la selección si es de una sola línea; si no hay, la palabra del cursor
    EditorView* view = editor();
    const sptr_t start = view->sci(SCI_GETSELECTIONSTART);
    const sptr_t end = view->sci(SCI_GETSELECTIONEND);
    ViewSearchTarget target(*view);
    if (end > start) {
        const std::string text = target.text(start, end);
        return text.find_first_of("\r\n") == std::string::npos ? QString::fromStdString(text) : QString();
    }
    const sptr_t pos = view->sci(SCI_GETCURRENTPOS);
    const sptr_t ws = view->sci(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(pos), 1);
    const sptr_t we = view->sci(SCI_WORDENDPOSITION, static_cast<uptr_t>(pos), 1);
    return QString::fromStdString(target.text(ws, we));
}

void MainWindow::showFindDialog(FindReplaceDialog::Tab tab)
{
    _findBarUsedLast = false;
    FindReplaceDialog* dialog = findDialog();
    if (tab == FindReplaceDialog::Tab::FindInFiles) {
        // El directorio del documento actual si el campo está vacío o se pide seguirlo
        const Buffer* buf = _documents.buffer(currentBuffer());
        if (buf && !buf->isUntitled() && (dialog->followCurrentDocument() || dialog->directory().isEmpty()))
            dialog->setDirectory(QFileInfo(buf->filePath()).absolutePath());
        else if (dialog->directory().isEmpty())
            dialog->setDirectory(suggestedDirectory());
    }
    dialog->showTab(tab, _prefs.fillFindFieldWithSelected ? textForFindField() : QString());
}

void MainWindow::setSearchStatus(const QString& text, FindReplaceDialog::StatusKind kind)
{
    _lastSearchStatus = text;
    if (_findDialog)
        _findDialog->setStatus(text, kind);
    statusBar()->showMessage(text, 5000);
}

QString MainWindow::scopeText(const SearchOptions& options, bool isEntire) const
{
    // getScopeInfoForStatusBar
    if (options.inSelection && !isEntire)
        return tr("in selection");
    if (options.wrapAround || isEntire)
        return tr("in entire file");
    return options.searchUp ? tr("from start-of-file to caret") : tr("from caret to end-of-file");
}

void MainWindow::selectFoundRange(SciPos start, SciPos end)
{
    // Searching::displaySectionCentered: visible y, si hace falta, centrado
    EditorView* view = editor();
    const sptr_t line = view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(start));
    view->sci(SCI_ENSUREVISIBLE, static_cast<uptr_t>(line));
    view->sci(SCI_SETSEL, static_cast<uptr_t>(start), end);
    const sptr_t first = view->sci(SCI_GETFIRSTVISIBLELINE);
    const sptr_t onScreen = view->sci(SCI_LINESONSCREEN);
    const sptr_t visible = view->sci(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(line));
    if (visible < first || visible >= first + onScreen)
        view->sci(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(std::max<sptr_t>(0, visible - onScreen / 2)));
    view->sci(SCI_SCROLLRANGE, static_cast<uptr_t>(end), start);
}

FindResult MainWindow::findNextInCurrent(const SearchOptions& options)
{
    using Kind = FindReplaceDialog::StatusKind;
    _lastOptions = options;
    EditorView* view = editor();
    view->sci(SCI_CALLTIPCANCEL);
    ViewSearchTarget target(*view);
    const FindResult r = findNext(target, options, view->sci(SCI_GETSELECTIONSTART), view->sci(SCI_GETSELECTIONEND));
    const QString pattern = QString::fromStdString(options.pattern);
    switch (r.status) {
    case FindStatus::Found:
        setSearchStatus(QString(), Kind::Message);
        break;
    case FindStatus::EndReached:
        setSearchStatus(tr("Find: Found the 1st occurrence from the top. The end of the document has been reached."),
                        Kind::EndReached);
        break;
    case FindStatus::TopReached:
        setSearchStatus(tr("Find: Found the 1st occurrence from the bottom. The beginning of the document has been reached."),
                        Kind::EndReached);
        break;
    case FindStatus::NotFound: {
        QString msg = tr("Find: Can't find the text \"%1\"").arg(pattern);
        const bool lax = options.wrapAround && !options.matchCase && !options.wholeWord;
        if (!lax)
            msg += tr(" (maybe a search option is not set correctly?)");
        setSearchStatus(msg, Kind::NotFound);
        break;
    }
    case FindStatus::InvalidRegex:
        setSearchStatus(tr("Find: Invalid regular expression: %1").arg(QString::fromStdString(r.regexError)), Kind::NotFound);
        break;
    }
    if (r.found()) {
        selectFoundRange(r.start, r.end);
        if (r.start == r.end)
            view->sends(SCI_CALLTIPSHOW, static_cast<uptr_t>(r.start), tr("^ zero length match").toUtf8().constData());
    }
    return r;
}

bool MainWindow::replaceInCurrent(const SearchOptions& options)
{
    using Kind = FindReplaceDialog::StatusKind;
    _lastOptions = options;
    const Buffer* buf = _documents.buffer(currentBuffer());
    if (!buf || buf->isReadOnly()) {
        setSearchStatus(tr("Replace: Cannot replace text. The current document is read only."), Kind::NotFound);
        return false;
    }
    EditorView* view = editor();
    ViewSearchTarget target(*view);
    const bool stop = _prefs.replaceStopsWithoutFindingNext;
    const ReplaceOutcome r =
        replaceNext(target, options, view->sci(SCI_GETSELECTIONSTART), view->sci(SCI_GETSELECTIONEND), stop);
    if (r.next.status == FindStatus::InvalidRegex) {
        setSearchStatus(tr("Replace: Invalid regular expression: %1").arg(QString::fromStdString(r.next.regexError)),
                        Kind::NotFound);
        return false;
    }
    if (r.selStart >= 0) {
        if (r.next.found())
            selectFoundRange(r.selStart, r.selEnd);
        else
            view->sci(SCI_SETSEL, static_cast<uptr_t>(r.selStart), r.selEnd);
    }
    if (!r.replaced) {
        if (!r.next.found())
            setSearchStatus(tr("Replace: no occurrence was found %1").arg(scopeText(options, false)), Kind::NotFound);
        else
            setSearchStatus(QString(), Kind::Message);
    } else if (stop) {
        setSearchStatus(tr("Replace: 1 occurrence was replaced."), Kind::Message);
    } else if (r.next.status == FindStatus::EndReached) {
        setSearchStatus(tr("Replace: 1 occurrence was replaced. The end of the document has been reached."), Kind::EndReached);
    } else if (r.next.status == FindStatus::TopReached) {
        setSearchStatus(tr("Replace: 1 occurrence was replaced. The beginning of the document has been reached."), Kind::EndReached);
    } else if (r.next.found()) {
        setSearchStatus(tr("Replace: 1 occurrence was replaced. The next occurrence found."), Kind::Message);
    } else {
        setSearchStatus(tr("Replace: 1 occurrence was replaced. No more occurrences were found."), Kind::Message);
    }
    return r.replaced;
}

int MainWindow::replaceAllInCurrent(const SearchOptions& options)
{
    using Kind = FindReplaceDialog::StatusKind;
    _lastOptions = options;
    const Buffer* buf = _documents.buffer(currentBuffer());
    if (!buf || buf->isReadOnly()) {
        setSearchStatus(tr("Replace All: Cannot replace text. The current document is read only."), Kind::NotFound);
        return 0;
    }
    EditorView* view = editor();
    ViewSearchTarget target(*view);
    const SciPos selStart = view->sci(SCI_GETSELECTIONSTART);
    const SciPos selEnd = view->sci(SCI_GETSELECTIONEND);
    const SciPos length = target.length();
    const auto [start, end] = rangeForOperation(RangeOperation::ReplaceAll, options, selStart, selEnd, length, false);
    // Todo el reemplazo se deshace de una vez
    view->sci(SCI_BEGINUNDOACTION);
    const RangeResult r = processRange(target, options, RangeOperation::ReplaceAll, start, end);
    view->sci(SCI_ENDUNDOACTION);
    if (r.invalidRegex) {
        setSearchStatus(tr("Replace All: Invalid regular expression: %1").arg(QString::fromStdString(r.regexError)),
                        Kind::NotFound);
        return -1;
    }
    if (r.count > 0 && options.inSelection) {
        // La selección abarca ahora el texto ya reemplazado
        const SciPos newEnd = end + (target.length() - length);
        view->sci(SCI_SETSEL, static_cast<uptr_t>(start), newEnd);
    }
    const QString scope = scopeText(options, false);
    if (r.count == 0)
        setSearchStatus(tr("Replace All: 0 occurrences were replaced %1").arg(scope), Kind::NotFound);
    else
        setSearchStatus(tr("Replace All: %Ln occurrence(s) were replaced %1", nullptr, r.count).arg(scope), Kind::Message);
    return r.count;
}

int MainWindow::countInCurrent(const SearchOptions& options)
{
    using Kind = FindReplaceDialog::StatusKind;
    _lastOptions = options;
    EditorView* view = editor();
    ViewSearchTarget target(*view);
    const auto [start, end] = rangeForOperation(RangeOperation::CountAll, options, view->sci(SCI_GETSELECTIONSTART),
                                                view->sci(SCI_GETSELECTIONEND), target.length(), false);
    const RangeResult r = processRange(target, options, RangeOperation::CountAll, start, end);
    if (r.invalidRegex) {
        setSearchStatus(tr("Count: Invalid regular expression: %1").arg(QString::fromStdString(r.regexError)), Kind::NotFound);
        return -1;
    }
    setSearchStatus(tr("Count: %Ln match(es) %1", nullptr, r.count).arg(scopeText(options, false)),
                    r.count ? Kind::Message : Kind::NotFound);
    return r.count;
}

int MainWindow::markAllInCurrent(const SearchOptions& options, bool bookmarkLine, bool purge)
{
    using Kind = FindReplaceDialog::StatusKind;
    _lastOptions = options;
    EditorView* view = editor();
    if (purge)
        clearMarks(options.inSelection, bookmarkLine);
    ViewSearchTarget target(*view);
    const auto [start, end] = rangeForOperation(RangeOperation::MarkAll, options, view->sci(SCI_GETSELECTIONSTART),
                                                view->sci(SCI_GETSELECTIONEND), target.length(), false);
    const RangeResult r = processRange(target, options, RangeOperation::MarkAll, start, end, [view, bookmarkLine](SciPos from, SciPos to) {
        // Una coincidencia vacía («^$») no se puede pintar, pero sí marca su línea
        if (to > from) {
            view->sci(SCI_SETINDICATORCURRENT, kIndicatorFound);
            view->sci(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(from), to - from);
        }
        if (bookmarkLine) {
            const sptr_t first = view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(from));
            const sptr_t last = to > from ? view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(to - 1)) : first;
            for (sptr_t line = first; line <= last; ++line) {
                if (!(view->sci(SCI_MARKERGET, static_cast<uptr_t>(line)) & (1 << kBookmarkMarker)))
                    view->sci(SCI_MARKERADD, static_cast<uptr_t>(line), kBookmarkMarker);
            }
        }
    });
    if (r.invalidRegex) {
        setSearchStatus(tr("Mark: Invalid regular expression: %1").arg(QString::fromStdString(r.regexError)), Kind::NotFound);
        return -1;
    }
    setSearchStatus(tr("Mark: %Ln match(es) %1", nullptr, r.count).arg(scopeText(options, false)),
                    r.count ? Kind::Message : Kind::NotFound);
    if (ScrollBarMarks* marks = scrollBarMarksOfCurrentView())
        marks->scheduleUpdate();
    return r.count;
}

void MainWindow::clearMarks(bool inSelection, bool bookmarkLine)
{
    EditorView* view = editor();
    sptr_t start = 0;
    sptr_t end = view->sci(SCI_GETLENGTH);
    if (inSelection) {
        start = view->sci(SCI_GETSELECTIONSTART);
        end = view->sci(SCI_GETSELECTIONEND);
    }
    view->sci(SCI_SETINDICATORCURRENT, kIndicatorFound);
    view->sci(SCI_INDICATORCLEARRANGE, static_cast<uptr_t>(start), end - start);
    if (bookmarkLine) {
        if (!inSelection) {
            view->sci(SCI_MARKERDELETEALL, kBookmarkMarker);
        } else if (end > start) {
            const sptr_t last = view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(end - 1));
            for (sptr_t line = view->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(start)); line <= last; ++line)
                view->sci(SCI_MARKERDELETE, static_cast<uptr_t>(line), kBookmarkMarker);
        }
    }
    setSearchStatus(QString(), FindReplaceDialog::StatusKind::Message);
    if (ScrollBarMarks* marks = scrollBarMarksOfCurrentView())
        marks->scheduleUpdate();
}

void MainWindow::copyStyledText(const std::vector<int>& indicators)
{
    EditorView* view = editor();
    const sptr_t length = view->sci(SCI_GETLENGTH);
    std::vector<std::pair<sptr_t, QString>> pieces;
    bool multiline = false;
    for (const int indicator : indicators) {
        for (sptr_t pos = 0; pos < length;) {
            sptr_t start = pos;
            if (!view->sci(SCI_INDICATORVALUEAT, static_cast<uptr_t>(indicator), pos))
                start = view->sci(SCI_INDICATOREND, static_cast<uptr_t>(indicator), pos);
            if (start >= length || start <= pos - 1)
                break;
            const sptr_t end = view->sci(SCI_INDICATOREND, static_cast<uptr_t>(indicator), start);
            if (end <= start)
                break;
            const auto* text = reinterpret_cast<const char*>(view->sci(SCI_GETRANGEPOINTER, static_cast<uptr_t>(start), end - start));
            const QString piece = QString::fromUtf8(text, static_cast<qsizetype>(end - start));
            multiline = multiline || piece.contains(QLatin1Char('\n')) || piece.contains(QLatin1Char('\r'));
            pieces.emplace_back(start, piece);
            pos = end;
        }
    }
    if (pieces.empty())
        return;
    std::stable_sort(pieces.begin(), pieces.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    // Como N++: un elemento por línea, y «----» entre ellos si alguno ocupa varias líneas
    const std::string_view eolBytes = eolSequence(static_cast<EolType>(view->sci(SCI_GETEOLMODE)));
    const QString eol = QString::fromLatin1(eolBytes.data(), static_cast<qsizetype>(eolBytes.size()));
    const QString delimiter = multiline && pieces.size() > 1 ? eol + QStringLiteral("----") + eol : eol;
    QStringList texts;
    for (const auto& [position, text] : pieces)
        texts << text;
    QString joined = texts.join(delimiter);
    if (pieces.size() > 1)
        joined += eol;
    QGuiApplication::clipboard()->setText(joined);
}

int MainWindow::findAllInCurrent(const SearchOptions& options)
{
    using Kind = FindReplaceDialog::StatusKind;
    _lastOptions = options;
    const Buffer* buf = _documents.buffer(currentBuffer());
    if (!buf)
        return 0;
    EditorView* view = editor();
    ViewSearchTarget target(*view);
    const bool isEntire = !options.inSelection;
    const auto [start, end] = rangeForOperation(RangeOperation::FindAll, options, view->sci(SCI_GETSELECTIONSTART),
                                                view->sci(SCI_GETSELECTIONEND), target.length(), isEntire);
    RangeResult r;
    const std::vector<LineHit> lines = findAllLines(target, options, start, end, r);
    if (r.invalidRegex) {
        setSearchStatus(tr("Find All: Invalid regular expression: %1").arg(QString::fromStdString(r.regexError)), Kind::NotFound);
        return -1;
    }
    _results->beginSearch(QString::fromStdString(options.pattern));
    if (r.count > 0)
        _results->addFile(buf->isUntitled() ? buf->displayName() : buf->filePath(), buf->filePath(), buf->id(), lines, r.count);
    _results->endSearch(r.count, 1, options, !isEntire);
    _results->show();
    setSearchStatus(tr("Find All in Current Document: %Ln hit(s)", nullptr, r.count),
                    r.count ? Kind::Message : Kind::NotFound);
    return r.count;
}

void MainWindow::withSearchView(const Buffer& buffer, const std::function<void(ScintillaEditBase&)>& work)
{
    _searchView->send(SCI_SETDOCPOINTER, 0, buffer.document());
    work(*_searchView);
    _searchView->send(SCI_SETDOCPOINTER, 0, _searchViewDefaultDoc);
}

int MainWindow::findAllInOpenDocuments(const SearchOptions& options)
{
    using Kind = FindReplaceDialog::StatusKind;
    _lastOptions = options;
    int total = 0;
    int searched = 0;
    bool invalid = false;
    QString error;
    _results->beginSearch(QString::fromStdString(options.pattern));
    for (const BufferId id : allBuffers()) {
        const Buffer* buf = _documents.buffer(id);
        if (!buf || invalid)
            continue;
        ++searched;
        withSearchView(*buf, [&](ScintillaEditBase& view) {
            ViewSearchTarget target(view);
            RangeResult r;
            const auto lines = findAllLines(target, options, 0, target.length(), r);
            if (r.invalidRegex) {
                invalid = true;
                error = QString::fromStdString(r.regexError);
                return;
            }
            if (r.count > 0) {
                _results->addFile(buf->isUntitled() ? buf->displayName() : buf->filePath(), buf->filePath(), id, lines, r.count);
                total += r.count;
            }
        });
    }
    if (invalid) {
        _results->endSearch(0, searched, options);
        setSearchStatus(tr("Find All: Invalid regular expression: %1").arg(error), Kind::NotFound);
        return -1;
    }
    _results->endSearch(total, searched, options);
    _results->show();
    setSearchStatus(tr("Find All in All Opened Documents: %Ln hit(s)", nullptr, total),
                    total ? Kind::Message : Kind::NotFound);
    return total;
}

int MainWindow::replaceAllInOpenDocuments(const SearchOptions& options)
{
    using Kind = FindReplaceDialog::StatusKind;
    _lastOptions = options;
    int total = 0;
    int documents = 0;
    for (const BufferId id : allBuffers()) {
        const Buffer* buf = _documents.buffer(id);
        if (!buf || buf->isReadOnly())
            continue;
        int count = 0;
        withSearchView(*buf, [&](ScintillaEditBase& view) {
            ViewSearchTarget target(view);
            view.send(SCI_BEGINUNDOACTION);
            count = processRange(target, options, RangeOperation::ReplaceAll, 0, target.length()).count;
            view.send(SCI_ENDUNDOACTION);
        });
        if (count > 0) {
            total += count;
            ++documents;
            _documents.syncDirtyState(id); // la vista visible quizá no muestra ese documento
        }
    }
    setSearchStatus(tr("Replace All in All Opened Documents: %Ln occurrence(s) were replaced in %1", nullptr, total)
                        .arg(tr("%Ln document(s)", nullptr, documents)),
                    total ? Kind::Message : Kind::NotFound);
    return total;
}

bool MainWindow::isFileSearchRunning() const
{
    return _fileJob && _fileJob->isRunning();
}

void MainWindow::cancelFileSearch()
{
    if (_fileJob)
        _fileJob->cancel();
}

void MainWindow::startFileJob(const FileSearchRequest& request, int alreadyReplaced, int alreadyFiles)
{
    using Kind = FindReplaceDialog::StatusKind;
    _fileJob = std::make_unique<FileSearchJob>(request);
    FileSearchJob* job = _fileJob.get();
    const bool replacing = request.replace;
    if (!replacing) {
        _results->beginSearch(QString::fromStdString(request.options.pattern));
        _results->show();
    }
    if (_findDialog)
        _findDialog->setFileSearchRunning(true);

    connect(job, &FileSearchJob::fileFound, this, [this](const FileHits& hits) {
        _results->addFile(hits.path, hits.path, _documents.findByPath(hits.path), hits.lines, hits.count);
    });
    connect(job, &FileSearchJob::progress, this, [this, replacing](int done, int total) {
        setSearchStatus((replacing ? tr("Replacing… %1 of %2 files") : tr("Searching… %1 of %2 files")).arg(done).arg(total),
                        FindReplaceDialog::StatusKind::Message);
    });
    connect(job, &FileSearchJob::finished, this, [this, replacing, alreadyReplaced, alreadyFiles,
                                                   options = request.options](const FileSearchSummary& s) {
        if (_findDialog)
            _findDialog->setFileSearchRunning(false);
        QString msg;
        Kind kind = Kind::Message;
        if (s.invalidRegex) {
            msg = tr("Find in Files: Invalid regular expression: %1").arg(s.regexError);
            kind = Kind::NotFound;
        } else if (replacing) {
            msg = tr("Replace in Files: %Ln occurrence(s) were replaced in %1", nullptr, s.totalHits + alreadyReplaced)
                      .arg(tr("%Ln file(s)", nullptr, s.filesWithHits + alreadyFiles));
        } else {
            _results->endSearch(s.totalHits, s.filesSearched, options);
            msg = tr("Find in Files: %Ln hit(s) in %1 of %2 searched", nullptr, s.totalHits)
                      .arg(s.filesWithHits)
                      .arg(tr("%Ln file(s)", nullptr, s.filesSearched));
            kind = s.totalHits ? Kind::Message : Kind::NotFound;
        }
        if (s.cancelled)
            msg += tr(" (canceled)");
        if (!s.errors.isEmpty())
            msg += tr(" — %Ln file(s) with errors", nullptr, static_cast<int>(s.errors.size()));
        setSearchStatus(msg, kind);
        // El hilo ya terminó: se libera después de entregar esta señal
        QTimer::singleShot(0, this, [this] {
            if (_fileJob && !_fileJob->isRunning()) {
                _fileJob->wait();
                _fileJob.reset();
            }
        });
    });
    job->start();
}

void MainWindow::findInFiles(const QString& directory, const QString& filters, bool recursive, bool includeHidden,
                             const SearchOptions& options)
{
    using Kind = FindReplaceDialog::StatusKind;
    if (isFileSearchRunning())
        return;
    _lastOptions = options;
    if (directory.isEmpty() || !QFileInfo(directory).isDir()) {
        setSearchStatus(tr("Find in Files: The directory doesn't exist: %1").arg(directory), Kind::NotFound);
        return;
    }
    if (options.pattern.empty())
        return;
    FileSearchRequest request;
    request.directory = QDir(directory).absolutePath();
    request.filters = filters;
    request.recursive = recursive;
    request.includeHidden = includeHidden;
    request.options = options;
    request.load = _documents.settings().load;
    // Se busca en el texto actual de los documentos abiertos (con cambios sin guardar)
    for (const BufferId id : allBuffers()) {
        const Buffer* buf = _documents.buffer(id);
        if (buf && !buf->isUntitled())
            request.openDocuments[QDir::cleanPath(buf->filePath())] = _documents.text(id);
    }
    startFileJob(request, 0, 0);
}

void MainWindow::replaceInFiles(const QString& directory, const QString& filters, bool recursive, bool includeHidden,
                                const SearchOptions& options)
{
    using Kind = FindReplaceDialog::StatusKind;
    if (isFileSearchRunning())
        return;
    _lastOptions = options;
    if (directory.isEmpty() || !QFileInfo(directory).isDir()) {
        setSearchStatus(tr("Replace in Files: The directory doesn't exist: %1").arg(directory), Kind::NotFound);
        return;
    }
    if (options.pattern.empty())
        return;
    if (!_prompter->confirm(this, tr("Replace in Files"),
                            tr("Replace all occurrences of \"%1\" with \"%2\" in the \"%3\" files of %4?")
                                .arg(QString::fromStdString(options.pattern), QString::fromStdString(options.replacement),
                                     filters.isEmpty() ? QStringLiteral("*.*") : filters, directory)))
        return;

    // Como N++: los archivos abiertos se reemplazan en su documento y se guardan
    const QString absDir = QDir(directory).absolutePath();
    const QStringList files = listMatchingFiles(absDir, parseFilters(filters), recursive, includeHidden);
    int replaced = 0;
    int replacedFiles = 0;
    FileSearchRequest request;
    for (const QString& file : files) {
        const BufferId id = _documents.findByPath(file);
        const Buffer* buf = _documents.buffer(id);
        if (!buf)
            continue;
        request.openDocuments[QDir::cleanPath(file)] = {}; // el hilo se lo salta
        if (buf->isReadOnly())
            continue;
        int count = 0;
        withSearchView(*buf, [&](ScintillaEditBase& view) {
            ViewSearchTarget target(view);
            view.send(SCI_BEGINUNDOACTION);
            const RangeResult r = processRange(target, options, RangeOperation::ReplaceAll, 0, target.length());
            view.send(SCI_ENDUNDOACTION);
            count = r.invalidRegex ? 0 : r.count;
        });
        if (count > 0) {
            replaced += count;
            ++replacedFiles;
            _documents.syncDirtyState(id);
            if (_documents.saveBuffer(id, buf->filePath()).ok())
                afterSave(buf->filePath());
        }
    }
    request.directory = absDir;
    request.filters = filters;
    request.recursive = recursive;
    request.includeHidden = includeHidden;
    request.options = options;
    request.replace = true;
    request.load = _documents.settings().load;
    startFileJob(request, replaced, replacedFiles);
}

void MainWindow::findSelectedText(bool backward)
{
    // IDM_SEARCH_VOLATILE_FINDNEXT: distingue mayúsculas, sin palabra completa
    const QString text = textForFindField();
    if (text.isEmpty())
        return;
    SearchOptions o;
    o.pattern = text.toStdString();
    o.matchCase = true;
    o.searchUp = backward;
    findNextInCurrent(o);
}

void MainWindow::openSearchResult(const QString& path, BufferId buffer, SciPos start, SciPos end)
{
    BufferId target = _documents.buffer(buffer) ? buffer : kInvalidBuffer;
    if (target == kInvalidBuffer && !path.isEmpty())
        target = openFile(path);
    if (target == kInvalidBuffer)
        return;
    if (EditorPane* pane = paneContaining(target)) {
        pane->activateBuffer(target);
        setActivePane(pane);
    }
    selectFoundRange(start, end);
    editor()->setFocus();
}

// --- Vista dividida ------------------------------------------------------------------

void MainWindow::transferToOtherView(bool clone)
{
    // docGotoAnotherEditView
    const BufferId id = currentBuffer();
    Buffer* buf = _documents.buffer(id);
    if (!buf)
        return;
    EditorPane* from = _activePane;
    EditorPane* to = otherPane(from);
    // Mover el único documento a una vista vacía no tiene sentido
    if (!clone && from->count() == 1 && to->count() == 0)
        return;

    if (to->contains(id)) {
        to->activateBuffer(id);
    } else {
        // La otra vista empieza donde estaba esta (cursor y scroll)
        from->syncCurrentPosition();
        buf->setPosition(to->editor(), buf->position(from->editor()));
        showLoadedBuffer(id, to);
    }
    if (!clone)
        from->removeBuffer(id);
    updatePaneVisibility();
    setActivePane(to);
    to->editor()->setFocus();
}

void MainWindow::moveToOtherView()
{
    transferToOtherView(false);
}

void MainWindow::cloneToOtherView()
{
    transferToOtherView(true);
}

void MainWindow::switchToOtherView()
{
    EditorPane* other = otherPane(_activePane);
    if (other->count() == 0)
        return;
    setActivePane(other);
    other->editor()->setFocus();
}

void MainWindow::toggleSplitOrientation()
{
    _splitter->setOrientation(_splitter->orientation() == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal);
}

// --- Sesión ----------------------------------------------------------------------------

QString MainWindow::sessionPath() const
{
    return QDir(paths::configDir()).filePath(QStringLiteral("session.xml"));
}

Session MainWindow::captureSession(bool named, bool withFolders)
{
    // Notepad_plus::getCurrentOpenedFiles
    Session session;
    const bool snapshot = isSnapshotMode() && !named;
    EditorPane* panes[] = {_mainPane, _subPane};
    for (int k = 0; k < 2; ++k) {
        EditorPane* pane = panes[k];
        pane->syncCurrentPosition();
        auto& files = k == 0 ? session.mainView : session.subView;
        int activeIndex = 0;
        for (const BufferId id : pane->buffers()) {
            const Buffer* buf = _documents.buffer(id);
            if (!buf)
                continue;
            // En modo snapshot van también los cambios sin guardar y los documentos
            // sin título, gracias a sus copias; si no, solo los archivos que existen
            const bool withBackup = snapshot && !buf->backupPath().isEmpty() && QFileInfo::exists(buf->backupPath());
            if (!withBackup && (buf->isUntitled() || !QFileInfo::exists(buf->filePath())))
                continue;
            if (id == pane->currentBuffer())
                activeIndex = static_cast<int>(files.size());
            SessionFile f;
            f.path = buf->isUntitled() ? buf->displayName() : buf->filePath();
            if (withBackup) {
                f.backupFilePath = buf->backupPath();
                f.originalLastModified = buf->lastModified();
            }
            const char* menuName = languageInfo(buf->language()).menuName;
            // Un lenguaje del usuario o de un plugin se guarda por su nombre, como en N++
            f.languageName = buf->hasNamedLanguage()
                ? buf->userLanguage()
                : QString::fromUtf8(menuName ? menuName : languageInfo(buf->language()).name);
            f.encoding = buf->encoding().codepage;
            f.userReadOnly = buf->isUserReadOnly();
            f.tabPinned = buf->isPinned();
            f.tabColor = buf->tabColor();
            f.position = buf->position(pane->editor());
            withSearchView(*buf, [&f](ScintillaEditBase& view) {
                for (const sptr_t line : bookmarkedLines(view))
                    f.bookmarks.push_back(static_cast<std::size_t>(line));
            });
            files.push_back(std::move(f));
        }
        (k == 0 ? session.activeMainIndex : session.activeSubIndex) = activeIndex;
    }
    session.activeView = (_activePane == _subPane && !session.subView.empty()) ? 1 : 0;
    // Las carpetas del Explorador de carpetas (session._includeFileBrowser)
    if (withFolders && _folderPanel && !_folderPanel->roots().isEmpty()) {
        session.hasFileBrowser = true;
        session.folderRoots = _folderPanel->state();
        session.folderSelected = _folderPanel->selectedPath();
    }
    return session;
}

void MainWindow::restoreSession(const Session& session, QStringList* missing)
{
    // Las carpetas vuelven, pero el panel se queda como estaba (config.xml): cerrado, si se
    // cerró. Como N++, que solo lo abre al cargar a mano una sesión (loadSessionFile)
    if (session.hasFileBrowser && _folderPanel)
        _folderPanel->setState(session.folderRoots, session.folderSelected);
    // Notepad_plus::loadSession. El "nuevo 1" inicial sin cambios desaparece si se abre algo.
    BufferId initialUntitled = kInvalidBuffer;
    if (_mainPane->count() == 1 && _subPane->count() == 0) {
        const Buffer* buf = _documents.buffer(_mainPane->buffers().front());
        if (buf && buf->isUntitled() && !buf->isDirty())
            initialUntitled = buf->id();
    }

    const bool snapshot = isSnapshotMode();
    auto findByBackup = [this](const QString& backup) {
        for (std::size_t i = 0; i < _documents.count(); ++i) {
            if (_documents.bufferAt(i)->backupPath() == backup)
                return _documents.bufferAt(i)->id();
        }
        return kInvalidBuffer;
    };
    bool opened = false;
    EditorPane* panes[] = {_mainPane, _subPane};
    for (int k = 0; k < 2; ++k) {
        EditorPane* pane = panes[k];
        const auto& files = k == 0 ? session.mainView : session.subView;
        for (const SessionFile& f : files) {
            // Notepad_plus::loadSession: con snapshot, la copia manda sobre el archivo
            const bool fromBackup = snapshot && !f.backupFilePath.isEmpty() && QFileInfo::exists(f.backupFilePath);
            if (!fromBackup && !QFileInfo::exists(f.path)) {
                if (missing)
                    *missing << f.path;
                continue;
            }
            // Un documento de la vista secundaria que ya está en la principal es un clon
            BufferId id = fromBackup ? findByBackup(f.backupFilePath) : _documents.findByPath(f.path);
            if (id == kInvalidBuffer) {
                QString error;
                id = fromBackup ? _documents.loadFromBackup(f.path, f.backupFilePath, f.encoding, f.originalLastModified, &error)
                                : _documents.loadFile(f.path, f.encoding, &error);
                if (id == kInvalidBuffer) {
                    if (missing)
                        *missing << f.path;
                    continue;
                }
            }
            Buffer* buf = _documents.buffer(id);
            bool known = false;
            const LangType lang = langTypeFromMenuName(f.languageName.toStdString(), &known);
            if (known && lang != buf->language() && !buf->isLargeFile())
                buf->setLanguage(lang, /*fromMenu*/ true); // distinto del detectado: lo eligió el usuario
            else if (!known && _syntax.userLanguages().find(f.languageName) && buf->userLanguage() != f.languageName)
                buf->setUserLanguage(f.languageName, /*fromMenu*/ true);
            else if (!known && _syntax.externalLexers().find(f.languageName.toStdString())
                     && buf->userLanguage() != f.languageName)
                buf->setExternalLanguage(f.languageName, /*fromMenu*/ true);
            buf->setUserReadOnly(f.userReadOnly);
            buf->setPinned(f.tabPinned);
            buf->setTabColor(f.tabColor);
            buf->setPosition(pane->editor(), f.position);
            if (!f.bookmarks.empty()) {
                withSearchView(*buf, [&f](ScintillaEditBase& view) {
                    setBookmarks(view, std::vector<sptr_t>(f.bookmarks.begin(), f.bookmarks.end()));
                });
            }
            if (!pane->contains(id))
                pane->addBuffer(id);
            opened = true;
        }
    }
    if (opened && initialUntitled != kInvalidBuffer)
        discardBuffer(initialUntitled);

    for (int k = 0; k < 2; ++k) {
        EditorPane* pane = panes[k];
        const std::vector<BufferId> ids = pane->buffers();
        if (ids.empty())
            continue;
        const int index = k == 0 ? session.activeMainIndex : session.activeSubIndex;
        pane->activateBuffer(ids[static_cast<std::size_t>(std::clamp(index, 0, static_cast<int>(ids.size()) - 1))]);
    }
    if (_mainPane->count() == 0 && _subPane->count() == 0)
        newDocument();
    updatePaneVisibility();
    EditorPane* active = (session.activeView == 1 && _subPane->count() > 0) ? _subPane : _mainPane;
    if (active->count() == 0)
        active = otherPane(active);
    _activePane = active;
    refreshCurrentDocument();
    active->editor()->setFocus();
}

bool MainWindow::isSessionFile(const QString& path) const
{
    return !_prefs.sessionExt.isEmpty() && QFileInfo(path).suffix().compare(_prefs.sessionExt, Qt::CaseInsensitive) == 0;
}

bool MainWindow::loadSessionFile(const QString& path)
{
    Session session;
    QString error;
    if (!readSessionFile(path, session, &error)) {
        _prompter->warn(this, tr("Load Session"), tr("%1 is not a session file.").arg(QFileInfo(path).fileName()));
        return false;
    }
    // Las carpetas se añaden a las que ya hay (las de la sesión no sustituyen a las abiertas)
    if (session.hasFileBrowser) {
        for (const Session::FolderRoot& root : session.folderRoots) {
            if (QFileInfo(root.folder).isDir())
                addFolderToWorkspace(root.folder);
        }
        session.hasFileBrowser = false;
    }
    QStringList missing;
    restoreSession(session, &missing);
    if (!missing.isEmpty())
        _notifications->show(NotificationBar::Kind::Warning, tr("Load Session"),
                             tr("These files of the session could not be opened:\n%1").arg(missing.join(QLatin1Char('\n'))));
    return true;
}

bool MainWindow::saveSessionFile(const QString& path, bool withFolders)
{
    QString error;
    if (!writeSessionFile(path, captureSession(/*named*/ true, withFolders), &error)) {
        _prompter->warn(this, tr("Save Session"), error);
        return false;
    }
    return true;
}

bool MainWindow::restoreLastSession()
{
    if (!_sessionEnabled || !_prefs.rememberLastSession)
        return false;
    Session session;
    bool usedBackup = false;
    if (!readSessionWithBackup(sessionPath(), session, &usedBackup))
        return false;
    if (usedBackup)
        _prompter->warn(this, tr("Session"), tr("session.xml was damaged; the backup copy has been used."));
    restoreSession(session);
    return true;
}

bool MainWindow::saveSession()
{
    QString error;
    const bool ok = writeSessionFile(sessionPath(), captureSession(), &error);
    if (!ok)
        _prompter->warn(this, tr("Session"), error);
    return ok;
}

void MainWindow::restoreWindowGeometry()
{
    // GUIConfig "AppPosition" de N++
    ConfigXml& config = _syntax.config();
    const int width = config.guiConfig(QStringLiteral("AppPosition"), QStringLiteral("width")).toInt();
    const int height = config.guiConfig(QStringLiteral("AppPosition"), QStringLiteral("height")).toInt();
    if (width > 200 && height > 150)
        resize(width, height);
    if (config.guiConfig(QStringLiteral("AppPosition"), QStringLiteral("isMaximized")) == QLatin1String("yes"))
        setWindowState(windowState() | Qt::WindowMaximized);
}

void MainWindow::saveWindowState()
{
    ConfigXml& config = _syntax.config();
    const QString name = QStringLiteral("AppPosition");
    config.setGuiConfig(name, QStringLiteral("x"), QString::number(normalGeometry().x()));
    config.setGuiConfig(name, QStringLiteral("y"), QString::number(normalGeometry().y()));
    config.setGuiConfig(name, QStringLiteral("width"), QString::number(normalGeometry().width()));
    config.setGuiConfig(name, QStringLiteral("height"), QString::number(normalGeometry().height()));
    config.setGuiConfig(name, QStringLiteral("isMaximized"), isMaximized() ? QStringLiteral("yes") : QStringLiteral("no"));
    RecentFilesConfig recent = config.recentFiles();
    recent.files = _recentFiles.mid(0, _prefs.recentFilesMax);
    config.setRecentFiles(recent);
    config.setGuiConfig(QStringLiteral("openSaveDir"), QStringLiteral("lastUsedDirPath"), _lastDirectory);
    if (_findDialog) {
        FindHistory h;
        h.finds = _findDialog->findHistory();
        h.replaces = _findDialog->replaceHistory();
        h.filters = _findDialog->filterHistory();
        h.paths = _findDialog->directoryHistory();
        const SearchOptions o = _findDialog->options();
        h.matchCase = o.matchCase;
        h.matchWord = o.wholeWord;
        h.wrap = o.wrapAround;
        h.directionDown = !o.searchUp;
        h.dotMatchesNewline = o.dotMatchesNewline;
        h.searchMode = static_cast<int>(o.mode);
        h.recursive = _findDialog->recursive();
        h.hidden = _findDialog->includeHidden();
        h.folderFollowsDoc = _findDialog->followCurrentDocument();
        h.bookmarkLine = _findDialog->bookmarkLine();
        h.purge = _findDialog->purge();
        h.findDlgHighlightMatches = _findDialog->highlightMatches();
        config.setFindHistory(h);
    }
    config.save();
}

// --- Estado de la ventana ----------------------------------------------------------

// FindReplaceDlg::updateMatchHighlighting (N++ 8.9.9): lo que coincide con «Buscar», en el documento
// activo, mientras el diálogo está abierto. Se limpia en las dos vistas: el documento que se ve ahora
// puede tener el resaltado de antes (los indicadores son del documento)
void MainWindow::updateFindDialogHighlight()
{
    for (EditorPane* pane : {_mainPane, _subPane}) {
        if (pane)
            clearFindMatches(*pane->editor());
    }
    if (!_findDialog || !_findDialog->highlightMatchesActive())
        return;
    if (isLargeFileIn(_activePane) && !_prefs.largeFileSmartHighlight) // allowSmartHilite
        return;
    highlightFindMatches(*editor(), _findDialog->options());
}

void MainWindow::refreshCurrentDocument()
{
    updatePanels();
    updateFindDialogHighlight();
    const Buffer* buf = _documents.buffer(currentBuffer());
    _statusBar->updateDocument(buf);
    _statusBar->updateCaret(editor());
    refreshWindowTitle();
    refreshMenuChecks();
}

void MainWindow::refreshWindowTitle()
{
    // Como N++: "*" si hay cambios, la ruta completa y el nombre de la aplicación
    const Buffer* buf = _documents.buffer(currentBuffer());
    const QString appName = QString::fromUtf8(kAppName);
    if (!buf) {
        setWindowTitle(appName);
        return;
    }
    const QString name = buf->isUntitled() ? buf->displayName() : QDir::toNativeSeparators(buf->filePath());
    setWindowTitle(QStringLiteral("%1%2 - %3").arg(buf->isDirty() ? QStringLiteral("*") : QString(), name, appName));
}

void MainWindow::refreshMenuChecks()
{
    const Buffer* buf = _documents.buffer(currentBuffer());
    if (!buf)
        return;

    QAction* checked = nullptr;
    const TextEncoding& enc = buf->encoding();
    if (enc.isUnicode()) {
        const auto it = _unicodeModeActions.find(static_cast<int>(enc.unicodeMode == UniMode::Ascii7 ? UniMode::Utf8 : enc.unicodeMode));
        checked = it == _unicodeModeActions.end() ? nullptr : it->second;
    } else {
        const auto it = _codepageActions.find(enc.codepage);
        checked = it == _codepageActions.end() ? nullptr : it->second;
    }
    if (checked)
        checked->setChecked(true);
    else if (QAction* current = _encodingGroup->checkedAction())
        current->setChecked(false);

    if (const auto it = _eolActions.find(static_cast<int>(buf->eol())); it != _eolActions.end())
        it->second->setChecked(true);

    if (const auto it = _languageActions.find(static_cast<int>(buf->language())); it != _languageActions.end())
        it->second->setChecked(true);
    else if (const auto ext = _externalLanguageActions.find(buf->userLanguage());
             buf->language() == LangType::External && ext != _externalLanguageActions.end())
        ext->second->setChecked(true);
    else if (QAction* current = _languageGroup->checkedAction())
        current->setChecked(false); // también con un lenguaje del usuario

    _readOnlyAction->setChecked(buf->isUserReadOnly());
    refreshTransferActions();
    if (QAction* pin = findChild<QAction*>(QStringLiteral("actionPinTab")))
        pin->setChecked(buf->isPinned());
    if (QAction* monitoring = findChild<QAction*>(QStringLiteral("actionMonitoring")))
        monitoring->setChecked(buf->isMonitoring());
}

} // namespace edita
