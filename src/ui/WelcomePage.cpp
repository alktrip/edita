// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/WelcomePage.h"

#include "ui/Appearance.h"
#include "ui/FileIcons.h"
#include "ui/Icons.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace edita {

namespace {

constexpr int kMaxRecent = 8;
constexpr int kContentWidth = 980;

// Un rectángulo que responde al clic (tarjetas y filas de recientes)
class Clickable : public QFrame
{
public:
    explicit Clickable(QWidget* parent, std::function<void()> onClick)
        : QFrame(parent)
        , _onClick(std::move(onClick))
    {
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
    }
    void click()
    {
        if (_onClick)
            _onClick();
    }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
            click();
        QFrame::mouseReleaseEvent(event);
    }
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
            click();
            return;
        }
        QFrame::keyPressEvent(event);
    }

private:
    std::function<void()> _onClick;
};

// «Ctrl+Mayús+A» como teclas: cada una en su recuadro
QString keyCaps(const QKeySequence& keys)
{
    if (keys.isEmpty())
        return {};
    QStringList caps;
    for (const QString& part : keys.toString(QKeySequence::NativeText).split(QLatin1Char('+'), Qt::SkipEmptyParts))
        caps << QStringLiteral("<span class=\"cap\">&nbsp;%1&nbsp;</span>").arg(part.toHtmlEscaped());
    return caps.join(QStringLiteral(" "));
}

QLabel* sectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("welcomeSection"));
    return label;
}

// Quita un widget de la página sin borrarlo en el momento: puede ser la fila en la que
// se acaba de hacer clic, que aún está atendiendo el evento (borrarla ahí cerraba Edita)
void discard(QWidget* widget)
{
    if (!widget)
        return;
    widget->hide();
    widget->deleteLater();
}

QString home(const QString& path)
{
    const QString h = QDir::homePath();
    return path == h || path.startsWith(h + QLatin1Char('/')) ? QLatin1Char('~') + path.mid(h.size()) : path;
}

} // namespace

