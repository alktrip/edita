// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// La parte de Win32 que no necesita ventanas, para los plugins de Notepad++ compilados
// con npp_compat.h: archivos, archivos .ini, conversiones de texto, rutas, entorno,
// tiempo, memoria global, portapapeles, MessageBox, ShellExecute, strsafe.h y el CRT
// de MSVC. Cada función tiene sus variantes A (UTF-8) y W (wchar_t); no depende de
// UNICODE, que solo elige el nombre genérico en npp_compat.h.
//
// Se compila dentro de cada plugin (con visibilidad oculta), así que sus símbolos no
// chocan entre plugins ni con Edita.

#define EDITA_NPP_COMPAT_GLUE
#include "npp_compat.h"

// npp_compat.h redefine estos nombres con la semántica de MSVC; aquí hacen falta los de C
#undef swprintf
#undef vswprintf

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QUrl>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cwchar>
#include <map>
#include <string>
#include <thread>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

thread_local DWORD t_lastError = ERROR_SUCCESS;

// --- Cadenas ----------------------------------------------------------------------------

QString fromA(const char* s, qsizetype length = -1)
{
    return s ? QString::fromUtf8(s, length) : QString();
}

QString fromW(const wchar_t* s, qsizetype length = -1)
{
    if (!s)
        return {};
    if (length < 0)
        length = static_cast<qsizetype>(std::wcslen(s));
    return QString::fromWCharArray(s, length);
}

template<typename Ch> QString fromT(const Ch* s);
template<> QString fromT<char>(const char* s) { return fromA(s); }
template<> QString fromT<wchar_t>(const wchar_t* s) { return fromW(s); }

template<typename Ch> std::basic_string<Ch> toT(const QString& s);
template<> std::string toT<char>(const QString& s) { return s.toStdString(); }
template<> std::wstring toT<wchar_t>(const QString& s) { return s.toStdWString(); }

// Una ruta de Windows: '\' → '/'
QString nativePath(QString path)
{
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return path;
}

QByteArray encodedPath(const QString& path)
{
    return QFile::encodeName(nativePath(path));
}

template<typename Ch> std::size_t length(const Ch* s);
template<> std::size_t length<char>(const char* s) { return std::strlen(s); }
template<> std::size_t length<wchar_t>(const wchar_t* s) { return std::wcslen(s); }

// Copia con la semántica de GetPrivateProfileString/GetModuleFileName: como mucho
// size - 1 caracteres y el nulo; devuelve los copiados. En UTF-8 no se corta un carácter.
template<typename Ch>
DWORD copyTruncated(const std::basic_string<Ch>& text, Ch* buffer, DWORD size)
{
    if (!buffer || size == 0)
        return 0;
    std::size_t n = std::min<std::size_t>(text.size(), size - 1);
    if constexpr (sizeof(Ch) == 1) {
        while (n > 0 && n < text.size() && (static_cast<unsigned char>(text[n]) & 0xC0) == 0x80)
            --n;
    }
    std::copy_n(text.data(), n, buffer);
    buffer[n] = Ch(0);
    return static_cast<DWORD>(n);
}

// Copia con la semántica de GetCurrentDirectory/GetEnvironmentVariable: si no cabe,
// devuelve el tamaño necesario contando el nulo; si cabe, la longitud sin el nulo
template<typename Ch>
DWORD copyOrRequired(const std::basic_string<Ch>& text, Ch* buffer, DWORD size)
{
    if (!buffer || size <= text.size())
        return static_cast<DWORD>(text.size() + 1);
    std::copy_n(text.data(), text.size(), buffer);
    buffer[text.size()] = Ch(0);
    return static_cast<DWORD>(text.size());
}

// Copia con la semántica de ExpandEnvironmentStrings: el tamaño cuenta el nulo, quepa o no
template<typename Ch>
DWORD copyCounted(const std::basic_string<Ch>& text, Ch* buffer, DWORD size)
{
    if (buffer && size > text.size()) {
        std::copy_n(text.data(), text.size(), buffer);
        buffer[text.size()] = Ch(0);
    }
    return static_cast<DWORD>(text.size() + 1);
}

// Una lista de cadenas terminadas en nulo y un nulo final (GetPrivateProfileSectionNames…)
template<typename Ch>
DWORD copyMultiString(const QStringList& items, Ch* buffer, DWORD size)
{
    if (!buffer || size < 2) {
        if (buffer && size == 1)
            buffer[0] = Ch(0);
        return 0;
    }
    DWORD used = 0;
    for (const QString& item : items) {
        const std::basic_string<Ch> text = toT<Ch>(item);
        if (used + text.size() + 2 > size) {
            // Como Windows: lo que cabe, con dos nulos al final, y size - 2
            const DWORD room = size - 2 > used ? size - 2 - used : 0;
            std::copy_n(text.data(), room, buffer + used);
            buffer[size - 2] = Ch(0);
            buffer[size - 1] = Ch(0);
            return size - 2;
        }
        std::copy_n(text.data(), text.size(), buffer + used);
        used += static_cast<DWORD>(text.size());
        buffer[used++] = Ch(0);
    }
    buffer[used] = Ch(0);
    return used;
}

DWORD errnoToWin32(int error)
{
    switch (error) {
    case ENOENT: return ERROR_FILE_NOT_FOUND;
    case ENOTDIR: return ERROR_PATH_NOT_FOUND;
    case EACCES:
    case EPERM:
    case EROFS: return ERROR_ACCESS_DENIED;
    case EEXIST: return ERROR_FILE_EXISTS;
    case ENOMEM: return ERROR_NOT_ENOUGH_MEMORY;
    case EBADF: return ERROR_INVALID_HANDLE;
    default: return ERROR_INVALID_PARAMETER;
    }
}

BOOL failWithErrno()
{
    t_lastError = errnoToWin32(errno);
    return FALSE;
}

// --- printf de MSVC -------------------------------------------------------------------------

// Traduce un formato de MSVC al de C: I64 → ll, I32 → (nada), I → z, w → l y, en las
// funciones anchas, %s/%c → %ls/%lc y %S/%C/%hs/%hc → %s/%c
template<typename Ch>
std::basic_string<Ch> translateFormat(const Ch* format, bool wide)
{
    std::basic_string<Ch> out;
    if (!format)
        return out;
    const Ch* p = format;
    while (*p) {
        if (*p != Ch('%')) {
            out += *p++;
            continue;
        }
        out += *p++;
        if (*p == Ch('%')) {
            out += *p++;
            continue;
        }
        while (*p == Ch('-') || *p == Ch('+') || *p == Ch(' ') || *p == Ch('#') || *p == Ch('0') || *p == Ch('\''))
            out += *p++;
        while (*p == Ch('*') || (*p >= Ch('0') && *p <= Ch('9')))
            out += *p++;
        if (*p == Ch('.')) {
            out += *p++;
            while (*p == Ch('*') || (*p >= Ch('0') && *p <= Ch('9')))
                out += *p++;
        }
        // Longitud
        std::basic_string<Ch> lengthModifier;
        bool narrow = false; // h delante de s/c
        bool wideMark = false; // l o w delante de s/c
        for (;;) {
            if (p[0] == Ch('I') && p[1] == Ch('6') && p[2] == Ch('4')) {
                lengthModifier += Ch('l');
                lengthModifier += Ch('l');
                p += 3;
            } else if (p[0] == Ch('I') && p[1] == Ch('3') && p[2] == Ch('2')) {
                p += 3;
            } else if (p[0] == Ch('I')) {
                lengthModifier += Ch('z');
                ++p;
            } else if (*p == Ch('w')) {
                wideMark = true;
                ++p;
            } else if (*p == Ch('h')) {
                narrow = true;
                lengthModifier += *p++;
            } else if (*p == Ch('l')) {
                wideMark = true;
                lengthModifier += *p++;
            } else if (*p == Ch('L') || *p == Ch('z') || *p == Ch('j') || *p == Ch('t') || *p == Ch('q')) {
                lengthModifier += *p++;
            } else {
                break;
            }
        }
        const Ch conversion = *p;
        if (!conversion)
            break;
        ++p;
        const bool isString = conversion == Ch('s') || conversion == Ch('c');
        const bool isOtherString = conversion == Ch('S') || conversion == Ch('C');
        if (isString || isOtherString) {
            const Ch base = (conversion == Ch('s') || conversion == Ch('S')) ? Ch('s') : Ch('c');
            bool asWide;
            if (wideMark)
                asWide = true;
            else if (narrow)
                asWide = false;
            else
                asWide = wide ? isString : isOtherString;
            if (asWide)
                out += Ch('l');
            out += base;
            continue;
        }
        out += lengthModifier;
        out += conversion;
    }
    return out;
}

int formatWide(wchar_t* buffer, std::size_t count, const wchar_t* format, va_list args)
{
    if (!buffer || count == 0)
        return -1;
    const std::wstring translated = translateFormat<wchar_t>(format, true);
    const int written = std::vswprintf(buffer, count, translated.c_str(), args);
    if (written < 0)
        buffer[count - 1] = L'\0';
    return written;
}

int formatNarrow(char* buffer, std::size_t count, const char* format, va_list args)
{
    const std::string translated = translateFormat<char>(format, false);
    return std::vsnprintf(buffer, count, translated.c_str(), args);
}

// --- Archivos ----------------------------------------------------------------------------

constexpr uint32_t kFileMagic = 0x46494C45;   // "FILE"
constexpr uint32_t kFindMagic = 0x46494E44;   // "FIND"

struct FileHandle
{
    uint32_t magic = kFileMagic;
    int fd = -1;
};

struct FindEntry
{
    QString name;
    struct stat info {};
};

struct FindHandle
{
    uint32_t magic = kFindMagic;
    QString directory;
    std::vector<FindEntry> entries;
    std::size_t next = 0;
};

FileHandle* fileHandle(HANDLE handle)
{
    auto* file = static_cast<FileHandle*>(handle);
    if (!file || handle == INVALID_HANDLE_VALUE || file->magic != kFileMagic)
        return nullptr;
    return file;
}

FILETIME fileTime(const struct timespec& time)
{
    // 100 ns desde el 1 de enero de 1601
    const uint64_t ticks = (static_cast<uint64_t>(time.tv_sec) + 11644473600ULL) * 10000000ULL
                           + static_cast<uint64_t>(time.tv_nsec) / 100;
    return FILETIME{static_cast<DWORD>(ticks & 0xFFFFFFFFu), static_cast<DWORD>(ticks >> 32)};
}

