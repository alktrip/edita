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


#include "core/FileAssociation.h"

#include "AppInfo.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>

namespace edita::file_association {

namespace {

const QString kDefaultSection = QStringLiteral("Default Applications");
const QString kAddedSection = QStringLiteral("Added Associations");
const QString kOwnPrefix = QStringLiteral("application/x-edita-");
const QString kGeneratedKey = QStringLiteral("X-Edita-Generated=true");

QString homeDir(const char* variable, const QString& fallback)
{
    const QString value = qEnvironmentVariable(variable);
    // La especificación pide rutas absolutas: una relativa no cuenta
    return QDir::isAbsolutePath(value) ? value : QDir::homePath() + fallback;
}

bool writeText(const QString& path, const QString& text, QString* error)
{
    if (!QDir().mkpath(QFileInfo(path).path())) {
        if (error)
            *error = QCoreApplication::translate("FileAssociation", "Could not create the folder of %1").arg(path);
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 0 || !file.commit()) {
        if (error)
            *error = QCoreApplication::translate("FileAssociation", "Could not write %1").arg(path);
        return false;
    }
    return true;
}

QString readText(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

// mimeapps.list con lo justo para cambiar unas claves y dejar todo lo demás (comentarios,
// otras secciones, el orden) tal cual. QSettings lo reescribiría a su manera
class MimeAppsList
{
public:
    explicit MimeAppsList(const QString& path)
        : _path(path)
    {
        const QString text = readText(path);
        if (!text.isEmpty()) {
            _lines = text.split(QLatin1Char('\n'));
            if (_lines.last().isEmpty())
                _lines.removeLast();
        }
    }

    QString value(const QString& section, const QString& key) const
    {
        const int at = find(section, key);
        return at < 0 ? QString() : _lines[at].mid(_lines[at].indexOf(QLatin1Char('=')) + 1).trimmed();
    }

    void setValue(const QString& section, const QString& key, const QString& value)
    {
        const QString line = key + QLatin1Char('=') + value;
        if (const int at = find(section, key); at >= 0) {
            _lines[at] = line;
            return;
        }
        int start = sectionStart(section);
        if (start < 0) {
            if (!_lines.isEmpty() && !_lines.last().trimmed().isEmpty())
                _lines << QString();
            _lines << QLatin1Char('[') + section + QLatin1Char(']');
            start = int(_lines.size()) - 1;
        }
        // Tras la última línea con algo de la sección
        int insert = start + 1;
        for (int i = start + 1; i < _lines.size() && !_lines[i].startsWith(QLatin1Char('[')); ++i) {
            if (!_lines[i].trimmed().isEmpty())
                insert = i + 1;
        }
        _lines.insert(insert, line);
    }

    void remove(const QString& section, const QString& key)
    {
        if (const int at = find(section, key); at >= 0)
            _lines.removeAt(at);
    }

    bool save(QString* error) const
    {
        return writeText(_path, _lines.join(QLatin1Char('\n')) + QLatin1Char('\n'), error);
    }

private:
    int sectionStart(const QString& section) const
    {
        const QString header = QLatin1Char('[') + section + QLatin1Char(']');
        for (int i = 0; i < _lines.size(); ++i) {
            if (_lines[i].trimmed() == header)
                return i;
        }
        return -1;
    }

    int find(const QString& section, const QString& key) const
    {
        const int start = sectionStart(section);
        if (start < 0)
            return -1;
        for (int i = start + 1; i < _lines.size() && !_lines[i].startsWith(QLatin1Char('[')); ++i) {
            const qsizetype eq = _lines[i].indexOf(QLatin1Char('='));
            if (eq > 0 && _lines[i].left(eq).trimmed() == key)
                return i;
        }
        return -1;
    }

    QString _path;
    QStringList _lines;
};

// "a.desktop;b.desktop;" ↔ lista
QStringList desktopList(const QString& value)
{
    QStringList out;
    for (const QString& id : value.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        if (!id.trimmed().isEmpty())
            out << id.trimmed();
    }
    return out;
}

QString joinDesktopList(const QStringList& ids)
{
    return ids.isEmpty() ? QString() : ids.join(QLatin1Char(';')) + QLatin1Char(';');
}

// El programa de una línea Exec= escrita por desktopExec (lo contrario)
QString programFromExec(const QString& exec)
{
    QString value = exec;
    if (value.endsWith(QLatin1String(" %F")))
        value.chop(3);
    value.replace(QLatin1String("%%"), QLatin1String("%"));
    if (!value.startsWith(QLatin1Char('"')))
        return value;
    value.replace(QLatin1String("\\\\"), QLatin1String("\\")); // escape de la cadena del .desktop
    QString out;
    for (qsizetype i = 1; i < value.size(); ++i) {
        const QChar c = value[i];
        if (c == QLatin1Char('\\') && i + 1 < value.size()) {
            out += value[++i];
        } else if (c == QLatin1Char('"')) {
            break;
        } else {
            out += c;
        }
    }
    return out;
}

} // namespace

const std::vector<ExtensionGroup>& extensionGroups()
{
    // defExtArray de regExtDlg.cpp, igual
    static const std::vector<ExtensionGroup> groups = [] {
        auto group = [](const char* name, const char* extensions) {
            return ExtensionGroup{QString::fromLatin1(name),
                                  QString::fromLatin1(extensions).split(QLatin1Char(' '), Qt::SkipEmptyParts)};
        };
        return std::vector<ExtensionGroup>{
            group("Notepad", ".txt .log"),
            group("ms ini/inf", ".ini .inf"),
            group("c, c++, objc", ".h .hh .hpp .hxx .c .cpp .cxx .cc .m .mm .vcxproj .vcproj .props .vsprops .manifest"),
            group("java, c#, pascal", ".java .cs .pas .pp .inc"),
            group("web script", ".html .htm .shtml .shtm .hta .asp .aspx .css .js .json .mjs .jsm .jsp .php .php3 "
                                ".php4 .php5 .phps .phpt .phtml .xml .xhtml .xht .xul .kml .xaml .xsml"),
            group("public script", ".sh .bsh .bash .bat .cmd .nsi .nsh .lua .pl .pm .py"),
            group("property script", ".rc .as .mx .vb .vbs"),
            group("fortran, TeX, SQL", ".f .for .f90 .f95 .f2k .tex .sql"),
            group("misc", ".nfo .mak"),
            group("customize", ""),
        };
    }();
    return groups;
}

Environment Environment::current()
{
    Environment env;
    env.dataHome = homeDir("XDG_DATA_HOME", QStringLiteral("/.local/share"));
    env.configHome = homeDir("XDG_CONFIG_HOME", QStringLiteral("/.config"));
    for (const QString& dir : qEnvironmentVariable("XDG_DATA_DIRS").split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        if (QDir::isAbsolutePath(dir))
            env.dataDirs << dir;
    }
    if (env.dataDirs.isEmpty())
        env.dataDirs = {QStringLiteral("/usr/local/share"), QStringLiteral("/usr/share")};
    env.appId = QString::fromLatin1(kAppId);
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    env.launcher = appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage;
    env.iconFile = QStringLiteral(":/edita/icons/hicolor/256x256/apps/edita.png");
    env.run = [](const QString& program, const QStringList& arguments) {
        const QString path = QStandardPaths::findExecutable(program);
        if (path.isEmpty())
            return false;
        QProcess process;
        process.start(path, arguments);
        return process.waitForFinished(30000) && process.exitStatus() == QProcess::NormalExit
               && process.exitCode() == 0;
    };
    return env;
}

QString normalizeExtension(const QString& extension)
{
    QString ext = extension.trimmed().toLower();
    if (!ext.startsWith(QLatin1Char('.')))
        ext.prepend(QLatin1Char('.'));
    bool hasName = false;
    for (const QChar c : ext) {
        if (c.isLetterOrNumber()) {
            hasName = true;
        } else if (!QStringLiteral("._-+~#").contains(c)) {
            return {};
        }
    }
    return hasName ? ext : QString();
}

QString desktopExec(const QString& program)
{
    // Especificación de Desktop Entry, «The Exec key»: entre comillas si hace falta, con una
    // barra invertida antes de " ` $ y de la barra misma; y después el escape de las
    // cadenas, que dobla cada barra invertida
    static const QString reserved = QStringLiteral(" \t\n\"'\\><~|&;$*?#()`");
    QString value = program;
    value.replace(QLatin1Char('%'), QLatin1String("%%"));
    if (std::none_of(value.begin(), value.end(), [](QChar c) { return reserved.contains(c); }))
        return value;
    QString quoted;
    for (const QChar c : value) {
        if (QStringLiteral("\"`$\\").contains(c))
            quoted += QLatin1Char('\\');
        quoted += c;
    }
    quoted.replace(QLatin1String("\\"), QLatin1String("\\\\"));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}

FileAssociations::FileAssociations(Environment env)
    : _env(std::move(env))
{
    if (!_env.run)
        _env.run = [](const QString&, const QStringList&) { return false; };
    load();
}

QString FileAssociations::recordFile() const
{
    return _env.configHome + QLatin1Char('/') + _env.appId + QStringLiteral("/fileAssociations.xml");
}

QString FileAssociations::mimeAppsFile() const
{
    return _env.configHome + QStringLiteral("/mimeapps.list");
}

QString FileAssociations::mimePackageFile() const
{
    return _env.dataHome + QStringLiteral("/mime/packages/") + _env.appId + QStringLiteral("-associations.xml");
}

QString FileAssociations::desktopFile() const
{
    return _env.dataHome + QStringLiteral("/applications/") + _env.desktopId();
}

bool FileAssociations::installedDesktopFile() const
{
    return std::any_of(_env.dataDirs.begin(), _env.dataDirs.end(), [this](const QString& dir) {
        return QFileInfo::exists(dir + QStringLiteral("/applications/") + _env.desktopId());
    });
}

bool FileAssociations::isRegistered(const QString& extension) const
{
    const QString ext = normalizeExtension(extension);
    return std::any_of(_registered.begin(), _registered.end(),
                       [&ext](const Association& a) { return a.extension == ext; });
}

QString FileAssociations::mimeTypeFor(const QString& extension, bool* own) const
{
    const QString ext = normalizeExtension(extension);
    if (own)
        *own = false;
    if (ext.isEmpty())
        return {};
    static const QMimeDatabase db;
    for (const QMimeType& type : db.mimeTypesForFileName(QStringLiteral("x") + ext)) {
        if (type.name() == QLatin1String("text/plain") || type.inherits(QStringLiteral("text/plain")))
            return type.name();
    }
    if (own)
        *own = true;
    QString name = ext.mid(1);
    for (QChar& c : name) {
        if (!(c.isLetterOrNumber() && c.unicode() < 128) && !QStringLiteral(".+-_").contains(c))
            c = QLatin1Char('_');
    }
    return kOwnPrefix + name;
}

void FileAssociations::load()
{
    _registered.clear();
    QFile file(recordFile());
    if (!file.open(QIODevice::ReadOnly))
        return;
    QXmlStreamReader xml(&file);
    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("FileAssociations")) {
            xml.skipCurrentElement();
            continue;
        }
        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1String("Association")) {
                const auto attrs = xml.attributes();
                Association a;
                a.extension = normalizeExtension(attrs.value(QLatin1String("extension")).toString());
                a.mimeType = attrs.value(QLatin1String("mimeType")).toString();
                a.previous = attrs.value(QLatin1String("previous")).toString();
                a.ownMimeType = attrs.value(QLatin1String("own")) == QLatin1String("yes");
                if (!a.extension.isEmpty() && !a.mimeType.isEmpty() && !isRegistered(a.extension))
                    _registered << a;
            }
            xml.skipCurrentElement();
        }
    }
}

