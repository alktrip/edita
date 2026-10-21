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


#include "ui/AutoCompletion.h"

#include "ui/EditorView.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <SciLexer.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace edita {

namespace {

// Iconos de la lista: función y palabra clave (los de N++, tema claro)
const char* const kXpmFunction[] = {
	/* columns rows colors chars-per-pixel */
	"16 16 36 1 ",
	"u c None",
	"  c #131313",
	". c #252525",
	"X c #161616",
	"o c #202020",
	"O c #393939",
	"+ c #242424",
	"@ c #282828",
	"# c #4E4E4E",
	"$ c #343434",
	"% c #5B5B5B",
	"& c #5F5F5F",
	"* c #626262",
	"= c #404040",
	"- c #686868",
	"; c #434343",
	": c #464646",
	"> c #484848",
	", c #494949",
	"< c #515151",
	"1 c #929292",
	"2 c #9B9B9B",
	"3 c #636363",
	"4 c #656565",
	"5 c #AFAFAF",
	"6 c #B7B7B7",
	"7 c #757575",
	"8 c #CDCDCD",
	"9 c #858585",
	"0 c #868686",
	"q c #DDDDDD",
	"w c #E1E1E1",
	"e c #E9E9E9",
	"r c #EEEEEE",
	"t c #959595",
	"y c #F6F6F6",
	/* pixels */
	"uuuuuuuuuuuuuuuu",
	"uuuuu5o.:uuuuuuu",
	"uuuu8 $:.0uuuuuu",
	"uuuu2 uuuuuuuuuu",
	"uuu6$ 46uuuuuuuu",
	"uuuO   Ouuuuuuuu",
	"uuuu;#uuuuuuuuuu",
	"uuuu##u& 3uu<+uu",
	"uuuu#;0.@X0, >uu",
	"uuuu+>uuuoo >uuu",
	"uuuu >uuu* =uuuu",
	"uuuu 2uu, Xotuuu",
	"uuu# 4u< >9 %ouu",
	"u:,#X0uO>uu1 $ u",
	"u- +7uuuuuuuuuuu",
	"uuuuuuuuuuuuuuuu"
};

const char* const kXpmKeyword[] = {
	/* columns rows colors chars-per-pixel */
	"16 16 33 1 ",
	"r c None",
	"  c #000000",
	". c #030303",
	"X c #101010",
	"o c #181818",
	"O c #202020",
	"+ c #282828",
	"@ c #191919",
	"# c #222222",
	"$ c #252525",
	"% c #484848",
	"& c #505050",
	"* c #606060",
	"= c #444444",
	"- c #474747",
	"; c #505050",
	": c #535353",
	"> c #565656",
	", c #979797",
	"< c #9A9A9A",
	"1 c #9F9F9F",
	"2 c #A7A7A7",
	"3 c #AFAFAF",
	"4 c #B7B7B7",
	"5 c #757575",
	"6 c #767676",
	"7 c #787878",
	"8 c #818181",
	"9 c #D7D7D7",
	"0 c #DFDFDF",
	"q c #E7E7E7",
	"w c #EFEFEF",
	"e c #979797",
	/* pixels */
	"rrrrrrrrrrrrrrrr",
	"rrrrrrrrrrrrrrrr",
	"rrre4;$  *rrrrrr",
	"r4@ $-rr6;X rrrr",
	"r1oX>rrrrr5+  rr",
	"r1*r%OrrrXO*&;rr",
	"r1*rrr==%rrr1;rr",
	"r1*rrrr$rrrr1;rr",
	"r1*rrrr$rrrr1;rr",
	"r1*rrrr$rrrr1;rr",
	"r1*rrrr$rrrr1;rr",
	"r3o<rrr$rrr8$;rr",
	"rr7#%rr$rr #;rrr",
	"rrrr,o.$.%:rrrrr",
	"rrrrrr7*7rrrrrrr",
	"rrrrrrrrrrrrrrrr"
	};


bool isAllDigits(const std::string& s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
}

bool lessNoCase(const std::string& a, const std::string& b)
{
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
        return std::toupper(static_cast<unsigned char>(x)) < std::toupper(static_cast<unsigned char>(y));
    });
}