DWORD attributesOf(const QString& name, const struct stat& info, const QByteArray& path)
{
    DWORD attributes = S_ISDIR(info.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_ARCHIVE;
    if (name.startsWith(QLatin1Char('.')) && name != QLatin1String(".") && name != QLatin1String(".."))
        attributes |= FILE_ATTRIBUTE_HIDDEN;
    if (!path.isEmpty() && ::access(path.constData(), W_OK) != 0)
        attributes |= FILE_ATTRIBUTE_READONLY;
    return attributes;
}

#ifdef __APPLE__
const struct timespec& modificationTime(const struct stat& info) { return info.st_mtimespec; }
const struct timespec& accessTime(const struct stat& info) { return info.st_atimespec; }
#else
const struct timespec& modificationTime(const struct stat& info) { return info.st_mtim; }
const struct timespec& accessTime(const struct stat& info) { return info.st_atim; }
#endif

template<typename FindData>
void fillFindData(const FindEntry& entry, const QByteArray& path, FindData* data)
{
    std::memset(data, 0, sizeof(*data));
    data->dwFileAttributes = attributesOf(entry.name, entry.info, path);
    data->ftCreationTime = fileTime(modificationTime(entry.info));
    data->ftLastWriteTime = fileTime(modificationTime(entry.info));
    data->ftLastAccessTime = fileTime(accessTime(entry.info));
    const auto size = static_cast<uint64_t>(entry.info.st_size);
    data->nFileSizeHigh = static_cast<DWORD>(size >> 32);
    data->nFileSizeLow = static_cast<DWORD>(size & 0xFFFFFFFFu);
    using Ch = std::remove_reference_t<decltype(data->cFileName[0])>;
    copyTruncated(toT<Ch>(entry.name), data->cFileName, static_cast<DWORD>(_countof(data->cFileName)));
}

struct FindState
{
    QString directory;
    std::vector<FindEntry> entries;
};

// FindFirstFile: el patrón es la última parte de la ruta (sin distinguir mayúsculas, como
// en Windows); "*.*" es todo, también lo que no tiene punto
bool findFiles(const QString& pattern, FindState& state)
{
    const QString path = nativePath(pattern);
    const int slash = static_cast<int>(path.lastIndexOf(QLatin1Char('/')));
    state.directory = slash >= 0 ? path.left(slash + 1) : QString();
    QString mask = slash >= 0 ? path.mid(slash + 1) : path;
    if (mask == QLatin1String("*.*"))
        mask = QStringLiteral("*");
    const QByteArray dirPath = QFile::encodeName(state.directory.isEmpty() ? QStringLiteral(".") : state.directory);
    const bool wildcard = mask.contains(QLatin1Char('*')) || mask.contains(QLatin1Char('?'));
    if (!wildcard) {
        FindEntry entry;
        entry.name = mask;
        if (::stat(QFile::encodeName(path).constData(), &entry.info) != 0)
            return false;
        state.entries.push_back(entry);
        return true;
    }
    DIR* dir = ::opendir(dirPath.constData());
    if (!dir)
        return false;
    const QByteArray maskBytes = QFile::encodeName(mask);
    while (const dirent* item = ::readdir(dir)) {
        if (::fnmatch(maskBytes.constData(), item->d_name, FNM_CASEFOLD) != 0)
            continue;
        FindEntry entry;
        entry.name = QFile::decodeName(item->d_name);
        const QByteArray full = dirPath + '/' + item->d_name;
        if (::stat(full.constData(), &entry.info) != 0)
            continue;
        state.entries.push_back(entry);
    }
    ::closedir(dir);
    return !state.entries.empty();
}

template<typename FindData>
HANDLE findFirst(const QString& pattern, FindData* data)
{
    FindState state;
    if (!data || !findFiles(pattern, state)) {
        t_lastError = ERROR_FILE_NOT_FOUND;
        return INVALID_HANDLE_VALUE;
    }
    auto* find = new FindHandle;
    find->directory = state.directory;
    find->entries = std::move(state.entries);
    // Con la ruta de la entrada, para saber si es de solo lectura
    fillFindData(find->entries[0], QFile::encodeName(find->directory + find->entries[0].name), data);
    find->next = 1;
    return find;
}

template<typename FindData>
BOOL findNext(HANDLE handle, FindData* data)
{
    auto* find = static_cast<FindHandle*>(handle);
    if (!find || handle == INVALID_HANDLE_VALUE || find->magic != kFindMagic || !data) {
        t_lastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    if (find->next >= find->entries.size()) {
        t_lastError = ERROR_NO_MORE_FILES;
        return FALSE;
    }
    const FindEntry& entry = find->entries[find->next++];
    fillFindData(entry, QFile::encodeName(find->directory + entry.name), data);
    return TRUE;
}

HANDLE createFile(const QString& path, DWORD access, DWORD creation)
{
    const QByteArray encoded = encodedPath(path);
    int flags = 0;
    if ((access & GENERIC_READ) && (access & GENERIC_WRITE))
        flags = O_RDWR;
    else if (access & (GENERIC_WRITE | GENERIC_ALL))
        flags = (access & GENERIC_ALL) ? O_RDWR : O_WRONLY;
    else
        flags = O_RDONLY;
    struct stat info {};
    const bool existed = ::stat(encoded.constData(), &info) == 0;
    switch (creation) {
    case CREATE_NEW: flags |= O_CREAT | O_EXCL; break;
    case CREATE_ALWAYS: flags |= O_CREAT | O_TRUNC; break;
    case OPEN_ALWAYS: flags |= O_CREAT; break;
    case TRUNCATE_EXISTING: flags |= O_TRUNC; break;
    case OPEN_EXISTING:
    default: break;
    }
    if (existed && S_ISDIR(info.st_mode))
        flags = O_RDONLY; // FILE_FLAG_BACKUP_SEMANTICS: un directorio
    const int fd = ::open(encoded.constData(), flags | O_CLOEXEC, 0666);
    if (fd < 0) {
        t_lastError = errnoToWin32(errno);
        return INVALID_HANDLE_VALUE;
    }
    auto* file = new FileHandle;
    file->fd = fd;
    t_lastError = (existed && (creation == CREATE_ALWAYS || creation == OPEN_ALWAYS)) ? ERROR_ALREADY_EXISTS : ERROR_SUCCESS;
    return file;
}

DWORD fileAttributes(const QString& path)
{
    const QByteArray encoded = encodedPath(path);
    struct stat info {};
    if (::stat(encoded.constData(), &info) != 0) {
        t_lastError = errnoToWin32(errno);
        return INVALID_FILE_ATTRIBUTES;
    }
    return attributesOf(QFileInfo(nativePath(path)).fileName(), info, encoded);
}

BOOL moveFile(const QString& from, const QString& to, DWORD flags)
{
    const QByteArray source = encodedPath(from);
    const QByteArray target = encodedPath(to);
    struct stat info {};
    if (!(flags & MOVEFILE_REPLACE_EXISTING) && ::stat(target.constData(), &info) == 0) {
        t_lastError = ERROR_ALREADY_EXISTS;
        return FALSE;
    }
    if (::rename(source.constData(), target.constData()) == 0)
        return TRUE;
    if (errno != EXDEV)
        return failWithErrno();
    // Otro sistema de archivos: copiar y borrar
    QFile::remove(nativePath(to));
    if (!QFile::copy(nativePath(from), nativePath(to)) || !QFile::remove(nativePath(from))) {
        t_lastError = ERROR_ACCESS_DENIED;
        return FALSE;
    }
    return TRUE;
}

BOOL copyFile(const QString& from, const QString& to, BOOL failIfExists)
{
    const QString target = nativePath(to);
    if (QFileInfo::exists(target)) {
        if (failIfExists) {
            t_lastError = ERROR_FILE_EXISTS;
            return FALSE;
        }
        QFile::remove(target);
    }
    if (!QFile::exists(nativePath(from))) {
        t_lastError = ERROR_FILE_NOT_FOUND;
        return FALSE;
    }
    if (!QFile::copy(nativePath(from), target)) {
        t_lastError = ERROR_ACCESS_DENIED;
        return FALSE;
    }
    return TRUE;
}

BOOL createDirectory(const QString& path)
{
    if (::mkdir(encodedPath(path).constData(), 0777) == 0)
        return TRUE;
    if (errno == EEXIST) {
        t_lastError = ERROR_ALREADY_EXISTS;
        return FALSE;
    }
    return failWithErrno();
}

QString fullPathName(const QString& path)
{
    return QDir::cleanPath(QFileInfo(nativePath(path)).absoluteFilePath());
}

// --- Módulos ------------------------------------------------------------------------------

char g_moduleMarker = 0;   // el HINSTANCE del plugin es su dirección
char g_hostMarker = 0;     // y el de Edita (GetModuleHandle(NULL)), esta

QString modulePath(HMODULE module)
{
    if (module == nullptr || module == &g_hostMarker)
        return QCoreApplication::applicationFilePath();
    Dl_info info{};
    if (::dladdr(reinterpret_cast<void*>(&edita_compat_module), &info) && info.dli_fname)
        return QFileInfo(QFile::decodeName(info.dli_fname)).absoluteFilePath();
    return {};
}

// --- Archivos .ini --------------------------------------------------------------------------

struct IniEntry
{
    QString key;     // vacía: comentario o línea en blanco (se conserva tal cual)
    QString value;
    QString raw;
};

struct IniSection
{
    QString name;
    std::vector<IniEntry> entries;
};

struct IniFile
{
    std::vector<IniSection> sections;   // la primera, sin nombre: lo que hay antes de la primera [sección]
    bool utf16 = false;

    IniSection* section(const QString& name)
    {
        for (IniSection& s : sections) {
            if (!s.name.isNull() && s.name.compare(name, Qt::CaseInsensitive) == 0)
                return &s;
        }
        return nullptr;
    }
};

QString unquoted(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2 && ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
                              || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))))
        value = value.mid(1, value.size() - 2);
    return value;
}

IniFile readIni(const QString& path)
{
    IniFile ini;
    ini.sections.push_back(IniSection{});
    QFile file(nativePath(path));
    if (!file.open(QIODevice::ReadOnly))
        return ini;
    const QByteArray data = file.readAll();
    QString text;
    if (data.startsWith("\xFF\xFE")) {
        ini.utf16 = true;
        text = QStringDecoder(QStringConverter::Utf16LE).decode(data.mid(2));
    } else if (data.startsWith("\xEF\xBB\xBF")) {
        text = QString::fromUtf8(data.mid(3));
    } else {
        text = QString::fromUtf8(data);
    }
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (qsizetype i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (i == lines.size() - 1 && line.isEmpty())
            break;
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.contains(QLatin1Char(']'))) {
            ini.sections.push_back(IniSection{trimmed.mid(1, trimmed.indexOf(QLatin1Char(']')) - 1).trimmed(), {}});
            continue;
        }
        IniEntry entry;
        entry.raw = line;
        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (!trimmed.startsWith(QLatin1Char(';')) && equals > 0) {
            entry.key = line.left(equals).trimmed();
            entry.value = unquoted(line.mid(equals + 1));
        }
        ini.sections.back().entries.push_back(entry);
    }
    return ini;
}

bool writeIni(const QString& path, const IniFile& ini)
{
    QString text;
    for (const IniSection& section : ini.sections) {
        if (!section.name.isNull())
            text += QLatin1Char('[') + section.name + QStringLiteral("]\n");
        for (const IniEntry& entry : section.entries)
            text += (entry.key.isEmpty() ? entry.raw : entry.key + QLatin1Char('=') + entry.value) + QLatin1Char('\n');
    }
    const QString native = nativePath(path);
    QDir().mkpath(QFileInfo(native).absolutePath());
    QSaveFile file(native);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    if (ini.utf16) {
        file.write("\xFF\xFE");
        file.write(QStringEncoder(QStringConverter::Utf16LE).encode(text));
    } else {
        file.write(text.toUtf8());
    }
    return file.commit();
}

QStringList sectionNames(const QString& path)
{
    QStringList names;
    for (const IniSection& section : readIni(path).sections) {
        if (!section.name.isNull())
            names << section.name;
    }
    return names;
}

QStringList keysOf(const QString& path, const QString& sectionName)
{
    QStringList keys;
    IniFile ini = readIni(path);
    if (IniSection* section = ini.section(sectionName)) {
        for (const IniEntry& entry : section->entries) {
            if (!entry.key.isEmpty())
                keys << entry.key;
        }
    }
    return keys;
}

bool lookup(const QString& path, const QString& sectionName, const QString& key, QString* value)
{
    IniFile ini = readIni(path);
    if (IniSection* section = ini.section(sectionName)) {
        for (const IniEntry& entry : section->entries) {
            if (!entry.key.isEmpty() && entry.key.compare(key, Qt::CaseInsensitive) == 0) {
                *value = entry.value;
                return true;
            }
        }
    }
    return false;
}

// WritePrivateProfileString: key NULL borra la sección; value NULL borra la clave
bool writeProfile(const QString& path, const QString& sectionName, const QString* key, const QString* value)
{
    IniFile ini = readIni(path);
    IniSection* section = ini.section(sectionName);
    if (!key) {
        if (section) {
            ini.sections.erase(ini.sections.begin() + (section - ini.sections.data()));
            return writeIni(path, ini);
        }
        return true;
    }
    if (!section) {
        if (!value)
            return true;
        ini.sections.push_back(IniSection{sectionName, {}});
        section = &ini.sections.back();
    }
    for (auto it = section->entries.begin(); it != section->entries.end(); ++it) {
        if (!it->key.isEmpty() && it->key.compare(*key, Qt::CaseInsensitive) == 0) {
            if (value)
                it->value = *value;
            else
                section->entries.erase(it);
            return writeIni(path, ini);
        }
    }
    if (value) {
        // Después de la última clave (las líneas en blanco del final siguen al final)
        auto insertAt = section->entries.end();
        while (insertAt != section->entries.begin() && std::prev(insertAt)->key.isEmpty()
               && std::prev(insertAt)->raw.trimmed().isEmpty())
            --insertAt;
        section->entries.insert(insertAt, IniEntry{*key, *value, {}});
    }
    return writeIni(path, ini);
}