bool FileAssociations::save(QString* error) const
{
    QString text;
    QXmlStreamWriter xml(&text);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(4);
    xml.writeStartDocument();
    xml.writeComment(QStringLiteral(" Preferences > File Association: what Edita associated, and the previous default "
                                    "application of each type (to give it back). Edita rewrites this file. "));
    xml.writeStartElement(QStringLiteral("FileAssociations"));
    for (const Association& a : _registered) {
        xml.writeEmptyElement(QStringLiteral("Association"));
        xml.writeAttribute(QStringLiteral("extension"), a.extension);
        xml.writeAttribute(QStringLiteral("mimeType"), a.mimeType);
        if (!a.previous.isEmpty())
            xml.writeAttribute(QStringLiteral("previous"), a.previous);
        if (a.ownMimeType)
            xml.writeAttribute(QStringLiteral("own"), QStringLiteral("yes"));
    }
    xml.writeEndDocument();
    return writeText(recordFile(), text, error);
}

bool FileAssociations::writeMimePackage(const QList<Association>& associations, QString* error) const
{
    QString types;
    for (const Association& a : associations) {
        if (!a.ownMimeType)
            continue;
        // Peso 60, más que el 50 de los tipos del sistema: la extensión es de Edita aunque
        // otro tipo la tenga (como en N++, que se queda con la extensión del registro)
        types += QStringLiteral("    <mime-type type=\"%1\">\n"
                                "        <comment>%2 file (Edita)</comment>\n"
                                "        <sub-class-of type=\"text/plain\"/>\n"
                                "        <glob pattern=\"*%2\" weight=\"60\"/>\n"
                                "    </mime-type>\n")
                     .arg(a.mimeType.toHtmlEscaped(), a.extension.toHtmlEscaped());
    }
    const QString package = mimePackageFile();
    if (types.isEmpty()) {
        if (!QFileInfo::exists(package))
            return true;
        QFile::remove(package);
    } else if (!writeText(package,
                          QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                         "<!-- Preferences > File Association of Edita. Edita rewrites this file. -->\n"
                                         "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n")
                              + types + QStringLiteral("</mime-info>\n"),
                          error)) {
        return false;
    }
    if (!_env.run(QStringLiteral("update-mime-database"), {_env.dataHome + QStringLiteral("/mime")})) {
        if (error)
            *error = QCoreApplication::translate("FileAssociation",
                                                 "Could not run update-mime-database (package shared-mime-info), "
                                                 "needed for an extension without a text file type");
        return false;
    }
    return true;
}

