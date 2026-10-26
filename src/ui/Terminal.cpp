// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/Terminal.h"

#include "syntax/DesktopFonts.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QProcess>
#include <QScrollBar>
#include <QSocketNotifier>
#include <QTimer>

#include <cerrno>
#include <csignal>
#include <cstring>

#include <fcntl.h>
#include <sys/ioctl.h>
#ifdef __APPLE__
#include <libproc.h>
#include <util.h> // forkpty
#else
#include <pty.h>
#endif
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
extern char** environ;
#endif

namespace edita {

namespace {

constexpr uint32_t kContinuation = 0xFFFFFFFF; // la segunda celda de un carácter ancho

// En macOS, Qt llama «Control» a ⌘ y «Meta» a la tecla Control: para la shell cuenta la
// tecla Control, y ⌘ queda para la ventana (⌘C copia, ⌘V pega)
#ifdef Q_OS_MACOS
constexpr Qt::KeyboardModifier kControlKey = Qt::MetaModifier;
#else
constexpr Qt::KeyboardModifier kControlKey = Qt::ControlModifier;
#endif

bool isCopyShortcut(Qt::KeyboardModifiers mods, int key)
{
#ifdef Q_OS_MACOS
    return key == Qt::Key_C && (mods & Qt::ControlModifier) && !(mods & Qt::MetaModifier);
#else
    return key == Qt::Key_C && (mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier);
#endif
}

bool isPasteShortcut(Qt::KeyboardModifiers mods, int key)
{
#ifdef Q_OS_MACOS
    return key == Qt::Key_V && (mods & Qt::ControlModifier) && !(mods & Qt::MetaModifier);
#else
    return (key == Qt::Key_V && (mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier))
        || (key == Qt::Key_Insert && (mods & Qt::ShiftModifier) && !(mods & Qt::ControlModifier));
#endif
}

QKeySequence copyKeys()
{
#ifdef Q_OS_MACOS
    return QKeySequence(Qt::CTRL | Qt::Key_C);
#else
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C);
#endif
}

QKeySequence pasteKeys()
{
#ifdef Q_OS_MACOS
    return QKeySequence(Qt::CTRL | Qt::Key_V);
#else
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V);
#endif
}

QColor rgb(const VTermColor& c)
{
    return QColor(c.rgb.red, c.rgb.green, c.rgb.blue);
}

VTermColor vtermColor(const QColor& c)
{
    VTermColor color;
    vterm_color_rgb(&color, static_cast<uint8_t>(c.red()), static_cast<uint8_t>(c.green()), static_cast<uint8_t>(c.blue()));
    return color;
}

QString cellText(const VTermScreenCell& cell)
{
    QString text;
    for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; ++i) {
        const char32_t c = cell.chars[i];
        text += QString::fromUcs4(&c, 1);
    }
    return text;
}

bool isWordCharacter(QChar c)
{
    return !c.isSpace() && !QStringLiteral("()[]{}<>'\"`|;,").contains(c);
}

} // namespace

// --- TerminalSession -------------------------------------------------------------------

TerminalSession::TerminalSession(QObject* parent)
    : QObject(parent)
{
}

TerminalSession::~TerminalSession()
{
    terminate();
}

QStringList TerminalSession::environmentFor(const QStringList& system)
{
    // Lo que pone el AppImage (y lo que la terminal define) no pasa a la shell
    static const QStringList kDropped = {QStringLiteral("APPIMAGE"), QStringLiteral("APPDIR"), QStringLiteral("ARGV0"),
                                         QStringLiteral("OWD"), QStringLiteral("APPIMAGE_EXTRACT_AND_RUN"), QStringLiteral("LINES"),
                                         QStringLiteral("COLUMNS"), QStringLiteral("TERM"), QStringLiteral("COLORTERM"),
                                         QStringLiteral("TERM_PROGRAM"), QStringLiteral("TERM_PROGRAM_VERSION")};
    QStringList env;
    for (const QString& entry : system) {
        const QString name = entry.section(QLatin1Char('='), 0, 0);
        if (!kDropped.contains(name))
            env << entry;
    }
    env << QStringLiteral("TERM=xterm-256color") << QStringLiteral("COLORTERM=truecolor") << QStringLiteral("TERM_PROGRAM=Edita");
    return env;
}

