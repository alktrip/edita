// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/ManualWindow.h"

#include "core/Paths.h"
#include "ui/Appearance.h"
#include "ui/Icons.h"
#include "ui/Translations.h"

#include <md4c-html.h>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QRegularExpression>
#include <QScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace edita {

namespace {

constexpr int kMaxImageWidth = 860;

const QRegularExpression& headingPattern()
{
    static const QRegularExpression re(QStringLiteral("^ {0,3}(#{1,6})[ \\t]+(.*?)(?:[ \\t]+#+)?[ \\t]*$"));
    return re;
}

const QRegularExpression& fencePattern()
{
    static const QRegularExpression re(QStringLiteral("^ {0,3}(```|~~~)"));
    return re;
}

// El texto de un encabezado sin la sintaxis de Markdown
QString headingText(const QString& raw)
{
    return Manual::plainText(raw).simplified();
}

QString escapeMarkdown(const QString& text)
{
    QString out;
    out.reserve(text.size() + 8);
    for (const QChar c : text) {
        if (QStringLiteral("\\`*_[]<>#|").contains(c))
            out += QLatin1Char('\\');
        out += c;
    }
    return out;
}

void appendHtml(const MD_CHAR* text, MD_SIZE size, void* userdata)
{
    static_cast<QByteArray*>(userdata)->append(text, static_cast<qsizetype>(size));
}

QColor mix(const QColor& a, const QColor& b, double amount)
{
    return QColor::fromRgbF(static_cast<float>(a.redF() * (1 - amount) + b.redF() * amount),
                            static_cast<float>(a.greenF() * (1 - amount) + b.greenF() * amount),
                            static_cast<float>(a.blueF() * (1 - amount) + b.blueF() * amount));
}

// Un apartado del manual (de un encabezado al siguiente), para buscar
struct Section
{
    QString heading;
    QString anchor;
    QString body;
};

std::vector<Section> sectionsOf(const Manual::Chapter& chapter)
{
    std::vector<Section> sections(1);
    sections.front().heading = chapter.title;
    size_t next = 0;
    bool fenced = false;
    QString body;
    const auto lines = chapter.markdown.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (fencePattern().match(line).hasMatch()) {
            fenced = !fenced;
            continue;
        }
        if (!fenced && headingPattern().match(line).hasMatch() && next < chapter.headings.size()) {
            const Manual::Heading& h = chapter.headings[next++];
            if (next == 1) { // el título del capítulo: su apartado es el del principio
                sections.front().anchor = h.anchor;
                continue;
            }
            sections.push_back({h.text, h.anchor, {}});
            continue;
        }
        sections.back().body += line + QLatin1Char('\n');
    }
    for (Section& s : sections)
        s.body = Manual::plainText(s.body).simplified();
    return sections;
}

// Un resultado de la búsqueda: dónde (en negrita) y un extracto atenuado de dos líneas
class HitDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    static constexpr int kExcerptRole = Qt::UserRole + 2;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const QString where = opt.text;
        opt.text.clear();
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
        const QRect area = opt.rect.adjusted(6, 4, -6, -4);
        const bool selected = opt.state & QStyle::State_Selected;
        const QPalette::ColorRole role = selected ? QPalette::HighlightedText : QPalette::Text;
        painter->save();
        QFont bold = opt.font;
        bold.setWeight(QFont::DemiBold);
        painter->setFont(bold);
        painter->setPen(opt.palette.color(role));
        const QFontMetrics boldMetrics(bold);
        painter->drawText(area.left(), area.top(), area.width(), boldMetrics.height(), Qt::AlignLeft | Qt::AlignVCenter,
                          boldMetrics.elidedText(where, Qt::ElideRight, area.width()));
        const QString excerpt = index.data(kExcerptRole).toString();
        if (!excerpt.isEmpty()) {
            painter->setFont(opt.font);
            const QColor text = opt.palette.color(role);
            painter->setPen(selected ? text : mix(opt.palette.color(QPalette::Base), text, 0.7));
            const QFontMetrics metrics(opt.font);
            const QRect rest(area.left(), area.top() + boldMetrics.height() + 1, area.width(), metrics.lineSpacing() * 2);
            painter->setClipRect(rest);
            painter->drawText(rest, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, excerpt);
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QFont bold = option.font;
        bold.setWeight(QFont::DemiBold);
        const int lines = index.data(kExcerptRole).toString().isEmpty() ? 0 : 2;
        return {option.rect.width(), QFontMetrics(bold).height() + QFontMetrics(option.font).lineSpacing() * lines + 10};
    }
};

} // namespace