bool FileAssociations::writeDesktopFile(QString* error) const
{
    // El de resources/edita.desktop.in, con la ruta de este Edita y los tipos asociados
    QStringList mimeTypes = {QStringLiteral("text/plain")};
    for (const Association& a : _registered) {
        if (!mimeTypes.contains(a.mimeType))
            mimeTypes << a.mimeType;
    }
    const QString text = QStringLiteral("[Desktop Entry]\n"
                                        "Type=Application\n"
                                        "Name=%1\n"
                                        "GenericName=Text Editor\n"
                                        "GenericName[es]=Editor de texto\n"
                                        "Comment=Text editor inspired by Notepad++\n"
                                        "Comment[es]=Editor de texto inspirado en Notepad++\n"
                                        "Exec=%2 %F\n"
                                        "Icon=%3\n"
                                        "Terminal=false\n"
                                        "StartupNotify=true\n"
                                        "StartupWMClass=%3\n"
                                        "Categories=Utility;TextEditor;\n"
                                        "MimeType=%4;\n"
                                        "Keywords=text;editor;code;notepad;\n"
                                        "Keywords[es]=texto;editor;código;notepad;\n"
                                        "%5\n")
                             .arg(QString::fromLatin1(kAppName), desktopExec(_env.launcher), _env.appId,
                                  mimeTypes.join(QLatin1Char(';')), kGeneratedKey);
    if (!writeText(desktopFile(), text, error))
        return false;
    // El ícono, para Icon= (si no está ya)
    const QString icon = _env.dataHome + QStringLiteral("/icons/hicolor/256x256/apps/") + _env.appId + QStringLiteral(".png");
    if (!QFileInfo::exists(icon) && QFileInfo::exists(_env.iconFile) && QDir().mkpath(QFileInfo(icon).path())) {
        QFile::copy(_env.iconFile, icon);
        QFile::setPermissions(icon, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    }
    // La caché de tipos de los .desktop; no hace falta en todos los escritorios
    _env.run(QStringLiteral("update-desktop-database"), {_env.dataHome + QStringLiteral("/applications")});
    return true;
}

bool FileAssociations::add(const QString& extension, QString* error)
{
    const QString ext = normalizeExtension(extension);
    if (ext.isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("FileAssociation", "\"%1\" is not a valid extension").arg(extension);
        return false;
    }
    if (isRegistered(ext))
        return true;

    Association a;
    a.extension = ext;
    a.mimeType = mimeTypeFor(ext, &a.ownMimeType);
    const auto same = std::find_if(_registered.begin(), _registered.end(),
                                   [&a](const Association& other) { return other.mimeType == a.mimeType; });
    if (a.ownMimeType) {
        QList<Association> next = _registered;
        next << a;
        if (!writeMimePackage(next, error)) {
            writeMimePackage(_registered, nullptr); // como estaba
            return false;
        }
    }
    if (same != _registered.end()) {
        a.previous = same->previous; // el tipo ya es de Edita
    } else {
        MimeAppsList list(mimeAppsFile());
        const QString previous = list.value(kDefaultSection, a.mimeType);
        if (!desktopList(previous).contains(_env.desktopId()))
            a.previous = previous;
        list.setValue(kDefaultSection, a.mimeType, _env.desktopId());
        QStringList added = desktopList(list.value(kAddedSection, a.mimeType));
        added.removeAll(_env.desktopId());
        added.prepend(_env.desktopId());
        list.setValue(kAddedSection, a.mimeType, joinDesktopList(added));
        if (!list.save(error))
            return false;
    }
    _registered << a;
    if (!installedDesktopFile() && !writeDesktopFile(error))
        return false;
    return save(error);
}

bool FileAssociations::remove(const QString& extension, QString* error)
{
    const QString ext = normalizeExtension(extension);
    const auto it = std::find_if(_registered.begin(), _registered.end(),
                                 [&ext](const Association& a) { return a.extension == ext; });
    if (it == _registered.end())
        return false;
    const Association a = *it;
    _registered.erase(it);
    const bool shared = std::any_of(_registered.begin(), _registered.end(),
                                    [&a](const Association& other) { return other.mimeType == a.mimeType; });
    if (!shared) {
        MimeAppsList list(mimeAppsFile());
        if (desktopList(list.value(kDefaultSection, a.mimeType)).contains(_env.desktopId())) {
            if (a.previous.isEmpty())
                list.remove(kDefaultSection, a.mimeType);
            else
                list.setValue(kDefaultSection, a.mimeType, a.previous);
        }
        QStringList added = desktopList(list.value(kAddedSection, a.mimeType));
        if (added.removeAll(_env.desktopId()) > 0) {
            if (added.isEmpty())
                list.remove(kAddedSection, a.mimeType);
            else
                list.setValue(kAddedSection, a.mimeType, joinDesktopList(added));
        }
        if (!list.save(error))
            return false;
    }
    if (a.ownMimeType)
        writeMimePackage(_registered, error);
    if (!installedDesktopFile() && QFileInfo::exists(desktopFile()) && readText(desktopFile()).contains(kGeneratedKey))
        writeDesktopFile(nullptr);
    return save(error);
}

bool FileAssociations::refreshLauncher()
{
    const QString text = readText(desktopFile());
    if (!text.contains(QLatin1Char('\n') + kGeneratedKey + QLatin1Char('\n')))
        return false;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        if (!line.startsWith(QLatin1String("Exec=")))
            continue;
        const QString program = programFromExec(line.mid(5));
        if (program == _env.launcher || QFileInfo::exists(program))
            return false;
        return writeDesktopFile(nullptr);
    }
    return false;
}

} // namespace edita::file_association
