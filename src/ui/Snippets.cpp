// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/Snippets.h"

#include "core/LangType.h"
#include "core/Paths.h"
#include "ui/EditorView.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLocale>
#include <QRandomGenerator>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <limits>

namespace edita {

namespace {

constexpr int kIndicator = 8; // los marcadores del fragmento (N++ deja 0-7 libres y los plugins empiezan en 9)

bool isWordChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

QString rangeText(ScintillaEditBase& view, sptr_t start, sptr_t end)
{
    if (end <= start)
        return {};
    const auto* text = reinterpret_cast<const char*>(view.send(SCI_GETRANGEPOINTER, static_cast<uptr_t>(start), end - start));
    return text ? QString::fromUtf8(text, static_cast<qsizetype>(end - start)) : QString();
}

} // namespace

// --- Biblioteca -------------------------------------------------------------------------

SnippetLibrary::SnippetLibrary(QObject* parent)
    : QObject(parent)
{
    reload();
}

QString SnippetLibrary::userDirectory()
{
    return QDir(paths::configDir()).filePath(QStringLiteral("snippets"));
}

void SnippetLibrary::reload()
{
    _builtin.clear();
    _user.clear();
    // Los de Edita: el primer data/snippets que tenga cada archivo
    for (const QString& dir : paths::dataDirs()) {
        const QDir snippets(QDir(dir).filePath(QStringLiteral("snippets")));
        for (const QFileInfo& info : snippets.entryInfoList({QStringLiteral("*.json")}, QDir::Files)) {
            if (!_builtin.contains(info.completeBaseName()))
                _builtin.insert(info.completeBaseName(), snippet::loadFile(info.absoluteFilePath()));
        }
    }
    for (const QFileInfo& info : QDir(userDirectory()).entryInfoList({QStringLiteral("*.json")}, QDir::Files))
        _user.insert(info.completeBaseName(), snippet::loadFile(info.absoluteFilePath()));
    emit changed();
}

std::vector<snippet::Definition> SnippetLibrary::forLanguage(const QString& language) const
{
    std::vector<snippet::Definition> result;
    for (const QString& key : {language, QStringLiteral("global")}) {
        const auto user = _user.value(key);
        result.insert(result.end(), user.begin(), user.end());
        for (const snippet::Definition& d : _builtin.value(key)) {
            const bool replaced = std::any_of(user.begin(), user.end(), [&](const auto& u) { return u.name == d.name || u.prefix == d.prefix; });
            if (!replaced)
                result.push_back(d);
        }
    }
    return result;
}

QString SnippetLibrary::languageKey(int langType, const QString& userLanguage)
{
    const auto lang = static_cast<LangType>(langType);
    if (lang == LangType::User)
        return userLanguage.contains(QLatin1String("markdown"), Qt::CaseInsensitive) ? QStringLiteral("markdown") : userLanguage.toLower();
    QString name = QString::fromLatin1(languageInfo(lang).name);
    if (name.startsWith(QLatin1String("javascript")))
        return QStringLiteral("javascript");
    if (lang == LangType::Text)
        return QStringLiteral("text");
    return name;
}

QString SnippetLibrary::ensureUserFile(const QString& language) const
{
    const QString path = QDir(userDirectory()).filePath(language + QStringLiteral(".json"));
    if (QFileInfo::exists(path))
        return path;
    QDir().mkpath(userDirectory());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        const QString text = tr("{\n"
                                "    // Snippets for %1 (the format of VS Code). They replace the ones of Edita with the same name or prefix.\n"
                                "    // Type the prefix and press Tab. $1, $2… are the places Tab goes to; ${1:text} with a text; $0, the end.\n"
                                "    // Example:\n"
                                "    // \"For loop\": {\n"
                                "    //     \"prefix\": \"for\",\n"
                                "    //     \"body\": [\"for (int ${1:i} = 0; $1 < ${2:n}; ++$1) {\", \"\\t$0\", \"}\"],\n"
                                "    //     \"description\": \"A for loop\"\n"
                                "    // }\n"
                                "}\n")
                                 .arg(language);
        file.write(text.toUtf8());
    }
    return path;
}

// --- Controlador ------------------------------------------------------------------------

