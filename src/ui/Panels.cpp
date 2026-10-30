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

#include "ui/Panels.h"

#include "ui/Appearance.h"
#include "ui/FileIcons.h"
#include "ui/FunctionParser.h"
#include "ui/GitStatus.h"
#include "ui/Icons.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace edita {

namespace {

constexpr int kIdRole = Qt::UserRole;
constexpr int kViewRole = Qt::UserRole + 1;
constexpr int kPathRole = Qt::UserRole;
constexpr int kIsDirRole = Qt::UserRole + 1;
constexpr int kPosRole = Qt::UserRole;

QToolButton* toolButton(const QString& icon, const QString& tip, const char* name, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(QString::fromLatin1(name));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setIconSize(QSize(16, 16));
    QAction* action = new QAction(tip, b);
    icons::setIcon(action, icon);
    b->setDefaultAction(action);
    return b;
}

// La zona visible en el mapa (ViewZoneDlg de N++): un rectángulo translúcido del acento
// Encima del mapa: la zona que se ve en el editor, bien marcada (lo de fuera, algo
// atenuado) y una raya donde está el cursor
class ViewZone : public QWidget
{
public:
    using QWidget::QWidget;

    void setZone(const QRect& zone, int caretY)
    {
        _zone = zone;
        _caretY = caretY;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool dark = appearance::isDarkApplied();
        const QColor accent = appearance::accent(dark);
        // Fuera de la zona, un velo
        const QColor veil = dark ? QColor(0, 0, 0, 70) : QColor(0, 0, 0, 22);
        p.fillRect(QRect(0, 0, width(), std::max(0, _zone.top())), veil);
        p.fillRect(QRect(0, _zone.bottom() + 1, width(), std::max(0, height() - _zone.bottom() - 1)), veil);
        // La zona
        QColor fill = accent;
        QColor border = accent;
        fill.setAlpha(dark ? 38 : 30);
        border.setAlpha(230);
        p.setPen(QPen(border, 1.5));
        p.setBrush(fill);
        p.drawRoundedRect(QRectF(_zone).adjusted(1, 0.75, -1, -0.75), 3, 3);
        // El cursor
        if (_caretY >= 0) {
            QColor caret = accent;
            p.fillRect(QRect(0, _caretY, width(), 2), caret);
        }
    }

private:
    QRect _zone;
    int _caretY = -1;
};

} // namespace

// --- Lista de documentos ----------------------------------------------------------------

DocumentListPanel::DocumentListPanel(QWidget* parent)
    : QWidget(parent)
{
    _list = new QTreeWidget(this);
    _list->setObjectName(QStringLiteral("documentList"));
    _list->setColumnCount(2);
    _list->setHeaderLabels({tr("Name"), tr("Folder")});
    _list->setRootIsDecorated(false);
    _list->header()->setStretchLastSection(true);
    _list->viewport()->installEventFilter(this);
    connect(_list, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        emit activated(item->data(0, kIdRole).value<BufferId>(), item->data(0, kViewRole).toInt());
    });
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_list);
}

void DocumentListPanel::setDocuments(const std::vector<Document>& documents, BufferId current, int currentView)
{
    _list->clear();
    for (const Document& d : documents) {
        auto* item = new QTreeWidgetItem(_list);
        item->setText(0, d.name);
        item->setText(1, d.path.isEmpty() ? QString() : QFileInfo(d.path).absolutePath());
        item->setToolTip(0, d.path.isEmpty() ? d.name : d.path);
        item->setIcon(0, DocTabBar::iconFor(d.path.isEmpty() ? QString() : QFileInfo(d.path).fileName(), d.state));
        item->setData(0, kIdRole, QVariant::fromValue(d.id));
        item->setData(0, kViewRole, d.view);
        if (d.id == current && d.view == currentView)
            _list->setCurrentItem(item);
    }
    _list->resizeColumnToContents(0);
}

