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

#include "ui/FunctionParser.h"

#include "core/Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <BoostRegexSearch.h>
#include <pugixml.hpp>

namespace edita {

namespace {

constexpr uptr_t kSearchFlags = SCFIND_REGEXP | SCFIND_POSIX | SCFIND_REGEXP_DOTMATCHESNL;

// ScintillaEditView::searchInTarget
sptr_t searchInTarget(ScintillaEditBase& view, const std::string& expr, sptr_t begin, sptr_t end)
{
    view.send(SCI_SETTARGETRANGE, static_cast<uptr_t>(begin), end);
    return view.send(SCI_SEARCHINTARGET, expr.size(), reinterpret_cast<sptr_t>(expr.c_str()));
}

std::string textRange(ScintillaEditBase& view, sptr_t start, sptr_t end)
{
    if (end <= start)
        return {};
    std::string text(static_cast<std::size_t>(end - start) + 1, '\0');
    Sci_TextRangeFull range{{start, end}, text.data()};
    view.send(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&range));
    text.resize(static_cast<std::size_t>(end - start));
    return text;
}

std::vector<std::string> exprs(const pugi::xml_node& parent, const char* child, const char* item)
{
    std::vector<std::string> list;
    for (const pugi::xml_node node : parent.child(child).children(item)) {
        if (const char* expr = node.attribute("expr").as_string(""); expr[0])
            list.emplace_back(expr);
    }
    return list;
}

} // namespace

FunctionParser::FunctionParser(std::string id, std::string displayName, std::string commentExpr, std::string functionExpr,
                               std::vector<std::string> functionNameExprs, std::vector<std::string> classNameExprs)
    : _id(std::move(id))
    , _displayName(std::move(displayName))
    , _commentExpr(std::move(commentExpr))
    , _functionExpr(std::move(functionExpr))
    , _functionNameExprs(std::move(functionNameExprs))
    , _classNameExprs(std::move(classNameExprs))
{
}

void FunctionParser::funcParse(ScintillaEditBase& view, std::vector<FunctionInfo>& found, sptr_t begin, sptr_t end,
                               const std::string& classStructName, const Zones* commentZones)
{
    if (begin >= end || _functionExpr.empty())
        return;
    view.send(SCI_SETSEARCHFLAGS, kSearchFlags);
    sptr_t targetStart = searchInTarget(view, _functionExpr, begin, end);
    while (targetStart >= 0) {
        targetStart = view.send(SCI_GETTARGETSTART);
        const sptr_t targetEnd = view.send(SCI_GETTARGETEND);
        if (targetEnd > end) // fuera del rango
            break;
        const sptr_t foundLength = targetEnd - targetStart;
        if (targetStart + foundLength == end) // como en N++
            break;
        FunctionInfo fi;
        if (_functionNameExprs.empty() && _classNameExprs.empty()) {
            fi.name = textRange(view, targetStart, targetEnd);
            fi.pos = targetStart;
        } else {
            sptr_t foundPos = -1;
            if (!_functionNameExprs.empty()) {
                fi.name = parseSubLevel(view, targetStart, targetEnd, _functionNameExprs, foundPos);
                fi.pos = foundPos;
            }
            if (!classStructName.empty()) {
                fi.className = classStructName;
                fi.classPos = -1;
            } else if (!_classNameExprs.empty()) {
                fi.className = parseSubLevel(view, targetStart, targetEnd, _classNameExprs, foundPos);
                fi.classPos = foundPos;
            }
        }
        if (fi.pos != -1 || fi.classPos != -1) {
            if (!commentZones || (!isInZones(fi.pos, *commentZones) && !isInZones(fi.classPos, *commentZones)))
                found.push_back(fi);
        }
        begin = targetStart + foundLength;
        targetStart = searchInTarget(view, _functionExpr, begin, end);
    }
}

std::string FunctionParser::parseSubLevel(ScintillaEditBase& view, sptr_t begin, sptr_t end, std::vector<std::string> list,
                                          sptr_t& foundPos)
{
    if (begin >= end) {
        foundPos = -1;
        return {};
    }
    if (list.empty())
        return {};
    view.send(SCI_SETSEARCHFLAGS, kSearchFlags);
    const sptr_t targetStart = searchInTarget(view, list.front(), begin, end);
    if (targetStart < 0) {
        foundPos = -1;
        return {};
    }
    const sptr_t targetEnd = view.send(SCI_GETTARGETEND);
    if (list.size() >= 2) {
        list.erase(list.begin());
        return parseSubLevel(view, targetStart, targetEnd, list, foundPos);
    }
    foundPos = targetStart;
    return textRange(view, targetStart, targetEnd);
}

