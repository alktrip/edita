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


#include "ui/UserLanguageDialog.h"

#include "ui/Prompter.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace edita {

namespace {

// Bits de anidamiento (SCE_USER_MASK_NESTING_*) en el orden de la casilla del diálogo
struct NestingOption
{
    const char* label;
    int mask;
};
constexpr NestingOption kNesting[21] = {
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiter 1"), 0x1},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiter 2"), 0x2},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiter 3"), 0x4},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiter 4"), 0x8},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiter 5"), 0x10},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiter 6"), 0x20},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiter 7"), 0x40},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiter 8"), 0x80},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Comment"), 0x100},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Comment line"), 0x200},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 1"), 0x400},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 2"), 0x800},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 3"), 0x1000},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 4"), 0x2000},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 5"), 0x4000},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 6"), 0x8000},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 7"), 0x10000},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 8"), 0x20000},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Operators 1"), 0x1000000},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Operators 2"), 0x2000000},
    {QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Numbers"), 0x4000000},
};

// Nombres de los estilos tal como se muestran
const char* const kStyleLabels[kUserStyleCount] = {
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Default"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Comments"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Line comments"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Numbers"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 1"), QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 2"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 3"), QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 4"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 5"), QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 6"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 7"), QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Keywords 8"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Operators"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Folder in code 1"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Folder in code 2"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Folder in comment"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiters 1"), QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiters 2"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiters 3"), QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiters 4"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiters 5"), QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiters 6"),
    QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiters 7"), QT_TRANSLATE_NOOP("edita::UserLanguageDialog", "Delimiters 8"),
};

bool acceptsNesting(int styleId)
{
    return styleId == 1 || styleId == 2 || (styleId >= 16 && styleId <= 23);
}

// BGR (COLORREF) ↔ QColor
QColor toQColor(int bgr)
{
    return QColor(bgr & 0xFF, (bgr >> 8) & 0xFF, (bgr >> 16) & 0xFF);
}

int fromQColor(const QColor& c)
{
    return c.red() | (c.green() << 8) | (c.blue() << 16);
}

void paintButton(QPushButton* button, int bgr)
{
    button->setText(toQColor(bgr).name());
    button->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                              .arg(toQColor(bgr).name(), toQColor(bgr).lightness() < 128 ? QStringLiteral("white") : QStringLiteral("black")));
}

QLineEdit* lineEdit(const char* name)
{
    auto* edit = new QLineEdit;
    edit->setObjectName(QString::fromLatin1(name));
    return edit;
}

} // namespace