bool DocumentListPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Clic central: cerrar (como en las pestañas)
    if (watched == _list->viewport() && event->type() == QEvent::MouseButtonRelease) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::MiddleButton) {
            if (QTreeWidgetItem* item = _list->itemAt(mouse->position().toPoint())) {
                emit closeRequested(item->data(0, kIdRole).value<BufferId>(), item->data(0, kViewRole).toInt());
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// --- Mapa del documento -------------------------------------------------------------------

DocumentMapPanel::DocumentMapPanel(QWidget* parent)
    : QWidget(parent)
{
    _map = new ScintillaEditBase(this);
    _map->setObjectName(QStringLiteral("documentMap"));
    _map->setFocusPolicy(Qt::NoFocus);
    _map->viewport()->installEventFilter(this);
    _map->viewport()->setCursor(Qt::PointingHandCursor);
    _zone = new ViewZone(_map->viewport());
    _zone->setObjectName(QStringLiteral("documentMapZone"));
    _zone->setAttribute(Qt::WA_TransparentForMouseEvents);
    configureMap();
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_map);
}

void DocumentMapPanel::configureMap()
{
    // DocumentMap::initWrapMap / reloadMap: miniatura sin márgenes, cursor ni barras
    _map->send(SCI_SETZOOM, static_cast<uptr_t>(-10));
    for (int margin = 0; margin < 5; ++margin)
        _map->send(SCI_SETMARGINWIDTHN, static_cast<uptr_t>(margin), 0);
    // Un margen estrecho con las marcas del documento: los cambios de Git (marcadores 0, 16
    // y 17) y los marcadores (20); sus colores los pone el estilo (setEditor)
    _map->send(SCI_SETMARGINTYPEN, 0, SC_MARGIN_SYMBOL);
    _map->send(SCI_SETMARGINMASKN, 0, (1 << 0) | (1 << 16) | (1 << 17) | (1 << 20));
    _map->send(SCI_SETMARGINWIDTHN, 0, 4);
    _map->send(SCI_SETVSCROLLBAR, 0);
    _map->send(SCI_SETHSCROLLBAR, 0);
    _map->send(SCI_SETCARETSTYLE, CARETSTYLE_INVISIBLE);
    _map->send(SCI_SETCARETLINEVISIBLE, 0);
    _map->send(SCI_SETCARETLINEVISIBLEALWAYS, 0);
    _map->send(SCI_SETINDENTATIONGUIDES, SC_IV_NONE);
    _map->send(SCI_SETVIEWWS, SCWS_INVISIBLE);
    _map->send(SCI_SETVIEWEOL, 0);
    _map->send(SCI_SETCURSOR, static_cast<uptr_t>(SC_CURSORARROW));
    _map->send(SCI_SETMOUSEDWELLTIME, SC_TIME_FOREVER);
}

void DocumentMapPanel::setEditor(ScintillaEditBase* editor, Styler styler)
{
    for (const auto& c : _connections)
        disconnect(c);
    _connections.clear();
    _editor = editor;
    _styler = std::move(styler);
    if (!editor)
        return;
    // El editor se desplaza o cambia: el mapa lo sigue
    _connections.push_back(connect(editor, &ScintillaEditBase::updateUi, this, [this] { sync(); }));
    _connections.push_back(connect(editor, &ScintillaEditBase::resized, this, [this] { sync(); }));
    restyle();
}

void DocumentMapPanel::restyle()
{
    if (!_editor)
        return;
    const sptr_t doc = _editor->send(SCI_GETDOCPOINTER);
    if (_map->send(SCI_GETDOCPOINTER) != doc)
        _map->send(SCI_SETDOCPOINTER, 0, doc);
    if (_styler)
        _styler(*_map);
    configureMap();
    _map->send(SCI_SETWRAPMODE, static_cast<uptr_t>(_editor->send(SCI_GETWRAPMODE)));
    sync();
}

void DocumentMapPanel::sync()
{
    if (!_editor)
        return;
    if (_map->send(SCI_GETDOCPOINTER) != _editor->send(SCI_GETDOCPOINTER)) {
        restyle();
        return;
    }
    // DocumentMap::scrollMap: el mapa se desplaza en proporción para que la zona se vea
    const sptr_t edFirst = _editor->send(SCI_GETFIRSTVISIBLELINE);
    const sptr_t edOnScreen = std::max<sptr_t>(1, _editor->send(SCI_LINESONSCREEN));
    const sptr_t edTotal = _editor->send(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(_editor->send(SCI_GETLINECOUNT)));
    const sptr_t mapOnScreen = std::max<sptr_t>(1, _map->send(SCI_LINESONSCREEN));
    const sptr_t mapTotal = _map->send(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(_map->send(SCI_GETLINECOUNT)));
    sptr_t mapFirst = 0;
    if (mapTotal > mapOnScreen && edTotal > edOnScreen) {
        const double ratio = static_cast<double>(edFirst) / static_cast<double>(edTotal - edOnScreen);
        mapFirst = static_cast<sptr_t>(ratio * static_cast<double>(mapTotal - mapOnScreen) + 0.5);
    }
    _map->send(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(mapFirst));
    // La capa cubre todo el mapa: la zona y la raya del cursor, en coordenadas del mapa
    _zone->setGeometry(_map->viewport()->rect());
    const sptr_t lineHeight = std::max<sptr_t>(1, _map->send(SCI_TEXTHEIGHT, 0));
    const sptr_t caretLine = _editor->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(_editor->send(SCI_GETCURRENTPOS)));
    const sptr_t caretY = (_map->send(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(caretLine)) - mapFirst) * lineHeight;
    static_cast<ViewZone*>(_zone)->setZone(viewZone(), static_cast<int>(caretY));
}

