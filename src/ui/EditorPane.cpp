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

#include "ui/EditorPane.h"

#include "doc/DocumentManager.h"
#include "ui/DocTabBar.h"
#include "ui/EditCommands.h"
#include "ui/EditorView.h"

#include <QSignalBlocker>
#include <QVBoxLayout>

namespace edita {

EditorPane::EditorPane(DocumentManager& documents, QWidget* parent)
    : QWidget(parent)
    , _documents(documents)
    , _tabs(new DocTabBar(this))
    , _editor(new EditorView(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_tabs);
    layout->addWidget(_editor, 1);

    connect(_tabs, &QTabBar::currentChanged, this, &EditorPane::onTabActivated);
    connect(_tabs, &QTabBar::tabCloseRequested, this, [this](int index) {
        emit closeRequested(_tabs->bufferAt(index));
    });
    connect(_tabs, &DocTabBar::newDocumentRequested, this, &EditorPane::newDocumentRequested);
    connect(_tabs, &DocTabBar::tabContextMenuRequested, this, [this](int index, const QPoint& pos) {
        emit tabContextMenuRequested(_tabs->bufferAt(index), pos);
    });
    connect(&_documents, &DocumentManager::bufferChanged, this, &EditorPane::onBufferChanged);

    // El estado de guardado lo notifica Scintilla a la vista que muestra el documento
    connect(_editor, &ScintillaEditBase::savePointChanged, this, [this](bool dirty) {
        if (Buffer* buf = _documents.buffer(_current))
            buf->setDirty(dirty);
    });
}

bool EditorPane::contains(BufferId id) const
{
    return _tabs->indexOf(id) >= 0;
}

int EditorPane::count() const
{
    return _tabs->count();
}

std::vector<BufferId> EditorPane::buffers() const
{
    return _tabs->buffers();
}

void EditorPane::addBuffer(BufferId id)
{
    const Buffer* buf = _documents.buffer(id);
    if (!buf || contains(id))
        return;
    {
        const QSignalBlocker blocker(_tabs);
        _tabs->addBuffer(*buf);
    }
    // Con la ventana aún oculta la barra puede quedarse con la altura en caché a 0
    _tabs->updateGeometry();
    if (_current == kInvalidBuffer)
        activateBuffer(id);
}

void EditorPane::replaceBuffer(BufferId oldId, BufferId newId)
{
    const int index = _tabs->indexOf(oldId);
    const Buffer* buf = _documents.buffer(newId);
    if (index < 0 || !buf)
        return;
    {
        const QSignalBlocker blocker(_tabs);
        _tabs->setBuffer(index, *buf);
    }
    if (_current == oldId)
        _current = kInvalidBuffer; // el documento saliente se va a cerrar: no guardar su posición
    activateBuffer(newId);
}

void EditorPane::syncCurrentPosition()
{
    if (Buffer* old = _documents.buffer(_current))
        old->setPosition(_editor, _editor->currentPosition());
}

void EditorPane::activateBuffer(BufferId id)
{
    Buffer* buf = _documents.buffer(id);
    if (!buf || id == _current)
        return;

    syncCurrentPosition();
    _current = id;
    _editor->sci(SCI_SETDOCPOINTER, 0, buf->document());
    _editor->sci(SCI_SETREADONLY, buf->isReadOnly() ? 1 : 0);
    // Como N++: los estilos antes de restaurar la posición (cambian la altura de línea)
    if (_styler)
        _styler(*_editor, *buf);
    _editor->restorePosition(buf->position(_editor));
    restoreHiddenLines(*_editor); // lo oculto vive en el documento; que se oculte es de la vista
    _editor->updateLineNumberWidth(true);

    const int index = _tabs->indexOf(id);
    if (index >= 0 && _tabs->currentIndex() != index) {
        const QSignalBlocker blocker(_tabs);
        _tabs->setCurrentIndex(index);
    }
    emit currentBufferChanged(id);
}

void EditorPane::reorder(const std::vector<BufferId>& order)
{
    const QSignalBlocker blocker(_tabs);
    int target = 0;
    for (const BufferId id : order) {
        const int index = _tabs->indexOf(id);
        if (index < 0)
            continue;
        if (index != target)
            _tabs->moveTab(index, target);
        ++target;
    }
}

void EditorPane::removeBuffer(BufferId id)
{
    const int index = _tabs->indexOf(id);
    if (index < 0)
        return;

    if (id == _current) {
        // Activar la pestaña de la derecha o, si no hay, la de la izquierda
        const int next = index + 1 < _tabs->count() ? index + 1 : index - 1;
        if (next >= 0) {
            activateBuffer(_tabs->bufferAt(next));
        } else {
            // Última pestaña: la vista vuelve a un documento propio vacío
            _current = kInvalidBuffer;
            _editor->sci(SCI_SETDOCPOINTER, 0, 0);
        }
    }
    {
        const QSignalBlocker blocker(_tabs);
        _tabs->removeTab(_tabs->indexOf(id));
    }
    _tabs->updateGeometry();
    if (Buffer* buf = _documents.buffer(id))
        buf->forgetView(_editor);
}

void EditorPane::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    _tabs->updateGeometry();
}

void EditorPane::restyle()
{
    const Buffer* buf = _documents.buffer(_current);
    if (!buf || !_styler)
        return;
    _styler(*_editor, *buf);
    _editor->updateLineNumberWidth(true);
}

void EditorPane::onTabActivated(int index)
{
    if (index < 0)
        return;
    activateBuffer(_tabs->bufferAt(index));
    _editor->setFocus();
}

void EditorPane::onBufferChanged(BufferId id, int mask)
{
    const Buffer* buf = _documents.buffer(id);
    if (!buf || !contains(id))
        return;
    _tabs->updateBuffer(*buf);
    if (id == _current && (mask & BufferChange::ReadOnly))
        _editor->sci(SCI_SETREADONLY, buf->isReadOnly() ? 1 : 0);
    if (id == _current && (mask & BufferChange::Language))
        restyle();
}

} // namespace edita
