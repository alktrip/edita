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

#include "ui/FindReplaceDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTabBar>
#include <QVBoxLayout>

namespace edita {

namespace {

constexpr int kHistorySize = 30; // N++ guarda hasta 30 entradas por lista

QComboBox* makeCombo(QWidget* parent, const char* name)
{
    auto* combo = new QComboBox(parent);
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    combo->setMaxCount(kHistorySize);
    combo->setMinimumContentsLength(40);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setObjectName(QString::fromLatin1(name));
    return combo;
}

void remember(QComboBox* combo)
{
    const QString text = combo->currentText();
    if (text.isEmpty())
        return;
    const int existing = combo->findText(text, Qt::MatchExactly | Qt::MatchCaseSensitive);
    if (existing >= 0)
        combo->removeItem(existing);
    combo->insertItem(0, text);
    combo->setCurrentIndex(0);
}

QStringList items(const QComboBox* combo)
{
    QStringList list;
    for (int i = 0; i < combo->count(); ++i)
        list << combo->itemText(i);
    return list;
}

} // namespace

FindReplaceDialog::FindReplaceDialog(QWidget* parent)
    : QDialog(parent)
    , _tabs(new QTabBar(this))
    , _find(makeCombo(this, "findCombo"))
    , _replace(makeCombo(this, "replaceCombo"))
    , _filters(makeCombo(this, "filtersCombo"))
    , _directory(makeCombo(this, "directoryCombo"))
    , _replaceLabel(new QLabel(tr("Rep&lace with:"), this))
    , _filtersLabel(new QLabel(tr("&Filters:"), this))
    , _directoryLabel(new QLabel(tr("&Directory:"), this))
    , _browse(new QPushButton(QStringLiteral("…"), this))
    , _wholeWord(new QCheckBox(tr("Match &whole word only"), this))
    , _matchCase(new QCheckBox(tr("Match &case"), this))
    , _wrapAround(new QCheckBox(tr("Wra&p around"), this))
    , _backward(new QCheckBox(tr("&Backward direction"), this))
    , _inSelection(new QCheckBox(tr("In &selection"), this))
    , _recursive(new QCheckBox(tr("In all su&b-folders"), this))
    , _hidden(new QCheckBox(tr("In &hidden folders"), this))
    , _followDoc(new QCheckBox(tr("Follow current doc."), this))
    , _normal(new QRadioButton(tr("&Normal"), this))
    , _extended(new QRadioButton(tr("&Extended (\\n, \\r, \\t, \\0, \\x…)"), this))
    , _regex(new QRadioButton(tr("Re&gular expression"), this))
    , _dotNewline(new QCheckBox(tr("&. matches newline"), this))
    , _status(new QLabel(this))
{
    setWindowTitle(tr("Find"));
    setObjectName(QStringLiteral("findReplaceDialog"));
    setModal(false);

    _tabs->addTab(tr("Find"));
    _tabs->addTab(tr("Replace"));
    _tabs->addTab(tr("Find in Files"));
    _tabs->setObjectName(QStringLiteral("findTabs"));

    _find->setObjectName(QStringLiteral("findCombo"));
    _filters->setEditText(QStringLiteral("*.*"));
    _wrapAround->setChecked(true);
    _recursive->setChecked(true);
    _normal->setChecked(true);
    _wholeWord->setObjectName(QStringLiteral("wholeWordCheck"));
    _matchCase->setObjectName(QStringLiteral("matchCaseCheck"));
    _wrapAround->setObjectName(QStringLiteral("wrapCheck"));
    _backward->setObjectName(QStringLiteral("backwardCheck"));
    _inSelection->setObjectName(QStringLiteral("inSelectionCheck"));
    _normal->setObjectName(QStringLiteral("modeNormal"));
    _extended->setObjectName(QStringLiteral("modeExtended"));
    _regex->setObjectName(QStringLiteral("modeRegex"));
    _dotNewline->setObjectName(QStringLiteral("dotNewlineCheck"));
    _status->setObjectName(QStringLiteral("findStatus"));
    _status->setWordWrap(true);

    auto* findLabel = new QLabel(tr("&Find what:"), this);
    findLabel->setBuddy(_find);
    _replaceLabel->setBuddy(_replace);
    _filtersLabel->setBuddy(_filters);
    _directoryLabel->setBuddy(_directory);

    auto* fields = new QGridLayout;
    fields->addWidget(findLabel, 0, 0);
    fields->addWidget(_find, 0, 1, 1, 2);
    fields->addWidget(_replaceLabel, 1, 0);
    fields->addWidget(_replace, 1, 1, 1, 2);
    fields->addWidget(_filtersLabel, 2, 0);
    fields->addWidget(_filters, 2, 1, 1, 2);
    fields->addWidget(_directoryLabel, 3, 0);
    fields->addWidget(_directory, 3, 1);
    fields->addWidget(_browse, 3, 2);
    fields->setColumnStretch(1, 1);

    auto* options = new QVBoxLayout;
    for (QWidget* w : std::initializer_list<QWidget*>{_wholeWord, _matchCase, _wrapAround, _backward, _inSelection,
                                                      _recursive, _hidden, _followDoc})
        options->addWidget(w);
    options->addStretch();

    auto* modeBox = new QGroupBox(tr("Search Mode"), this);
    auto* modeLayout = new QVBoxLayout(modeBox);
    modeLayout->addWidget(_normal);
    modeLayout->addWidget(_extended);
    auto* regexRow = new QHBoxLayout;
    regexRow->addWidget(_regex);
    regexRow->addWidget(_dotNewline);
    regexRow->addStretch();
    modeLayout->addLayout(regexRow);

    _findNext = makeButton(tr("Find &Next"), "findNextButton");
    _count = makeButton(tr("C&ount"), "countButton");
    _findAllCurrent = makeButton(tr("Find All in Current &Document"), "findAllCurrentButton");
    _findAllOpen = makeButton(tr("Find All in All &Opened Documents"), "findAllOpenButton");
    _replaceOne = makeButton(tr("&Replace"), "replaceButton");
    _replaceAll = makeButton(tr("Replace &All"), "replaceAllButton");
    _replaceAllOpen = makeButton(tr("Replace All in All Opened Documents"), "replaceAllOpenButton");
    _findInFiles = makeButton(tr("Find &All"), "findInFilesButton");
    _replaceInFiles = makeButton(tr("Replace in &Files"), "replaceInFilesButton");
    _cancel = makeButton(tr("Cancel"), "cancelButton");
    QPushButton* close = makeButton(tr("Close"), "closeButton");

    auto* buttons = new QVBoxLayout;
    for (QPushButton* b : {_findNext, _count, _findAllCurrent, _findAllOpen, _replaceOne, _replaceAll, _replaceAllOpen,
                           _findInFiles, _replaceInFiles, _cancel})
        buttons->addWidget(b);
    buttons->addStretch();
    buttons->addWidget(close);

    auto* left = new QVBoxLayout;
    left->addLayout(fields);
    auto* optionsRow = new QHBoxLayout;
    optionsRow->addLayout(options);
    optionsRow->addWidget(modeBox, 1);
    left->addLayout(optionsRow);
    left->addWidget(_status);

    auto* body = new QHBoxLayout;
    body->addLayout(left, 1);
    body->addLayout(buttons);

    auto* main = new QVBoxLayout(this);
    main->addWidget(_tabs);
    main->addLayout(body);

    connect(_tabs, &QTabBar::currentChanged, this, &FindReplaceDialog::updateForTab);
    connect(_regex, &QRadioButton::toggled, this, &FindReplaceDialog::updateModeDependentOptions);
    connect(_findNext, &QPushButton::clicked, this, [this] { emit findNextRequested(false); });
    connect(_count, &QPushButton::clicked, this, &FindReplaceDialog::countRequested);
    connect(_findAllCurrent, &QPushButton::clicked, this, &FindReplaceDialog::findAllInCurrentRequested);
    connect(_findAllOpen, &QPushButton::clicked, this, &FindReplaceDialog::findAllInOpenRequested);
    connect(_replaceOne, &QPushButton::clicked, this, &FindReplaceDialog::replaceRequested);
    connect(_replaceAll, &QPushButton::clicked, this, &FindReplaceDialog::replaceAllRequested);
    connect(_replaceAllOpen, &QPushButton::clicked, this, &FindReplaceDialog::replaceAllInOpenRequested);
    connect(_findInFiles, &QPushButton::clicked, this, &FindReplaceDialog::findInFilesRequested);
    connect(_replaceInFiles, &QPushButton::clicked, this, &FindReplaceDialog::replaceInFilesRequested);
    connect(_cancel, &QPushButton::clicked, this, &FindReplaceDialog::cancelFileSearchRequested);
    connect(close, &QPushButton::clicked, this, &QDialog::hide);
    connect(_browse, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Directory"), directory());
        if (!dir.isEmpty())
            setDirectory(dir);
    });
    // Intro en el campo de búsqueda = acción principal de la pestaña
    connect(_find->lineEdit(), &QLineEdit::returnPressed, this, [this] {
        if (currentTab() == Tab::FindInFiles)
            emit findInFilesRequested();
        else
            emit findNextRequested(false);
    });

