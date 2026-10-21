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
#include "search/SearchEngine.h"

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class QThread;

namespace edita {

// --- Filtros (cutString, matchInList, matchInExcludeDirList, getAndValidatePatterns) ---

// Separa los filtros por espacios. Sin filtros = "*.*"; si todos son
// exclusiones ("!…") se antepone "*.*".
std::vector<QString> parseFilters(const QString& filters);

// PathMatchSpec de Windows: comodines * y ?, sin distinguir mayúsculas;
// "*.*" coincide también con nombres sin punto
bool wildcardMatch(const QString& name, const QString& pattern);

// ¿Pasa el archivo los filtros? Una exclusión "!patrón" que coincide manda.
bool matchesFileFilters(const QString& fileName, const std::vector<QString>& patterns);

// ¿Se excluye el directorio? "!\patrón" solo en el primer nivel y "!+\patrón"
// en todos (en Linux también valen "!/" y "!+/")
bool isExcludedDirectory(const QString& dirName, const std::vector<QString>& patterns, int level);

// getMatchedFileNames: archivos de `directory` que pasan los filtros, en orden
// alfabético y en profundidad, como N++. Los directorios ocultos (en Linux, los
// que empiezan por punto) solo con `includeHidden`. No sigue enlaces simbólicos
// a directorios (evita ciclos).
QStringList listMatchingFiles(const QString& directory, const std::vector<QString>& patterns, bool recursive,
                              bool includeHidden, const std::atomic<bool>* cancel = nullptr);

// --- Buscar / reemplazar en archivos -------------------------------------------------

struct FileSearchRequest
{
    QString directory;
    QString filters;
    bool recursive = true;
    bool includeHidden = false;
    SearchOptions options;
    bool replace = false;
    LoadOptions load;
    // Archivos abiertos en el editor: se busca en su texto actual (con los
    // cambios sin guardar), como N++. Al reemplazar se saltan: los trata la ventana.
    std::map<QString, std::string> openDocuments;
};

struct FileHits
{
    QString path;
    int count = 0;
    std::vector<LineHit> lines; // vacío al reemplazar
};

struct FileSearchSummary
{
    int filesSearched = 0;
    int filesWithHits = 0;
    int totalHits = 0;
    bool cancelled = false;
    bool invalidRegex = false;
    QString regexError;
    QStringList errors; // archivos que no se pudieron leer o guardar
};

// Ejecuta la búsqueda en el hilo actual. `onFile` recibe cada archivo con
// coincidencias; `onProgress(hechos, total)` se llama de vez en cuando.
FileSearchSummary runFileSearch(const FileSearchRequest& request, const std::atomic<bool>& cancel,
                                const std::function<void(FileHits&&)>& onFile,
                                const std::function<void(int done, int total)>& onProgress = {});

// La misma búsqueda en un hilo de trabajo. Las señales llegan en el hilo del objeto.
class FileSearchJob : public QObject
{
    Q_OBJECT

public:
    explicit FileSearchJob(FileSearchRequest request, QObject* parent = nullptr);
    ~FileSearchJob() override;

    void start();
    void cancel();
    bool isRunning() const;
    void wait();
    const FileSearchRequest& request() const { return _request; }

signals:
    void progress(int done, int total);
    void fileFound(const edita::FileHits& hits);
    void finished(const edita::FileSearchSummary& summary);

private:
    FileSearchRequest _request;
    std::shared_ptr<std::atomic<bool>> _cancel;
    QThread* _thread = nullptr;
};

} // namespace edita

Q_DECLARE_METATYPE(edita::FileHits)
Q_DECLARE_METATYPE(edita::FileSearchSummary)