bool startsWith(const std::string& word, const std::string& prefix, bool ignoreCase)
{
    if (word.size() < prefix.size())
        return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        const auto a = static_cast<unsigned char>(word[i]);
        const auto b = static_cast<unsigned char>(prefix[i]);
        if (ignoreCase ? std::tolower(a) != std::tolower(b) : a != b)
            return false;
    }
    return true;
}

bool contains(const std::vector<std::string>& words, const std::string& word)
{
    return std::find(words.begin(), words.end(), word) != words.end();
}

// Escapa para una expresión regular (el principio de la palabra puede tener "$", ".", …)
std::string escapeRegex(const std::string& s)
{
    std::string out;
    for (const char c : s) {
        if (std::strchr("\\^$.|?*+()[]{}", c))
            out += '\\';
        out += c;
    }
    return out;
}

} // namespace

AutoCompletion::AutoCompletion(EditorView& view, const Preferences& prefs)
    : _view(view)
    , _prefs(prefs)
{
}

AutoCompletion::~AutoCompletion() = default;

std::string AutoCompletion::text(sptr_t start, sptr_t end) const
{
    if (end <= start)
        return {};
    std::string buffer(static_cast<std::size_t>(end - start) + 1, '\0');
    Sci_TextRangeFull range{{start, end}, buffer.data()};
    _view.sci(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&range));
    buffer.resize(static_cast<std::size_t>(end - start));
    return buffer;
}

bool AutoCompletion::setLanguage(LangType lang)
{
    if (_loaded && lang == _lang)
        return hasApi();
    _lang = lang;
    _loaded = true;
    closeCallTip();
    _function = nullptr;
    _functionName.clear();
    loadApiFile(findApiFile(lang), _api);

    // Lista de palabras clave con su icono, ordenada como la compara Scintilla
    _keywordArray.clear();
    _keywords.clear();
    _keywordMaxLen = 0;
    for (const ApiKeyword& keyword : _api.keywords) {
        _keywordArray.push_back(keyword.name + kTypeSeparator
                                + std::to_string(keyword.isFunction ? kFunctionImage : kKeywordImage));
        _keywordMaxLen = std::max(_keywordMaxLen, keyword.name.size());
    }
    if (_api.ignoreCase)
        std::stable_sort(_keywordArray.begin(), _keywordArray.end(), lessNoCase);
    else
        std::sort(_keywordArray.begin(), _keywordArray.end());
    for (const std::string& word : _keywordArray) {
        _keywords += word;
        _keywords += ' ';
    }
    return hasApi();
}

void AutoCompletion::showList(sptr_t enteredLength, const std::string& list)
{
    if (!_imagesRegistered) {
        _view.sci(SCI_CLEARREGISTEREDIMAGES);
        _view.sci(SCI_REGISTERIMAGE, kFunctionImage, reinterpret_cast<sptr_t>(kXpmFunction));
        _view.sci(SCI_REGISTERIMAGE, kKeywordImage, reinterpret_cast<sptr_t>(kXpmKeyword));
        _imagesRegistered = true;
    }
    const bool ignoreCase = hasApi() ? _api.ignoreCase : false;
    _view.sci(SCI_AUTOCSETTYPESEPARATOR, static_cast<uptr_t>(kTypeSeparator));
    _view.sci(SCI_AUTOCSETSEPARATOR, ' ');
    _view.sci(SCI_AUTOCSETIGNORECASE, ignoreCase ? 1 : 0);
    _view.sci(SCI_AUTOCSETCASEINSENSITIVEBEHAVIOUR, ignoreCase ? SC_CASEINSENSITIVEBEHAVIOUR_IGNORECASE
                                                               : SC_CASEINSENSITIVEBEHAVIOUR_RESPECTCASE);
    _view.sends(SCI_AUTOCSHOW, static_cast<uptr_t>(enteredLength), list.c_str());
}

