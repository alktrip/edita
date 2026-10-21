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

#include "ui/StyleConfiguratorDialog.h"

#include "syntax/LanguageRegistry.h"
#include "syntax/SyntaxManager.h"
#include "ui/Prompter.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace edita {

namespace {

// 0x00BBGGRR (Scintilla) ↔ QColor
QColor toQColor(int bgr)
{
    return QColor(bgr & 0xFF, (bgr >> 8) & 0xFF, (bgr >> 16) & 0xFF);
}

int toBgr(const QColor& c)
{
    return c.red() | (c.green() << 8) | (c.blue() << 16);
}

QIcon swatch(const QColor& color, bool enabled)
{
    QPixmap pixmap(40, 18);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QColor(0x80, 0x80, 0x80));
    p.setBrush(enabled ? color : QColor(Qt::transparent));
    p.drawRoundedRect(QRectF(0.5, 0.5, 39, 17), 3, 3);
    if (!enabled)
        p.drawLine(QPointF(3, 15), QPointF(37, 3));
    return QIcon(pixmap);
}

// Las del menú de N++ (IDC_FONTSIZE_COMBO): vacío = el del estilo por defecto
constexpr int kFontSizes[] = {5, 6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28};

} // namespace

StyleConfiguratorDialog::StyleConfiguratorDialog(SyntaxManager& syntax, Prompter& prompter, ThemeSwitcher switchTheme,
                                                 QWidget* parent)
    : QDialog(parent)
    , _syntax(syntax)
    , _prompter(prompter)
    , _switchTheme(std::move(switchTheme))
    , _working(syntax.styles())
    , _original(syntax.styles())
{
    setWindowTitle(tr("Style Configurator"));
    buildUi();
    fillThemes();
    fillLanguages();
    selectLanguage(-1);
    resize(sizeHint().expandedTo(QSize(900, 560)));
}

