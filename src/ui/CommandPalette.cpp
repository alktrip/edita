// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/CommandPalette.h"

#include "core/FuzzyMatch.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QVBoxLayout>

#include <algorithm>

namespace edita {

namespace {

constexpr int kMaxRows = 200;        // filas que se muestran como mucho
constexpr int kVisibleRows = 12;     // alto de la lista
constexpr int kPositionsRole = Qt::UserRole + 1;
constexpr int kDetailRole = Qt::UserRole + 2;
constexpr int kShortcutRole = Qt::UserRole + 3;
constexpr int kCheckedRole = Qt::UserRole + 4;
constexpr int kIndexRole = Qt::UserRole + 5;

// Una fila: icono, el texto con las letras encontradas resaltadas, el detalle atenuado
// y, a la derecha, el atajo
class PaletteDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override
    {
        return {option.rect.width(), option.fontMetrics.height() + 10};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& opt, const QModelIndex& index) const override
    {
        QStyleOptionViewItem option = opt;
        initStyleOption(&option, index);
        const QString text = option.text;
        option.text.clear();
        option.icon = QIcon();
        QStyle* style = option.widget ? option.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

        const bool selected = option.state & QStyle::State_Selected;
        const QPalette& pal = option.palette;
        const QColor textColor = pal.color(selected ? QPalette::HighlightedText : QPalette::Text);
        QColor dim = textColor;
        dim.setAlphaF(0.6f);
        QColor accent = selected ? textColor : pal.color(QPalette::Link);

        QRect r = option.rect.adjusted(8, 0, -10, 0);
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        const int iconSize = option.fontMetrics.height();
        if (!icon.isNull())
            icon.paint(painter, QRect(r.left(), r.center().y() - iconSize / 2, iconSize, iconSize));
        r.setLeft(r.left() + iconSize + 8);

        painter->save();
        painter->setFont(option.font);
        // El atajo y la marca, a la derecha
        QString right = index.data(kShortcutRole).toString();
        if (index.data(kCheckedRole).toBool())
            right = right.isEmpty() ? QStringLiteral("✓") : QStringLiteral("✓   ") + right;
        if (!right.isEmpty()) {
            painter->setPen(dim);
            const int width = option.fontMetrics.horizontalAdvance(right);
            painter->drawText(QRect(r.right() - width, r.top(), width, r.height()), Qt::AlignVCenter | Qt::AlignRight, right);
            r.setRight(r.right() - width - 16);
        }
        // El texto, con las letras encontradas en negrita y en el color de acento
        QTextLayout layout(text, option.font);
        QList<QTextLayout::FormatRange> formats;
        for (const QVariant& v : index.data(kPositionsRole).toList()) {
            QTextLayout::FormatRange range;
            range.start = v.toInt();
            range.length = 1;
            range.format.setFontWeight(QFont::Bold);
            range.format.setForeground(accent);
            formats << range;
        }
        layout.setFormats(formats);
        layout.beginLayout();
        QTextLine line = layout.createLine();
        line.setLineWidth(r.width());
        layout.endLayout();
        painter->setPen(textColor);
        const qreal y = r.top() + (r.height() - line.height()) / 2;
        layout.draw(painter, QPointF(r.left(), y));
        // El detalle, a continuación
        const int used = static_cast<int>(line.naturalTextWidth()) + 12;
        const QString detail = index.data(kDetailRole).toString();
        if (!detail.isEmpty() && used < r.width()) {
            painter->setPen(dim);
            const QString elided = option.fontMetrics.elidedText(detail, Qt::ElideMiddle, r.width() - used);
            painter->drawText(QRect(r.left() + used, r.top(), r.width() - used, r.height()), Qt::AlignVCenter, elided);
        }
        painter->restore();
    }
};

} // namespace

CommandPalette::CommandPalette(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("commandPalette"));
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 90));
    setGraphicsEffect(shadow);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);
    _input = new QLineEdit(this);
    _input->setObjectName(QStringLiteral("commandPaletteInput"));
    _input->setClearButtonEnabled(true);
    layout->addWidget(_input);
    _list = new QListView(this);
    _list->setObjectName(QStringLiteral("commandPaletteList"));
    _list->setFrameShape(QFrame::NoFrame);
    _list->setUniformItemSizes(true);
    _list->setFocusPolicy(Qt::NoFocus);
    _list->setItemDelegate(new PaletteDelegate(_list));
    _list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _model = new QStandardItemModel(this);
    _list->setModel(_model);
    layout->addWidget(_list);
    _status = new QLabel(this);
    _status->setObjectName(QStringLiteral("commandPaletteStatus"));
    QFont small = _status->font();
    small.setPointSizeF(small.pointSizeF() * 0.9);
    _status->setFont(small);
    _status->setEnabled(false);
    layout->addWidget(_status);

    connect(_input, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(_list, &QListView::clicked, this, [this](const QModelIndex& index) { activate(index); });
    _input->installEventFilter(this);
    parent->installEventFilter(this);
    hide();
}

