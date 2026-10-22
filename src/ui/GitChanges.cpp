// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/GitChanges.h"

#include "core/Codec.h"
#include "syntax/SyntaxStyler.h"
#include "ui/EditorView.h"

#include <QFileInfo>
#include <QHash>
#include <QHelpEvent>
#include <QProcess>
#include <QToolTip>

namespace edita {

namespace {

// La versión de HEAD de cada archivo (compartida por las dos vistas); sin entrada: aún
// no se pidió; con nullptr: no está en un repositorio o no está en HEAD
QHash<QString, std::shared_ptr<const std::string>>& cache()
{
    static QHash<QString, std::shared_ptr<const std::string>> instance;
    return instance;
}

bool gitMissing = false; // git no está instalado: no se vuelve a intentar

constexpr qint64 kMaxDocumentBytes = 20 * 1024 * 1024;

sptr_t rgb(int r, int g, int b)
{
    return r | (g << 8) | (b << 16);
}

} // namespace

GitChanges::GitChanges(EditorView* view)
    : QObject(view)
    , _view(view)
{
    _debounce.setSingleShot(true);
    _debounce.setInterval(250);
    connect(&_debounce, &QTimer::timeout, this, &GitChanges::recompute);
    connect(view, &ScintillaEditBase::modified, this, [this](Scintilla::ModificationFlags type, Scintilla::Position, Scintilla::Position,
                                                             Scintilla::Position, const QByteArray&, Scintilla::Position,
                                                             Scintilla::FoldLevel, Scintilla::FoldLevel) {
        const int flags = static_cast<int>(type);
        if (_base && (flags & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)))
            _debounce.start();
    });
    view->send(SCI_SETMARGINTYPEN, kMargin, SC_MARGIN_COLOUR);
    view->send(SCI_SETMARGINMASKN, kMargin, (1 << kMarkerAdded) | (1 << kMarkerModified) | (1 << kMarkerDeleted));
    view->send(SCI_MARKERDEFINE, kMarkerAdded, SC_MARK_FULLRECT);
    view->send(SCI_MARKERDEFINE, kMarkerModified, SC_MARK_FULLRECT);
    view->send(SCI_MARKERDEFINE, kMarkerDeleted, SC_MARK_ARROW);
    view->send(SCI_SETMARGINWIDTHN, kMargin, 0);
    view->viewport()->installEventFilter(this);
    applyColors();
}

GitChanges::~GitChanges()
{
    if (_process) {
        _process->disconnect(this);
        _process->kill();
        _process->waitForFinished(1000);
    }
}

void GitChanges::clearCache()
{
    cache().clear();
}

void GitChanges::setEnabled(bool enabled)
{
    if (enabled == _enabled)
        return;
    _enabled = enabled;
    if (!enabled) {
        clearMarkers();
        _changes.clear();
        emit changed();
    } else {
        const QString path = _path;
        _path.clear();
        setDocument(path, _encoding);
    }
}

void GitChanges::applyColors()
{
    if (!_view)
        return;
    // Los colores de GitHub, más claros sobre fondo oscuro
    const sptr_t back = _view->send(SCI_STYLEGETBACK, STYLE_DEFAULT);
    const int luma = static_cast<int>((back & 0xFF) * 3 + ((back >> 8) & 0xFF) * 6 + ((back >> 16) & 0xFF)) / 10;
    const bool dark = luma < 128;
    const sptr_t added = dark ? rgb(0x3F, 0xB9, 0x50) : rgb(0x2E, 0xA0, 0x43);
    const sptr_t modified = dark ? rgb(0x58, 0xA6, 0xFF) : rgb(0x09, 0x69, 0xDA);
    const sptr_t deleted = dark ? rgb(0xF8, 0x51, 0x49) : rgb(0xCF, 0x22, 0x2E);
    auto toQColor = [](sptr_t c) {
        return QColor(static_cast<int>(c & 0xFF), static_cast<int>((c >> 8) & 0xFF), static_cast<int>((c >> 16) & 0xFF));
    };
    _colors[static_cast<int>(LineChangeKind::Added)] = toQColor(added);
    _colors[static_cast<int>(LineChangeKind::Modified)] = toQColor(modified);
    _colors[static_cast<int>(LineChangeKind::Deleted)] = toQColor(deleted);
    for (const auto& [marker, color] : {std::pair{kMarkerAdded, added}, {kMarkerModified, modified}, {kMarkerDeleted, deleted}}) {
        _view->send(SCI_MARKERSETFORE, static_cast<uptr_t>(marker), color);
        _view->send(SCI_MARKERSETBACK, static_cast<uptr_t>(marker), color);
    }
    // El fondo del margen, el de los números de línea
    _view->send(SCI_SETMARGINBACKN, kMargin, _view->send(SCI_STYLEGETBACK, STYLE_LINENUMBER));
}

