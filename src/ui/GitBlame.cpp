// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/GitBlame.h"

#include "core/Codec.h"
#include "ui/EolAnnotations.h"
#include "ui/EditorView.h"

#include <QFileInfo>
#include <QHelpEvent>
#include <QProcess>
#include <QToolTip>

namespace edita {

namespace {

constexpr sptr_t kMaxDocument = 5 * 1024 * 1024;
constexpr int kAgeStyles = 5; // del más reciente al más antiguo
const QString kUncommittedHash = QString(40, QLatin1Char('0'));

QColor fromScintilla(sptr_t c)
{
    return QColor(static_cast<int>(c & 0xFF), static_cast<int>((c >> 8) & 0xFF), static_cast<int>((c >> 16) & 0xFF));
}

sptr_t toScintilla(const QColor& c)
{
    return c.red() | (c.green() << 8) | (c.blue() << 16);
}

QColor mix(const QColor& a, const QColor& b, double t)
{
    return QColor::fromRgbF(static_cast<float>(a.redF() * (1 - t) + b.redF() * t), static_cast<float>(a.greenF() * (1 - t) + b.greenF() * t),
                            static_cast<float>(a.blueF() * (1 - t) + b.blueF() * t));
}

// La antigüedad, en cinco tramos (una semana, un mes, medio año, dos años, más)
int ageStyle(const GitBlame::Commit& commit, const QDateTime& now)
{
    if (commit.uncommitted)
        return 0;
    const qint64 days = commit.time.daysTo(now);
    return days < 7 ? 0 : days < 31 ? 1 : days < 183 ? 2 : days < 730 ? 3 : 4;
}

} // namespace

GitBlame::GitBlame(EditorView* view)
    : QObject(view)
    , _view(view)
{
    _timer.setSingleShot(true);
    _timer.setInterval(1500);
    connect(&_timer, &QTimer::timeout, this, &GitBlame::run);
    connect(view, &ScintillaEditBase::modified, this, [this](Scintilla::ModificationFlags type) {
        if (!_path.isEmpty() && (static_cast<int>(type) & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)))
            _timer.start();
    });
    connect(view, &ScintillaEditBase::updateUi, this, [this](Scintilla::Update updated) {
        if (static_cast<int>(updated) & (SC_UPDATE_SELECTION | SC_UPDATE_CONTENT))
            updateInline();
    });
    if (view->send(SCI_GETMARGINS) <= static_cast<sptr_t>(kMargin))
        view->send(SCI_SETMARGINS, kMargin + 1);
    view->send(SCI_SETMARGINTYPEN, kMargin, SC_MARGIN_TEXT);
    view->send(SCI_SETMARGINWIDTHN, kMargin, 0);
    view->send(SCI_SETMARGINMASKN, kMargin, 0);
    view->viewport()->installEventFilter(this);
}

GitBlame::~GitBlame()
{
    if (_process) {
        _process->disconnect(this);
        _process->kill();
        _process->waitForFinished(500);
    }
}

void GitBlame::setInlineEnabled(bool enabled)
{
    _inline = enabled;
    updateInline();
}

void GitBlame::setMarginVisible(bool visible)
{
    _marginVisible = visible;
    if (visible && !hasData())
        run();
    updateMargin();
}

void GitBlame::setDocument(const QString& path, const TextEncoding& encoding)
{
    _path = path;
    _encoding = encoding;
    _lines.clear();
    _commits.clear();
    _inlineLine = -1;
    ++_request;
    if (_view) {
        // Lo que quedara de otra vez en este documento
        EolAnnotations::of(_view)->reset();
        _view->send(SCI_MARGINTEXTCLEARALL);
    }
    updateMargin();
    if (!path.isEmpty() && (_inline || _marginVisible))
        run();
}

void GitBlame::reload()
{
    if (!_path.isEmpty() && (_inline || _marginVisible))
        run();
}

void GitBlame::run()
{
    _timer.stop();
    if (!_view || _path.isEmpty() || _view->send(SCI_GETLENGTH) > kMaxDocument)
        return;
    if (_process) {
        _process->disconnect(this);
        connect(_process, &QProcess::finished, _process, &QObject::deleteLater);
        _process->kill();
    }
    // El texto del editor, como estaría en el disco: git lo compara con su historia
    const auto* raw = reinterpret_cast<const char*>(_view->send(SCI_GETCHARACTERPOINTER));
    std::string bytes = encodeAll(std::string_view(raw, static_cast<std::size_t>(_view->send(SCI_GETLENGTH))), _encoding);
    if (_encoding.hasBom() && _encoding.unicodeMode == UniMode::Utf8Bom)
        bytes.insert(0, "\xEF\xBB\xBF");
    auto* process = new QProcess(this);
    _process = process;
    const quint64 request = ++_request;
    connect(process, &QProcess::finished, this, [this, process, request](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (request != _request)
            return;
        _lines.clear();
        _commits.clear();
        if (status == QProcess::NormalExit && code == 0)
            _lines = parsePorcelain(process->readAllStandardOutput(), _commits);
        apply();
    });
    connect(process, &QProcess::errorOccurred, this, [process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            process->deleteLater(); // sin git
    });
    const QFileInfo info(_path);
    process->start(QStringLiteral("git"), {QStringLiteral("-C"), info.absolutePath(), QStringLiteral("blame"), QStringLiteral("--porcelain"),
                                           QStringLiteral("--contents"), QStringLiteral("-"), QStringLiteral("--"), info.fileName()});
    process->write(bytes.data(), static_cast<qint64>(bytes.size()));
    process->closeWriteChannel();
}