template<typename Ch>
DWORD getProfileString(const Ch* section, const Ch* key, const Ch* fallback, Ch* buffer, DWORD size, const Ch* file)
{
    if (!buffer || size == 0)
        return 0;
    const QString path = fromT(file);
    if (!section)
        return copyMultiString(sectionNames(path), buffer, size);
    if (!key)
        return copyMultiString(keysOf(path, fromT(section)), buffer, size);
    QString value;
    if (!lookup(path, fromT(section), fromT(key), &value)) {
        value = fromT(fallback);
        // Como Windows: sin los espacios del final del valor por defecto
        while (value.endsWith(QLatin1Char(' ')))
            value.chop(1);
    }
    return copyTruncated(toT<Ch>(value), buffer, size);
}

template<typename Ch>
UINT getProfileInt(const Ch* section, const Ch* key, INT fallback, const Ch* file)
{
    QString value;
    if (!section || !key || !lookup(fromT(file), fromT(section), fromT(key), &value))
        return static_cast<UINT>(fallback);
    // Como Windows: los dígitos del principio (y 0x…); 0 si no hay
    const QString trimmed = value.trimmed();
    bool ok = false;
    long long number = 0;
    if (trimmed.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
        number = trimmed.mid(2).toLongLong(&ok, 16);
    } else {
        qsizetype end = 0;
        if (end < trimmed.size() && (trimmed[end] == QLatin1Char('-') || trimmed[end] == QLatin1Char('+')))
            ++end;
        while (end < trimmed.size() && trimmed[end].isDigit())
            ++end;
        number = trimmed.left(end).toLongLong(&ok);
    }
    return ok ? static_cast<UINT>(number) : 0u;
}

template<typename Ch>
BOOL writeProfileString(const Ch* section, const Ch* key, const Ch* value, const Ch* file)
{
    if (!section || !file) {
        t_lastError = ERROR_INVALID_PARAMETER;
        return FALSE;
    }
    const QString keyText = fromT(key);
    const QString valueText = fromT(value);
    return writeProfile(fromT(file), fromT(section), key ? &keyText : nullptr, value ? &valueText : nullptr) ? TRUE : FALSE;
}

template<typename Ch>
DWORD getProfileSection(const Ch* sectionName, Ch* buffer, DWORD size, const Ch* file)
{
    QStringList pairs;
    IniFile ini = readIni(fromT(file));
    if (IniSection* section = ini.section(fromT(sectionName))) {
        for (const IniEntry& entry : section->entries) {
            if (!entry.key.isEmpty())
                pairs << entry.key + QLatin1Char('=') + entry.value;
        }
    }
    return copyMultiString(pairs, buffer, size);
}

// --- Memoria global -------------------------------------------------------------------------

constexpr uint32_t kGlobalMagic = 0x474D454D; // "GMEM"

struct alignas(std::max_align_t) GlobalHeader
{
    uint32_t magic;
    std::size_t size;
};

GlobalHeader* globalHeader(void* memory)
{
    if (!memory)
        return nullptr;
    auto* header = reinterpret_cast<GlobalHeader*>(static_cast<char*>(memory) - sizeof(GlobalHeader));
    return header->magic == kGlobalMagic ? header : nullptr;
}

void* globalAlloc(std::size_t bytes, bool zero)
{
    void* block = zero ? std::calloc(1, sizeof(GlobalHeader) + bytes) : std::malloc(sizeof(GlobalHeader) + bytes);
    if (!block) {
        t_lastError = ERROR_NOT_ENOUGH_MEMORY;
        return nullptr;
    }
    auto* header = static_cast<GlobalHeader*>(block);
    header->magic = kGlobalMagic;
    header->size = bytes;
    return static_cast<char*>(block) + sizeof(GlobalHeader);
}

void globalFree(void* memory)
{
    if (GlobalHeader* header = globalHeader(memory)) {
        header->magic = 0;
        std::free(header);
    }
}

// --- Portapapeles ---------------------------------------------------------------------------

struct ClipboardState
{
    bool open = false;
    QMimeData* pending = nullptr;              // EmptyClipboard + SetClipboardData
    std::vector<void*> handed;                 // lo que devolvió GetClipboardData
    std::map<UINT, QString> registered;        // RegisterClipboardFormat
};

ClipboardState& clipboardState()
{
    static ClipboardState state;
    return state;
}

QString clipboardMime(UINT format)
{
    const auto& registered = clipboardState().registered;
    const auto it = registered.find(format);
    if (it == registered.end())
        return {};
    // El mismo tipo con el que Qt publica en Windows un formato registrado
    return QStringLiteral("application/x-qt-windows-mime;value=\"%1\"").arg(it->second);
}

UINT registerClipboardFormat(const QString& name)
{
    auto& registered = clipboardState().registered;
    for (const auto& [id, existing] : registered) {
        if (existing.compare(name, Qt::CaseInsensitive) == 0)
            return id;
    }
    const UINT id = 0xC000u + static_cast<UINT>(registered.size());
    registered[id] = name;
    return id;
}

// --- Rutas (shlwapi) ------------------------------------------------------------------------

template<typename Ch> bool isSeparator(Ch c) { return c == Ch('/') || c == Ch('\\'); }

template<typename Ch>
Ch* findFileName(const Ch* path)
{
    if (!path)
        return nullptr;
    const Ch* name = path;
    for (const Ch* p = path; *p; ++p) {
        if (isSeparator(*p) && p[1])
            name = p + 1;
    }
    return const_cast<Ch*>(name);
}

template<typename Ch>
Ch* findExtension(const Ch* path)
{
    if (!path)
        return nullptr;
    const Ch* dot = nullptr;
    const Ch* p = path;
    for (; *p; ++p) {
        if (isSeparator(*p) || *p == Ch(' '))
            dot = nullptr;
        else if (*p == Ch('.'))
            dot = p;
    }
    return const_cast<Ch*>(dot ? dot : p);
}

template<typename Ch>
BOOL removeFileSpec(Ch* path)
{
    if (!path || !*path)
        return FALSE;
    Ch* last = nullptr;
    for (Ch* p = path; *p; ++p) {
        if (isSeparator(*p))
            last = p;
    }
    if (!last) {
        path[0] = Ch(0);
        return TRUE;
    }
    if (last == path) { // "/archivo" → "/"
        if (!path[1])
            return FALSE;
        path[1] = Ch(0);
        return TRUE;
    }
    *last = Ch(0);
    return TRUE;
}

template<typename Ch>
bool isRelative(const Ch* path)
{
    if (!path || !*path)
        return true;
    if (isSeparator(path[0]))
        return false;
    return !(path[1] == Ch(':') && ((path[0] >= Ch('a') && path[0] <= Ch('z')) || (path[0] >= Ch('A') && path[0] <= Ch('Z'))));
}

template<typename Ch>
BOOL appendPath(Ch* path, const Ch* more)
{
    if (!path)
        return FALSE;
    if (!more)
        return TRUE;
    while (isSeparator(*more))
        ++more;
    std::size_t n = length(path);
    if (*more && n > 0 && !isSeparator(path[n - 1])) {
        if (n + 1 >= MAX_PATH)
            return FALSE;
        path[n++] = Ch('/');
        path[n] = Ch(0);
    }
    const std::size_t extra = length(more);
    if (n + extra >= MAX_PATH)
        return FALSE;
    std::copy_n(more, extra + 1, path + n);
    return TRUE;
}

template<typename Ch>
Ch* combinePath(Ch* dst, const Ch* dir, const Ch* file)
{
    if (!dst)
        return nullptr;
    std::basic_string<Ch> result;
    if (file && !isRelative(file))
        result = file;
    else {
        if (dir)
            result = dir;
        if (file && *file) {
            if (!result.empty() && !isSeparator(result.back()))
                result += Ch('/');
            result += file;
        }
    }
    if (result.size() >= MAX_PATH) {
        dst[0] = Ch(0);
        return nullptr;
    }
    std::copy_n(result.c_str(), result.size() + 1, dst);
    return dst;
}

template<typename Ch>
Ch* addSeparator(Ch* path)
{
    if (!path)
        return nullptr;
    std::size_t n = length(path);
    if (n == 0 || !isSeparator(path[n - 1])) {
        if (n + 1 >= MAX_PATH)
            return nullptr;
        path[n++] = Ch('/');
        path[n] = Ch(0);
    }
    return path + n;
}

template<typename Ch>
Ch* removeSeparator(Ch* path)
{
    if (!path)
        return nullptr;
    std::size_t n = length(path);
    if (n > 1 && isSeparator(path[n - 1])) {
        path[--n] = Ch(0);
        return path + n;
    }
    return n > 0 ? path + n - 1 : path;
}

template<typename Ch>
void stripPath(Ch* path)
{
    if (!path)
        return;
    Ch* name = findFileName(path);
    if (name != path)
        std::memmove(path, name, (length(name) + 1) * sizeof(Ch));
}

template<typename Ch>
BOOL renameExtension(Ch* path, const Ch* extension)
{
    if (!path || !extension)
        return FALSE;
    Ch* dot = findExtension(path);
    const std::size_t base = static_cast<std::size_t>(dot - path);
    const std::size_t extra = length(extension);
    if (base + extra >= MAX_PATH)
        return FALSE;
    std::copy_n(extension, extra + 1, dot);
    return TRUE;
}

// --- Varios -------------------------------------------------------------------------------

template<typename Ch>
Ch* integerToString(unsigned long long magnitude, bool negative, Ch* buffer, int radix)
{
    if (!buffer)
        return nullptr;
    if (radix < 2 || radix > 36) {
        buffer[0] = Ch(0);
        return buffer;
    }
    Ch digits[72];
    int n = 0;
    do {
        const auto digit = static_cast<int>(magnitude % static_cast<unsigned>(radix));
        digits[n++] = static_cast<Ch>(digit < 10 ? '0' + digit : 'a' + digit - 10);
        magnitude /= static_cast<unsigned>(radix);
    } while (magnitude);
    Ch* out = buffer;
    if (negative)
        *out++ = Ch('-');
    while (n > 0)
        *out++ = digits[--n];
    *out = Ch(0);
    return buffer;
}

template<typename Ch, typename T>
Ch* toText(T value, Ch* buffer, int radix)
{
    // Como MSVC: con signo solo en base 10
    if (radix == 10 && value < 0)
        return integerToString(0ull - static_cast<unsigned long long>(value), true, buffer, radix);
    using U = std::make_unsigned_t<T>;
    return integerToString(static_cast<unsigned long long>(static_cast<U>(value)), false, buffer, radix);
}

// «Sin límite» en copySafe/catSafe (SIZE_MAX es _TRUNCATE, que significa otra cosa)
constexpr std::size_t kNoLimit = SIZE_MAX - 1;

template<typename Ch>
errno_t copySafe(Ch* dst, std::size_t size, const Ch* src, std::size_t count)
{
    if (!dst || size == 0)
        return EINVAL;
    if (!src) {
        dst[0] = Ch(0);
        return EINVAL;
    }
    std::size_t n = 0;
    while (n < count && src[n])
        ++n;
    if (n >= size) {
        if (count == _TRUNCATE) {
            std::copy_n(src, size - 1, dst);
            dst[size - 1] = Ch(0);
            return 80; // STRUNCATE
        }
        dst[0] = Ch(0);
        return ERANGE;
    }
    std::copy_n(src, n, dst);
    dst[n] = Ch(0);
    return 0;
}

template<typename Ch>
errno_t catSafe(Ch* dst, std::size_t size, const Ch* src, std::size_t count)
{
    if (!dst || size == 0)
        return EINVAL;
    std::size_t used = 0;
    while (used < size && dst[used])
        ++used;
    if (used == size)
        return EINVAL;
    return copySafe(dst + used, size - used, src, count);
}

