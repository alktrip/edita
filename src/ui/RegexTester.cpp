// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/RegexTester.h"

#include "search/SearchEngine.h"
#include "syntax/DesktopFonts.h"
#include "syntax/SyntaxManager.h"

#include <ScintillaEditBase.h>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace edita {

namespace {

// Indicadores del texto de prueba (es una vista propia: no chocan con los del editor)
constexpr int kIndicatorMatchA = 8;  // las coincidencias, alternando dos colores
constexpr int kIndicatorMatchB = 9;
constexpr int kIndicatorGroup = 10;  // el texto de los grupos: 10..15, uno por color
constexpr int kGroupColors = 6;
constexpr int kMaxListed = 1000;      // coincidencias en la lista y resaltadas
constexpr qsizetype kMaxPreview = 4 * 1024 * 1024;

constexpr int kRoleStart = Qt::UserRole;
constexpr int kRoleEnd = Qt::UserRole + 1;

QColor fromScintilla(sptr_t color)
{
    return QColor(static_cast<int>(color & 0xFF), static_cast<int>((color >> 8) & 0xFF), static_cast<int>((color >> 16) & 0xFF));
}

sptr_t toScintilla(const QColor& color)
{
    return color.red() | (color.green() << 8) | (color.blue() << 16);
}

QToolButton* toggleButton(QWidget* parent, const QString& text, const QString& tip, const char* name)
{
    auto* button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(tip);
    button->setCheckable(true);
    button->setAutoRaise(true);
    button->setObjectName(QString::fromLatin1(name));
    return button;
}

// Lo que se ve de un trozo de texto en la lista: en una línea y no muy largo
QString displayText(const std::string& utf8)
{
    if (utf8.empty())
        return RegexTesterPanel::tr("(empty)");
    QString text = QString::fromUtf8(utf8.data(), static_cast<qsizetype>(std::min<std::size_t>(utf8.size(), 600)));
    text.replace(QLatin1String("\r\n"), QStringLiteral("⏎"));
    text.replace(QLatin1Char('\n'), QStringLiteral("⏎"));
    text.replace(QLatin1Char('\r'), QStringLiteral("⏎"));
    text.replace(QLatin1Char('\t'), QStringLiteral("→"));
    if (text.size() > 200)
        text = text.left(200) + QStringLiteral("…");
    return text;
}

QIcon colorDot(const QColor& color)
{
    QPixmap pixmap(12, 12);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(1, 1, 10, 10), 3, 3);
    return QIcon(pixmap);
}

} // namespace

