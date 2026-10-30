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

#include "doc/DocumentManager.h"

#include "core/Backup.h"

#include <ScintillaEditBase.h>

// Internos de Scintilla para vigilar los documentos sin pasar por una vista
// (mismo orden que BoostRegExSearch.cxx)
#include <optional>
#include <vector>
#include "Scintilla.h"
#include "ScintillaTypes.h"
#include "ScintillaMessages.h"
#include "Debugging.h"
#include "Geometry.h"
#include "Platform.h"
#include "ILoader.h"
#include "ILexer.h"
#include "Position.h"
#include "UniqueString.h"
#include "SplitVector.h"
#include "Partitioning.h"
#include "RunStyles.h"
#include "ContractionState.h"
#include "CellBuffer.h"
#include "CharClassify.h"
#include "Decoration.h"
#include "CaseFolder.h"
#include "CharacterCategoryMap.h"
#include "Document.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace edita {

namespace {

QString absolutePath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

Scintilla::Internal::Document* asDocument(SciDocument doc)
{
    // SCI_CREATEDOCUMENT devuelve el IDocumentEditable* de un Document
    return static_cast<Scintilla::Internal::Document*>(reinterpret_cast<Scintilla::IDocumentEditable*>(doc));
}

} // namespace

// Observa los documentos de Scintilla directamente: así se cuentan también las
// ediciones hechas desde la vista de búsqueda invisible o con varias vistas a la vez.
class DocumentManager::Watcher : public Scintilla::Internal::DocWatcher
{
public:
    using Document = Scintilla::Internal::Document;
    using DocModification = Scintilla::Internal::DocModification;

    void NotifyModifyAttempt(Document*, void*) override {}
    void NotifySavePoint(Document*, void*, bool) override {}
    void NotifyModified(Document*, DocModification mh, void* userData) override
    {
        using Scintilla::ModificationFlags;
        if (Scintilla::FlagSet(mh.modificationType, ModificationFlags::InsertText | ModificationFlags::DeleteText))
            ++static_cast<Buffer*>(userData)->_contentVersion;
    }
    void NotifyDeleted(Document*, void*) noexcept override {}
    void NotifyStyleNeeded(Document*, void*, Sci::Position) override {}
    void NotifyErrorOccurred(Document*, void*, Scintilla::Status) override {}
    void NotifyGroupCompleted(Document*, void*) noexcept override {}
};

// Asigna un documento a la vista auxiliar mientras dura el objeto y después
// la devuelve a su documento propio (como _scratchDocDefault en N++).
class DocumentManager::ScratchDocument
{
public:
    ScratchDocument(const DocumentManager& manager, SciDocument document)
        : _manager(manager)
    {
        send(SCI_SETDOCPOINTER, 0, document);
    }
    ~ScratchDocument() { send(SCI_SETDOCPOINTER, 0, _manager._scratchDefaultDocument); }
    ScratchDocument(const ScratchDocument&) = delete;
    ScratchDocument& operator=(const ScratchDocument&) = delete;

    sptr_t send(unsigned int message, uptr_t wParam = 0, sptr_t lParam = 0) const
    {
        return _manager._scratch->send(message, wParam, lParam);
    }

    std::string_view characters() const
    {
        const auto length = static_cast<std::size_t>(send(SCI_GETLENGTH));
        const auto* data = reinterpret_cast<const char*>(send(SCI_GETCHARACTERPOINTER));
        return {data, length};
    }

private:
    const DocumentManager& _manager;
};

DocumentManager::DocumentManager(QObject* parent)
    : QObject(parent)
    , _scratch(std::make_unique<ScintillaEditBase>())
    , _watcher(std::make_unique<Watcher>())
{
    // FileManager::init: se guarda una referencia extra al documento propio de
    // la vista auxiliar para que no se destruya al asignarle otros.
    _scratchDefaultDocument = _scratch->send(SCI_GETDOCPOINTER);
    _scratch->send(SCI_ADDREFDOCUMENT, 0, _scratchDefaultDocument);
}