UserLanguageDialog::UserLanguageDialog(UserLanguages& languages, Prompter& prompter, QWidget* parent)
    : QDialog(parent)
    , _languages(languages)
    , _prompter(prompter)
    , _languageCombo(new QComboBox(this))
{
    setWindowTitle(tr("User Defined Languages"));
    _languageCombo->setObjectName(QStringLiteral("languageCombo"));

    // Fila superior: lenguaje y acciones (como la cabecera del diálogo de N++)
    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Language:")));
    top->addWidget(_languageCombo, 1);
    auto addButton = [&](const QString& text, auto&& slot) {
        auto* button = new QPushButton(text);
        connect(button, &QPushButton::clicked, this, slot);
        top->addWidget(button);
        return button;
    };
    addButton(tr("Create New…"), [this] {
        const QString name = askName(tr("New language"), QString());
        if (!name.isEmpty())
            createLanguage(name);
    });
    addButton(tr("Save As…"), [this] {
        const QString name = askName(tr("Save As"), currentName());
        if (!name.isEmpty())
            saveCurrentAs(name);
    });
    addButton(tr("Rename…"), [this] {
        const QString name = askName(tr("Rename"), currentName());
        if (!name.isEmpty())
            renameCurrent(name);
    });
    addButton(tr("Remove"), [this] {
        if (!currentName().isEmpty()
            && _prompter.confirm(this, tr("Remove"), tr("Remove the language \"%1\"?").arg(currentName())))
            removeCurrent();
    });
    addButton(tr("Import…"), [this] {
        const QStringList files = _prompter.openFileNames(this, QDir::homePath(), tr("Notepad++ languages (*.xml)"));
        for (const QString& file : files) {
            QStringList imported;
            QString error;
            if (!_languages.importFile(file, &imported, &error))
                _prompter.warn(this, tr("Import"), error);
            else if (!imported.isEmpty())
                refreshLanguageList(imported.front());
        }
        emit languagesChanged();
    });
    addButton(tr("Export…"), [this] {
        if (currentName().isEmpty())
            return;
        const QString path = _prompter.saveFileName(this, QDir::home().filePath(currentName() + QStringLiteral(".xml")),
                                                    tr("XML (*.xml)"), tr("XML (*.xml)"));
        QString error;
        if (!path.isEmpty() && !_languages.exportLanguage(currentName(), path, &error))
            _prompter.warn(this, tr("Export"), error);
    });

    auto* settings = new QHBoxLayout;
    _extensions = lineEdit("extensions");
    _extensions->setPlaceholderText(tr("space separated extensions, without dot"));
    _caseIgnored = new QCheckBox(tr("Ignore case"));
    _caseIgnored->setObjectName(QStringLiteral("caseIgnored"));
    settings->addWidget(new QLabel(tr("Ext.:")));
    settings->addWidget(_extensions, 1);
    settings->addWidget(_caseIgnored);

    auto* tabs = new QTabWidget;
    auto addTab = [&](const QString& title, void (UserLanguageDialog::*build)(QWidget*)) {
        auto* page = new QWidget;
        (this->*build)(page);
        tabs->addTab(page, title);
    };
    addTab(tr("Folder && Default"), &UserLanguageDialog::buildFolderPage);
    addTab(tr("Keywords Lists"), &UserLanguageDialog::buildKeywordsPage);
    addTab(tr("Comment && Number"), &UserLanguageDialog::buildCommentsPage);
    addTab(tr("Operators && Delimiter"), &UserLanguageDialog::buildOperatorsPage);
    addTab(tr("Styles"), &UserLanguageDialog::buildStylesPage);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save"));
    buttons->button(QDialogButtonBox::Close)->setText(tr("Close"));
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, this, &UserLanguageDialog::saveCurrent);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addLayout(settings);
    layout->addWidget(tabs, 1);
    layout->addWidget(buttons);

    connect(_languageCombo, &QComboBox::currentTextChanged, this, [this](const QString& name) {
        if (_loading)
            return;
        if (const UserLanguage* lang = _languages.find(name))
            loadLanguage(*lang);
    });
    refreshLanguageList(QString());
    resize(860, 600);
}

// --- Páginas ---------------------------------------------------------------------------------

void UserLanguageDialog::buildFolderPage(QWidget* page)
{
    auto* grid = new QGridLayout(page);
    const QString columns[] = {tr("Open"), tr("Middle"), tr("Close")};
    for (int c = 0; c < 3; ++c)
        grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(columns[c])), 0, c + 1);
    const QString rows[] = {tr("Folding in code 1 style:"), tr("Folding in code 2 style (separators needed):"), tr("Folding in comment style:")};
    const char* names[] = {"foldCode1Open", "foldCode1Middle", "foldCode1Close", "foldCode2Open", "foldCode2Middle",
                           "foldCode2Close", "foldCommentOpen", "foldCommentMiddle", "foldCommentClose"};
    for (int r = 0; r < 3; ++r) {
        grid->addWidget(new QLabel(rows[r]), r + 1, 0);
        for (int c = 0; c < 3; ++c) {
            const int i = r * 3 + c;
            _folders[static_cast<std::size_t>(i)] = lineEdit(names[i]);
            grid->addWidget(_folders[static_cast<std::size_t>(i)], r + 1, c + 1);
        }
    }
    _foldCompact = new QCheckBox(tr("Fold compact (fold empty lines too)"));
    _foldCompact->setObjectName(QStringLiteral("foldCompact"));
    grid->addWidget(_foldCompact, 4, 0, 1, 4);
    grid->setRowStretch(5, 1);
}