bool AutoCompletion::showAutoComplete(Type type, bool autoInsert)
{
    if (type == Type::Func && !hasApi())
        return false;
    const sptr_t curPos = _view.sci(SCI_GETCURRENTPOS);
    const sptr_t startPos = _view.sci(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(curPos), 1);
    if (curPos == startPos)
        return false;
    const auto len = static_cast<std::size_t>(curPos - startPos);

    if (type == Type::Func) {
        if (len >= _keywordMaxLen)
            return false;
        showList(curPos - startPos, _keywords);
        return true;
    }

    constexpr std::size_t kBufSize = 256;
    const sptr_t endPos = _view.sci(SCI_WORDENDPOSITION, static_cast<uptr_t>(curPos), 1);
    if (len >= kBufSize || static_cast<std::size_t>(endPos - startPos) >= kBufSize)
        return false;
    const std::string begin = text(startPos, curPos);

    std::vector<std::string> words;
    if (type == Type::Word || type == Type::FuncAndWord)
        getWordArray(words, begin, text(startPos, endPos));

    if (type == Type::FuncBrief || type == Type::FuncAndWord) {
        for (const std::string& keyword : _keywordArray) {
            if (startsWith(keyword, begin, _api.ignoreCase) && !contains(words, keyword))
                words.push_back(keyword);
        }
    }
    if (words.empty())
        return false;

    // Una sola palabra: se escribe directamente
    if (type == Type::Word && words.size() == 1 && autoInsert) {
        const std::string word = words.front().substr(0, words.front().find(kTypeSeparator));
        _view.sci(SCI_SETTARGETRANGE, static_cast<uptr_t>(startPos), curPos);
        const sptr_t replaced = _view.sends(SCI_REPLACETARGET, word.size(), word.c_str());
        _view.sci(SCI_GOTOPOS, static_cast<uptr_t>(startPos + replaced));
        return true;
    }

    if (hasApi() && _api.ignoreCase)
        std::stable_sort(words.begin(), words.end(), lessNoCase);
    else
        std::sort(words.begin(), words.end());
    std::string list;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i)
            list += ' ';
        list += words[i];
    }
    showList(curPos - startPos, list);
    return true;
}

void AutoCompletion::getWordArray(std::vector<std::string>& words, const std::string& begin,
                                  const std::string& exclude) const
{
    if (_prefs.autocIgnoreNumbers && isAllDigits(begin))
        return;
    const std::string expr = "\\<" + escapeRegex(begin) + "[^ \\t\\n\\r.,;:\"(){}=<>'+!?\\[\\]]+";
    int flags = SCFIND_WORDSTART | SCFIND_REGEXP | SCFIND_POSIX;
    if (!hasApi() || !_api.ignoreCase)
        flags |= SCFIND_MATCHCASE;
    _view.sci(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(flags));

    const std::string boxId = kTypeSeparator + std::to_string(kKeywordImage);
    const std::string funcId = kTypeSeparator + std::to_string(kFunctionImage);
    const sptr_t docLength = _view.sci(SCI_GETLENGTH);
    sptr_t pos = 0;
    while (pos < docLength) {
        _view.sci(SCI_SETTARGETRANGE, static_cast<uptr_t>(pos), docLength);
        if (_view.sends(SCI_SEARCHINTARGET, expr.size(), expr.c_str()) < 0)
            break;
        const sptr_t wordStart = _view.sci(SCI_GETTARGETSTART);
        const sptr_t wordEnd = _view.sci(SCI_GETTARGETEND);
        if (wordEnd - wordStart < 256) {
            const std::string w = text(wordStart, wordEnd);
            if (w != exclude) {
                // Si además es una palabra de la API, con su icono
                std::string entry = w;
                if (contains(_keywordArray, w + boxId))
                    entry = w + boxId;
                else if (contains(_keywordArray, w + funcId))
                    entry = w + funcId;
                if (!contains(words, entry))
                    words.push_back(entry);
            }
        }
        pos = wordEnd > wordStart ? wordEnd : wordStart + 1;
    }
}

bool AutoCompletion::showApiComplete()
{
    return showAutoComplete(Type::Func, false);
}

bool AutoCompletion::showWordComplete(bool autoInsert)
{
    return showAutoComplete(Type::Word, autoInsert);
}

bool AutoCompletion::showApiAndWordComplete()
{
    return showAutoComplete(Type::FuncAndWord, false);
}

bool AutoCompletion::showFunctionComplete()
{
    return hasApi() && updateCallTip(0, true);
}

