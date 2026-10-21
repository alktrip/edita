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


#include "syntax/UserLanguages.h"

#include "syntax/StyleXml.h"

#include "core/Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

#include <cstdio>
#include <cstring>
#include <sstream>

namespace edita {

namespace {

constexpr const char* kKeywordListNames[UserKeywordList::Total] = {
    "Comments",
    "Numbers, prefix1", "Numbers, prefix2", "Numbers, extras1", "Numbers, extras2",
    "Numbers, suffix1", "Numbers, suffix2", "Numbers, range",
    "Operators1", "Operators2",
    "Folders in code1, open", "Folders in code1, middle", "Folders in code1, close",
    "Folders in code2, open", "Folders in code2, middle", "Folders in code2, close",
    "Folders in comment, open", "Folders in comment, middle", "Folders in comment, close",
    "Keywords1", "Keywords2", "Keywords3", "Keywords4", "Keywords5", "Keywords6", "Keywords7", "Keywords8",
    "Delimiters",
};

constexpr const char* kStyleNames[kUserStyleCount] = {
    "DEFAULT", "COMMENTS", "LINE COMMENTS", "NUMBERS",
    "KEYWORDS1", "KEYWORDS2", "KEYWORDS3", "KEYWORDS4", "KEYWORDS5", "KEYWORDS6", "KEYWORDS7", "KEYWORDS8",
    "OPERATORS", "FOLDER IN CODE1", "FOLDER IN CODE2", "FOLDER IN COMMENT",
    "DELIMITERS1", "DELIMITERS2", "DELIMITERS3", "DELIMITERS4", "DELIMITERS5", "DELIMITERS6", "DELIMITERS7", "DELIMITERS8",
};

// Nombres antiguos (anteriores a la versión 2.0/2.1 del formato) que N++ sigue aceptando
int keywordListFromName(std::string_view name)
{
    for (int i = 0; i < UserKeywordList::Total; ++i) {
        if (name == kKeywordListNames[i])
            return i;
    }
    static constexpr std::pair<const char*, int> kAliases[] = {
        {"Operators", UserKeywordList::Operators1},
        {"Folder+", UserKeywordList::FoldersInCode1Open},
        {"Folder-", UserKeywordList::FoldersInCode1Close},
        {"Words1", UserKeywordList::Keywords1}, {"Words2", UserKeywordList::Keywords2},
        {"Words3", UserKeywordList::Keywords3}, {"Words4", UserKeywordList::Keywords4},
        {"Numbers, additional", UserKeywordList::NumberRange},
        {"Numbers, prefixes", UserKeywordList::NumberPrefix2},
        {"Numbers, extras with prefixes", UserKeywordList::NumberExtras2},
        {"Numbers, suffixes", UserKeywordList::NumberSuffix2},
    };
    for (const auto& [alias, index] : kAliases) {
        if (name == alias)
            return index;
    }
    return -1;
}

int styleFromName(std::string_view name)
{
    for (int i = 0; i < kUserStyleCount; ++i) {
        if (name == kStyleNames[i])
            return i;
    }
    static constexpr std::pair<const char*, int> kAliases[] = {
        {"FOLDEROPEN", 13}, {"FOLDERCLOSE", 13}, {"KEYWORD1", 4}, {"KEYWORD2", 5}, {"KEYWORD3", 6}, {"KEYWORD4", 7},
        {"COMMENT", 1}, {"COMMENT LINE", 2}, {"NUMBER", 3}, {"OPERATOR", 12},
        {"DELIMINER1", 16}, {"DELIMINER2", 17}, {"DELIMINER3", 18},
    };
    for (const auto& [alias, id] : kAliases) {
        if (name == alias)
            return id;
    }
    return -1;
}

bool yes(const pugi::xml_node& node, const char* attr)
{
    return std::strcmp(node.attribute(attr).as_string("no"), "yes") == 0;
}

int nextLexerId()
{
    static int counter = 0;
    return ++counter;
}

void replaceAll(std::string& s, const std::string& from, const std::string& to, bool continueAfter)
{
    std::size_t pos = s.find(from);
    while (pos != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos = s.find(from, continueAfter ? pos + 1 : 0);
    }
}

// NppParameters::feedUserKeywordList
void readKeywordLists(const pugi::xml_node& root, const std::string& udlVersion, UserLanguage& lang)
{
    for (const pugi::xml_node node : root.children("Keywords")) {
        const std::string_view name = node.attribute("name").as_string("");
        const char* kwl = node.child_value();
        if (udlVersion.empty() && name == "Delimiters") {
            // Formato antiguo: seis caracteres "abcdef" → casillas numeradas
            const std::string old = kwl;
            auto at = [&old](std::size_t i) { return i < old.size() && old[i] != '0' ? std::string(1, old[i]) : std::string(); };
            std::string temp = "00" + at(0) + " 01 02" + at(3) + " 03" + at(1) + " 04 05" + at(4) + " 06" + at(2)
                             + " 07 08" + at(5) + " 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23";
            lang.keywordLists[UserKeywordList::Delimiters] = temp;
        } else if (name == "Comment") {
            // Formato antiguo: "0" abre línea, "1"/"2" abren y cierran bloque
            std::string temp = std::string(" ") + kwl;
            replaceAll(temp, " 0", " 00", true);
            replaceAll(temp, " 1", " 03", false);
            replaceAll(temp, " 2", " 04", false);
            temp += " 01 02";
            if (!temp.empty() && temp[0] == ' ')
                temp.erase(0, 1);
            lang.keywordLists[UserKeywordList::Comments] = temp;
        } else if (const int index = keywordListFromName(name); index >= 0) {
            lang.keywordLists[static_cast<std::size_t>(index)] = kwl;
        }
    }
}

std::string colorToHex(int bgr)
{
    const auto v = static_cast<unsigned int>(bgr);
    char buffer[8];
    std::snprintf(buffer, sizeof buffer, "%.6X", ((v & 0xFF) << 16) | (v & 0xFF00) | ((v >> 16) & 0xFF));
    return buffer;
}

} // namespace

const char* userKeywordListName(int index)
{
    return index >= 0 && index < UserKeywordList::Total ? kKeywordListNames[index] : "";
}

const char* userStyleName(int id)
{
    return id >= 0 && id < kUserStyleCount ? kStyleNames[id] : "";
}

const Style* UserLanguage::style(int id) const
{
    for (const Style& s : styles) {
        if (s.id == id)
            return &s;
    }
    return nullptr;
}

Style* UserLanguage::style(int id)
{
    return const_cast<Style*>(std::as_const(*this).style(id));
}

bool UserLanguage::matches(const QString& filePath) const
{
    const QFileInfo info(filePath);
    const QString suffix = info.suffix();
    const QString fileName = info.fileName();
    for (const QString& ext : extensions.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if ((!suffix.isEmpty() && ext.compare(suffix, Qt::CaseInsensitive) == 0)
            || (fileName.contains(QLatin1Char('.')) && ext.compare(fileName, Qt::CaseInsensitive) == 0))
            return true;
    }
    return false;
}

QString userSlotWords(const std::string& list, int slot)
{
    // CommentStyleDialog::retrieve
    char prefix[12];
    std::snprintf(prefix, sizeof prefix, "%02d", slot);
    std::string dest;
    bool copying = false;
    bool inGroup = false;
    const std::size_t len = list.size();
    auto at = [&](std::size_t i) { return i < len ? list[i] : '\0'; };
    for (std::size_t i = 0; i < len; ++i) {
        if ((i == 0 || list[i - 1] == ' ') && list[i] == prefix[0] && at(i + 1) == prefix[1]) {
            if (!dest.empty())
                dest += ' ';
            copying = true;
            ++i;
            continue;
        }
        if (list[i] == '(' && at(i + 1) == '(' && !inGroup && copying)
            inGroup = true;
        if (i >= 2 && list[i] != ')' && list[i - 1] == ')' && list[i - 2] == ')' && inGroup)
            inGroup = false;
        if (list[i] == ' ' && copying)
            copying = false;
        if (copying || inGroup)
            dest += list[i];
    }
    return QString::fromStdString(dest);
}

std::string userComposeSlots(const QStringList& slotTexts)
{
    // convertTo de UserDefineDialog.cpp, para cada casilla
    std::string dest;
    for (int slot = 0; slot < slotTexts.size(); ++slot) {
        char prefix[12];
        std::snprintf(prefix, sizeof prefix, "%02d", slot);
        const std::string words = slotTexts[slot].toStdString();
        if (!dest.empty())
            dest += ' ';
        dest += prefix;
        bool inGroup = false;
        const std::size_t len = words.size();
        auto at = [&](std::size_t i) { return i < len ? words[i] : '\0'; };
        for (std::size_t i = 0; i < len; ++i) {
            if (i == 0 && words[i] == '(' && at(i + 1) == '(') {
                inGroup = true;
            } else if (words[i] == ' ' && at(i + 1) == '(' && at(i + 2) == '(') {
                inGroup = true;
                dest += ' ';
                dest += prefix;
                ++i;
            }
            if (inGroup && i >= 2 && words[i - 1] == ')' && words[i - 2] == ')')
                inGroup = false;
            if (words[i] == ' ') {
                if (at(i + 1) != ' ' && at(i + 1) != '\0') {
                    dest += ' ';
                    if (!inGroup)
                        dest += prefix;
                }
            } else {
                dest += words[i];
            }
        }
    }
    return dest;
}

UserLanguage newUserLanguage(const QString& name)
{
    UserLanguage lang;
    lang.name = name;
    lang.lexerId = nextLexerId();
    // Casillas vacías de comentarios y delimitadores, como las deja el diálogo de N++
    lang.keywordLists[UserKeywordList::Comments] = userComposeSlots(QStringList(5, QString()));
    lang.keywordLists[UserKeywordList::Delimiters] = userComposeSlots(QStringList(24, QString()));
    for (int id = 0; id < kUserStyleCount; ++id) {
        Style s;
        s.id = id;
        s.name = kStyleNames[id];
        if (id != 0)
            s.colorStyle = kColorStyleForeground; // fondo transparente salvo en DEFAULT
        lang.styles.push_back(s);
    }
    return lang;
}

// --- Archivos --------------------------------------------------------------------------------

bool UserLanguages::readFile(const QString& path, std::vector<UserLanguage>& languages, QString* error)
{
    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_file(QFile::encodeName(path).constData());
    if (!result) {
        if (error)
            *error = QCoreApplication::translate("UserLanguages", "%1 is not valid XML: %2").arg(path, QString::fromUtf8(result.description()));
        return false;
    }
    // NppParameters::feedUserLang
    for (const pugi::xml_node node : doc.child("NotepadPlus").children("UserLang")) {
        UserLanguage lang;
        lang.name = QString::fromUtf8(node.attribute("name").as_string(""));
        if (lang.name.isEmpty())
            continue;
        lang.extensions = QString::fromUtf8(node.attribute("ext").as_string(""));
        lang.darkModeTheme = yes(node, "darkModeTheme");
        const std::string udlVersion = node.attribute("udlVersion").as_string("");
        const pugi::xml_node settings = node.child("Settings");
        const pugi::xml_node keywordLists = node.child("KeywordLists");
        const pugi::xml_node stylesNode = node.child("Styles");
        if (!settings || !keywordLists || !stylesNode)
            continue; // N++ descarta el lenguaje si falta alguna sección

        if (const pugi::xml_node global = settings.child("Global")) {
            lang.caseIgnored = yes(global, "caseIgnored");
            lang.allowFoldOfComments = yes(global, "allowFoldOfComments");
            lang.foldCompact = yes(global, "foldCompact");
            lang.forcePureLC = global.attribute("forcePureLC").as_int(0);
            lang.decimalSeparator = global.attribute("decimalSeparator").as_int(0);
        }
        if (const pugi::xml_node prefix = settings.child("Prefix")) {
            const bool modern = udlVersion == "2.1" || udlVersion == "2.0";
            for (int i = 0; i < (modern ? 8 : 4); ++i) {
                const std::string attr = modern ? kKeywordListNames[UserKeywordList::Keywords1 + i] : "words" + std::to_string(i + 1);
                lang.isPrefix[static_cast<std::size_t>(i)] = yes(prefix, attr.c_str());
            }
        }
        readKeywordLists(keywordLists, udlVersion, lang);

        // Estilos; los que falten, con los valores por defecto
        const UserLanguage defaults = newUserLanguage(lang.name);
        lang.styles = defaults.styles;
        for (const pugi::xml_node styleNode : stylesNode.children("WordsStyle")) {
            const int id = styleFromName(styleNode.attribute("name").as_string(""));
            if (id < 0)
                continue;
            Style s = parseStyle(styleNode);
            s.id = id;
            s.name = kStyleNames[id];
            *lang.style(id) = s;
        }
        lang.sourceFile = path;
        lang.lexerId = nextLexerId();
        languages.push_back(std::move(lang));
    }
    return true;
}

bool UserLanguages::writeFile(const QString& path, const std::vector<UserLanguage>& languages, QString* error)
{
    // NppParameters::insertUserLang2Tree
    pugi::xml_document doc;
    pugi::xml_node decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";
    pugi::xml_node root = doc.append_child("NotepadPlus");
    for (const UserLanguage& lang : languages) {
        pugi::xml_node node = root.append_child("UserLang");
        node.append_attribute("name") = lang.name.toUtf8().constData();
        node.append_attribute("ext") = lang.extensions.toUtf8().constData();
        if (lang.darkModeTheme)
            node.append_attribute("darkModeTheme") = "yes";
        node.append_attribute("udlVersion") = "2.1";
        pugi::xml_node settings = node.append_child("Settings");
        pugi::xml_node global = settings.append_child("Global");
        global.append_attribute("caseIgnored") = lang.caseIgnored ? "yes" : "no";
        global.append_attribute("allowFoldOfComments") = lang.allowFoldOfComments ? "yes" : "no";
        global.append_attribute("foldCompact") = lang.foldCompact ? "yes" : "no";
        global.append_attribute("forcePureLC") = lang.forcePureLC;
        global.append_attribute("decimalSeparator") = lang.decimalSeparator;
        pugi::xml_node prefix = settings.append_child("Prefix");
        for (int i = 0; i < 8; ++i)
            prefix.append_attribute(kKeywordListNames[UserKeywordList::Keywords1 + i]) = lang.isPrefix[static_cast<std::size_t>(i)] ? "yes" : "no";
        pugi::xml_node lists = node.append_child("KeywordLists");
        for (int i = 0; i < UserKeywordList::Total; ++i) {
            pugi::xml_node kw = lists.append_child("Keywords");
            kw.append_attribute("name") = kKeywordListNames[i];
            kw.text() = lang.keywordLists[static_cast<std::size_t>(i)].c_str();
        }
        pugi::xml_node stylesNode = node.append_child("Styles");
        for (const Style& s : lang.styles) {
            if (s.id < 0 || s.id >= kUserStyleCount)
                continue;
            pugi::xml_node style = stylesNode.append_child("WordsStyle");
            style.append_attribute("name") = kStyleNames[s.id];
            style.append_attribute("fgColor") = colorToHex(s.fgColor == kStyleNotUsed ? 0 : s.fgColor).c_str();
            style.append_attribute("bgColor") = colorToHex(s.bgColor == kStyleNotUsed ? 0xFFFFFF : s.bgColor).c_str();
            if (s.colorStyle != kColorStyleAll)
                style.append_attribute("colorStyle") = s.colorStyle;
            if (!s.fontName.empty())
                style.append_attribute("fontName") = s.fontName.c_str();
            style.append_attribute("fontStyle") = s.fontStyle == kStyleNotUsed ? 0 : s.fontStyle;
            if (s.fontSize != kStyleNotUsed) {
                if (s.fontSize == 0)
                    style.append_attribute("fontSize") = "";
                else
                    style.append_attribute("fontSize") = s.fontSize;
            }
            style.append_attribute("nesting") = s.nesting;
        }
    }
    std::ostringstream out;
    doc.save(out, "    ", pugi::format_indent);
    const std::string text = out.str();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    const bool ok = file.open(QIODevice::WriteOnly)
        && file.write(text.data(), static_cast<qint64>(text.size())) == static_cast<qint64>(text.size()) && file.commit();
    if (!ok && error)
        *error = QCoreApplication::translate("UserLanguages", "Could not write %1").arg(path);
    return ok;
}

// --- Colección -------------------------------------------------------------------------------

QStringList UserLanguages::load(const QString& configDir)
{
    _configDir = configDir;
    _languages.clear();
    QStringList warnings;
    QStringList files;
    const QString single = QDir(configDir).filePath(QStringLiteral("userDefineLang.xml"));
    if (QFileInfo::exists(single))
        files << single;
    const QDir folder(QDir(configDir).filePath(QStringLiteral("userDefineLangs")));
    for (const QString& name : folder.entryList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name))
        files << folder.filePath(name);
    // Los que trae la aplicación (los Markdown preinstalados de N++), detrás de los del usuario
    for (const QString& data : paths::dataDirs()) {
        const QDir installed(QDir(data).filePath(QStringLiteral("userDefineLangs")));
        for (const QString& name : installed.entryList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name))
            files << installed.filePath(name);
    }
    for (const QString& file : files) {
        QString error;
        std::vector<UserLanguage> found;
        if (!readFile(file, found, &error)) {
            warnings << error;
            continue;
        }
        for (UserLanguage& lang : found) {
            if (find(lang.name)) // el primero gana, como en N++
                continue;
            _languages.push_back(std::move(lang));
        }
    }
    return warnings;
}