CommandPalette::Mode CommandPalette::modeFor(const QString& text)
{
    if (text.startsWith(QLatin1Char('>')))
        return Mode::Commands;
    if (text.startsWith(QLatin1Char('@')))
        return Mode::Symbols;
    if (text.startsWith(QLatin1Char(':')))
        return Mode::Line;
    return Mode::Files;
}

void CommandPalette::open(const QString& prefix)
{
    // Lo que se pide cada vez que se abre: los elementos pueden haber cambiado
    for (bool& requested : _requested)
        requested = false;
    reposition();
    show();
    raise();
    _input->setText(prefix);
    _input->setFocus();
    filter();
}

void CommandPalette::close()
{
    hide();
    if (QWidget* window = parentWidget())
        window->setFocus();
}

void CommandPalette::setItems(Mode mode, std::vector<Item> items)
{
    _items[static_cast<int>(mode)] = std::move(items);
    if (!isHidden() && mode == _mode)
        filter();
}

void CommandPalette::addItems(Mode mode, std::vector<Item> items)
{
    auto& target = _items[static_cast<int>(mode)];
    target.insert(target.end(), std::make_move_iterator(items.begin()), std::make_move_iterator(items.end()));
    if (!isHidden() && mode == _mode)
        filter();
}

void CommandPalette::setBusy(Mode mode, bool busy)
{
    _busy[static_cast<int>(mode)] = busy;
    if (!isHidden() && mode == _mode)
        filter();
}

void CommandPalette::reposition()
{
    QWidget* window = parentWidget();
    const int width = std::clamp(window->width() * 6 / 10, std::min(420, window->width() - 20), 720);
    const int rowHeight = fontMetrics().height() + 10;
    const int height = _input->sizeHint().height() + rowHeight * kVisibleRows + _status->sizeHint().height() + 24;
    // Debajo de la barra de herramientas, centrada
    int top = 4;
    for (QWidget* child : window->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (child != this && child->isVisible() && child->inherits("QToolBar") && child->geometry().top() < 80)
            top = std::max(top, child->geometry().bottom() + 6);
    }
    if (const QWidget* menu = window->findChild<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        menu && menu->inherits("QMenuBar"))
        top = std::max(top, menu->geometry().bottom() + 6);
    setGeometry((window->width() - width) / 2, top, width, std::min(height, window->height() - top - 10));
}