RegexTesterPanel::RegexTesterPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("regexTester"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // La expresión, con sus opciones y la referencia rápida
    auto* patternRow = new QHBoxLayout;
    patternRow->setSpacing(2);
    _pattern = new QLineEdit(this);
    _pattern->setObjectName(QStringLiteral("regexTesterPattern"));
    _pattern->setPlaceholderText(tr("Regular expression"));
    _pattern->setFont(desktopfonts::fixedFont());
    _pattern->setClearButtonEnabled(true);
    patternRow->addWidget(_pattern, 1);
    _matchCase = toggleButton(this, QStringLiteral("Aa"), tr("Match case"), "regexTesterMatchCase");
    _dotAll = toggleButton(this, QStringLiteral(".⏎"), tr(". matches newline"), "regexTesterDotAll");
    patternRow->addWidget(_matchCase);
    patternRow->addWidget(_dotAll);
    patternRow->addWidget(referenceButton());
    auto* fromEditor = new QPushButton(tr("From Editor"), this);
    fromEditor->setObjectName(QStringLiteral("regexTesterFromEditor"));
    fromEditor->setToolTip(tr("Use the selection (or the whole document) as the test text"));
    auto* useInFind = new QPushButton(tr("Use in Find…"), this);
    useInFind->setObjectName(QStringLiteral("regexTesterUseInFind"));
    useInFind->setToolTip(tr("Open the Find dialog with this expression"));
    patternRow->addWidget(fromEditor);
    patternRow->addWidget(useInFind);
    layout->addLayout(patternRow);

    auto* replaceRow = new QHBoxLayout;
    replaceRow->setSpacing(2);
    _replacement = new QLineEdit(this);
    _replacement->setObjectName(QStringLiteral("regexTesterReplacement"));
    _replacement->setPlaceholderText(tr("Replace with ($1, $&, \\U$1\\E…) to preview the result"));
    _replacement->setFont(desktopfonts::fixedFont());
    _replacement->setClearButtonEnabled(true);
    replaceRow->addWidget(_replacement, 1);
    _status = new QLabel(this);
    _status->setObjectName(QStringLiteral("regexTesterStatus"));
    _status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    replaceRow->addWidget(_status);
    layout->addLayout(replaceRow);

    // El texto de prueba a la izquierda; las coincidencias y la vista previa a la derecha
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    _sample = new ScintillaEditBase(splitter);
    _sample->setObjectName(QStringLiteral("regexTesterSample"));
    configureSample();
    auto* right = new QSplitter(Qt::Vertical, splitter);
    _tree = new QTreeWidget(right);
    _tree->setObjectName(QStringLiteral("regexTesterMatches"));
    _tree->setColumnCount(3);
    _tree->setHeaderLabels({tr("Match"), tr("Text"), tr("Position")});
    _tree->setUniformRowHeights(true);
    _tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    _tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _tree->header()->setStretchLastSection(false);
    _preview = new QPlainTextEdit(right);
    _preview->setObjectName(QStringLiteral("regexTesterPreview"));
    _preview->setReadOnly(true);
    _preview->setFont(desktopfonts::fixedFont());
    _preview->setLineWrapMode(QPlainTextEdit::NoWrap);
    _preview->setPlaceholderText(tr("Type a replacement to preview the result"));
    _preview->hide();
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    _timer.setSingleShot(true);
    _timer.setInterval(150);
    connect(&_timer, &QTimer::timeout, this, &RegexTesterPanel::evaluate);
    connect(_pattern, &QLineEdit::textChanged, this, [this] { _timer.start(); });
    connect(_replacement, &QLineEdit::textChanged, this, [this] { _timer.start(); });
    connect(_matchCase, &QToolButton::toggled, this, &RegexTesterPanel::evaluate);
    connect(_dotAll, &QToolButton::toggled, this, &RegexTesterPanel::evaluate);
    // Solo lo que cambia el texto (los indicadores también avisan de cambios)
    connect(_sample, &ScintillaEditBase::modified, this, [this](Scintilla::ModificationFlags type) {
        if (static_cast<int>(type) & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
            _timer.start();
    });
    connect(_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        if (item)
            selectInSample(item->data(0, kRoleStart).value<qint64>(), item->data(0, kRoleEnd).value<qint64>());
    });
    connect(fromEditor, &QPushButton::clicked, this, &RegexTesterPanel::sampleFromEditorRequested);
    connect(useInFind, &QPushButton::clicked, this, [this] {
        if (!_pattern->text().isEmpty())
            emit useInFindRequested(options());
    });
    evaluate();
}

