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

#include "core/LangType.h"

#include <ScintillaEditBase.h>

#include <QString>
#include <QStringList>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace edita {

// Una función encontrada (foundInfo de N++): nombre y posición; clase, si la hay
struct FunctionInfo
{
    std::string name;
    sptr_t pos = -1;
    std::string className;
    sptr_t classPos = -1;
};

using Zones = std::vector<std::pair<sptr_t, sptr_t>>;

// Port de functionParser.cpp de N++. Los analizadores buscan con las expresiones
// regulares de functionList/*.xml en el documento de una vista de Scintilla
// (SCI_SEARCHINTARGET con la regex de Boost, como en N++).
class FunctionParser
{
public:
    FunctionParser(std::string id, std::string displayName, std::string commentExpr, std::string functionExpr,
                   std::vector<std::string> functionNameExprs, std::vector<std::string> classNameExprs);
    virtual ~FunctionParser() = default;

    virtual void parse(ScintillaEditBase& view, std::vector<FunctionInfo>& found, sptr_t begin, sptr_t end,
                       const std::string& classStructName = {}) = 0;

    const std::string& id() const { return _id; }
    const std::string& displayName() const { return _displayName; }

    void funcParse(ScintillaEditBase& view, std::vector<FunctionInfo>& found, sptr_t begin, sptr_t end,
                   const std::string& classStructName = {}, const Zones* commentZones = nullptr);

protected:
    std::string parseSubLevel(ScintillaEditBase& view, sptr_t begin, sptr_t end, std::vector<std::string> exprs,
                              sptr_t& foundPos);
    void getCommentZones(ScintillaEditBase& view, Zones& zones, sptr_t begin, sptr_t end);
    static bool isInZones(sptr_t pos, const Zones& zones);
    static void getInvertZones(Zones& dest, const Zones& source, sptr_t begin, sptr_t end);

    std::string _id;
    std::string _displayName;
    std::string _commentExpr;
    std::string _functionExpr;
    std::vector<std::string> _functionNameExprs;
    std::vector<std::string> _classNameExprs;
};

// <function> sin clases
class FunctionUnitParser : public FunctionParser
{
public:
    using FunctionParser::FunctionParser;
    void parse(ScintillaEditBase& view, std::vector<FunctionInfo>& found, sptr_t begin, sptr_t end,
               const std::string& classStructName = {}) override;
};

// <classRange>: zonas de clase (con sus símbolos de apertura y cierre) y las funciones de dentro
class FunctionZoneParser : public FunctionParser
{
public:
    FunctionZoneParser(std::string id, std::string displayName, std::string commentExpr, std::string rangeExpr,
                       std::string openSymbol, std::string closeSymbol, std::vector<std::string> classNameExprs,
                       std::string functionExpr, std::vector<std::string> functionNameExprs);
    void parse(ScintillaEditBase& view, std::vector<FunctionInfo>& found, sptr_t begin, sptr_t end,
               const std::string& classStructName = {}) override;

protected:
    void classParse(ScintillaEditBase& view, std::vector<FunctionInfo>& found, Zones& scannedZones,
                    const Zones& commentZones, sptr_t begin, sptr_t end);
    sptr_t bodyClosePos(ScintillaEditBase& view, sptr_t begin, const Zones& commentZones);

    std::string _rangeExpr;
    std::string _openSymbol;
    std::string _closeSymbol;
};

// <classRange> + <function>: las clases y, fuera de ellas, las funciones sueltas
class FunctionMixParser : public FunctionZoneParser
{
public:
    FunctionMixParser(std::string id, std::string displayName, std::string commentExpr, std::string rangeExpr,
                      std::string openSymbol, std::string closeSymbol, std::vector<std::string> classNameExprs,
                      std::string functionExpr, std::vector<std::string> functionNameExprs,
                      std::unique_ptr<FunctionUnitParser> unitParser);
    void parse(ScintillaEditBase& view, std::vector<FunctionInfo>& found, sptr_t begin, sptr_t end,
               const std::string& classStructName = {}) override;

private:
    std::unique_ptr<FunctionUnitParser> _unitParser;
};

// FunctionParsersManager: qué analizador va con cada lenguaje (overrideMap.xml; si
// no lo dice, <nombre del lenguaje>.xml) y se carga la primera vez que hace falta.
// Las carpetas se miran en orden (la del usuario primero).
class FunctionParsers
{
public:
    explicit FunctionParsers(QStringList directories = {});

    FunctionParser* parser(LangType lang, const QString& userLanguage = {});
    // El documento entero; falso si el lenguaje no tiene analizador
    bool parse(ScintillaEditBase& view, LangType lang, const QString& userLanguage, std::vector<FunctionInfo>& found);

    // functionList/ en los datos de la aplicación y en ~/.config/edita
    static QStringList defaultDirectories();

private:
    struct Entry
    {
        QString file;
        bool tried = false;
        std::unique_ptr<FunctionParser> parser;
    };
    void loadOverrideMap();
    std::unique_ptr<FunctionParser> load(const QString& file) const;

    QStringList _directories;
    bool _mapLoaded = false;
    std::map<int, Entry> _byLang;
    std::map<QString, Entry> _byUserLanguage;
};

} // namespace edita