WelcomePage::WelcomePage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("welcomePage"));
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("welcomeScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setFocusPolicy(Qt::NoFocus);
    outer->addWidget(scroll);

    // El contenido, centrado y con un ancho máximo
    auto* viewport = new QWidget;
    viewport->setObjectName(QStringLiteral("welcomeViewport"));
    auto* center = new QHBoxLayout(viewport);
    center->setContentsMargins(32, 22, 32, 18);
    _content = new QWidget(viewport);
    _content->setMaximumWidth(kContentWidth);
    center->addStretch(1);
    center->addWidget(_content, 100);
    center->addStretch(1);
    scroll->setWidget(viewport);

    auto* main = new QVBoxLayout(_content);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(20);

    // Encabezado: logo, nombre y lema
    auto* header = new QHBoxLayout;
    header->setSpacing(18);
    auto* logo = new QLabel(_content);
    logo->setPixmap(QApplication::windowIcon().pixmap(64, 64));
    header->addWidget(logo);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto* title = new QLabel(QStringLiteral("Edita"), _content);
    title->setObjectName(QStringLiteral("welcomeTitle"));
    titles->addWidget(title);
    auto* tagline = new QLabel(tr("A native text and code editor for Linux, inspired by Notepad++"), _content);
    tagline->setObjectName(QStringLiteral("welcomeTagline"));
    titles->addWidget(tagline);
    _version = new QLabel(_content);
    _version->setObjectName(QStringLiteral("welcomeDim"));
    titles->addWidget(_version);
    header->addLayout(titles, 1);
    // «Mostrar esta página», arriba a la derecha: siempre a la vista, sin desplazarse
    _showOnStartup = new QCheckBox(tr("Show this page when there are no documents"), _content);
    _showOnStartup->setObjectName(QStringLiteral("welcomeShowOnStartup"));
    connect(_showOnStartup, &QCheckBox::toggled, this, [this](bool checked) {
        if (!_updating)
            emit showOnStartupChanged(checked);
    });
    header->addWidget(_showOnStartup, 0, Qt::AlignTop);
    main->addLayout(header);

    // Para empezar: cuatro tarjetas
    auto* cards = new QGridLayout;
    cards->setSpacing(14);
    struct CardDef
    {
        const char* name;
        const char* icon;
        QString title;
        QString subtitle;
        void (WelcomePage::*signal)();
    };
    const CardDef defs[] = {
        {"welcomeNewFile", "new", tr("New file"), tr("Start writing right away"), &WelcomePage::newFileRequested},
        {"welcomeOpenFile", "open", tr("Open file…"), tr("Text, code, any encoding"), &WelcomePage::openFileRequested},
        {"welcomeOpenFolder", "folder-workspace", tr("Open folder…"), tr("As a workspace, with Go to File"), &WelcomePage::openFolderRequested},
        {"welcomePalette", "command-palette", tr("Command palette"), tr("Every command, searchable"), &WelcomePage::commandPaletteRequested},
    };
    for (int i = 0; i < 4; ++i) {
        const CardDef& def = defs[i];
        const auto signal = def.signal;
        auto* card = new Clickable(_content, [this, signal] { emit(this->*signal)(); });
        card->setObjectName(QString::fromLatin1(def.name));
        card->setProperty("welcomeCard", true);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(6);
        auto* icon = new QLabel(card);
        icon->setObjectName(QStringLiteral("welcomeCardIcon"));
        icon->setProperty("iconName", QString::fromLatin1(def.icon));
        layout->addWidget(icon);
        auto* cardTitle = new QLabel(def.title, card);
        cardTitle->setObjectName(QStringLiteral("welcomeCardTitle"));
        layout->addWidget(cardTitle);
        auto* subtitle = new QLabel(def.subtitle, card);
        subtitle->setObjectName(QStringLiteral("welcomeDim"));
        subtitle->setWordWrap(true);
        layout->addWidget(subtitle);
        layout->addStretch(1);
        _cardKeys[i] = new QLabel(card);
        _cardKeys[i]->setTextFormat(Qt::RichText);
        layout->addWidget(_cardKeys[i]);
        cards->addWidget(card, 0, i);
    }
    main->addLayout(cards);

    // Dos columnas: recientes | apariencia y atajos
    auto* columns = new QHBoxLayout;
    columns->setSpacing(18);
    auto* left = new QFrame(_content);
    left->setProperty("welcomePanel", true);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(18, 16, 18, 16);
    leftLayout->setSpacing(6);
    leftLayout->addWidget(sectionTitle(tr("Recent files"), left));
    _recentFiles = new QVBoxLayout;
    _recentFiles->setSpacing(2);
    leftLayout->addLayout(_recentFiles);
    _foldersSection = new QWidget(left);
    auto* foldersLayout = new QVBoxLayout(_foldersSection);
    foldersLayout->setContentsMargins(0, 12, 0, 0);
    foldersLayout->setSpacing(6);
    foldersLayout->addWidget(sectionTitle(tr("Recent folders"), _foldersSection));
    _recentFolders = new QVBoxLayout;
    _recentFolders->setSpacing(2);
    foldersLayout->addLayout(_recentFolders);
    leftLayout->addWidget(_foldersSection);
    leftLayout->addStretch(1);
    columns->addWidget(left, 3);

    auto* right = new QVBoxLayout;
    right->setSpacing(18);
    auto* appearance = new QFrame(_content);
    appearance->setProperty("welcomePanel", true);
    auto* appearanceLayout = new QVBoxLayout(appearance);
    appearanceLayout->setContentsMargins(18, 16, 18, 16);
    appearanceLayout->setSpacing(10);
    appearanceLayout->addWidget(sectionTitle(tr("Appearance"), appearance));
    auto* modes = new QHBoxLayout;
    modes->setSpacing(0);
    _modes = new QButtonGroup(this);
    _modes->setExclusive(true);
    const QString modeNames[] = {tr("Light"), tr("Dark"), tr("System")};
    const char* modeObjects[] = {"welcomeModeLight", "welcomeModeDark", "welcomeModeSystem"};
    for (int i = 0; i < 3; ++i) {
        auto* button = new QToolButton(appearance);
        button->setObjectName(QString::fromLatin1(modeObjects[i]));
        button->setProperty("welcomeSegment", i == 0 ? "first" : i == 2 ? "last" : "middle");
        button->setText(modeNames[i]);
        button->setCheckable(true);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        _modes->addButton(button, i);
        modes->addWidget(button);
    }
    connect(_modes, &QButtonGroup::idClicked, this, [this](int id) {
        if (!_updating)
            emit modeRequested(static_cast<Mode>(id));
    });
    appearanceLayout->addLayout(modes);
    auto* themeRow = new QHBoxLayout;
    themeRow->addWidget(new QLabel(tr("Theme:"), appearance));
    _themes = new QComboBox(appearance);
    _themes->setObjectName(QStringLiteral("welcomeTheme"));
    _themes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(_themes, &QComboBox::activated, this, [this](int index) {
        if (!_updating)
            emit themeRequested(index);
    });
    themeRow->addWidget(_themes, 1);
    appearanceLayout->addLayout(themeRow);
    right->addWidget(appearance);

    auto* shortcuts = new QFrame(_content);
    shortcuts->setProperty("welcomePanel", true);
    auto* shortcutsLayout = new QVBoxLayout(shortcuts);
    shortcutsLayout->setContentsMargins(18, 16, 18, 16);
    shortcutsLayout->setSpacing(6);
    shortcutsLayout->addWidget(sectionTitle(tr("Useful shortcuts"), shortcuts));
    _shortcuts = new QVBoxLayout;
    _shortcuts->setSpacing(4);
    shortcutsLayout->addLayout(_shortcuts);
    right->addWidget(shortcuts);
    right->addStretch(1);
    columns->addLayout(right, 2);
    main->addLayout(columns);

    main->addStretch(1);

    applyStyle();
}