std::vector<QString> GitBlame::parsePorcelain(const QByteArray& output, QHash<QString, Commit>& commits)
{
    std::vector<QString> lines;
    QString current;
    int next = 0; // la línea final que viene
    for (const QByteArray& raw : output.split('\n')) {
        if (raw.startsWith('\t')) { // el contenido de la línea
            if (!current.isEmpty() && next >= 0) {
                if (static_cast<int>(lines.size()) <= next)
                    lines.resize(static_cast<std::size_t>(next) + 1);
                lines[static_cast<std::size_t>(next)] = current;
                ++next;
            }
            continue;
        }
        const QList<QByteArray> parts = raw.split(' ');
        if (parts.size() >= 3 && parts[0].size() == 40) { // «hash línea-original línea-final [cuántas]»
            current = QString::fromLatin1(parts[0]);
            next = parts[2].toInt() - 1;
            Commit& c = commits[current];
            c.hash = current;
            c.uncommitted = current == kUncommittedHash;
            continue;
        }
        if (current.isEmpty())
            continue;
        Commit& c = commits[current];
        const qsizetype space = raw.indexOf(' ');
        const QByteArray key = space < 0 ? raw : raw.left(space);
        // En NFC: en macOS, lo que pasó por la línea de órdenes puede venir descompuesto (o + ´)
        const QString value = space < 0 ? QString() : QString::fromUtf8(raw.mid(space + 1)).normalized(QString::NormalizationForm_C);
        if (key == "author")
            c.author = value;
        else if (key == "author-mail")
            c.mail = value.mid(1).chopped(1); // sin < >
        else if (key == "author-time")
            c.time = QDateTime::fromSecsSinceEpoch(value.toLongLong());
        else if (key == "summary")
            c.summary = value;
    }
    return lines;
}

QString GitBlame::relativeTime(const QDateTime& when, const QDateTime& now)
{
    const qint64 seconds = when.secsTo(now);
    if (seconds < 60)
        return tr("just now");
    if (seconds < 3600)
        return tr("%n minute(s) ago", nullptr, static_cast<int>(seconds / 60));
    if (seconds < 86400)
        return tr("%n hour(s) ago", nullptr, static_cast<int>(seconds / 3600));
    const qint64 days = seconds / 86400;
    if (days < 31)
        return tr("%n day(s) ago", nullptr, static_cast<int>(days));
    if (days < 365)
        return tr("%n month(s) ago", nullptr, static_cast<int>(days / 30));
    return tr("%n year(s) ago", nullptr, static_cast<int>(days / 365));
}

const GitBlame::Commit* GitBlame::commitAt(int line) const
{
    if (line < 0 || line >= static_cast<int>(_lines.size()) || _lines[static_cast<std::size_t>(line)].isEmpty())
        return nullptr;
    const auto it = _commits.constFind(_lines[static_cast<std::size_t>(line)]);
    return it == _commits.constEnd() ? nullptr : &*it;
}

QString GitBlame::inlineText(int line) const
{
    const Commit* c = commitAt(line);
    if (!c)
        return {};
    if (c->uncommitted)
        return tr("You, not committed yet");
    QString summary = c->summary;
    if (summary.size() > 60)
        summary = summary.left(59) + QStringLiteral("…");
    return QStringLiteral("%1, %2 • %3").arg(c->author, relativeTime(c->time, QDateTime::currentDateTime()), summary);
}

QString GitBlame::details(const Commit& c) const
{
    if (c.uncommitted)
        return tr("Not committed yet");
    return QStringLiteral("<b>%1</b> &lt;%2&gt;<br>%3 (%4)<br><br>%5<br><br><span style=\"color:gray\">%6</span>")
        .arg(c.author.toHtmlEscaped(), c.mail.toHtmlEscaped(), QLocale().toString(c.time, QLocale::LongFormat),
             relativeTime(c.time, QDateTime::currentDateTime()), c.summary.toHtmlEscaped(), c.hash);
}