SnippetController::SnippetController(EditorView* view, const SnippetLibrary& library)
    : QObject(view)
    , _view(view)
    , _library(library)
{
    setObjectName(QStringLiteral("snippetController"));
    view->installEventFilter(this);
    connect(view, &ScintillaEditBase::modified, this,
            [this](Scintilla::ModificationFlags type, Scintilla::Position position, Scintilla::Position length) { track(type, position, length); });
    connect(view, &ScintillaEditBase::updateUi, this, [this](Scintilla::Update updated) {
        // Si el cursor sale del fragmento, se acabó
        if (!isActive() || _changing || !(static_cast<int>(updated) & SC_UPDATE_SELECTION) || !_view)
            return;
        const sptr_t caret = _view->send(SCI_GETCURRENTPOS);
        sptr_t first = std::numeric_limits<sptr_t>::max();
        sptr_t last = 0;
        for (const Stop& s : _stops) {
            for (const auto& [a, b] : s.ranges) {
                first = std::min(first, a);
                last = std::max(last, b);
            }
        }
        if (caret < first || caret > last)
            stop();
    });
}

void SnippetController::setContext(const QString& language, const QString& filePath)
{
    stop();
    _language = language;
    _filePath = filePath;
}

QString SnippetController::indentUnit() const
{
    if (_view->send(SCI_GETUSETABS))
        return QStringLiteral("\t");
    return QString(static_cast<qsizetype>(_view->send(SCI_GETINDENT) > 0 ? _view->send(SCI_GETINDENT) : _view->send(SCI_GETTABWIDTH)), QLatin1Char(' '));
}

bool SnippetController::expandAtCaret()
{
    if (!_view || _language.isEmpty())
        return false;
    const sptr_t caret = _view->send(SCI_GETCURRENTPOS);
    const sptr_t lineStart = _view->send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(_view->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(caret))));
    const QString before = rangeText(*_view, lineStart, caret);
    const snippet::Definition* best = nullptr;
    const auto definitions = _library.forLanguage(_language);
    for (const snippet::Definition& d : definitions) {
        if (!before.endsWith(d.prefix) || (best && best->prefix.size() >= d.prefix.size()))
            continue;
        // El prefijo, entero: «fori» no expande «i»
        const qsizetype at = before.size() - d.prefix.size() - 1;
        if (at >= 0 && isWordChar(d.prefix.front()) && isWordChar(before[at]))
            continue;
        best = &d;
    }
    if (!best)
        return false;
    insert(best->body, caret - static_cast<sptr_t>(best->prefix.toUtf8().size()), caret);
    return true;
}