    updateForTab();
    updateModeDependentOptions();
}

QPushButton* FindReplaceDialog::makeButton(const QString& text, const char* name)
{
    auto* button = new QPushButton(text, this);
    button->setObjectName(QString::fromLatin1(name));
    button->setAutoDefault(false);
    return button;
}

void FindReplaceDialog::showTab(Tab tab, const QString& text)
{
    _tabs->setCurrentIndex(static_cast<int>(tab));
    if (!text.isEmpty())
        _find->setEditText(text);
    show();
    raise();
    activateWindow();
    _find->setFocus();
    _find->lineEdit()->selectAll();
}

FindReplaceDialog::Tab FindReplaceDialog::currentTab() const
{
    return static_cast<Tab>(_tabs->currentIndex());
}

void FindReplaceDialog::updateForTab()
{
    const Tab tab = currentTab();
    const bool replace = tab == Tab::Replace;
    const bool files = tab == Tab::FindInFiles;
    setWindowTitle(_tabs->tabText(_tabs->currentIndex()));
    _replaceLabel->setVisible(replace || files);
    _replace->setVisible(replace || files);
    for (QWidget* w : std::initializer_list<QWidget*>{_filtersLabel, _filters, _directoryLabel, _directory, _browse,
                                                      _recursive, _hidden, _followDoc, _findInFiles, _replaceInFiles})
        w->setVisible(files);
    _cancel->setVisible(files && _running);
    _wrapAround->setVisible(!files);
    _backward->setVisible(!files);
    _inSelection->setVisible(!files);
    _findNext->setVisible(!files);
    _count->setVisible(tab == Tab::Find);
    _findAllCurrent->setVisible(tab == Tab::Find);
    _findAllOpen->setVisible(tab == Tab::Find);
    _replaceOne->setVisible(replace);
    _replaceAll->setVisible(replace);
    _replaceAllOpen->setVisible(replace);
    adjustSize();
}