bool TerminalSession::start(const QString& program, const QStringList& arguments, const QString& directory, int columns, int rows)
{
    if (_pid > 0)
        return false;
    // Todo se prepara antes de fork(): en el hijo solo hay llamadas al sistema
    std::vector<QByteArray> argStore{QFile::encodeName(program)};
    for (const QString& argument : arguments)
        argStore.push_back(argument.toLocal8Bit());
    std::vector<char*> argv;
    for (QByteArray& a : argStore)
        argv.push_back(a.data());
    argv.push_back(nullptr);
    std::vector<QByteArray> envStore;
    for (const QString& entry : environmentFor(QProcess::systemEnvironment()))
        envStore.push_back(entry.toLocal8Bit());
    std::vector<char*> envp;
    for (QByteArray& e : envStore)
        envp.push_back(e.data());
    envp.push_back(nullptr);
    const QByteArray dir = QFile::encodeName(directory);
    char shell[] = "/bin/sh";
    char* fallback[] = {shell, nullptr};

    struct winsize size = {};
    size.ws_col = static_cast<unsigned short>(std::max(1, columns));
    size.ws_row = static_cast<unsigned short>(std::max(1, rows));
    int master = -1;
    const pid_t pid = forkpty(&master, nullptr, nullptr, &size);
    if (pid < 0)
        return false;
    if (pid == 0) {
        // Las señales como las espera una shell nueva (Qt bloquea o captura algunas)
        sigset_t none;
        sigemptyset(&none);
        sigprocmask(SIG_SETMASK, &none, nullptr);
        for (int s = 1; s < NSIG; ++s)
            signal(s, SIG_DFL);
        if (!dir.isEmpty() && chdir(dir.constData()) != 0 && chdir("/") != 0)
            _exit(126);
#ifdef __APPLE__
        environ = envp.data(); // macOS no tiene execvpe
        execvp(argv[0], argv.data());
#else
        execvpe(argv[0], argv.data(), envp.data());
#endif
        execve(fallback[0], fallback, envp.data());
        _exit(127);
    }
    _master = master;
    _pid = pid;
    _finished = false;
    fcntl(_master, F_SETFL, fcntl(_master, F_GETFL) | O_NONBLOCK);
    fcntl(_master, F_SETFD, FD_CLOEXEC);
    _readNotifier = new QSocketNotifier(static_cast<qintptr>(_master), QSocketNotifier::Read, this);
    connect(_readNotifier, &QSocketNotifier::activated, this, &TerminalSession::readAvailable);
    _writeNotifier = new QSocketNotifier(static_cast<qintptr>(_master), QSocketNotifier::Write, this);
    _writeNotifier->setEnabled(false);
    connect(_writeNotifier, &QSocketNotifier::activated, this, &TerminalSession::flushPending);
    return true;
}

QString TerminalSession::currentDirectory() const
{
    if (!isRunning())
        return {};
#ifdef __APPLE__
    struct proc_vnodepathinfo info = {};
    if (proc_pidinfo(static_cast<int>(_pid), PROC_PIDVNODEPATHINFO, 0, &info, static_cast<int>(sizeof(info))) <= 0)
        return {};
    return QString::fromUtf8(info.pvi_cdir.vip_path);
#else
    return QFileInfo(QStringLiteral("/proc/%1/cwd").arg(_pid)).symLinkTarget();
#endif
}

void TerminalSession::readAvailable()
{
    char buffer[65536];
    QByteArray data;
    bool ended = false;
    for (;;) {
        const ssize_t n = ::read(_master, buffer, sizeof(buffer));
        if (n > 0) {
            data.append(buffer, static_cast<qsizetype>(n));
            if (data.size() > 1024 * 1024) // que la interfaz respire con mucha salida
                break;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        ended = true; // 0 o EIO: ya no hay nadie al otro lado
        break;
    }
    if (!data.isEmpty())
        emit dataReceived(data);
    if (ended) {
        _readNotifier->setEnabled(false);
        _writeNotifier->setEnabled(false);
        reap();
    }
}

void TerminalSession::reap()
{
    if (_finished || _pid <= 0)
        return;
    int status = 0;
    const pid_t r = ::waitpid(static_cast<pid_t>(_pid), &status, WNOHANG);
    if (r == 0) { // cerró la terminal pero aún no terminó
        QTimer::singleShot(50, this, &TerminalSession::reap);
        return;
    }
    _finished = true;
    int code = -1;
    if (r > 0)
        code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    if (_master >= 0) {
        delete _readNotifier;
        delete _writeNotifier;
        _readNotifier = _writeNotifier = nullptr;
        ::close(_master);
        _master = -1;
    }
    emit finished(code);
}

void TerminalSession::write(const QByteArray& data)
{
    if (_master < 0 || _finished)
        return;
    _pending += data;
    flushPending();
}

void TerminalSession::flushPending()
{
    while (!_pending.isEmpty() && _master >= 0) {
        const ssize_t n = ::write(_master, _pending.constData(), static_cast<size_t>(_pending.size()));
        if (n > 0) {
            _pending.remove(0, static_cast<qsizetype>(n));
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            _writeNotifier->setEnabled(true);
            return;
        }
        _pending.clear(); // error: la shell ya no lee
    }
    if (_writeNotifier)
        _writeNotifier->setEnabled(false);
}

void TerminalSession::resize(int columns, int rows)
{
    if (_master < 0)
        return;
    struct winsize size = {};
    size.ws_col = static_cast<unsigned short>(std::max(1, columns));
    size.ws_row = static_cast<unsigned short>(std::max(1, rows));
    ::ioctl(_master, TIOCSWINSZ, &size); // el núcleo avisa con SIGWINCH al programa de delante
}

void TerminalSession::terminate()
{
    if (_master >= 0) {
        delete _readNotifier;
        delete _writeNotifier;
        _readNotifier = _writeNotifier = nullptr;
        ::close(_master); // cuelga la terminal: SIGHUP a lo que esté delante
        _master = -1;
    }
    if (_pid <= 0 || _finished)
        return;
    const auto pid = static_cast<pid_t>(_pid);
    ::kill(pid, SIGHUP);
    for (int i = 0; i < 20; ++i) {
        if (::waitpid(pid, nullptr, WNOHANG) != 0) {
            _finished = true;
            return;
        }
        ::usleep(10000);
    }
    ::kill(pid, SIGKILL);
    ::waitpid(pid, nullptr, 0);
    _finished = true;
}

// --- TerminalView ----------------------------------------------------------------------

TerminalView::TerminalView(QWidget* parent)
    : QAbstractScrollArea(parent)
{
    setObjectName(QStringLiteral("terminalView"));
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled);
    viewport()->setCursor(Qt::IBeamCursor);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    _vt = vterm_new(_rows, _columns);
    vterm_set_utf8(_vt, 1);
    _screen = vterm_obtain_screen(_vt);
    _state = vterm_obtain_state(_vt);
    static const VTermScreenCallbacks callbacks = {
        onDamage, onMoveRect, onMoveCursor, onSetTermProp, onBell, nullptr, onPushLine, onPopLine, onClearScrollback,
    };
    vterm_screen_set_callbacks(_screen, &callbacks, this);
    vterm_screen_set_damage_merge(_screen, VTERM_DAMAGE_SCROLL);
    vterm_screen_enable_altscreen(_screen, 1);
    vterm_screen_enable_reflow(_screen, true);
    vterm_output_set_callback(_vt, onOutput, this);
    vterm_state_set_bold_highbright(_state, 1);
    vterm_screen_reset(_screen, 1);

    setTerminalFont(desktopfonts::fixedFont());
    setColors(defaultColors(palette().color(QPalette::Text), palette().color(QPalette::Base)));
}