QRect DocumentMapPanel::viewZone() const
{
    if (!_editor)
        return {};
    const sptr_t edFirst = _editor->send(SCI_GETFIRSTVISIBLELINE);
    const sptr_t edOnScreen = std::max<sptr_t>(1, _editor->send(SCI_LINESONSCREEN));
    const sptr_t docFirst = _editor->send(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(edFirst));
    const sptr_t docLast = _editor->send(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(edFirst + edOnScreen - 1));
    const sptr_t lineHeight = std::max<sptr_t>(1, _map->send(SCI_TEXTHEIGHT, 0));
    const sptr_t mapFirst = _map->send(SCI_GETFIRSTVISIBLELINE);
    const sptr_t top = _map->send(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(docFirst)) - mapFirst;
    const sptr_t bottom = _map->send(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(docLast))
                        + _map->send(SCI_WRAPCOUNT, static_cast<uptr_t>(docLast)) - mapFirst;
    return QRect(0, static_cast<int>(top * lineHeight), _map->viewport()->width(),
                 static_cast<int>(std::max<sptr_t>(1, bottom - top) * lineHeight));
}

void DocumentMapPanel::scrollEditorTo(int y)
{
    if (!_editor)
        return;
    // Clic en el mapa: esa línea queda en el centro del editor
    const sptr_t lineHeight = std::max<sptr_t>(1, _map->send(SCI_TEXTHEIGHT, 0));
    const sptr_t mapLine = _map->send(SCI_GETFIRSTVISIBLELINE) + std::max(0, y) / lineHeight;
    const sptr_t docLine = _map->send(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(mapLine));
    const sptr_t edLine = _editor->send(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(docLine));
    const sptr_t onScreen = _editor->send(SCI_LINESONSCREEN);
    _editor->send(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(std::max<sptr_t>(0, edLine - onScreen / 2)));
    sync();
}

bool DocumentMapPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != _map->viewport())
        return QWidget::eventFilter(watched, event);
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            _dragging = true;
            scrollEditorTo(static_cast<int>(mouse->position().y()));
        }
        return true;
    }
    case QEvent::MouseMove: {
        if (_dragging)
            scrollEditorTo(static_cast<int>(static_cast<QMouseEvent*>(event)->position().y()));
        return true;
    }
    case QEvent::MouseButtonRelease:
        _dragging = false;
        return true;
    case QEvent::Wheel: {
        // La rueda desplaza el editor
        if (_editor) {
            const int steps = static_cast<QWheelEvent*>(event)->angleDelta().y() / 40;
            _editor->send(SCI_LINESCROLL, 0, -steps);
        }
        return true;
    }
    case QEvent::Resize:
        QTimer::singleShot(0, this, [this] { sync(); });
        break;
    case QEvent::ContextMenu:
        return true;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

// --- Lista de funciones ---------------------------------------------------------------------