DocumentManager::~DocumentManager()
{
    for (const auto& buffer : _buffers)
        releaseDocument(*buffer);
    _scratch->send(SCI_RELEASEDOCUMENT, 0, _scratchDefaultDocument);
}

void DocumentManager::notify(Buffer* buffer, int mask)
{
    emit bufferChanged(buffer->id(), mask);
}

SciDocument DocumentManager::createDocument(bool largeFile)
{
    // Archivos grandes sin estilos: la mitad de memoria (FileManager::loadFile)
    const sptr_t options = largeFile ? (SC_DOCUMENTOPTION_STYLES_NONE | SC_DOCUMENTOPTION_TEXT_LARGE)
                                     : SC_DOCUMENTOPTION_TEXT_LARGE;
    // El documento nace con una referencia, que es la de este DocumentManager
    return _scratch->send(SCI_CREATEDOCUMENT, 0, options);
}

Buffer* DocumentManager::addBuffer(SciDocument document)
{
    auto buffer = std::unique_ptr<Buffer>(new Buffer(this, _nextId++, document));
    asDocument(document)->AddWatcher(_watcher.get(), buffer.get());
    _buffers.push_back(std::move(buffer));
    return _buffers.back().get();
}

void DocumentManager::releaseDocument(Buffer& buffer)
{
    asDocument(buffer._document)->RemoveWatcher(_watcher.get(), &buffer);
    _scratch->send(SCI_RELEASEDOCUMENT, 0, buffer._document);
}

void DocumentManager::setUntitledName(BufferId id, const QString& name)
{
    Buffer* buf = buffer(id);
    if (!buf || !buf->isUntitled() || buf->_untitledName == name)
        return;
    buf->_untitledName = name;
    notify(buf, BufferChange::Filename);
}

BufferId DocumentManager::newEmptyDocument()
{
    const SciDocument doc = createDocument(false);
    if (!doc)
        return kInvalidBuffer;
    {
        ScratchDocument scratch(*this, doc);
        scratch.send(SCI_SETCODEPAGE, SC_CP_UTF8);
        scratch.send(SCI_SETEOLMODE, static_cast<uptr_t>(_settings.load.defaultEol));
    }
    const int number = nextUntitledNumber();
    Buffer* buffer = addBuffer(doc);
    buffer->_untitledNumber = number;
    buffer->_status = DocStatus::Unnamed;
    buffer->_encoding = _settings.newDocEncoding; // NewDocDefaultSettings (UTF-8 sin BOM)
    buffer->_eol = _settings.load.defaultEol;
    buffer->_language = _settings.newDocLanguage;
    const BufferId id = buffer->id();
    emit bufferAdded(id);
    return id;
}

int DocumentManager::nextUntitledNumber() const
{
    // El número libre más bajo, como FileManager::nextUntitledNewNumber
    std::vector<int> used;
    for (const auto& buffer : _buffers) {
        if (!buffer->isUntitled())
            continue;
        if (buffer->_untitledName.isEmpty()) {
            used.push_back(buffer->_untitledNumber);
        } else if (buffer->_untitledName.startsWith(_settings.untitledPrefix)) {
            // Restaurado de una sesión con un número ocupado entonces: "nuevo 1" sigue en uso
            bool ok = false;
            const int n = buffer->_untitledName.mid(_settings.untitledPrefix.size()).toInt(&ok);
            if (ok)
                used.push_back(n);
        }
    }
    int number = 1;
    while (std::find(used.begin(), used.end(), number) != used.end())
        ++number;
    return number;
}

