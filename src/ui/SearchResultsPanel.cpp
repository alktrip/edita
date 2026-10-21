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

#include "ui/SearchResultsPanel.h"

#include "core/LangType.h"
#include "syntax/SyntaxManager.h"

#include <ScintillaEditBase.h>

#include <QLocale>

#include <cstdio>

namespace edita {

namespace {

std::string utf8(const QString& s)
{
    return s.toStdString();
}

QString hitsText(int count)
{
    // Finder::getHitsString
    return SearchResultsPanel::tr("(%Ln hit(s))", nullptr, count);
}

int digits(SciPos n)
{
    int d = 1;
    while (n >= 10) {
        n /= 10;
        ++d;
    }
    return d;
}

} // namespace

SearchResultsPanel::SearchResultsPanel(QWidget* parent)
    : QDockWidget(tr("Search results"), parent)
    , _view(new ScintillaEditBase(this))
{
    setObjectName(QStringLiteral("searchResultsPanel"));
    setWidget(_view);
    _view->send(SCI_SETCODEPAGE, SC_CP_UTF8);
    _view->send(SCI_SETREADONLY, 1);
    _view->send(SCI_SETMARGINWIDTHN, 0, 0);
    _view->send(SCI_SETMARGINWIDTHN, 1, 0);
    _view->send(SCI_SETMARGINMASKN, 2, static_cast<sptr_t>(SC_MASK_FOLDERS));
    _view->send(SCI_SETMARGINSENSITIVEN, 2, 1);
    _view->send(SCI_SETAUTOMATICFOLD, SC_AUTOMATICFOLD_SHOW | SC_AUTOMATICFOLD_CLICK | SC_AUTOMATICFOLD_CHANGE);
    _view->send(SCI_SETCARETLINEVISIBLEALWAYS, 1);
    connect(_view, &ScintillaEditBase::doubleClick, this, [this](Scintilla::Position, Scintilla::Position line) {
        activateLine(line);
    });
}

SearchResultsPanel::~SearchResultsPanel() = default;

void SearchResultsPanel::applyStyles(const SyntaxManager& syntax)
{
    syntax.apply(*_view, LangType::SearchResult);
    // SCI_SETILEXER borra las propiedades: hay que volver a pasar las marcas
    rebuild();
}

void SearchResultsPanel::appendLine(Block& block, const std::string& line, SearchResultMarkingLine marking,
                                    std::optional<Entry> entry)
{
    block.text += line;
    block.text += '\n';
    block.markings.push_back(std::move(marking));
    block.entries.push_back(std::move(entry));
}

void SearchResultsPanel::beginSearch(const QString& searchText)
{
    // Finder::addSearchLine: el texto buscado en una sola línea
    QString oneLine = searchText;
    oneLine.remove(QLatin1Char('\r')).remove(QLatin1Char('\n'));
    _current = Block{};
    const std::string header = utf8(tr("Search \"%1\" ").arg(oneLine));
    appendLine(*_current, header);
    _current->headerLength = header.size();
}

void SearchResultsPanel::addFile(const QString& displayName, const QString& path, BufferId buffer,
                                 const std::vector<LineHit>& lines, int count)
{
    if (!_current)
        beginSearch(QString());
    Block& block = *_current;
    appendLine(block, "  " + utf8(displayName) + " " + utf8(hitsText(count)));
    ++block.filesWithHits;

    // Finder::foundLine: "\tLínea   N: texto", con el número alineado
    SciPos maxLine = 0;
    for (const LineHit& hit : lines)
        maxLine = std::max(maxLine, hit.line + 1);
    const std::string prefix = utf8(tr("Line"));
    for (const LineHit& hit : lines) {
        const SciPos number = hit.line + 1;
        std::string header = "\t" + prefix + " ";
        header.append(static_cast<std::size_t>(digits(maxLine) - digits(number)), ' ');
        header += std::to_string(number) + ": ";
        SearchResultMarkingLine marking;
        for (const auto& [s, e] : hit.segments)
            marking._segmentPostions.emplace_back(static_cast<intptr_t>(header.size()) + s,
                                                  static_cast<intptr_t>(header.size()) + e);
        Entry entry{path, buffer, hit.ranges.front().first, hit.ranges.front().second};
        appendLine(block, header + hit.text, std::move(marking), std::move(entry));
    }
}

void SearchResultsPanel::endSearch(int totalHits, int filesSearched, const SearchOptions& options, bool inSelection)
{
    if (!_current)
        beginSearch(QString::fromStdString(options.pattern));
    Block& block = *_current;
    // Finder::addSearchResultInfo
    const QLocale locale;
    const QString where = inSelection ? tr("%Ln selection(s)", nullptr, block.filesWithHits)
                                      : tr("%Ln file(s)", nullptr, block.filesWithHits);
    QString info = tr("(%Ln hit(s) in %1 of %2 searched)", nullptr, totalHits).arg(where, locale.toString(filesSearched));
    QString mode;
    switch (options.mode) {
    case SearchMode::Normal: mode = tr("Normal"); break;
    case SearchMode::Extended: mode = tr("Extended"); break;
    case SearchMode::Regex: mode = tr("Regex") + (options.dotMatchesNewline ? QStringLiteral(".") : QString()); break;
    }
    QStringList flags;
    if (options.matchCase)
        flags << tr("Case");
    if (options.wholeWord && options.mode != SearchMode::Regex)
        flags << tr("Word");
    info += QStringLiteral(" [%1%2]").arg(mode, flags.isEmpty() ? QString() : QStringLiteral(": ") + flags.join(QLatin1Char('/')));
    block.text.insert(block.headerLength, utf8(info));

    _blocks.insert(_blocks.begin(), std::move(block));
    _current.reset();
    _currentLine = -1;
    rebuild();
}

void SearchResultsPanel::clear()
{
    _blocks.clear();
    _current.reset();
    _currentLine = -1;
    rebuild();
}

void SearchResultsPanel::rebuild()
{
    std::string text;
    _markings.clear();
    _entries.clear();
    std::vector<SciPos> blockStarts;
    for (const Block& block : _blocks) {
        blockStarts.push_back(static_cast<SciPos>(_entries.size()));
        text += block.text;
        _markings.insert(_markings.end(), block.markings.begin(), block.markings.end());
        _entries.insert(_entries.end(), block.entries.begin(), block.entries.end());
    }
    _markingsStruct._length = static_cast<intptr_t>(_markings.size());
    _markingsStruct._markings = _markings.empty() ? nullptr : _markings.data();

    // Finder::finishFilesSearch: dirección de las marcas para el lexer
    char pointer[2 * sizeof(void*) + 8];
    std::snprintf(pointer, sizeof(pointer), "%p", static_cast<void*>(&_markingsStruct));
    _view->send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("@MarkingsStruct"), reinterpret_cast<sptr_t>(pointer));
    _view->send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("fold"), reinterpret_cast<sptr_t>("1"));

    _view->send(SCI_SETREADONLY, 0);
    _view->send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(text.c_str()));
    _view->send(SCI_SETREADONLY, 1);
    _view->send(SCI_EMPTYUNDOBUFFER);
    _view->send(SCI_COLOURISE, 0, -1);
    // Las búsquedas anteriores, plegadas
    for (std::size_t i = 1; i < blockStarts.size(); ++i)
        _view->send(SCI_FOLDLINE, static_cast<uptr_t>(blockStarts[i]), SC_FOLDACTION_CONTRACT);
    _view->send(SCI_GOTOPOS, 0);
}

