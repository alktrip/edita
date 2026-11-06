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

#include "ui/DocTabBar.h"

#include "ui/Appearance.h"
#include "ui/FileIcons.h"
#include "ui/Icons.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QEvent>
#include <QIcon>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QToolButton>
#include <QVariantMap>

#include <algorithm>

namespace edita {

namespace {

const QString kIdKey = QStringLiteral("id");
const QString kStateKey = QStringLiteral("state");
const QString kNameKey = QStringLiteral("name");
const QString kPinnedKey = QStringLiteral("pinned");
const QString kColorKey = QStringLiteral("color");
const char* const kPinButton = "editaPinButton";
const char* const kCloseButton = "editaCloseButton"; // el de cerrar, mientras la pestaña está fijada

// Indicador del estado de la pestaña: un punto si hay cambios sin guardar, un
// candado si es de solo lectura y un hueco del mismo tamaño si está guardada (para
// que el texto no salte). N++ usa disquetes azules, rojos y grises.
QIcon stateIcon(DocTabBar::TabState state)
{
    const bool dark = appearance::isDarkApplied();
    if (state == DocTabBar::TabState::ReadOnly)
        return icons::icon(QStringLiteral("lock"), QApplication::palette().color(QPalette::PlaceholderText));
    QIcon icon;
    for (const int size : {16, 32}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        if (state == DocTabBar::TabState::Dirty) {
            QPainter p(&pixmap);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(appearance::unsavedColor(dark));
            const qreal r = size * 0.22;
            p.drawEllipse(QPointF(size / 2.0, size / 2.0), r, r);
        }
        icon.addPixmap(pixmap);
    }
    return icon;
}

// Con iconos por tipo de archivo (de serie), el del tipo con el estado encima
QIcon fileTabIcon(const QString& fileName, DocTabBar::TabState state)
{
    if (!fileicons::enabled())
        return stateIcon(state);
    const auto fileState = state == DocTabBar::TabState::Dirty      ? fileicons::State::Dirty
                         : state == DocTabBar::TabState::ReadOnly ? fileicons::State::ReadOnly
                                                                    : fileicons::State::Saved;
    return fileicons::forFile(fileName, fileState);
}

DocTabBar::TabState stateOf(const Buffer& buffer)
{
    if (buffer.isReadOnly())
        return DocTabBar::TabState::ReadOnly;
    return buffer.isDirty() ? DocTabBar::TabState::Dirty : DocTabBar::TabState::Saved;
}

} // namespace

DocTabBar::DocTabBar(QWidget* parent)
    : QTabBar(parent)
{
    setTabsClosable(true);
    setMovable(true);
    setExpanding(false);
    setDocumentMode(true);
    setUsesScrollButtons(true);
    setElideMode(Qt::ElideNone);
    setSelectionBehaviorOnRemove(QTabBar::SelectRightTab);
}

int DocTabBar::addBuffer(const Buffer& buffer)
{
    const int index = addTab(QString());
    applyBuffer(index, buffer);
    return index;
}

void DocTabBar::setBuffer(int index, const Buffer& buffer)
{
    applyBuffer(index, buffer);
}

void DocTabBar::updateBuffer(const Buffer& buffer)
{
    const int index = indexOf(buffer.id());
    if (index >= 0)
        applyBuffer(index, buffer);
}

void DocTabBar::applyBuffer(int index, const Buffer& buffer)
{
    const TabState state = stateOf(buffer);
    setTabText(index, buffer.displayName());
    const QString name = buffer.isUntitled() ? QString() : QFileInfo(buffer.filePath()).fileName();
    setTabIcon(index, fileTabIcon(name, state));
    setTabToolTip(index, buffer.isUntitled() ? buffer.displayName() : buffer.filePath());
    setTabData(index, QVariantMap{{kIdKey, QVariant::fromValue(buffer.id())},
                                  {kStateKey, static_cast<int>(state)},
                                  {kNameKey, name},
                                  {kPinnedKey, buffer.isPinned()},
                                  {kColorKey, buffer.tabColor()}});
    applyPin(index, buffer.isPinned(), buffer.id());
    update(tabRect(index));
}

void DocTabBar::applyPin(int index, bool pinned, BufferId id)
{
    // A la izquierda, el alfiler (pulsarlo desfija). El botón de cerrar sale de la pestaña mientras
    // está fijada (oculto seguiría ocupando su sitio) y se guarda en el alfiler para devolverlo
    auto* pin = qobject_cast<QToolButton*>(tabButton(index, QTabBar::LeftSide));
    if (pin && !pin->property(kPinButton).toBool())
        pin = nullptr;
    if (pinned) {
        if (!pin) {
            pin = new QToolButton(this);
            pin->setProperty(kPinButton, true);
            pin->setAutoRaise(true);
            pin->setIcon(icons::icon(QStringLiteral("pin")));
            pin->setIconSize(QSize(14, 14));
            pin->setFixedSize(18, 18);
            pin->setToolTip(tr("Unpin Tab"));
            pin->setObjectName(QStringLiteral("tabPin"));
            connect(pin, &QToolButton::clicked, this, [this, id] { emit unpinRequested(id); });
            setTabButton(index, QTabBar::LeftSide, pin);
        }
        // También uno nuevo, si setTabsClosable volvió a crear los botones de cerrar
        if (QWidget* close = tabButton(index, QTabBar::RightSide)) {
            setTabButton(index, QTabBar::RightSide, nullptr); // lo oculta, no lo borra
            if (auto* old = pin->property(kCloseButton).value<QObject*>(); old && old != close)
                old->deleteLater();
            pin->setProperty(kCloseButton, QVariant::fromValue<QObject*>(close));
        }
    } else if (pin) {
        auto* close = qobject_cast<QWidget*>(pin->property(kCloseButton).value<QObject*>());
        setTabButton(index, QTabBar::LeftSide, nullptr);
        pin->deleteLater();
        if (close && tabsClosable() && !tabButton(index, QTabBar::RightSide)) {
            setTabButton(index, QTabBar::RightSide, close);
            close->show();
        } else if (close) {
            close->deleteLater();
        }
    }
}

bool DocTabBar::isPinnedAt(int index) const
{
    return tabData(index).toMap().value(kPinnedKey).toBool();
}

int DocTabBar::colorAt(int index) const
{
    return tabData(index).toMap().value(kColorKey, -1).toInt();
}

QColor DocTabBar::tabColor(int color, bool dark)
{
    // Los de stylers.model.xml de N++ («Tab color N» y «Tab color dark mode N»), con la
    // saturación y la luminosidad de su versión «saturated»
    static const char* const kLight[] = {"#F3F0CB", "#DBF3CB", "#CBDBF3", "#F3DBCB", "#F3CBEE"};
    static const char* const kDark[] = {"#807848", "#568048", "#507094", "#804849", "#754880"};
    if (color < 0 || color > 4)
        return {};
    const QColor base(dark ? kDark[color] : kLight[color]);
    const float sat = std::min(1.0f, base.hslSaturationF() + (dark ? 100.0f : 30.0f) / 240.0f);
    return QColor::fromHslF(base.hslHueF(), sat, (dark ? 146.0f : 140.0f) / 240.0f);
}

void DocTabBar::paintEvent(QPaintEvent* event)
{
    QTabBar::paintEvent(event);
    // El color de cada pestaña: una franja arriba
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < count(); ++i) {
        const int color = colorAt(i);
        if (color < 0)
            continue;
        const QRect r = tabRect(i);
        painter.setPen(Qt::NoPen);
        painter.setBrush(tabColor(color, dark));
        painter.drawRoundedRect(QRectF(r.left() + 2, r.top() + 1, r.width() - 4, 3), 1.5, 1.5);
    }
}