void RegexTesterPanel::configureSample()
{
    _sample->send(SCI_SETCODEPAGE, SC_CP_UTF8);
    _sample->send(SCI_SETEOLMODE, SC_EOL_LF);
    _sample->send(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    _sample->send(SCI_SETMARGINWIDTHN, 1, 4);
    _sample->send(SCI_SETMARGINWIDTHN, 2, 0);
    _sample->send(SCI_SETSCROLLWIDTHTRACKING, 1);
    _sample->send(SCI_SETSCROLLWIDTH, 1);
    _sample->send(SCI_SETCHANGEHISTORY, SC_CHANGE_HISTORY_DISABLED);
}

void RegexTesterPanel::applyStyles(const SyntaxManager& syntax)
{
    syntax.apply(*_sample, LangType::Text);
    configureSample();
    _dark = fromScintilla(_sample->send(SCI_STYLEGETBACK, STYLE_DEFAULT)).lightness() < 128;
    // Las coincidencias con relleno; el texto de los grupos, cada uno de su color
    const QColor matchColors[2] = {_dark ? QColor(0x3d, 0x8b, 0xfd) : QColor(0x1a, 0x73, 0xe8),
                                   _dark ? QColor(0x2e, 0xc4, 0xb6) : QColor(0x00, 0x96, 0x88)};
    for (int i = 0; i < 2; ++i) {
        const auto id = static_cast<uptr_t>(kIndicatorMatchA + i);
        _sample->send(SCI_INDICSETSTYLE, id, INDIC_FULLBOX);
        _sample->send(SCI_INDICSETFORE, id, toScintilla(matchColors[i]));
        _sample->send(SCI_INDICSETALPHA, id, _dark ? 70 : 45);
        _sample->send(SCI_INDICSETOUTLINEALPHA, id, 160);
        _sample->send(SCI_INDICSETUNDER, id, 1);
    }
    for (int g = 0; g < kGroupColors; ++g) {
        const auto id = static_cast<uptr_t>(kIndicatorGroup + g);
        _sample->send(SCI_INDICSETSTYLE, id, INDIC_TEXTFORE);
        _sample->send(SCI_INDICSETFORE, id, toScintilla(groupColor(g + 1)));
    }
    // El margen con los números, a la medida
    _sample->send(SCI_SETMARGINWIDTHN, 0,
                  _sample->send(SCI_TEXTWIDTH, STYLE_LINENUMBER, reinterpret_cast<sptr_t>("_9999")));
    evaluate();
}

QColor RegexTesterPanel::groupColor(int number) const
{
    static const QColor light[kGroupColors] = {QColor(0xd9, 0x30, 0x25), QColor(0xe3, 0x74, 0x00), QColor(0x18, 0x80, 0x38),
                                               QColor(0x93, 0x34, 0xe6), QColor(0xc2, 0x18, 0x5b), QColor(0x00, 0x7b, 0x83)};
    static const QColor dark[kGroupColors] = {QColor(0xff, 0x6b, 0x6b), QColor(0xff, 0xa9, 0x4d), QColor(0x69, 0xdb, 0x7c),
                                              QColor(0xc0, 0x84, 0xfc), QColor(0xf7, 0x83, 0xac), QColor(0x4f, 0xd1, 0xc5)};
    const int index = (std::max(1, number) - 1) % kGroupColors;
    return _dark ? dark[index] : light[index];
}

void RegexTesterPanel::setSample(const QString& text)
{
    const QByteArray utf8 = text.toUtf8();
    _sample->send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(utf8.constData()));
    _sample->send(SCI_EMPTYUNDOBUFFER);
    evaluate();
}

QString RegexTesterPanel::sample() const
{
    const sptr_t length = _sample->send(SCI_GETLENGTH);
    QByteArray text(static_cast<qsizetype>(length) + 1, '\0');
    _sample->send(SCI_GETTEXT, static_cast<uptr_t>(length + 1), reinterpret_cast<sptr_t>(text.data()));
    text.resize(static_cast<qsizetype>(length));
    return QString::fromUtf8(text);
}

void RegexTesterPanel::setPattern(const QString& pattern)
{
    _pattern->setText(pattern);
    evaluate();
}

void RegexTesterPanel::setToggles(bool matchCase, bool dotMatchesNewline)
{
    const QSignalBlocker a(_matchCase);
    const QSignalBlocker b(_dotAll);
    _matchCase->setChecked(matchCase);
    _dotAll->setChecked(dotMatchesNewline);
    evaluate();
}

SearchOptions RegexTesterPanel::options() const
{
    SearchOptions o;
    o.pattern = _pattern->text().toStdString();
    o.replacement = _replacement->text().toStdString();
    o.mode = SearchMode::Regex;
    o.matchCase = _matchCase->isChecked();
    o.dotMatchesNewline = _dotAll->isChecked();
    o.wrapAround = true;
    return o;
}

QString RegexTesterPanel::statusText() const
{
    return _status->text();
}

void RegexTesterPanel::clearHighlights()
{
    const sptr_t length = _sample->send(SCI_GETLENGTH);
    for (int id = kIndicatorMatchA; id < kIndicatorGroup + kGroupColors; ++id) {
        _sample->send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(id));
        _sample->send(SCI_INDICATORCLEARRANGE, 0, length);
    }
}

