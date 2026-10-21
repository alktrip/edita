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

#include "search/FileSearch.h"

#include <algorithm>

#include <QDir>

#include <QElapsedTimer>
#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QThread>

namespace edita {

namespace {

// Comodines * y ? sin distinguir mayúsculas (recursivo con memoria del último *)
bool globMatch(const QString& name, const QString& pattern)
{
    qsizetype n = 0;
    qsizetype p = 0;
    qsizetype starP = -1;
    qsizetype starN = 0;
    while (n < name.size()) {
        if (p < pattern.size()
            && (pattern[p] == QLatin1Char('?') || pattern[p].toCaseFolded() == name[n].toCaseFolded())) {
            ++n;
            ++p;
        } else if (p < pattern.size() && pattern[p] == QLatin1Char('*')) {
            starP = p++;
            starN = n;
        } else if (starP >= 0) {
            p = starP + 1;
            n = ++starN;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == QLatin1Char('*'))
        ++p;
    return p == pattern.size();
}

void collectFiles(const QString& dir, int level, const std::vector<QString>& patterns, bool recursive,
                  bool includeHidden, const std::atomic<bool>* cancel, QStringList& out)
{
    ++level;
    const QDir qdir(dir);
    const QFileInfoList entries = qdir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                                                     QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& entry : entries) {
        if (cancel && cancel->load())
            return;
        const QString name = entry.fileName();
        if (entry.isDir()) {
            const bool hidden = name.startsWith(QLatin1Char('.'));
            if ((hidden && !includeHidden) || !recursive || entry.isSymLink())
                continue;
            if (!isExcludedDirectory(name, patterns, level))
                collectFiles(entry.absoluteFilePath(), level, patterns, recursive, includeHidden, cancel, out);
        } else if (entry.isFile() && matchesFileFilters(name, patterns)) {
            out << entry.absoluteFilePath();
        }
    }
}

} // namespace

std::vector<QString> parseFilters(const QString& filters)
{
    std::vector<QString> patterns;
    for (const QString& p : filters.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts))
        patterns.push_back(p);
    if (patterns.empty()) {
        patterns.push_back(QStringLiteral("*.*"));
    } else {
        const bool allExclusions = std::all_of(patterns.begin(), patterns.end(),
                                               [](const QString& p) { return p.startsWith(QLatin1Char('!')); });
        if (allExclusions)
            patterns.insert(patterns.begin(), QStringLiteral("*.*"));
    }
    return patterns;
}

bool wildcardMatch(const QString& name, const QString& pattern)
{
    if (pattern == QLatin1String("*.*"))
        return true;
    return globMatch(name, pattern);
}

bool matchesFileFilters(const QString& fileName, const std::vector<QString>& patterns)
{
    bool matched = false;
    for (const QString& pattern : patterns) {
        if (pattern.size() > 1 && pattern.startsWith(QLatin1Char('!'))) {
            // Las exclusiones de directorio ("!\x", "!+\x") no aplican a archivos
            if (pattern[1] == QLatin1Char('\\') || pattern[1] == QLatin1Char('/') || pattern[1] == QLatin1Char('+'))
                continue;
            if (wildcardMatch(fileName, pattern.mid(1)))
                return false;
            continue;
        }
        if (wildcardMatch(fileName, pattern))
            matched = true;
    }
    return matched;
}

bool isExcludedDirectory(const QString& dirName, const std::vector<QString>& patterns, int level)
{
    auto isSep = [](QChar c) { return c == QLatin1Char('\\') || c == QLatin1Char('/'); };
    for (const QString& pattern : patterns) {
        if (pattern.size() > 3 && pattern[0] == QLatin1Char('!') && pattern[1] == QLatin1Char('+') && isSep(pattern[2])) {
            if (wildcardMatch(dirName, pattern.mid(3)))
                return true;
        } else if (pattern.size() > 2 && pattern[0] == QLatin1Char('!') && isSep(pattern[1])) {
            if (level == 1 && wildcardMatch(dirName, pattern.mid(2)))
                return true;
        }
    }
    return false;
}

QStringList listMatchingFiles(const QString& directory, const std::vector<QString>& patterns, bool recursive,
                              bool includeHidden, const std::atomic<bool>* cancel)
{
    QStringList files;
    collectFiles(directory, 0, patterns, recursive, includeHidden, cancel, files);
    return files;
}