void WelcomePage::setVersion(const QString& version)
{
    _version->setText(tr("Version %1").arg(version) + QStringLiteral("  ·  ")
                      + tr("Start typing to write in a new document · Esc to close"));
}

void WelcomePage::fillRecent(QVBoxLayout* layout, const QStringList& paths, bool folders)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        discard(item->widget());
        delete item;
    }
    if (paths.isEmpty()) {
        auto* empty = new QLabel(folders ? tr("No recent folders") : tr("No recent files yet"), _content);
        empty->setObjectName(QStringLiteral("welcomeDim"));
        layout->addWidget(empty);
        return;
    }
    for (const QString& path : paths) {
        auto* row = new Clickable(_content, [this, path, folders] {
            if (folders)
                emit recentFolderRequested(path);
            else
                emit recentFileRequested(path);
        });
        row->setProperty("welcomeRow", true);
        row->setProperty("path", path);
        row->setToolTip(path);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(8, 5, 8, 5);
        rowLayout->setSpacing(10);
        auto* icon = new QLabel(row);
        const int size = fontMetrics().height() + 2;
        icon->setPixmap((folders ? icons::icon(QStringLiteral("open")) : fileicons::forFile(path)).pixmap(size, size));
        rowLayout->addWidget(icon);
        auto* name = new QLabel(QFileInfo(path).fileName().isEmpty() ? path : QFileInfo(path).fileName(), row);
        name->setObjectName(QStringLiteral("welcomeRowName"));
        rowLayout->addWidget(name);
        auto* folder = new QLabel(row);
        folder->setObjectName(QStringLiteral("welcomeDim"));
        const QString where = home(folders ? QFileInfo(path).absolutePath() : QFileInfo(path).absolutePath());
        folder->setText(fontMetrics().elidedText(where, Qt::ElideMiddle, 360));
        rowLayout->addWidget(folder, 1);
        layout->addWidget(row);
    }
}

void WelcomePage::setRecentFiles(const QStringList& paths)
{
    _filePaths = paths.mid(0, kMaxRecent);
    fillRecent(_recentFiles, _filePaths, false);
}