// --- Manual ----------------------------------------------------------------------

QString Manual::directoryFor(const QString& language)
{
    QStringList candidates{language};
    const QString base = language.section(QRegularExpression(QStringLiteral("[_-]")), 0, 0);
    if (base != language)
        candidates << base;
    candidates << QStringLiteral("en");
    for (const QString& code : candidates) {
        if (code.isEmpty())
            continue;
        const QString dir = paths::findDataFile(QStringLiteral("manual/") + code);
        if (!dir.isEmpty() && !QDir(dir).entryList({QStringLiteral("*.md")}, QDir::Files).isEmpty())
            return dir;
    }
    return {};
}

bool Manual::load(const QString& directory, const Commands& commands)
{
    _directory = directory;
    _chapters.clear();
    _unresolved.clear();
    const QDir dir(directory);
    const QStringList files = dir.entryList({QStringLiteral("*.md")}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        Chapter chapter;
        chapter.fileName = name;
        chapter.markdown = expand(QString::fromUtf8(file.readAll()).remove(QLatin1Char('\r')), commands, &_unresolved);
        QHash<QString, int> used;
        bool fenced = false;
        const auto lines = chapter.markdown.split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            if (fencePattern().match(line).hasMatch()) {
                fenced = !fenced;
                continue;
            }
            const auto m = headingPattern().match(line);
            if (fenced || !m.hasMatch())
                continue;
            Heading h;
            h.level = static_cast<int>(m.capturedLength(1));
            h.text = headingText(m.captured(2));
            h.anchor = slug(h.text);
            // Como GitHub: el segundo «Opciones» es «opciones-1»
            const int n = used.value(h.anchor, 0);
            used.insert(h.anchor, n + 1);
            if (n > 0)
                h.anchor += QLatin1Char('-') + QString::number(n);
            chapter.headings.push_back(h);
        }
        chapter.title = chapter.headings.empty() ? QFileInfo(name).completeBaseName() : chapter.headings.front().text;
        _chapters.push_back(std::move(chapter));
    }
    _unresolved.removeDuplicates();
    return !_chapters.empty();
}

int Manual::indexOf(const QString& fileName) const
{
    for (size_t i = 0; i < _chapters.size(); ++i) {
        if (_chapters[i].fileName == fileName)
            return static_cast<int>(i);
    }
    return -1;
}

QString Manual::html(int index) const
{
    if (index < 0 || index >= static_cast<int>(_chapters.size()))
        return {};
    const Chapter& chapter = _chapters[static_cast<size_t>(index)];
    const QByteArray markdown = chapter.markdown.toUtf8();
    QByteArray out;
    md_html(markdown.constData(), static_cast<MD_SIZE>(markdown.size()), appendHtml, &out, MD_DIALECT_GITHUB, 0);
    QString html = QString::fromUtf8(out);

    // Un ancla delante de cada encabezado (en el mismo orden que en el Markdown)
    static const QRegularExpression openHeading(QStringLiteral("<h([1-6])>"));
    qsizetype pos = 0;
    for (const Heading& h : chapter.headings) {
        const auto m = openHeading.match(html, pos);
        if (!m.hasMatch())
            break;
        const QString anchor = QStringLiteral("<a name=\"%1\"></a>").arg(h.anchor);
        html.insert(m.capturedEnd(), anchor);
        pos = m.capturedEnd() + anchor.size();
    }
    html.replace(QStringLiteral("<table>"),
                 QStringLiteral("<table border=\"1\" cellspacing=\"0\" cellpadding=\"5\" width=\"100%\">"));

    // Las capturas grandes, a un ancho que quepa
    static const QRegularExpression image(QStringLiteral("<img src=\"([^\"]+)\""));
    const QDir dir(_directory);
    QString sized;
    qsizetype last = 0;
    for (auto it = image.globalMatch(html); it.hasNext();) {
        const auto m = it.next();
        sized += QStringView(html).mid(last, m.capturedEnd() - last);
        const QSize size = QImageReader(dir.filePath(m.captured(1))).size();
        if (size.width() > kMaxImageWidth)
            sized += QStringLiteral(" width=\"%1\"").arg(kMaxImageWidth);
        last = m.capturedEnd();
    }
    sized += QStringView(html).mid(last);
    return sized;
}

