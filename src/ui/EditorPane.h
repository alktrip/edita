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

#include <QWidget>

#include <functional>
#include <vector>

namespace edita {

class DocTabBar;
class DocumentManager;
class EditorView;

// Una "vista" de Notepad++: una barra de pestañas (DocTabView) y un editor
// (ScintillaEditView) que muestra el documento de la pestaña activa cambiando
// de documento con SCI_SETDOCPOINTER. La Fase 5 usará dos, una por lado de la
// vista dividida.
class EditorPane : public QWidget
{
    Q_OBJECT

public:
    // Aplica lexer y estilos del documento a la vista (defineDocType en N++)
    using Styler = std::function<void(EditorView& view, const Buffer& buffer)>;

    explicit EditorPane(DocumentManager& documents, QWidget* parent = nullptr);

    void setStyler(Styler styler) { _styler = std::move(styler); }
    // Vuelve a aplicar los estilos al documento visible (cambió el tema o los XML)
    void restyle();

    DocTabBar* tabBar() const { return _tabs; }
    EditorView* editor() const { return _editor; }

    BufferId currentBuffer() const { return _current; }
    bool contains(BufferId id) const;
    int count() const;
    std::vector<BufferId> buffers() const;

    // Añade una pestaña al final, sin activarla
    void addBuffer(BufferId id);
    // Pone `newId` en la pestaña de `oldId` y lo activa (loadBufferIntoView de N++)
    void replaceBuffer(BufferId oldId, BufferId newId);
    // ScintillaEditView::activateBuffer: guarda la posición del documento saliente
    // y restaura la del entrante
    void activateBuffer(BufferId id);
    // Quita la pestaña; si era la activa, activa antes la vecina
    void removeBuffer(BufferId id);
    // Guarda en el Buffer la posición actual del editor (saveCurrentPos)
    void syncCurrentPosition();
    // Pone las pestañas en ese orden (los documentos que no estén en la lista, al final)
    void reorder(const std::vector<BufferId>& order);

signals:
    void currentBufferChanged(edita::BufferId id);
    void closeRequested(edita::BufferId id);
    void newDocumentRequested();
    void tabContextMenuRequested(edita::BufferId id, const QPoint& globalPos);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void onTabActivated(int index);
    void onBufferChanged(BufferId id, int mask);

    DocumentManager& _documents;
    DocTabBar* _tabs;
    EditorView* _editor;
    BufferId _current = kInvalidBuffer;
    Styler _styler;
};

} // namespace edita