TerminalView::~TerminalView()
{
    if (_session) {
        _session->disconnect(this);
        _session->terminate();
    }
    vterm_free(_vt);
}

bool TerminalView::start(const QString& program, const QStringList& arguments, const QString& directory)
{
    if (_session)
        return false;
    updateGeometryFromSize();
    _session = new TerminalSession(this);
    connect(_session, &TerminalSession::dataReceived, this, &TerminalView::feed);
    connect(_session, &TerminalSession::finished, this, &TerminalView::finished);
    if (!_session->start(program, arguments, directory, _columns, _rows)) {
        delete _session;
        _session = nullptr;
        return false;
    }
    return true;
}

TerminalView::Colors TerminalView::defaultColors(const QColor& foreground, const QColor& background)
{
    // Las de la terminal de VS Code
    static const char* const kDark[16] = {"#000000", "#cd3131", "#0dbc79", "#e5e510", "#2472c8", "#bc3fbc", "#11a8cd", "#e5e5e5",
                                          "#666666", "#f14c4c", "#23d18b", "#f5f543", "#3b8eea", "#d670d6", "#29b8db", "#ffffff"};
    static const char* const kLight[16] = {"#000000", "#cd3131", "#00bc00", "#949800", "#0451a5", "#bc05bc", "#0598bc", "#555555",
                                           "#666666", "#cd3131", "#14ce14", "#b5ba00", "#0451a5", "#bc05bc", "#0598bc", "#a5a5a5"};
    const bool dark = background.lightness() < 128;
    Colors colors;
    colors.foreground = foreground;
    colors.background = background;
    colors.selection = dark ? QColor(0x26, 0x4f, 0x78) : QColor(0xad, 0xd6, 0xff);
    for (int i = 0; i < 16; ++i)
        colors.ansi[i] = QColor(QString::fromLatin1(dark ? kDark[i] : kLight[i]));
    return colors;
}

void TerminalView::setTerminalFont(const QFont& font)
{
    _font = font;
    _font.setStyleHint(QFont::TypeWriter);
    _font.setKerning(false);
    _font.setFixedPitch(true);
    for (int i = 0; i < 4; ++i) {
        _fonts[i] = _font;
        _fonts[i].setBold(i & 1);
        _fonts[i].setItalic(i & 2);
    }
    const QFontMetricsF metrics(_font);
    _cellWidth = std::max<qreal>(1, metrics.horizontalAdvance(QLatin1Char('M')));
    _cellHeight = std::max(1, static_cast<int>(std::ceil(metrics.height())));
    _ascent = static_cast<int>(std::round(metrics.ascent()));
    updateGeometryFromSize();
    viewport()->update();
}

void TerminalView::setColors(const Colors& colors)
{
    _colors = colors;
    applyPalette();
    viewport()->update();
}

void TerminalView::applyPalette()
{
    const VTermColor fg = vtermColor(_colors.foreground);
    const VTermColor bg = vtermColor(_colors.background);
    vterm_state_set_default_colors(_state, &fg, &bg);
    for (int i = 0; i < 16; ++i) {
        const VTermColor c = vtermColor(_colors.ansi[i]);
        vterm_state_set_palette_color(_state, i, &c);
    }
}

// --- Lo que llama libvterm --------------------------------------------------------------

int TerminalView::onDamage(VTermRect, void* user)
{
    static_cast<TerminalView*>(user)->viewport()->update();
    return 1;
}

int TerminalView::onMoveRect(VTermRect, VTermRect, void*)
{
    return 0; // que lo pinte como daño
}

int TerminalView::onMoveCursor(VTermPos pos, VTermPos, int, void* user)
{
    auto* self = static_cast<TerminalView*>(user);
    self->_cursor = pos;
    self->viewport()->update();
    return 1;
}

int TerminalView::onSetTermProp(VTermProp prop, VTermValue* value, void* user)
{
    auto* self = static_cast<TerminalView*>(user);
    switch (prop) {
    case VTERM_PROP_CURSORVISIBLE:
        self->_cursorVisible = value->boolean != 0;
        break;
    case VTERM_PROP_CURSORSHAPE:
        self->_cursorShape = value->number;
        break;
    case VTERM_PROP_ALTSCREEN:
        self->_altScreen = value->boolean != 0;
        self->clearSelection();
        self->updateScrollBar();
        self->scrollToBottom();
        break;
    case VTERM_PROP_MOUSE:
        self->_mouseMode = value->number;
        self->viewport()->setMouseTracking(value->number == VTERM_PROP_MOUSE_MOVE);
        break;
    case VTERM_PROP_TITLE:
        // El título llega por trozos (VTermStringFragment)
        if (value->string.initial)
            self->_titleBuffer.clear();
        self->_titleBuffer.append(value->string.str, static_cast<qsizetype>(value->string.len));
        if (value->string.final) {
            self->_title = QString::fromUtf8(self->_titleBuffer);
            emit self->titleChanged(self->_title);
        }
        break;
    default:
        break;
    }
    self->viewport()->update();
    return 1;
}