template<typename Ch>
HRESULT cchCopy(Ch* dst, std::size_t size, const Ch* src, std::size_t count)
{
    if (!dst || size == 0 || size > STRSAFE_MAX_CCH)
        return STRSAFE_E_INVALID_PARAMETER;
    std::size_t n = 0;
    while (src && n < count && src[n])
        ++n;
    if (n >= size) {
        std::copy_n(src, size - 1, dst);
        dst[size - 1] = Ch(0);
        return STRSAFE_E_INSUFFICIENT_BUFFER;
    }
    if (n)
        std::copy_n(src, n, dst);
    dst[n] = Ch(0);
    return S_OK;
}

template<typename Ch>
HRESULT cchCat(Ch* dst, std::size_t size, const Ch* src, std::size_t count)
{
    if (!dst || size == 0 || size > STRSAFE_MAX_CCH)
        return STRSAFE_E_INVALID_PARAMETER;
    std::size_t used = 0;
    while (used < size && dst[used])
        ++used;
    if (used == size)
        return STRSAFE_E_INVALID_PARAMETER;
    return cchCopy(dst + used, size - used, src, count);
}

HRESULT cchPrintfResult(int written, std::size_t size)
{
    return written < 0 || static_cast<std::size_t>(written) >= size ? STRSAFE_E_INSUFFICIENT_BUFFER : S_OK;
}

QString expandEnvironment(const QString& source)
{
    QString out;
    qsizetype i = 0;
    while (i < source.size()) {
        const qsizetype start = source.indexOf(QLatin1Char('%'), i);
        if (start < 0) {
            out += source.mid(i);
            break;
        }
        const qsizetype end = source.indexOf(QLatin1Char('%'), start + 1);
        if (end < 0) {
            out += source.mid(i);
            break;
        }
        out += source.mid(i, start - i);
        const QString name = source.mid(start + 1, end - start - 1);
        if (!name.isEmpty() && qEnvironmentVariableIsSet(name.toLocal8Bit().constData())) {
            out += qEnvironmentVariable(name.toLocal8Bit().constData());
            i = end + 1;
        } else {
            out += source.mid(start, end - start);
            i = end;
        }
    }
    return out;
}

QString specialFolder(int csidl)
{
    switch (csidl & 0xFF) {
    case CSIDL_APPDATA: return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    case CSIDL_LOCAL_APPDATA: return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    case CSIDL_COMMON_APPDATA: return QStringLiteral("/etc/xdg");
    case CSIDL_PERSONAL: return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    case CSIDL_PROFILE: return QDir::homePath();
    case CSIDL_DESKTOP: return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    default: return {};
    }
}

template<typename Ch>
HRESULT folderPath(int csidl, Ch* path)
{
    const QString folder = specialFolder(csidl);
    if (!path || folder.isEmpty())
        return E_FAIL;
    if (csidl & CSIDL_FLAG_CREATE)
        QDir().mkpath(folder);
    copyTruncated(toT<Ch>(folder), path, MAX_PATH);
    return S_OK;
}

HINSTANCE shellExecute(const QString& verb, const QString& file, const QString& parameters, const QString& directory)
{
    const auto result = [](intptr_t code) { return reinterpret_cast<HINSTANCE>(code); };
    if (file.isEmpty())
        return result(2); // SE_ERR_FNF
    if (!verb.isEmpty() && verb.compare(QLatin1String("open"), Qt::CaseInsensitive) != 0
        && verb.compare(QLatin1String("explore"), Qt::CaseInsensitive) != 0
        && verb.compare(QLatin1String("edit"), Qt::CaseInsensitive) != 0)
        return result(31); // SE_ERR_NOASSOC: runas, print…
    const bool url = file.contains(QLatin1String("://")) || file.startsWith(QLatin1String("mailto:"), Qt::CaseInsensitive);
    const QString dir = nativePath(directory);
    if (url)
        return QDesktopServices::openUrl(QUrl(file)) ? result(42) : result(2);
    QString path = nativePath(file);
    if (QFileInfo(path).isRelative() && !dir.isEmpty() && QFileInfo::exists(QDir(dir).filePath(path)))
        path = QDir(dir).filePath(path);
    const QFileInfo info(path);
    if (!parameters.isEmpty() || (info.isFile() && info.isExecutable())) {
        const QString program = info.exists() ? info.absoluteFilePath() : file;
        return QProcess::startDetached(program, QProcess::splitCommand(parameters), dir) ? result(42) : result(2);
    }
    if (!info.exists())
        return result(2);
    return QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath())) ? result(42) : result(2);
}

int messageBox(const QString& text, const QString& caption, UINT type)
{
    QMessageBox box(QApplication::activeWindow());
    box.setWindowTitle(caption);
    box.setText(text);
    switch (type & 0xF0) {
    case MB_ICONHAND: box.setIcon(QMessageBox::Critical); break;
    case MB_ICONQUESTION: box.setIcon(QMessageBox::Question); break;
    case MB_ICONEXCLAMATION: box.setIcon(QMessageBox::Warning); break;
    case MB_ICONASTERISK: box.setIcon(QMessageBox::Information); break;
    default: break;
    }
    std::vector<std::pair<QMessageBox::StandardButton, int>> buttons;
    switch (type & 0xF) {
    case MB_OKCANCEL: buttons = {{QMessageBox::Ok, IDOK}, {QMessageBox::Cancel, IDCANCEL}}; break;
    case MB_ABORTRETRYIGNORE: buttons = {{QMessageBox::Abort, IDABORT}, {QMessageBox::Retry, IDRETRY}, {QMessageBox::Ignore, IDIGNORE}}; break;
    case MB_YESNOCANCEL: buttons = {{QMessageBox::Yes, IDYES}, {QMessageBox::No, IDNO}, {QMessageBox::Cancel, IDCANCEL}}; break;
    case MB_YESNO: buttons = {{QMessageBox::Yes, IDYES}, {QMessageBox::No, IDNO}}; break;
    case MB_RETRYCANCEL: buttons = {{QMessageBox::Retry, IDRETRY}, {QMessageBox::Cancel, IDCANCEL}}; break;
    default: buttons = {{QMessageBox::Ok, IDOK}}; break;
    }
    QMessageBox::StandardButtons standard;
    for (const auto& b : buttons)
        standard |= b.first;
    box.setStandardButtons(standard);
    const std::size_t defaultIndex = (type & MB_DEFBUTTON3) ? 2 : (type & MB_DEFBUTTON2) ? 1 : 0;
    if (defaultIndex < buttons.size())
        box.setDefaultButton(buttons[defaultIndex].first);
    const int clicked = box.exec();
    for (const auto& b : buttons) {
        if (clicked == b.first)
            return b.second;
    }
    return IDCANCEL;
}

void toSystemTime(const QDateTime& time, LPSYSTEMTIME out)
{
    if (!out)
        return;
    const QDate date = time.date();
    const QTime clock = time.time();
    out->wYear = static_cast<WORD>(date.year());
    out->wMonth = static_cast<WORD>(date.month());
    out->wDayOfWeek = static_cast<WORD>(date.dayOfWeek() % 7); // 0 = domingo
    out->wDay = static_cast<WORD>(date.day());
    out->wHour = static_cast<WORD>(clock.hour());
    out->wMinute = static_cast<WORD>(clock.minute());
    out->wSecond = static_cast<WORD>(clock.second());
    out->wMilliseconds = static_cast<WORD>(clock.msec());
}

} // namespace

// =============================================================================================
// API
// =============================================================================================

extern "C" {

HINSTANCE edita_compat_module(void)
{
    return &g_moduleMarker;
}

DWORD GetLastError(void)
{
    return t_lastError;
}

void SetLastError(DWORD error)
{
    t_lastError = error;
}

// --- Cuadros de mensaje -------------------------------------------------------------------

int MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type)
{
    return messageBox(fromA(text), fromA(caption), type);
}

int MessageBoxW(HWND, LPCWSTR text, LPCWSTR caption, UINT type)
{
    return messageBox(fromW(text), fromW(caption), type);
}

BOOL MessageBeep(UINT)
{
    QApplication::beep();
    return TRUE;
}

// --- Conversiones de texto ------------------------------------------------------------------