const UserLanguage* UserLanguages::find(const QString& name) const
{
    for (const UserLanguage& lang : _languages) {
        if (lang.name == name)
            return &lang;
    }
    return nullptr;
}

const UserLanguage* UserLanguages::forFile(const QString& filePath, bool darkMode) const
{
    // getUserDefinedLangNameFromExt: mejor uno del modo actual (darkModeTheme)
    const UserLanguage* matched = nullptr;
    for (const UserLanguage& lang : _languages) {
        if (!lang.matches(filePath))
            continue;
        if (lang.darkModeTheme == darkMode)
            return &lang;
        if (!matched)
            matched = &lang;
    }
    return matched;
}

bool UserLanguages::isUserFile(const QString& file) const
{
    return !file.isEmpty() && QFileInfo(file).absoluteFilePath().startsWith(QDir(_configDir).absolutePath() + QLatin1Char('/'));
}

bool UserLanguages::rewriteSource(const QString& file, QString* error)
{
    std::vector<UserLanguage> inFile;
    for (const UserLanguage& lang : _languages) {
        if (lang.sourceFile == file)
            inFile.push_back(lang);
    }
    if (inFile.empty()) {
        QFile::remove(file);
        return true;
    }
    return writeFile(file, inFile, error);
}

bool UserLanguages::save(UserLanguage language, QString* error)
{
    language.lexerId = nextLexerId(); // LexUser vuelve a leer las listas
    // Uno instalado con la aplicación se guarda como copia del usuario
    if (!language.sourceFile.isEmpty() && !isUserFile(language.sourceFile))
        language.sourceFile.clear();
    auto it = std::find_if(_languages.begin(), _languages.end(), [&](const UserLanguage& l) { return l.name == language.name; });
    if (language.sourceFile.isEmpty() && it != _languages.end() && isUserFile(it->sourceFile))
        language.sourceFile = it->sourceFile;
    if (language.sourceFile.isEmpty()) {
        QString safe = language.name;
        safe.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("_"));
        language.sourceFile = QDir(QDir(_configDir).filePath(QStringLiteral("userDefineLangs"))).filePath(safe + QStringLiteral(".xml"));
    }
    if (it != _languages.end())
        *it = language;
    else
        _languages.push_back(language);
    return rewriteSource(language.sourceFile, error);
}