void AutoCompletion::showPathCompletion()
{
    // Lo escrito antes del cursor (como mucho una ruta larga)
    constexpr sptr_t kMaxPath = 4096;
    const sptr_t curPos = _view.sci(SCI_GETCURRENTPOS);
    const std::string before = text(std::max<sptr_t>(0, curPos - kMaxPath), curPos);

    // En Linux la ruta empieza por "/", "~/" o "./" y va precedida de un espacio,
    // una comilla o un paréntesis (en N++, "C:" con las mismas reglas)
    std::size_t start = before.size();
    while (start > 0) {
        const char c = before[start - 1];
        if (c == '\'' || c == '"' || c == '(' || std::isspace(static_cast<unsigned char>(c)))
            break;
        --start;
    }
    const std::string rawPath = before.substr(start);
    if (rawPath.empty() || (rawPath[0] != '/' && rawPath[0] != '~' && rawPath[0] != '.')
        || rawPath.find('/') == std::string::npos)
        return;
    QString path = QString::fromStdString(rawPath);
    if (path.startsWith(QLatin1Char('~')))
        path.replace(0, 1, QDir::homePath());
    if (QFileInfo(path).isFile())
        return;
    // "/usr/lo" → se listan las entradas de "/usr/"
    const QString dirPath = QFileInfo(path).isDir() && path.endsWith(QLatin1Char('/'))
        ? path : path.left(path.lastIndexOf(QLatin1Char('/')) + 1);
    const QString shownDir = QString::fromStdString(rawPath.substr(0, rawPath.rfind('/') + 1));
    const QDir dir(dirPath);
    if (!dir.exists())
        return;
    constexpr int kMaxEntries = 2000;
    std::string entries;
    const QFileInfoList list = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                                                 QDir::Name | QDir::IgnoreCase);
    for (int i = 0; i < list.size() && i < kMaxEntries; ++i) {
        if (!entries.empty())
            entries += '\n';
        entries += (shownDir + list[i].fileName() + (list[i].isDir() ? QStringLiteral("/") : QString())).toStdString();
    }
    if (entries.empty())
        return;
    _view.sci(SCI_AUTOCSETSEPARATOR, '\n');
    _view.sci(SCI_AUTOCSETIGNORECASE, 1);
    _view.sci(SCI_AUTOCSETCASEINSENSITIVEBEHAVIOUR, SC_CASEINSENSITIVEBEHAVIOUR_IGNORECASE);
    _view.sends(SCI_AUTOCSHOW, rawPath.size(), entries.c_str());
}

void AutoCompletion::update(int ch)
{
    // AutoCompletion::update
    if (!ch || _prefs.autocStatus == 0)
        return;
    if (!hasApi() && _prefs.autocStatus == 1)
        return;
    if (_prefs.funcParams || isCallTipVisible()) {
        if (updateCallTip(ch, false))
            return;
    }
    if (!_prefs.autocBrief && _view.sci(SCI_AUTOCACTIVE))
        return;

    const sptr_t curPos = _view.sci(SCI_GETCURRENTPOS);
    const sptr_t startPos = _view.sci(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(curPos), 1);
    if (curPos - startPos < _prefs.autocFromLen || curPos - startPos >= 64)
        return;
    switch (_prefs.autocStatus) {
    case 2:
        showWordComplete(false);
        break;
    case 1:
        if (_prefs.autocBrief)
            showAutoComplete(Type::FuncBrief, false);
        else
            showApiComplete();
        break;
    case 3:
        showApiAndWordComplete();
        break;
    default:
        break;
    }
}

// --- Ayuda de parámetros (FunctionCallTip) ---------------------------------------------

bool AutoCompletion::isWordChar(char c) const
{
    const auto u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '_' || u >= 0x80
        || (!_api.additionalWordChars.empty() && _api.additionalWordChars.find(c) != std::string::npos);
}

bool AutoCompletion::isCallTipVisible() const
{
    return _view.sci(SCI_CALLTIPACTIVE) != 0;
}

void AutoCompletion::closeCallTip()
{
    if (!isCallTipVisible() || !_callTipActive)
        return;
    _view.sci(SCI_CALLTIPCANCEL);
    _callTipActive = false;
    _currentOverload = 0;
}

void AutoCompletion::resetCallTip()
{
    _currentOverload = 0;
    _currentParam = 0;
    _callTipPos = 0;
}

bool AutoCompletion::updateCallTip(int ch, bool needShown)
{
    if (!hasApi())
        return false;
    if (!needShown && ch != _api.startFunc && ch != _api.paramSeparator && !isCallTipVisible())
        return false;
    _curPos = _view.sci(SCI_GETCURRENTPOS);
    if (!getCursorFunction()) {
        closeCallTip();
        return false;
    }
    showCallTip();
    return true;
}