void FindReplaceDialog::updateModeDependentOptions()
{
    // N++: "palabra completa" no aplica a una regex y ". coincide con salto" solo a ella
    const bool regex = _regex->isChecked();
    _wholeWord->setEnabled(!regex);
    _dotNewline->setEnabled(regex);
}

SearchOptions FindReplaceDialog::options() const
{
    SearchOptions o;
    o.pattern = _find->currentText().toStdString();
    o.replacement = _replace->currentText().toStdString();
    o.mode = _regex->isChecked() ? SearchMode::Regex : (_extended->isChecked() ? SearchMode::Extended : SearchMode::Normal);
    o.matchCase = _matchCase->isChecked();
    o.wholeWord = _wholeWord->isChecked() && o.mode != SearchMode::Regex;
    o.wrapAround = _wrapAround->isChecked();
    o.searchUp = _backward->isChecked();
    o.dotMatchesNewline = _dotNewline->isChecked();
    o.inSelection = _inSelection->isChecked();
    return o;
}

void FindReplaceDialog::setOptions(const SearchOptions& o)
{
    _find->setEditText(QString::fromStdString(o.pattern));
    _replace->setEditText(QString::fromStdString(o.replacement));
    _normal->setChecked(o.mode == SearchMode::Normal);
    _extended->setChecked(o.mode == SearchMode::Extended);
    _regex->setChecked(o.mode == SearchMode::Regex);
    _matchCase->setChecked(o.matchCase);
    _wholeWord->setChecked(o.wholeWord);
    _wrapAround->setChecked(o.wrapAround);
    _backward->setChecked(o.searchUp);
    _dotNewline->setChecked(o.dotMatchesNewline);
    _inSelection->setChecked(o.inSelection);
}

