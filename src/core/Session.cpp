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

#include "core/Session.h"

#include "core/Backup.h"

#include <pugixml.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <sstream>
#include <string_view>

namespace edita {

namespace {

bool parseSession(const pugi::xml_document& doc, Session& session, QString* error)
{
    const pugi::xml_node root = doc.child("NotepadPlus").child("Session");
    if (!root) {
        if (error)
            *error = QCoreApplication::translate("Session", "<NotepadPlus><Session> is missing");
        return false;
    }
    Session result;
    result.activeView = root.attribute("activeView").as_int(0);
    const char* viewNames[] = {"mainView", "subView"};
    for (int k = 0; k < 2; ++k) {
        const pugi::xml_node view = root.child(viewNames[k]);
        if (!view)
            continue;
        const int activeIndex = view.attribute("activeIndex").as_int(0);
        (k == 0 ? result.activeMainIndex : result.activeSubIndex) = activeIndex;
        auto& files = k == 0 ? result.mainView : result.subView;
        for (const pugi::xml_node node : view.children("File")) {
            const char* filename = node.attribute("filename").as_string(nullptr);
            if (!filename || !filename[0])
                continue;
            SessionFile f;
            // Sesiones de Windows: "C:\a\b.txt" → "C:/a/b.txt" (no existirá y se omitirá al abrir)
            f.path = QDir::fromNativeSeparators(QString::fromUtf8(filename)).replace(QLatin1Char('\\'), QLatin1Char('/'));
            f.languageName = QString::fromUtf8(node.attribute("lang").as_string(""));
            f.encoding = node.attribute("encoding").as_int(-1);
            f.userReadOnly = std::string_view(node.attribute("userReadOnly").as_string("no")) == "yes";
            f.backupFilePath = QString::fromUtf8(node.attribute("backupFilePath").as_string("")).replace(QLatin1Char('\\'), QLatin1Char('/'));
            f.originalLastModified = fromFileTime({node.attribute("originalFileLastModifTimestamp").as_int(0),
                                                   node.attribute("originalFileLastModifTimestampHigh").as_int(0)});
            Position& p = f.position;
            p.firstVisibleLine = node.attribute("firstVisibleLine").as_llong(0);
            p.xOffset = node.attribute("xOffset").as_llong(0);
            p.scrollWidth = node.attribute("scrollWidth").as_llong(1);
            p.startPos = node.attribute("startPos").as_llong(0);
            p.endPos = node.attribute("endPos").as_llong(0);
            p.selMode = node.attribute("selMode").as_llong(0);
            for (const pugi::xml_node mark : node.children("Mark"))
                f.bookmarks.push_back(mark.attribute("line").as_ullong(0));
            for (const pugi::xml_node fold : node.children("Fold"))
                f.foldedLines.push_back(fold.attribute("line").as_ullong(0));
            files.push_back(std::move(f));
        }
    }
    session = std::move(result);
    return true;
}

void writeFileNode(pugi::xml_node parent, const SessionFile& f)
{
    // Mismos atributos y en el mismo orden que NppParameters::writeSession
    pugi::xml_node node = parent.append_child("File");
    const Position& p = f.position;
    node.append_attribute("firstVisibleLine") = static_cast<long long>(p.firstVisibleLine);
    node.append_attribute("xOffset") = static_cast<long long>(p.xOffset);
    node.append_attribute("scrollWidth") = static_cast<long long>(p.scrollWidth);
    node.append_attribute("startPos") = static_cast<long long>(p.startPos);
    node.append_attribute("endPos") = static_cast<long long>(p.endPos);
    node.append_attribute("selMode") = static_cast<long long>(p.selMode);
    node.append_attribute("offset") = 0;
    node.append_attribute("wrapCount") = 0;
    node.append_attribute("lang") = f.languageName.toUtf8().constData();
    node.append_attribute("encoding") = f.encoding;
    node.append_attribute("userReadOnly") = f.userReadOnly ? "yes" : "no";
    node.append_attribute("filename") = f.path.toUtf8().constData();
    node.append_attribute("backupFilePath") = f.backupFilePath.toUtf8().constData();
    const FileTimeParts time = toFileTime(f.originalLastModified);
    node.append_attribute("originalFileLastModifTimestamp") = time.low;
    node.append_attribute("originalFileLastModifTimestampHigh") = time.high;
    node.append_attribute("tabColourId") = -1;
    node.append_attribute("RTL") = "no";
    node.append_attribute("tabPinned") = "no";
    for (const std::size_t line : f.bookmarks)
        node.append_child("Mark").append_attribute("line") = static_cast<unsigned long long>(line);
    for (const std::size_t line : f.foldedLines)
        node.append_child("Fold").append_attribute("line") = static_cast<unsigned long long>(line);
}

} // namespace

bool readSessionFile(const QString& path, Session& session, QString* error)
{
    if (!QFileInfo::exists(path)) {
        if (error)
            *error = QCoreApplication::translate("Session", "%1 does not exist").arg(path);
        return false;
    }
    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_file(path.toUtf8().constData());
    if (!result) {
        if (error)
            *error = QCoreApplication::translate("Session", "%1 is not valid XML: %2").arg(path, QString::fromUtf8(result.description()));
        return false;
    }
    return parseSession(doc, session, error);
}

bool readSessionWithBackup(const QString& path, Session& session, bool* usedBackup, QString* error)
{
    if (usedBackup)
        *usedBackup = false;
    if (readSessionFile(path, session, error))
        return true;
    const QString backup = path + QLatin1String(kSessionBackupSuffix);
    if (QFileInfo::exists(path) && readSessionFile(backup, session, nullptr)) {
        if (usedBackup)
            *usedBackup = true;
        return true;
    }
    return false;
}

bool writeSessionFile(const QString& path, const Session& session, QString* error)
{
    // Copia de seguridad de la sesión anterior (writeSession)
    const QString backup = path + QLatin1String(kSessionBackupSuffix);
    bool haveBackup = false;
    if (QFileInfo::exists(path)) {
        QFile::remove(backup);
        haveBackup = QFile::copy(path, backup);
    }

    pugi::xml_document doc;
    pugi::xml_node decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";
    pugi::xml_node root = doc.append_child("NotepadPlus");
    pugi::xml_node sessionNode = root.append_child("Session");
    sessionNode.append_attribute("activeView") = session.activeView;
    pugi::xml_node mainNode = sessionNode.append_child("mainView");
    mainNode.append_attribute("activeIndex") = session.activeMainIndex;
    for (const SessionFile& f : session.mainView)
        writeFileNode(mainNode, f);
    pugi::xml_node subNode = sessionNode.append_child("subView");
    subNode.append_attribute("activeIndex") = session.activeSubIndex;
    for (const SessionFile& f : session.subView)
        writeFileNode(subNode, f);

    std::ostringstream out;
    doc.save(out, "    ", pugi::format_indent);
    const std::string text = out.str();
    QSaveFile file(path);
    bool ok = file.open(QIODevice::WriteOnly)
        && file.write(text.data(), static_cast<qint64>(text.size())) == static_cast<qint64>(text.size()) && file.commit();

    // Comprobación doble: que lo escrito se pueda volver a leer
    Session check;
    if (ok)
        ok = readSessionFile(path, check, nullptr);
    if (!ok) {
        if (error)
            *error = QCoreApplication::translate("Session", "Could not save the session to %1").arg(path);
        if (haveBackup) {
            QFile::remove(path);
            QFile::copy(backup, path);
        }
    }
    return ok;
}

} // namespace edita