void AutoCompletion::callTipClick(int direction)
{
    if (!hasApi() || !isCallTipVisible() || !_function || _function->overloads.empty())
        return;
    const std::size_t count = _function->overloads.size();
    if (direction == 1)
        _currentOverload = _currentOverload > 0 ? _currentOverload - 1 : count - 1;
    else if (direction == 2)
        _currentOverload = (_currentOverload + 1) % count;
    showCallTip();
}

bool AutoCompletion::getCursorFunction()
{
    // FunctionCallTip::getCursorFunction: se analiza la línea hasta el cursor
    const sptr_t line = _view.sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(_curPos));
    const sptr_t lineStart = _view.sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
    const sptr_t offset = _curPos - lineStart;
    constexpr sptr_t kMaxLen = 256;
    if (offset < 2 || offset >= kMaxLen) {
        resetCallTip();
        return false;
    }
    const std::string lineData = text(lineStart, _curPos);

    struct Token
    {
        std::string text;
        bool isIdentifier;
    };
    std::vector<Token> tokens;
    for (std::size_t i = 0; i < lineData.size(); ++i) {
        const char c = lineData[i];
        if (isWordChar(c)) {
            std::size_t end = i;
            while (end < lineData.size() && isWordChar(lineData[end]))
                ++end;
            tokens.push_back({lineData.substr(i, end - i), true});
            i = end - 1;
        } else if (c == '"' || c == '\'') {
            // Lo de dentro de una cadena no cuenta como parámetros ni paréntesis
            std::size_t end = i + 1;
            while (end < lineData.size() && lineData[end] != c) {
                if (lineData[end] == '\\')
                    ++end;
                ++end;
            }
            i = end;
        } else if (!std::isspace(static_cast<unsigned char>(c))) {
            tokens.push_back({std::string(1, c), false});
        }
    }

    struct Values
    {
        int lastIdentifier = -1;
        int lastFunctionIdentifier = -1;
        int param = 0;
    };
    std::vector<Values> stack;
    Values current;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens[i];
        const int index = static_cast<int>(i);
        if (token.isIdentifier) {
            current.lastIdentifier = index;
            continue;
        }
        const char c = token.text[0];
        if (c == _api.startFunc) {
            stack.push_back(current);
            // El identificador tiene que ir justo antes del "(": si no, es una expresión
            if (i > 0 && current.lastIdentifier == index - 1) {
                current.lastFunctionIdentifier = current.lastIdentifier;
                current.param = 0;
            } else {
                current.lastFunctionIdentifier = -1;
            }
        } else if (c == _api.paramSeparator && current.lastFunctionIdentifier > -1) {
            ++current.param;
        } else if (c == _api.stopFunc) {
            if (!stack.empty()) {
                current = stack.back();
                stack.pop_back();
            } else {
                current = Values{};
            }
        } else if (c == _api.terminal) {
            stack.clear();
            current = Values{};
        }
    }
    while (current.lastFunctionIdentifier == -1 && !stack.empty()) {
        current = stack.back();
        stack.pop_back();
    }
    if (current.lastFunctionIdentifier < 0)
        return false;

    const std::string& name = tokens[static_cast<std::size_t>(current.lastFunctionIdentifier)].text;
    _currentParam = static_cast<std::size_t>(current.param);
    const bool same = _function && name.size() == _functionName.size()
                   && startsWith(name, _functionName, _api.ignoreCase);
    if (same)
        return true;
    _functionName = name;
    return loadFunction();
}

bool AutoCompletion::loadFunction()
{
    resetCallTip();
    _function = nullptr;
    const ApiKeyword* keyword = _api.find(_functionName);
    if (!keyword || !keyword->isFunction || keyword->overloads.empty())
        return false;
    _function = keyword;
    return true;
}

