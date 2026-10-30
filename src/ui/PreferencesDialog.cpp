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


#include "ui/PreferencesDialog.h"

#include "core/Encoding.h"
#include "ui/Icons.h"
#include "ui/TerminalPanel.h"
#include "lsp/LspServers.h"
#include "ui/Translations.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QHeaderView>
#include <QTableWidget>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace edita {

namespace {

QCheckBox* addCheck(QFormLayout* form, const QString& text, const char* name)
{
    auto* box = new QCheckBox(text);
    box->setObjectName(QString::fromLatin1(name));
    form->addRow(box);
    return box;
}

QComboBox* addCombo(QFormLayout* form, const QString& label, const char* name,
                    std::initializer_list<std::pair<QString, int>> items)
{
    auto* combo = new QComboBox;
    combo->setObjectName(QString::fromLatin1(name));
    for (const auto& [text, value] : items)
        combo->addItem(text, value);
    form->addRow(label, combo);
    return combo;
}

QSpinBox* addSpin(QFormLayout* form, const QString& label, const char* name, int min, int max, const QString& suffix = {})
{
    auto* spin = new QSpinBox;
    spin->setObjectName(QString::fromLatin1(name));
    spin->setRange(min, max);
    spin->setSuffix(suffix);
    form->addRow(label, spin);
    return spin;
}

// Campo de ruta con botón "…" para elegir una carpeta
QLineEdit* addDirectory(QFormLayout* form, const QString& label, const char* name, QWidget* parent)
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* edit = new QLineEdit;
    edit->setObjectName(QString::fromLatin1(name));
    auto* browse = new QToolButton;
    browse->setText(QStringLiteral("…"));
    layout->addWidget(edit);
    layout->addWidget(browse);
    QObject::connect(browse, &QToolButton::clicked, parent, [edit, parent] {
        const QString dir = QFileDialog::getExistingDirectory(parent, QString(), edit->text());
        if (!dir.isEmpty())
            edit->setText(dir);
    });
    form->addRow(label, row);
    return edit;
}

void setComboValue(QComboBox* combo, int value)
{
    const int index = combo->findData(value);
    if (index >= 0)
        combo->setCurrentIndex(index);
}

int comboValue(const QComboBox* combo)
{
    return combo->currentData().toInt();
}

void fillCodepages(QComboBox* combo)
{
    for (const int cp : knownCodepages()) {
        if (cp == kCodepageUtf8)
            continue;
        combo->addItem(QString::fromStdString(encodingDisplayName(TextEncoding::fromCodepage(cp))), cp);
    }
}

} // namespace

