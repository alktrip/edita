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

#include "core/ApiFile.h"
#include "core/LangType.h"
#include "core/Preferences.h"

#include <ScintillaEditBase.h>

#include <memory>
#include <string>
#include <vector>

namespace edita {

class EditorView;

// Autocompletado de una vista. Port de AutoCompletion y FunctionCallTip de N++
// (ScintillaComponent/AutoCompletion.cpp, FunctionCallTip.cpp):
//   - lista de palabras del documento y/o de las del archivo de API del lenguaje
//   - ayuda de parámetros (call tip) con sobrecargas
//   - completar rutas
//   - cerrar paréntesis, corchetes, llaves, comillas y etiquetas HTML/XML
class AutoCompletion
{
public:
    // Separador de tipo de Scintilla e imágenes de la lista (FUNC_IMG_ID, BOX_IMG_ID)
    static constexpr char kTypeSeparator = '\x1E';
    static constexpr int kFunctionImage = 1000;
    static constexpr int kKeywordImage = 1001;

    AutoCompletion(EditorView& view, const Preferences& prefs);
    ~AutoCompletion();
    AutoCompletion(const AutoCompletion&) = delete;
    AutoCompletion& operator=(const AutoCompletion&) = delete;

    // Carga el archivo de API del lenguaje (si lo hay). true si hay API.
    bool setLanguage(LangType lang);
    bool hasApi() const { return !_api.empty(); }
    const ApiLanguage& api() const { return _api; }

    // Ctrl+Espacio (IDM_EDIT_AUTOCOMPLETE): palabras de la API
    bool showApiComplete();
    // Ctrl+Intro (IDM_EDIT_AUTOCOMPLETE_CURRENTFILE): palabras del documento.
    // Con `autoInsert`, si solo hay una, se escribe sin mostrar la lista.
    bool showWordComplete(bool autoInsert);
    bool showApiAndWordComplete();
    // Ctrl+Mayús+Espacio (IDM_EDIT_FUNCCALLTIP): parámetros de la función del cursor
    bool showFunctionComplete();
    // Ctrl+Alt+Espacio (IDM_EDIT_AUTOCOMPLETE_PATH)
    void showPathCompletion();

    // SCN_CHARADDED: cierre automático de pares y, después, lista o ayuda
    void insertMatchedChars(int ch);
    void update(int ch);
    // SCN_CALLTIPCLICK: 1 = flecha arriba, 2 = flecha abajo
    void callTipClick(int direction);
    bool isCallTipVisible() const;
    void closeCallTip();

private:
    enum class Type { Word, Func, FuncBrief, FuncAndWord };
    struct MatchedChar
    {
        char c;
        sptr_t pos;
    };

    bool showAutoComplete(Type type, bool autoInsert);
    void getWordArray(std::vector<std::string>& words, const std::string& begin, const std::string& exclude) const;
    std::string text(sptr_t start, sptr_t end) const;
    void showList(sptr_t enteredLength, const std::string& list);
    std::string closeTag(sptr_t caret, bool isHtml) const;

    // FunctionCallTip
    bool updateCallTip(int ch, bool needShown);
    bool getCursorFunction();
    bool loadFunction();
    void showCallTip();
    void resetCallTip();
    bool isWordChar(char c) const;

    // InsertedMatchedChars
    void removeInvalidMatched(char c, sptr_t pos);
    void addMatched(char c, sptr_t pos);
    sptr_t searchMatched(char start, char end, sptr_t pos);

    EditorView& _view;
    const Preferences& _prefs;
    LangType _lang = LangType::Text;
    bool _loaded = false;
    ApiLanguage _api;
    std::vector<std::string> _keywordArray; // "nombre\x1E1000"
    std::string _keywords;                  // la lista entera, separada por espacios
    std::size_t _keywordMaxLen = 0;
    bool _imagesRegistered = false;

    // Estado de la ayuda de parámetros
    const ApiKeyword* _function = nullptr;
    std::string _functionName;
    std::size_t _currentOverload = 0;
    std::size_t _currentParam = 0;
    sptr_t _callTipPos = 0;
    sptr_t _curPos = 0;
    bool _callTipActive = false;

    std::vector<MatchedChar> _matched;
};

} // namespace edita