void AutoCompletion::showCallTip()
{
    if (!_function || _function->overloads.empty())
        return;
    // Si el parámetro actual no cabe en la sobrecarga, otra con más parámetros
    const auto& overloads = _function->overloads;
    if (_currentParam >= overloads[_currentOverload].params.size() + 1) {
        for (std::size_t i = 0; i < overloads.size(); ++i) {
            if (_currentParam < overloads[i].params.size() + 1) {
                _currentOverload = i;
                break;
            }
        }
    }
    const ApiOverload& overload = overloads[_currentOverload];
    std::ostringstream tip;
    if (overloads.size() > 1)
        tip << "\001"
            << QCoreApplication::translate("AutoCompletion", "%1 of %2")
                   .arg(_currentOverload + 1)
                   .arg(overloads.size())
                   .toStdString()
            << "\002";
    tip << overload.retVal << ' ' << _function->name << ' ' << _api.startFunc;
    std::size_t highlightStart = 0;
    std::size_t highlightEnd = 0;
    for (std::size_t i = 0; i < overload.params.size(); ++i) {
        if (i == _currentParam) {
            highlightStart = tip.str().size();
            highlightEnd = highlightStart + overload.params[i].size();
        }
        tip << overload.params[i];
        if (i + 1 < overload.params.size())
            tip << _api.paramSeparator << ' ';
    }
    tip << _api.stopFunc;
    if (!overload.description.empty())
        tip << '\n' << overload.description;

    if (isCallTipVisible())
        _view.sci(SCI_CALLTIPCANCEL);
    else
        _callTipPos = _curPos;
    const std::string shown = tip.str();
    _view.sends(SCI_CALLTIPSHOW, static_cast<uptr_t>(_callTipPos), shown.c_str());
    _callTipActive = true;
    if (highlightStart != highlightEnd)
        _view.sci(SCI_CALLTIPSETHLT, highlightStart, static_cast<sptr_t>(highlightEnd));
}

// --- Cierre automático (insertMatchedChars) --------------------------------------------

void AutoCompletion::removeInvalidMatched(char c, sptr_t pos)
{
    if (c == '\n' || c == '\r') {
        _matched.clear();
        return;
    }
    const sptr_t line = _view.sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos));
    std::erase_if(_matched, [&](const MatchedChar& m) {
        return m.pos >= pos || _view.sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(m.pos)) != line;
    });
}

void AutoCompletion::addMatched(char c, sptr_t pos)
{
    removeInvalidMatched(c, pos);
    _matched.push_back({c, pos});
}

sptr_t AutoCompletion::searchMatched(char start, char end, sptr_t pos)
{
    // InsertedMatchedChars::search: si justo detrás del cursor (salvo espacios) está
    // el cierre que se insertó, se devuelve su posición para "saltarlo"
    const sptr_t line = _view.sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos));
    for (auto i = static_cast<int>(_matched.size()) - 1; i >= 0; --i) {
        const MatchedChar m = _matched[static_cast<std::size_t>(i)];
        if (m.c != start)
            continue;
        _matched.erase(_matched.begin() + i);
        if (m.pos >= pos || _view.sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(m.pos)) != line)
            continue;
        const sptr_t lineEnd = _view.sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line));
        for (sptr_t j = pos; j <= lineEnd; ++j) {
            const auto c = static_cast<char>(_view.sci(SCI_GETCHARAT, static_cast<uptr_t>(j)));
            if (c != ' ')
                return c == end ? j : -1;
        }
    }
    return -1;
}

std::string AutoCompletion::closeTag(sptr_t caret, bool isHtml) const
{
    // AutoCompletion::getCloseTag
    if (isHtml && _view.sci(SCI_GETSTYLEAT, static_cast<uptr_t>(caret)) >= SCE_HJ_START)
        return {};
    const auto prev = static_cast<char>(_view.sci(SCI_GETCHARAT, static_cast<uptr_t>(caret - 2)));
    const auto prevprev = static_cast<char>(_view.sci(SCI_GETCHARAT, static_cast<uptr_t>(caret - 3)));
    if ((prevprev == '-' && prev == '-') || prev == '/')
        return {};
    _view.sci(SCI_SETSEARCHFLAGS, SCFIND_REGEXP | SCFIND_POSIX);
    _view.sci(SCI_SETTARGETRANGE, static_cast<uptr_t>(caret), 0); // hacia atrás
    const char expr[] = "<[^\\s>]*";
    const sptr_t start = _view.sends(SCI_SEARCHINTARGET, std::strlen(expr), expr);
    if (start < 0)
        return {};
    const sptr_t end = _view.sci(SCI_GETTARGETEND);
    if (end - start < 2 || end - start > 250)
        return {};
    const std::string head = text(start, end);
    if (head[1] == '/' || head[1] == '?' || head.starts_with("<!--"))
        return {};
    if (isHtml) {
        // Elementos vacíos de HTML
        static constexpr const char* kVoid[] = {"area", "base", "br", "col", "embed", "hr", "img", "input", "keygen",
                                                "link", "meta", "param", "source", "track", "wbr", "!doctype"};
        for (const char* tag : kVoid) {
            if (startsWith(head.substr(1), tag, true))
                return {};
        }
    }
    return "</" + head.substr(1) + ">";
}