int TerminalView::onBell(void*)
{
    QApplication::beep();
    return 1;
}

int TerminalView::onPushLine(int cols, const VTermScreenCell* cells, void* user)
{
    auto* self = static_cast<TerminalView*>(user);
    self->_scrollback.emplace_back(cells, cells + cols);
    if (static_cast<int>(self->_scrollback.size()) > self->_maxScrollback)
        self->_scrollback.pop_front();
    if (self->_hasSelection && !self->_selecting) // las posiciones de la selección se mueven
        self->clearSelection();
    return 1;
}

int TerminalView::onPopLine(int cols, VTermScreenCell* cells, void* user)
{
    auto* self = static_cast<TerminalView*>(user);
    if (self->_scrollback.empty())
        return 0;
    const std::vector<VTermScreenCell> line = std::move(self->_scrollback.back());
    self->_scrollback.pop_back();
    VTermScreenCell blank = {};
    blank.width = 1;
    vterm_color_rgb(&blank.fg, 0, 0, 0);
    blank.fg.type |= VTERM_COLOR_DEFAULT_FG;
    vterm_color_rgb(&blank.bg, 0, 0, 0);
    blank.bg.type |= VTERM_COLOR_DEFAULT_BG;
    for (int i = 0; i < cols; ++i)
        cells[i] = i < static_cast<int>(line.size()) ? line[static_cast<std::size_t>(i)] : blank;
    return 1;
}

int TerminalView::onClearScrollback(void* user)
{
    auto* self = static_cast<TerminalView*>(user);
    self->_scrollback.clear();
    self->updateScrollBar();
    return 1;
}

void TerminalView::onOutput(const char* bytes, size_t length, void* user)
{
    auto* self = static_cast<TerminalView*>(user);
    if (self->_session)
        self->_session->write(QByteArray(bytes, static_cast<qsizetype>(length)));
}

// --- Entrada y salida --------------------------------------------------------------------

void TerminalView::feed(const QByteArray& bytes)
{
    vterm_input_write(_vt, bytes.constData(), static_cast<size_t>(bytes.size()));
    vterm_screen_flush_damage(_screen);
    updateScrollBar();
    viewport()->update();
}

void TerminalView::sendText(const QString& text)
{
    for (const char32_t c : text.toUcs4()) {
        if (c == U'\r')
            continue;
        if (c == U'\n')
            vterm_keyboard_key(_vt, VTERM_KEY_ENTER, VTERM_MOD_NONE);
        else
            vterm_keyboard_unichar(_vt, c, VTERM_MOD_NONE);
    }
    scrollToBottom();
}

void TerminalView::paste(const QString& text)
{
    if (text.isEmpty())
        return;
    QString normalized = text;
    normalized.replace(QLatin1String("\r\n"), QStringLiteral("\r"));
    normalized.replace(QLatin1Char('\n'), QLatin1Char('\r'));
    // Con «bracketed paste» activado por el programa, libvterm lo envuelve
    vterm_keyboard_start_paste(_vt);
    for (const char32_t c : normalized.toUcs4())
        vterm_keyboard_unichar(_vt, c, VTERM_MOD_NONE);
    vterm_keyboard_end_paste(_vt);
    scrollToBottom();
}

void TerminalView::updateGeometryFromSize()
{
    const int columns = std::max(2, static_cast<int>(viewport()->width() / _cellWidth));
    const int rows = std::max(1, viewport()->height() / _cellHeight);
    if (columns == _columns && rows == _rows)
        return;
    _columns = columns;
    _rows = rows;
    vterm_set_size(_vt, rows, columns); // con «reflow», las líneas se vuelven a partir
    vterm_screen_flush_damage(_screen);
    if (_session)
        _session->resize(columns, rows);
    clearSelection();
    updateScrollBar();
}

void TerminalView::updateScrollBar()
{
    QScrollBar* bar = verticalScrollBar();
    const bool atBottom = bar->value() == bar->maximum();
    const int maximum = _altScreen ? 0 : static_cast<int>(_scrollback.size());
    bar->setRange(0, maximum);
    bar->setPageStep(_rows);
    bar->setSingleStep(1);
    if (atBottom)
        bar->setValue(maximum);
}

void TerminalView::scrollToBottom()
{
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

int TerminalView::firstVisibleLine() const
{
    return verticalScrollBar()->value() - verticalScrollBar()->maximum();
}

void TerminalView::scrollContentsBy(int, int)
{
    viewport()->update();
}

void TerminalView::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateGeometryFromSize();
}

VTermScreenCell TerminalView::cellAt(int row, int column) const
{
    VTermScreenCell cell = {};
    vterm_screen_get_cell(_screen, VTermPos{row, column}, &cell);
    return cell;
}

VTermScreenCell TerminalView::cellAtLine(int line, int column) const
{
    if (line >= 0)
        return cellAt(line, column);
    const int index = static_cast<int>(_scrollback.size()) + line;
    if (index >= 0) {
        const auto& cells = _scrollback[static_cast<std::size_t>(index)];
        if (column < static_cast<int>(cells.size()))
            return cells[static_cast<std::size_t>(column)];
    }
    VTermScreenCell blank = {};
    blank.width = 1;
    blank.fg.type = VTERM_COLOR_DEFAULT_FG;
    blank.bg.type = VTERM_COLOR_DEFAULT_BG;
    return blank;
}