int DocTabBar::indexOf(BufferId id) const
{
    for (int i = 0; i < count(); ++i) {
        if (bufferAt(i) == id)
            return i;
    }
    return -1;
}

BufferId DocTabBar::bufferAt(int index) const
{
    return tabData(index).toMap().value(kIdKey).value<BufferId>();
}

std::vector<BufferId> DocTabBar::buffers() const
{
    std::vector<BufferId> ids;
    ids.reserve(static_cast<std::size_t>(count()));
    for (int i = 0; i < count(); ++i)
        ids.push_back(bufferAt(i));
    return ids;
}

DocTabBar::TabState DocTabBar::stateAt(int index) const
{
    return static_cast<TabState>(tabData(index).toMap().value(kStateKey).toInt());
}

void DocTabBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int index = tabAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && index < 0) {
        emit newDocumentRequested();
        return;
    }
    if (event->button() == Qt::LeftButton && _doubleClickCloses) {
        emit tabCloseRequested(index);
        return;
    }
    QTabBar::mouseDoubleClickEvent(event);
}

void DocTabBar::mouseReleaseEvent(QMouseEvent* event)
{
    // Clic central cierra la pestaña (opción por defecto de N++)
    if (event->button() == Qt::MiddleButton) {
        const int index = tabAt(event->position().toPoint());
        if (index >= 0) {
            emit tabCloseRequested(index);
            return;
        }
    }
    QTabBar::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton)
        emit dragFinished();
}

QIcon DocTabBar::iconForState(TabState state)
{
    return stateIcon(state);
}

QIcon DocTabBar::iconFor(const QString& fileName, TabState state)
{
    return fileTabIcon(fileName, state);
}

void DocTabBar::refreshIcons()
{
    for (int i = 0; i < count(); ++i)
        setTabIcon(i, fileTabIcon(tabData(i).toMap().value(kNameKey).toString(), stateAt(i)));
}

void DocTabBar::changeEvent(QEvent* event)
{
    // Claro u oscuro: los indicadores con los colores nuevos
    if (event->type() == QEvent::PaletteChange)
        refreshIcons();
    QTabBar::changeEvent(event);
}

void DocTabBar::contextMenuEvent(QContextMenuEvent* event)
{
    const int index = tabAt(event->pos());
    if (index >= 0)
        emit tabContextMenuRequested(index, event->globalPos());
}

} // namespace edita