int MultiByteToWideChar(UINT codePage, DWORD flags, LPCSTR multiByte, int multiByteCount, LPWSTR wide, int wideCount)
{
    if (!multiByte || multiByteCount == 0 || wideCount < 0) {
        t_lastError = ERROR_INVALID_PARAMETER;
        return 0;
    }
    const qsizetype bytes = multiByteCount < 0 ? static_cast<qsizetype>(std::strlen(multiByte)) + 1 : multiByteCount;
    QString text;
    if (codePage == 1252 || codePage == 28591) {
        text = QString::fromLatin1(multiByte, bytes);
    } else if (codePage == CP_UTF8 || codePage == CP_ACP || codePage == CP_OEMCP || codePage == CP_THREAD_ACP
               || codePage == CP_MACCP) {
        QStringDecoder decoder(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
        text = decoder.decode(QByteArrayView(multiByte, bytes));
        if ((flags & MB_ERR_INVALID_CHARS) && decoder.hasError()) {
            t_lastError = ERROR_NO_UNICODE_TRANSLATION;
            return 0;
        }
    } else {
        t_lastError = ERROR_INVALID_PARAMETER;
        return 0;
    }
    const std::wstring converted = text.toStdWString();
    const auto needed = static_cast<int>(converted.size());
    if (wideCount == 0)
        return needed;
    if (!wide || needed > wideCount) {
        t_lastError = ERROR_INSUFFICIENT_BUFFER;
        return 0;
    }
    std::copy_n(converted.data(), converted.size(), wide);
    return needed;
}

int WideCharToMultiByte(UINT codePage, DWORD, LPCWSTR wide, int wideCount, LPSTR multiByte, int multiByteCount,
                        LPCSTR defaultChar, LPBOOL usedDefaultChar)
{
    if (!wide || wideCount == 0 || multiByteCount < 0) {
        t_lastError = ERROR_INVALID_PARAMETER;
        return 0;
    }
    const qsizetype chars = wideCount < 0 ? static_cast<qsizetype>(std::wcslen(wide)) + 1 : wideCount;
    const QString text = QString::fromWCharArray(wide, chars);
    QByteArray converted;
    if (usedDefaultChar)
        *usedDefaultChar = FALSE;
    if (codePage == 1252 || codePage == 28591) {
        const char replacement = defaultChar && *defaultChar ? *defaultChar : '?';
        converted.reserve(text.size());
        for (const QChar c : text) {
            if (c.unicode() > 0xFF) {
                converted += replacement;
                if (usedDefaultChar)
                    *usedDefaultChar = TRUE;
            } else {
                converted += static_cast<char>(c.unicode());
            }
        }
    } else if (codePage == CP_UTF8 || codePage == CP_ACP || codePage == CP_OEMCP || codePage == CP_THREAD_ACP
               || codePage == CP_MACCP) {
        converted = text.toUtf8();
    } else {
        t_lastError = ERROR_INVALID_PARAMETER;
        return 0;
    }
    const auto needed = static_cast<int>(converted.size());
    if (multiByteCount == 0)
        return needed;
    if (!multiByte || needed > multiByteCount) {
        t_lastError = ERROR_INSUFFICIENT_BUFFER;
        return 0;
    }
    std::memcpy(multiByte, converted.constData(), static_cast<std::size_t>(needed));
    return needed;
}

// --- Cadenas ----------------------------------------------------------------------------------

int lstrlenA(LPCSTR s) { return s ? static_cast<int>(std::strlen(s)) : 0; }
int lstrlenW(LPCWSTR s) { return s ? static_cast<int>(std::wcslen(s)) : 0; }
LPSTR lstrcpyA(LPSTR dst, LPCSTR src) { return dst && src ? std::strcpy(dst, src) : nullptr; }
LPWSTR lstrcpyW(LPWSTR dst, LPCWSTR src) { return dst && src ? std::wcscpy(dst, src) : nullptr; }
LPSTR lstrcatA(LPSTR dst, LPCSTR src) { return dst && src ? std::strcat(dst, src) : nullptr; }
LPWSTR lstrcatW(LPWSTR dst, LPCWSTR src) { return dst && src ? std::wcscat(dst, src) : nullptr; }
int lstrcmpA(LPCSTR a, LPCSTR b) { return std::strcmp(a ? a : "", b ? b : ""); }
int lstrcmpW(LPCWSTR a, LPCWSTR b) { return std::wcscmp(a ? a : L"", b ? b : L""); }
int lstrcmpiA(LPCSTR a, LPCSTR b) { return fromA(a).compare(fromA(b), Qt::CaseInsensitive); }
int lstrcmpiW(LPCWSTR a, LPCWSTR b) { return fromW(a).compare(fromW(b), Qt::CaseInsensitive); }

LPSTR lstrcpynA(LPSTR dst, LPCSTR src, int count)
{
    if (!dst || !src || count <= 0)
        return dst;
    copySafe(dst, static_cast<std::size_t>(count), src, _TRUNCATE);
    return dst;
}

LPWSTR lstrcpynW(LPWSTR dst, LPCWSTR src, int count)
{
    if (!dst || !src || count <= 0)
        return dst;
    copySafe(dst, static_cast<std::size_t>(count), src, _TRUNCATE);
    return dst;
}

// CharUpper/CharLower: si la palabra alta del puntero es 0, es un carácter
LPSTR CharUpperA(LPSTR s)
{
    if ((reinterpret_cast<uintptr_t>(s) >> 16) == 0)
        return reinterpret_cast<LPSTR>(static_cast<uintptr_t>(std::toupper(static_cast<int>(reinterpret_cast<uintptr_t>(s)))));
    const QByteArray upper = fromA(s).toUpper().toUtf8();
    if (static_cast<std::size_t>(upper.size()) == std::strlen(s))
        std::memcpy(s, upper.constData(), static_cast<std::size_t>(upper.size()));
    return s;
}

LPSTR CharLowerA(LPSTR s)
{
    if ((reinterpret_cast<uintptr_t>(s) >> 16) == 0)
        return reinterpret_cast<LPSTR>(static_cast<uintptr_t>(std::tolower(static_cast<int>(reinterpret_cast<uintptr_t>(s)))));
    const QByteArray lower = fromA(s).toLower().toUtf8();
    if (static_cast<std::size_t>(lower.size()) == std::strlen(s))
        std::memcpy(s, lower.constData(), static_cast<std::size_t>(lower.size()));
    return s;
}

LPWSTR CharUpperW(LPWSTR s)
{
    if ((reinterpret_cast<uintptr_t>(s) >> 16) == 0)
        return reinterpret_cast<LPWSTR>(static_cast<uintptr_t>(std::towupper(static_cast<wint_t>(reinterpret_cast<uintptr_t>(s)))));
    for (wchar_t* p = s; *p; ++p)
        *p = static_cast<wchar_t>(std::towupper(static_cast<wint_t>(*p)));
    return s;
}

LPWSTR CharLowerW(LPWSTR s)
{
    if ((reinterpret_cast<uintptr_t>(s) >> 16) == 0)
        return reinterpret_cast<LPWSTR>(static_cast<uintptr_t>(std::towlower(static_cast<wint_t>(reinterpret_cast<uintptr_t>(s)))));
    for (wchar_t* p = s; *p; ++p)
        *p = static_cast<wchar_t>(std::towlower(static_cast<wint_t>(*p)));
    return s;
}

DWORD CharUpperBuffW(LPWSTR s, DWORD count)
{
    for (DWORD i = 0; s && i < count; ++i)
        s[i] = static_cast<wchar_t>(std::towupper(static_cast<wint_t>(s[i])));
    return count;
}

DWORD CharLowerBuffW(LPWSTR s, DWORD count)
{
    for (DWORD i = 0; s && i < count; ++i)
        s[i] = static_cast<wchar_t>(std::towlower(static_cast<wint_t>(s[i])));
    return count;
}

BOOL IsCharAlphaW(WCHAR c) { return std::iswalpha(static_cast<wint_t>(c)) ? TRUE : FALSE; }
BOOL IsCharAlphaNumericW(WCHAR c) { return std::iswalnum(static_cast<wint_t>(c)) ? TRUE : FALSE; }
BOOL IsCharUpperW(WCHAR c) { return std::iswupper(static_cast<wint_t>(c)) ? TRUE : FALSE; }
BOOL IsCharLowerW(WCHAR c) { return std::iswlower(static_cast<wint_t>(c)) ? TRUE : FALSE; }

// --- printf ------------------------------------------------------------------------------------

int edita_compat_vswprintf(wchar_t* buffer, size_t count, const wchar_t* format, va_list args)
{
    return formatWide(buffer, count, format, args);
}

int edita_compat_swprintf(wchar_t* buffer, size_t count, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = formatWide(buffer, count, format, args);
    va_end(args);
    return written;
}

int edita_compat_vsnprintf(char* buffer, size_t count, const char* format, va_list args)
{
    return formatNarrow(buffer, count, format, args);
}

int edita_compat_snprintf(char* buffer, size_t count, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = formatNarrow(buffer, count, format, args);
    va_end(args);
    return written;
}

// wsprintf: como en Windows, como mucho 1024 caracteres
int wvsprintfA(LPSTR buffer, LPCSTR format, va_list args)
{
    const int written = formatNarrow(buffer, 1025, format, args);
    return written > 1024 ? 1024 : written;
}

int wvsprintfW(LPWSTR buffer, LPCWSTR format, va_list args)
{
    const int written = formatWide(buffer, 1025, format, args);
    return written < 0 ? static_cast<int>(std::wcslen(buffer)) : written;
}

int wsprintfA(LPSTR buffer, LPCSTR format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = wvsprintfA(buffer, format, args);
    va_end(args);
    return written;
}

int wsprintfW(LPWSTR buffer, LPCWSTR format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = wvsprintfW(buffer, format, args);
    va_end(args);
    return written;
}

int _swprintf(wchar_t* buffer, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = formatWide(buffer, static_cast<std::size_t>(INT_MAX) / sizeof(wchar_t), format, args);
    va_end(args);
    return written;
}

int _vsnwprintf(wchar_t* buffer, size_t count, const wchar_t* format, va_list args)
{
    return formatWide(buffer, count, format, args);
}

int _snwprintf(wchar_t* buffer, size_t count, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = formatWide(buffer, count, format, args);
    va_end(args);
    return written;
}

int _snwprintf_s(wchar_t* buffer, size_t size, size_t count, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    const std::size_t limit = count == _TRUNCATE ? size : std::min(size, count + 1);
    const int written = formatWide(buffer, limit, format, args);
    va_end(args);
    return written;
}

int vswprintf_s(wchar_t* buffer, size_t size, const wchar_t* format, va_list args)
{
    return formatWide(buffer, size, format, args);
}

int swprintf_s(wchar_t* buffer, size_t size, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = formatWide(buffer, size, format, args);
    va_end(args);
    return written;
}

int _vsnprintf(char* buffer, size_t count, const char* format, va_list args)
{
    const int written = formatNarrow(buffer, count, format, args);
    return written >= 0 && static_cast<std::size_t>(written) >= count ? -1 : written;
}

int _snprintf(char* buffer, size_t count, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = _vsnprintf(buffer, count, format, args);
    va_end(args);
    return written;
}

int _snprintf_s(char* buffer, size_t size, size_t count, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const std::size_t limit = count == _TRUNCATE ? size : std::min(size, count + 1);
    const int written = _vsnprintf(buffer, limit, format, args);
    va_end(args);
    return written;
}

int vsprintf_s(char* buffer, size_t size, const char* format, va_list args)
{
    const int written = formatNarrow(buffer, size, format, args);
    return written >= 0 && static_cast<std::size_t>(written) >= size ? -1 : written;
}

int sprintf_s(char* buffer, size_t size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = vsprintf_s(buffer, size, format, args);
    va_end(args);
    return written;
}

int _scwprintf(const wchar_t* format, ...)
{
    // vswprintf no dice cuánto hace falta: se prueba con búferes más grandes
    std::vector<wchar_t> buffer(256);
    for (;;) {
        va_list args;
        va_start(args, format);
        const int written = formatWide(buffer.data(), buffer.size(), format, args);
        va_end(args);
        if (written >= 0 || buffer.size() > (1u << 24))
            return written;
        buffer.resize(buffer.size() * 4);
    }
}

int _scprintf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = formatNarrow(nullptr, 0, format, args);
    va_end(args);
    return written;
}

// --- CRT -----------------------------------------------------------------------------------------

errno_t strcpy_s(char* dst, size_t size, const char* src) { return copySafe(dst, size, src, kNoLimit); }
errno_t strncpy_s(char* dst, size_t size, const char* src, size_t count) { return copySafe(dst, size, src, count); }
errno_t strcat_s(char* dst, size_t size, const char* src) { return catSafe(dst, size, src, kNoLimit); }
errno_t strncat_s(char* dst, size_t size, const char* src, size_t count) { return catSafe(dst, size, src, count); }
errno_t wcscpy_s(wchar_t* dst, size_t size, const wchar_t* src) { return copySafe(dst, size, src, kNoLimit); }
errno_t wcsncpy_s(wchar_t* dst, size_t size, const wchar_t* src, size_t count) { return copySafe(dst, size, src, count); }
errno_t wcscat_s(wchar_t* dst, size_t size, const wchar_t* src) { return catSafe(dst, size, src, kNoLimit); }
errno_t wcsncat_s(wchar_t* dst, size_t size, const wchar_t* src, size_t count) { return catSafe(dst, size, src, count); }

errno_t memcpy_s(void* dst, size_t size, const void* src, size_t count)
{
    if (count == 0)
        return 0;
    if (!dst || !src || count > size)
        return dst ? ERANGE : EINVAL;
    std::memcpy(dst, src, count);
    return 0;
}

errno_t memmove_s(void* dst, size_t size, const void* src, size_t count)
{
    if (count == 0)
        return 0;
    if (!dst || !src || count > size)
        return dst ? ERANGE : EINVAL;
    std::memmove(dst, src, count);
    return 0;
}

char* _strlwr(char* s) { return CharLowerA(s); }
char* _strupr(char* s) { return CharUpperA(s); }
wchar_t* _wcslwr(wchar_t* s) { return s ? CharLowerW(s) : s; }
wchar_t* _wcsupr(wchar_t* s) { return s ? CharUpperW(s) : s; }
errno_t _strlwr_s(char* s, size_t) { return s ? (CharLowerA(s), 0) : EINVAL; }
errno_t _strupr_s(char* s, size_t) { return s ? (CharUpperA(s), 0) : EINVAL; }
errno_t _wcslwr_s(wchar_t* s, size_t) { return s ? (CharLowerW(s), 0) : EINVAL; }
errno_t _wcsupr_s(wchar_t* s, size_t) { return s ? (CharUpperW(s), 0) : EINVAL; }

wchar_t* _wcsrev(wchar_t* s)
{
    if (s)
        std::reverse(s, s + std::wcslen(s));
    return s;
}

char* _strrev(char* s)
{
    if (s)
        std::reverse(s, s + std::strlen(s));
    return s;
}

char* _itoa(int value, char* buffer, int radix) { return toText(value, buffer, radix); }
wchar_t* _itow(int value, wchar_t* buffer, int radix) { return toText(value, buffer, radix); }
char* _ltoa(long value, char* buffer, int radix) { return toText(value, buffer, radix); }
wchar_t* _ltow(long value, wchar_t* buffer, int radix) { return toText(value, buffer, radix); }
char* _ultoa(unsigned long value, char* buffer, int radix) { return toText(value, buffer, radix); }
wchar_t* _ultow(unsigned long value, wchar_t* buffer, int radix) { return toText(value, buffer, radix); }
char* _i64toa(long long value, char* buffer, int radix) { return toText(value, buffer, radix); }
wchar_t* _i64tow(long long value, wchar_t* buffer, int radix) { return toText(value, buffer, radix); }

errno_t _itoa_s(int value, char* buffer, size_t size, int radix)
{
    char text[72];
    toText(value, text, radix);
    return copySafe(buffer, size, text, kNoLimit);
}

errno_t _itow_s(int value, wchar_t* buffer, size_t size, int radix)
{
    wchar_t text[72];
    toText(value, text, radix);
    return copySafe(buffer, size, text, kNoLimit);
}