void WelcomePage::setRecentFolders(const QStringList& paths)
{
    _folderPaths = paths.mid(0, kMaxRecent);
    _foldersSection->setVisible(!_folderPaths.isEmpty());
    fillRecent(_recentFolders, _folderPaths, true);
}

void WelcomePage::setThemes(const QStringList& names, int current)
{
    _updating = true;
    _themes->clear();
    _themes->addItems(names);
    _themes->setCurrentIndex(current);
    _updating = false;
}

void WelcomePage::setMode(Mode mode)
{
    _updating = true;
    if (QAbstractButton* button = _modes->button(static_cast<int>(mode)))
        button->setChecked(true);
    _updating = false;
}

void WelcomePage::setShortcuts(const std::vector<Shortcut>& shortcuts)
{
    while (QLayoutItem* item = _shortcuts->takeAt(0)) {
        if (QLayout* row = item->layout()) {
            while (QLayoutItem* child = row->takeAt(0)) {
                discard(child->widget());
                delete child;
            }
        }
        discard(item->widget());
        delete item;
    }
    _shortcutLabels.clear();
    for (const Shortcut& s : shortcuts) {
        if (s.keys.isEmpty())
            continue;
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(s.text, _content), 1);
        auto* keys = new QLabel(_content);
        keys->setTextFormat(Qt::RichText);
        renderKeys(keys, s.keys);
        _shortcutLabels.emplace_back(keys, s.keys);
        row->addWidget(keys);
        _shortcuts->addLayout(row);
    }
}

void WelcomePage::setActionShortcuts(const QKeySequence& newFile, const QKeySequence& open, const QKeySequence& openFolder,
                                     const QKeySequence& palette)
{
    const QKeySequence keys[] = {newFile, open, openFolder, palette};
    for (int i = 0; i < 4; ++i) {
        _cardSequences[i] = keys[i];
        renderKeys(_cardKeys[i], keys[i]);
    }
}

void WelcomePage::renderKeys(QLabel* label, const QKeySequence& keys) const
{
    label->setText(keys.isEmpty() ? QString() : _capStyle + keyCaps(keys));
}

void WelcomePage::setShowOnStartup(bool show)
{
    _updating = true;
    _showOnStartup->setChecked(show);
    _updating = false;
}

QStringList WelcomePage::recentFileTexts() const
{
    return _filePaths;
}

QStringList WelcomePage::recentFolderTexts() const
{
    return _folderPaths;
}

bool WelcomePage::clickCard(const QString& objectName)
{
    for (QFrame* frame : findChildren<QFrame*>(objectName)) {
        if (auto* card = dynamic_cast<Clickable*>(frame)) {
            card->click();
            return true;
        }
    }
    return false;
}

bool WelcomePage::clickRecent(const QString& path)
{
    for (QFrame* frame : findChildren<QFrame*>()) {
        if (frame->property("path").toString() == path) {
            if (auto* row = dynamic_cast<Clickable*>(frame)) {
                row->click();
                return true;
            }
        }
    }
    return false;
}