PreferencesDialog::PreferencesDialog(const Preferences& prefs, std::vector<std::pair<int, QString>> languages,
                                     QWidget* parent)
    : QDialog(parent)
    , _categories(new QListWidget(this))
    , _pages(new QStackedWidget(this))
    , _languages(std::move(languages))
{
    setWindowTitle(tr("Preferences"));
    _categories->setObjectName(QStringLiteral("categories"));

    buildGeneral();
    buildEditing();
    buildMargins();
    buildNewDocument();
    buildRecentFiles();
    buildIndentation();
    buildAutoCompletion();
    buildHighlighting();
    buildSearching();
    buildPrinting();
    buildBackup();
    buildMisc();
    buildLanguageServers();

    connect(_categories, &QListWidget::currentRowChanged, _pages, &QStackedWidget::setCurrentIndex);
    _categories->setCurrentRow(0);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    buttons->button(QDialogButtonBox::Apply)->setText(tr("Apply"));
    buttons->button(QDialogButtonBox::Apply)->setObjectName(QStringLiteral("apply"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            [this] { emit applyRequested(preferences()); });

    auto* top = new QHBoxLayout;
    top->addWidget(_categories);
    top->addWidget(_pages, 1);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(buttons);

    setPreferences(prefs);
    for (QCheckBox* box : findChildren<QCheckBox*>())
        connect(box, &QCheckBox::toggled, this, &PreferencesDialog::updateEnabledState);
    for (QComboBox* combo : findChildren<QComboBox*>())
        connect(combo, &QComboBox::currentIndexChanged, this, &PreferencesDialog::updateEnabledState);
    updateEnabledState();

    // La lista de páginas, tan ancha y alta como sus nombres con su icono, sin cortar
    // ni barras de desplazamiento (medida con la hoja de estilos ya aplicada)
    _categories->setIconSize(QSize(20, 20));
    _categories->setTextElideMode(Qt::ElideNone);
    _categories->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _categories->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _categories->ensurePolished();
    int listWidth = 0;
    int listHeight = 0;
    for (int i = 0; i < _categories->count(); ++i) {
        const QSize item = _categories->sizeHintForIndex(_categories->model()->index(i, 0));
        listWidth = std::max(listWidth, item.width());
        listHeight += item.height() + 2 * _categories->spacing();
    }
    const int chrome = 2 * _categories->frameWidth() + 24; // relleno de la lista y margen
    _categories->setFixedWidth(listWidth + chrome);
    _categories->setMinimumHeight(listHeight + chrome);

    // El tamaño que pide la página más grande, sin pasar de la pantalla
    QSize size = sizeHint().expandedTo(QSize(760, 520));
    if (const QScreen* screen = parent ? parent->screen() : QGuiApplication::primaryScreen())
        size = size.boundedTo(screen->availableSize() * 9 / 10);
    resize(size);
}

QWidget* PreferencesDialog::addPage(const QString& name, const QString& title)
{
    auto* page = new QWidget;
    page->setObjectName(name);
    _pages->addWidget(page);
    auto* item = new QListWidgetItem(title, _categories);
    item->setData(Qt::UserRole, name);
    // Un icono por página, de los de la interfaz
    static const std::pair<const char*, const char*> kPageIcons[] = {
        {"general", "general"},     {"editing", "editing"},           {"margins", "margins"},
        {"newDocument", "new"},     {"recentFiles", "recent"},        {"indentation", "tabs"},
        {"autoCompletion", "autocompletion"}, {"highlighting", "highlighting"}, {"searching", "find"},
        {"printing", "print"},      {"backup", "backup"},             {"misc", "misc"},
    };
    for (const auto& [pageName, icon] : kPageIcons) {
        if (name == QLatin1String(pageName))
            item->setIcon(icons::icon(QString::fromLatin1(icon)));
    }
    return page;
}

void PreferencesDialog::showPage(const QString& name)
{
    for (int i = 0; i < _pages->count(); ++i) {
        if (_pages->widget(i)->objectName() == name)
            _categories->setCurrentRow(i);
    }
}

QStringList PreferencesDialog::pageNames() const
{
    QStringList names;
    for (int i = 0; i < _pages->count(); ++i)
        names << _pages->widget(i)->objectName();
    return names;
}

// --- Páginas ---------------------------------------------------------------------------

void PreferencesDialog::buildGeneral()
{
    auto* form = new QFormLayout(addPage(QStringLiteral("general"), tr("General")));
    // Localization de N++: el idioma se aplica al reiniciar
    _language = new QComboBox;
    _language->setObjectName(QStringLiteral("language"));
    _language->addItem(tr("System language"), QString());
    for (const i18n::Language& language : i18n::availableLanguages())
        _language->addItem(language.nativeName, language.code);
    form->addRow(tr("Localization:"), _language);
    // Modo oscuro de N++ (con "seguir a Windows"): aquí, seguir al sistema
    _appearance = addCombo(form, tr("Appearance:"), "appearance",
                           {{tr("Follow the system"), 0}, {tr("Light"), 1}, {tr("Dark"), 2}});
    // Multi-Instance de N++ (la sesión en una instancia nueva no aplica: como una sola)
    _multiInst = addCombo(form, tr("Instances:"), "multiInst",
                          {{tr("Open files in the running window"), 0}, {tr("Always open a new window"), 2}});
    _toolbar = addCombo(form, tr("Toolbar:"), "toolbar",
                        {{tr("Hide"), -1}, {tr("Small icons"), 0}, {tr("Standard icons"), 1}, {tr("Large icons"), 2}});
    _qtFileDialog = addCheck(form, tr("Use Edita's file dialogs (in the language of Edita, not of the desktop)"),
                             "qtFileDialog");
    _statusBar = addCheck(form, tr("Show status bar"), "statusBarShow");
    _showWelcome = addCheck(form, tr("Show the welcome page when there are no documents"), "showWelcome");
    form->addRow(new QLabel(tr("<b>Tab bar</b>")));
    _tabBarHide = addCheck(form, tr("Hide"), "tabBarHide");
    _tabCloseButton = addCheck(form, tr("Show close button on each tab"), "tabCloseButton");
    _tabDoubleClick = addCheck(form, tr("Double click to close document"), "tabDoubleClickCloses");
    _fileTypeIcons = addCheck(form, tr("File type icons (tabs, Document List, Folder as Workspace, Go to File)"), "fileTypeIcons");
    _quitOnEmpty = addCheck(form, tr("Exit on close the last tab"), "quitOnEmpty");
}

void PreferencesDialog::buildEditing()
{
    auto* form = new QFormLayout(addPage(QStringLiteral("editing"), tr("Editing")));
    _currentLine = addCombo(form, tr("Current line:"), "currentLineMode",
                            {{tr("None"), 0}, {tr("Highlight background"), 1}, {tr("Frame"), 2}});
    _frameWidth = addSpin(form, tr("Frame width:"), "currentLineFrameWidth", 1, 6, tr(" px"));
    _caretWidth = addCombo(form, tr("Caret:"), "caretWidth",
                           {{tr("Block"), 0}, {tr("1 px"), 1}, {tr("2 px"), 2}, {tr("3 px"), 3}});
    _blinkRate = addSpin(form, tr("Blink rate:"), "caretBlinkRate", 0, 2000, tr(" ms"));
    _blinkRate->setSpecialValueText(tr("No blink"));
    _blinkRate->setSingleStep(50);
    _multiSelection = addCheck(form, tr("Multi-Editing (Ctrl+click or column selection)"), "multiSelection");
    _virtualSpace = addCheck(form, tr("Virtual space"), "virtualSpace");
    _scrollBeyond = addCheck(form, tr("Scroll beyond last line"), "scrollBeyondLastLine");
    _lineCopyCut = addCheck(form, tr("Copy or cut the entire line when there is no selection"), "lineCopyCutWithoutSelection");
    _smoothFont = addCheck(form, tr("LCD font smoothing"), "smoothFont");
    _systemMonospace = addCheck(form, tr("Use the system monospace font instead of the Windows fonts of the themes "
                                         "(Courier New, Consolas, Lucida Console)"),
                                "systemMonospaceFont");
    form->addRow(new QLabel(tr("<b>Show</b>")));
    _whiteSpace = addCheck(form, tr("Space and TAB"), "whiteSpaceShow");
    _eol = addCheck(form, tr("End of Line"), "eolShow");
    _indentGuides = addCheck(form, tr("Indent guides"), "indentGuideLine");
    _activeIndentGuide = addCheck(form, tr("Highlight the indent guide of the current block"), "activeIndentGuide");
    _colorPreview = addCheck(form, tr("Color preview (#rrggbb, rgb(), hsl() in CSS, HTML, JavaScript, Python…)"), "colorPreview");
    _colorPreview->setToolTip(tr("Ctrl+click a color to change it with the color picker."));
    _rainbowBrackets = addCheck(form, tr("Color brackets by nesting level"), "rainbowBrackets");
    _breadcrumbs = addCheck(form, tr("Breadcrumbs above the editor (folder › file › class › function)"), "breadcrumbs");
    _stickyScroll = addCheck(form, tr("Sticky scroll (keep the first line of the current block at the top)"), "stickyScroll");
    form->addRow(new QLabel(tr("<b>Line wrap</b>")));
    _wrap = addCheck(form, tr("Wrap long lines"), "wrap");
    _wrapIndent = addCombo(form, tr("Wrap indent:"), "wrapIndent",
                           {{tr("Default"), 0}, {tr("Aligned"), 1}, {tr("Indent"), 2}});
    _wrapSymbol = addCheck(form, tr("Wrap symbol at the end"), "wrapSymbolShow");
}

void PreferencesDialog::buildMargins()
{
    auto* form = new QFormLayout(addPage(QStringLiteral("margins"), tr("Margins")));
    _lineNumbers = addCheck(form, tr("Line numbers"), "lineNumberMargin");
    _dynamicWidth = addCheck(form, tr("Dynamic width (based on visible lines)"), "lineNumberDynamicWidth");
    _folderStyle = addCombo(form, tr("Folder:"), "folderStyle",
                            {{tr("Simple"), 1}, {tr("Arrow"), 2}, {tr("Circle tree"), 3}, {tr("Box tree"), 4}, {tr("No margin"), 5}});
    _edgeColumns = new QLineEdit;
    _edgeColumns->setObjectName(QStringLiteral("edgeColumns"));
    _edgeColumns->setPlaceholderText(tr("e.g. 80 or 80 120"));
    form->addRow(tr("Vertical edge at columns:"), _edgeColumns);
    _edgeBackground = addCheck(form, tr("Background mode past the column (single column only)"), "edgeBackground");
    _bookmarkMargin = addCheck(form, tr("Bookmark margin"), "bookmarkMargin");
    _changeHistory = addCombo(form, tr("Change history:"), "changeHistory",
                              {{tr("Disabled"), 0}, {tr("Show in margin"), 1}, {tr("Show as indicators in text"), 2},
                               {tr("Show in margin and text"), 3}});
    _currentLineNumber = addCheck(form, tr("Highlight the current line number"), "currentLineNumber");
    _gitMargin = addCheck(form, tr("Changes since the last Git commit"), "gitMargin");
    _gitMargin->setToolTip(tr("Green: added lines, blue: modified lines, red: deleted lines. Hover a mark to see the "
                              "original lines."));
    _gitExplorer = addCheck(form, tr("Git status of the files in Folder as Workspace"), "gitExplorer");
    _gitBlameInline = addCheck(form, tr("Who changed the current line (Git blame), at the end of the line"), "gitBlameInline");
    _scrollBarMarks = addCheck(form, tr("Marks in the scroll bar (Git, bookmarks, Mark All, smart highlighting, caret)"),
                               "scrollBarMarks");
}

void PreferencesDialog::buildNewDocument()
{
    auto* form = new QFormLayout(addPage(QStringLiteral("newDocument"), tr("New Document")));
    _newEol = addCombo(form, tr("End of line:"), "newDocEol",
                       {{tr("Windows (CR LF)"), 0}, {tr("Macintosh (CR)"), 1}, {tr("Unix (LF)"), 2}});
    _newEncoding = addCombo(form, tr("Encoding:"), "newDocEncoding",
                            {{QStringLiteral("UTF-8"), static_cast<int>(UniMode::Utf8)},
                             {QStringLiteral("UTF-8 BOM"), static_cast<int>(UniMode::Utf8Bom)},
                             {QStringLiteral("UTF-16 BE BOM"), static_cast<int>(UniMode::Utf16BeBom)},
                             {QStringLiteral("UTF-16 LE BOM"), static_cast<int>(UniMode::Utf16LeBom)},
                             {tr("Character set:"), static_cast<int>(UniMode::Ansi)}});
    _newCodepage = new QComboBox;
    _newCodepage->setObjectName(QStringLiteral("newDocCodepage"));
    fillCodepages(_newCodepage);
    form->addRow(QString(), _newCodepage);
    _newLanguage = new QComboBox;
    _newLanguage->setObjectName(QStringLiteral("newDocLanguage"));
    for (const auto& [lang, name] : _languages)
        _newLanguage->addItem(name, lang);
    form->addRow(tr("Language:"), _newLanguage);

    form->addRow(new QLabel(tr("<b>Default Directory (Open/Save)</b>")));
    _openSaveDir = addCombo(form, tr("Start in:"), "openSaveDir",
                            {{tr("Follow current document"), 0}, {tr("Remember last used directory"), 1}, {tr("This one:"), 2}});
    _defaultDir = addDirectory(form, QString(), "defaultDirPath", this);
}

void PreferencesDialog::buildRecentFiles()
{
    auto* form = new QFormLayout(addPage(QStringLiteral("recentFiles"), tr("Recent Files History")));
    _recentMax = addSpin(form, tr("Maximum entries:"), "recentFilesMax", 0, 30);
    _recentSubMenu = addCheck(form, tr("In submenu"), "recentFilesInSubMenu");
    _recentLengthMode = addCombo(form, tr("Display:"), "recentFilesLengthMode",
                                 {{tr("Full file name path"), -1}, {tr("Only file name"), 0}, {tr("Customize maximum length:"), 1}});
    _recentLength = addSpin(form, QString(), "recentFilesCustomLength", 1, 260, tr(" characters"));
}

void PreferencesDialog::buildIndentation()
{
    auto* form = new QFormLayout(addPage(QStringLiteral("indentation"), tr("Indentation")));
    _tabSize = addSpin(form, tr("Tab size:"), "tabSize", 1, 64);
    _tabSpaces = addCheck(form, tr("Replace tabs by spaces"), "tabReplacedBySpace");
    _backspaceUnindent = addCheck(form, tr("Backspace key unindents"), "backspaceUnindent");
    _autoIndent = addCombo(form, tr("Auto-indent:"), "autoIndent",
                           {{tr("Default"), 0}, {tr("Basic (like the previous line)"), 1}, {tr("Advanced (braces, Python)"), 2}});
    auto* note = new QLabel(tr("Languages with their own tab settings in langs.xml keep them."));
    note->setWordWrap(true);
    form->addRow(note);
}

void PreferencesDialog::buildAutoCompletion()
{
    auto* form = new QFormLayout(addPage(QStringLiteral("autoCompletion"), tr("Auto-Completion")));
    _autocStatus = addCombo(form, tr("Show the list:"), "autocStatus",
                            {{tr("No"), 0}, {tr("Function completion (language API)"), 1}, {tr("Word completion (document words)"), 2},
                             {tr("Function and word completion"), 3}});
    _autocFromLen = addSpin(form, tr("From the:"), "autocFromLen", 1, 9, tr(" characters"));
    _autocIgnoreNumbers = addCheck(form, tr("Ignore numbers"), "autocIgnoreNumbers");
    _autocBrief = addCheck(form, tr("Brief list (only matching prefixes)"), "autocBrief");
    _snippetsTab = addCheck(form, tr("Tab after the prefix of a snippet inserts it"), "snippetsTab");
    _funcParams = addCheck(form, tr("Function parameters hint on input \"(\""), "funcParams");
    form->addRow(new QLabel(tr("<b>Auto-Insert</b>")));
    _insertParentheses = addCheck(form, QStringLiteral("( )"), "insertParentheses");
    _insertBrackets = addCheck(form, QStringLiteral("[ ]"), "insertBrackets");
    _insertCurly = addCheck(form, QStringLiteral("{ }"), "insertCurlyBrackets");
    _insertQuotes = addCheck(form, QStringLiteral("' '"), "insertQuotes");
    _insertDoubleQuotes = addCheck(form, QStringLiteral("\" \""), "insertDoubleQuotes");
    _insertTag = addCheck(form, tr("HTML/XML close tag"), "insertHtmlXmlTag");
}

void PreferencesDialog::buildHighlighting()
{
    auto* form = new QFormLayout(addPage(QStringLiteral("highlighting"), tr("Highlighting")));
    _smart = addCheck(form, tr("Smart highlighting: mark the occurrences of the selection"), "smartHighlight");
    _smartCase = addCheck(form, tr("Match case"), "smartHighlightMatchCase");
    _smartWord = addCheck(form, tr("Match whole word only"), "smartHighlightWholeWord");
    _smartFind = addCheck(form, tr("Use Find dialog settings"), "smartHighlightUseFindSettings");
    _smartOther = addCheck(form, tr("Highlight another view"), "smartHighlightOtherView");
    _tagsMatch = addCheck(form, tr("Highlight matching HTML/XML tags"), "tagsMatchHighlight");
    _tagAttributes = addCheck(form, tr("Highlight tag attributes"), "tagAttributesHighlight");
    connect(_tagsMatch, &QCheckBox::toggled, _tagAttributes, &QWidget::setEnabled);
    _markAllCase = addCheck(form, tr("Token style: match case"), "markAllMatchCase");
    _markAllWord = addCheck(form, tr("Token style: match whole word only"), "markAllWholeWord");
}

void PreferencesDialog::buildSearching()
{
    auto* form = new QFormLayout(addPage(QStringLiteral("searching"), tr("Searching")));
    _inlineFind = addCheck(form, tr("Ctrl+F and Ctrl+H open the inline find bar (the dialog stays in its \"⋯\" button)"),
                           "inlineFind");
    _fillFind = addCheck(form, tr("Fill Find field with selected text"), "fillFindFieldWithSelected");
    _confirmReplaceAll = addCheck(form, tr("Confirm \"Replace All in All Opened Documents\""),
                                  "confirmReplaceInAllOpenDocs");
    _monoFind = addCheck(form, tr("Use monospaced font in Find dialog"), "monospacedFontFindDlg");
}

void PreferencesDialog::buildPrinting()
{
    auto* form = new QFormLayout(addPage(QStringLiteral("printing"), tr("Print")));
    _printLineNumber = addCheck(form, tr("Print line number"), "printLineNumber");
    _printColour = addCombo(form, tr("Colors:"), "printColourMode",
                            {{tr("WYSIWYG"), 0}, {tr("Invert"), 1}, {tr("Black on white"), 2},
                             {tr("No background color"), 3}});
    _printFormFeed = addCheck(form, tr("New page on each form feed (\\f)"), "printFormFeedPageBreak");

    auto* margins = new QWidget;
    auto* marginsLayout = new QHBoxLayout(margins);
    marginsLayout->setContentsMargins(0, 0, 0, 0);
    const char* marginNames[] = {"printMarginLeft", "printMarginTop", "printMarginRight", "printMarginBottom"};
    const QString marginLabels[] = {tr("Left"), tr("Top"), tr("Right"), tr("Bottom")};
    for (int i = 0; i < 4; ++i) {
        _printMargins[i] = new QSpinBox;
        _printMargins[i]->setObjectName(QString::fromLatin1(marginNames[i]));
        _printMargins[i]->setRange(0, 200);
        _printMargins[i]->setSuffix(tr(" mm"));
        marginsLayout->addWidget(new QLabel(marginLabels[i]));
        marginsLayout->addWidget(_printMargins[i]);
    }
    form->addRow(tr("Margins:"), margins);

    auto fontRow = [&](const QString& label, const char* prefix, QComboBox*& family, QSpinBox*& size, QComboBox*& style) {
        auto* row = new QWidget;
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        family = new QComboBox;
        family->setObjectName(QStringLiteral("%1FontName").arg(QString::fromLatin1(prefix)));
        family->addItem(tr("(default)"), QString());
        for (const QString& name : QFontDatabase::families())
            family->addItem(name, name);
        size = new QSpinBox;
        size->setObjectName(QStringLiteral("%1FontSize").arg(QString::fromLatin1(prefix)));
        size->setRange(0, 72);
        size->setSpecialValueText(tr("9 (default)"));
        style = new QComboBox;
        style->setObjectName(QStringLiteral("%1FontStyle").arg(QString::fromLatin1(prefix)));
        style->addItem(tr("Normal"), 0);
        style->addItem(tr("Bold"), 1);
        style->addItem(tr("Italic"), 2);
        style->addItem(tr("Bold italic"), 3);
        layout->addWidget(family, 1);
        layout->addWidget(size);
        layout->addWidget(style);
        form->addRow(label, row);
    };
    auto bandRow = [&](const QString& label, const char* prefix, QLineEdit* (&edits)[3]) {
        const char* parts[] = {"Left", "Middle", "Right"};
        const QString placeholders[] = {tr("left"), tr("center"), tr("right")};
        auto* row = new QWidget;
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        for (int i = 0; i < 3; ++i) {
            edits[i] = new QLineEdit;
            edits[i]->setObjectName(QStringLiteral("%1%2").arg(QString::fromLatin1(prefix), QString::fromLatin1(parts[i])));
            edits[i]->setPlaceholderText(placeholders[i]);
            layout->addWidget(edits[i]);
        }
        form->addRow(label, row);
    };
    form->addRow(new QLabel(tr("<b>Header and Footer</b>")));
    bandRow(tr("Header:"), "printHeader", _printHeader);
    fontRow(tr("Font:"), "printHeader", _printHeaderFont, _printHeaderSize, _printHeaderStyle);
    bandRow(tr("Footer:"), "printFooter", _printFooter);
    fontRow(tr("Font:"), "printFooter", _printFooterFont, _printFooterSize, _printFooterStyle);
    auto* help = new QLabel(tr("Variables: $(FULL_CURRENT_PATH) $(FILE_NAME) $(NAME_PART) $(EXT_PART) "
                               "$(CURRENT_DIRECTORY) $(SHORT_DATE) $(LONG_DATE) $(TIME) $(CURRENT_PRINTING_PAGE)"));
    help->setWordWrap(true);
    help->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(help);
}

void PreferencesDialog::buildBackup()
{
    auto* form = new QFormLayout(addPage(QStringLiteral("backup"), tr("Backup")));
    _remember = addCheck(form, tr("Remember current session for next launch"), "rememberLastSession");
    _snapshot = addCheck(form, tr("Periodic backup of unsaved changes (snapshot mode)"), "snapshotMode");
    _snapshotSeconds = addSpin(form, tr("Every:"), "snapshotSeconds", 1, 3600, tr(" s"));
    _backupOnSave = addCombo(form, tr("On save, back up the previous version:"), "backupOnSave",
                             {{tr("No"), 0}, {tr("file.bak"), 1}, {tr("Verbose, in nppBackup/"), 2}});
    _customBackupDir = addCheck(form, tr("In this folder instead of the file's folder:"), "useCustomBackupDir");
    _backupDir = addDirectory(form, QString(), "backupDir", this);
    _sessionExt = new QLineEdit;
    _sessionExt->setObjectName(QStringLiteral("sessionExt"));
    _sessionExt->setPlaceholderText(QStringLiteral("edita-session"));
    _sessionExt->setToolTip(tr("Files with this extension are opened as sessions (File › Load Session…)"));
    form->addRow(tr("Session file extension:"), _sessionExt);
}

void PreferencesDialog::buildMisc()
{
    auto* form = new QFormLayout(addPage(QStringLiteral("misc"), tr("MISC.")));
    form->addRow(new QLabel(tr("<b>File Status Auto-Detection</b>")));
    _detect = addCheck(form, tr("Enable"), "fileAutoDetection");
    _detectAll = addCheck(form, tr("For all opened files (not only the visible one)"), "detectAllFiles");
    _autoUpdate = addCheck(form, tr("Update silently if there are no unsaved changes"), "autoUpdate");
    _goToEnd = addCheck(form, tr("Scroll to the last line after update"), "goToEnd");
    form->addRow(new QLabel(tr("<b>Encoding</b>")));
    _detectEncoding = addCheck(form, tr("Autodetect character encoding on open"), "detectEncoding");
    _fallbackCodepage = new QComboBox;
    _fallbackCodepage->setObjectName(QStringLiteral("fallbackCodepage"));
    fillCodepages(_fallbackCodepage);
    form->addRow(tr("Default 8-bit character set:"), _fallbackCodepage);
    form->addRow(new QLabel(tr("<b>Terminal</b>")));
    _terminalShell = new QLineEdit;
    _terminalShell->setObjectName(QStringLiteral("terminalShell"));
    _terminalShell->setPlaceholderText(TerminalPanel::resolveShell(QString()));
    _terminalShell->setToolTip(tr("Program and arguments, e.g. \"zsh -l\". Empty: the login shell ($SHELL)"));
    form->addRow(tr("Shell:"), _terminalShell);
}

// Servidores de lenguaje (no existe en N++): cuál usa cada lenguaje y si está instalado
void PreferencesDialog::buildLanguageServers()
{
    QWidget* page = addPage(QStringLiteral("languageServers"), tr("Language Servers"));
    auto* layout = new QVBoxLayout(page);
    _lspEnabled = new QCheckBox(tr("Use language servers (diagnostics, completion, information on hover…)"));
    _lspEnabled->setObjectName(QStringLiteral("lspEnabled"));
    layout->addWidget(_lspEnabled);
    _inlayHints = new QCheckBox(tr("Hints (types, parameter names) at the end of the lines"));
    _inlayHints->setObjectName(QStringLiteral("inlayHints"));
    layout->addWidget(_inlayHints);
    auto* hint = new QLabel(tr("Each language uses the first of its known servers that is installed. Write another "
                               "command to use it instead, or «off» to disable that language."));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    _lspServers = new QTableWidget(0, 3);
    _lspServers->setObjectName(QStringLiteral("lspServers"));
    _lspServers->setHorizontalHeaderLabels({tr("Languages"), tr("Command"), tr("Found")});
    _lspServers->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _lspServers->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    _lspServers->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _lspServers->verticalHeader()->hide();
    _lspServers->setSelectionMode(QAbstractItemView::NoSelection);
    for (const lsp::ServerSpec& spec : lsp::knownServers()) {
        const int row = _lspServers->rowCount();
        _lspServers->insertRow(row);
        auto* languages = new QTableWidgetItem(spec.languages);
        languages->setFlags(Qt::ItemIsEnabled);
        languages->setData(Qt::UserRole, spec.id);
        _lspServers->setItem(row, 0, languages);
        auto* command = new QTableWidgetItem;
        command->setToolTip(tr("Empty: %1").arg(spec.candidates.join(QStringLiteral(" | "))));
        _lspServers->setItem(row, 1, command);
        auto* found = new QTableWidgetItem;
        found->setFlags(Qt::ItemIsEnabled);
        _lspServers->setItem(row, 2, found);
    }
    // Al cambiar una orden se vuelve a buscar
    connect(_lspServers, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item->column() != 1)
            return;
        const QString id = _lspServers->item(item->row(), 0)->data(Qt::UserRole).toString();
        const lsp::ServerSpec* spec = nullptr;
        for (const lsp::ServerSpec& s : lsp::knownServers())
            spec = s.id == id ? &s : spec;
        QTableWidgetItem* found = _lspServers->item(item->row(), 2);
        if (!spec || !found)
            return;
        const auto server = lsp::resolveServer(*spec, {{id, item->text()}});
        const QSignalBlocker blocker(_lspServers);
        found->setText(server ? QStringLiteral("✓ ") + server->program
                              : (item->text().trimmed().compare(QLatin1String("off"), Qt::CaseInsensitive) == 0 ? tr("Disabled")
                                                                                                                : tr("Not installed")));
        if (item->text().trimmed().isEmpty() && server)
            item->setData(Qt::ToolTipRole, server->command);
    });
    layout->addWidget(_lspServers, 1);
}