void FunctionParser::getCommentZones(ScintillaEditBase& view, Zones& zones, sptr_t begin, sptr_t end)
{
    if (begin >= end || _commentExpr.empty())
        return;
    view.send(SCI_SETSEARCHFLAGS, kSearchFlags);
    sptr_t targetStart = searchInTarget(view, _commentExpr, begin, end);
    while (targetStart >= 0) {
        targetStart = view.send(SCI_GETTARGETSTART);
        const sptr_t targetEnd = view.send(SCI_GETTARGETEND);
        if (targetEnd > end)
            break;
        zones.emplace_back(targetStart, targetEnd);
        const sptr_t length = targetEnd - targetStart;
        if (targetStart + length == end)
            break;
        begin = targetStart + length;
        targetStart = searchInTarget(view, _commentExpr, begin, end);
    }
}

bool FunctionParser::isInZones(sptr_t pos, const Zones& zones)
{
    for (const auto& [first, second] : zones) {
        if (pos >= first && pos < second)
            return true;
    }
    return false;
}

void FunctionParser::getInvertZones(Zones& dest, const Zones& source, sptr_t begin, sptr_t end)
{
    if (source.empty()) {
        dest.emplace_back(begin, end);
        return;
    }
    if (begin < source.front().first)
        dest.emplace_back(begin, source.front().first - 1);
    std::size_t i = 0;
    for (; i + 1 < source.size(); ++i) {
        const sptr_t newBegin = source[i].second + 1;
        const sptr_t newEnd = source[i + 1].first - 1;
        if (newBegin < newEnd)
            dest.emplace_back(newBegin, newEnd);
    }
    const sptr_t lastBegin = source[i].second + 1;
    if (lastBegin < end)
        dest.emplace_back(lastBegin, end);
}

void FunctionUnitParser::parse(ScintillaEditBase& view, std::vector<FunctionInfo>& found, sptr_t begin, sptr_t end,
                               const std::string& classStructName)
{
    Zones commentZones, nonCommentZones;
    getCommentZones(view, commentZones, begin, end);
    getInvertZones(nonCommentZones, commentZones, begin, end);
    for (const auto& [first, second] : nonCommentZones)
        funcParse(view, found, first, second, classStructName);
}

FunctionZoneParser::FunctionZoneParser(std::string id, std::string displayName, std::string commentExpr,
                                       std::string rangeExpr, std::string openSymbol, std::string closeSymbol,
                                       std::vector<std::string> classNameExprs, std::string functionExpr,
                                       std::vector<std::string> functionNameExprs)
    : FunctionParser(std::move(id), std::move(displayName), std::move(commentExpr), std::move(functionExpr),
                     std::move(functionNameExprs), std::move(classNameExprs))
    , _rangeExpr(std::move(rangeExpr))
    , _openSymbol(std::move(openSymbol))
    , _closeSymbol(std::move(closeSymbol))
{
}

sptr_t FunctionZoneParser::bodyClosePos(ScintillaEditBase& view, sptr_t begin, const Zones& commentZones)
{
    // getBodyClosePos: cuenta aperturas y cierres fuera de los comentarios
    std::size_t open = 1;
    const sptr_t length = view.send(SCI_GETLENGTH);
    if (begin >= length)
        return length;
    const std::string either = "(" + _openSymbol + "|" + _closeSymbol + ")";
    view.send(SCI_SETSEARCHFLAGS, kSearchFlags);
    sptr_t targetStart = searchInTarget(view, either, begin, length);
    sptr_t targetEnd = 0;
    do {
        if (targetStart >= 0) {
            targetEnd = view.send(SCI_GETTARGETEND);
            if (!isInZones(targetStart, commentZones)) {
                if (searchInTarget(view, _openSymbol, targetStart, targetEnd) >= 0)
                    ++open;
                else
                    --open;
            }
        } else {
            open = 0;
            targetEnd = begin;
        }
        targetStart = searchInTarget(view, either, targetEnd, length);
    } while (open);
    return targetEnd;
}