QString FindReplaceDialog::directory() const { return _directory->currentText(); }
void FindReplaceDialog::setDirectory(const QString& dir) { _directory->setEditText(QDir::toNativeSeparators(dir)); }
QString FindReplaceDialog::filters() const { return _filters->currentText(); }
void FindReplaceDialog::setFilters(const QString& filters) { _filters->setEditText(filters); }
bool FindReplaceDialog::recursive() const { return _recursive->isChecked(); }
bool FindReplaceDialog::includeHidden() const { return _hidden->isChecked(); }
bool FindReplaceDialog::followCurrentDocument() const { return _followDoc->isChecked(); }

void FindReplaceDialog::setStatus(const QString& text, StatusKind kind)
{
    // Colores por defecto de "Find status: …" en stylers.model.xml
    const char* color = "#0000FF";
    if (kind == StatusKind::NotFound)
        color = "#FF0000";
    else if (kind == StatusKind::EndReached)
        color = "#008000";
    _status->setStyleSheet(QStringLiteral("color: %1;").arg(QLatin1String(color)));
    _status->setText(text);
}

QString FindReplaceDialog::statusText() const
{
    return _status->text();
}

void FindReplaceDialog::setFileSearchRunning(bool running)
{
    _running = running;
    for (QWidget* w : std::initializer_list<QWidget*>{_findInFiles, _replaceInFiles, _findNext, _replaceOne, _replaceAll,
                                                      _replaceAllOpen, _count, _findAllCurrent, _findAllOpen})
        w->setEnabled(!running);
    updateForTab();
}

void FindReplaceDialog::rememberCurrentTexts()
{
    remember(_find);
    if (currentTab() != Tab::Find)
        remember(_replace);
    if (currentTab() == Tab::FindInFiles) {
        remember(_filters);
        remember(_directory);
    }
}

QStringList FindReplaceDialog::findHistory() const { return items(_find); }
QStringList FindReplaceDialog::replaceHistory() const { return items(_replace); }
QStringList FindReplaceDialog::filterHistory() const { return items(_filters); }
QStringList FindReplaceDialog::directoryHistory() const { return items(_directory); }

void FindReplaceDialog::setFindInFilesOptions(bool recursive, bool hidden, bool followCurrentDocument)
{
    _recursive->setChecked(recursive);
    _hidden->setChecked(hidden);
    _followDoc->setChecked(followCurrentDocument);
}

void FindReplaceDialog::setMonospacedFont(bool on)
{
    const QFont font = on ? QFontDatabase::systemFont(QFontDatabase::FixedFont) : QApplication::font(_find);
    for (QComboBox* combo : {_find, _replace})
        combo->setFont(font);
}

void FindReplaceDialog::setHistories(const QStringList& find, const QStringList& replace, const QStringList& filters,
                                     const QStringList& directories)
{
    auto fill = [](QComboBox* combo, const QStringList& list) {
        const QString current = combo->currentText();
        combo->clear();
        combo->addItems(list);
        combo->setEditText(list.isEmpty() ? current : list.first());
    };
    fill(_find, find);
    fill(_replace, replace);
    if (!filters.isEmpty())
        fill(_filters, filters);
    if (!directories.isEmpty())
        fill(_directory, directories);
}

} // namespace edita