void UserLanguageDialog::buildKeywordsPage(QWidget* page)
{
    auto* grid = new QGridLayout(page);
    for (int i = 0; i < 8; ++i) {
        auto* box = new QGroupBox(tr("Keywords %1").arg(i + 1));
        auto* v = new QVBoxLayout(box);
        _keywords[static_cast<std::size_t>(i)] = new QPlainTextEdit;
        _keywords[static_cast<std::size_t>(i)]->setObjectName(QStringLiteral("keywords%1").arg(i + 1));
        _prefix[static_cast<std::size_t>(i)] = new QCheckBox(tr("Prefix mode"));
        _prefix[static_cast<std::size_t>(i)]->setObjectName(QStringLiteral("prefix%1").arg(i + 1));
        v->addWidget(_keywords[static_cast<std::size_t>(i)]);
        v->addWidget(_prefix[static_cast<std::size_t>(i)]);
        grid->addWidget(box, i / 4, i % 4);
    }
}

void UserLanguageDialog::buildCommentsPage(QWidget* page)
{
    auto* layout = new QHBoxLayout(page);
    auto* comments = new QGroupBox(tr("Comments"));
    auto* form = new QFormLayout(comments);
    const char* names[] = {"lineCommentOpen", "lineCommentContinue", "lineCommentClose", "commentOpen", "commentClose"};
    const QString labels[] = {tr("Line comment open:"), tr("Line comment continue character:"), tr("Line comment close:"), tr("Comment open:"),
                              tr("Comment close:")};
    for (int i = 0; i < 5; ++i) {
        _comments[static_cast<std::size_t>(i)] = lineEdit(names[i]);
        form->addRow(labels[i], _comments[static_cast<std::size_t>(i)]);
    }
    _foldComments = new QCheckBox(tr("Allow folding of comments"));
    _foldComments->setObjectName(QStringLiteral("allowFoldOfComments"));
    form->addRow(_foldComments);
    _pureLC = new QButtonGroup(this);
    const QString pure[] = {tr("Allow anywhere"), tr("Force at beginning of line"),
                            tr("Allow preceding whitespace")};
    for (int i = 0; i < 3; ++i) {
        auto* radio = new QRadioButton(pure[i]);
        radio->setObjectName(QStringLiteral("pureLC%1").arg(i));
        _pureLC->addButton(radio, i);
        form->addRow(radio);
    }

    auto* numbers = new QGroupBox(tr("Numbers"));
    auto* nform = new QFormLayout(numbers);
    const char* numberNames[] = {"numberPrefix1", "numberPrefix2", "numberExtras1", "numberExtras2",
                                 "numberSuffix1", "numberSuffix2", "numberRange"};
    const QString numberLabels[] = {tr("Prefix 1:"), tr("Prefix 2:"), tr("Extras 1:"), tr("Extras 2:"),
                                    tr("Suffix 1:"), tr("Suffix 2:"), tr("Range:")};
    for (int i = 0; i < 7; ++i) {
        _numbers[static_cast<std::size_t>(i)] = lineEdit(numberNames[i]);
        nform->addRow(numberLabels[i], _numbers[static_cast<std::size_t>(i)]);
    }
    _decimal = new QButtonGroup(this);
    const QString decimal[] = {tr("Decimal separator: dot"), tr("comma"), tr("both")};
    auto* decimalRow = new QHBoxLayout;
    for (int i = 0; i < 3; ++i) {
        auto* radio = new QRadioButton(decimal[i]);
        radio->setObjectName(QStringLiteral("decimal%1").arg(i));
        _decimal->addButton(radio, i);
        decimalRow->addWidget(radio);
    }
    nform->addRow(decimalRow);
    layout->addWidget(comments, 1);
    layout->addWidget(numbers, 1);
}