void SnippetController::insert(const QString& body, sptr_t start, sptr_t end, const QString& selectedText)
{
    stop();
    if (!_view)
        return;
    // La sangría de la línea, en cada línea del fragmento; sus tabuladores, la del documento
    const sptr_t line = _view->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(start));
    const sptr_t lineStart = _view->send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
    const QString lineText = rangeText(*_view, lineStart, _view->send(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line)));
    QString indent;
    for (const QChar c : rangeText(*_view, lineStart, start)) {
        if (c != QLatin1Char(' ') && c != QLatin1Char('\t'))
            break;
        indent += c;
    }
    QString prepared = body;
    prepared.replace(QLatin1Char('\t'), indentUnit());
    prepared.replace(QLatin1Char('\n'), QLatin1Char('\n') + indent);

    const QFileInfo file(_filePath);
    const QDateTime now = QDateTime::currentDateTime();
    const QString wordAtCaret = rangeText(*_view, _view->send(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(start), 1),
                                          _view->send(SCI_WORDENDPOSITION, static_cast<uptr_t>(start), 1));
    const snippet::Variables variables = [&](const QString& name) -> std::optional<QString> {
        if (name == QLatin1String("TM_FILENAME")) return file.fileName();
        if (name == QLatin1String("TM_FILENAME_BASE")) return file.completeBaseName();
        if (name == QLatin1String("TM_DIRECTORY")) return file.absolutePath();
        if (name == QLatin1String("TM_FILEPATH")) return file.absoluteFilePath();
        if (name == QLatin1String("TM_SELECTED_TEXT")) return selectedText;
        if (name == QLatin1String("TM_CURRENT_LINE")) return lineText;
        if (name == QLatin1String("TM_CURRENT_WORD")) return wordAtCaret;
        if (name == QLatin1String("TM_LINE_NUMBER")) return QString::number(line + 1);
        if (name == QLatin1String("TM_LINE_INDEX")) return QString::number(line);
        if (name == QLatin1String("CLIPBOARD")) return QApplication::clipboard()->text();
        if (name == QLatin1String("CURRENT_YEAR")) return now.toString(QStringLiteral("yyyy"));
        if (name == QLatin1String("CURRENT_YEAR_SHORT")) return now.toString(QStringLiteral("yy"));
        if (name == QLatin1String("CURRENT_MONTH")) return now.toString(QStringLiteral("MM"));
        if (name == QLatin1String("CURRENT_MONTH_NAME")) return QLocale().monthName(now.date().month());
        if (name == QLatin1String("CURRENT_DATE")) return now.toString(QStringLiteral("dd"));
        if (name == QLatin1String("CURRENT_DAY_NAME")) return QLocale().dayName(now.date().dayOfWeek());
        if (name == QLatin1String("CURRENT_HOUR")) return now.toString(QStringLiteral("HH"));
        if (name == QLatin1String("CURRENT_MINUTE")) return now.toString(QStringLiteral("mm"));
        if (name == QLatin1String("CURRENT_SECOND")) return now.toString(QStringLiteral("ss"));
        if (name == QLatin1String("UUID")) return QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (name == QLatin1String("RANDOM")) return QString::number(QRandomGenerator::global()->bounded(1000000)).rightJustified(6, QLatin1Char('0'));
        if (name == QLatin1String("RANDOM_HEX")) return QString::number(QRandomGenerator::global()->bounded(0x1000000), 16).rightJustified(6, QLatin1Char('0'));
        return std::nullopt;
    };
    const snippet::Parsed parsed = snippet::parse(prepared, variables);
    const QByteArray utf8 = parsed.text.toUtf8();
    auto bytes = [&](int offset) { return static_cast<sptr_t>(QStringView(parsed.text).first(offset).toUtf8().size()); };

    _changing = true;
    _view->send(SCI_BEGINUNDOACTION);
    _view->send(SCI_SETTARGETRANGE, static_cast<uptr_t>(start), end);
    _view->send(SCI_REPLACETARGET, static_cast<uptr_t>(utf8.size()), reinterpret_cast<sptr_t>(utf8.constData()));
    _view->send(SCI_ENDUNDOACTION);
    _changing = false;

    std::vector<Stop> stops;
    for (const snippet::Tabstop& t : parsed.tabstops) {
        Stop s;
        s.index = t.index;
        for (const auto& [a, b] : t.ranges)
            s.ranges.emplace_back(start + bytes(a), start + bytes(b));
        stops.push_back(std::move(s));
    }
    if (stops.size() <= 1) { // solo el final
        const sptr_t final = stops.empty() ? start + static_cast<sptr_t>(utf8.size()) : stops.front().ranges.front().first;
        _view->send(SCI_GOTOPOS, static_cast<uptr_t>(final));
        return;
    }
    _stops = std::move(stops);
    _savedTyping = _view->send(SCI_GETADDITIONALSELECTIONTYPING) != 0;
    _view->send(SCI_SETMULTIPLESELECTION, 1);
    _view->send(SCI_SETADDITIONALSELECTIONTYPING, 1); // para escribir en los espejos a la vez
    select(0);
}

int SnippetController::currentIndex() const
{
    return isActive() ? _stops[_current].index : -1;
}

void SnippetController::select(std::size_t stop)
{
    _current = stop;
    const Stop& s = _stops[stop];
    _changing = true;
    if (s.index == 0) { // el final: el cursor ahí y se acabó
        _view->send(SCI_GOTOPOS, static_cast<uptr_t>(s.ranges.front().first));
        _changing = false;
        this->stop();
        return;
    }
    // Todos los sitios del marcador seleccionados: lo que se escriba va a todos
    _view->send(SCI_SETSELECTION, static_cast<uptr_t>(s.ranges.front().second), s.ranges.front().first);
    for (std::size_t i = 1; i < s.ranges.size(); ++i)
        _view->send(SCI_ADDSELECTION, static_cast<uptr_t>(s.ranges[i].second), s.ranges[i].first);
    _view->send(SCI_SETMAINSELECTION, 0);
    _view->send(SCI_SCROLLCARET);
    _changing = false;
    highlight();
}