bool DocumentManager::fillDocument(SciDocument doc, const QString& path, int forcedCodepage, LoadResult& result)
{
    ScratchDocument scratch(*this, doc);
    const bool wasReadOnly = scratch.send(SCI_GETREADONLY) != 0;
    scratch.send(SCI_SETREADONLY, 0);
    scratch.send(SCI_CLEARALL);
    scratch.send(SCI_SETUNDOCOLLECTION, 0); // cargar no se puede deshacer
    scratch.send(SCI_SETCODEPAGE, SC_CP_UTF8);

    // Reservar memoria para todo el archivo de una vez (FileManager::loadFileData)
    const qint64 size = QFileInfo(path).size();
    if (size > 0)
        scratch.send(SCI_ALLOCATE, static_cast<uptr_t>(size + std::min<qint64>(1 << 20, size / 6)));

    LoadOptions options = _settings.load;
    options.forcedCodepage = forcedCodepage;
    bool outOfMemory = false;
    result = loadTextFile(path, options, [&](std::string_view chunk) {
        scratch.send(SCI_APPENDTEXT, chunk.size(), reinterpret_cast<sptr_t>(chunk.data()));
        const sptr_t status = scratch.send(SCI_GETSTATUS);
        outOfMemory = status > SC_STATUS_OK && status < SC_STATUS_WARN_START;
        return !outOfMemory;
    });
    if (outOfMemory) {
        result.error = FileError::ReadFailed;
        result.errorString = tr("Not enough memory to open %1").arg(path);
        scratch.send(SCI_SETSTATUS, SC_STATUS_OK);
    }

    if (result.ok())
        scratch.send(SCI_SETEOLMODE, static_cast<uptr_t>(result.eol));
    scratch.send(SCI_EMPTYUNDOBUFFER);
    scratch.send(SCI_SETSAVEPOINT);
    scratch.send(SCI_SETUNDOCOLLECTION, 1);
    if (wasReadOnly)
        scratch.send(SCI_SETREADONLY, 1);
    return result.ok();
}

void DocumentManager::applyLoadResult(Buffer& buffer, const QString& path, const LoadResult& result)
{
    const QFileInfo info(path);
    buffer._filePath = absolutePath(path);
    buffer._untitledNumber = 0;
    buffer._untitledName.clear();
    buffer._status = DocStatus::Regular;
    buffer._encoding = result.encoding;
    buffer._eol = result.eol;
    buffer._dirty = false;
    buffer._fileReadOnly = !info.isWritable();
    buffer._lastModified = info.lastModified();
    buffer._backupVersion = buffer._contentVersion;
}

void DocumentManager::assignDetectedLanguage(Buffer& buffer, const QString& path) const
{
    buffer._userLanguage.clear();
    if (_userLanguageDetector && !buffer._largeFile) {
        if (const QString name = _userLanguageDetector(path); !name.isEmpty()) {
            buffer._language = LangType::User;
            buffer._userLanguage = name;
            return;
        }
    }
    buffer._language = detectLanguage(buffer, path);
}

LangType DocumentManager::detectLanguage(const Buffer& buffer, const QString& path) const
{
    // Archivos grandes: sin lenguaje, para no colorear (N++: _isLargeFile)
    if (!_languageDetector || buffer._largeFile)
        return LangType::Text;
    // Basta con el principio: la detección por contenido solo mira la primera línea
    constexpr std::size_t kPeek = 1024;
    const ScratchDocument scratch(*this, buffer._document);
    const std::string_view text = scratch.characters();
    return _languageDetector(path, text.substr(0, std::min(kPeek, text.size())));
}