int _wtoi(const wchar_t* s) { return s ? static_cast<int>(std::wcstol(s, nullptr, 10)) : 0; }
long _wtol(const wchar_t* s) { return s ? std::wcstol(s, nullptr, 10) : 0; }
long long _wtoi64(const wchar_t* s) { return s ? std::wcstoll(s, nullptr, 10) : 0; }
double _wtof(const wchar_t* s) { return s ? std::wcstod(s, nullptr) : 0.0; }

FILE* _wfopen(const wchar_t* path, const wchar_t* mode)
{
    if (!path || !mode)
        return nullptr;
    // "r, ccs=UTF-8" y "rt" de MSVC: sin la codificación y sin la t
    std::string narrowMode;
    for (const wchar_t* m = mode; *m && *m != L','; ++m) {
        if (*m != L't' && *m != L' ')
            narrowMode += static_cast<char>(*m);
    }
    return std::fopen(encodedPath(fromW(path)).constData(), narrowMode.c_str());
}

errno_t _wfopen_s(FILE** file, const wchar_t* path, const wchar_t* mode)
{
    if (!file)
        return EINVAL;
    *file = _wfopen(path, mode);
    return *file ? 0 : errno;
}

errno_t fopen_s(FILE** file, const char* path, const char* mode)
{
    if (!file || !path || !mode)
        return EINVAL;
    std::string narrowMode;
    for (const char* m = mode; *m && *m != ','; ++m) {
        if (*m != 't' && *m != ' ')
            narrowMode += *m;
    }
    *file = std::fopen(encodedPath(fromA(path)).constData(), narrowMode.c_str());
    return *file ? 0 : errno;
}

int _wremove(const wchar_t* path) { return std::remove(encodedPath(fromW(path)).constData()); }
int _wrename(const wchar_t* from, const wchar_t* to)
{
    return std::rename(encodedPath(fromW(from)).constData(), encodedPath(fromW(to)).constData());
}
int _waccess(const wchar_t* path, int mode) { return ::access(encodedPath(fromW(path)).constData(), mode); }
int _wmkdir(const wchar_t* path) { return ::mkdir(encodedPath(fromW(path)).constData(), 0777); }
int _wrmdir(const wchar_t* path) { return ::rmdir(encodedPath(fromW(path)).constData()); }

wchar_t* _wgetenv(const wchar_t* name)
{
    // Una copia ancha por variable que vive mientras viva el plugin
    static thread_local std::map<std::wstring, std::wstring> values;
    if (!name)
        return nullptr;
    const QByteArray key = fromW(name).toLocal8Bit();
    if (!qEnvironmentVariableIsSet(key.constData()))
        return nullptr;
    std::wstring& value = values[name];
    value = qEnvironmentVariable(key.constData()).toStdWString();
    return value.data();
}

// --- Archivos ----------------------------------------------------------------------------------

HANDLE CreateFileA(LPCSTR path, DWORD access, DWORD, LPSECURITY_ATTRIBUTES, DWORD creation, DWORD, HANDLE)
{
    return createFile(fromA(path), access, creation);
}

HANDLE CreateFileW(LPCWSTR path, DWORD access, DWORD, LPSECURITY_ATTRIBUTES, DWORD creation, DWORD, HANDLE)
{
    return createFile(fromW(path), access, creation);
}

BOOL ReadFile(HANDLE handle, LPVOID buffer, DWORD toRead, LPDWORD read, LPOVERLAPPED)
{
    FileHandle* file = fileHandle(handle);
    if (read)
        *read = 0;
    if (!file || (!buffer && toRead)) {
        t_lastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    std::size_t done = 0;
    while (done < toRead) {
        const ssize_t n = ::read(file->fd, static_cast<char*>(buffer) + done, toRead - done);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return failWithErrno();
        }
        if (n == 0)
            break;
        done += static_cast<std::size_t>(n);
    }
    if (read)
        *read = static_cast<DWORD>(done);
    return TRUE;
}

BOOL WriteFile(HANDLE handle, LPCVOID buffer, DWORD toWrite, LPDWORD written, LPOVERLAPPED)
{
    FileHandle* file = fileHandle(handle);
    if (written)
        *written = 0;
    if (!file || (!buffer && toWrite)) {
        t_lastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    std::size_t done = 0;
    while (done < toWrite) {
        const ssize_t n = ::write(file->fd, static_cast<const char*>(buffer) + done, toWrite - done);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return failWithErrno();
        }
        done += static_cast<std::size_t>(n);
    }
    if (written)
        *written = static_cast<DWORD>(done);
    return TRUE;
}

BOOL GetFileSizeEx(HANDLE handle, PLARGE_INTEGER size)
{
    FileHandle* file = fileHandle(handle);
    struct stat info {};
    if (!file || !size || ::fstat(file->fd, &info) != 0) {
        t_lastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    size->QuadPart = static_cast<LONGLONG>(info.st_size);
    return TRUE;
}

DWORD GetFileSize(HANDLE handle, LPDWORD high)
{
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size))
        return INVALID_FILE_SIZE;
    const auto value = static_cast<uint64_t>(size.QuadPart);
    if (high)
        *high = static_cast<DWORD>(value >> 32);
    return static_cast<DWORD>(value & 0xFFFFFFFFu);
}

BOOL SetFilePointerEx(HANDLE handle, LARGE_INTEGER distance, PLARGE_INTEGER newPointer, DWORD method)
{
    FileHandle* file = fileHandle(handle);
    if (!file) {
        t_lastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    const int whence = method == FILE_END ? SEEK_END : method == FILE_CURRENT ? SEEK_CUR : SEEK_SET;
    const off_t position = ::lseek(file->fd, static_cast<off_t>(distance.QuadPart), whence);
    if (position < 0)
        return failWithErrno();
    if (newPointer)
        newPointer->QuadPart = static_cast<LONGLONG>(position);
    return TRUE;
}

DWORD SetFilePointer(HANDLE handle, LONG distance, PLONG high, DWORD method)
{
    LARGE_INTEGER move{};
    if (high)
        move.QuadPart = static_cast<LONGLONG>((static_cast<uint64_t>(static_cast<uint32_t>(*high)) << 32)
                                              | static_cast<uint32_t>(distance));
    else
        move.QuadPart = distance;
    LARGE_INTEGER position{};
    if (!SetFilePointerEx(handle, move, &position, method))
        return INVALID_SET_FILE_POINTER;
    const auto value = static_cast<uint64_t>(position.QuadPart);
    if (high)
        *high = static_cast<LONG>(value >> 32);
    return static_cast<DWORD>(value & 0xFFFFFFFFu);
}

BOOL SetEndOfFile(HANDLE handle)
{
    FileHandle* file = fileHandle(handle);
    if (!file) {
        t_lastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    const off_t position = ::lseek(file->fd, 0, SEEK_CUR);
    return position >= 0 && ::ftruncate(file->fd, position) == 0 ? TRUE : failWithErrno();
}

BOOL FlushFileBuffers(HANDLE handle)
{
    FileHandle* file = fileHandle(handle);
    return file && ::fsync(file->fd) == 0 ? TRUE : FALSE;
}

BOOL CloseHandle(HANDLE handle)
{
    if (FileHandle* file = fileHandle(handle)) {
        ::close(file->fd);
        file->magic = 0;
        delete file;
        return TRUE;
    }
    auto* find = static_cast<FindHandle*>(handle);
    if (find && handle != INVALID_HANDLE_VALUE && find->magic == kFindMagic)
        return FindClose(handle);
    t_lastError = ERROR_INVALID_HANDLE;
    return FALSE;
}

DWORD GetFileAttributesA(LPCSTR path) { return fileAttributes(fromA(path)); }
DWORD GetFileAttributesW(LPCWSTR path) { return fileAttributes(fromW(path)); }

BOOL SetFileAttributesA(LPCSTR path, DWORD) { return QFileInfo::exists(nativePath(fromA(path))) ? TRUE : FALSE; }
BOOL SetFileAttributesW(LPCWSTR path, DWORD) { return QFileInfo::exists(nativePath(fromW(path))) ? TRUE : FALSE; }

BOOL DeleteFileA(LPCSTR path) { return ::unlink(encodedPath(fromA(path)).constData()) == 0 ? TRUE : failWithErrno(); }
BOOL DeleteFileW(LPCWSTR path) { return ::unlink(encodedPath(fromW(path)).constData()) == 0 ? TRUE : failWithErrno(); }

BOOL CreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES) { return createDirectory(fromA(path)); }
BOOL CreateDirectoryW(LPCWSTR path, LPSECURITY_ATTRIBUTES) { return createDirectory(fromW(path)); }

BOOL RemoveDirectoryA(LPCSTR path) { return ::rmdir(encodedPath(fromA(path)).constData()) == 0 ? TRUE : failWithErrno(); }
BOOL RemoveDirectoryW(LPCWSTR path) { return ::rmdir(encodedPath(fromW(path)).constData()) == 0 ? TRUE : failWithErrno(); }

BOOL MoveFileA(LPCSTR from, LPCSTR to) { return moveFile(fromA(from), fromA(to), 0); }
BOOL MoveFileW(LPCWSTR from, LPCWSTR to) { return moveFile(fromW(from), fromW(to), 0); }
BOOL MoveFileExA(LPCSTR from, LPCSTR to, DWORD flags) { return moveFile(fromA(from), fromA(to), flags); }
BOOL MoveFileExW(LPCWSTR from, LPCWSTR to, DWORD flags) { return moveFile(fromW(from), fromW(to), flags); }
BOOL CopyFileA(LPCSTR from, LPCSTR to, BOOL failIfExists) { return copyFile(fromA(from), fromA(to), failIfExists); }
BOOL CopyFileW(LPCWSTR from, LPCWSTR to, BOOL failIfExists) { return copyFile(fromW(from), fromW(to), failIfExists); }

HANDLE FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA data) { return findFirst(fromA(pattern), data); }
HANDLE FindFirstFileW(LPCWSTR pattern, LPWIN32_FIND_DATAW data) { return findFirst(fromW(pattern), data); }
BOOL FindNextFileA(HANDLE find, LPWIN32_FIND_DATAA data) { return findNext(find, data); }
BOOL FindNextFileW(HANDLE find, LPWIN32_FIND_DATAW data) { return findNext(find, data); }

BOOL FindClose(HANDLE handle)
{
    auto* find = static_cast<FindHandle*>(handle);
    if (!find || handle == INVALID_HANDLE_VALUE || find->magic != kFindMagic) {
        t_lastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    find->magic = 0;
    delete find;
    return TRUE;
}

DWORD GetCurrentDirectoryA(DWORD size, LPSTR buffer) { return copyOrRequired(toT<char>(QDir::currentPath()), buffer, size); }
DWORD GetCurrentDirectoryW(DWORD size, LPWSTR buffer) { return copyOrRequired(toT<wchar_t>(QDir::currentPath()), buffer, size); }
BOOL SetCurrentDirectoryA(LPCSTR path) { return QDir::setCurrent(nativePath(fromA(path))) ? TRUE : FALSE; }
BOOL SetCurrentDirectoryW(LPCWSTR path) { return QDir::setCurrent(nativePath(fromW(path))) ? TRUE : FALSE; }

// Como en Windows, con el separador al final
DWORD GetTempPathA(DWORD size, LPSTR buffer) { return copyOrRequired(toT<char>(QDir::tempPath() + QLatin1Char('/')), buffer, size); }
DWORD GetTempPathW(DWORD size, LPWSTR buffer) { return copyOrRequired(toT<wchar_t>(QDir::tempPath() + QLatin1Char('/')), buffer, size); }

DWORD GetFullPathNameA(LPCSTR path, DWORD size, LPSTR buffer, LPSTR* filePart)
{
    const DWORD result = copyOrRequired(toT<char>(fullPathName(fromA(path))), buffer, size);
    if (filePart)
        *filePart = result < size && buffer ? findFileName(buffer) : nullptr;
    return result;
}

DWORD GetFullPathNameW(LPCWSTR path, DWORD size, LPWSTR buffer, LPWSTR* filePart)
{
    const DWORD result = copyOrRequired(toT<wchar_t>(fullPathName(fromW(path))), buffer, size);
    if (filePart)
        *filePart = result < size && buffer ? findFileName(buffer) : nullptr;
    return result;
}

// --- Módulos -----------------------------------------------------------------------------------

// Como Windows: si no cabe, size caracteres (con el nulo) y ERROR_INSUFFICIENT_BUFFER
DWORD GetModuleFileNameA(HMODULE module, LPSTR buffer, DWORD size)
{
    const std::string path = toT<char>(modulePath(module));
    const DWORD copied = copyTruncated(path, buffer, size);
    if (size && copied < path.size()) {
        t_lastError = ERROR_INSUFFICIENT_BUFFER;
        return size;
    }
    return copied;
}

DWORD GetModuleFileNameW(HMODULE module, LPWSTR buffer, DWORD size)
{
    const std::wstring path = toT<wchar_t>(modulePath(module));
    const DWORD copied = copyTruncated(path, buffer, size);
    if (size && copied < path.size()) {
        t_lastError = ERROR_INSUFFICIENT_BUFFER;
        return size;
    }
    return copied;
}

HMODULE GetModuleHandleA(LPCSTR name) { return name ? edita_compat_module() : &g_hostMarker; }
HMODULE GetModuleHandleW(LPCWSTR name) { return name ? edita_compat_module() : &g_hostMarker; }

// --- Archivos .ini -------------------------------------------------------------------------------

DWORD GetPrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR fallback, LPSTR buffer, DWORD size, LPCSTR file)
{
    return getProfileString(section, key, fallback, buffer, size, file);
}