FunctionListPanel::FunctionListPanel(FunctionParsers& parsers, QWidget* parent)
    : QWidget(parent)
    , _parsers(parsers)
{
    _scanner = new ScintillaEditBase(this);
    _scanner->hide();
    _filter = new QLineEdit(this);
    _filter->setObjectName(QStringLiteral("functionFilter"));
    _filter->setPlaceholderText(tr("Search"));
    _filter->setClearButtonEnabled(true);
    _sort = toolButton(QStringLiteral("sort"), tr("Sort"), "functionSort", this);
    _sort->defaultAction()->setCheckable(true);
    QToolButton* reloadButton = toolButton(QStringLiteral("reload"), tr("Reload"), "functionReload", this);
    _tree = new QTreeWidget(this);
    _tree->setObjectName(QStringLiteral("functionTree"));
    _tree->setHeaderHidden(true);
    _refresh = new QTimer(this);
    _refresh->setSingleShot(true);
    _refresh->setInterval(800);
    connect(_refresh, &QTimer::timeout, this, &FunctionListPanel::reload);
    connect(_filter, &QLineEdit::textChanged, this, &FunctionListPanel::setFilter);
    connect(_sort->defaultAction(), &QAction::toggled, this, &FunctionListPanel::setSorted);
    connect(reloadButton->defaultAction(), &QAction::triggered, this, &FunctionListPanel::reload);
    connect(_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        const QVariant pos = item->data(0, kPosRole);
        if (pos.isValid() && pos.toLongLong() >= 0)
            emit goToPosition(static_cast<sptr_t>(pos.toLongLong()));
    });

    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);
    bar->addWidget(_filter, 1);
    bar->addWidget(_sort);
    bar->addWidget(reloadButton);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(bar);
    layout->addWidget(_tree, 1);
}

void FunctionListPanel::setSource(ScintillaEditBase* editor, LangType lang, const QString& userLanguage, const QString& title)
{
    if (_editor != editor) {
        disconnect(_modified);
        _editor = editor;
        // Al escribir, se vuelve a analizar al rato
        if (editor) {
            _modified = connect(editor, &ScintillaEditBase::modified, this, [this](Scintilla::ModificationFlags type) {
                if (Scintilla::FlagSet(type, Scintilla::ModificationFlags::InsertText)
                    || Scintilla::FlagSet(type, Scintilla::ModificationFlags::DeleteText))
                    _refresh->start();
            });
        }
    }
    _lang = lang;
    _userLanguage = userLanguage;
    _title = title;
    reload();
}

void FunctionListPanel::reload()
{
    _refresh->stop();
    _tree->clear();
    if (!_editor)
        return;
    _scanner->send(SCI_SETDOCPOINTER, 0, _editor->send(SCI_GETDOCPOINTER));
    std::vector<FunctionInfo> found;
    if (!_parsers.parse(*_scanner, _lang, _userLanguage, found)) {
        _scanner->send(SCI_SETDOCPOINTER, 0, 0);
        return;
    }
    _scanner->send(SCI_SETDOCPOINTER, 0, 0); // no retener el documento
    // FunctionListPanel::addInTreeStateArray: la raíz es el archivo; las clases, ramas
    auto* root = new QTreeWidgetItem(_tree, {_title});
    root->setIcon(0, icons::icon(QStringLiteral("doc-list")));
    std::map<std::string, QTreeWidgetItem*> classes;
    for (const FunctionInfo& fi : found) {
        QTreeWidgetItem* parent = root;
        if (!fi.className.empty()) {
            auto it = classes.find(fi.className);
            if (it == classes.end()) {
                auto* cls = new QTreeWidgetItem(root, {QString::fromStdString(fi.className)});
                cls->setData(0, kPosRole, static_cast<qlonglong>(fi.classPos));
                cls->setIcon(0, icons::icon(QStringLiteral("function-list")));
                it = classes.emplace(fi.className, cls).first;
            }
            parent = it->second;
        }
        if (fi.name.empty())
            continue;
        auto* item = new QTreeWidgetItem(parent, {QString::fromStdString(fi.name)});
        item->setData(0, kPosRole, static_cast<qlonglong>(fi.pos));
    }
    _tree->expandAll();
    if (_sort->defaultAction()->isChecked())
        _tree->sortItems(0, Qt::AscendingOrder);
    applyFilter();
}

void FunctionListPanel::setSorted(bool sorted)
{
    _sort->defaultAction()->setChecked(sorted);
    _tree->setSortingEnabled(false);
    if (sorted)
        _tree->sortItems(0, Qt::AscendingOrder);
    else
        reload(); // el orden del documento
}

void FunctionListPanel::setFilter(const QString& text)
{
    if (_filter->text() != text)
        _filter->setText(text);
    applyFilter();
}