void FunctionZoneParser::classParse(ScintillaEditBase& view, std::vector<FunctionInfo>& found, Zones& scannedZones,
                                    const Zones& commentZones, sptr_t begin, sptr_t end)
{
    if (begin >= end)
        return;
    view.send(SCI_SETSEARCHFLAGS, kSearchFlags);
    sptr_t targetStart = searchInTarget(view, _rangeExpr, begin, end);
    while (targetStart >= 0) {
        sptr_t targetEnd = view.send(SCI_GETTARGETEND);
        sptr_t foundPos = 0;
        const std::string className = parseSubLevel(view, targetStart, targetEnd, _classNameExprs, foundPos);
        if (!_openSymbol.empty() && !_closeSymbol.empty())
            targetEnd = bodyClosePos(view, targetEnd, commentZones);
        if (targetEnd > end)
            break;
        scannedZones.emplace_back(targetStart, targetEnd);
        const sptr_t length = targetEnd - targetStart;
        if (targetStart + length == end)
            break;
        if (!isInZones(targetStart, commentZones))
            funcParse(view, found, targetStart, targetEnd, className, &commentZones);
        begin = targetStart + length;
        targetStart = searchInTarget(view, _rangeExpr, begin, end);
    }
}

void FunctionZoneParser::parse(ScintillaEditBase& view, std::vector<FunctionInfo>& found, sptr_t begin, sptr_t end,
                               const std::string&)
{
    Zones classZones, commentZones, nonCommentZones;
    getCommentZones(view, commentZones, begin, end);
    getInvertZones(nonCommentZones, commentZones, begin, end);
    for (const auto& [first, second] : nonCommentZones)
        classParse(view, found, classZones, commentZones, first, second);
}

FunctionMixParser::FunctionMixParser(std::string id, std::string displayName, std::string commentExpr, std::string rangeExpr,
                                     std::string openSymbol, std::string closeSymbol, std::vector<std::string> classNameExprs,
                                     std::string functionExpr, std::vector<std::string> functionNameExprs,
                                     std::unique_ptr<FunctionUnitParser> unitParser)
    : FunctionZoneParser(std::move(id), std::move(displayName), std::move(commentExpr), std::move(rangeExpr),
                         std::move(openSymbol), std::move(closeSymbol), std::move(classNameExprs), std::move(functionExpr),
                         std::move(functionNameExprs))
    , _unitParser(std::move(unitParser))
{
}

void FunctionMixParser::parse(ScintillaEditBase& view, std::vector<FunctionInfo>& found, sptr_t begin, sptr_t end,
                              const std::string& classStructName)
{
    Zones commentZones, scannedZones, nonScannedZones;
    getCommentZones(view, commentZones, begin, end);
    classParse(view, found, scannedZones, commentZones, begin, end);
    // El segundo nivel (clases dentro de clases)
    for (const auto& [first, second] : Zones(scannedZones)) {
        Zones temp;
        classParse(view, found, temp, commentZones, first, second);
    }
    getInvertZones(nonScannedZones, scannedZones, begin, end);
    if (_unitParser) {
        for (const auto& [first, second] : nonScannedZones)
            _unitParser->funcParse(view, found, first, second, classStructName, &commentZones);
    }
}

// --- FunctionParsersManager -------------------------------------------------------------------

FunctionParsers::FunctionParsers(QStringList directories)
    : _directories(directories.isEmpty() ? defaultDirectories() : std::move(directories))
{
}

QStringList FunctionParsers::defaultDirectories()
{
    QStringList dirs{QDir(paths::configDir()).filePath(QStringLiteral("functionList"))};
    for (const QString& data : paths::dataDirs())
        dirs << QDir(data).filePath(QStringLiteral("functionList"));
    return dirs;
}

void FunctionParsers::loadOverrideMap()
{
    // getOverrideMapFromXmlTree: el primer overrideMap.xml que se pueda leer
    _mapLoaded = true;
    for (const QString& dir : _directories) {
        pugi::xml_document doc;
        if (!doc.load_file(QFile::encodeName(QDir(dir).filePath(QStringLiteral("overrideMap.xml"))).constData()))
            continue;
        const pugi::xml_node map = doc.child("NotepadPlus").child("functionList").child("associationMap");
        for (const pugi::xml_node node : map.children("association")) {
            const QString id = QString::fromUtf8(node.attribute("id").as_string(""));
            if (id.isEmpty())
                continue;
            const int langId = node.attribute("langID").as_int(-1);
            const QString udl = QString::fromUtf8(node.attribute("userDefinedLangName").as_string(""));
            if (langId >= 0)
                _byLang[langId].file = id;
            else if (!udl.isEmpty())
                _byUserLanguage[udl].file = id;
        }
        return;
    }
}