void CommandPalette::filter()
{
    const QString text = _input->text();
    _mode = modeFor(text);
    const int m = static_cast<int>(_mode);
    if (!_requested[m]) {
        _requested[m] = true;
        emit itemsRequested(_mode);
    }
    const QString query = (_mode == Mode::Files ? text : text.mid(1)).trimmed();
    if (_mode == Mode::Line) {
        // «:120» o «:120:5»: un solo elemento
        const QStringList parts = query.split(QLatin1Char(':'));
        bool okLine = false;
        bool okColumn = true;
        const int line = parts.value(0).trimmed().toInt(&okLine);
        const int column = parts.size() > 1 ? parts.value(1).trimmed().toInt(&okColumn) : 1;
        Item item;
        if (okLine && okColumn && line > 0 && column > 0) {
            item.text = parts.size() > 1 ? tr("Go to line %1, column %2").arg(line).arg(column) : tr("Go to line %1").arg(line);
            item.run = [this, line, column] { emit goToLineRequested(line, column); };
        } else {
            item.text = tr("Type a line number");
        }
        _items[m] = {item};
    }
    const std::vector<Item>& items = _items[m];

    struct Scored
    {
        int index;
        int score;
        QList<int> positions;
    };
    std::vector<Scored> scored;
    if (_mode == Mode::Line) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
            scored.push_back({i, 0, {}});
    } else if (query.isEmpty()) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            int score = -items[static_cast<size_t>(i)].order;
            // Las órdenes usadas hace poco, primero
            if (_mode == Mode::Commands) {
                const qsizetype recent = _history.indexOf(items[static_cast<size_t>(i)].id);
                score = recent >= 0 ? 1000000 - static_cast<int>(recent) : -i;
            }
            scored.push_back({i, score, {}});
        }
    } else {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const Item& item = items[static_cast<size_t>(i)];
            FuzzyMatch match = fuzzyMatch(query, item.text);
            int score = match.score;
            QList<int> positions = match.positions;
            if (!match.matched()) {
                // También por el detalle (el menú, la carpeta), con menos peso
                const FuzzyMatch inDetail = fuzzyMatch(query, item.detail + QLatin1Char(' ') + item.text);
                if (!inDetail.matched())
                    continue;
                score = inDetail.score / 2;
                positions.clear();
                const int offset = static_cast<int>(item.detail.size()) + 1;
                for (int p : inDetail.positions) {
                    if (p >= offset)
                        positions << p - offset;
                }
            }
            if (_mode == Mode::Commands) {
                const qsizetype recent = _history.indexOf(item.id);
                if (recent >= 0)
                    score += std::max(0, 40 - static_cast<int>(recent) * 4);
            }
            scored.push_back({i, score, positions});
        }
    }
    std::stable_sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) { return a.score > b.score; });
    if (scored.size() > static_cast<size_t>(kMaxRows))
        scored.resize(static_cast<size_t>(kMaxRows));

    _model->clear();
    _shown.clear();
    for (const Scored& s : scored) {
        const Item& item = items[static_cast<size_t>(s.index)];
        auto* row = new QStandardItem(item.icon, item.text);
        QVariantList positions;
        for (int p : s.positions)
            positions << p;
        row->setData(positions, kPositionsRole);
        row->setData(item.detail, kDetailRole);
        row->setData(item.shortcut, kShortcutRole);
        row->setData(item.checked, kCheckedRole);
        row->setData(s.index, kIndexRole);
        row->setEditable(false);
        _model->appendRow(row);
        _shown.push_back(s.index);
    }
    if (_model->rowCount() > 0)
        _list->setCurrentIndex(_model->index(0, 0));

    static const char* const hints[] = {
        QT_TR_NOOP("Files: type a name · > commands · @ symbols · : line"),
        QT_TR_NOOP("Commands · Enter runs the selected one"),
        QT_TR_NOOP("Symbols of the current document"),
        QT_TR_NOOP("Go to line (:line or :line:column)"),
    };
    QString status = tr(hints[m]);
    if (_busy[m])
        status += QStringLiteral(" · ") + tr("searching…");
    else if (_mode != Mode::Line)
        status += QStringLiteral(" · ") + tr("%n result(s)", nullptr, static_cast<int>(scored.size()));
    _status->setText(status);
    _input->setPlaceholderText(_mode == Mode::Files ? tr("Go to file (> commands, @ symbols, : line)") : QString());
}

QStringList CommandPalette::visibleTexts() const
{
    QStringList texts;
    for (int row = 0; row < _model->rowCount(); ++row)
        texts << _model->item(row)->text();
    return texts;
}

bool CommandPalette::activateRow(int row)
{
    if (row < 0 || row >= _model->rowCount())
        return false;
    activate(_model->index(row, 0));
    return true;
}

void CommandPalette::activate(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    const int i = index.data(kIndexRole).toInt();
    const std::vector<Item>& items = _items[static_cast<int>(_mode)];
    if (i < 0 || i >= static_cast<int>(items.size()))
        return;
    const Item item = items[static_cast<size_t>(i)]; // copia: run puede cambiar los elementos
    if (_mode == Mode::Commands && !item.id.isEmpty()) {
        _history.removeAll(item.id);
        _history.prepend(item.id);
        while (_history.size() > 30)
            _history.removeLast();
        emit historyChanged(_history);
    }
    close();
    if (item.run)
        item.run();
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget()) {
        if (event->type() == QEvent::Resize && !isHidden())
            reposition();
        return false;
    }
    if (watched != _input)
        return false;
    if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        switch (key->key()) {
        case Qt::Key_Escape:
            close();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            activate(_list->currentIndex());
            return true;
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown: {
            const int rows = _model->rowCount();
            if (rows == 0)
                return true;
            int row = _list->currentIndex().row();
            const int page = kVisibleRows - 1;
            if (key->key() == Qt::Key_Up)
                row = row <= 0 ? rows - 1 : row - 1;
            else if (key->key() == Qt::Key_Down)
                row = row >= rows - 1 ? 0 : row + 1;
            else if (key->key() == Qt::Key_PageUp)
                row = std::max(0, row - page);
            else
                row = std::min(rows - 1, row + page);
            _list->setCurrentIndex(_model->index(row, 0));
            return true;
        }
        default:
            break;
        }
    } else if (event->type() == QEvent::FocusOut) {
        // Al pasar a otra parte de la ventana, se cierra (un clic en la lista no quita el foco)
        auto* focus = static_cast<QFocusEvent*>(event);
        if (focus->reason() != Qt::PopupFocusReason)
            hide();
    }
    return false;
}

} // namespace edita