BufferId DocumentManager::loadFile(const QString& path, int forcedCodepage, QString* error)
{
    const QFileInfo info(path);
    const bool largeFile = info.size() >= _settings.largeFileThreshold;
    const SciDocument doc = createDocument(largeFile);
    if (!doc) {
        if (error)
            *error = tr("Scintilla could not create the document");
        return kInvalidBuffer;
    }

    LoadResult result;
    if (!fillDocument(doc, path, forcedCodepage, result)) {
        _scratch->send(SCI_RELEASEDOCUMENT, 0, doc);
        if (error)
            *error = result.errorString;
        return kInvalidBuffer;
    }

    Buffer* buffer = addBuffer(doc);
    buffer->_largeFile = largeFile;
    applyLoadResult(*buffer, path, result);
    assignDetectedLanguage(*buffer, buffer->_filePath);
    const BufferId id = buffer->id();
    emit bufferAdded(id);
    return id;
}

bool DocumentManager::reloadBuffer(BufferId id, int forcedCodepage, QString* error)
{
    Buffer* buf = buffer(id);
    if (!buf || buf->isUntitled()) {
        if (error)
            *error = tr("The document has no associated file");
        return false;
    }
    LoadResult result;
    if (!fillDocument(buf->_document, buf->_filePath, forcedCodepage, result)) {
        if (error)
            *error = result.errorString;
        return false;
    }
    applyLoadResult(*buf, buf->_filePath, result);
    notify(buf, BufferChange::Unicode | BufferChange::Format | BufferChange::Dirty
                    | BufferChange::Timestamp | BufferChange::ReadOnly | BufferChange::Status);
    return true;
}

SaveResult DocumentManager::saveBuffer(BufferId id, const QString& path, bool isCopy)
{
    Buffer* buf = buffer(id);
    if (!buf) {
        SaveResult result;
        result.error = FileError::WriteFailed;
        result.errorString = tr("Nonexistent document");
        return result;
    }

    ScratchDocument scratch(*this, buf->_document);
    const SaveResult result = saveTextFile(path, scratch.characters(), buf->_encoding);
    if (!result.ok() || isCopy)
        return result;

    scratch.send(SCI_SETSAVEPOINT);
    deleteBackup(id); // ya no hay cambios sin guardar (FileManager::saveBuffer)

    const QString newPath = absolutePath(path);
    const QFileInfo info(newPath);
    int mask = BufferChange::Dirty | BufferChange::Timestamp | BufferChange::Status | BufferChange::ReadOnly;
    if (newPath != buf->_filePath)
        mask |= BufferChange::Filename;
    buf->_filePath = newPath;
    buf->_untitledNumber = 0;
    buf->_untitledName.clear();
    buf->_status = DocStatus::Regular;
    buf->_dirty = false;
    buf->_fileReadOnly = !info.isWritable();
    buf->_lastModified = info.lastModified();
    // Al cambiar de nombre se vuelve a detectar el lenguaje, salvo si el usuario
    // lo eligió a mano; sin cambio de nombre, solo si sigue siendo texto normal
    if ((mask & BufferChange::Filename) ? !buf->_languageFromMenu : buf->_language == LangType::Text) {
        const LangType oldLang = buf->_language;
        const QString oldUser = buf->_userLanguage;
        assignDetectedLanguage(*buf, newPath);
        if (buf->_language != oldLang || buf->_userLanguage != oldUser)
            mask |= BufferChange::Language;
    }
    notify(buf, mask);
    return result;
}

void DocumentManager::closeBuffer(BufferId id, bool keepBackup)
{
    const auto it = std::find_if(_buffers.begin(), _buffers.end(),
        [id](const auto& b) { return b->id() == id; });
    if (it == _buffers.end())
        return;
    if (!keepBackup)
        deleteBackup(id);
    releaseDocument(**it);
    _buffers.erase(it);
    emit bufferClosed(id);
}

void DocumentManager::convertEol(BufferId id, EolType eol)
{
    Buffer* buf = buffer(id);
    if (!buf || eol == EolType::Unknown)
        return;
    bool modified = false;
    {
        ScratchDocument scratch(*this, buf->_document);
        scratch.send(SCI_CONVERTEOLS, static_cast<uptr_t>(eol));
        scratch.send(SCI_SETEOLMODE, static_cast<uptr_t>(eol));
        modified = scratch.send(SCI_GETMODIFY) != 0;
    }
    int mask = BufferChange::Format;
    if (modified != buf->_dirty)
        mask |= BufferChange::Dirty;
    buf->_eol = eol;
    buf->_dirty = modified;
    notify(buf, mask);
}

