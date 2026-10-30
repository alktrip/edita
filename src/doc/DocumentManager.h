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

#include "core/TextFile.h"
#include "doc/Buffer.h"

#include <QCoreApplication>
#include <QObject>

#include <functional>
#include <memory>
#include <string>
#include <vector>

class ScintillaEditBase;

namespace edita {

struct DocumentSettings
{
    LoadOptions load;
    // A partir de este tamaño el documento se crea sin estilos (mitad de memoria).
    // NPP_STYLING_FILESIZE_LIMIT_DEFAULT = 200 MB.
    std::int64_t largeFileThreshold = 200LL * 1024 * 1024;
    // Prefijo de los documentos sin título ("new 1", "new 2"…; traducido)
    QString untitledPrefix = QCoreApplication::translate("DocumentManager", "new ");
    // NewDocDefaultSettings: codificación y lenguaje de los documentos nuevos (el
    // fin de línea es load.defaultEol)
    TextEncoding newDocEncoding = TextEncoding::utf8();
    LangType newDocLanguage = LangType::Text;
};

// Dueño de todos los documentos abiertos. Port de FileManager de Notepad++
// (ScintillaComponent/Buffer.cpp).
//
// Igual que en N++, los documentos son documentos de Scintilla independientes de
// las vistas: se crean y se rellenan con una vista auxiliar invisible ("scratch"),
// y las vistas visibles los muestran con SCI_SETDOCPOINTER. Así hay N documentos
// para solo dos editores, y cada documento conserva su propio historial de deshacer.
//
// Dentro de Scintilla el texto siempre está en UTF-8, con SCI_SETCODEPAGE y
// SCI_SETEOLMODE fijados por documento al cargar.
class DocumentManager : public QObject
{
    Q_OBJECT

public:
    // Decide el lenguaje de un documento a partir de su ruta y del principio
    // de su texto (Buffer::setFileName + detectLanguageFromTextBeginning en N++).
    // Lo proporciona la capa de sintaxis; sin detector, todo es texto normal.
    using LanguageDetector = std::function<LangType(const QString& filePath, std::string_view textStart)>;

    explicit DocumentManager(QObject* parent = nullptr);
    ~DocumentManager() override;

    DocumentSettings& settings() { return _settings; }
    const DocumentSettings& settings() const { return _settings; }

    void setLanguageDetector(LanguageDetector detector) { _languageDetector = std::move(detector); }
    // Lenguajes definidos por el usuario: se miran antes (Buffer::setFileName).
    // Devuelve el nombre del lenguaje o vacío.
    using UserLanguageDetector = std::function<QString(const QString& filePath)>;
    void setUserLanguageDetector(UserLanguageDetector detector) { _userLanguageDetector = std::move(detector); }

    // Documento vacío "nuevo N", con el número libre más bajo (nextUntitledNewNumber)
    BufferId newEmptyDocument();
    // El nombre que muestra un documento sin archivo («main.cpp (HEAD)» al comparar)
    void setUntitledName(BufferId id, const QString& name);

    // Carga un archivo en un documento nuevo. `forcedCodepage` = -1 para detectar.
    // No comprueba si ya estaba abierto: para eso está findByPath() (doOpen en N++).
    // Devuelve kInvalidBuffer si falla y deja el motivo en `error`.
    BufferId loadFile(const QString& path, int forcedCodepage = -1, QString* error = nullptr);

    // Vuelve a leer el archivo del disco. Con `forcedCodepage` reinterpreta los
    // bytes con otra codificación ("Codificación > Juegos de caracteres").
    // Se pierde el historial de deshacer, como en N++.
    bool reloadBuffer(BufferId id, int forcedCodepage = -1, QString* error = nullptr);

    // Guarda en `path` con la codificación y el fin de línea del documento.
    // Con `isCopy` ("Guardar una copia como…") el documento no cambia de ruta.
    SaveResult saveBuffer(BufferId id, const QString& path, bool isCopy = false);