void PreferencesDialog::updateEnabledState()
{
    _frameWidth->setEnabled(comboValue(_currentLine) == static_cast<int>(CurrentLineMode::Frame));
    _dynamicWidth->setEnabled(_lineNumbers->isChecked());
    _wrapIndent->setEnabled(_wrap->isChecked());
    _wrapSymbol->setEnabled(_wrap->isChecked());
    _newCodepage->setEnabled(comboValue(_newEncoding) == static_cast<int>(UniMode::Ansi));
    _defaultDir->parentWidget()->setEnabled(comboValue(_openSaveDir) == static_cast<int>(OpenSaveDir::Custom));
    _recentLength->setEnabled(comboValue(_recentLengthMode) > 0);
    for (QCheckBox* box : {_smartCase, _smartWord, _smartFind, _smartOther})
        box->setEnabled(_smart->isChecked());
    _smartCase->setEnabled(_smart->isChecked() && !_smartFind->isChecked());
    _smartWord->setEnabled(_smart->isChecked() && !_smartFind->isChecked());
    _snapshot->setEnabled(_remember->isChecked());
    _snapshotSeconds->setEnabled(_remember->isChecked() && _snapshot->isChecked());
    _customBackupDir->setEnabled(comboValue(_backupOnSave) != 0);
    _backupDir->parentWidget()->setEnabled(comboValue(_backupOnSave) != 0 && _customBackupDir->isChecked());
    for (QWidget* w : std::initializer_list<QWidget*>{_autocFromLen, _autocIgnoreNumbers, _autocBrief})
        w->setEnabled(comboValue(_autocStatus) != 0);
    for (QCheckBox* box : {_detectAll, _autoUpdate, _goToEnd})
        box->setEnabled(_detect->isChecked());
    _fallbackCodepage->setEnabled(true);
}

