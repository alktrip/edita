// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plugin de lexer para las pruebas (fase 63): exporta, además de la API de plugins, las
// funciones de Lexilla con las que N++ carga los lexers de los plugins (GetLexerCount,
// GetLexerName, CreateLexer). Da dos lenguajes:
//   «Fichas»: una línea que empieza por # es un comentario (estilo 1), las palabras de la
//             lista 0 son instrucciones (estilo 2) y los números, el estilo 3
//   «Vacío»: sin XML de estilos, todo el texto en el estilo 0

#include <edita_plugin.h>

#include <ILexer.h>

#include <cctype>
#include <cstring>
#include <set>
#include <sstream>
#include <string>

namespace {

class FichasLexer final : public Scintilla::ILexer5
{
public:
    explicit FichasLexer(bool styled) : _styled(styled) {}

    int SCI_METHOD Version() const override { return Scintilla::lvRelease5; }
    void SCI_METHOD Release() override { delete this; }
    const char* SCI_METHOD PropertyNames() override { return ""; }
    int SCI_METHOD PropertyType(const char*) override { return 0; }
    const char* SCI_METHOD DescribeProperty(const char*) override { return ""; }
    Sci_Position SCI_METHOD PropertySet(const char*, const char*) override { return -1; }
    const char* SCI_METHOD DescribeWordListSets() override { return "Instrucciones"; }
    Sci_Position SCI_METHOD WordListSet(int n, const char* list) override
    {
        if (n != 0)
            return -1;
        _keywords.clear();
        std::istringstream words(list ? list : "");
        for (std::string word; words >> word;)
            _keywords.insert(word);
        return 0; // desde el principio
    }
    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int, Scintilla::IDocument* doc) override
    {
        // Desde el principio de la línea: cada línea se colorea sola
        const Sci_Position start = doc->LineStart(doc->LineFromPosition(static_cast<Sci_Position>(startPos)));
        const Sci_Position end = static_cast<Sci_Position>(startPos) + lengthDoc;
        std::string text(static_cast<std::size_t>(end - start), '\0');
        doc->GetCharRange(text.data(), start, end - start);
        doc->StartStyling(start);
        std::size_t i = 0;
        while (i < text.size()) {
            if (!_styled) {
                doc->SetStyleFor(static_cast<Sci_Position>(text.size() - i), 0);
                break;
            }
            const bool lineStart = i == 0 || text[i - 1] == '\n';
            if (lineStart && text[i] == '#') {
                const std::size_t eol = text.find('\n', i);
                const std::size_t n = (eol == std::string::npos ? text.size() : eol) - i;
                doc->SetStyleFor(static_cast<Sci_Position>(n), 1);
                i += n;
            } else if (std::isalpha(static_cast<unsigned char>(text[i]))) {
                std::size_t j = i;
                while (j < text.size() && std::isalnum(static_cast<unsigned char>(text[j])))
                    ++j;
                doc->SetStyleFor(static_cast<Sci_Position>(j - i), _keywords.count(text.substr(i, j - i)) ? 2 : 0);
                i = j;
            } else if (std::isdigit(static_cast<unsigned char>(text[i]))) {
                std::size_t j = i;
                while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j])))
                    ++j;
                doc->SetStyleFor(static_cast<Sci_Position>(j - i), 3);
                i = j;
            } else {
                doc->SetStyleFor(1, 0);
                ++i;
            }
        }
    }
    void SCI_METHOD Fold(Sci_PositionU, Sci_Position, int, Scintilla::IDocument*) override {}
    void* SCI_METHOD PrivateCall(int, void*) override { return nullptr; }
    int SCI_METHOD LineEndTypesSupported() override { return 0; }
    int SCI_METHOD AllocateSubStyles(int, int) override { return -1; }
    int SCI_METHOD SubStylesStart(int) override { return -1; }
    int SCI_METHOD SubStylesLength(int) override { return 0; }
    int SCI_METHOD StyleFromSubStyle(int subStyle) override { return subStyle; }
    int SCI_METHOD PrimaryStyleFromStyle(int style) override { return style; }
    void SCI_METHOD FreeSubStyles() override {}
    void SCI_METHOD SetIdentifiers(int, const char*) override {}
    int SCI_METHOD DistanceToSecondaryStyles() override { return 0; }
    const char* SCI_METHOD GetSubStyleBases() override { return ""; }
    int SCI_METHOD NamedStyles() override { return 4; }
    const char* SCI_METHOD NameOfStyle(int style) override
    {
        static const char* const names[] = {"default", "comment", "instruction", "number"};
        return style >= 0 && style < 4 ? names[style] : "";
    }
    const char* SCI_METHOD TagsOfStyle(int) override { return ""; }
    const char* SCI_METHOD DescriptionOfStyle(int) override { return ""; }
    const char* SCI_METHOD GetName() override { return _styled ? "Fichas" : "Vacío"; }
    int SCI_METHOD GetIdentifier() override { return 0; }
    const char* SCI_METHOD PropertyGet(const char*) override { return ""; }

private:
    bool _styled;
    std::set<std::string> _keywords;
};

EditaData g_data;

void about() {} // un plugin necesita al menos una orden

EditaFuncItem g_items[1];

} // namespace

EDITA_PLUGIN_EXPORT int edita_plugin_api_version(void) { return EDITA_PLUGIN_API_VERSION; }
EDITA_PLUGIN_EXPORT void setInfo(EditaData data) { g_data = data; }
EDITA_PLUGIN_EXPORT const char* getName(void) { return "Fichas Lexer"; }
EDITA_PLUGIN_EXPORT EditaFuncItem* getFuncsArray(int* count)
{
    std::strcpy(g_items[0].itemName, "About");
    g_items[0].pFunc = about;
    *count = 1;
    return g_items;
}

// Lexilla (Lexilla.h): con los mismos nombres que exportan los plugins de lexer de N++
EDITA_PLUGIN_EXPORT int GetLexerCount() { return 3; }
EDITA_PLUGIN_EXPORT void GetLexerName(unsigned int index, char* name, int length)
{
    // El tercero repite el primero: como en N++, un nombre ya registrado se ignora
    const char* names[] = {"Fichas", "Vacío", "Fichas"};
    std::strncpy(name, index < 3 ? names[index] : "", static_cast<std::size_t>(length));
}
EDITA_PLUGIN_EXPORT Scintilla::ILexer5* CreateLexer(const char* name)
{
    if (std::strcmp(name, "Fichas") == 0)
        return new FichasLexer(true);
    if (std::strcmp(name, "Vacío") == 0)
        return new FichasLexer(false);
    return nullptr;
}
