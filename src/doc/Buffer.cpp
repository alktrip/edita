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

#include "doc/Buffer.h"

#include "doc/DocumentManager.h"

#include <QFileInfo>

namespace edita {

Buffer::Buffer(DocumentManager* manager, BufferId id, SciDocument document)
    : _manager(manager)
    , _id(id)
    , _document(document)
{
}

QString Buffer::displayName() const
{
    if (isUntitled() && !_untitledName.isEmpty())
        return _untitledName;
    if (isUntitled())
        return _manager->settings().untitledPrefix + QString::number(_untitledNumber);
    return QFileInfo(_filePath).fileName();
}

void Buffer::setEncoding(const TextEncoding& encoding)
{
    if (encoding == _encoding)
        return;
    _encoding = encoding;
    // Como en N++, convertir deja el documento pendiente de guardar
    _dirty = true;
    notify(BufferChange::Unicode | BufferChange::Dirty);
}

void Buffer::setLanguage(LangType lang, bool fromMenu)
{
    _languageFromMenu = _languageFromMenu || fromMenu;
    if (lang == _language && (lang != LangType::User || _userLanguage.isEmpty()))
        return;
    _language = lang;
    _userLanguage.clear();
    notify(BufferChange::Language);
}

void Buffer::setUserLanguage(const QString& name, bool fromMenu)
{
    _languageFromMenu = _languageFromMenu || fromMenu;
    if (_language == LangType::User && _userLanguage == name)
        return;
    _language = LangType::User;
    _userLanguage = name;
    notify(BufferChange::Language);
}

void Buffer::setDirty(bool dirty)
{
    if (dirty == _dirty)
        return;
    _dirty = dirty;
    notify(BufferChange::Dirty);
}

void Buffer::setUserReadOnly(bool readOnly)
{
    if (readOnly == _userReadOnly)
        return;
    _userReadOnly = readOnly;
    notify(BufferChange::ReadOnly);
}

Position Buffer::position(const void* view) const
{
    const auto it = _positions.find(view);
    return it == _positions.end() ? Position{} : it->second;
}

void Buffer::notify(int mask)
{
    _manager->notify(this, mask);
}

} // namespace edita