FileSearchSummary runFileSearch(const FileSearchRequest& request, const std::atomic<bool>& cancel,
                                const std::function<void(FileHits&&)>& onFile,
                                const std::function<void(int, int)>& onProgress)
{
    FileSearchSummary summary;
    const QStringList files = listMatchingFiles(request.directory, parseFilters(request.filters), request.recursive,
                                                request.includeHidden, &cancel);
    const int total = static_cast<int>(files.size());
    QElapsedTimer sinceProgress;
    sinceProgress.start();

    for (int i = 0; i < total; ++i) {
        if (cancel.load()) {
            summary.cancelled = true;
            break;
        }
        const QString& path = files[i];
        const auto open = request.openDocuments.find(QDir::cleanPath(path));
        const bool isOpen = open != request.openDocuments.end();
        if (request.replace && isOpen)
            continue; // los documentos abiertos los reemplaza la ventana, en el propio documento

        std::string text;
        LoadResult loaded;
        if (isOpen) {
            text = open->second;
        } else {
            loaded = loadTextFile(path, request.load, text);
            if (!loaded.ok()) {
                summary.errors << loaded.errorString;
                continue;
            }
        }
        ++summary.filesSearched;

        DocumentSearchTarget target(text);
        FileHits hits;
        hits.path = path;
        RangeResult result;
        if (request.replace) {
            result = processRange(target, request.options, RangeOperation::ReplaceAll, 0, target.length());
            if (result.count > 0 && !result.invalidRegex) {
                // Se guarda con la misma codificación; el texto conserva sus finales de línea
                const SaveResult saved = saveTextFile(path, target.allText(), loaded.encoding);
                if (!saved.ok())
                    summary.errors << saved.errorString;
            }
        } else {
            hits.lines = findAllLines(target, request.options, 0, target.length(), result);
        }
        if (result.invalidRegex) {
            summary.invalidRegex = true;
            summary.regexError = QString::fromStdString(result.regexError);
            break;
        }
        hits.count = result.count;
        if (hits.count > 0) {
            ++summary.filesWithHits;
            summary.totalHits += hits.count;
            if (onFile)
                onFile(std::move(hits));
        }
        if (onProgress && (sinceProgress.elapsed() > 50 || i + 1 == total)) {
            onProgress(i + 1, total);
            sinceProgress.restart();
        }
    }
    if (cancel.load())
        summary.cancelled = true;
    return summary;
}

FileSearchJob::FileSearchJob(FileSearchRequest request, QObject* parent)
    : QObject(parent)
    , _request(std::move(request))
    , _cancel(std::make_shared<std::atomic<bool>>(false))
{
}

FileSearchJob::~FileSearchJob()
{
    cancel();
    wait();
}

void FileSearchJob::start()
{
    if (_thread)
        return;
    QPointer<FileSearchJob> self(this);
    const auto cancelFlag = _cancel;
    const FileSearchRequest request = _request;
    _thread = QThread::create([self, cancelFlag, request] {
        const FileSearchSummary summary = runFileSearch(
            request, *cancelFlag,
            [self](FileHits&& hits) {
                // Se pasa al hilo del objeto; si ya no existe, se descarta
                QMetaObject::invokeMethod(self.data(), [self, hits = std::move(hits)] {
                    if (self)
                        emit self->fileFound(hits);
                }, Qt::QueuedConnection);
            },
            [self](int done, int total) {
                QMetaObject::invokeMethod(self.data(), [self, done, total] {
                    if (self)
                        emit self->progress(done, total);
                }, Qt::QueuedConnection);
            });
        QMetaObject::invokeMethod(self.data(), [self, summary] {
            if (self)
                emit self->finished(summary);
        }, Qt::QueuedConnection);
    });
    _thread->start();
}

void FileSearchJob::cancel()
{
    _cancel->store(true);
}

bool FileSearchJob::isRunning() const
{
    return _thread && _thread->isRunning();
}

void FileSearchJob::wait()
{
    if (_thread) {
        _thread->wait();
        delete _thread;
        _thread = nullptr;
    }
}

} // namespace edita
