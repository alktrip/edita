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

#include "core/TextFile.h"

#include "core/Codec.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace edita {

namespace {

LoadResult loadError(FileError error, const QString& message)
{
    LoadResult result;
    result.error = error;
    result.errorString = message;
    return result;
}

SaveResult saveError(FileError error, const QString& message)
{
    SaveResult result;
    result.error = error;
    result.errorString = message;
    return result;
}

// Lee hasta `size` bytes; devuelve false si hubo error de lectura
bool readBlock(QFile& file, std::string& block, std::size_t size)
{
    block.resize(size);
    const qint64 n = file.read(block.data(), static_cast<qint64>(size));
    if (n < 0)
        return false;
    block.resize(static_cast<std::size_t>(n));
    return true;
}

} // namespace

LoadResult loadTextFile(const QString& path, const LoadOptions& options, const Utf8Sink& sink)
{
    const QFileInfo info(path);
    if (!info.exists())
        return loadError(FileError::NotFound, QCoreApplication::translate("TextFile", "The file does not exist: %1").arg(path));
    if (info.isDir())
        return loadError(FileError::IsDirectory, QCoreApplication::translate("TextFile", "It is a directory: %1").arg(path));

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        // Qt informa EACCES como OpenError, no como PermissionsError
        const FileError error = !info.isReadable() ? FileError::PermissionDenied : FileError::ReadFailed;
        return loadError(error, QCoreApplication::translate("TextFile", "Cannot open %1: %2").arg(path, file.errorString()));
    }

    LoadResult result;
    result.fileSize = file.size();

    std::string block;
    if (!readBlock(file, block, options.blockSize))
        return loadError(FileError::ReadFailed, QCoreApplication::translate("TextFile", "Error reading %1: %2").arg(path, file.errorString()));

    const bool isWholeFile = file.atEnd();
    result.encoding = detectEncoding(block, isWholeFile, options.forcedCodepage, options.detection);

    Decoder decoder(result.encoding);
    if (!decoder.isValid()) {
        return loadError(FileError::UnsupportedEncoding,
            QCoreApplication::translate("TextFile", "The encoding %1 is not available on this system")
                .arg(QString::fromStdString(encodingDisplayName(result.encoding))));
    }

    EolDetector eolDetector;
    std::string decoded;
    for (;;) {
        const bool final = file.atEnd();
        decoded.clear();
        decoder.decode(block, final, decoded);
        eolDetector.feed(decoded);
        if (!decoded.empty() && !sink(decoded))
            return loadError(FileError::Aborted, QCoreApplication::translate("TextFile", "Loading canceled"));
        if (final)
            break;
        if (!readBlock(file, block, options.blockSize))
            return loadError(FileError::ReadFailed, QCoreApplication::translate("TextFile", "Error reading %1: %2").arg(path, file.errorString()));
    }

    const EolType detectedEol = eolDetector.finish(EolType::Unknown);
    result.eolDetected = detectedEol != EolType::Unknown;
    result.eol = result.eolDetected ? detectedEol : options.defaultEol;
    result.invalidSequences = decoder.invalidSequences();
    return result;
}

LoadResult loadTextFile(const QString& path, const LoadOptions& options, std::string& utf8Out)
{
    utf8Out.clear();
    return loadTextFile(path, options, [&utf8Out](std::string_view chunk) {
        utf8Out.append(chunk);
        return true;
    });
}

SaveResult saveTextFile(const QString& path, std::string_view utf8, const TextEncoding& encoding)
{
    Encoder encoder(encoding);
    if (!encoder.isValid()) {
        return saveError(FileError::UnsupportedEncoding,
            QCoreApplication::translate("TextFile", "The encoding %1 is not available on this system")
                .arg(QString::fromStdString(encodingDisplayName(encoding))));
    }

    // Escribir en el destino del enlace para no sustituir el enlace por un archivo
    const QFileInfo info(path);
    const QString target = info.isSymLink() ? info.symLinkTarget() : path;
    if (QFileInfo(target).isDir())
        return saveError(FileError::IsDirectory, QCoreApplication::translate("TextFile", "It is a directory: %1").arg(target));

    const bool existed = QFileInfo::exists(target);
    const QFileDevice::Permissions permissions = existed ? QFile::permissions(target) : QFileDevice::Permissions{};

    QSaveFile file(target);
    file.setDirectWriteFallback(true);
    if (!file.open(QIODevice::WriteOnly)) {
        const QFileInfo targetInfo(target);
        const bool denied = targetInfo.exists() ? !targetInfo.isWritable()
                                                : !QFileInfo(targetInfo.absolutePath()).isWritable();
        const FileError error = denied ? FileError::PermissionDenied : FileError::WriteFailed;
        return saveError(error, QCoreApplication::translate("TextFile", "Cannot write %1: %2").arg(target, file.errorString()));
    }

    // Se convierte por bloques para no duplicar en memoria documentos grandes
    constexpr std::size_t kSaveBlock = 1 << 20;
    std::string encoded;
    std::size_t pos = 0;
    do {
        const std::size_t len = std::min(kSaveBlock, utf8.size() - pos);
        const bool final = pos + len == utf8.size();
        encoded.clear();
        encoder.encode(utf8.substr(pos, len), final, encoded);
        if (!encoded.empty()
            && file.write(encoded.data(), static_cast<qint64>(encoded.size())) != static_cast<qint64>(encoded.size())) {
            file.cancelWriting();
            return saveError(FileError::WriteFailed, QCoreApplication::translate("TextFile", "Error writing %1: %2").arg(target, file.errorString()));
        }
        pos += len;
    } while (pos < utf8.size());

    if (!file.commit())
        return saveError(FileError::WriteFailed, QCoreApplication::translate("TextFile", "Error saving %1: %2").arg(target, file.errorString()));

    if (existed && QFile::permissions(target) != permissions)
        QFile::setPermissions(target, permissions);

    SaveResult result;
    result.unmappableChars = encoder.unmappableChars();
    return result;
}

} // namespace edita