void GitChanges::setDocument(const QString& path, const TextEncoding& encoding)
{
    if (!_view)
        return;
    const bool samePath = path == _path;
    _path = path;
    _encoding = encoding;
    if (!_enabled || path.isEmpty() || gitMissing) {
        _base.reset();
        _changes.clear();
        clearMarkers();
        emit changed();
        return;
    }
    if (const auto it = cache().constFind(path); it != cache().constEnd()) {
        _base = it.value();
        recompute();
        return;
    }
    if (!samePath) {
        _base.reset();
        _changes.clear();
        clearMarkers();
        emit changed();
    }
    fetch();
}

void GitChanges::reload()
{
    if (_path.isEmpty() || !_enabled)
        return;
    cache().remove(_path);
    fetch();
}

void GitChanges::fetch()
{
    if (_process) {
        _process->disconnect(this);
        _process->kill();
        _process->deleteLater();
    }
    const QFileInfo info(_path);
    if (!info.exists()) {
        _base.reset();
        recompute();
        return;
    }
    auto* process = new QProcess(this);
    _process = process;
    const quint64 request = ++_request;
    const QString path = _path;
    connect(process, &QProcess::finished, this, [this, process, request, path](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (request != _request)
            return;
        std::shared_ptr<const std::string> base;
        if (status == QProcess::NormalExit && code == 0) {
            const QByteArray bytes = process->readAllStandardOutput();
            std::string utf8 = decodeAll(std::string_view(bytes.constData(), static_cast<size_t>(bytes.size())), _encoding);
            if (utf8.starts_with("\xEF\xBB\xBF"))
                utf8.erase(0, 3);
            base = std::make_shared<const std::string>(std::move(utf8));
        }
        cache().insert(path, base);
        if (path == _path) {
            _base = base;
            recompute();
        }
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            gitMissing = true; // sin git, sin marcas
            process->deleteLater();
            _base.reset();
            recompute();
        }
    });
    // HEAD:./nombre, relativo a la carpeta del archivo (-C): no hace falta saber dónde
    // está la raíz del repositorio. Sin conversiones de texto ni paginador
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->start(QStringLiteral("git"), {QStringLiteral("-C"), info.absolutePath(), QStringLiteral("cat-file"), QStringLiteral("blob"),
                                           QStringLiteral("HEAD:./") + info.fileName()});
}

void GitChanges::recompute()
{
    _debounce.stop();
    if (!_view)
        return;
    if (!_base || !_enabled) {
        _changes.clear();
        clearMarkers();
        emit changed();
        return;
    }
    const sptr_t length = _view->send(SCI_GETLENGTH);
    if (length > kMaxDocumentBytes) {
        _changes.clear();
        clearMarkers();
        emit changed();
        return;
    }
    const auto* data = reinterpret_cast<const char*>(_view->send(SCI_GETCHARACTERPOINTER));
    const std::string_view text(data, static_cast<size_t>(length));
    _changes = diffLines(splitLines(*_base), splitLines(text));
    applyMarkers();
    emit changed();
}