std::vector<Manual::Hit> Manual::search(const QString& query) const
{
    const QStringList terms = fold(query).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    std::vector<Hit> hits;
    if (terms.isEmpty())
        return hits;
    for (size_t c = 0; c < _chapters.size(); ++c) {
        for (const Section& section : sectionsOf(_chapters[c])) {
            const QString heading = fold(section.heading);
            const QString body = fold(section.body);
            int score = 0;
            bool all = true;
            for (const QString& term : terms) {
                const bool inHeading = heading.contains(term);
                const auto inBody = body.count(term);
                if (!inHeading && inBody == 0) {
                    all = false;
                    break;
                }
                score += (inHeading ? 10 : 0) + static_cast<int>(std::min<qsizetype>(inBody, 5));
            }
            if (!all)
                continue;
            Hit hit;
            hit.chapter = static_cast<int>(c);
            hit.anchor = section.anchor;
            hit.heading = section.heading;
            hit.score = score;
            const qsizetype at = body.indexOf(terms.front());
            if (at >= 0) {
                const qsizetype start = std::max<qsizetype>(0, at - 50);
                hit.excerpt = (start > 0 ? QStringLiteral("…") : QString()) + section.body.mid(start, 140).trimmed();
                if (start + 140 < section.body.size())
                    hit.excerpt += QStringLiteral("…");
            } else {
                hit.excerpt = section.body.left(140);
            }
            hits.push_back(std::move(hit));
        }
    }
    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.score > b.score; });
    return hits;
}

QString Manual::slug(const QString& heading)
{
    QString out;
    for (const QChar c : fold(heading.trimmed())) {
        if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_'))
            out += c;
        else if (c.isSpace())
            out += QLatin1Char('-');
    }
    return out;
}

QString Manual::fold(const QString& text)
{
    QString out;
    out.reserve(text.size());
    for (QChar c : text) {
        if (c.decompositionTag() == QChar::Canonical) {
            const QString decomposed = c.decomposition();
            if (!decomposed.isEmpty())
                c = decomposed.front();
        }
        out += c.toLower();
    }
    return out;
}

QStringList Manual::commandsIn(const QString& markdown)
{
    static const QRegularExpression re(QStringLiteral("\\{\\{(?:key:|item:)?([A-Za-z0-9_]+)\\}\\}"));
    QStringList names;
    for (auto it = re.globalMatch(markdown); it.hasNext();)
        names << it.next().captured(1);
    names.removeDuplicates();
    return names;
}

QString Manual::expand(const QString& markdown, const Commands& commands, QStringList* unresolved)
{
    static const QRegularExpression re(QStringLiteral("\\{\\{(key:|item:)?([A-Za-z0-9_]+)\\}\\}"));
    QString out;
    qsizetype last = 0;
    for (auto it = re.globalMatch(markdown); it.hasNext();) {
        const auto m = it.next();
        out += QStringView(markdown).mid(last, m.capturedStart() - last);
        last = m.capturedEnd();
        const QString name = m.captured(2);
        const auto found = commands.constFind(name);
        if (found == commands.cend()) {
            if (unresolved)
                *unresolved << name;
            out += QStringLiteral("`") + name + QStringLiteral("`");
            continue;
        }
        const QString key = found->shortcut.isEmpty() ? QString() : QStringLiteral("`") + found->shortcut + QStringLiteral("`");
        const QString kind = m.captured(1);
        if (kind == QLatin1String("key:") && !key.isEmpty()) {
            out += key;
            continue;
        }
        // item: solo el nombre en su menú («Estilo 1»), para listas dentro de un submenú
        const QString text = kind == QLatin1String("item:") ? found->path.section(QStringLiteral(" › "), -1) : found->path;
        out += QStringLiteral("**") + escapeMarkdown(text) + QStringLiteral("**")
               + (key.isEmpty() ? QString() : QStringLiteral(" (") + key + QStringLiteral(")"));
    }
    out += QStringView(markdown).mid(last);
    return out;
}