int SearchResultsPanel::resultLineCount() const
{
    int n = 0;
    for (const auto& e : _entries)
        n += e.has_value() ? 1 : 0;
    return n;
}

bool SearchResultsPanel::activateLine(SciPos line)
{
    if (line < 0 || line >= static_cast<SciPos>(_entries.size()) || !_entries[static_cast<std::size_t>(line)])
        return false;
    _currentLine = line;
    _view->send(SCI_ENSUREVISIBLE, static_cast<uptr_t>(line));
    _view->send(SCI_GOTOLINE, static_cast<uptr_t>(line));
    const Entry& e = *_entries[static_cast<std::size_t>(line)];
    emit resultActivated(e.path, e.buffer, e.start, e.end);
    return true;
}

bool SearchResultsPanel::activateAdjacentResult(bool forward)
{
    // Solo dentro de la búsqueda más reciente, como F4 en N++
    const auto lastLine = static_cast<SciPos>(_blocks.empty() ? 0 : _blocks.front().entries.size());
    if (lastLine == 0)
        return false;
    SciPos line = _currentLine;
    for (SciPos tries = 0; tries < lastLine; ++tries) {
        line = forward ? (line + 1) % lastLine : (line <= 0 ? lastLine - 1 : line - 1);
        if (_entries[static_cast<std::size_t>(line)])
            return activateLine(line);
    }
    return false;
}

} // namespace edita