void UserLanguageDialog::buildOperatorsPage(QWidget* page)
{
    auto* layout = new QVBoxLayout(page);
    auto* ops = new QFormLayout;
    _operators1 = lineEdit("operators1");
    _operators2 = lineEdit("operators2");
    ops->addRow(tr("Operators 1 (separators not required):"), _operators1);
    ops->addRow(tr("Operators 2 (separators required):"), _operators2);
    layout->addLayout(ops);
    auto* grid = new QGridLayout;
    const QString columns[] = {tr("Open"), tr("Escape"), tr("Close")};
    for (int c = 0; c < 3; ++c)
        grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(columns[c])), 0, c + 1);
    for (int d = 0; d < 8; ++d) {
        grid->addWidget(new QLabel(tr("Delimiter %1:").arg(d + 1)), d + 1, 0);
        for (int c = 0; c < 3; ++c) {
            const int i = d * 3 + c;
            _delimiters[static_cast<std::size_t>(i)] = lineEdit("");
            _delimiters[static_cast<std::size_t>(i)]->setObjectName(QStringLiteral("delimiter%1").arg(i));
            grid->addWidget(_delimiters[static_cast<std::size_t>(i)], d + 1, c + 1);
        }
    }
    layout->addLayout(grid);
    layout->addStretch(1);
}

void UserLanguageDialog::buildStylesPage(QWidget* page)
{
    auto* layout = new QHBoxLayout(page);
    _styleList = new QListWidget;
    _styleList->setObjectName(QStringLiteral("styleList"));
    for (const char* label : kStyleLabels)
        _styleList->addItem(tr(label));
    _styleList->setMaximumWidth(220);
    layout->addWidget(_styleList);

    auto* editor = new QWidget;
    auto* form = new QFormLayout(editor);
    auto colorRow = [&](QPushButton*& button, QCheckBox*& transparent, const char* name, int UserLanguageDialog::* color) {
        auto* row = new QHBoxLayout;
        button = new QPushButton;
        button->setObjectName(QString::fromLatin1(name));
        transparent = new QCheckBox(tr("Transparent"));
        transparent->setObjectName(QStringLiteral("%1Transparent").arg(QString::fromLatin1(name)));
        row->addWidget(button);
        row->addWidget(transparent);
        connect(button, &QPushButton::clicked, this, [this, button, color] {
            const QColor chosen = QColorDialog::getColor(toQColor(this->*color), this);
            if (!chosen.isValid())
                return;
            this->*color = fromQColor(chosen);
            paintButton(button, this->*color);
            storeStyle();
        });
        connect(transparent, &QCheckBox::toggled, this, [this] { storeStyle(); updateStyleButtons(); });
        return row;
    };
    form->addRow(tr("Foreground:"), colorRow(_fgButton, _fgTransparent, "fgColor", &UserLanguageDialog::_fgColor));
    form->addRow(tr("Background:"), colorRow(_bgButton, _bgTransparent, "bgColor", &UserLanguageDialog::_bgColor));
    _fontName = new QComboBox;
    _fontName->setObjectName(QStringLiteral("fontName"));
    _fontName->addItem(tr("(default)"), QString());
    for (const QString& family : QFontDatabase::families())
        _fontName->addItem(family, family);
    form->addRow(tr("Font:"), _fontName);
    _fontSize = new QSpinBox;
    _fontSize->setObjectName(QStringLiteral("fontSize"));
    _fontSize->setRange(0, 72);
    _fontSize->setSpecialValueText(tr("(default)"));
    form->addRow(tr("Size:"), _fontSize);
    auto* styleRow = new QHBoxLayout;
    _bold = new QCheckBox(tr("Bold"));
    _italic = new QCheckBox(tr("Italic"));
    _underline = new QCheckBox(tr("Underline"));
    _bold->setObjectName(QStringLiteral("bold"));
    _italic->setObjectName(QStringLiteral("italic"));
    _underline->setObjectName(QStringLiteral("underline"));
    styleRow->addWidget(_bold);
    styleRow->addWidget(_italic);
    styleRow->addWidget(_underline);
    form->addRow(styleRow);

    _nestingBox = new QGroupBox(tr("Nesting"));
    auto* nestingGrid = new QGridLayout(_nestingBox);
    for (int i = 0; i < 21; ++i) {
        _nesting[static_cast<std::size_t>(i)] = new QCheckBox(tr(kNesting[i].label));
        _nesting[static_cast<std::size_t>(i)]->setObjectName(QStringLiteral("nesting%1").arg(i));
        nestingGrid->addWidget(_nesting[static_cast<std::size_t>(i)], i / 3, i % 3);
        connect(_nesting[static_cast<std::size_t>(i)], &QCheckBox::toggled, this, &UserLanguageDialog::storeStyle);
    }
    form->addRow(_nestingBox);
    layout->addWidget(editor, 1);

    connect(_fontName, &QComboBox::currentIndexChanged, this, &UserLanguageDialog::storeStyle);
    connect(_fontSize, &QSpinBox::valueChanged, this, &UserLanguageDialog::storeStyle);
    for (QCheckBox* box : {_bold, _italic, _underline})
        connect(box, &QCheckBox::toggled, this, &UserLanguageDialog::storeStyle);
    connect(_styleList, &QListWidget::currentRowChanged, this, &UserLanguageDialog::showStyle);
}