QColor TerminalView::cellForeground(const VTermScreenCell& cell) const
{
    VTermColor c = cell.fg;
    if (VTERM_COLOR_IS_DEFAULT_FG(&c))
        return _colors.foreground;
    if (VTERM_COLOR_IS_INDEXED(&c) && c.indexed.idx < 16)
        return _colors.ansi[c.indexed.idx];
    vterm_screen_convert_color_to_rgb(_screen, &c);
    return rgb(c);
}

QColor TerminalView::cellBackground(const VTermScreenCell& cell) const
{
    VTermColor c = cell.bg;
    if (VTERM_COLOR_IS_DEFAULT_BG(&c))
        return _colors.background;
    if (VTERM_COLOR_IS_INDEXED(&c) && c.indexed.idx < 16)
        return _colors.ansi[c.indexed.idx];
    vterm_screen_convert_color_to_rgb(_screen, &c);
    return rgb(c);
}

QString TerminalView::lineText(int line) const
{
    const int columns = line < 0 && static_cast<int>(_scrollback.size()) + line >= 0
        ? static_cast<int>(_scrollback[_scrollback.size() - static_cast<std::size_t>(-line)].size())
        : _columns;
    QString text;
    for (int col = 0; col < columns; ++col) {
        const VTermScreenCell cell = cellAtLine(line, col);
        if (cell.chars[0] == kContinuation)
            continue;
        const QString t = cellText(cell);
        text += t.isEmpty() ? QStringLiteral(" ") : t;
    }
    while (text.endsWith(QLatin1Char(' ')))
        text.chop(1);
    return text;
}

QString TerminalView::text() const
{
    QStringList lines;
    for (int line = -static_cast<int>(_scrollback.size()); line < _rows; ++line)
        lines << lineText(line);
    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    return lines.join(QLatin1Char('\n'));
}

// --- Selección --------------------------------------------------------------------------

TerminalView::Position TerminalView::positionAt(const QPoint& point) const
{
    Position pos;
    pos.line = firstVisibleLine() + std::clamp(point.y() / _cellHeight, 0, _rows - 1);
    pos.column = std::clamp(static_cast<int>(std::round(point.x() / _cellWidth)), 0, _columns);
    return pos;
}

bool TerminalView::isSelected(int line, int column) const
{
    if (!_hasSelection)
        return false;
    const Position start = std::min(_selectionAnchor, _selectionEnd);
    const Position end = std::max(_selectionAnchor, _selectionEnd);
    const Position here{line, column};
    return !(here < start) && here < end;
}

QString TerminalView::selectedText() const
{
    if (!_hasSelection)
        return {};
    const Position start = std::min(_selectionAnchor, _selectionEnd);
    const Position end = std::max(_selectionAnchor, _selectionEnd);
    QStringList lines;
    for (int line = start.line; line <= end.line; ++line) {
        const int from = line == start.line ? start.column : 0;
        const int to = line == end.line ? end.column : _columns;
        QString text;
        for (int col = from; col < to; ++col) {
            const VTermScreenCell cell = cellAtLine(line, col);
            if (cell.chars[0] == kContinuation)
                continue;
            const QString t = cellText(cell);
            text += t.isEmpty() ? QStringLiteral(" ") : t;
        }
        while (text.endsWith(QLatin1Char(' ')))
            text.chop(1);
        lines << text;
    }
    return lines.join(QLatin1Char('\n'));
}

void TerminalView::selectAll()
{
    _selectionAnchor = {-static_cast<int>(_scrollback.size()), 0};
    _selectionEnd = {_rows - 1, _columns};
    _hasSelection = true;
    viewport()->update();
}

void TerminalView::clearSelection()
{
    if (!_hasSelection)
        return;
    _hasSelection = false;
    viewport()->update();
}

void TerminalView::copy()
{
    if (_hasSelection)
        QApplication::clipboard()->setText(selectedText());
}

void TerminalView::clear()
{
    _scrollback.clear();
    clearSelection();
    if (_session && _session->isRunning())
        _session->write(QByteArrayLiteral("\x0c")); // Ctrl+L: la shell limpia y vuelve a escribir el prompt
    else
        feed(QByteArrayLiteral("\x1b[2J\x1b[H"));
    updateScrollBar();
    viewport()->update();
}

// --- Dibujo -----------------------------------------------------------------------------