void AutoCompletion::insertMatchedChars(int character)
{
    const sptr_t caret = _view.sci(SCI_GETCURRENTPOS);
    if (character > 127 || character < 0) {
        if (!_matched.empty())
            removeInvalidMatched('\0', caret - 1);
        return;
    }
    const auto ch = static_cast<char>(character);
    const auto charPrev = static_cast<char>(_view.sci(SCI_GETCHARAT, static_cast<uptr_t>(caret - 2)));
    const auto charNext = static_cast<char>(_view.sci(SCI_GETCHARAT, static_cast<uptr_t>(caret)));
    const bool prevBlank = charPrev == ' ' || charPrev == '\t' || charPrev == '\n' || charPrev == '\r' || charPrev == '\0';
    const bool nextBlank = charNext == ' ' || charNext == '\t' || charNext == '\n' || charNext == '\r'
                        || caret == _view.sci(SCI_GETLENGTH);
    const bool nextClose = charNext == ')' || charNext == ']' || charNext == '}';
    const bool sandwich = (charPrev == '(' && charNext == ')') || (charPrev == '[' && charNext == ']')
                       || (charPrev == '{' && charNext == '}');

    std::string matched;
    auto openPair = [&](bool enabled, const char* close) {
        if (enabled && (nextBlank || nextClose)) {
            matched = close;
            addMatched(ch, caret - 1);
        }
    };
    auto quote = [&](bool enabled, const char* close) {
        if (!enabled)
            return false;
        if (!_matched.empty()) {
            // Escribir la comilla de cierre que se insertó la salta
            if (const sptr_t pos = searchMatched(ch, ch, caret); pos != -1) {
                _view.sci(SCI_DELETERANGE, static_cast<uptr_t>(pos), 1);
                _view.sci(SCI_GOTOPOS, static_cast<uptr_t>(pos));
                return true;
            }
        }
        if ((prevBlank && nextBlank) || sandwich || (charPrev == '(' && nextBlank) || (prevBlank && charNext == ')')
            || (charPrev == '[' && nextBlank) || (prevBlank && charNext == ']') || (charPrev == '{' && nextBlank)
            || (prevBlank && charNext == '}')) {
            matched = close;
            addMatched(ch, caret - 1);
        }
        return false;
    };

    switch (ch) {
    case '(':
        openPair(_prefs.insertParentheses, ")");
        break;
    case '[':
        openPair(_prefs.insertBrackets, "]");
        break;
    case '{':
        openPair(_prefs.insertCurlyBrackets, "}");
        break;
    case '"':
        if (quote(_prefs.insertDoubleQuotes, "\""))
            return;
        break;
    case '\'':
        if (quote(_prefs.insertQuotes, "'"))
            return;
        break;
    case '>':
        if (_prefs.insertHtmlXmlTag && (_lang == LangType::Html || _lang == LangType::Xml))
            matched = closeTag(caret, _lang == LangType::Html);
        break;
    case ')':
    case ']':
    case '}': {
        if (_matched.empty())
            break;
        const bool enabled = ch == ')' ? _prefs.insertParentheses : ch == ']' ? _prefs.insertBrackets : _prefs.insertCurlyBrackets;
        if (!enabled)
            return;
        const char start = ch == ')' ? '(' : ch == ']' ? '[' : '{';
        if (const sptr_t pos = searchMatched(start, ch, caret); pos != -1) {
            _view.sci(SCI_DELETERANGE, static_cast<uptr_t>(pos), 1);
            _view.sci(SCI_GOTOPOS, static_cast<uptr_t>(pos));
        }
        return;
    }
    default:
        if (!_matched.empty())
            removeInvalidMatched(ch, caret - 1);
        break;
    }
    if (!matched.empty())
        _view.sends(SCI_INSERTTEXT, static_cast<uptr_t>(caret), matched.c_str());
}

} // namespace edita