// --- Estado ----------------------------------------------------------------------------------

QString UserLanguageDialog::askName(const QString& title, const QString& initial)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, title, tr("Name:"), QLineEdit::Normal, initial, &ok).trimmed();
    return ok ? name : QString();
}

void UserLanguageDialog::refreshLanguageList(const QString& select)
{
    _loading = true;
    _languageCombo->clear();
    for (const UserLanguage& lang : _languages.languages())
        _languageCombo->addItem(lang.name);
    _loading = false;
    const int index = std::max(0, _languageCombo->findText(select));
    if (_languageCombo->count() > 0) {
        _languageCombo->setCurrentIndex(index);
        if (const UserLanguage* lang = _languages.find(_languageCombo->currentText()))
            loadLanguage(*lang);
    } else {
        loadLanguage(newUserLanguage(QString()));
    }
    setEnabled(true);
}

QString UserLanguageDialog::currentName() const
{
    return _current.name;
}

void UserLanguageDialog::selectLanguage(const QString& name)
{
    refreshLanguageList(name);
}

void UserLanguageDialog::loadLanguage(const UserLanguage& lang)
{
    _loading = true;
    _current = lang;
    _extensions->setText(lang.extensions);
    _caseIgnored->setChecked(lang.caseIgnored);
    for (int i = 0; i < 9; ++i)
        _folders[static_cast<std::size_t>(i)]->setText(QString::fromStdString(
            lang.keywordLists[static_cast<std::size_t>(UserKeywordList::FoldersInCode1Open + i)]));
    _foldCompact->setChecked(lang.foldCompact);
    for (int i = 0; i < 8; ++i) {
        _keywords[static_cast<std::size_t>(i)]->setPlainText(QString::fromStdString(
            lang.keywordLists[static_cast<std::size_t>(UserKeywordList::Keywords1 + i)]));
        _prefix[static_cast<std::size_t>(i)]->setChecked(lang.isPrefix[static_cast<std::size_t>(i)]);
    }
    for (int i = 0; i < 5; ++i)
        _comments[static_cast<std::size_t>(i)]->setText(userSlotWords(lang.keywordLists[UserKeywordList::Comments], i));
    _foldComments->setChecked(lang.allowFoldOfComments);
    if (QAbstractButton* b = _pureLC->button(std::clamp(lang.forcePureLC, 0, 2)))
        b->setChecked(true);
    for (int i = 0; i < 7; ++i)
        _numbers[static_cast<std::size_t>(i)]->setText(QString::fromStdString(
            lang.keywordLists[static_cast<std::size_t>(UserKeywordList::NumberPrefix1 + i)]));
    if (QAbstractButton* b = _decimal->button(std::clamp(lang.decimalSeparator, 0, 2)))
        b->setChecked(true);
    _operators1->setText(QString::fromStdString(lang.keywordLists[UserKeywordList::Operators1]));
    _operators2->setText(QString::fromStdString(lang.keywordLists[UserKeywordList::Operators2]));
    for (int i = 0; i < 24; ++i)
        _delimiters[static_cast<std::size_t>(i)]->setText(userSlotWords(lang.keywordLists[UserKeywordList::Delimiters], i));
    _loading = false;
    _styleList->setCurrentRow(-1);
    _styleList->setCurrentRow(0);
}