void DocumentManager::syncDirtyState(BufferId id)
{
    Buffer* buf = buffer(id);
    if (!buf)
        return;
    const ScratchDocument scratch(*this, buf->_document);
    buf->setDirty(scratch.send(SCI_GETMODIFY) != 0);
}

Buffer* DocumentManager::buffer(BufferId id) const
{
    const auto it = std::find_if(_buffers.begin(), _buffers.end(),
        [id](const auto& b) { return b->id() == id; });
    return it == _buffers.end() ? nullptr : it->get();
}

BufferId DocumentManager::findByPath(const QString& path) const
{
    const QString wanted = absolutePath(path);
    for (const auto& buffer : _buffers) {
        if (!buffer->isUntitled() && buffer->_filePath == wanted)
            return buffer->id();
    }
    return kInvalidBuffer;
}

void DocumentManager::markDirtyUntilSaved(BufferId id)
{
    Buffer* buf = buffer(id);
    if (!buf)
        return;
    {
        // Punto de guardado inalcanzable: deshacer no lo vuelve a dejar "sin cambios"
        const ScratchDocument scratch(*this, buf->_document);
        scratch.send(SCI_SETUNDOSAVEPOINT, static_cast<uptr_t>(-1));
    }
    buf->setDirty(true);
}

bool DocumentManager::backupBuffer(BufferId id, const QString& dir, bool* pathChanged)
{
    if (pathChanged)
        *pathChanged = false;
    Buffer* buf = buffer(id);
    if (!buf || buf->_largeFile)
        return false;
    if (!buf->_dirty) {
        // Sin cambios pendientes: la copia sobra
        if (!buf->_backupPath.isEmpty()) {
            deleteBackup(id);
            if (pathChanged)
                *pathChanged = true;
        }
        return true;
    }
    if (buf->_contentVersion == buf->_backupVersion && !buf->_backupPath.isEmpty() && QFileInfo::exists(buf->_backupPath))
        return true;

    QString path = buf->_backupPath;
    if (path.isEmpty()) {
        if (!QDir().mkpath(dir))
            return false;
        path = snapshotBackupPath(dir, buf->displayName(), QDateTime::currentDateTime());
    }
    // Los juegos de caracteres de un byte se copian en UTF-8 para no perder lo que
    // no quepa en ellos; la sesión guarda el codepage y se recupera al restaurar
    const TextEncoding encoding = buf->_encoding.isUnicode() ? buf->_encoding : TextEncoding::utf8();
    SaveResult result;
    {
        const ScratchDocument scratch(*this, buf->_document);
        result = saveTextFile(path, scratch.characters(), encoding);
    }
    if (!result.ok())
        return false;
    if (path != buf->_backupPath && pathChanged)
        *pathChanged = true;
    buf->_backupPath = path;
    buf->_backupVersion = buf->_contentVersion;
    return true;
}

void DocumentManager::deleteBackup(BufferId id)
{
    Buffer* buf = buffer(id);
    if (!buf || buf->_backupPath.isEmpty())
        return;
    QFile::remove(buf->_backupPath);
    buf->_backupPath.clear();
}