DWORD GetPrivateProfileStringW(LPCWSTR section, LPCWSTR key, LPCWSTR fallback, LPWSTR buffer, DWORD size, LPCWSTR file)
{
    return getProfileString(section, key, fallback, buffer, size, file);
}

UINT GetPrivateProfileIntA(LPCSTR section, LPCSTR key, INT fallback, LPCSTR file)
{
    return getProfileInt(section, key, fallback, file);
}

UINT GetPrivateProfileIntW(LPCWSTR section, LPCWSTR key, INT fallback, LPCWSTR file)
{
    return getProfileInt(section, key, fallback, file);
}

BOOL WritePrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR value, LPCSTR file)
{
    return writeProfileString(section, key, value, file);
}

BOOL WritePrivateProfileStringW(LPCWSTR section, LPCWSTR key, LPCWSTR value, LPCWSTR file)
{
    return writeProfileString(section, key, value, file);
}

DWORD GetPrivateProfileSectionA(LPCSTR section, LPSTR buffer, DWORD size, LPCSTR file)
{
    return getProfileSection(section, buffer, size, file);
}

DWORD GetPrivateProfileSectionW(LPCWSTR section, LPWSTR buffer, DWORD size, LPCWSTR file)
{
    return getProfileSection(section, buffer, size, file);
}

DWORD GetPrivateProfileSectionNamesA(LPSTR buffer, DWORD size, LPCSTR file)
{
    return copyMultiString(sectionNames(fromA(file)), buffer, size);
}

DWORD GetPrivateProfileSectionNamesW(LPWSTR buffer, DWORD size, LPCWSTR file)
{
    return copyMultiString(sectionNames(fromW(file)), buffer, size);
}

// --- Entorno, tiempo y proceso -------------------------------------------------------------------

DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buffer, DWORD size)
{
    if (!name || !qEnvironmentVariableIsSet(name)) {
        t_lastError = 203; // ERROR_ENVVAR_NOT_FOUND
        return 0;
    }
    return copyOrRequired(toT<char>(qEnvironmentVariable(name)), buffer, size);
}

DWORD GetEnvironmentVariableW(LPCWSTR name, LPWSTR buffer, DWORD size)
{
    const QByteArray key = fromW(name).toLocal8Bit();
    if (!name || !qEnvironmentVariableIsSet(key.constData())) {
        t_lastError = 203; // ERROR_ENVVAR_NOT_FOUND
        return 0;
    }
    return copyOrRequired(toT<wchar_t>(qEnvironmentVariable(key.constData())), buffer, size);
}

BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value)
{
    if (!name)
        return FALSE;
    return (value ? qputenv(name, QByteArray(value)) : qunsetenv(name)) ? TRUE : FALSE;
}

BOOL SetEnvironmentVariableW(LPCWSTR name, LPCWSTR value)
{
    if (!name)
        return FALSE;
    const QByteArray key = fromW(name).toLocal8Bit();
    return (value ? qputenv(key.constData(), fromW(value).toUtf8()) : qunsetenv(key.constData())) ? TRUE : FALSE;
}

DWORD ExpandEnvironmentStringsA(LPCSTR source, LPSTR buffer, DWORD size)
{
    return copyCounted(toT<char>(expandEnvironment(fromA(source))), buffer, size);
}

DWORD ExpandEnvironmentStringsW(LPCWSTR source, LPWSTR buffer, DWORD size)
{
    return copyCounted(toT<wchar_t>(expandEnvironment(fromW(source))), buffer, size);
}

DWORD GetTickCount(void)
{
    return static_cast<DWORD>(GetTickCount64() & 0xFFFFFFFFu);
}