void StyleConfiguratorDialog::buildUi()
{
    _themes = new QComboBox;
    _themes->setObjectName(QStringLiteral("themes"));
    auto* themeRow = new QHBoxLayout;
    themeRow->addWidget(new QLabel(tr("Select theme:")));
    themeRow->addWidget(_themes, 1);

    _languages = new QListWidget;
    _languages->setObjectName(QStringLiteral("languages"));
    _styles = new QListWidget;
    _styles->setObjectName(QStringLiteral("styles"));
    auto* languageBox = new QVBoxLayout;
    languageBox->addWidget(new QLabel(tr("Language:")));
    languageBox->addWidget(_languages);
    auto* styleBox = new QVBoxLayout;
    styleBox->addWidget(new QLabel(tr("Style:")));
    styleBox->addWidget(_styles);

    _styleInfo = new QLabel;
    _styleInfo->setObjectName(QStringLiteral("styleInfo"));
    QFont bold = _styleInfo->font();
    bold.setBold(true);
    _styleInfo->setFont(bold);

    // Colores
    auto* colors = new QGroupBox(tr("Colour Style"));
    auto* colorForm = new QFormLayout(colors);
    _foreground = new QToolButton;
    _foreground->setObjectName(QStringLiteral("foreground"));
    _background = new QToolButton;
    _background->setObjectName(QStringLiteral("background"));
    for (QToolButton* button : {_foreground, _background}) {
        button->setIconSize(QSize(40, 18));
        button->setAutoRaise(false);
    }
    colorForm->addRow(tr("Foreground colour"), _foreground);
    colorForm->addRow(tr("Background colour"), _background);
    connect(_foreground, &QToolButton::clicked, this, [this] { pickColor(true); });
    connect(_background, &QToolButton::clicked, this, [this] { pickColor(false); });

    // Fuente
    auto* font = new QGroupBox(tr("Font Style"));
    auto* fontGrid = new QGridLayout(font);
    _fontName = new QComboBox;
    _fontName->setObjectName(QStringLiteral("fontName"));
    _fontName->addItem(QString());
    _fontName->addItems(QFontDatabase::families());
    _fontSize = new QComboBox;
    _fontSize->setObjectName(QStringLiteral("fontSize"));
    _fontSize->addItem(QString(), 0);
    for (const int size : kFontSizes)
        _fontSize->addItem(QString::number(size), size);
    _bold = new QCheckBox(tr("Bold"));
    _bold->setObjectName(QStringLiteral("bold"));
    _italic = new QCheckBox(tr("Italic"));
    _italic->setObjectName(QStringLiteral("italic"));
    _underline = new QCheckBox(tr("Underline"));
    _underline->setObjectName(QStringLiteral("underline"));
    fontGrid->addWidget(new QLabel(tr("Font name:")), 0, 0);
    fontGrid->addWidget(_fontName, 0, 1, 1, 3);
    fontGrid->addWidget(new QLabel(tr("Font size:")), 1, 0);
    fontGrid->addWidget(_fontSize, 1, 1);
    fontGrid->addWidget(_bold, 2, 0);
    fontGrid->addWidget(_italic, 2, 1);
    fontGrid->addWidget(_underline, 2, 2);
    connect(_fontName, &QComboBox::currentTextChanged, this, [this](const QString& name) {
        if (Style* s = current(); s && !_updating) {
            s->fontName = name.toStdString();
            changed();
        }
    });
    connect(_fontSize, &QComboBox::currentIndexChanged, this, [this] {
        if (Style* s = current(); s && !_updating) {
            s->fontSize = _fontSize->currentData().toInt();
            changed();
        }
    });
    connect(_bold, &QCheckBox::toggled, this, [this](bool on) { setFontStyleBit(kFontStyleBold, on); });
    connect(_italic, &QCheckBox::toggled, this, [this](bool on) { setFontStyleBit(kFontStyleItalic, on); });
    connect(_underline, &QCheckBox::toggled, this, [this](bool on) { setFontStyleBit(kFontStyleUnderline, on); });

    // Global override (IDC_GLOBAL_*_CHECK)
    _override = new QGroupBox(tr("Global override"));
    auto* overrideBox = new QVBoxLayout(_override);
    const QString overrideTexts[] = {
        tr("Force foreground color for all styles"), tr("Force background color for all styles"),
        tr("Force font choice for all styles"),      tr("Force font size choice for all styles"),
        tr("Force bold choice for all styles"),      tr("Force italic choice for all styles"),
        tr("Force underline choice for all styles"),
    };
    static constexpr bool GlobalOverride::*kFlags[] = {
        &GlobalOverride::foreground, &GlobalOverride::background, &GlobalOverride::font, &GlobalOverride::fontSize,
        &GlobalOverride::bold,       &GlobalOverride::italic,     &GlobalOverride::underline,
    };
    for (int i = 0; i < 7; ++i) {
        _overrideChecks[i] = new QCheckBox(overrideTexts[i]);
        _overrideChecks[i]->setObjectName(QStringLiteral("override%1").arg(i));
        overrideBox->addWidget(_overrideChecks[i]);
        connect(_overrideChecks[i], &QCheckBox::toggled, this, [this, i](bool on) {
            if (_updating)
                return;
            _working.globalOverride().*kFlags[i] = on;
            changed();
        });
    }

    // Extensiones del lenguaje (las de langs.xml y las del usuario)
    _extensions = new QGroupBox(tr("File extensions"));
    auto* extForm = new QFormLayout(_extensions);
    _defaultExtensions = new QLabel;
    _defaultExtensions->setObjectName(QStringLiteral("defaultExtensions"));
    _defaultExtensions->setWordWrap(true);
    _userExtensions = new QLineEdit;
    _userExtensions->setObjectName(QStringLiteral("userExtensions"));
    extForm->addRow(tr("Default ext.:"), _defaultExtensions);
    extForm->addRow(tr("User ext.:"), _userExtensions);
    connect(_userExtensions, &QLineEdit::textEdited, this, [this](const QString& text) {
        if (_lexerIndex >= 0) {
            _working.lexers()[static_cast<std::size_t>(_lexerIndex)].userExtensions = text.toStdString();
            changed();
        }
    });

    // Palabras clave
    _keywords = new QGroupBox(tr("Keywords"));
    auto* kwBox = new QVBoxLayout(_keywords);
    _defaultKeywords = new QPlainTextEdit;
    _defaultKeywords->setObjectName(QStringLiteral("defaultKeywords"));
    _defaultKeywords->setReadOnly(true);
    _userKeywords = new QPlainTextEdit;
    _userKeywords->setObjectName(QStringLiteral("userKeywords"));
    kwBox->addWidget(new QLabel(tr("Default keywords")));
    kwBox->addWidget(_defaultKeywords);
    kwBox->addWidget(new QLabel(tr("User-defined keywords")));
    kwBox->addWidget(_userKeywords);
    connect(_userKeywords, &QPlainTextEdit::textChanged, this, [this] {
        if (Style* s = current(); s && !_updating) {
            s->keywords = _userKeywords->toPlainText().toStdString();
            changed();
        }
    });

    auto* right = new QVBoxLayout;
    right->addWidget(_styleInfo);
    auto* top = new QHBoxLayout;
    top->addWidget(colors);
    top->addWidget(font, 1);
    right->addLayout(top);
    right->addWidget(_override);
    right->addWidget(_extensions);
    right->addWidget(_keywords, 1);
    right->addStretch();

    auto* body = new QHBoxLayout;
    body->addLayout(languageBox, 2);
    body->addLayout(styleBox, 2);
    body->addLayout(right, 5);

    auto* buttons = new QDialogButtonBox;
    QPushButton* save = buttons->addButton(tr("Save && Close"), QDialogButtonBox::AcceptRole);
    save->setObjectName(QStringLiteral("save"));
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { saveAndClose(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &StyleConfiguratorDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(themeRow);
    layout->addLayout(body, 1);
    layout->addWidget(buttons);

    connect(_languages, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!_updating)
            selectLanguage(row - 1);
    });
    connect(_styles, &QListWidget::currentRowChanged, this, [this](int row) {
        if (_updating || row < 0)
            return;
        _styleIndex = row;
        showStyle();
    });
    connect(_themes, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!_updating)
            switchTheme(index);
    });
}