void FunctionListPanel::applyFilter()
{
    const QString text = _filter->text();
    std::function<bool(QTreeWidgetItem*)> visit = [&](QTreeWidgetItem* item) {
        bool anyChild = false;
        for (int i = 0; i < item->childCount(); ++i)
            anyChild = visit(item->child(i)) || anyChild;
        const bool match = text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive);
        const bool visible = item->parent() == nullptr || match || anyChild;
        item->setHidden(!visible);
        return visible;
    };
    for (int i = 0; i < _tree->topLevelItemCount(); ++i)
        visit(_tree->topLevelItem(i));
}

// --- Explorador de carpetas -------------------------------------------------------------------

FileBrowserPanel::FileBrowserPanel(QWidget* parent)
    : QWidget(parent)
{
    _tree = new QTreeWidget(this);
    _tree->setObjectName(QStringLiteral("folderTree"));
    _tree->setHeaderHidden(true);
    // La segunda columna: la letra del estado de Git (M, A, U…)
    _tree->setColumnCount(2);
    _tree->header()->setStretchLastSection(false);
    _tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    _tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _tree->setContextMenuPolicy(Qt::CustomContextMenu);
    _watcher = new QFileSystemWatcher(this);
    connect(_tree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* item) { populate(item); });
    connect(_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        if (!item->data(0, kIsDirRole).toBool())
            emit openFile(item->data(0, kPathRole).toString());
    });
    connect(_tree, &QTreeWidget::customContextMenuRequested, this, &FileBrowserPanel::showMenu);
    // Cambios en una carpeta ya desplegada: se vuelve a leer
    connect(_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString& dir) {
        std::function<void(QTreeWidgetItem*)> find = [&](QTreeWidgetItem* item) {
            if (item->data(0, kPathRole).toString() == dir && item->isExpanded())
                populate(item);
            for (int i = 0; i < item->childCount(); ++i)
                find(item->child(i));
        };
        for (int i = 0; i < _tree->topLevelItemCount(); ++i)
            find(_tree->topLevelItem(i));
        emit folderChanged();
    });

    QToolButton* add = toolButton(QStringLiteral("folder-add"), tr("Add folder"), "folderAdd", this);
    QToolButton* locateButton = toolButton(QStringLiteral("locate"), tr("Locate current file"), "folderLocate", this);
    QToolButton* collapse = toolButton(QStringLiteral("collapse"), tr("Collapse all"), "folderCollapse", this);
    connect(add->defaultAction(), &QAction::triggered, this, &FileBrowserPanel::addFolderRequested);
    connect(locateButton->defaultAction(), &QAction::triggered, this, &FileBrowserPanel::locateRequested);
    connect(collapse->defaultAction(), &QAction::triggered, _tree, &QTreeWidget::collapseAll);
    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);
    bar->addStretch();
    bar->addWidget(add);
    bar->addWidget(locateButton);
    bar->addWidget(collapse);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(bar);
    layout->addWidget(_tree, 1);
}

void FileBrowserPanel::refreshIcons()
{
    std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* item) {
        if (!item->data(0, kIsDirRole).toBool()) {
            const QString path = item->data(0, kPathRole).toString();
            item->setIcon(0, fileicons::enabled() ? fileicons::forFile(path) : icons::icon(QStringLiteral("new")));
        }
        for (int i = 0; i < item->childCount(); ++i)
            walk(item->child(i));
    };
    for (int i = 0; i < _tree->topLevelItemCount(); ++i)
        walk(_tree->topLevelItem(i));
}

QTreeWidgetItem* FileBrowserPanel::makeItem(const QString& path, bool isDir)
{
    auto* item = new QTreeWidgetItem;
    item->setText(0, QFileInfo(path).fileName().isEmpty() ? path : QFileInfo(path).fileName());
    item->setToolTip(0, path);
    item->setData(0, kPathRole, path);
    item->setData(0, kIsDirRole, isDir);
    item->setIcon(0, isDir ? icons::icon(QStringLiteral("open"))
                           : fileicons::enabled() ? fileicons::forFile(path) : icons::icon(QStringLiteral("new")));
    if (isDir)
        item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    decorate(item);
    return item;
}