void TerminalView::paintEvent(QPaintEvent*)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), _colors.background);
    const int first = firstVisibleLine();
    const bool focused = hasFocus();

    struct Run
    {
        int column = 0;
        int cells = 0;
        QString text;
        bool simple = true; // todo ASCII: se dibuja de una vez
        QColor fg, bg;
        int font = 0;
        int underline = 0;
        bool strike = false;
    };
    auto drawRun = [&](const Run& run, int y) {
        const qreal x = run.column * _cellWidth;
        const qreal width = run.cells * _cellWidth;
        if (run.bg != _colors.background)
            painter.fillRect(QRectF(x, y, width, _cellHeight), run.bg);
        painter.setFont(_fonts[run.font]);
        painter.setPen(run.fg);
        if (run.simple) {
            painter.drawText(QPointF(x, y + _ascent), run.text);
        } else {
            // Cada carácter en su columna (los anchos ocupan dos)
            int col = 0;
            for (const QString& piece : run.text.split(QChar(0x1F))) {
                if (!piece.isEmpty())
                    painter.drawText(QPointF(x + col * _cellWidth, y + _ascent), piece);
                col += 1;
            }
        }
        if (run.underline) {
            const qreal uy = y + _ascent + 2;
            painter.drawLine(QPointF(x, uy), QPointF(x + width, uy));
            if (run.underline == VTERM_UNDERLINE_DOUBLE)
                painter.drawLine(QPointF(x, uy + 2), QPointF(x + width, uy + 2));
        }
        if (run.strike) {
            const qreal sy = y + _ascent * 0.65;
            painter.drawLine(QPointF(x, sy), QPointF(x + width, sy));
        }
    };

    const int visibleRows = std::min(_rows, viewport()->height() / _cellHeight + 1);
    for (int row = 0; row < visibleRows; ++row) {
        const int line = first + row;
        if (line >= _rows)
            break;
        const int y = row * _cellHeight;
        Run run;
        bool open = false;
        for (int col = 0; col < _columns; ++col) {
            const VTermScreenCell cell = cellAtLine(line, col);
            if (cell.chars[0] == kContinuation)
                continue;
            QColor fg = cellForeground(cell);
            QColor bg = cellBackground(cell);
            if (cell.attrs.reverse)
                std::swap(fg, bg);
            if (isSelected(line, col))
                bg = _colors.selection;
            if (cell.attrs.conceal)
                fg = bg;
            const int font = (cell.attrs.bold ? 1 : 0) | (cell.attrs.italic ? 2 : 0);
            QString t = cellText(cell);
            if (t.isEmpty())
                t = QStringLiteral(" ");
            const bool simple = cell.width == 1 && t.size() == 1 && t[0].unicode() < 0x80;
            const bool same = open && fg == run.fg && bg == run.bg && font == run.font && cell.attrs.underline == run.underline
                && static_cast<bool>(cell.attrs.strike) == run.strike && simple == run.simple && cell.width == 1;
            if (!same) {
                if (open)
                    drawRun(run, y);
                run = Run{col, 0, QString(), simple, fg, bg, font, static_cast<int>(cell.attrs.underline), static_cast<bool>(cell.attrs.strike)};
                open = true;
            }
            if (!run.simple && !run.text.isEmpty())
                run.text += QChar(0x1F); // separa las celdas de un tramo no ASCII
            run.text += t;
            run.cells += cell.width;
            if (!run.simple && cell.width == 2)
                run.text += QChar(0x1F); // el carácter ancho ocupa también la siguiente columna
            if (cell.width == 2) { // los anchos, cada uno en su tramo
                drawRun(run, y);
                open = false;
            }
        }
        if (open)
            drawRun(run, y);
    }

    // El cursor: lleno con el foco; si no, solo el borde
    const int cursorRow = _cursor.row - first;
    if (_cursorVisible && cursorRow >= 0 && cursorRow < visibleRows) {
        const VTermScreenCell cell = cellAt(_cursor.row, _cursor.col);
        const qreal x = _cursor.col * _cellWidth;
        const int y = cursorRow * _cellHeight;
        const qreal width = _cellWidth * std::max(1, static_cast<int>(cell.width));
        const QColor color = _colors.foreground;
        if (_cursorShape == VTERM_PROP_CURSORSHAPE_UNDERLINE) {
            painter.fillRect(QRectF(x, y + _cellHeight - 2, width, 2), color);
        } else if (_cursorShape == VTERM_PROP_CURSORSHAPE_BAR_LEFT) {
            painter.fillRect(QRectF(x, y, 2, _cellHeight), color);
        } else if (focused) {
            painter.fillRect(QRectF(x, y, width, _cellHeight), color);
            const QString t = cellText(cell);
            if (!t.isEmpty() && t != QLatin1String(" ")) {
                painter.setFont(_fonts[(cell.attrs.bold ? 1 : 0) | (cell.attrs.italic ? 2 : 0)]);
                painter.setPen(_colors.background);
                painter.drawText(QPointF(x, y + _ascent), t);
            }
        } else {
            painter.setPen(color);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(x + 0.5, y + 0.5, width - 1, _cellHeight - 1));
        }
    }
}

// --- Teclado ----------------------------------------------------------------------------

VTermModifier TerminalView::modifiers(Qt::KeyboardModifiers mods) const
{
    int mod = VTERM_MOD_NONE;
    if (mods & Qt::ShiftModifier)
        mod |= VTERM_MOD_SHIFT;
    if (mods & Qt::AltModifier)
        mod |= VTERM_MOD_ALT;
    if (mods & kControlKey)
        mod |= VTERM_MOD_CTRL;
    return static_cast<VTermModifier>(mod);
}

bool TerminalView::event(QEvent* event)
{
    // Las teclas son de la terminal, no de los atajos de la ventana, salvo Ctrl+Mayús+…
    // (la paleta de órdenes, etc.), Ctrl+Tab y los de mostrar u ocultar la terminal
    if (event->type() == QEvent::ShortcutOverride) {
        auto* key = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers mods = key->modifiers();
        const int k = key->key();
#ifdef Q_OS_MACOS
        // Los atajos con ⌘ son de la ventana (salvo copiar y pegar); Control, de la shell
        const bool forWindow = (mods & Qt::ControlModifier) && !isCopyShortcut(mods, k) && !isPasteShortcut(mods, k);
#else
        const bool ctrl = mods & Qt::ControlModifier;
        const bool ctrlShift = ctrl && (mods & Qt::ShiftModifier);
        const bool forWindow = (ctrlShift && k != Qt::Key_C && k != Qt::Key_V)
            || (ctrl && (k == Qt::Key_QuoteLeft || k == Qt::Key_Ntilde || k == Qt::Key_Tab || k == Qt::Key_Backtab
                         || k == Qt::Key_PageUp || k == Qt::Key_PageDown));
#endif
        if (!forWindow) {
            event->accept();
            return true;
        }
    }
    return QAbstractScrollArea::event(event);
}