void StyleConfiguratorDialog::fillThemes()
{
    _updating = true;
    _themes->clear();
    const QString active = _syntax.activeThemePath();
    for (const SyntaxManager::Theme& theme : _syntax.themes()) {
        _themes->addItem(theme.name, theme.path);
        // Un tema copiado a ~/.config/edita/themes tiene otra ruta pero el mismo nombre
        if (theme.path == active || (!active.isEmpty() && QFileInfo(theme.path).fileName() == QFileInfo(active).fileName()))
            _themes->setCurrentIndex(_themes->count() - 1);
    }
    _updating = false;
}

void StyleConfiguratorDialog::fillLanguages()
{
    _updating = true;
    _languages->clear();
    _languages->addItem(tr("Global Styles"));
    for (const LexerStyles& lexer : _working.lexers())
        _languages->addItem(QString::fromStdString(lexer.description.empty() ? lexer.name : lexer.description));
    _updating = false;
}

bool StyleConfiguratorDialog::selectLanguage(int lexerIndex)
{
    if (lexerIndex < -1 || lexerIndex >= static_cast<int>(_working.lexers().size()))
        return false;
    _lexerIndex = lexerIndex;
    _styleIndex = 0;
    _updating = true;
    _languages->setCurrentRow(lexerIndex + 1);
    _updating = false;
    fillStyles();
    showStyle();
    return true;
}

void StyleConfiguratorDialog::fillStyles()
{
    _updating = true;
    _styles->clear();
    const std::vector<Style>& styles =
        _lexerIndex < 0 ? _working.globalStyles() : _working.lexers()[static_cast<std::size_t>(_lexerIndex)].styles;
    for (const Style& style : styles)
        _styles->addItem(QString::fromStdString(style.name));
    _styles->setCurrentRow(_styleIndex);
    _updating = false;
}

bool StyleConfiguratorDialog::selectStyle(const QString& name)
{
    const auto items = _styles->findItems(name, Qt::MatchExactly);
    if (items.isEmpty())
        return false;
    _styles->setCurrentItem(items.first()); // currentRowChanged → showStyle
    return true;
}

Style* StyleConfiguratorDialog::current()
{
    std::vector<Style>& styles =
        _lexerIndex < 0 ? _working.globalStyles() : _working.lexers()[static_cast<std::size_t>(_lexerIndex)].styles;
    if (_styleIndex < 0 || _styleIndex >= static_cast<int>(styles.size()))
        return nullptr;
    return &styles[static_cast<std::size_t>(_styleIndex)];
}

const Style* StyleConfiguratorDialog::currentStyle() const
{
    return const_cast<StyleConfiguratorDialog*>(this)->current();
}