void RegexTesterPanel::evaluate()
{
    _timer.stop();
    clearHighlights();
    _matches.clear();
    _count = 0;
    _error.clear();
    const QString pattern = _pattern->text();
    auto setStatus = [this](const QString& text, bool error) {
        _status->setText(text);
        _status->setStyleSheet(error ? QStringLiteral("color: #d93025;") : QString());
    };
    if (pattern.isEmpty()) {
        setStatus(tr("Type a regular expression"), false);
        fillTree();
        updatePreview();
        return;
    }

    // Las coincidencias con el mismo motor que Buscar; los grupos, justo después de cada una
    const SearchOptions o = options();
    ViewSearchTarget target(*_sample);
    const std::vector<QString> names = groupNames(pattern);
    const RangeResult result = processRange(target, o, RangeOperation::FindAll, 0, target.length(), [&](SciPos start, SciPos end) {
        if (static_cast<int>(_matches.size()) >= kMaxListed)
            return;
        Match match{start, end, {}};
        const auto groups = SearchTarget::lastMatchGroups();
        for (std::size_t i = 1; i < groups.size(); ++i)
            match.groups.push_back({static_cast<int>(i), i < names.size() ? names[i] : QString(), groups[i].first, groups[i].second});
        _matches.push_back(std::move(match));
    });
    if (result.invalidRegex) {
        _matches.clear();
        _error = QString::fromStdString(result.regexError);
        setStatus(tr("Invalid regular expression: %1").arg(_error), true);
        fillTree();
        updatePreview();
        return;
    }
    _count = result.count;

    for (std::size_t i = 0; i < _matches.size(); ++i) {
        const Match& m = _matches[i];
        if (m.end > m.start) {
            _sample->send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(i % 2 == 0 ? kIndicatorMatchA : kIndicatorMatchB));
            _sample->send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(m.start), m.end - m.start);
        }
        for (const Group& g : m.groups) {
            if (g.start < 0 || g.end <= g.start)
                continue;
            _sample->send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(kIndicatorGroup + (g.number - 1) % kGroupColors));
            _sample->send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(g.start), g.end - g.start);
        }
    }

    const int groupCount = _matches.empty() ? static_cast<int>(names.size()) - 1 : static_cast<int>(_matches.front().groups.size());
    QString status = _count == 0 ? tr("No matches") : tr("%n match(es)", nullptr, _count);
    if (groupCount > 0)
        status += QStringLiteral(" · ") + tr("%n group(s)", nullptr, groupCount);
    if (_count > kMaxListed)
        status += QStringLiteral(" · ") + tr("first %1 listed").arg(kMaxListed);
    setStatus(status, false);
    fillTree();
    updatePreview();
}

void RegexTesterPanel::fillTree()
{
    _tree->clear();
    auto position = [this](SciPos pos) {
        const sptr_t line = _sample->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos));
        const sptr_t column = _sample->send(SCI_COUNTCHARACTERS, static_cast<uptr_t>(_sample->send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line))), pos);
        return tr("Ln %1, Col %2").arg(line + 1).arg(column + 1);
    };
    ViewSearchTarget target(*_sample);
    QList<QTreeWidgetItem*> items;
    for (std::size_t i = 0; i < _matches.size(); ++i) {
        const Match& m = _matches[i];
        auto* item = new QTreeWidgetItem({tr("Match %1").arg(i + 1), displayText(target.text(m.start, m.end)), position(m.start)});
        item->setData(0, kRoleStart, static_cast<qint64>(m.start));
        item->setData(0, kRoleEnd, static_cast<qint64>(m.end));
        for (const Group& g : m.groups) {
            const QString label = g.name.isEmpty() ? tr("Group %1").arg(g.number) : tr("Group %1 (%2)").arg(g.number).arg(g.name);
            const bool matched = g.start >= 0;
            auto* child = new QTreeWidgetItem(item, {label, matched ? displayText(target.text(g.start, g.end)) : tr("(not matched)"),
                                                     matched ? position(g.start) : QString()});
            child->setIcon(0, colorDot(groupColor(g.number)));
            child->setForeground(1, groupColor(g.number));
            child->setData(0, kRoleStart, static_cast<qint64>(matched ? g.start : m.start));
            child->setData(0, kRoleEnd, static_cast<qint64>(matched ? g.end : m.start));
        }
        items.append(item);
    }
    _tree->addTopLevelItems(items);
    // Con pocas coincidencias, los grupos a la vista
    if (items.size() <= 20)
        _tree->expandAll();
}