void FileBrowserPanel::setGitStatus(const GitStatus* status)
{
    if (_git)
        disconnect(_git, nullptr, this, nullptr);
    _git = status;
    if (status)
        connect(status, &GitStatus::updated, this, &FileBrowserPanel::refreshGitDecorations);
    refreshGitDecorations();
}

void FileBrowserPanel::refreshGitDecorations()
{
    std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* item) {
        decorate(item);
        for (int i = 0; i < item->childCount(); ++i)
            walk(item->child(i));
    };
    for (int i = 0; i < _tree->topLevelItemCount(); ++i)
        walk(_tree->topLevelItem(i));
}

void FileBrowserPanel::decorate(QTreeWidgetItem* item) const
{
    const QString path = item->data(0, kPathRole).toString();
    const GitStatus::State state = _git ? _git->stateOf(path) : GitStatus::State::None;
    const bool dark = palette().color(QPalette::Base).lightness() < 128;
    const bool isDir = item->data(0, kIsDirRole).toBool();
    QString tip = path;
    if (!item->parent() && _git && !_git->branchOf(path).isEmpty())
        tip += QLatin1Char('\n') + tr("Git branch: %1").arg(_git->branchOf(path));
    if (state == GitStatus::State::None) {
        item->setForeground(0, QBrush());
        item->setText(1, QString());
        item->setToolTip(0, tip);
        return;
    }
    const QColor color = GitStatus::color(state, dark);
    item->setForeground(0, color);
    item->setForeground(1, color);
    // Las carpetas, un punto (lo que tienen dentro ya lleva su letra)
    item->setText(1, state == GitStatus::State::Ignored ? QString() : isDir ? QStringLiteral("•") : GitStatus::badge(state));
    item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    item->setToolTip(0, tip + QLatin1Char('\n') + GitStatus::description(state));
    item->setToolTip(1, GitStatus::description(state));
}

void FileBrowserPanel::populate(QTreeWidgetItem* item)
{
    // Carpetas primero y luego archivos, por nombre; sin los ocultos
    const QString path = item->data(0, kPathRole).toString();
    QStringList expanded;
    for (int i = 0; i < item->childCount(); ++i) {
        if (item->child(i)->isExpanded())
            expanded << item->child(i)->data(0, kPathRole).toString();
    }
    qDeleteAll(item->takeChildren());
    const QDir dir(path);
    for (const QFileInfo& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase))
        item->addChild(makeItem(info.absoluteFilePath(), true));
    for (const QFileInfo& info : dir.entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase))
        item->addChild(makeItem(info.absoluteFilePath(), false));
    item->setChildIndicatorPolicy(item->childCount() ? QTreeWidgetItem::ShowIndicator : QTreeWidgetItem::DontShowIndicator);
    if (!_watcher->directories().contains(path))
        _watcher->addPath(path);
    for (int i = 0; i < item->childCount(); ++i) {
        if (expanded.contains(item->child(i)->data(0, kPathRole).toString()))
            item->child(i)->setExpanded(true);
    }
}

bool FileBrowserPanel::addRoot(const QString& folder)
{
    const QString path = QDir(folder).absolutePath();
    if (!QFileInfo(path).isDir() || roots().contains(path))
        return false;
    QTreeWidgetItem* item = makeItem(path, true);
    item->setText(0, QFileInfo(path).fileName().isEmpty() ? path : QFileInfo(path).fileName());
    _tree->addTopLevelItem(item);
    item->setExpanded(true);
    return true;
}

void FileBrowserPanel::removeRoot(const QString& folder)
{
    for (int i = 0; i < _tree->topLevelItemCount(); ++i) {
        if (_tree->topLevelItem(i)->data(0, kPathRole).toString() == folder)
            delete _tree->takeTopLevelItem(i);
    }
}

QStringList FileBrowserPanel::roots() const
{
    QStringList list;
    for (int i = 0; i < _tree->topLevelItemCount(); ++i)
        list << _tree->topLevelItem(i)->data(0, kPathRole).toString();
    return list;
}