void StyleConfiguratorDialog::showStyle()
{
    const Style* style = current();
    _updating = true;
    const bool isGlobal = _lexerIndex < 0;
    const QString language = _languages->currentItem() ? _languages->currentItem()->text() : QString();
    _styleInfo->setText(style ? language + QStringLiteral(": ") + QString::fromStdString(style->name) : QString());

    // setVisualFromStyleList: los colores que el estilo no usa quedan desactivados
    const bool hasFg = style && style->fgColor != kStyleNotUsed;
    const bool hasBg = style && style->bgColor != kStyleNotUsed;
    _foreground->setEnabled(hasFg);
    _background->setEnabled(hasBg);
    _foreground->setIcon(swatch(hasFg ? toQColor(style->fgColor) : QColor(), hasFg));
    _background->setIcon(swatch(hasBg ? toQColor(style->bgColor) : QColor(), hasBg));

    const QString fontName = style ? QString::fromStdString(style->fontName) : QString();
    int fontIndex = _fontName->findText(fontName);
    if (fontIndex < 0) {
        _fontName->addItem(fontName); // una fuente que no está instalada
        fontIndex = _fontName->count() - 1;
    }
    _fontName->setCurrentIndex(fontIndex);
    const int sizeIndex = style ? _fontSize->findData(std::max(0, style->fontSize)) : 0;
    _fontSize->setCurrentIndex(std::max(0, sizeIndex));
    const int fontStyle = style && style->fontStyle != kStyleNotUsed ? style->fontStyle : 0;
    _bold->setChecked(fontStyle & kFontStyleBold);
    _italic->setChecked(fontStyle & kFontStyleItalic);
    _underline->setChecked(fontStyle & kFontStyleUnderline);

    // El Global override solo en su estilo
    const bool isOverride = isGlobal && style && style->name == "Global override";
    _override->setVisible(isOverride);
    const GlobalOverride& go = _working.globalOverride();
    const bool flags[] = {go.foreground, go.background, go.font, go.fontSize, go.bold, go.italic, go.underline};
    for (int i = 0; i < 7; ++i)
        _overrideChecks[i]->setChecked(flags[i]);

    // Extensiones y palabras clave del lenguaje (langs.xml + las del usuario)
    const LanguageDefinition* def = nullptr;
    if (!isGlobal) {
        const LexerStyles& lexer = _working.lexers()[static_cast<std::size_t>(_lexerIndex)];
        for (const LanguageDefinition& candidate : _syntax.languages().languages()) {
            if (candidate.name == lexer.name)
                def = &candidate;
        }
        _defaultExtensions->setText(def ? QString::fromStdString(def->extensions) : QString());
        _userExtensions->setText(QString::fromStdString(lexer.userExtensions));
    }
    _extensions->setVisible(!isGlobal);
    const bool hasKeywords = style && style->keywordClass != kStyleNotUsed;
    _keywords->setVisible(hasKeywords);
    if (hasKeywords) {
        _defaultKeywords->setPlainText(def && style->keywordClass < kKeywordListCount
                                           ? QString::fromStdString(def->keywords[static_cast<std::size_t>(style->keywordClass)])
                                           : QString());
        _userKeywords->setPlainText(QString::fromStdString(style->keywords));
    }
    _updating = false;
}

void StyleConfiguratorDialog::changed()
{
    _dirty = true;
    _syntax.previewStyles(_working); // se ve al momento en el editor
}

void StyleConfiguratorDialog::setFontStyleBit(int bit, bool on)
{
    Style* s = current();
    if (!s || _updating)
        return;
    int value = s->fontStyle == kStyleNotUsed ? 0 : s->fontStyle;
    value = on ? (value | bit) : (value & ~bit);
    s->fontStyle = value;
    changed();
}

void StyleConfiguratorDialog::setStyleColor(bool foreground, const QColor& color)
{
    Style* s = current();
    if (!s || !color.isValid())
        return;
    (foreground ? s->fgColor : s->bgColor) = toBgr(color);
    showStyle();
    changed();
}

void StyleConfiguratorDialog::pickColor(bool foreground)
{
    const Style* s = current();
    if (!s)
        return;
    const QColor initial = toQColor(foreground ? s->fgColor : s->bgColor);
    const QColor color = QColorDialog::getColor(initial, this, foreground ? tr("Foreground colour") : tr("Background colour"));
    if (color.isValid())
        setStyleColor(foreground, color);
}

void StyleConfiguratorDialog::switchTheme(int index)
{
    // switchToTheme: avisa si hay cambios sin guardar
    if (_dirty
        && _prompter.confirm(this, tr("Style Configurator"),
                             tr("Unsaved changes are about to be discarded!\nDo you want to save your changes before "
                                "switching themes?"))) {
        QString error;
        if (!_syntax.saveStyles(_working, &error))
            _prompter.warn(this, tr("Style Configurator"), error);
    }
    if (_switchTheme)
        _switchTheme(_themes->itemData(index).toString());
    _working = _syntax.styles();
    _original = _syntax.styles();
    _dirty = false;
    const int lexer = std::min(_lexerIndex, static_cast<int>(_working.lexers().size()) - 1);
    fillLanguages();
    selectLanguage(lexer);
}

bool StyleConfiguratorDialog::saveAndClose()
{
    QString error;
    if (_dirty && !_syntax.saveStyles(_working, &error)) {
        _prompter.warn(this, tr("Style Configurator"), error);
        return false;
    }
    _dirty = false;
    accept();
    return true;
}

void StyleConfiguratorDialog::reject()
{
    // Cancelar: vuelven los estilos de antes
    if (_dirty)
        _syntax.previewStyles(_original);
    _dirty = false;
    QDialog::reject();
}

} // namespace edita