void GitBlame::configureStyles()
{
    // Estilos propios (extendidos) para el margen, del color de acento (reciente) al gris
    // (antiguo). El texto al final de la línea es de EolAnnotations
    if (_view->send(SCI_MARGINGETSTYLEOFFSET) == 0)
        _view->send(SCI_MARGINSETSTYLEOFFSET, static_cast<uptr_t>(_view->send(SCI_ALLOCATEEXTENDEDSTYLES, kAgeStyles)));
    const bool dark = fromScintilla(_view->send(SCI_STYLEGETBACK, STYLE_DEFAULT)).lightness() < 128;
    const auto margin = static_cast<uptr_t>(_view->send(SCI_MARGINGETSTYLEOFFSET));
    const QColor marginBack = fromScintilla(_view->send(SCI_STYLEGETBACK, STYLE_LINENUMBER));
    const QColor marginFore = fromScintilla(_view->send(SCI_STYLEGETFORE, STYLE_LINENUMBER));
    const QColor accent = dark ? QColor(0xE2, 0xA0, 0x4D) : QColor(0xB3, 0x5C, 0x00);
    for (int i = 0; i < kAgeStyles; ++i) {
        const auto style = margin + static_cast<uptr_t>(i);
        _view->send(SCI_STYLESETFORE, style, toScintilla(mix(accent, marginFore, i / double(kAgeStyles - 1))));
        _view->send(SCI_STYLESETBACK, style, toScintilla(marginBack));
        _view->send(SCI_STYLESETSIZEFRACTIONAL, style, _view->send(SCI_STYLEGETSIZEFRACTIONAL, STYLE_LINENUMBER));
    }
}

void GitBlame::apply()
{
    if (!_view)
        return;
    configureStyles();
    _inlineLine = -1;
    updateInline();
    updateMargin();
    emit updated();
}

void GitBlame::updateInline()
{
    if (!_view)
        return;
    const auto caretLine = static_cast<int>(_view->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(_view->send(SCI_GETCURRENTPOS))));
    const int line = _inline && hasData() ? caretLine : -1;
    if (line == _inlineLine)
        return;
    EolAnnotations* eol = EolAnnotations::of(_view);
    eol->clear(EolAnnotations::Blame);
    _inlineLine = line;
    if (line >= 0)
        eol->set(EolAnnotations::Blame, line, inlineText(line));
}

void GitBlame::updateMargin()
{
    if (!_view)
        return;
    _view->send(SCI_MARGINTEXTCLEARALL);
    if (!_marginVisible || !hasData()) {
        _view->send(SCI_SETMARGINWIDTHN, kMargin, 0);
        return;
    }
    configureStyles();
    // En la primera línea de cada bloque del mismo commit: hash, autor y fecha
    const QDateTime now = QDateTime::currentDateTime();
    const auto lineCount = static_cast<int>(_view->send(SCI_GETLINECOUNT));
    for (int line = 0; line < std::min(lineCount, static_cast<int>(_lines.size())); ++line) {
        const Commit* c = commitAt(line);
        if (!c)
            continue;
        _view->send(SCI_MARGINSETSTYLE, static_cast<uptr_t>(line), ageStyle(*c, now));
        if (line > 0 && _lines[static_cast<std::size_t>(line - 1)] == _lines[static_cast<std::size_t>(line)])
            continue;
        QString author = c->uncommitted ? tr("Not committed") : c->author;
        if (author.size() > 16)
            author = author.left(15) + QStringLiteral("…");
        const QString text = c->uncommitted ? QStringLiteral("        %1").arg(author)
                                            : QStringLiteral(" %1  %2  %3").arg(c->hash.left(7), author.leftJustified(16), c->time.toString(QStringLiteral("yyyy-MM-dd")));
        const QByteArray utf8 = text.toUtf8();
        _view->send(SCI_MARGINSETTEXT, static_cast<uptr_t>(line), reinterpret_cast<sptr_t>(utf8.constData()));
    }
    const QByteArray sample = QStringLiteral(" 0000000  %1  0000-00-00 ").arg(QString(16, QLatin1Char('M'))).toUtf8();
    _view->send(SCI_SETMARGINWIDTHN, kMargin,
                _view->send(SCI_TEXTWIDTH, static_cast<uptr_t>(_view->send(SCI_MARGINGETSTYLEOFFSET)), reinterpret_cast<sptr_t>(sample.constData())));
}

int GitBlame::marginLeft() const
{
    int left = 0;
    for (unsigned m = 0; m < kMargin; ++m)
        left += static_cast<int>(_view->send(SCI_GETMARGINWIDTHN, m));
    return left;
}

// Los detalles del commit al pasar el ratón por el margen
bool GitBlame::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::ToolTip || !_view || watched != _view->viewport() || !_marginVisible || !hasData())
        return false;
    auto* help = static_cast<QHelpEvent*>(event);
    const int left = marginLeft();
    const auto width = static_cast<int>(_view->send(SCI_GETMARGINWIDTHN, kMargin));
    if (width == 0 || help->pos().x() < left || help->pos().x() >= left + width)
        return false;
    const sptr_t pos = _view->send(SCI_POSITIONFROMPOINT, static_cast<uptr_t>(left + width + 1), help->pos().y());
    const Commit* c = commitAt(static_cast<int>(_view->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos))));
    if (c)
        QToolTip::showText(help->globalPos(), details(*c), _view->viewport());
    else
        QToolTip::hideText();
    return true;
}

} // namespace edita
