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

#include "core/ConfigXml.h"

#include <pugixml.hpp>

#include <algorithm>

#include <QFileInfo>
#include <QSaveFile>

#include <sstream>
#include <string_view>

namespace edita {

namespace {

pugi::xml_node guiConfigs(pugi::xml_document& doc)
{
    pugi::xml_node root = doc.child("NotepadPlus");
    if (!root)
        root = doc.append_child("NotepadPlus");
    pugi::xml_node configs = root.child("GUIConfigs");
    if (!configs)
        configs = root.append_child("GUIConfigs");
    return configs;
}

} // namespace

ConfigXml::ConfigXml()
    : _doc(std::make_unique<pugi::xml_document>())
{
}

ConfigXml::~ConfigXml() = default;

bool ConfigXml::load(const QString& path)
{
    _path = path;
    _doc->reset();
    if (!QFileInfo::exists(path))
        return true;
    if (_doc->load_file(path.toUtf8().constData()))
        return true;
    _doc->reset();
    return false;
}

bool ConfigXml::save() const
{
    if (_path.isEmpty())
        return false;
    pugi::xml_document copy;
    copy.reset(*_doc);
    if (!copy.first_child() || copy.first_child().type() != pugi::node_declaration) {
        pugi::xml_node decl = copy.prepend_child(pugi::node_declaration);
        decl.append_attribute("version") = "1.0";
        decl.append_attribute("encoding") = "UTF-8";
    }
    std::ostringstream out;
    copy.save(out, "    ", pugi::format_indent);
    const std::string text = out.str();

    QSaveFile file(_path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(text.data(), static_cast<qint64>(text.size()));
    return file.commit();
}

QString ConfigXml::guiConfig(const QString& name, const QString& attribute, const QString& fallback) const
{
    const pugi::xml_node node = _doc->child("NotepadPlus").child("GUIConfigs")
                                    .find_child_by_attribute("GUIConfig", "name", name.toUtf8().constData());
    const pugi::xml_attribute attr = node.attribute(attribute.toUtf8().constData());
    return attr ? QString::fromUtf8(attr.value()) : fallback;
}

void ConfigXml::setGuiConfig(const QString& name, const QString& attribute, const QString& value)
{
    pugi::xml_node configs = guiConfigs(*_doc);
    const QByteArray nameUtf8 = name.toUtf8();
    pugi::xml_node node = configs.find_child_by_attribute("GUIConfig", "name", nameUtf8.constData());
    if (!node) {
        node = configs.append_child("GUIConfig");
        node.append_attribute("name") = nameUtf8.constData();
    }
    const QByteArray attrUtf8 = attribute.toUtf8();
    pugi::xml_attribute attr = node.attribute(attrUtf8.constData());
    if (!attr)
        attr = node.append_attribute(attrUtf8.constData());
    attr = value.toUtf8().constData();
}

QString ConfigXml::guiConfigText(const QString& name, const QString& fallback) const
{
    const pugi::xml_node node = _doc->child("NotepadPlus").child("GUIConfigs")
                                    .find_child_by_attribute("GUIConfig", "name", name.toUtf8().constData());
    return node ? QString::fromUtf8(node.text().as_string()) : fallback;
}

void ConfigXml::setGuiConfigText(const QString& name, const QString& value)
{
    pugi::xml_node configs = guiConfigs(*_doc);
    const QByteArray nameUtf8 = name.toUtf8();
    pugi::xml_node node = configs.find_child_by_attribute("GUIConfig", "name", nameUtf8.constData());
    if (!node) {
        node = configs.append_child("GUIConfig");
        node.append_attribute("name") = nameUtf8.constData();
    }
    node.text() = value.toUtf8().constData();
}

RecentFilesConfig ConfigXml::recentFiles() const
{
    RecentFilesConfig recent;
    const pugi::xml_node node = _doc->child("NotepadPlus").child("History");
    if (!node)
        return recent;
    recent.max = std::clamp(node.attribute("nbMaxFile").as_int(recent.max), 0, 30);
    recent.inSubMenu = std::string_view(node.attribute("inSubMenu").as_string("no")) == "yes";
    recent.customLength = node.attribute("customLength").as_int(-1);
    for (const pugi::xml_node file : node.children("File")) {
        const QString path = QString::fromUtf8(file.attribute("filename").as_string(""));
        if (!path.isEmpty() && recent.files.size() < 30)
            recent.files << path;
    }
    return recent;
}

void ConfigXml::setRecentFiles(const RecentFilesConfig& recent)
{
    pugi::xml_node root = _doc->child("NotepadPlus");
    if (!root)
        root = _doc->append_child("NotepadPlus");
    pugi::xml_node node = root.child("History");
    if (node)
        root.remove_child(node);
    // Antes de <GUIConfigs>, donde lo escribe N++
    const pugi::xml_node configs = root.child("GUIConfigs");
    node = configs ? root.insert_child_before("History", configs) : root.append_child("History");
    node.append_attribute("nbMaxFile") = recent.max;
    node.append_attribute("inSubMenu") = recent.inSubMenu ? "yes" : "no";
    node.append_attribute("customLength") = recent.customLength;
    for (const QString& path : recent.files)
        node.append_child("File").append_attribute("filename") = path.toUtf8().constData();
}

namespace {

QStringList readNames(const pugi::xml_node& parent, const char* child)
{
    QStringList names;
    for (const pugi::xml_node node : parent.children(child))
        names << QString::fromUtf8(node.attribute("name").as_string(""));
    return names;
}

void writeNames(pugi::xml_node parent, const char* child, const QStringList& names)
{
    for (const QString& name : names)
        parent.append_child(child).append_attribute("name") = name.toUtf8().constData();
}

void setBool(pugi::xml_node node, const char* name, bool value)
{
    pugi::xml_attribute a = node.attribute(name);
    if (!a)
        a = node.append_attribute(name);
    a = value ? "yes" : "no";
}

bool getBool(const pugi::xml_node& node, const char* name, bool fallback)
{
    const pugi::xml_attribute a = node.attribute(name);
    return a ? std::string_view(a.value()) == "yes" : fallback;
}

} // namespace

FindHistory ConfigXml::findHistory() const
{
    FindHistory h;
    const pugi::xml_node node = _doc->child("NotepadPlus").child("FindHistory");
    if (!node)
        return h;
    h.paths = readNames(node, "Path");
    h.filters = readNames(node, "Filter");
    h.finds = readNames(node, "Find");
    h.replaces = readNames(node, "Replace");
    h.matchWord = getBool(node, "matchWord", h.matchWord);
    h.matchCase = getBool(node, "matchCase", h.matchCase);
    h.wrap = getBool(node, "wrap", h.wrap);
    h.directionDown = getBool(node, "directionDown", h.directionDown);
    h.recursive = getBool(node, "fifRecuisive", h.recursive); // sic, como en N++
    h.hidden = getBool(node, "fifInHiddenFolder", h.hidden);
    h.dotMatchesNewline = getBool(node, "dotMatchesNewline", h.dotMatchesNewline);
    h.folderFollowsDoc = getBool(node, "fifFolderFollowsDoc", h.folderFollowsDoc);
    h.searchMode = node.attribute("searchMode").as_int(0);
    return h;
}

void ConfigXml::setFindHistory(const FindHistory& h)
{
    pugi::xml_node root = _doc->child("NotepadPlus");
    if (!root)
        root = _doc->append_child("NotepadPlus");
    root.remove_child("FindHistory");
    pugi::xml_node node = root.append_child("FindHistory");
    node.append_attribute("nbMaxFindHistoryPath") = 10;
    node.append_attribute("nbMaxFindHistoryFilter") = 10;
    node.append_attribute("nbMaxFindHistoryFind") = 10;
    node.append_attribute("nbMaxFindHistoryReplace") = 10;
    setBool(node, "matchWord", h.matchWord);
    setBool(node, "matchCase", h.matchCase);
    setBool(node, "wrap", h.wrap);
    setBool(node, "directionDown", h.directionDown);
    setBool(node, "fifRecuisive", h.recursive);
    setBool(node, "fifInHiddenFolder", h.hidden);
    setBool(node, "fifFolderFollowsDoc", h.folderFollowsDoc);
    node.append_attribute("searchMode") = h.searchMode;
    setBool(node, "dotMatchesNewline", h.dotMatchesNewline);
    writeNames(node, "Path", h.paths.mid(0, 10));
    writeNames(node, "Filter", h.filters.mid(0, 10));
    writeNames(node, "Find", h.finds.mid(0, 10));
    writeNames(node, "Replace", h.replaces.mid(0, 10));
}

} // namespace edita