void RegexTesterPanel::updatePreview()
{
    const bool show = !_replacement->text().isEmpty();
    _preview->setVisible(show);
    if (!show)
        return;
    if (!_error.isEmpty() || _pattern->text().isEmpty()) {
        _preview->clear();
        return;
    }
    const sptr_t length = _sample->send(SCI_GETLENGTH);
    if (length > kMaxPreview) {
        _preview->setPlainText(tr("The test text is too large to preview"));
        return;
    }
    const auto* text = reinterpret_cast<const char*>(_sample->send(SCI_GETCHARACTERPOINTER));
    DocumentSearchTarget doc(std::string_view(text, static_cast<std::size_t>(length)));
    processRange(doc, options(), RangeOperation::ReplaceAll, 0, doc.length());
    const std::string_view replaced = doc.allText();
    _preview->setPlainText(QString::fromUtf8(replaced.data(), static_cast<qsizetype>(replaced.size())));
}

void RegexTesterPanel::selectInSample(SciPos start, SciPos end)
{
    _sample->send(SCI_SETSEL, static_cast<uptr_t>(start), end);
    _sample->send(SCI_SCROLLRANGE, static_cast<uptr_t>(end), start);
}

// Referencia rápida: elegir un elemento lo escribe en la expresión (o en el reemplazo).
// Si hay texto seleccionado, los grupos lo envuelven
QToolButton* RegexTesterPanel::referenceButton()
{
    struct Token
    {
        const char* text;
        int cursorBack; // dónde queda el cursor (desde el final)
        const char* description;
    };
    static constexpr Token kPattern[] = {
        {".", 0, QT_TR_NOOP("Any character")},
        {"\\d", 0, QT_TR_NOOP("Digit")},
        {"\\w", 0, QT_TR_NOOP("Word character (letter, digit or _)")},
        {"\\s", 0, QT_TR_NOOP("Whitespace")},
        {"\\b", 0, QT_TR_NOOP("Word boundary")},
        {"^", 0, QT_TR_NOOP("Start of line")},
        {"$", 0, QT_TR_NOOP("End of line")},
        {"[]", 1, QT_TR_NOOP("Any of these characters, e.g. [a-z0-9]")},
        {"[^]", 1, QT_TR_NOOP("Any character except these")},
        {"|", 0, QT_TR_NOOP("Either the left or the right side")},
        {nullptr, 0, nullptr},
        {"*", 0, QT_TR_NOOP("Zero or more times")},
        {"+", 0, QT_TR_NOOP("One or more times")},
        {"?", 0, QT_TR_NOOP("Optional (zero or one time)")},
        {"{2,4}", 0, QT_TR_NOOP("Between 2 and 4 times")},
        {"*?", 0, QT_TR_NOOP("As few times as possible (lazy)")},
        {nullptr, 0, nullptr},
        {"()", 1, QT_TR_NOOP("Capturing group")},
        {"(?<name>)", 1, QT_TR_NOOP("Named group")},
        {"(?:)", 1, QT_TR_NOOP("Group without capturing")},
        {"\\1", 0, QT_TR_NOOP("The text of group 1 again")},
        {"(?=)", 1, QT_TR_NOOP("Followed by (lookahead)")},
        {"(?!)", 1, QT_TR_NOOP("Not followed by")},
        {"(?<=)", 1, QT_TR_NOOP("Preceded by (lookbehind)")},
        {"(?<!)", 1, QT_TR_NOOP("Not preceded by")},
    };
    static constexpr Token kReplacement[] = {
        {"$1", 0, QT_TR_NOOP("Group 1")},
        {"$+{name}", 0, QT_TR_NOOP("Named group")},
        {"$&", 0, QT_TR_NOOP("The whole match")},
        {"\\U\\E", 2, QT_TR_NOOP("Uppercase until \\E")},
        {"\\L\\E", 2, QT_TR_NOOP("Lowercase until \\E")},
        {"\\n", 0, QT_TR_NOOP("New line")},
    };
    auto* button = new QToolButton(this);
    button->setObjectName(QStringLiteral("regexTesterReference"));
    button->setText(QStringLiteral("?"));
    button->setToolTip(tr("Quick reference"));
    button->setAutoRaise(true);
    button->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(button);
    auto insert = [](QLineEdit* field, const QString& token, int cursorBack) {
        const QString selected = field->selectedText();
        const QString before = token.left(token.size() - cursorBack);
        const QString after = token.right(cursorBack);
        field->insert(before + (cursorBack > 0 ? selected : QString()) + after);
        field->cursorBackward(false, static_cast<int>(after.size()));
        field->setFocus();
    };
    auto addTokens = [&](auto& tokens, QLineEdit* field) {
        for (const Token& t : tokens) {
            if (!t.text) {
                menu->addSeparator();
                continue;
            }
            const QString token = QString::fromLatin1(t.text);
            QAction* action = menu->addAction(token + QStringLiteral("\t") + tr(t.description));
            connect(action, &QAction::triggered, this, [insert, field, token, back = t.cursorBack] { insert(field, token, back); });
        }
    };
    menu->addSection(tr("Expression"));
    addTokens(kPattern, _pattern);
    QAction* opposite = menu->addAction(tr("\\D \\W \\S\tThe opposite of \\d \\w \\s"));
    opposite->setEnabled(false);
    menu->addSection(tr("Replacement"));
    addTokens(kReplacement, _replacement);
    button->setMenu(menu);
    return button;
}

