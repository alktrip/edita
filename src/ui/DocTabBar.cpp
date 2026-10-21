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
#include "ui/Icons.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QEvent>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QVariantMap>

namespace edita {

namespace {

const QString kIdKey = QStringLiteral("id");
const QString kStateKey = QStringLiteral("state");

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
    setTabIcon(index, stateIcon(state));
    setTabToolTip(index, buffer.isUntitled() ? buffer.displayName() : buffer.filePath());
    setTabData(index, QVariantMap{{kIdKey, QVariant::fromValue(buffer.id())}, {kStateKey, static_cast<int>(state)}});
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
}

QIcon DocTabBar::iconForState(TabState state)
{
    return stateIcon(state);
}

void DocTabBar::changeEvent(QEvent* event)
{
    // Claro u oscuro: los indicadores con los colores nuevos
    if (event->type() == QEvent::PaletteChange) {
        for (int i = 0; i < count(); ++i)
            setTabIcon(i, stateIcon(stateAt(i)));
    }
    QTabBar::changeEvent(event);
}

void DocTabBar::contextMenuEvent(QContextMenuEvent* event)
{
    const int index = tabAt(event->pos());
    if (index >= 0)
        emit tabContextMenuRequested(index, event->globalPos());
}

} // namespace edita