    // Libera el documento de Scintilla. Las vistas que lo muestren deben haber
    // cambiado de documento antes. Borra su copia "snapshot" salvo con
    // `keepBackup` (al salir en modo snapshot, para restaurarla en el próximo arranque).
    void closeBuffer(BufferId id, bool keepBackup = false);

    // --- Modo "snapshot" (FileManager::backupCurrentBuffer…) -----------------------
    // Escribe la copia de un documento con cambios que la copia aún no tiene; si
    // ya no tiene cambios sin guardar, borra la copia. `pathChanged` indica si la
    // ruta de la copia cambió (hay que guardar la sesión). false si falla al escribir.
    bool backupBuffer(BufferId id, const QString& dir, bool* pathChanged = nullptr);
    void deleteBackup(BufferId id);
    // Restaura un documento desde su copia (doOpen con backupFileName en N++).
    // Si `path` existe, el documento queda asociado a ese archivo pero con el
    // contenido de la copia y modificado; si no existe y no es una ruta absoluta
    // es un documento sin título con ese nombre ("nuevo 3"). `codepage` es el de
    // la sesión; `originalLastModified`, la fecha del archivo cuando se hizo la copia.
    BufferId loadFromBackup(const QString& path, const QString& backupPath, int codepage,
                            const QDateTime& originalLastModified, QString* error = nullptr);

    // --- Cambios externos (Buffer::checkFileState) ----------------------------------
    enum class FileState { Unchanged, Modified, Deleted };
    // Compara con el disco: si el archivo desapareció el documento pasa a
    // DocStatus::Deleted y queda modificado; si cambió (o reapareció) pasa a
    // DocStatus::Modified y se anota la nueva fecha para no avisar dos veces.
    FileState checkFileState(BufferId id);
    // El documento queda modificado hasta que se guarde, aunque se deshagan los
    // cambios (al elegir no recargar un archivo que cambió fuera, como N++)
    void markDirtyUntilSaved(BufferId id);

    // Convierte todos los finales de línea del documento y fija los nuevos
    // (Editar > Conversión de fin de línea). Marca el documento como modificado.
    void convertEol(BufferId id, EolType eol);

    // Vuelve a leer del documento de Scintilla si está modificado respecto al
    // último guardado (SCI_GETMODIFY) y actualiza Buffer::isDirty().
    void syncDirtyState(BufferId id);

    Buffer* buffer(BufferId id) const;
    std::size_t count() const { return _buffers.size(); }
    Buffer* bufferAt(std::size_t index) const { return _buffers.at(index).get(); }
    // Documento abierto con esa ruta (comparando rutas absolutas); kInvalidBuffer si no
    BufferId findByPath(const QString& path) const;

    // Texto completo del documento en UTF-8
    std::string text(BufferId id) const;

signals:
    void bufferChanged(edita::BufferId id, int mask);
    void bufferAdded(edita::BufferId id);
    void bufferClosed(edita::BufferId id);

private:
    friend class Buffer;
    class ScratchDocument;
    class Watcher;

    void notify(Buffer* buffer, int mask);
    SciDocument createDocument(bool largeFile);
    Buffer* addBuffer(SciDocument document);
    void releaseDocument(Buffer& buffer);
    bool fillDocument(SciDocument doc, const QString& path, int forcedCodepage, LoadResult& result);
    void applyLoadResult(Buffer& buffer, const QString& path, const LoadResult& result);
    LangType detectLanguage(const Buffer& buffer, const QString& path) const;
    // detectLanguage y, si es un lenguaje del usuario, su nombre en el buffer
    void assignDetectedLanguage(Buffer& buffer, const QString& path) const;
    int nextUntitledNumber() const;
    bool findUntitledNumber(int number) const;

    DocumentSettings _settings;
    std::unique_ptr<ScintillaEditBase> _scratch;
    std::unique_ptr<Watcher> _watcher;
    SciDocument _scratchDefaultDocument = 0;
    LanguageDetector _languageDetector;
    UserLanguageDetector _userLanguageDetector;
    std::vector<std::unique_ptr<Buffer>> _buffers;
    BufferId _nextId = 1;
};

} // namespace edita