std::unique_ptr<FunctionParser> FunctionParsers::load(const QString& file) const
{
    // loadFuncListFromXmlTree, en la primera carpeta que lo tenga
    // Como loadFileFunctionParser de N++: los saltos de línea de los atributos se
    // conservan, porque las expresiones (?x) llevan comentarios "# …" hasta el final de
    // la línea (el XML normal los cambia por espacios)
    constexpr unsigned int kFlags = pugi::parse_default & ~pugi::parse_wconv_attribute;
    for (const QString& dir : _directories) {
        pugi::xml_document doc;
        if (!doc.load_file(QFile::encodeName(QDir(dir).filePath(file)).constData(), kFlags))
            continue;
        const pugi::xml_node parserRoot = doc.child("NotepadPlus").child("functionList").child("parser");
        const std::string id = parserRoot.attribute("id").as_string("");
        if (id.empty())
            return nullptr;
        const std::string displayName = parserRoot.attribute("displayName").as_string(id.c_str());
        const std::string commentExpr = parserRoot.attribute("commentExpr").as_string("");
        const pugi::xml_node range = parserRoot.child("classRange");
        const pugi::xml_node function = parserRoot.child("function");
        auto unit = [&]() -> std::unique_ptr<FunctionUnitParser> {
            if (!function || !function.attribute("mainExpr").as_string("")[0])
                return nullptr;
            return std::make_unique<FunctionUnitParser>(id, displayName, commentExpr, function.attribute("mainExpr").as_string(""),
                                                        exprs(function, "functionName", "nameExpr"),
                                                        exprs(function, "className", "nameExpr"));
        };
        if (range && range.attribute("mainExpr").as_string("")[0]) {
            const pugi::xml_node rangeFunction = range.child("function");
            auto zoneArgs = std::make_tuple(std::string(range.attribute("mainExpr").as_string("")),
                                            std::string(range.attribute("openSymbole").as_string("")),
                                            std::string(range.attribute("closeSymbole").as_string("")),
                                            exprs(range, "className", "nameExpr"),
                                            std::string(rangeFunction.attribute("mainExpr").as_string("")),
                                            exprs(rangeFunction, "functionName", "funcNameExpr"));
            if (function) {
                return std::make_unique<FunctionMixParser>(id, displayName, commentExpr, std::get<0>(zoneArgs), std::get<1>(zoneArgs),
                                                           std::get<2>(zoneArgs), std::get<3>(zoneArgs), std::get<4>(zoneArgs),
                                                           std::get<5>(zoneArgs), unit());
            }
            return std::make_unique<FunctionZoneParser>(id, displayName, commentExpr, std::get<0>(zoneArgs), std::get<1>(zoneArgs),
                                                        std::get<2>(zoneArgs), std::get<3>(zoneArgs), std::get<4>(zoneArgs),
                                                        std::get<5>(zoneArgs));
        }
        return unit();
    }
    return nullptr;
}

FunctionParser* FunctionParsers::parser(LangType lang, const QString& userLanguage)
{
    if (!_mapLoaded)
        loadOverrideMap();
    Entry* entry = nullptr;
    if (lang == LangType::User) {
        const auto it = _byUserLanguage.find(userLanguage);
        if (it == _byUserLanguage.end())
            return nullptr;
        entry = &it->second;
    } else {
        entry = &_byLang[static_cast<int>(lang)];
        if (entry->file.isEmpty())
            entry->file = QString::fromUtf8(languageInfo(lang).name) + QStringLiteral(".xml");
    }
    if (!entry->tried) {
        entry->tried = true;
        entry->parser = load(entry->file);
    }
    return entry->parser.get();
}

bool FunctionParsers::parse(ScintillaEditBase& view, LangType lang, const QString& userLanguage, std::vector<FunctionInfo>& found)
{
    FunctionParser* p = parser(lang, userLanguage);
    if (!p)
        return false;
    p->parse(view, found, 0, view.send(SCI_GETLENGTH));
    return true;
}

} // namespace edita