UserLanguage UserLanguageDialog::editedLanguage() const
{
    UserLanguage lang = _current;
    lang.extensions = _extensions->text().simplified();
    lang.caseIgnored = _caseIgnored->isChecked();
    for (int i = 0; i < 9; ++i)
        lang.keywordLists[static_cast<std::size_t>(UserKeywordList::FoldersInCode1Open + i)] =
            _folders[static_cast<std::size_t>(i)]->text().toStdString();
    lang.foldCompact = _foldCompact->isChecked();
    for (int i = 0; i < 8; ++i) {
        lang.keywordLists[static_cast<std::size_t>(UserKeywordList::Keywords1 + i)] =
            _keywords[static_cast<std::size_t>(i)]->toPlainText().toStdString();
        lang.isPrefix[static_cast<std::size_t>(i)] = _prefix[static_cast<std::size_t>(i)]->isChecked();
    }
    QStringList comments;
    for (QLineEdit* edit : _comments)
        comments << edit->text();
    lang.keywordLists[UserKeywordList::Comments] = userComposeSlots(comments);
    lang.allowFoldOfComments = _foldComments->isChecked();
    lang.forcePureLC = std::max(0, _pureLC->checkedId());
    for (int i = 0; i < 7; ++i)
        lang.keywordLists[static_cast<std::size_t>(UserKeywordList::NumberPrefix1 + i)] =
            _numbers[static_cast<std::size_t>(i)]->text().toStdString();
    lang.decimalSeparator = std::max(0, _decimal->checkedId());
    lang.keywordLists[UserKeywordList::Operators1] = _operators1->text().toStdString();
    lang.keywordLists[UserKeywordList::Operators2] = _operators2->text().toStdString();
    QStringList delimiters;
    for (QLineEdit* edit : _delimiters)
        delimiters << edit->text();
    lang.keywordLists[UserKeywordList::Delimiters] = userComposeSlots(delimiters);
    return lang;
}

void UserLanguageDialog::showStyle(int id)
{
    if (id < 0)
        return;
    _styleId = id;
    const Style* s = _current.style(id);
    if (!s)
        return;
    _loading = true;
    _fgColor = s->fgColor == kStyleNotUsed ? 0 : s->fgColor;
    _bgColor = s->bgColor == kStyleNotUsed ? 0xFFFFFF : s->bgColor;
    paintButton(_fgButton, _fgColor);
    paintButton(_bgButton, _bgColor);
    _fgTransparent->setChecked(!(s->colorStyle & kColorStyleForeground));
    _bgTransparent->setChecked(!(s->colorStyle & kColorStyleBackground));
    const int fontIndex = _fontName->findData(QString::fromStdString(s->fontName));
    if (fontIndex < 0 && !s->fontName.empty())
        _fontName->addItem(QString::fromStdString(s->fontName), QString::fromStdString(s->fontName));
    _fontName->setCurrentIndex(std::max(0, _fontName->findData(QString::fromStdString(s->fontName))));
    _fontSize->setValue(s->fontSize > 0 ? s->fontSize : 0);
    const int fontStyle = s->fontStyle == kStyleNotUsed ? 0 : s->fontStyle;
    _bold->setChecked(fontStyle & kFontStyleBold);
    _italic->setChecked(fontStyle & kFontStyleItalic);
    _underline->setChecked(fontStyle & kFontStyleUnderline);
    for (int i = 0; i < 21; ++i)
        _nesting[static_cast<std::size_t>(i)]->setChecked((s->nesting & kNesting[i].mask) != 0);
    _nestingBox->setVisible(acceptsNesting(id));
    _loading = false;
    updateStyleButtons();
}