void TerminalView::keyPressEvent(QKeyEvent* event)
{
    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool ctrl = mods & kControlKey;
    const bool shift = mods & Qt::ShiftModifier;
    const int key = event->key();
    if (isCopyShortcut(mods, key)) {
        copy();
        return;
    }
    if (isPasteShortcut(mods, key)) {
        paste(QApplication::clipboard()->text());
        return;
    }
#ifdef Q_OS_MACOS
    if (mods & Qt::ControlModifier) { // ⌘ + otra tecla: no es texto para la shell
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
#endif
    // Mayús+RePág/AvPág recorre el historial
    if (shift && !ctrl && (key == Qt::Key_PageUp || key == Qt::Key_PageDown) && !_altScreen) {
        QScrollBar* bar = verticalScrollBar();
        bar->setValue(bar->value() + (key == Qt::Key_PageUp ? -1 : 1) * std::max(1, _rows - 1));
        return;
    }

    VTermModifier mod = modifiers(mods);
    VTermKey vk = VTERM_KEY_NONE;
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter: vk = VTERM_KEY_ENTER; break;
    case Qt::Key_Tab: vk = VTERM_KEY_TAB; break;
    case Qt::Key_Backtab:
        vk = VTERM_KEY_TAB;
        mod = static_cast<VTermModifier>(mod | VTERM_MOD_SHIFT);
        break;
    case Qt::Key_Backspace: vk = VTERM_KEY_BACKSPACE; break;
    case Qt::Key_Escape: vk = VTERM_KEY_ESCAPE; break;
    case Qt::Key_Up: vk = VTERM_KEY_UP; break;
    case Qt::Key_Down: vk = VTERM_KEY_DOWN; break;
    case Qt::Key_Left: vk = VTERM_KEY_LEFT; break;
    case Qt::Key_Right: vk = VTERM_KEY_RIGHT; break;
    case Qt::Key_Insert: vk = VTERM_KEY_INS; break;
    case Qt::Key_Delete: vk = VTERM_KEY_DEL; break;
    case Qt::Key_Home: vk = VTERM_KEY_HOME; break;
    case Qt::Key_End: vk = VTERM_KEY_END; break;
    case Qt::Key_PageUp: vk = VTERM_KEY_PAGEUP; break;
    case Qt::Key_PageDown: vk = VTERM_KEY_PAGEDOWN; break;
    default:
        if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
            vk = static_cast<VTermKey>(VTERM_KEY_FUNCTION(key - Qt::Key_F1 + 1));
        break;
    }
    if (vk != VTERM_KEY_NONE) {
        vterm_keyboard_key(_vt, vk, mod);
        scrollToBottom();
        return;
    }

    // Ctrl+letra y los demás controles por la tecla (el texto depende de la distribución)
    if (ctrl) {
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            vterm_keyboard_unichar(_vt, static_cast<uint32_t>('a' + (key - Qt::Key_A)), mod);
            scrollToBottom();
            return;
        }
        uint32_t c = 0;
        switch (key) {
        case Qt::Key_BracketLeft:
            if (_session)
                _session->write(QByteArrayLiteral("\x1b")); // Ctrl+[ es Esc
            scrollToBottom();
            return;
        case Qt::Key_Backslash: c = '\\'; break;
        case Qt::Key_BracketRight: c = ']'; break;
        case Qt::Key_AsciiCircum: c = '^'; break;
        case Qt::Key_Underscore:
        case Qt::Key_Minus: c = '_'; break;
        case Qt::Key_Space:
        case Qt::Key_At: c = ' '; break; // NUL
        default: break;
        }
        if (c) {
            vterm_keyboard_unichar(_vt, c, static_cast<VTermModifier>(mod & ~VTERM_MOD_SHIFT));
            scrollToBottom();
            return;
        }
    }

    const QString text = event->text();
    if (text.isEmpty()) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    for (const char32_t c : text.toUcs4()) {
        if (c < 0x20 || c == 0x7f) { // un control que ya trae el texto: tal cual
            if (_session)
                _session->write(QByteArray(1, static_cast<char>(c)));
        } else {
            vterm_keyboard_unichar(_vt, c, static_cast<VTermModifier>(mod & ~VTERM_MOD_CTRL));
        }
    }
    scrollToBottom();
}

void TerminalView::inputMethodEvent(QInputMethodEvent* event)
{
    for (const char32_t c : event->commitString().toUcs4())
        vterm_keyboard_unichar(_vt, c, VTERM_MOD_NONE);
    if (!event->commitString().isEmpty())
        scrollToBottom();
    event->accept();
}

QVariant TerminalView::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (query == Qt::ImCursorRectangle) {
        const int row = _cursor.row - firstVisibleLine();
        return QRect(static_cast<int>(_cursor.col * _cellWidth), row * _cellHeight, static_cast<int>(_cellWidth), _cellHeight);
    }
    if (query == Qt::ImFont)
        return _font;
    return QAbstractScrollArea::inputMethodQuery(query);
}

// --- Ratón ------------------------------------------------------------------------------

