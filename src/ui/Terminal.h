// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Terminal integrada (no existe en Notepad++, que solo abre la carpeta en una consola
// externa). TerminalSession lanza la shell en un pseudoterminal (forkpty) y
// TerminalView la muestra: libvterm (third_party/libvterm) interpreta las secuencias
// de xterm y la vista dibuja la pantalla con la fuente y los colores del editor.
// Tiene historial con la rueda, selección y copiar/pegar (Ctrl+Mayús+C/V), ratón para
// los programas que lo piden (htop, mc, vim…) y la paleta ANSI del tema claro u oscuro.

#pragma once

#include <QAbstractScrollArea>
#include <QColor>
#include <QFont>
#include <QPointer>

#include <deque>
#include <vector>

#include <vterm.h>

class QSocketNotifier;

namespace edita {

// Un proceso en un pseudoterminal
class TerminalSession : public QObject
{
    Q_OBJECT

public:
    explicit TerminalSession(QObject* parent = nullptr);
    ~TerminalSession() override;

    // Lanza `program` (una shell) en `directory`. El entorno es el de Edita sin lo que
    // pone el AppImage (APPDIR, APPIMAGE…) y con TERM=xterm-256color
    bool start(const QString& program, const QStringList& arguments, const QString& directory, int columns, int rows);
    bool isRunning() const { return _pid > 0 && !_finished; }
    qint64 pid() const { return _pid; }
    // La carpeta en la que está ahora la shell (/proc/<pid>/cwd)
    QString currentDirectory() const;

    void write(const QByteArray& data);
    void resize(int columns, int rows);
    // Cuelga (SIGHUP) y espera un momento; si no termina, SIGKILL
    void terminate();

    // El entorno que recibe la shell (para las pruebas)
    static QStringList environmentFor(const QStringList& system);

signals:
    void dataReceived(const QByteArray& data);
    void finished(int exitCode);

private:
    void readAvailable();
    void flushPending();
    void reap();

    int _master = -1;
    qint64 _pid = 0;
    bool _finished = false;
    QSocketNotifier* _readNotifier = nullptr;
    QSocketNotifier* _writeNotifier = nullptr;
    QByteArray _pending;
};

class TerminalView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    struct Colors
    {
        QColor foreground;
        QColor background;
        QColor selection;
        QColor ansi[16];
    };

    explicit TerminalView(QWidget* parent = nullptr);
    ~TerminalView() override;

    // Lanza la shell; columnas y filas salen del tamaño de la vista
    bool start(const QString& program, const QStringList& arguments, const QString& directory);
    TerminalSession* session() const { return _session; }

    void setTerminalFont(const QFont& font);
    void setColors(const Colors& colors);
    // La paleta ANSI por defecto (la de VS Code) para el fondo dado
    static Colors defaultColors(const QColor& foreground, const QColor& background);

    // Lo que llega de la shell (público para las pruebas: se puede alimentar sin proceso)
    void feed(const QByteArray& bytes);
    // Lo que se escribe: a la shell como si se tecleara (los saltos de línea, como Intro)
    void sendText(const QString& text);
    void paste(const QString& text);

    QString title() const { return _title; }
    int columns() const { return _columns; }
    int rows() const { return _rows; }
    // El texto de una línea: negativas, el historial (−1 la más reciente); 0.., la pantalla
    QString lineText(int line) const;
    // Todo el texto: historial y pantalla, sin las líneas vacías del final
    QString text() const;
    int scrollbackLines() const { return static_cast<int>(_scrollback.size()); }
    QString selectedText() const;
    void selectAll();
    void clearSelection();
    void copy();
    void clear(); // el historial y la pantalla (como «clear» y el historial)
    VTermScreenCell cellAt(int row, int column) const; // de la pantalla
    QColor cellForeground(const VTermScreenCell& cell) const;
    QColor cellBackground(const VTermScreenCell& cell) const;
    bool mouseReporting() const { return _mouseMode != VTERM_PROP_MOUSE_NONE; }
    bool altScreen() const { return _altScreen; }

signals:
    void titleChanged(const QString& title);
    void finished(int exitCode);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    bool focusNextPrevChild(bool) override { return false; } // Tab es de la shell
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private:
    struct Position
    {
        int line = 0; // como lineText: negativas, el historial
        int column = 0;
        bool operator<(const Position& o) const { return line < o.line || (line == o.line && column < o.column); }
        bool operator==(const Position& o) const { return line == o.line && column == o.column; }
    };

    // Las funciones que llama libvterm
    static int onDamage(VTermRect rect, void* user);
    static int onMoveRect(VTermRect dest, VTermRect src, void* user);
    static int onMoveCursor(VTermPos pos, VTermPos oldPos, int visible, void* user);
    static int onSetTermProp(VTermProp prop, VTermValue* value, void* user);
    static int onBell(void* user);
    static int onPushLine(int cols, const VTermScreenCell* cells, void* user);
    static int onPopLine(int cols, VTermScreenCell* cells, void* user);
    static int onClearScrollback(void* user);
    static void onOutput(const char* bytes, size_t length, void* user);

    void updateGeometryFromSize();
    void updateScrollBar();
    void scrollToBottom();
    int firstVisibleLine() const; // en las líneas de lineText
    VTermScreenCell cellAtLine(int line, int column) const;
    Position positionAt(const QPoint& point) const;
    bool isSelected(int line, int column) const;
    void applyPalette();
    VTermModifier modifiers(Qt::KeyboardModifiers mods) const;
    bool forwardMouse(QMouseEvent* event, bool pressed, bool released);

    VTerm* _vt = nullptr;
    VTermScreen* _screen = nullptr;
    VTermState* _state = nullptr;
    TerminalSession* _session = nullptr;
    int _columns = 80;
    int _rows = 24;
    std::deque<std::vector<VTermScreenCell>> _scrollback;
    int _maxScrollback = 10000;
    VTermPos _cursor{0, 0};
    bool _cursorVisible = true;
    int _cursorShape = VTERM_PROP_CURSORSHAPE_BLOCK;
    bool _altScreen = false;
    int _mouseMode = VTERM_PROP_MOUSE_NONE;
    QString _title;
    QByteArray _titleBuffer;
    QFont _font;
    QFont _fonts[4]; // normal, negrita, cursiva, negrita cursiva
    qreal _cellWidth = 8;
    int _cellHeight = 16;
    int _ascent = 12;
    Colors _colors;
    bool _selecting = false;
    bool _hasSelection = false;
    Position _selectionAnchor;
    Position _selectionEnd;
    int _mouseButtons = 0; // los que se enviaron al programa (para soltarlos)
};

} // namespace edita
