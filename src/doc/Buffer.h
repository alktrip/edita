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

#include "core/Encoding.h"
#include "core/Eol.h"
#include "core/LangType.h"
#include "core/Position.h"

#include <QDateTime>
#include <QString>

#include <cstdint>
#include <map>

namespace edita {

class DocumentManager;

// Identificador estable de un documento abierto. En N++ BufferID es el propio
// puntero Buffer*; aquí es un número que nunca se reutiliza, así un id viejo no
// puede apuntar por error a otro documento.
using BufferId = std::uint64_t;
inline constexpr BufferId kInvalidBuffer = 0;

// Puntero opaco a un documento de Scintilla (SCI_CREATEDOCUMENT), con el tipo
// de los parámetros de los mensajes SCI_*.
using SciDocument = std::intptr_t;

// Qué ha cambiado en un Buffer. Mismos valores que BufferStatusInfo de N++
// (Buffer.h), que los plugins conocen.
namespace BufferChange {
enum : int
{
    None = 0x000,
    Language = 0x001,
    Dirty = 0x002,
    Format = 0x004,   // fin de línea
    Unicode = 0x008,  // codificación
    ReadOnly = 0x010,
    Status = 0x020,
    Timestamp = 0x040,
    Filename = 0x080,
    RecentTag = 0x100,
    Lexing = 0x200,
    Mask = 0x3FF,
};
}

// DocFileStatus de N++
enum class DocStatus
{
    Regular,  // asociado a un archivo existente
    Unnamed,  // "nuevo 1", sin archivo
    Deleted,  // el archivo desapareció del disco
    Modified, // el archivo cambió fuera del editor
};

// Un documento abierto: un documento de Scintilla más sus metadatos. Port de la
// clase Buffer de Notepad++ (ScintillaComponent/Buffer.h), sin lo que llega en
// fases posteriores (plegado, marcadores).
//
// Los cambios se notifican con DocumentManager::bufferChanged(id, máscara).
class Buffer
{
public:
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    BufferId id() const { return _id; }
    SciDocument document() const { return _document; }

    // Ruta absoluta; vacía en documentos sin título
    const QString& filePath() const { return _filePath; }
    // Nombre para la pestaña: el del archivo o "nuevo N"
    QString displayName() const;
    bool isUntitled() const { return _status == DocStatus::Unnamed; }
    int untitledNumber() const { return _untitledNumber; }
    DocStatus status() const { return _status; }

    const TextEncoding& encoding() const { return _encoding; }
    // "Convertir a…": cambia cómo se guardará. El texto no cambia (dentro es UTF-8).
    void setEncoding(const TextEncoding& encoding);

    EolType eol() const { return _eol; }

    LangType language() const { return _language; }
    // `fromMenu`: el usuario lo eligió a mano; ya no se redetecta al cambiar el
    // nombre del archivo (Buffer::_hasLangBeenSetFromMenu en N++)
    void setLanguage(LangType lang, bool fromMenu = false);
    bool isLanguageSetFromMenu() const { return _languageFromMenu; }
    // Con LangType::User, el nombre del lenguaje del usuario (Buffer::_userLangExt en N++)
    const QString& userLanguage() const { return _userLanguage; }
    void setUserLanguage(const QString& name, bool fromMenu = false);

    bool isDirty() const { return _dirty; }
    void setDirty(bool dirty);

    // Solo lectura por el sistema de archivos o por decisión del usuario
    bool isFileReadOnly() const { return _fileReadOnly; }
    bool isUserReadOnly() const { return _userReadOnly; }
    void setUserReadOnly(bool readOnly);
    bool isReadOnly() const { return _fileReadOnly || _userReadOnly; }

    // Archivos de más de 200 MB: sin estilos (N++: _largeFileRestriction)
    bool isLargeFile() const { return _largeFile; }

    // Fecha de modificación del archivo al cargarlo o guardarlo
    const QDateTime& lastModified() const { return _lastModified; }

    // Contador que aumenta con cada inserción o borrado en el documento, venga
    // de la vista que venga (Buffer::_isModified de N++, pero sin perder cambios)
    std::uint64_t contentVersion() const { return _contentVersion; }
    // Copia "snapshot" de los cambios sin guardar; vacía si no hay
    const QString& backupPath() const { return _backupPath; }
    // Hay cambios sin guardar que la copia aún no tiene
    bool needsBackup() const { return _dirty && _contentVersion != _backupVersion; }

    // Posición de cada vista que muestra el documento (Buffer::setPosition en N++).
    // `view` solo se usa como clave.
    void setPosition(const void* view, const Position& position) { _positions[view] = position; }
    Position position(const void* view) const;
    void forgetView(const void* view) { _positions.erase(view); }

private:
    friend class DocumentManager;
    Buffer(DocumentManager* manager, BufferId id, SciDocument document);

    void notify(int mask);

    DocumentManager* _manager;
    BufferId _id;
    SciDocument _document;
    QString _filePath;
    int _untitledNumber = 0;
    QString _untitledName; // nombre restaurado de una sesión que no sigue el patrón "nuevo N"
    DocStatus _status = DocStatus::Unnamed;
    TextEncoding _encoding;
    EolType _eol = EolType::Unix;
    LangType _language = LangType::Text;
    bool _languageFromMenu = false;
    QString _userLanguage;
    bool _dirty = false;
    bool _fileReadOnly = false;
    bool _userReadOnly = false;
    bool _largeFile = false;
    QDateTime _lastModified;
    std::uint64_t _contentVersion = 0;
    std::uint64_t _backupVersion = 0;
    QString _backupPath;
    std::map<const void*, Position> _positions;
};

} // namespace edita