// --- Datos ------------------------------------------------------------------------------

void PreferencesDialog::setPreferences(const Preferences& p)
{
    _base = p;
    _statusBar->setChecked(p.statusBarShow);
    _tabBarHide->setChecked(p.tabBarHide);
    _showWelcome->setChecked(p.showWelcome);
    _tabCloseButton->setChecked(p.tabCloseButton);
    _fileTypeIcons->setChecked(p.fileTypeIcons);
    _tabDoubleClick->setChecked(p.tabDoubleClickCloses);
    _quitOnEmpty->setChecked(p.quitOnEmpty);
    _language->setCurrentIndex(std::max(0, _language->findData(p.language)));
    _qtFileDialog->setChecked(p.qtFileDialog);

    setComboValue(_currentLine, static_cast<int>(p.currentLineMode));
    _frameWidth->setValue(p.currentLineFrameWidth);
    setComboValue(_caretWidth, p.caretWidth);
    _blinkRate->setValue(p.caretBlinkRate);
    _multiSelection->setChecked(p.multiSelection);
    _virtualSpace->setChecked(p.virtualSpace);
    _scrollBeyond->setChecked(p.scrollBeyondLastLine);
    _lineCopyCut->setChecked(p.lineCopyCutWithoutSelection);
    _smoothFont->setChecked(p.smoothFont);
    _systemMonospace->setChecked(p.systemMonospaceFont);
    setComboValue(_appearance, p.darkModeFollowSystem ? 0 : p.darkModeEnabled ? 2 : 1);
    setComboValue(_multiInst, p.multiInstSetting == MultiInst::Always ? 2 : 0);
    setComboValue(_toolbar, p.toolbarVisible ? static_cast<int>(p.toolbarIcons) : -1);
    _whiteSpace->setChecked(p.whiteSpaceShow);
    _eol->setChecked(p.eolShow);
    _indentGuides->setChecked(p.indentGuideLine);
    _activeIndentGuide->setChecked(p.activeIndentGuide);
    _colorPreview->setChecked(p.colorPreview);
    _rainbowBrackets->setChecked(p.rainbowBrackets);
    _breadcrumbs->setChecked(p.breadcrumbs);
    _stickyScroll->setChecked(p.stickyScroll);
    _wrap->setChecked(p.wrap);
    setComboValue(_wrapIndent, static_cast<int>(p.wrapIndent));
    _wrapSymbol->setChecked(p.wrapSymbolShow);

    _lineNumbers->setChecked(p.lineNumberMargin);
    _dynamicWidth->setChecked(p.lineNumberDynamicWidth);
    setComboValue(_folderStyle, static_cast<int>(p.folderStyle));
    _edgeColumns->setText(edgeColumnsToText(p.edgeColumns));
    _edgeBackground->setChecked(p.edgeBackground);
    _bookmarkMargin->setChecked(p.bookmarkMargin);
    _gitMargin->setChecked(p.gitMargin);
    _gitExplorer->setChecked(p.gitExplorer);
    _snippetsTab->setChecked(p.snippetsTab);
    _gitBlameInline->setChecked(p.gitBlameInline);
    _currentLineNumber->setChecked(p.currentLineNumber);
    _scrollBarMarks->setChecked(p.scrollBarMarks);
    setComboValue(_changeHistory, p.changeHistory);

    setComboValue(_newEol, p.newDocEol);
    setComboValue(_newEncoding, p.newDocUnicodeMode == static_cast<int>(UniMode::Ascii7) ? static_cast<int>(UniMode::Utf8)
                                                                                   : p.newDocUnicodeMode);
    setComboValue(_newCodepage, p.newDocCodepage > 0 ? p.newDocCodepage : 1252);
    setComboValue(_newLanguage, p.newDocLanguage);
    setComboValue(_openSaveDir, static_cast<int>(p.openSaveDir));
    _defaultDir->setText(p.defaultDirPath);

    _recentMax->setValue(p.recentFilesMax);
    _recentSubMenu->setChecked(p.recentFilesInSubMenu);
    setComboValue(_recentLengthMode, p.recentFilesCustomLength < 0 ? -1 : (p.recentFilesCustomLength == 0 ? 0 : 1));
    _recentLength->setValue(p.recentFilesCustomLength > 0 ? p.recentFilesCustomLength : 100);

    _tabSize->setValue(p.tabSize);
    _tabSpaces->setChecked(p.tabReplacedBySpace);
    _backspaceUnindent->setChecked(p.backspaceUnindent);
    setComboValue(_autoIndent, static_cast<int>(p.autoIndent));

    setComboValue(_autocStatus, p.autocStatus);
    _autocFromLen->setValue(p.autocFromLen);
    _autocIgnoreNumbers->setChecked(p.autocIgnoreNumbers);
    _autocBrief->setChecked(p.autocBrief);
    _funcParams->setChecked(p.funcParams);
    _insertParentheses->setChecked(p.insertParentheses);
    _insertBrackets->setChecked(p.insertBrackets);
    _insertCurly->setChecked(p.insertCurlyBrackets);
    _insertQuotes->setChecked(p.insertQuotes);
    _insertDoubleQuotes->setChecked(p.insertDoubleQuotes);
    _insertTag->setChecked(p.insertHtmlXmlTag);

    _smart->setChecked(p.smartHighlight);
    _smartCase->setChecked(p.smartHighlightMatchCase);
    _smartWord->setChecked(p.smartHighlightWholeWord);
    _smartFind->setChecked(p.smartHighlightUseFindSettings);
    _smartOther->setChecked(p.smartHighlightOtherView);
    _tagsMatch->setChecked(p.tagsMatchHighlight);
    _tagAttributes->setChecked(p.tagAttributesHighlight);
    _tagAttributes->setEnabled(p.tagsMatchHighlight);
    _markAllCase->setChecked(p.markAllMatchCase);
    _markAllWord->setChecked(p.markAllWholeWord);

    _fillFind->setChecked(p.fillFindFieldWithSelected);
    _inlineFind->setChecked(p.inlineFind);
    _confirmReplaceAll->setChecked(p.confirmReplaceInAllOpenDocs);
    _monoFind->setChecked(p.monospacedFontFindDlg);

    _printLineNumber->setChecked(p.printLineNumber);
    _printFormFeed->setChecked(p.printFormFeedPageBreak);
    setComboValue(_printColour, p.printColourMode);
    const int margins[] = {p.printMarginLeft, p.printMarginTop, p.printMarginRight, p.printMarginBottom};
    for (int i = 0; i < 4; ++i)
        _printMargins[i]->setValue(margins[i]);
    const QString header[] = {p.printHeaderLeft, p.printHeaderMiddle, p.printHeaderRight};
    const QString footer[] = {p.printFooterLeft, p.printFooterMiddle, p.printFooterRight};
    for (int i = 0; i < 3; ++i) {
        _printHeader[i]->setText(header[i]);
        _printFooter[i]->setText(footer[i]);
    }
    auto setFamily = [](QComboBox* combo, const QString& family) {
        int index = combo->findData(family);
        if (index < 0 && !family.isEmpty()) { // una fuente que no hay en este sistema se conserva
            combo->addItem(family, family);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(std::max(index, 0));
    };
    setFamily(_printHeaderFont, p.printHeaderFontName);
    setFamily(_printFooterFont, p.printFooterFontName);
    _printHeaderSize->setValue(p.printHeaderFontSize);
    _printFooterSize->setValue(p.printFooterFontSize);
    setComboValue(_printHeaderStyle, p.printHeaderFontStyle);
    setComboValue(_printFooterStyle, p.printFooterFontStyle);

    _remember->setChecked(p.rememberLastSession);
    _snapshot->setChecked(p.snapshotMode);
    _snapshotSeconds->setValue(std::max(1, p.snapshotBackupTiming / 1000));
    setComboValue(_backupOnSave, static_cast<int>(p.backupOnSave));
    _customBackupDir->setChecked(p.useCustomBackupDir);
    _backupDir->setText(p.backupDir);
    _sessionExt->setText(p.sessionExt);

    _detect->setChecked(p.detectsChanges());
    _detectAll->setChecked(p.detectsAllFiles());
    _autoUpdate->setChecked(p.autoUpdates());
    _goToEnd->setChecked(p.goesToEnd());
    _detectEncoding->setChecked(p.detectEncoding);
    setComboValue(_fallbackCodepage, p.fallbackCodepage);
    _terminalShell->setText(p.terminalShell);
    _lspEnabled->setChecked(p.lspEnabled);
    _inlayHints->setChecked(p.inlayHints);
    const QMap<QString, QString> overrides = lsp::parseOverrides(p.lspServers);
    for (int row = 0; row < _lspServers->rowCount(); ++row) {
        const QString id = _lspServers->item(row, 0)->data(Qt::UserRole).toString();
        QTableWidgetItem* command = _lspServers->item(row, 1);
        command->setText(QStringLiteral("x")); // fuerza itemChanged (y la búsqueda)
        command->setText(overrides.value(id));
    }
    updateEnabledState();
}

Preferences PreferencesDialog::preferences() const
{
    Preferences p = _base;
    p.statusBarShow = _statusBar->isChecked();
    p.tabBarHide = _tabBarHide->isChecked();
    p.showWelcome = _showWelcome->isChecked();
    p.tabCloseButton = _tabCloseButton->isChecked();
    p.fileTypeIcons = _fileTypeIcons->isChecked();
    p.tabDoubleClickCloses = _tabDoubleClick->isChecked();
    p.quitOnEmpty = _quitOnEmpty->isChecked();
    p.language = _language->currentData().toString();
    p.qtFileDialog = _qtFileDialog->isChecked();

    p.currentLineMode = static_cast<CurrentLineMode>(comboValue(_currentLine));
    p.currentLineFrameWidth = _frameWidth->value();
    p.caretWidth = comboValue(_caretWidth);
    p.caretBlinkRate = _blinkRate->value();
    p.multiSelection = _multiSelection->isChecked();
    p.virtualSpace = _virtualSpace->isChecked();
    p.scrollBeyondLastLine = _scrollBeyond->isChecked();
    p.lineCopyCutWithoutSelection = _lineCopyCut->isChecked();
    p.smoothFont = _smoothFont->isChecked();
    p.systemMonospaceFont = _systemMonospace->isChecked();
    p.darkModeFollowSystem = comboValue(_appearance) == 0;
    if (!p.darkModeFollowSystem)
        p.darkModeEnabled = comboValue(_appearance) == 2;
    p.multiInstSetting = comboValue(_multiInst) == 2 ? MultiInst::Always : MultiInst::Mono;
    p.toolbarVisible = comboValue(_toolbar) >= 0;
    if (p.toolbarVisible)
        p.toolbarIcons = static_cast<ToolbarIcons>(comboValue(_toolbar));
    p.whiteSpaceShow = _whiteSpace->isChecked();
    p.eolShow = _eol->isChecked();
    p.indentGuideLine = _indentGuides->isChecked();
    p.activeIndentGuide = _activeIndentGuide->isChecked();
    p.colorPreview = _colorPreview->isChecked();
    p.rainbowBrackets = _rainbowBrackets->isChecked();
    p.breadcrumbs = _breadcrumbs->isChecked();
    p.stickyScroll = _stickyScroll->isChecked();
    p.wrap = _wrap->isChecked();
    p.wrapIndent = static_cast<WrapIndent>(comboValue(_wrapIndent));
    p.wrapSymbolShow = _wrapSymbol->isChecked();

    p.lineNumberMargin = _lineNumbers->isChecked();
    p.lineNumberDynamicWidth = _dynamicWidth->isChecked();
    p.folderStyle = static_cast<FolderStyle>(comboValue(_folderStyle));
    p.edgeColumns = parseEdgeColumns(_edgeColumns->text());
    p.edgeBackground = _edgeBackground->isChecked();
    p.bookmarkMargin = _bookmarkMargin->isChecked();
    p.gitMargin = _gitMargin->isChecked();
    p.gitExplorer = _gitExplorer->isChecked();
    p.snippetsTab = _snippetsTab->isChecked();
    p.gitBlameInline = _gitBlameInline->isChecked();
    p.currentLineNumber = _currentLineNumber->isChecked();
    p.scrollBarMarks = _scrollBarMarks->isChecked();
    p.changeHistory = comboValue(_changeHistory);

    p.newDocEol = comboValue(_newEol);
    p.newDocUnicodeMode = comboValue(_newEncoding);
    p.newDocCodepage = p.newDocUnicodeMode == static_cast<int>(UniMode::Ansi) ? comboValue(_newCodepage) : -1;
    p.newDocLanguage = _newLanguage->count() > 0 ? comboValue(_newLanguage) : p.newDocLanguage;
    p.openSaveDir = static_cast<OpenSaveDir>(comboValue(_openSaveDir));
    p.defaultDirPath = _defaultDir->text().trimmed();
    p.terminalShell = _terminalShell->text().trimmed();
    p.lspEnabled = _lspEnabled->isChecked();
    p.inlayHints = _inlayHints->isChecked();
    QMap<QString, QString> overrides;
    for (int row = 0; row < _lspServers->rowCount(); ++row)
        overrides.insert(_lspServers->item(row, 0)->data(Qt::UserRole).toString(), _lspServers->item(row, 1)->text().trimmed());
    p.lspServers = lsp::formatOverrides(overrides);

    p.recentFilesMax = _recentMax->value();
    p.recentFilesInSubMenu = _recentSubMenu->isChecked();
    const int lengthMode = comboValue(_recentLengthMode);
    p.recentFilesCustomLength = lengthMode > 0 ? _recentLength->value() : lengthMode;

    p.tabSize = _tabSize->value();
    p.tabReplacedBySpace = _tabSpaces->isChecked();
    p.backspaceUnindent = _backspaceUnindent->isChecked();
    p.autoIndent = static_cast<AutoIndent>(comboValue(_autoIndent));

    p.autocStatus = comboValue(_autocStatus);
    p.autocFromLen = _autocFromLen->value();
    p.autocIgnoreNumbers = _autocIgnoreNumbers->isChecked();
    p.autocBrief = _autocBrief->isChecked();
    p.funcParams = _funcParams->isChecked();
    p.insertParentheses = _insertParentheses->isChecked();
    p.insertBrackets = _insertBrackets->isChecked();
    p.insertCurlyBrackets = _insertCurly->isChecked();
    p.insertQuotes = _insertQuotes->isChecked();
    p.insertDoubleQuotes = _insertDoubleQuotes->isChecked();
    p.insertHtmlXmlTag = _insertTag->isChecked();

    p.smartHighlight = _smart->isChecked();
    p.smartHighlightMatchCase = _smartCase->isChecked();
    p.smartHighlightWholeWord = _smartWord->isChecked();
    p.smartHighlightUseFindSettings = _smartFind->isChecked();
    p.smartHighlightOtherView = _smartOther->isChecked();
    p.tagsMatchHighlight = _tagsMatch->isChecked();
    p.tagAttributesHighlight = _tagAttributes->isChecked();
    p.markAllMatchCase = _markAllCase->isChecked();
    p.markAllWholeWord = _markAllWord->isChecked();

    p.fillFindFieldWithSelected = _fillFind->isChecked();
    p.inlineFind = _inlineFind->isChecked();
    p.confirmReplaceInAllOpenDocs = _confirmReplaceAll->isChecked();
    p.monospacedFontFindDlg = _monoFind->isChecked();

    p.printLineNumber = _printLineNumber->isChecked();
    p.printFormFeedPageBreak = _printFormFeed->isChecked();
    p.printColourMode = comboValue(_printColour);
    p.printMarginLeft = _printMargins[0]->value();
    p.printMarginTop = _printMargins[1]->value();
    p.printMarginRight = _printMargins[2]->value();
    p.printMarginBottom = _printMargins[3]->value();
    p.printHeaderLeft = _printHeader[0]->text();
    p.printHeaderMiddle = _printHeader[1]->text();
    p.printHeaderRight = _printHeader[2]->text();
    p.printFooterLeft = _printFooter[0]->text();
    p.printFooterMiddle = _printFooter[1]->text();
    p.printFooterRight = _printFooter[2]->text();
    p.printHeaderFontName = _printHeaderFont->currentData().toString();
    p.printFooterFontName = _printFooterFont->currentData().toString();
    p.printHeaderFontSize = _printHeaderSize->value();
    p.printFooterFontSize = _printFooterSize->value();
    p.printHeaderFontStyle = comboValue(_printHeaderStyle);
    p.printFooterFontStyle = comboValue(_printFooterStyle);

    p.rememberLastSession = _remember->isChecked();
    p.snapshotMode = _snapshot->isChecked();
    p.snapshotBackupTiming = _snapshotSeconds->value() * 1000;
    p.backupOnSave = static_cast<BackupOnSave>(comboValue(_backupOnSave));
    p.useCustomBackupDir = _customBackupDir->isChecked();
    p.backupDir = _backupDir->text().trimmed();
    p.sessionExt = _sessionExt->text().trimmed();
    if (p.sessionExt.startsWith(QLatin1Char('.')))
        p.sessionExt.remove(0, 1);

    int detection = FileAutoDetection::Disabled;
    if (_detect->isChecked()) {
        detection = _detectAll->isChecked() ? FileAutoDetection::AllFiles : FileAutoDetection::CurrentFile;
        if (_autoUpdate->isChecked())
            detection |= FileAutoDetection::AutoUpdate;
        if (_goToEnd->isChecked())
            detection |= FileAutoDetection::GoToEnd;
    }
    p.fileAutoDetection = detection;
    p.detectEncoding = _detectEncoding->isChecked();
    p.fallbackCodepage = comboValue(_fallbackCodepage);
    return p;
}

} // namespace edita