// Los colores de la interfaz (claro u oscuro), con el acento de Edita
void WelcomePage::applyStyle()
{
    const bool dark = appearance::isDarkApplied();
    const QPalette pal = QApplication::palette();
    // setStyleSheet manda un PaletteChange: sin esto, applyStyle se llamaría sin fin
    const QString key = QStringLiteral("%1|%2|%3").arg(dark).arg(pal.color(QPalette::Window).name(), pal.color(QPalette::WindowText).name());
    if (key == _styleKey)
        return;
    _styleKey = key;
    const QColor window = pal.color(QPalette::Window);
    const QColor text = pal.color(QPalette::WindowText);
    const QColor accent = appearance::accent(dark);
    auto mix = [](const QColor& a, const QColor& b, double t) {
        return QColor::fromRgbF(static_cast<float>(a.redF() * (1 - t) + b.redF() * t), static_cast<float>(a.greenF() * (1 - t) + b.greenF() * t),
                                static_cast<float>(a.blueF() * (1 - t) + b.blueF() * t));
    };
    const QColor background = dark ? mix(window, QColor(0, 0, 0), 0.18) : mix(window, QColor(255, 255, 255), 0.55);
    const QColor card = dark ? mix(window, QColor(255, 255, 255), 0.04) : QColor(255, 255, 255);
    const QColor border = mix(card, text, dark ? 0.16 : 0.12);
    const QColor hover = mix(card, accent, dark ? 0.12 : 0.07);
    const QColor dim = mix(text, background, 0.42);
    const QColor capBack = mix(card, text, dark ? 0.10 : 0.05);
    setStyleSheet(QStringLiteral(R"(
        #welcomePage, #welcomeViewport, #welcomeScroll { background: %1; }
        QLabel { color: %2; background: transparent; }
        #welcomeTitle { font-size: 28pt; font-weight: 600; }
        #welcomeTagline { font-size: 12pt; }
        #welcomeDim { color: %3; }
        #welcomeSection { font-weight: 600; font-size: 10.5pt; padding-bottom: 4px; }
        #welcomeCardTitle { font-weight: 600; font-size: 11pt; }
        #welcomeRowName { font-weight: 500; }
        QFrame[welcomeCard="true"], QFrame[welcomePanel="true"] {
            background: %4; border: 1px solid %5; border-radius: 12px; }
        QFrame[welcomeCard="true"]:hover, QFrame[welcomeCard="true"]:focus { background: %6; border-color: %7; }
        QFrame[welcomeRow="true"] { border-radius: 7px; }
        QFrame[welcomeRow="true"]:hover, QFrame[welcomeRow="true"]:focus { background: %6; }
        QToolButton[welcomeSegment] { padding: 6px 10px; border: 1px solid %5; background: %4; color: %2; }
        QToolButton[welcomeSegment="first"] { border-top-left-radius: 7px; border-bottom-left-radius: 7px; }
        QToolButton[welcomeSegment="last"] { border-top-right-radius: 7px; border-bottom-right-radius: 7px; }
        QToolButton[welcomeSegment]:hover { background: %6; }
        QToolButton[welcomeSegment]:checked { background: %7; color: %8; border-color: %7; }
        QCheckBox { color: %2; background: transparent; }
    )")
                      .arg(background.name(), text.name(), dim.name(), card.name(), border.name(), hover.name(), accent.name(),
                           (dark ? QColor(0x0d, 0x11, 0x17) : QColor(255, 255, 255)).name()));
    // Las teclas («Ctrl», «E») como recuadros: el texto enriquecido de QLabel no admite
    // bordes redondeados en un <span>; basta un fondo
    _capStyle = QStringLiteral("<style>.cap { background-color: %1; color: %2; font-size: 9pt; }</style>")
                    .arg(capBack.name(), text.name());
    for (int i = 0; i < 4; ++i)
        renderKeys(_cardKeys[i], _cardSequences[i]);
    for (const auto& [label, keys] : _shortcutLabels)
        renderKeys(label, keys);
    // Los iconos de las tarjetas, en el color de acento
    for (QLabel* label : findChildren<QLabel*>(QStringLiteral("welcomeCardIcon")))
        label->setPixmap(icons::icon(label->property("iconName").toString(), accent).pixmap(30, 30));
}

void WelcomePage::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        emit dismissRequested();
        return;
    }
    // Escribir sobre la página: al documento nuevo
    const bool plain = !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    if (plain && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        emit typingStarted(QString());
        return;
    }
    if (plain && !event->text().isEmpty() && event->text().at(0).isPrint()) {
        emit typingStarted(event->text());
        return;
    }
    QWidget::keyPressEvent(event);
}

void WelcomePage::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange)
        applyStyle();
}

void WelcomePage::refreshStyle()
{
    _styleKey.clear();
    applyStyle();
}

void WelcomePage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    applyStyle();
}

} // namespace edita