void FileBrowserPanel::setState(const std::vector<Session::FolderRoot>& folders, const QString& selected)
{
    _tree->clear();
    for (const Session::FolderRoot& folder : folders) {
        if (!addRoot(folder.folder))
            continue;
        QTreeWidgetItem* root = _tree->topLevelItem(_tree->topLevelItemCount() - 1);
        // Las carpetas desplegadas, de arriba abajo (cada una se lee al desplegar la anterior)
        QStringList expanded = folder.expanded;
        std::sort(expanded.begin(), expanded.end());
        for (const QString& path : expanded) {
            std::function<void(QTreeWidgetItem*)> find = [&](QTreeWidgetItem* item) {
                for (int i = 0; i < item->childCount(); ++i) {
                    QTreeWidgetItem* child = item->child(i);
                    const QString childPath = child->data(0, kPathRole).toString();
                    if (childPath == path)
                        child->setExpanded(true);
                    else if (path.startsWith(childPath + QLatin1Char('/')) && child->isExpanded())
                        find(child);
                }
            };
            find(root);
        }
    }
    if (!selected.isEmpty())
        locate(selected);
}

std::vector<Session::FolderRoot> FileBrowserPanel::state() const
{
    std::vector<Session::FolderRoot> list;
    for (int i = 0; i < _tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* root = _tree->topLevelItem(i);
        Session::FolderRoot folder{root->data(0, kPathRole).toString(), {}};
        std::function<void(QTreeWidgetItem*)> collect = [&](QTreeWidgetItem* item) {
            for (int c = 0; c < item->childCount(); ++c) {
                QTreeWidgetItem* child = item->child(c);
                if (child->isExpanded()) {
                    folder.expanded << child->data(0, kPathRole).toString();
                    collect(child);
                }
            }
        };
        collect(root);
        list.push_back(folder);
    }
    return list;
}

QString FileBrowserPanel::selectedPath() const
{
    const QTreeWidgetItem* item = _tree->currentItem();
    return item ? item->data(0, kPathRole).toString() : QString();
}

QTreeWidgetItem* FileBrowserPanel::rootFor(const QString& path) const
{
    for (int i = 0; i < _tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* root = _tree->topLevelItem(i);
        const QString rootPath = root->data(0, kPathRole).toString();
        if (path == rootPath || path.startsWith(rootPath + QLatin1Char('/')))
            return root;
    }
    return nullptr;
}

bool FileBrowserPanel::locate(const QString& filePath)
{
    const QString path = QFileInfo(filePath).absoluteFilePath();
    QTreeWidgetItem* item = rootFor(path);
    while (item && item->data(0, kPathRole).toString() != path) {
        item->setExpanded(true); // populate
        QTreeWidgetItem* next = nullptr;
        for (int i = 0; i < item->childCount(); ++i) {
            const QString childPath = item->child(i)->data(0, kPathRole).toString();
            if (path == childPath || path.startsWith(childPath + QLatin1Char('/')))
                next = item->child(i);
        }
        item = next;
    }
    if (!item)
        return false;
    _tree->setCurrentItem(item);
    _tree->scrollToItem(item);
    return true;
}

void FileBrowserPanel::showMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = _tree->itemAt(pos);
    QMenu menu(this);
    if (!item) {
        menu.addAction(tr("Add folder…"), this, &FileBrowserPanel::addFolderRequested);
        menu.exec(_tree->viewport()->mapToGlobal(pos));
        return;
    }
    const QString path = item->data(0, kPathRole).toString();
    const bool isDir = item->data(0, kIsDirRole).toBool();
    if (!isDir)
        menu.addAction(tr("Open"), this, [this, path] { emit openFile(path); });
    menu.addAction(tr("Copy path"), this, [path] { QApplication::clipboard()->setText(path); });
    menu.addAction(tr("Copy file name"), this, [path] { QApplication::clipboard()->setText(QFileInfo(path).fileName()); });
    menu.addAction(tr("Open containing folder"), this, [path, isDir] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(isDir ? path : QFileInfo(path).absolutePath()));
    });
    if (isDir)
        menu.addAction(tr("Find in Files…"), this, [this, path] { emit findInFolder(path); });
    menu.addAction(tr("Open in Terminal"), this, [this, path, isDir] { emit openTerminal(isDir ? path : QFileInfo(path).absolutePath()); });
    if (!item->parent()) {
        menu.addSeparator();
        menu.addAction(tr("Remove"), this, [this, path] { removeRoot(path); });
        menu.addAction(tr("Remove all"), _tree, &QTreeWidget::clear);
    }
    menu.exec(_tree->viewport()->mapToGlobal(pos));
}

} // namespace edita