bool UserLanguages::remove(const QString& name, QString* error)
{
    auto it = std::find_if(_languages.begin(), _languages.end(), [&](const UserLanguage& l) { return l.name == name; });
    if (it == _languages.end())
        return false;
    if (!isUserFile(it->sourceFile)) {
        if (error)
            *error = QCoreApplication::translate("UserLanguages", "\"%1\" ships with the application and cannot be removed").arg(name);
        return false;
    }
    const QString file = it->sourceFile;
    _languages.erase(it);
    return rewriteSource(file, error);
}

bool UserLanguages::importFile(const QString& path, QStringList* imported, QString* error)
{
    std::vector<UserLanguage> found;
    if (!readFile(path, found, error))
        return false;
    if (found.empty()) {
        if (error)
            *error = QCoreApplication::translate("UserLanguages", "%1 does not contain any user defined language").arg(path);
        return false;
    }
    // Se copia a userDefineLangs/ (como "Importar…" de N++) y se guardan sus lenguajes allí
    const QString target = QDir(QDir(_configDir).filePath(QStringLiteral("userDefineLangs"))).filePath(QFileInfo(path).fileName());
    for (UserLanguage& lang : found) {
        auto it = std::find_if(_languages.begin(), _languages.end(), [&](const UserLanguage& l) { return l.name == lang.name; });
        const QString oldSource = it != _languages.end() ? it->sourceFile : QString();
        if (it != _languages.end())
            _languages.erase(it);
        lang.sourceFile = target;
        if (imported)
            *imported << lang.name;
        _languages.push_back(lang);
        if (!oldSource.isEmpty() && oldSource != target)
            rewriteSource(oldSource, nullptr);
    }
    return rewriteSource(target, error);
}

bool UserLanguages::exportLanguage(const QString& name, const QString& path, QString* error) const
{
    const UserLanguage* lang = find(name);
    if (!lang) {
        if (error)
            *error = QCoreApplication::translate("UserLanguages", "The language \"%1\" does not exist").arg(name);
        return false;
    }
    return writeFile(path, {*lang}, error);
}

} // namespace edita
