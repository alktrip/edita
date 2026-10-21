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

#include "core/Encoding.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace edita {

namespace {

struct CodepageInfo
{
    int codepage;
    const char* aliases;     // EncodingMapper.cpp: lista separada por espacios
    const char* iconvName;   // nullptr = no disponible en glibc
    const char* displayName; // texto del menú Codificación (Notepad_plus.rc)
};

// Mismo orden y alias que encodings[] en EncodingMapper.cpp (sin las entradas
// vacías de ISO-8859-10/11/16). Ojo: los alias son los de N++ tal cual, incluidas
// sus rarezas (p. ej. "koi8_r" no coincide con el "KOI8-R" que devuelve uchardet).
constexpr std::array kCodepages{
    CodepageInfo{1250, "windows-1250", "WINDOWS-1250", "Windows-1250"},
    CodepageInfo{1251, "windows-1251", "WINDOWS-1251", "Windows-1251"},
    CodepageInfo{1252, "windows-1252", "WINDOWS-1252", "Windows-1252"},
    CodepageInfo{1253, "windows-1253", "WINDOWS-1253", "Windows-1253"},
    CodepageInfo{1254, "windows-1254", "WINDOWS-1254", "Windows-1254"},
    CodepageInfo{1255, "windows-1255", "WINDOWS-1255", "Windows-1255"},
    CodepageInfo{1256, "windows-1256", "WINDOWS-1256", "Windows-1256"},
    CodepageInfo{1257, "windows-1257", "WINDOWS-1257", "Windows-1257"},
    CodepageInfo{1258, "windows-1258", "WINDOWS-1258", "Windows-1258"},
    CodepageInfo{28591, "latin1 ISO_8859-1 ISO-8859-1 CP819 IBM819 csISOLatin1 iso-ir-100 l1", "ISO-8859-1", "ISO 8859-1"},
    CodepageInfo{28592, "latin2 ISO_8859-2 ISO-8859-2 csISOLatin2 iso-ir-101 l2", "ISO-8859-2", "ISO 8859-2"},
    CodepageInfo{28593, "latin3 ISO_8859-3 ISO-8859-3 csISOLatin3 iso-ir-109 l3", "ISO-8859-3", "ISO 8859-3"},
    CodepageInfo{28594, "latin4 ISO_8859-4 ISO-8859-4 csISOLatin4 iso-ir-110 l4", "ISO-8859-4", "ISO 8859-4"},
    CodepageInfo{28595, "cyrillic ISO_8859-5 ISO-8859-5 csISOLatinCyrillic iso-ir-144", "ISO-8859-5", "ISO 8859-5"},
    CodepageInfo{28596, "arabic ISO_8859-6 ISO-8859-6 csISOLatinArabic iso-ir-127 ASMO-708 ECMA-114", "ISO-8859-6", "ISO 8859-6"},
    CodepageInfo{28597, "greek ISO_8859-7 ISO-8859-7 csISOLatinGreek greek8 iso-ir-126 ELOT_928 ECMA-118", "ISO-8859-7", "ISO 8859-7"},
    CodepageInfo{28598, "hebrew ISO_8859-8 ISO-8859-8 csISOLatinHebrew iso-ir-138", "ISO-8859-8", "ISO 8859-8"},
    CodepageInfo{28599, "latin5 ISO_8859-9 ISO-8859-9 csISOLatin5 iso-ir-148 l5", "ISO-8859-9", "ISO 8859-9"},
    CodepageInfo{28603, "ISO_8859-13 ISO-8859-13", "ISO-8859-13", "ISO 8859-13"},
    CodepageInfo{28604, "iso-celtic latin8 ISO_8859-14 ISO-8859-14 18 iso-ir-199", "ISO-8859-14", "ISO 8859-14"},
    CodepageInfo{28605, "Latin-9 ISO_8859-15 ISO-8859-15", "ISO-8859-15", "ISO 8859-15"},
    CodepageInfo{437, "IBM437 cp437 437 csPC8CodePage437", "CP437", "OEM-US : CP437"},
    CodepageInfo{720, "IBM720 cp720 oem720 720", nullptr, "OEM 720"},
    CodepageInfo{737, "IBM737 cp737 oem737 737", "CP737", "OEM 737"},
    CodepageInfo{775, "IBM775 cp775 oem775 775", "CP775", "OEM 775"},
    CodepageInfo{850, "IBM850 cp850 oem850 850", "CP850", "OEM 850"},
    CodepageInfo{852, "IBM852 cp852 oem852 852", "CP852", "OEM 852"},
    CodepageInfo{855, "IBM855 cp855 oem855 855 csIBM855", "CP855", "OEM 855"},
    CodepageInfo{857, "IBM857 cp857 oem857 857", "CP857", "OEM 857"},
    CodepageInfo{858, "IBM858 cp858 oem858 858", "CP858", "OEM 858"},
    CodepageInfo{860, "IBM860 cp860 oem860 860", "CP860", "OEM 860 : Portuguese"},
    CodepageInfo{861, "IBM861 cp861 oem861 861", "CP861", "OEM 861 : Icelandic"},
    CodepageInfo{862, "IBM862 cp862 oem862 862", "CP862", "OEM 862"},
    CodepageInfo{863, "IBM863 cp863 oem863 863", "CP863", "OEM 863 : French"},
    CodepageInfo{865, "IBM865 cp865 oem865 865", "CP865", "OEM 865 : Nordic"},
    CodepageInfo{866, "IBM866 cp866 oem866 866", "CP866", "OEM 866"},
    CodepageInfo{869, "IBM869 cp869 oem869 869", "CP869", "OEM 869"},
    // Los CJK usan las variantes de Microsoft (CP9xx), que es lo que hace Windows
    CodepageInfo{950, "big5 csBig5", "CP950", "Big5 (Traditional)"},
    CodepageInfo{936, "gb2312 gbk csGB2312 gb18030", "CP936", "GB2312 (Simplified)"},
    CodepageInfo{932, "Shift_JIS MS_Kanji csShiftJIS csWindows31J", "CP932", "Shift-JIS"},
    CodepageInfo{949, "windows-949 korean", "CP949", "Windows 949"},
    CodepageInfo{51949, "euc-kr csEUCKR", "EUC-KR", "EUC-KR"},
    CodepageInfo{874, "tis-620", "CP874", "TIS-620"},
    CodepageInfo{10007, "x-mac-cyrillic xmaccyrillic", "MAC-CYRILLIC", "Macintosh"},
    CodepageInfo{21866, "koi8_u", "KOI8-U", "KOI8-U"},
    CodepageInfo{20866, "koi8_r csKOI8R", "KOI8-R", "KOI8-R"},
};

constexpr std::array<int, kCodepages.size()> makeCodepageList()
{
    std::array<int, kCodepages.size()> list{};
    for (std::size_t i = 0; i < kCodepages.size(); ++i)
        list[i] = kCodepages[i].codepage;
    return list;
}

constexpr auto kCodepageList = makeCodepageList();

const CodepageInfo* findCodepage(int codepage)
{
    const auto it = std::find_if(kCodepages.begin(), kCodepages.end(),
        [codepage](const CodepageInfo& info) { return info.codepage == codepage; });
    return it == kCodepages.end() ? nullptr : &*it;
}

bool equalsIgnoreCase(std::string_view a, std::string_view b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

// Port de isInListA (EncodingMapper.cpp)
bool isInList(std::string_view token, std::string_view list)
{
    while (!list.empty()) {
        const std::size_t space = list.find(' ');
        const std::string_view word = list.substr(0, space);
        if (!word.empty() && equalsIgnoreCase(token, word))
            return true;
        if (space == std::string_view::npos)
            break;
        list.remove_prefix(space + 1);
    }
    return false;
}

} // namespace

bool TextEncoding::hasBom() const
{
    return codepage == -1
        && (unicodeMode == UniMode::Utf8Bom || unicodeMode == UniMode::Utf16BeBom
            || unicodeMode == UniMode::Utf16LeBom);
}

bool TextEncoding::isUtf16() const
{
    return codepage == -1
        && (unicodeMode == UniMode::Utf16BeBom || unicodeMode == UniMode::Utf16LeBom
            || unicodeMode == UniMode::Utf16BeNoBom || unicodeMode == UniMode::Utf16LeNoBom);
}

std::span<const int> knownCodepages()
{
    return kCodepageList;
}

int codepageFromCharsetName(std::string_view name)
{
    if (name.empty())
        return -1;
    if (isInList(name, "utf-8 utf8"))
        return kCodepageUtf8;
    for (const CodepageInfo& info : kCodepages) {
        if (isInList(name, info.aliases))
            return info.codepage;
    }
    return -1;
}

const char* iconvNameForCodepage(int codepage)
{
    const CodepageInfo* info = findCodepage(codepage);
    return info ? info->iconvName : nullptr;
}

std::string encodingDisplayName(const TextEncoding& encoding)
{
    if (encoding.codepage != -1) {
        const CodepageInfo* info = findCodepage(encoding.codepage);
        return info ? info->displayName : "CP" + std::to_string(encoding.codepage);
    }
    // Notepad_plus::setUniModeText
    switch (encoding.unicodeMode) {
    case UniMode::Utf8Bom: return "UTF-8-BOM";
    case UniMode::Utf16BeBom: return "UTF-16 BE BOM";
    case UniMode::Utf16LeBom: return "UTF-16 LE BOM";
    case UniMode::Utf16BeNoBom: return "UTF-16 Big Endian";
    case UniMode::Utf16LeNoBom: return "UTF-16 Little Endian";
    case UniMode::Utf8: return "UTF-8";
    case UniMode::Ansi:
    case UniMode::Ascii7: break;
    }
    return "ANSI";
}

} // namespace edita