QString Manual::plainText(const QString& markdown)
{
    static const QRegularExpression image(QStringLiteral("!\\[[^\\]]*\\]\\([^)]*\\)"));
    static const QRegularExpression link(QStringLiteral("\\[([^\\]]*)\\]\\([^)]*\\)"));
    static const QRegularExpression tableRule(QStringLiteral("^\\s*\\|?\\s*:?-{3,}.*$"), QRegularExpression::MultilineOption);
    static const QRegularExpression marks(QStringLiteral("^\\s{0,3}(?:#{1,6}\\s+|>\\s?|[-*+]\\s+|\\d+\\.\\s+)"),
                                          QRegularExpression::MultilineOption);
    QString text = markdown;
    text.remove(image);
    text.replace(link, QStringLiteral("\\1"));
    text.remove(tableRule);
    text.remove(marks);
    text.remove(QLatin1Char('*'));
    text.remove(QLatin1Char('`'));
    text.replace(QLatin1Char('|'), QLatin1Char(' '));
    text.replace(QStringLiteral("\\"), QString());
    return text;
}

// --- ManualBrowser ---------------------------------------------------------------

ManualBrowser::ManualBrowser(const Manual& manual, QWidget* parent)
    : QTextBrowser(parent)
    , _manual(manual)
{
    setObjectName(QStringLiteral("manualBrowser"));
    setOpenExternalLinks(true);
}

QVariant ManualBrowser::loadResource(int type, const QUrl& name)
{
    if (name.isLocalFile() && name.path().endsWith(QLatin1String(".md"))) {
        const int index = _manual.indexOf(QFileInfo(name.toLocalFile()).fileName());
        return index >= 0 ? QVariant(_manual.html(index)) : QVariant();
    }
    return QTextBrowser::loadResource(type, name);
}

void ManualBrowser::doSetSource(const QUrl& name, QTextDocument::ResourceType type)
{
    // Qt cargaría los .md con su propio Markdown: aquí son HTML (el de loadResource)
    if (name.path().endsWith(QLatin1String(".md")))
        type = QTextDocument::HtmlResource;
    QTextBrowser::doSetSource(name, type);
}

// --- ManualWindow ----------------------------------------------------------------