void UserLanguageDialog::updateStyleButtons()
{
    _fgButton->setEnabled(!_fgTransparent->isChecked());
    _bgButton->setEnabled(!_bgTransparent->isChecked());
}

void UserLanguageDialog::storeStyle()
{
    if (_loading)
        return;
    Style* s = _current.style(_styleId);
    if (!s)
        return;
    s->fgColor = _fgColor;
    s->bgColor = _bgColor;
    s->colorStyle = (_fgTransparent->isChecked() ? 0 : kColorStyleForeground)
                  | (_bgTransparent->isChecked() ? 0 : kColorStyleBackground);
    s->fontName = _fontName->currentData().toString().toStdString();
    s->fontSize = _fontSize->value() > 0 ? _fontSize->value() : kStyleNotUsed;
    s->fontStyle = (_bold->isChecked() ? kFontStyleBold : 0) | (_italic->isChecked() ? kFontStyleItalic : 0)
                 | (_underline->isChecked() ? kFontStyleUnderline : 0);
    int nesting = 0;
    if (acceptsNesting(_styleId)) {
        for (int i = 0; i < 21; ++i) {
            if (_nesting[static_cast<std::size_t>(i)]->isChecked())
                nesting |= kNesting[i].mask;
        }
    }
    s->nesting = nesting;
}

// --- Acciones --------------------------------------------------------------------------------

bool UserLanguageDialog::createLanguage(const QString& name)
{
    if (name.isEmpty() || _languages.find(name)) {
        _prompter.warn(this, tr("New language"), tr("A language named \"%1\" already exists.").arg(name));
        return false;
    }
    QString error;
    if (!_languages.save(newUserLanguage(name), &error)) {
        _prompter.warn(this, tr("New language"), error);
        return false;
    }
    refreshLanguageList(name);
    emit languagesChanged();
    return true;
}

bool UserLanguageDialog::saveCurrent()
{
    if (currentName().isEmpty())
        return false;
    QString error;
    if (!_languages.save(editedLanguage(), &error)) {
        _prompter.warn(this, tr("Save"), error);
        return false;
    }
    if (const UserLanguage* saved = _languages.find(currentName()))
        _current = *saved;
    emit languagesChanged();
    return true;
}

bool UserLanguageDialog::saveCurrentAs(const QString& newName)
{
    if (newName.isEmpty() || _languages.find(newName)) {
        _prompter.warn(this, tr("Save As"), tr("A language named \"%1\" already exists.").arg(newName));
        return false;
    }
    UserLanguage copy = editedLanguage();
    copy.name = newName;
    copy.sourceFile.clear();
    QString error;
    if (!_languages.save(copy, &error)) {
        _prompter.warn(this, tr("Save As"), error);
        return false;
    }
    refreshLanguageList(newName);
    emit languagesChanged();
    return true;
}

bool UserLanguageDialog::renameCurrent(const QString& newName)
{
    if (currentName().isEmpty() || newName == currentName())
        return false;
    if (_languages.find(newName)) {
        _prompter.warn(this, tr("Rename"), tr("A language named \"%1\" already exists.").arg(newName));
        return false;
    }
    UserLanguage renamed = editedLanguage();
    const QString oldName = renamed.name;
    renamed.name = newName;
    QString error;
    if (!_languages.remove(oldName, &error) || !_languages.save(renamed, &error)) {
        _prompter.warn(this, tr("Rename"), error);
        return false;
    }
    refreshLanguageList(newName);
    emit languagesChanged();
    return true;
}

bool UserLanguageDialog::removeCurrent()
{
    if (currentName().isEmpty())
        return false;
    QString error;
    if (!_languages.remove(currentName(), &error)) {
        _prompter.warn(this, tr("Remove"), error);
        return false;
    }
    refreshLanguageList(QString());
    emit languagesChanged();
    return true;
}

} // namespace edita