BufferId DocumentManager::loadFromBackup(const QString& path, const QString& backupPath, int codepage,
                                         const QDateTime& originalLastModified, QString* error)
{
    const SciDocument doc = createDocument(false);
    if (!doc) {
        if (error)
            *error = tr("Scintilla could not create the document");
        return kInvalidBuffer;
    }
    LoadResult result;
    // Ver backupBuffer: con codepage, la copia está en UTF-8
    if (!fillDocument(doc, backupPath, codepage > 0 ? kCodepageUtf8 : -1, result)) {
        _scratch->send(SCI_RELEASEDOCUMENT, 0, doc);
        if (error)
            *error = result.errorString;
        return kInvalidBuffer;
    }
    {
        const ScratchDocument scratch(*this, doc);
        scratch.send(SCI_SETUNDOSAVEPOINT, static_cast<uptr_t>(-1)); // con cambios hasta guardar
    }

    Buffer* buffer = addBuffer(doc);
    if (QDir::isAbsolutePath(path)) {
        const QFileInfo info(path);
        buffer->_filePath = absolutePath(path);
        buffer->_status = DocStatus::Regular;
        buffer->_fileReadOnly = info.exists() && !info.isWritable();
        // La fecha de cuando se hizo la copia: si el archivo cambió desde entonces,
        // la siguiente comprobación lo detecta y pregunta
        buffer->_lastModified = originalLastModified.isValid() ? originalLastModified : info.lastModified();
    } else {
        buffer->_status = DocStatus::Unnamed;
        const QString& prefix = _settings.untitledPrefix;
        bool isNumber = false;
        const int number = path.startsWith(prefix) ? path.mid(prefix.size()).toInt(&isNumber) : 0;
        if (isNumber && number > 0 && !findUntitledNumber(number))
            buffer->_untitledNumber = number;
        else
            buffer->_untitledName = path.isEmpty() ? prefix + QString::number(nextUntitledNumber()) : path;
    }
    buffer->_encoding = codepage > 0 ? TextEncoding::fromCodepage(codepage) : result.encoding;
    buffer->_eol = result.eol;
    buffer->_dirty = true;
    buffer->_backupPath = backupPath;
    buffer->_backupVersion = buffer->_contentVersion;
    assignDetectedLanguage(*buffer, buffer->_filePath.isEmpty() ? buffer->displayName() : buffer->_filePath);
    const BufferId id = buffer->id();
    emit bufferAdded(id);
    return id;
}

bool DocumentManager::findUntitledNumber(int number) const
{
    return std::any_of(_buffers.begin(), _buffers.end(),
        [number](const auto& b) { return b->isUntitled() && b->_untitledName.isEmpty() && b->_untitledNumber == number; });
}

DocumentManager::FileState DocumentManager::checkFileState(BufferId id)
{
    // Buffer::checkFileState
    Buffer* buf = buffer(id);
    if (!buf || buf->isUntitled())
        return FileState::Unchanged;
    const QFileInfo info(buf->_filePath);
    if (buf->_status != DocStatus::Deleted && !info.exists()) {
        buf->_status = DocStatus::Deleted;
        buf->_fileReadOnly = false;
        notify(buf, BufferChange::Status | BufferChange::ReadOnly);
        markDirtyUntilSaved(id);
        return FileState::Deleted;
    }
    if (!info.exists())
        return FileState::Unchanged;

    int mask = BufferChange::None;
    if (const bool readOnly = !info.isWritable(); readOnly != buf->_fileReadOnly) {
        buf->_fileReadOnly = readOnly;
        mask |= BufferChange::ReadOnly;
    }
    FileState state = FileState::Unchanged;
    // Reapareció ("ha vuelto de la tumba") o cambió la fecha
    if (buf->_status == DocStatus::Deleted || info.lastModified() != buf->_lastModified) {
        buf->_status = DocStatus::Modified;
        buf->_lastModified = info.lastModified();
        mask |= BufferChange::Status | BufferChange::Timestamp;
        state = FileState::Modified;
    }
    if (mask != BufferChange::None)
        notify(buf, mask);
    return state;
}

std::string DocumentManager::text(BufferId id) const
{
    const Buffer* buf = buffer(id);
    if (!buf)
        return {};
    const ScratchDocument scratch(*this, buf->_document);
    return std::string(scratch.characters());
}

} // namespace edita