ManualWindow::ManualWindow(std::function<Manual::Commands()> commands, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , _commands(std::move(commands))
{
    setObjectName(QStringLiteral("manualWindow"));

    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(6, 6, 6, 0);
    auto makeButton = [this, toolbar](const char* name, const QString& icon) {
        auto* button = new QToolButton(this);
        button->setObjectName(QString::fromLatin1(name));
        button->setIcon(icons::icon(icon));
        button->setAutoRaise(true);
        toolbar->addWidget(button);
        return button;
    };
    _back = makeButton("manualBack", QStringLiteral("back"));
    _forward = makeButton("manualForward", QStringLiteral("forward"));
    _home = makeButton("manualHome", QStringLiteral("home"));
    _search = new QLineEdit(this);
    _search->setObjectName(QStringLiteral("manualSearch"));
    _search->setClearButtonEnabled(true);
    _search->installEventFilter(this);
    toolbar->addSpacing(8);
    toolbar->addWidget(_search, 1);
    toolbar->addStretch(2);

    _tree = new QTreeWidget(this);
    _tree->setObjectName(QStringLiteral("manualContents"));
    _tree->setHeaderHidden(true);
    _tree->setFrameShape(QFrame::NoFrame);
    _results = new QListWidget(this);
    _results->setObjectName(QStringLiteral("manualResults"));
    _results->setFrameShape(QFrame::NoFrame);
    _results->setItemDelegate(new HitDelegate(_results));
    _results->setUniformItemSizes(false);
    _side = new QStackedWidget(this);
    _side->addWidget(_tree);
    _side->addWidget(_results);

    _browser = new ManualBrowser(_manual, this);
    _browser->setFrameShape(QFrame::NoFrame);

    _splitter = new QSplitter(this);
    _splitter->addWidget(_side);
    _splitter->addWidget(_browser);
    _splitter->setStretchFactor(1, 1);
    _splitter->setSizes({260, 740});

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(toolbar);
    layout->addWidget(_splitter, 1);

    connect(_back, &QToolButton::clicked, _browser, &QTextBrowser::backward);
    connect(_forward, &QToolButton::clicked, _browser, &QTextBrowser::forward);
    connect(_home, &QToolButton::clicked, this, [this] {
        if (!_manual.chapters().empty())
            _browser->setSource(urlFor(_manual.chapters().front().fileName, {}));
    });
    connect(_browser, &QTextBrowser::backwardAvailable, _back, &QWidget::setEnabled);
    connect(_browser, &QTextBrowser::forwardAvailable, _forward, &QWidget::setEnabled);
    // Qt no emite sourceChanged al abrir otro capítulo en un apartado (con #ancla); el
    // historial cambia siempre
    connect(_browser, &QTextBrowser::historyChanged, this, [this] {
        syncContents();
        highlightTerms();
    });
    connect(_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        if (_syncing || !item)
            return;
        _browser->setSource(urlFor(item->data(0, Qt::UserRole).toString(), item->data(0, Qt::UserRole + 1).toString()));
    });
    connect(_search, &QLineEdit::textChanged, this, &ManualWindow::runSearch);
    connect(_results, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) { openHit(_results->row(item)); });
    connect(_results, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { openHit(_results->row(item)); });

    auto addShortcut = [this](const QKeySequence& keys, auto&& slot) {
        auto* action = new QAction(this);
        action->setShortcut(keys);
        action->setShortcutContext(Qt::WindowShortcut);
        connect(action, &QAction::triggered, this, slot);
        addAction(action);
    };
    addShortcut(QKeySequence::Back, [this] { _browser->backward(); });
    addShortcut(QKeySequence::Forward, [this] { _browser->forward(); });
    addShortcut(QKeySequence::Find, [this] {
        _search->setFocus();
        _search->selectAll();
    });
    addShortcut(QKeySequence::ZoomIn, [this] { _browser->zoomIn(); });
    addShortcut(QKeySequence::ZoomOut, [this] { _browser->zoomOut(); });
    addShortcut(QKeySequence::Close, [this] { close(); });

    _back->setEnabled(false);
    _forward->setEnabled(false);
    retranslate();
    applyTheme();
    reload();

    resize(1040, 720);
    if (parent) {
        const QRect area = parent->window()->geometry();
        move(area.center() - rect().center());
    }
}

QUrl ManualWindow::urlFor(const QString& fileName, const QString& anchor) const
{
    QUrl url = QUrl::fromLocalFile(QDir(_manual.directory()).filePath(fileName));
    if (!anchor.isEmpty())
        url.setFragment(anchor);
    return url;
}

QString ManualWindow::currentChapter() const
{
    return QFileInfo(_browser->source().toLocalFile()).fileName();
}

void ManualWindow::open(const QString& fileName, const QString& anchor)
{
    if (!fileName.isEmpty() && _manual.indexOf(fileName) >= 0)
        _browser->setSource(urlFor(fileName, anchor));
    else if (_browser->source().isEmpty() && !_manual.chapters().empty())
        _browser->setSource(urlFor(_manual.chapters().front().fileName, {}));
    show();
    if (isMinimized())
        showNormal();
    raise();
    activateWindow();
}