std::vector<QString> RegexTesterPanel::groupNames(const QString& p)
{
    std::vector<QString> names{QString()};
    bool inClass = false;
    for (qsizetype i = 0; i < p.size(); ++i) {
        const QChar c = p[i];
        if (c == QLatin1Char('\\')) {
            if (i + 1 < p.size() && p[i + 1] == QLatin1Char('Q')) { // \Q…\E: literal
                const qsizetype e = p.indexOf(QLatin1String("\\E"), i + 2);
                i = e < 0 ? p.size() : e + 1;
            } else {
                ++i;
            }
            continue;
        }
        if (inClass) {
            if (c == QLatin1Char(']'))
                inClass = false;
            continue;
        }
        if (c == QLatin1Char('[')) {
            inClass = true;
            if (i + 1 < p.size() && p[i + 1] == QLatin1Char('^'))
                ++i;
            if (i + 1 < p.size() && p[i + 1] == QLatin1Char(']')) // []…] : el ] es literal
                ++i;
            continue;
        }
        if (c != QLatin1Char('('))
            continue;
        if (i + 1 >= p.size() || (p[i + 1] != QLatin1Char('?') && p[i + 1] != QLatin1Char('*'))) {
            names.emplace_back();
            continue;
        }
        if (p[i + 1] == QLatin1Char('*'))
            continue;
        // (?<nombre>…) (?'nombre'…) (?P<nombre>…); no (?<=…) ni (?<!…)
        qsizetype k = i + 2;
        if (k < p.size() && p[k] == QLatin1Char('P'))
            ++k;
        if (k >= p.size() || (p[k] != QLatin1Char('<') && p[k] != QLatin1Char('\'')))
            continue;
        if (k + 1 < p.size() && (p[k + 1] == QLatin1Char('=') || p[k + 1] == QLatin1Char('!')))
            continue;
        const QChar close = p[k] == QLatin1Char('<') ? QLatin1Char('>') : QLatin1Char('\'');
        const qsizetype e = p.indexOf(close, k + 1);
        if (e > k + 1)
            names.push_back(p.mid(k + 1, e - k - 1));
    }
    return names;
}

} // namespace edita
