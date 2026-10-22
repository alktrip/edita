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

#pragma once

#include "doc/Buffer.h"

#include <QTabBar>

#include <vector>

namespace edita {

// Barra de pestañas de documentos. Port de DocTabView (DocTabView.cpp): cada
// pestaña guarda el BufferId de su documento y muestra su estado con un icono
// (modificado: un punto; solo lectura: un candado), donde N++ pone disquetes.
class DocTabBar : public QTabBar
{
    Q_OBJECT

public:
    enum class TabState { Saved, Dirty, ReadOnly };

    explicit DocTabBar(QWidget* parent = nullptr);

    int addBuffer(const Buffer& buffer);
    // Sustituye el documento de una pestaña (para no hacer parpadear la barra)
    void setBuffer(int index, const Buffer& buffer);
    // TabBar de config.xml: doble clic en una pestaña la cierra (doubleClick2Close)
    void setDoubleClickCloses(bool on) { _doubleClickCloses = on; }
    // Refresca nombre, icono y tooltip tras un cambio del documento
    void updateBuffer(const Buffer& buffer);

    int indexOf(BufferId id) const;
    BufferId bufferAt(int index) const;
    std::vector<BufferId> buffers() const;
    TabState stateAt(int index) const;
    // El indicador de un estado (punto, candado o hueco), con los colores del modo actual
    static QIcon iconForState(TabState state);
    // El icono del tipo de archivo con el estado (o el de estado, si están desactivados)
    static QIcon iconFor(const QString& fileName, TabState state);
    // Al cambiar la preferencia de los iconos o el tema
    void refreshIcons();

signals:
    // Doble clic en la zona vacía de la barra (N++: nuevo documento)
    void newDocumentRequested();
    void tabContextMenuRequested(int index, const QPoint& globalPos);

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void applyBuffer(int index, const Buffer& buffer);

    bool _doubleClickCloses = false;
};

} // namespace edita