void ManualWindow::reload()
{
    const QString chapter = currentChapter();
    const QString anchor = _browser->source().fragment();
    _language = i18n::currentLanguage();
    _manual.load(Manual::directoryFor(_language), _commands ? _commands() : Manual::Commands());
    _browser->clearHistory();
    _browser->setSource({});
    fillContents();
    if (!chapter.isEmpty() && _manual.indexOf(chapter) >= 0)
        _browser->setSource(urlFor(chapter, anchor));
    else if (!_manual.chapters().empty())
        _browser->setSource(urlFor(_manual.chapters().front().fileName, {}));
    if (!_search->text().isEmpty())
        runSearch();
}

void ManualWindow::retranslate()
{
    setWindowTitle(tr("User Manual"));
    _back->setToolTip(tr("Back"));
    _forward->setToolTip(tr("Forward"));
    _home->setToolTip(tr("Contents"));
    _search->setPlaceholderText(tr("Search the manual"));
}

void ManualWindow::applyTheme()
{
    const bool dark = appearance::isDarkApplied();
    const QPalette pal = palette();
    const QColor text = pal.color(QPalette::Text);
    const QColor base = pal.color(QPalette::Base);
    const QColor accent = appearance::accent(dark);
    const QColor code = mix(base, text, dark ? 0.10 : 0.06);
    const QColor border = mix(base, text, 0.22);
    const QColor muted = mix(base, text, 0.65);
    const QString mono = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    const QString css = QStringLiteral(
                            "h1 { font-size: 22pt; font-weight: 600; margin-top: 4px; margin-bottom: 10px; }"
                            "h2 { font-size: 15pt; font-weight: 600; margin-top: 22px; margin-bottom: 6px; color: %1; }"
                            "h3 { font-size: 12pt; font-weight: 600; margin-top: 16px; margin-bottom: 4px; }"
                            "p, li { line-height: 135%; }"
                            "a { color: %1; text-decoration: none; }"
                            "code { font-family: '%2'; background-color: %3; }"
                            "pre { font-family: '%2'; background-color: %3; padding: 8px; }"
                            "table { border-color: %4; border-style: solid; border-collapse: collapse; margin-top: 6px; margin-bottom: 6px; }"
                            "th { background-color: %3; font-weight: 600; text-align: left; }"
                            "blockquote { margin-left: 14px; color: %5; }")
                            .arg(accent.name(), mono, code.name(), border.name(), muted.name());
    _browser->document()->setDefaultStyleSheet(css);
    _browser->document()->setDocumentMargin(18);
    _back->setIcon(icons::icon(QStringLiteral("back")));
    _forward->setIcon(icons::icon(QStringLiteral("forward")));
    _home->setIcon(icons::icon(QStringLiteral("home")));
    if (!_browser->source().isEmpty())
        _browser->reload();
}

void ManualWindow::fillContents()
{
    _syncing = true;
    _tree->clear();
    for (const Manual::Chapter& chapter : _manual.chapters()) {
        auto* top = new QTreeWidgetItem(_tree, {chapter.title});
        top->setData(0, Qt::UserRole, chapter.fileName);
        QTreeWidgetItem* lastSection = nullptr;
        for (size_t i = 1; i < chapter.headings.size(); ++i) {
            const Manual::Heading& h = chapter.headings[i];
            if (h.level > 3)
                continue;
            QTreeWidgetItem* parent = (h.level == 3 && lastSection) ? lastSection : top;
            auto* item = new QTreeWidgetItem(parent, {h.text});
            item->setData(0, Qt::UserRole, chapter.fileName);
            item->setData(0, Qt::UserRole + 1, h.anchor);
            if (h.level <= 2)
                lastSection = item;
        }
    }
    _syncing = false;
}

