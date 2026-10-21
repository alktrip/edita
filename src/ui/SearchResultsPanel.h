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

#include "doc/Buffer.h"
#include "search/SearchEngine.h"

#include <Scintilla.h>

#include <QDockWidget>

#include <optional>
#include <string>
#include <vector>

class ScintillaEditBase;

namespace edita {

class SyntaxManager;

// Panel de resultados de búsqueda. Port de Finder (FindReplaceDlg.cpp): un
// Scintilla de solo lectura con el lexer "searchResult" de N++, que colorea
// según el formato de cada línea:
//   Buscar "x" (3 coincidencias en 2 archivos de 5 analizados) [Normal]   ← cabecera
//     /ruta/archivo (2 coincidencias)                                      ← archivo
//   \tLínea 12: texto con x                                                 ← resultado
// Las posiciones de cada coincidencia se le pasan al lexer con la propiedad
// "@MarkingsStruct" (puntero a SearchResultMarkings), como hace N++.
// Cada búsqueda nueva va arriba y las anteriores quedan plegadas.
class SearchResultsPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit SearchResultsPanel(QWidget* parent = nullptr);
    ~SearchResultsPanel() override;

    ScintillaEditBase* view() const { return _view; }

    // Aplica el lexer y los estilos de "searchResult" del tema
    void applyStyles(const SyntaxManager& syntax);

    void beginSearch(const QString& searchText);
    // `displayName` es lo que se muestra (ruta o "nuevo 1"); `path`/`buffer` sirven para volver al resultado
    void addFile(const QString& displayName, const QString& path, BufferId buffer, const std::vector<LineHit>& lines,
                 int count);
    void endSearch(int totalHits, int filesSearched, const SearchOptions& options, bool inSelection = false);
    void clear();

    int resultLineCount() const;
    // F4 / Mayús+F4: siguiente o anterior resultado; false si no hay
    bool activateAdjacentResult(bool forward);
    // Activa el resultado de una línea del panel (doble clic)
    bool activateLine(SciPos line);

signals:
    void resultActivated(const QString& path, edita::BufferId buffer, edita::SciPos start, edita::SciPos end);

private:
    struct Entry
    {
        QString path;
        BufferId buffer = kInvalidBuffer;
        SciPos start = 0;
        SciPos end = 0;
    };
    struct Block
    {
        std::string text;
        std::vector<SearchResultMarkingLine> markings; // una por línea del bloque
        std::vector<std::optional<Entry>> entries;      // una por línea del bloque
        std::size_t headerLength = 0;                   // para insertar el resumen en la cabecera
        std::size_t fileHeaderLine = 0;
        int filesWithHits = 0;
    };

    void appendLine(Block& block, const std::string& line, SearchResultMarkingLine marking = {},
                    std::optional<Entry> entry = std::nullopt);
    void rebuild();

    ScintillaEditBase* _view;
    std::vector<Block> _blocks; // el primero es el más reciente
    std::optional<Block> _current;
    std::vector<SearchResultMarkingLine> _markings;
    std::vector<std::optional<Entry>> _entries;
    SearchResultMarkings _markingsStruct{0, nullptr};
    SciPos _currentLine = -1;
};

} // namespace edita