bool SnippetController::next()
{
    if (!isActive())
        return false;
    select(std::min(_current + 1, _stops.size() - 1));
    return true;
}

bool SnippetController::previous()
{
    if (!isActive())
        return false;
    select(_current > 0 ? _current - 1 : 0);
    return true;
}

void SnippetController::stop()
{
    if (!isActive())
        return;
    _stops.clear();
    if (!_view)
        return;
    const sptr_t previous = _view->send(SCI_GETINDICATORCURRENT);
    _view->send(SCI_SETINDICATORCURRENT, kIndicator);
    _view->send(SCI_INDICATORCLEARRANGE, 0, _view->send(SCI_GETLENGTH));
    _view->send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(previous));
    _view->send(SCI_SETADDITIONALSELECTIONTYPING, _savedTyping ? 1 : 0);
}

void SnippetController::track(Scintilla::ModificationFlags type, sptr_t position, sptr_t length)
{
    if (!isActive() || _changing)
        return;
    const int flags = static_cast<int>(type);
    if (flags & SC_MOD_INSERTTEXT) {
        for (std::size_t i = 0; i < _stops.size(); ++i) {
            for (auto& [a, b] : _stops[i].ranges) {
                if (position < a || (position == a && i != _current)) { // antes: se mueve
                    a += length;
                    b += length;
                } else if (position < b || (position == b && i == _current)) { // dentro (o al final del actual)
                    b += length;
                }
            }
        }
    } else if (flags & SC_MOD_DELETETEXT) {
        const sptr_t end = position + length;
        auto adjust = [&](sptr_t x) { return x <= position ? x : x >= end ? x - length : position; };
        for (Stop& s : _stops) {
            for (auto& [a, b] : s.ranges) {
                a = adjust(a);
                b = adjust(b);
            }
        }
    } else {
        return;
    }
    QTimer::singleShot(0, this, &SnippetController::highlight);
}

void SnippetController::highlight()
{
    if (!_view)
        return;
    const sptr_t previous = _view->send(SCI_GETINDICATORCURRENT);
    _view->send(SCI_SETINDICATORCURRENT, kIndicator);
    _view->send(SCI_INDICATORCLEARRANGE, 0, _view->send(SCI_GETLENGTH));
    if (isActive()) {
        _view->send(SCI_INDICSETSTYLE, kIndicator, INDIC_DOTBOX);
        _view->send(SCI_INDICSETFORE, kIndicator, _view->send(SCI_STYLEGETFORE, STYLE_DEFAULT));
        _view->send(SCI_INDICSETALPHA, kIndicator, 30);
        _view->send(SCI_INDICSETOUTLINEALPHA, kIndicator, 140);
        for (const Stop& s : _stops) {
            if (s.index == 0)
                continue;
            for (const auto& [a, b] : s.ranges) {
                if (b > a)
                    _view->send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(a), b - a);
            }
        }
    }
    _view->send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(previous));
}

bool SnippetController::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != _view || event->type() != QEvent::KeyPress || !_view)
        return false;
    auto* key = static_cast<QKeyEvent*>(event);
    const bool lists = _view->send(SCI_AUTOCACTIVE) != 0;
    const Qt::KeyboardModifiers mods = key->modifiers() & ~Qt::KeypadModifier;
    if (key->key() == Qt::Key_Tab && mods == Qt::NoModifier) {
        if (isActive() && !lists)
            return next();
        // Con la lista de autocompletado abierta (al escribir «for» sale la palabra clave),
        // el fragmento gana si la palabra es justo su prefijo
        if (_tabExpansion && _view->send(SCI_GETSELECTIONEMPTY) && _view->send(SCI_GETSELECTIONS) == 1) {
            if (lists)
                _view->send(SCI_AUTOCCANCEL);
            if (expandAtCaret())
                return true;
        }
        return false;
    }
    if (key->key() == Qt::Key_Backtab && isActive())
        return previous();
    if (key->key() == Qt::Key_Escape && isActive() && !lists && !_view->send(SCI_CALLTIPACTIVE)) {
        stop();
        return true;
    }
    return false;
}

} // namespace edita