ULONGLONG GetTickCount64(void)
{
    using namespace std::chrono;
    return static_cast<ULONGLONG>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void Sleep(DWORD milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void GetLocalTime(LPSYSTEMTIME time) { toSystemTime(QDateTime::currentDateTime(), time); }
void GetSystemTime(LPSYSTEMTIME time) { toSystemTime(QDateTime::currentDateTimeUtc(), time); }

void GetSystemTimeAsFileTime(LPFILETIME time)
{
    if (!time)
        return;
    struct timespec now {};
    ::clock_gettime(CLOCK_REALTIME, &now);
    *time = fileTime(now);
}

BOOL QueryPerformanceCounter(LARGE_INTEGER* counter)
{
    if (!counter)
        return FALSE;
    using namespace std::chrono;
    counter->QuadPart = static_cast<LONGLONG>(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
    return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER* frequency)
{
    if (!frequency)
        return FALSE;
    frequency->QuadPart = 1000000000LL;
    return TRUE;
}

DWORD GetCurrentProcessId(void) { return static_cast<DWORD>(::getpid()); }
DWORD GetCurrentThreadId(void) { return static_cast<DWORD>(std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFFFFFFu); }
void OutputDebugStringA(LPCSTR text) { std::fputs(text ? text : "", stderr); }
void OutputDebugStringW(LPCWSTR text) { std::fputs(fromW(text).toUtf8().constData(), stderr); }

// --- Memoria -------------------------------------------------------------------------------------

HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes) { return globalAlloc(bytes, (flags & GMEM_ZEROINIT) != 0); }
LPVOID GlobalLock(HGLOBAL memory) { return globalHeader(memory) ? memory : nullptr; }
BOOL GlobalUnlock(HGLOBAL) { return FALSE; } // FALSE: ya no queda ningún bloqueo
SIZE_T GlobalSize(HGLOBAL memory) { const GlobalHeader* header = globalHeader(memory); return header ? header->size : 0; }
HGLOBAL GlobalFree(HGLOBAL memory) { globalFree(memory); return nullptr; }
HLOCAL LocalAlloc(UINT flags, SIZE_T bytes) { return globalAlloc(bytes, (flags & LMEM_ZEROINIT) != 0); }
HLOCAL LocalFree(HLOCAL memory) { globalFree(memory); return nullptr; }
HANDLE GetProcessHeap(void) { return &g_hostMarker; }
LPVOID HeapAlloc(HANDLE, DWORD flags, SIZE_T bytes) { return globalAlloc(bytes, (flags & HEAP_ZERO_MEMORY) != 0); }
BOOL HeapFree(HANDLE, DWORD, LPVOID memory) { globalFree(memory); return TRUE; }

// --- Portapapeles --------------------------------------------------------------------------------

BOOL OpenClipboard(HWND)
{
    ClipboardState& state = clipboardState();
    if (state.open)
        return FALSE;
    state.open = true;
    return TRUE;
}

BOOL CloseClipboard(void)
{
    ClipboardState& state = clipboardState();
    if (!state.open)
        return FALSE;
    state.open = false;
    if (state.pending) {
        QApplication::clipboard()->setMimeData(state.pending);
        state.pending = nullptr;
    }
    for (void* memory : state.handed)
        globalFree(memory);
    state.handed.clear();
    return TRUE;
}

BOOL EmptyClipboard(void)
{
    ClipboardState& state = clipboardState();
    if (!state.open)
        return FALSE;
    delete state.pending;
    state.pending = new QMimeData;
    return TRUE;
}

HANDLE SetClipboardData(UINT format, HANDLE data)
{
    ClipboardState& state = clipboardState();
    const GlobalHeader* header = globalHeader(data);
    if (!state.open || !header)
        return nullptr;
    if (!state.pending)
        state.pending = new QMimeData;
    const char* bytes = static_cast<const char*>(data);
    const std::size_t size = header->size;
    switch (format) {
    case CF_UNICODETEXT: {
        const auto* wide = static_cast<const wchar_t*>(data);
        const std::size_t chars = ::wcsnlen(wide, size / sizeof(wchar_t));
        state.pending->setText(QString::fromWCharArray(wide, static_cast<qsizetype>(chars)));
        break;
    }
    case CF_TEXT:
    case CF_OEMTEXT:
        state.pending->setText(QString::fromUtf8(bytes, static_cast<qsizetype>(::strnlen(bytes, size))));
        break;
    default: {
        const QString mime = clipboardMime(format);
        if (mime.isEmpty())
            return nullptr;
        state.pending->setData(mime, QByteArray(bytes, static_cast<qsizetype>(size)));
        break;
    }
    }
    // El portapapeles es dueño de la memoria: ya se copió
    globalFree(data);
    return data;
}

HANDLE GetClipboardData(UINT format)
{
    ClipboardState& state = clipboardState();
    if (!state.open)
        return nullptr;
    const QMimeData* mime = state.pending ? state.pending : QApplication::clipboard()->mimeData();
    if (!mime)
        return nullptr;
    void* memory = nullptr;
    if (format == CF_UNICODETEXT || format == CF_TEXT || format == CF_OEMTEXT) {
        if (!mime->hasText())
            return nullptr;
        const QString text = mime->text();
        if (format == CF_UNICODETEXT) {
            const std::wstring wide = text.toStdWString();
            memory = globalAlloc((wide.size() + 1) * sizeof(wchar_t), true);
            if (memory)
                std::copy_n(wide.c_str(), wide.size() + 1, static_cast<wchar_t*>(memory));
        } else {
            const QByteArray utf8 = text.toUtf8();
            memory = globalAlloc(static_cast<std::size_t>(utf8.size()) + 1, true);
            if (memory)
                std::memcpy(memory, utf8.constData(), static_cast<std::size_t>(utf8.size()) + 1);
        }
    } else {
        const QString type = clipboardMime(format);
        if (type.isEmpty() || !mime->hasFormat(type))
            return nullptr;
        const QByteArray bytes = mime->data(type);
        memory = globalAlloc(static_cast<std::size_t>(bytes.size()), true);
        if (memory && !bytes.isEmpty())
            std::memcpy(memory, bytes.constData(), static_cast<std::size_t>(bytes.size()));
    }
    if (memory)
        state.handed.push_back(memory);
    return memory;
}

BOOL IsClipboardFormatAvailable(UINT format)
{
    const ClipboardState& state = clipboardState();
    const QMimeData* mime = state.pending ? state.pending : QApplication::clipboard()->mimeData();
    if (!mime)
        return FALSE;
    if (format == CF_UNICODETEXT || format == CF_TEXT || format == CF_OEMTEXT)
        return mime->hasText() ? TRUE : FALSE;
    const QString type = clipboardMime(format);
    return !type.isEmpty() && mime->hasFormat(type) ? TRUE : FALSE;
}

UINT RegisterClipboardFormatA(LPCSTR name) { return name && *name ? registerClipboardFormat(fromA(name)) : 0; }
UINT RegisterClipboardFormatW(LPCWSTR name) { return name && *name ? registerClipboardFormat(fromW(name)) : 0; }

// --- Teclado ---------------------------------------------------------------------------------------

SHORT GetKeyState(int virtualKey)
{
    const Qt::KeyboardModifiers modifiers = QGuiApplication::queryKeyboardModifiers();
    bool down = false;
    switch (virtualKey) {
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: down = modifiers.testFlag(Qt::ShiftModifier); break;
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: down = modifiers.testFlag(Qt::ControlModifier); break;
    case VK_MENU: case VK_LMENU: case VK_RMENU: down = modifiers.testFlag(Qt::AltModifier); break;
    case VK_LWIN: down = modifiers.testFlag(Qt::MetaModifier); break;
    default: break;
    }
    return down ? static_cast<SHORT>(-32768) : static_cast<SHORT>(0);
}

SHORT GetAsyncKeyState(int virtualKey) { return GetKeyState(virtualKey); }

// --- shlwapi ---------------------------------------------------------------------------------------

BOOL PathFileExistsA(LPCSTR path) { return path && QFileInfo::exists(nativePath(fromA(path))) ? TRUE : FALSE; }
BOOL PathFileExistsW(LPCWSTR path) { return path && QFileInfo::exists(nativePath(fromW(path))) ? TRUE : FALSE; }
BOOL PathIsDirectoryA(LPCSTR path) { return path && QFileInfo(nativePath(fromA(path))).isDir() ? TRUE : FALSE; }
BOOL PathIsDirectoryW(LPCWSTR path) { return path && QFileInfo(nativePath(fromW(path))).isDir() ? TRUE : FALSE; }
BOOL PathIsRelativeA(LPCSTR path) { return isRelative(path) ? TRUE : FALSE; }
BOOL PathIsRelativeW(LPCWSTR path) { return isRelative(path) ? TRUE : FALSE; }
LPSTR PathFindFileNameA(LPCSTR path) { return findFileName(path); }
LPWSTR PathFindFileNameW(LPCWSTR path) { return findFileName(path); }
LPSTR PathFindExtensionA(LPCSTR path) { return findExtension(path); }
LPWSTR PathFindExtensionW(LPCWSTR path) { return findExtension(path); }
BOOL PathRemoveFileSpecA(LPSTR path) { return removeFileSpec(path); }
BOOL PathRemoveFileSpecW(LPWSTR path) { return removeFileSpec(path); }
void PathRemoveExtensionA(LPSTR path) { if (path) *findExtension(path) = '\0'; }
void PathRemoveExtensionW(LPWSTR path) { if (path) *findExtension(path) = L'\0'; }
BOOL PathRenameExtensionA(LPSTR path, LPCSTR extension) { return renameExtension(path, extension); }
BOOL PathRenameExtensionW(LPWSTR path, LPCWSTR extension) { return renameExtension(path, extension); }
void PathStripPathA(LPSTR path) { stripPath(path); }
void PathStripPathW(LPWSTR path) { stripPath(path); }
BOOL PathAppendA(LPSTR path, LPCSTR more) { return appendPath(path, more); }
BOOL PathAppendW(LPWSTR path, LPCWSTR more) { return appendPath(path, more); }
LPSTR PathCombineA(LPSTR dst, LPCSTR dir, LPCSTR file) { return combinePath(dst, dir, file); }
LPWSTR PathCombineW(LPWSTR dst, LPCWSTR dir, LPCWSTR file) { return combinePath(dst, dir, file); }
LPSTR PathAddBackslashA(LPSTR path) { return addSeparator(path); }
LPWSTR PathAddBackslashW(LPWSTR path) { return addSeparator(path); }
LPSTR PathRemoveBackslashA(LPSTR path) { return removeSeparator(path); }
LPWSTR PathRemoveBackslashW(LPWSTR path) { return removeSeparator(path); }

int StrCmpIA(LPCSTR a, LPCSTR b) { return lstrcmpiA(a, b); }
int StrCmpIW(LPCWSTR a, LPCWSTR b) { return lstrcmpiW(a, b); }
int StrCmpNIA(LPCSTR a, LPCSTR b, int count)
{
    return count <= 0 ? 0 : fromA(a).left(count).compare(fromA(b).left(count), Qt::CaseInsensitive);
}
int StrCmpNIW(LPCWSTR a, LPCWSTR b, int count) { return count <= 0 ? 0 : ::wcsncasecmp(a ? a : L"", b ? b : L"", static_cast<std::size_t>(count)); }

LPSTR StrStrIA(LPCSTR haystack, LPCSTR needle)
{
    if (!haystack || !needle)
        return nullptr;
    const std::size_t n = std::strlen(needle);
    for (const char* p = haystack; *p; ++p) {
        if (::strncasecmp(p, needle, n) == 0)
            return const_cast<LPSTR>(p);
    }
    return n == 0 ? const_cast<LPSTR>(haystack) : nullptr;
}

LPWSTR StrStrIW(LPCWSTR haystack, LPCWSTR needle)
{
    if (!haystack || !needle)
        return nullptr;
    const std::size_t n = std::wcslen(needle);
    for (const wchar_t* p = haystack; *p; ++p) {
        if (::wcsncasecmp(p, needle, n) == 0)
            return const_cast<LPWSTR>(p);
    }
    return n == 0 ? const_cast<LPWSTR>(haystack) : nullptr;
}

LPSTR StrChrA(LPCSTR s, WORD c) { return s ? const_cast<LPSTR>(std::strchr(s, static_cast<char>(c))) : nullptr; }
LPWSTR StrChrW(LPCWSTR s, WCHAR c) { return s ? const_cast<LPWSTR>(std::wcschr(s, c)) : nullptr; }

LPSTR StrRChrA(LPCSTR s, LPCSTR end, WORD c)
{
    if (!s)
        return nullptr;
    const char* found = nullptr;
    for (const char* p = s; (end ? p < end : *p != '\0'); ++p) {
        if (*p == static_cast<char>(c))
            found = p;
    }
    return const_cast<LPSTR>(found);
}

LPWSTR StrRChrW(LPCWSTR s, LPCWSTR end, WCHAR c)
{
    if (!s)
        return nullptr;
    const wchar_t* found = nullptr;
    for (const wchar_t* p = s; (end ? p < end : *p != L'\0'); ++p) {
        if (*p == c)
            found = p;
    }
    return const_cast<LPWSTR>(found);
}

int StrToIntA(LPCSTR s) { return s ? static_cast<int>(std::strtol(s, nullptr, 10)) : 0; }
int StrToIntW(LPCWSTR s) { return _wtoi(s); }

// --- shell32 ---------------------------------------------------------------------------------------

HINSTANCE ShellExecuteA(HWND, LPCSTR verb, LPCSTR file, LPCSTR parameters, LPCSTR directory, INT)
{
    return shellExecute(fromA(verb), fromA(file), fromA(parameters), fromA(directory));
}

HINSTANCE ShellExecuteW(HWND, LPCWSTR verb, LPCWSTR file, LPCWSTR parameters, LPCWSTR directory, INT)
{
    return shellExecute(fromW(verb), fromW(file), fromW(parameters), fromW(directory));
}

HRESULT SHGetFolderPathA(HWND, int csidl, HANDLE, DWORD, LPSTR path) { return folderPath(csidl, path); }
HRESULT SHGetFolderPathW(HWND, int csidl, HANDLE, DWORD, LPWSTR path) { return folderPath(csidl, path); }
BOOL SHGetSpecialFolderPathA(HWND, LPSTR path, int csidl, BOOL create)
{
    return SUCCEEDED(folderPath(create ? (csidl | CSIDL_FLAG_CREATE) : csidl, path)) ? TRUE : FALSE;
}
BOOL SHGetSpecialFolderPathW(HWND, LPWSTR path, int csidl, BOOL create)
{
    return SUCCEEDED(folderPath(create ? (csidl | CSIDL_FLAG_CREATE) : csidl, path)) ? TRUE : FALSE;
}

// --- strsafe.h -------------------------------------------------------------------------------------

HRESULT StringCchCopyA(LPSTR dst, size_t size, LPCSTR src) { return cchCopy(dst, size, src, SIZE_MAX); }
HRESULT StringCchCopyW(LPWSTR dst, size_t size, LPCWSTR src) { return cchCopy(dst, size, src, SIZE_MAX); }
HRESULT StringCchCopyNA(LPSTR dst, size_t size, LPCSTR src, size_t count) { return cchCopy(dst, size, src, count); }
HRESULT StringCchCopyNW(LPWSTR dst, size_t size, LPCWSTR src, size_t count) { return cchCopy(dst, size, src, count); }
HRESULT StringCchCatA(LPSTR dst, size_t size, LPCSTR src) { return cchCat(dst, size, src, SIZE_MAX); }
HRESULT StringCchCatW(LPWSTR dst, size_t size, LPCWSTR src) { return cchCat(dst, size, src, SIZE_MAX); }
HRESULT StringCchCatNA(LPSTR dst, size_t size, LPCSTR src, size_t count) { return cchCat(dst, size, src, count); }
HRESULT StringCchCatNW(LPWSTR dst, size_t size, LPCWSTR src, size_t count) { return cchCat(dst, size, src, count); }
HRESULT StringCbCopyA(LPSTR dst, size_t bytes, LPCSTR src) { return cchCopy(dst, bytes, src, SIZE_MAX); }
HRESULT StringCbCopyW(LPWSTR dst, size_t bytes, LPCWSTR src) { return cchCopy(dst, bytes / sizeof(wchar_t), src, SIZE_MAX); }
HRESULT StringCbCatA(LPSTR dst, size_t bytes, LPCSTR src) { return cchCat(dst, bytes, src, SIZE_MAX); }
HRESULT StringCbCatW(LPWSTR dst, size_t bytes, LPCWSTR src) { return cchCat(dst, bytes / sizeof(wchar_t), src, SIZE_MAX); }

HRESULT StringCchLengthA(LPCSTR s, size_t max, size_t* length)
{
    if (!s || max > STRSAFE_MAX_CCH)
        return STRSAFE_E_INVALID_PARAMETER;
    const std::size_t n = ::strnlen(s, max);
    if (length)
        *length = n;
    return n < max ? S_OK : STRSAFE_E_INVALID_PARAMETER;
}

HRESULT StringCchLengthW(LPCWSTR s, size_t max, size_t* length)
{
    if (!s || max > STRSAFE_MAX_CCH)
        return STRSAFE_E_INVALID_PARAMETER;
    const std::size_t n = ::wcsnlen(s, max);
    if (length)
        *length = n;
    return n < max ? S_OK : STRSAFE_E_INVALID_PARAMETER;
}

HRESULT StringCchVPrintfA(LPSTR dst, size_t size, LPCSTR format, va_list args)
{
    if (!dst || size == 0)
        return STRSAFE_E_INVALID_PARAMETER;
    return cchPrintfResult(formatNarrow(dst, size, format, args), size);
}

HRESULT StringCchVPrintfW(LPWSTR dst, size_t size, LPCWSTR format, va_list args)
{
    if (!dst || size == 0)
        return STRSAFE_E_INVALID_PARAMETER;
    return cchPrintfResult(formatWide(dst, size, format, args), size);
}

HRESULT StringCchPrintfA(LPSTR dst, size_t size, LPCSTR format, ...)
{
    va_list args;
    va_start(args, format);
    const HRESULT result = StringCchVPrintfA(dst, size, format, args);
    va_end(args);
    return result;
}

HRESULT StringCchPrintfW(LPWSTR dst, size_t size, LPCWSTR format, ...)
{
    va_list args;
    va_start(args, format);
    const HRESULT result = StringCchVPrintfW(dst, size, format, args);
    va_end(args);
    return result;
}

HRESULT StringCbPrintfA(LPSTR dst, size_t bytes, LPCSTR format, ...)
{
    va_list args;
    va_start(args, format);
    const HRESULT result = StringCchVPrintfA(dst, bytes, format, args);
    va_end(args);
    return result;
}

HRESULT StringCbPrintfW(LPWSTR dst, size_t bytes, LPCWSTR format, ...)
{
    va_list args;
    va_start(args, format);
    const HRESULT result = StringCchVPrintfW(dst, bytes / sizeof(wchar_t), format, args);
    va_end(args);
    return result;
}

} // extern "C"