void ManualWindow::syncContents()
{
    const QString chapter = currentChapter();
    const QString anchor = _browser->source().fragment();
    QTreeWidgetItem* found = nullptr;
    for (int i = 0; i < _tree->topLevelItemCount() && !found; ++i) {
        QTreeWidgetItem* top = _tree->topLevelItem(i);
        if (top->data(0, Qt::UserRole).toString() != chapter)
            continue;
        found = top;
        if (anchor.isEmpty())
            break;
        for (QTreeWidgetItemIterator it(top); *it; ++it) {
            if ((*it)->data(0, Qt::UserRole).toString() != chapter)
                break;
            if ((*it)->data(0, Qt::UserRole + 1).toString() == anchor) {
                found = *it;
                break;
            }
        }
    }
    if (!found || found == _tree->currentItem())
        return;
    _syncing = true;
    _tree->setCurrentItem(found);
    found->setExpanded(true);
    _tree->scrollToItem(found);
    _syncing = false;
}

void ManualWindow::runSearch()
{
    const QString query = _search->text().trimmed();
    if (query.isEmpty()) {
        _side->setCurrentWidget(_tree);
        highlightTerms();
        return;
    }
    _side->setCurrentWidget(_results);
    _results->clear();
    const auto hits = _manual.search(query);
    for (const Manual::Hit& hit : hits) {
        const Manual::Chapter& chapter = _manual.chapters()[static_cast<size_t>(hit.chapter)];
        const QString where = hit.heading == chapter.title ? chapter.title : chapter.title + QStringLiteral(" › ") + hit.heading;
        auto* item = new QListWidgetItem(where, _results);
        item->setData(Qt::UserRole, hit.chapter);
        item->setData(Qt::UserRole + 1, hit.anchor);
        item->setData(HitDelegate::kExcerptRole, hit.excerpt);
        item->setToolTip(hit.excerpt);
    }
    if (hits.empty()) {
        auto* item = new QListWidgetItem(tr("No results"), _results);
        item->setFlags(Qt::NoItemFlags);
    }
    highlightTerms();
}

void ManualWindow::openHit(int row)
{
    QListWidgetItem* item = _results->item(row);
    if (!item || !(item->flags() & Qt::ItemIsEnabled))
        return;
    const int chapter = item->data(Qt::UserRole).toInt();
    if (chapter < 0 || chapter >= static_cast<int>(_manual.chapters().size()))
        return;
    const QUrl url = urlFor(_manual.chapters()[static_cast<size_t>(chapter)].fileName, item->data(Qt::UserRole + 1).toString());
    if (url == _browser->source())
        highlightTerms();
    else
        _browser->setSource(url);
}

void ManualWindow::highlightTerms()
{
    QList<QTextEdit::ExtraSelection> selections;
    const QStringList terms =
        Manual::fold(_search->text()).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (!terms.isEmpty()) {
        const QString text = Manual::fold(_browser->document()->toPlainText());
        QTextCharFormat format;
        const bool dark = appearance::isDarkApplied();
        format.setBackground(dark ? QColor(0x80, 0x66, 0x10) : QColor(0xff, 0xe5, 0x80));
        for (const QString& term : terms) {
            for (qsizetype at = text.indexOf(term); at >= 0; at = text.indexOf(term, at + term.size())) {
                QTextEdit::ExtraSelection selection;
                selection.cursor = QTextCursor(_browser->document());
                selection.cursor.setPosition(static_cast<int>(at));
                selection.cursor.setPosition(static_cast<int>(at + term.size()), QTextCursor::KeepAnchor);
                selection.format = format;
                selections << selection;
            }
        }
    }
    _browser->setExtraSelections(selections);
}

bool ManualWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _search && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape && !_search->text().isEmpty()) {
            _search->clear();
            return true;
        }
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && _side->currentWidget() == _results) {
            openHit(0);
            return true;
        }
        if (key->key() == Qt::Key_Down) {
            QWidget* list = _side->currentWidget();
            list->setFocus();
            if (list == _results && _results->count() > 0 && !_results->currentItem())
                _results->setCurrentRow(0);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ManualWindow::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::LanguageChange:
        retranslate();
        // Los menús se traducen después: el manual (y sus órdenes), a continuación
        QTimer::singleShot(0, this, [this] { reload(); });
        break;
    case QEvent::PaletteChange:
        applyTheme();
        highlightTerms();
        break;
    default:
        break;
    }
}

} // namespace edita