void GitChanges::clearMarkers()
{
    if (!_view)
        return;
    for (int marker : {kMarkerAdded, kMarkerModified, kMarkerDeleted})
        _view->send(SCI_MARKERDELETEALL, static_cast<uptr_t>(marker));
    _view->send(SCI_SETMARGINWIDTHN, kMargin, 0);
}

void GitChanges::applyMarkers()
{
    clearMarkers();
    // El margen solo con los archivos de un repositorio
    _view->send(SCI_SETMARGINWIDTHN, kMargin, _base ? 5 : 0);
    const sptr_t lines = _view->send(SCI_GETLINECOUNT);
    for (const LineChange& c : _changes) {
        if (c.kind == LineChangeKind::Deleted) {
            // El triángulo, en la línea donde estaba lo borrado (al final, en la última)
            const sptr_t line = std::min<sptr_t>(c.line, lines - 1);
            _view->send(SCI_MARKERADD, static_cast<uptr_t>(line), kMarkerDeleted);
            continue;
        }
        const int marker = c.kind == LineChangeKind::Added ? kMarkerAdded : kMarkerModified;
        for (int i = 0; i < c.count; ++i)
            _view->send(SCI_MARKERADD, static_cast<uptr_t>(c.line + i), marker);
    }
}

const LineChange* GitChanges::changeAt(int line) const
{
    for (const LineChange& c : _changes) {
        if (c.kind == LineChangeKind::Deleted ? line == c.line : (line >= c.line && line < c.line + c.count))
            return &c;
    }
    return nullptr;
}

QStringList GitChanges::originalLines(const LineChange& change, int maxLines) const
{
    QStringList result;
    if (!_base)
        return result;
    const std::vector<std::string_view> lines = splitLines(*_base);
    for (int i = 0; i < change.oldCount && i < maxLines; ++i) {
        const size_t index = static_cast<size_t>(change.oldLine + i);
        if (index < lines.size())
            result << QString::fromUtf8(lines[index].data(), static_cast<qsizetype>(lines[index].size()));
    }
    if (change.oldCount > maxLines)
        result << QStringLiteral("…");
    return result;
}

int GitChanges::marginLeft() const
{
    int left = 0;
    for (int m = 0; m < kMargin; ++m)
        left += static_cast<int>(_view->send(SCI_GETMARGINWIDTHN, static_cast<uptr_t>(m)));
    return left;
}

// El aviso con las líneas originales, al pasar el ratón por una marca
bool GitChanges::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::ToolTip || !_view || watched != _view->viewport() || _changes.empty())
        return false;
    auto* help = static_cast<QHelpEvent*>(event);
    const int left = marginLeft();
    const int width = static_cast<int>(_view->send(SCI_GETMARGINWIDTHN, kMargin));
    if (width == 0 || help->pos().x() < left || help->pos().x() >= left + width)
        return false;
    const sptr_t pos = _view->send(SCI_POSITIONFROMPOINT, static_cast<uptr_t>(left + width + 1), help->pos().y());
    const int line = static_cast<int>(_view->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos)));
    const LineChange* change = changeAt(line);
    if (!change) {
        QToolTip::hideText();
        return true;
    }
    QString text;
    switch (change->kind) {
    case LineChangeKind::Added:
        text = tr("Added since the last commit: %n line(s)", nullptr, change->count);
        break;
    case LineChangeKind::Modified:
        text = tr("Modified since the last commit. Before:");
        break;
    case LineChangeKind::Deleted:
        text = tr("Deleted since the last commit:");
        break;
    }
    if (change->kind != LineChangeKind::Added) {
        QStringList lines = originalLines(*change);
        for (QString& l : lines)
            l = l.toHtmlEscaped();
        text = QStringLiteral("%1<pre style=\"margin:4px 0 0 0\">%2</pre>").arg(text.toHtmlEscaped(), lines.join(QLatin1Char('\n')));
    }
    QToolTip::showText(help->globalPos(), text, _view->viewport());
    return true;
}

} // namespace edita
