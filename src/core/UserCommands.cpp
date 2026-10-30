// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/UserCommands.h"

#include <pugixml.hpp>

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

#include <sstream>

namespace edita {

namespace {

bool isYes(const pugi::xml_node& node, const char* name)
{
    return QString::fromUtf8(node.attribute(name).as_string("no")).compare(QLatin1String("yes"), Qt::CaseInsensitive) == 0;
}

} // namespace

bool readUserCommands(const QString& path, std::vector<UserCommand>& commands)
{
    commands.clear();
    if (!QFileInfo::exists(path))
        return true;
    pugi::xml_document doc;
    if (!doc.load_file(QFile::encodeName(path).constData()))
        return false;
    for (const pugi::xml_node node : doc.child("NotepadPlus").child("UserDefinedCommands").children("Command")) {
        UserCommand c;
        c.name = QString::fromUtf8(node.attribute("name").as_string(""));
        c.command = QString::fromUtf8(node.child_value());
        if (c.name.isEmpty() || c.command.isEmpty())
            continue;
        c.ctrl = isYes(node, "Ctrl");
        c.alt = isYes(node, "Alt");
        c.shift = isYes(node, "Shift");
        c.key = node.attribute("Key").as_int(0);
        commands.push_back(std::move(c));
    }
    return true;
}

bool writeUserCommands(const QString& path, const std::vector<UserCommand>& commands)
{
    pugi::xml_document doc;
    if (QFileInfo::exists(path) && !doc.load_file(QFile::encodeName(path).constData()))
        return false; // no se pisa un shortcuts.xml que no se entiende
    if (!doc.child("NotepadPlus")) {
        pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
        decl.append_attribute("version") = "1.0";
        decl.append_attribute("encoding") = "UTF-8";
        pugi::xml_node root = doc.append_child("NotepadPlus");
        root.append_child("InternalCommands");
        root.append_child("Macros");
        root.append_child("UserDefinedCommands");
        root.append_child("PluginCommands");
        root.append_child("ScintillaKeys");
    }
    pugi::xml_node root = doc.child("NotepadPlus");
    pugi::xml_node section = root.child("UserDefinedCommands");
    if (section)
        section.remove_children();
    else
        section = root.insert_child_after("UserDefinedCommands", root.child("Macros"));
    for (const UserCommand& c : commands) {
        pugi::xml_node node = section.append_child("Command");
        node.append_attribute("name") = c.name.toUtf8().constData();
        node.append_attribute("Ctrl") = c.ctrl ? "yes" : "no";
        node.append_attribute("Alt") = c.alt ? "yes" : "no";
        node.append_attribute("Shift") = c.shift ? "yes" : "no";
        node.append_attribute("Key") = c.key;
        node.append_child(pugi::node_pcdata).set_value(c.command.toUtf8().constData());
    }
    std::ostringstream out;
    doc.save(out, "    ", pugi::format_indent);
    const std::string text = out.str();
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(text.data(), static_cast<qint64>(text.size())) == static_cast<qint64>(text.size()) && file.commit();
}

std::vector<UserCommand> defaultUserCommands(const QString& editaPath)
{
    // Las teclas de N++: Alt+F1, Alt+F3 y Alt+F6 (VK_F1 = 112)
    UserCommand php{QStringLiteral("Get PHP help"), false, true, false, 112, QStringLiteral("https://www.php.net/$(CURRENT_WORD)")};
    UserCommand wikipedia{QStringLiteral("Wikipedia Search"), false, true, false, 114,
                          QStringLiteral("https://en.wikipedia.org/wiki/Special:Search?search=$(CURRENT_WORD)")};
    UserCommand instance{QStringLiteral("Open selected file path in new instance"), false, true, false, 117,
                         QStringLiteral("\"%1\" --multiInst --nosession \"$(CURRENT_WORD)\"").arg(editaPath)};
    return {php, wikipedia, instance};
}

const std::vector<QString>& runVariableNames()
{
    static const std::vector<QString> names{
        QStringLiteral("FULL_CURRENT_PATH"), QStringLiteral("CURRENT_DIRECTORY"), QStringLiteral("FILE_NAME"),
        QStringLiteral("NAME_PART"),         QStringLiteral("EXT_PART"),          QStringLiteral("CURRENT_WORD"),
        QStringLiteral("CURRENT_LINE"),      QStringLiteral("CURRENT_COLUMN"),    QStringLiteral("CURRENT_LINESTR"),
        QStringLiteral("NPP_DIRECTORY"),     QStringLiteral("NPP_FULL_FILE_PATH"),
    };
    return names;
}

QString expandRunVariables(const QString& command, const RunVariables& v)
{
    const QFileInfo file(v.fullCurrentPath);
    auto value = [&](const QString& name, bool& known) -> QString {
        known = true;
        if (name == QLatin1String("FULL_CURRENT_PATH"))
            return v.fullCurrentPath;
        if (name == QLatin1String("CURRENT_DIRECTORY"))
            return file.isAbsolute() ? file.absolutePath() : QString();
        if (name == QLatin1String("FILE_NAME"))
            return file.fileName();
        if (name == QLatin1String("NAME_PART"))
            return file.completeBaseName();
        if (name == QLatin1String("EXT_PART"))
            return file.suffix().isEmpty() ? QString() : QLatin1Char('.') + file.suffix();
        if (name == QLatin1String("CURRENT_WORD"))
            return v.currentWord;
        if (name == QLatin1String("CURRENT_LINE"))
            return QString::number(v.line);
        if (name == QLatin1String("CURRENT_COLUMN"))
            return QString::number(v.column);
        if (name == QLatin1String("CURRENT_LINESTR"))
            return v.currentLineText;
        if (name == QLatin1String("NPP_DIRECTORY"))
            return v.appDirectory;
        if (name == QLatin1String("NPP_FULL_FILE_PATH"))
            return v.appPath;
        known = false;
        return {};
    };
    static const QRegularExpression variable(QStringLiteral("\\$\\(([A-Za-z_]+)\\)"));
    QString out;
    qsizetype last = 0;
    for (auto it = variable.globalMatch(command); it.hasNext();) {
        const auto m = it.next();
        bool known = false;
        const QString text = value(m.captured(1), known);
        out += QStringView(command).mid(last, (known ? m.capturedStart() : m.capturedEnd()) - last);
        if (known)
            out += text;
        last = m.capturedEnd();
    }
    out += QStringView(command).mid(last);
    return out;
}

bool isUrlCommand(const QString& expanded)
{
    static const QRegularExpression url(QStringLiteral("^\\s*[A-Za-z][A-Za-z0-9+.-]*://\\S+\\s*$"));
    return url.match(expanded).hasMatch();
}

} // namespace edita