bool TerminalView::forwardMouse(QMouseEvent* event, bool pressed, bool released)
{
    // Con Mayús, el ratón vuelve a ser de la terminal (para seleccionar)
    if (!mouseReporting() || (event->modifiers() & Qt::ShiftModifier))
        return false;
    const QPoint p = event->position().toPoint();
    const int row = std::clamp(p.y() / _cellHeight, 0, _rows - 1);
    const int col = std::clamp(static_cast<int>(p.x() / _cellWidth), 0, _columns - 1);
    const VTermModifier mod = modifiers(event->modifiers());
    vterm_mouse_move(_vt, row, col, mod);
    int button = 0;
    switch (event->button()) {
    case Qt::LeftButton: button = 1; break;
    case Qt::MiddleButton: button = 2; break;
    case Qt::RightButton: button = 3; break;
    default: break;
    }
    if (button && (pressed || released)) {
        vterm_mouse_button(_vt, button, pressed, mod);
        if (pressed)
            _mouseButtons |= 1 << button;
        else
            _mouseButtons &= ~(1 << button);
    }
    return true;
}

void TerminalView::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    if (forwardMouse(event, true, false))
        return;
    if (event->button() == Qt::LeftButton) {
        _selecting = true;
        _hasSelection = false;
        _selectionAnchor = _selectionEnd = positionAt(event->position().toPoint());
        viewport()->update();
    } else if (event->button() == Qt::MiddleButton) {
        paste(QApplication::clipboard()->text(QClipboard::Selection));
    }
}

void TerminalView::mouseMoveEvent(QMouseEvent* event)
{
    if (forwardMouse(event, false, false))
        return;
    if (!_selecting)
        return;
    // Arrastrar por encima o por debajo desplaza el historial
    const QPoint p = event->position().toPoint();
    if (p.y() < 0)
        verticalScrollBar()->setValue(verticalScrollBar()->value() - 1);
    else if (p.y() > viewport()->height())
        verticalScrollBar()->setValue(verticalScrollBar()->value() + 1);
    _selectionEnd = positionAt(p);
    _hasSelection = !(_selectionEnd == _selectionAnchor);
    viewport()->update();
}

void TerminalView::mouseReleaseEvent(QMouseEvent* event)
{
    if (forwardMouse(event, false, true))
        return;
    if (_selecting && event->button() == Qt::LeftButton) {
        _selecting = false;
        if (_hasSelection) // la selección de X11: se pega con el botón central
            QApplication::clipboard()->setText(selectedText(), QClipboard::Selection);
    }
}

void TerminalView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (forwardMouse(event, true, false))
        return;
    // Doble clic: la palabra (o la ruta, la URL…)
    const Position pos = positionAt(event->position().toPoint());
    const QString text = lineText(pos.line);
    int column = std::min(static_cast<int>(event->position().x() / _cellWidth), static_cast<int>(text.size()) - 1);
    if (column < 0 || !isWordCharacter(text[column]))
        return;
    int start = column;
    int end = column + 1;
    while (start > 0 && isWordCharacter(text[start - 1]))
        --start;
    while (end < text.size() && isWordCharacter(text[end]))
        ++end;
    _selectionAnchor = {pos.line, start};
    _selectionEnd = {pos.line, end};
    _hasSelection = true;
    _selecting = false;
    QApplication::clipboard()->setText(selectedText(), QClipboard::Selection);
    viewport()->update();
}

void TerminalView::wheelEvent(QWheelEvent* event)
{
    const int steps = event->angleDelta().y() / 40; // tres líneas por paso de la rueda
    if (steps == 0)
        return;
    if (mouseReporting() && !(event->modifiers() & Qt::ShiftModifier)) {
        const QPoint p = event->position().toPoint();
        const int row = std::clamp(p.y() / _cellHeight, 0, _rows - 1);
        const int col = std::clamp(static_cast<int>(p.x() / _cellWidth), 0, _columns - 1);
        vterm_mouse_move(_vt, row, col, modifiers(event->modifiers()));
        for (int i = 0; i < std::abs(steps) / 3 + 1; ++i)
            vterm_mouse_button(_vt, steps > 0 ? 4 : 5, true, modifiers(event->modifiers()));
    } else if (_altScreen) {
        // Sin ratón, los programas a pantalla completa reciben flechas (less, man…)
        for (int i = 0; i < std::abs(steps); ++i)
            vterm_keyboard_key(_vt, steps > 0 ? VTERM_KEY_UP : VTERM_KEY_DOWN, VTERM_MOD_NONE);
    } else {
        verticalScrollBar()->setValue(verticalScrollBar()->value() - steps);
    }
    event->accept();
}

void TerminalView::focusInEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusInEvent(event);
    vterm_state_focus_in(_state);
    viewport()->update();
}

void TerminalView::focusOutEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusOutEvent(event);
    vterm_state_focus_out(_state);
    viewport()->update();
}

void TerminalView::contextMenuEvent(QContextMenuEvent* event)
{
    if (mouseReporting() && !(event->modifiers() & Qt::ShiftModifier))
        return;
    QMenu menu(this);
    QAction* copyAction = menu.addAction(tr("Copy"), copyKeys(), this, &TerminalView::copy);
    copyAction->setEnabled(_hasSelection);
    menu.addAction(tr("Paste"), pasteKeys(), this,
                   [this] { paste(QApplication::clipboard()->text()); });
    menu.addAction(tr("Select All"), this, &TerminalView::selectAll);
    menu.addSeparator();
    menu.addAction(tr("Clear"), this, &TerminalView::clear);
    menu.exec(event->globalPos());
}

} // namespace edita
