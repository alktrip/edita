// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Las ventanas de Win32 para los plugins de Notepad++ (npp_compat_user.h): cada HWND de
// la capa es una Window con su QWidget, y los mensajes van por su procedimiento como en
// Windows. Los controles estándar son widgets de Qt; sus cambios llegan al padre como
// WM_COMMAND. Las ventanas de Edita (nppData._nppHandle, los editores) son QWidget*:
// sirven para medir y como padre (edita_compat_registerForeign).
//
// Se compila dentro de cada plugin con su modo: las cadenas de los mensajes (WM_SETTEXT,
// LB_ADDSTRING…) son TCHAR, como en una ventana de Windows de ese tipo.
//
// Las Window no se liberan nunca: un HWND destruido sigue siendo una dirección válida que
// IsWindow rechaza, y el plugin puede tocarla al descargarse (~StaticDialog).

#define EDITA_NPP_COMPAT_GLUE
#include "npp_compat.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QEvent>
#include <QEventLoop>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QScrollBar>
#include <QStyle>
#include <QTextBlock>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifndef EDITA_PLUGIN_VERSION
#define EDITA_PLUGIN_VERSION ""
#endif
#ifndef EDITA_PLUGIN_DISPLAY_NAME
#define EDITA_PLUGIN_DISPLAY_NAME "Plugin"
#endif
#ifndef EDITA_PLUGIN_HOMEPAGE
#define EDITA_PLUGIN_HOMEPAGE ""
#endif

namespace {

using tstring = std::basic_string<TCHAR>;

// --- Cadenas -----------------------------------------------------------------------------

QString fromT(const TCHAR* s)
{
    if (!s)
        return {};
#ifdef UNICODE
    return QString::fromWCharArray(s);
#else
    return QString::fromUtf8(s);
#endif
}

tstring toT(const QString& s)
{
#ifdef UNICODE
    return s.toStdWString();
#else
    return s.toStdString();
#endif
}

QString fromA(const char* s) { return s ? QString::fromUtf8(s) : QString(); }
QString fromW(const wchar_t* s) { return s ? QString::fromWCharArray(s) : QString(); }

// Un nombre de recurso o de clase: un número (MAKEINTRESOURCE) o una cadena
template<typename Ch>
QString resourceName(const Ch* name, QString (*convert)(const Ch*))
{
    if (IS_INTRESOURCE(name))
        return QStringLiteral("#%1").arg(static_cast<unsigned>(reinterpret_cast<uintptr_t>(name)));
    return convert(name);
}

// Copia al búfer de una ventana (WM_GETTEXT, GetWindowText): como mucho size - 1
int copyText(const QString& text, TCHAR* buffer, int size)
{
    if (!buffer || size <= 0)
        return 0;
    const tstring converted = toT(text);
    const std::size_t n = std::min(converted.size(), static_cast<std::size_t>(size - 1));
    std::copy_n(converted.data(), n, buffer);
    buffer[n] = TCHAR(0);
    return static_cast<int>(n);
}

template<typename Ch>
int copyTo(const std::basic_string<Ch>& text, Ch* buffer, int size)
{
    if (!buffer || size <= 0)
        return 0;
    const std::size_t n = std::min(text.size(), static_cast<std::size_t>(size - 1));
    std::copy_n(text.data(), n, buffer);
    buffer[n] = Ch(0);
    return static_cast<int>(n);
}

bool spanish()
{
    return QLocale().language() == QLocale::Spanish;
}

// --- Ventanas ------------------------------------------------------------------------------

constexpr uint32_t kWindowMagic = 0x48574E44; // "HWND"

enum class Kind { Generic, Dialog, Button, Edit, Static, ListBox, ComboBox, Custom };

struct WindowClass
{
    QString name;
    WNDPROC proc = nullptr;
    int extraBytes = 0;
    UINT style = 0;
    ATOM atom = 0;
};

struct Window
{
    uint32_t magic = kWindowMagic;
    bool alive = true;
    bool created = false;       // ya recibió WM_CREATE / WM_INITDIALOG
    Kind kind = Kind::Generic;
    QPointer<QWidget> widget;
    WNDPROC proc = nullptr;     // la cadena de procedimientos (subclassing)
    WNDPROC builtin = nullptr;  // el de la clase
    DLGPROC dlgProc = nullptr;
    HWND parent = nullptr;      // padre (WS_CHILD) o dueño
    HINSTANCE instance = nullptr;
    int id = 0;
    DWORD style = 0;
    DWORD exStyle = 0;
    QString className;
    QString text;               // el texto que dio el plugin (los estáticos muestran otro, sin '&')
    LONG_PTR userData = 0;
    LONG_PTR dialogUser = 0;
    LONG_PTR messageResult = 0;
    std::map<int, LONG_PTR> extra;
    std::map<QString, HANDLE> properties;
    std::vector<Window*> children;
    std::map<UINT_PTR, QTimer*> timers;
    HFONT font = nullptr;
    QLabel* placeholder = nullptr;  // diálogos sin plantilla
    QEventLoop* modalLoop = nullptr;
    INT_PTR modalResult = 0;
    bool keyReached = false;    // la cadena llegó al procedimiento de la clase
};

struct Foreign
{
    QPointer<QWidget> widget;
    HWND parent = nullptr;
    int id = 0;
};

struct Registry
{
    std::set<Window*> alive;
    std::vector<WindowClass> classes;
    std::map<void*, Foreign> foreign;   // las ventanas de Edita
    std::map<UINT_PTR, QTimer*> threadTimers;
    UINT_PTR nextTimer = 0x7F00;
    HWND capture = nullptr;
    int cursorOverrides = 0;
};

// Nunca se destruye: el plugin la usa también desde sus destructores estáticos
Registry& registry()
{
    static auto* instance = new Registry;
    return *instance;
}

char g_desktop = 0;
HWND desktopHwnd() { return reinterpret_cast<HWND>(&g_desktop); }

Window* windowOf(HWND hwnd)
{
    auto* w = reinterpret_cast<Window*>(hwnd);
    if (!hwnd || !registry().alive.count(w))
        return nullptr;
    return w;
}

HWND hwndOf(Window* w)
{
    return reinterpret_cast<HWND>(w);
}

const char* const kHwndProperty = "edita_compat_hwnd";

// El QWidget de un HWND de la capa o de Edita
QWidget* widgetOf(HWND hwnd)
{
    if (Window* w = windowOf(hwnd))
        return w->widget;
    auto& foreign = registry().foreign;
    const auto it = foreign.find(hwnd);
    return it != foreign.end() ? it->second.widget.data() : nullptr;
}

// El HWND de un QWidget (o del primero de sus padres que tenga uno)
HWND hwndOfWidget(QWidget* widget)
{
    for (QWidget* w = widget; w; w = w->parentWidget()) {
        const QVariant value = w->property(kHwndProperty);
        if (value.isValid()) {
            HWND hwnd = reinterpret_cast<HWND>(value.value<quintptr>());
            if (windowOf(hwnd))
                return hwnd;
        }
        if (registry().foreign.count(w))
            return reinterpret_cast<HWND>(w);
    }
    return nullptr;
}

LRESULT send(Window* w, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!w || !w->alive)
        return 0;
    HWND hwnd = hwndOf(w);
    return w->proc ? w->proc(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

void notifyParent(Window* w, WORD code)
{
    Window* parent = windowOf(w->parent);
    if (parent && (w->style & WS_CHILD))
        send(parent, WM_COMMAND, MAKEWPARAM(static_cast<WORD>(w->id), code), reinterpret_cast<LPARAM>(hwndOf(w)));
}

Window* dialogAncestor(Window* w)
{
    for (Window* p = w; p; p = windowOf(p->parent)) {
        if (p->kind == Kind::Dialog)
            return p;
        if (!(p->style & WS_CHILD))
            break;
    }
    return nullptr;
}

// --- Texto de cada control ------------------------------------------------------------------

// Los estáticos muestran "&Nombre" como "Nombre" (SS_NOPREFIX: tal cual)
QString withoutMnemonic(const QString& text)
{
    QString out;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text[i] == QLatin1Char('&') && i + 1 < text.size()) {
            ++i;
        }
        out += text[i];
    }
    return out;
}

QString crlfToLf(QString text)
{
    return text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
}

QString lfToCrlf(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    return text.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
}

QString windowText(Window* w)
{
    QWidget* widget = w->widget;
    if (!widget)
        return w->text;
    switch (w->kind) {
    case Kind::Button:
        if (auto* group = qobject_cast<QGroupBox*>(widget))
            return group->title();
        return static_cast<QAbstractButton*>(widget)->text();
    case Kind::Edit:
        if (auto* line = qobject_cast<QLineEdit*>(widget))
            return line->text();
        return lfToCrlf(static_cast<QPlainTextEdit*>(widget)->toPlainText());
    case Kind::ComboBox:
        return static_cast<QComboBox*>(widget)->currentText();
    case Kind::Static:
        return w->text;
    default:
        return widget->isWindow() ? widget->windowTitle() : w->text;
    }
}

void setWindowText(Window* w, const QString& text)
{
    w->text = text;
    QWidget* widget = w->widget;
    if (!widget)
        return;
    switch (w->kind) {
    case Kind::Button:
        if (auto* group = qobject_cast<QGroupBox*>(widget))
            group->setTitle(text);
        else
            static_cast<QAbstractButton*>(widget)->setText(text);
        break;
    case Kind::Edit:
        if (auto* line = qobject_cast<QLineEdit*>(widget)) {
            line->setText(text); // EN_CHANGE, como en Windows
        } else {
            // En los multilínea, WM_SETTEXT no manda EN_CHANGE
            const QSignalBlocker blocker(widget);
            static_cast<QPlainTextEdit*>(widget)->setPlainText(crlfToLf(text));
        }
        break;
    case Kind::ComboBox: {
        auto* combo = static_cast<QComboBox*>(widget);
        const QSignalBlocker blocker(combo);
        if (combo->isEditable())
            combo->setEditText(text);
        break;
    }
    case Kind::Static:
        if (auto* label = qobject_cast<QLabel*>(widget))
            label->setText((w->style & SS_NOPREFIX) ? text : withoutMnemonic(text));
        break;
    default:
        if (widget->isWindow())
            widget->setWindowTitle(text);
        break;
    }
}

// --- Fuentes (GDI sin dibujo) --------------------------------------------------------------

constexpr uint32_t kGdiMagic = 0x47444930; // "GDI0"

struct GdiObject
{
    uint32_t magic = kGdiMagic;
    enum class Type { Font, Brush, Pen } type = Type::Font;
    bool stock = false;
    QFont font;
    COLORREF color = 0;
    LOGFONTW logFont{};
};

struct DeviceContext
{
    uint32_t magic = 0x48444330; // "HDC0"
    QPointer<QWidget> widget;
    HGDIOBJ font = nullptr;
    COLORREF textColor = 0;
    COLORREF backgroundColor = 0xFFFFFF;
    int backgroundMode = OPAQUE;
};

GdiObject* gdiObject(HGDIOBJ object)
{
    auto* o = static_cast<GdiObject*>(object);
    return o && o->magic == kGdiMagic ? o : nullptr;
}

DeviceContext* deviceContext(HDC dc)
{
    auto* d = static_cast<DeviceContext*>(dc);
    return d && d->magic == 0x48444330 ? d : nullptr;
}

QFont fontOf(HGDIOBJ object)
{
    const GdiObject* o = gdiObject(object);
    return o && o->type == GdiObject::Type::Font ? o->font : QApplication::font();
}

LOGFONTW logFontOf(const QFont& font)
{
    LOGFONTW lf{};
    const QFontMetrics metrics(font);
    lf.lfHeight = -(font.pixelSize() > 0 ? font.pixelSize() : metrics.ascent());
    lf.lfWeight = font.bold() ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = font.italic() ? 1 : 0;
    lf.lfUnderline = font.underline() ? 1 : 0;
    lf.lfStrikeOut = font.strikeOut() ? 1 : 0;
    lf.lfCharSet = DEFAULT_CHARSET;
    const std::wstring face = font.family().toStdWString();
    copyTo(face, lf.lfFaceName, LF_FACESIZE);
    return lf;
}

QFont fontFromLogFont(LONG height, LONG weight, bool italic, bool underline, bool strikeOut, const QString& face)
{
    QFont font = QApplication::font();
    if (!face.isEmpty())
        font.setFamily(face);
    // Negativa: la altura de los caracteres; positiva: la de la celda (con el interlineado)
    if (height < 0)
        font.setPixelSize(static_cast<int>(-height));
    else if (height > 0)
        font.setPixelSize(std::max(1, static_cast<int>(height * 4 / 5)));
    if (weight > 0)
        font.setWeight(static_cast<QFont::Weight>(std::clamp(static_cast<int>(weight), 100, 900)));
    font.setItalic(italic);
    font.setUnderline(underline);
    font.setStrikeOut(strikeOut);
    return font;
}

HFONT makeFont(const QFont& font, const LOGFONTW& logFont)
{
    auto* o = new GdiObject;
    o->type = GdiObject::Type::Font;
    o->font = font;
    o->logFont = logFont;
    return o;
}

// --- Controles: widgets de Qt y sus notificaciones --------------------------------------

LRESULT CALLBACK controlProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK dialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

// Lo que el plugin ve del teclado y el ratón de los widgets
class EventForwarder : public QObject
{
public:
    bool eventFilter(QObject* object, QEvent* event) override;
};

EventForwarder* forwarder()
{
    static auto* instance = new EventForwarder;
    return instance;
}

// El widget de las ventanas que no son controles: diálogos, clases registradas, genéricas
class HostWidget : public QWidget
{
public:
    explicit HostWidget(QWidget* parent, Qt::WindowFlags flags = {}) : QWidget(parent, flags) {}
    Window* window = nullptr;
    QSize preferred{320, 240};

    QSize sizeHint() const override { return preferred; }

protected:
    bool event(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
};

DWORD virtualKey(const QKeyEvent* key)
{
    const int k = key->key();
    if (k >= Qt::Key_A && k <= Qt::Key_Z)
        return static_cast<DWORD>('A' + (k - Qt::Key_A));
    if (k >= Qt::Key_0 && k <= Qt::Key_9)
        return static_cast<DWORD>('0' + (k - Qt::Key_0));
    if (k >= Qt::Key_F1 && k <= Qt::Key_F12)
        return static_cast<DWORD>(VK_F1 + (k - Qt::Key_F1));
    switch (k) {
    case Qt::Key_Return:
    case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Tab:
    case Qt::Key_Backtab: return VK_TAB;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Shift: return VK_SHIFT;
    case Qt::Key_Control: return VK_CONTROL;
    case Qt::Key_Alt: return VK_MENU;
    default: return 0;
    }
}

LPARAM pointParam(const QPoint& p)
{
    return MAKELPARAM(static_cast<WORD>(static_cast<short>(p.x())), static_cast<WORD>(static_cast<short>(p.y())));
}

WPARAM mouseKeys(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    WPARAM keys = 0;
    if (buttons & Qt::LeftButton)
        keys |= MK_LBUTTON;
    if (buttons & Qt::RightButton)
        keys |= MK_RBUTTON;
    if (buttons & Qt::MiddleButton)
        keys |= MK_MBUTTON;
    if (modifiers & Qt::ShiftModifier)
        keys |= MK_SHIFT;
    if (modifiers & Qt::ControlModifier)
        keys |= MK_CONTROL;
    return keys;
}

// Intro en un diálogo: el botón por defecto (BS_DEFPUSHBUTTON) o IDOK
void sendDefaultCommand(Window* dialog)
{
    int id = IDOK;
    HWND button = nullptr;
    for (Window* child : dialog->children) {
        if (child->alive && child->kind == Kind::Button && (child->style & BS_TYPEMASK) == BS_DEFPUSHBUTTON) {
            id = child->id;
            button = hwndOf(child);
            break;
        }
    }
    send(dialog, WM_COMMAND, MAKEWPARAM(static_cast<WORD>(id), BN_CLICKED), reinterpret_cast<LPARAM>(button));
}

// Teclado: si el plugin subclasificó el control, o es una ventana suya, recibe WM_KEYDOWN y
// WM_CHAR; si no los deja pasar al procedimiento de la clase, Qt no los procesa
bool forwardKey(Window* w, QKeyEvent* key, bool press)
{
    const bool own = w->kind == Kind::Custom || w->kind == Kind::Generic;
    if (!own && w->proc == w->builtin)
        return false;
    const DWORD vk = virtualKey(key);
    w->keyReached = false;
    bool reached = true;
    if (vk) {
        send(w, press ? WM_KEYDOWN : WM_KEYUP, vk, 1);
        reached = w->keyReached;
    }
    if (press && !key->text().isEmpty() && key->text()[0].isPrint()) {
        w->keyReached = false;
        send(w, WM_CHAR, key->text()[0].unicode(), 1);
        reached = reached && w->keyReached;
    }
    return !reached;
}

bool EventForwarder::eventFilter(QObject* object, QEvent* event)
{
    auto* widget = qobject_cast<QWidget*>(object);
    if (!widget)
        return false;
    Window* w = windowOf(reinterpret_cast<HWND>(widget->property(kHwndProperty).value<quintptr>()));
    if (!w || !w->alive)
        return false;
    switch (event->type()) {
    case QEvent::KeyPress: {
        auto* key = static_cast<QKeyEvent*>(event);
        if (forwardKey(w, key, true))
            return true;
        Window* dialog = dialogAncestor(w);
        if (!dialog || dialog == w)
            return false;
        const bool multiline = w->kind == Kind::Edit && (w->style & ES_MULTILINE);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && !(multiline && (w->style & ES_WANTRETURN))) {
            if (auto* button = qobject_cast<QPushButton*>(widget)) {
                button->click();
                return true;
            }
            if (auto* combo = qobject_cast<QComboBox*>(widget); combo && combo->view()->isVisible())
                return false;
            sendDefaultCommand(dialog);
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            send(dialog, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
            return true;
        }
        return false;
    }
    case QEvent::KeyRelease:
        return forwardKey(w, static_cast<QKeyEvent*>(event), false);
    case QEvent::ContextMenu: {
        // WM_CONTEXTMENU al control; si no lo trata, DefWindowProc lo pasa al padre
        auto* menu = static_cast<QContextMenuEvent*>(event);
        w->keyReached = false;
        send(w, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(hwndOf(w)), pointParam(menu->globalPos()));
        return true;
    }
    case QEvent::MouseButtonPress:
        if (w->kind == Kind::Static && (w->style & SS_NOTIFY))
            notifyParent(w, STN_CLICKED);
        return false;
    case QEvent::MouseButtonDblClick:
        if (w->kind == Kind::Static && (w->style & SS_NOTIFY))
            notifyParent(w, STN_DBLCLK);
        return false;
    default:
        return false;
    }
}

bool HostWidget::event(QEvent* event)
{
    Window* w = window;
    if (!w || !w->alive || !w->created)
        return QWidget::event(event);
    switch (event->type()) {
    case QEvent::Close:
        // Como en Windows: WM_CLOSE (DefDlgProc lo convierte en IDCANCEL); la ventana se
        // cierra solo si el plugin la destruye o la oculta
        send(w, WM_CLOSE, 0, 0);
        event->ignore();
        return true;
    case QEvent::Move:
        send(w, WM_MOVE, 0, pointParam(pos()));
        break;
    case QEvent::Show:
        send(w, WM_SHOWWINDOW, TRUE, 0);
        break;
    case QEvent::Hide:
        send(w, WM_SHOWWINDOW, FALSE, 0);
        break;
    case QEvent::FocusIn:
        send(w, WM_SETFOCUS, 0, 0);
        break;
    case QEvent::FocusOut:
        send(w, WM_KILLFOCUS, 0, 0);
        break;
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
        auto* key = static_cast<QKeyEvent*>(event);
        if (w->kind == Kind::Dialog) {
            if (key->type() == QEvent::KeyPress && key->key() == Qt::Key_Escape) {
                send(w, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
                return true;
            }
            if (key->type() == QEvent::KeyPress && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
                sendDefaultCommand(w);
                return true;
            }
            break;
        }
        if (forwardKey(w, key, event->type() == QEvent::KeyPress))
            return true;
        break;
    }
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const bool press = event->type() != QEvent::MouseButtonRelease;
        const bool doubleClick = event->type() == QEvent::MouseButtonDblClick;
        UINT message = 0;
        switch (mouse->button()) {
        case Qt::LeftButton: message = doubleClick ? WM_LBUTTONDBLCLK : press ? WM_LBUTTONDOWN : WM_LBUTTONUP; break;
        case Qt::RightButton: message = doubleClick ? WM_RBUTTONDBLCLK : press ? WM_RBUTTONDOWN : WM_RBUTTONUP; break;
        case Qt::MiddleButton: message = press ? WM_MBUTTONDOWN : WM_MBUTTONUP; break;
        default: break;
        }
        if (message)
            send(w, message, mouseKeys(mouse->buttons(), mouse->modifiers()), pointParam(mouse->position().toPoint()));
        break;
    }
    case QEvent::MouseMove: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        send(w, WM_MOUSEMOVE, mouseKeys(mouse->buttons(), mouse->modifiers()), pointParam(mouse->position().toPoint()));
        break;
    }
    case QEvent::Wheel: {
        auto* wheel = static_cast<QWheelEvent*>(event);
        const auto delta = static_cast<short>(wheel->angleDelta().y());
        send(w, WM_MOUSEWHEEL, MAKEWPARAM(static_cast<WORD>(mouseKeys(wheel->buttons(), wheel->modifiers())), static_cast<WORD>(delta)),
             pointParam(wheel->globalPosition().toPoint()));
        break;
    }
    case QEvent::ContextMenu: {
        auto* menu = static_cast<QContextMenuEvent*>(event);
        send(w, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(hwndOf(w)), pointParam(menu->globalPos()));
        return true;
    }
    default:
        break;
    }
    return QWidget::event(event);
}

void HostWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    Window* w = window;
    if (!w || !w->alive)
        return;
    if (w->placeholder)
        w->placeholder->setGeometry(rect().adjusted(12, 12, -12, -12));
    if (w->created)
        send(w, WM_SIZE, SIZE_RESTORED, MAKELPARAM(static_cast<WORD>(width()), static_cast<WORD>(height())));
}

// --- Listas y combos -----------------------------------------------------------------------

int insertSorted(QStringList items, const QString& text)
{
    const auto it = std::lower_bound(items.begin(), items.end(), text,
                                     [](const QString& a, const QString& b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
    return static_cast<int>(it - items.begin());
}

QStringList listItems(QListWidget* list)
{
    QStringList items;
    for (int i = 0; i < list->count(); ++i)
        items << list->item(i)->text();
    return items;
}

QStringList comboItems(QComboBox* combo)
{
    QStringList items;
    for (int i = 0; i < combo->count(); ++i)
        items << combo->itemText(i);
    return items;
}

// LB_FINDSTRING / CB_FINDSTRING: desde el siguiente a `start`, dando la vuelta;
// sin distinguir mayúsculas, por el principio (exact: entero)
int findItem(const QStringList& items, int start, const QString& text, bool exact)
{
    const int count = static_cast<int>(items.size());
    if (count == 0)
        return -1;
    const int first = (start < 0 || start >= count) ? 0 : start + 1;
    for (int n = 0; n < count; ++n) {
        const int i = (first + n) % count;
        const bool match = exact ? items[i].compare(text, Qt::CaseInsensitive) == 0
                                 : items[i].startsWith(text, Qt::CaseInsensitive);
        if (match)
            return i;
    }
    return -1;
}

// Posiciones de un edit multilínea: Windows cuenta "\r\n" como dos caracteres
int qtPosition(const QString& text, LPARAM win)
{
    if (win < 0)
        return static_cast<int>(text.size());
    LPARAM count = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (count >= win)
            return static_cast<int>(i);
        count += text[i] == QLatin1Char('\n') ? 2 : 1;
    }
    return static_cast<int>(text.size());
}

LRESULT winPosition(const QString& text, int qt)
{
    LRESULT count = 0;
    for (qsizetype i = 0; i < qt && i < text.size(); ++i)
        count += text[i] == QLatin1Char('\n') ? 2 : 1;
    return count;
}

LRESULT editMessage(Window* w, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* line = qobject_cast<QLineEdit*>(w->widget);
    auto* multi = qobject_cast<QPlainTextEdit*>(w->widget);
    switch (message) {
    case EM_GETSEL: {
        int start = 0, end = 0;
        if (line) {
            start = line->hasSelectedText() ? line->selectionStart() : line->cursorPosition();
            end = line->hasSelectedText() ? line->selectionEnd() : start;
        } else if (multi) {
            const QTextCursor c = multi->textCursor();
            const QString text = multi->toPlainText();
            start = static_cast<int>(winPosition(text, c.selectionStart()));
            end = static_cast<int>(winPosition(text, c.selectionEnd()));
        }
        if (auto* out = reinterpret_cast<DWORD*>(wParam))
            *out = static_cast<DWORD>(start);
        if (auto* out = reinterpret_cast<DWORD*>(lParam))
            *out = static_cast<DWORD>(end);
        return MAKELONG(static_cast<WORD>(start), static_cast<WORD>(end));
    }
    case EM_SETSEL: {
        const auto start = static_cast<intptr_t>(wParam);
        if (line) {
            const int length = static_cast<int>(line->text().size());
            if (start < 0) {
                line->deselect();
                return 0;
            }
            const int from = static_cast<int>(std::min<intptr_t>(start, length));
            const int to = lParam < 0 ? length : static_cast<int>(std::min<intptr_t>(lParam, length));
            line->setSelection(from, to - from);
        } else if (multi) {
            const QString text = multi->toPlainText();
            QTextCursor c = multi->textCursor();
            if (start < 0) {
                c.clearSelection();
            } else {
                c.setPosition(qtPosition(text, start));
                c.setPosition(qtPosition(text, lParam), QTextCursor::KeepAnchor);
            }
            multi->setTextCursor(c);
        }
        return 0;
    }
    case EM_REPLACESEL: {
        const QString text = fromT(reinterpret_cast<const TCHAR*>(lParam));
        if (line)
            line->insert(text);
        else if (multi)
            multi->insertPlainText(crlfToLf(text));
        return 0;
    }
    case EM_LIMITTEXT:
        if (line)
            line->setMaxLength(wParam ? static_cast<int>(wParam) : 32767);
        return 0;
    case EM_GETLIMITTEXT:
        return line ? line->maxLength() : 0x7FFFFFFE;
    case EM_SETREADONLY:
        if (line)
            line->setReadOnly(wParam != 0);
        else if (multi)
            multi->setReadOnly(wParam != 0);
        if (wParam)
            w->style |= ES_READONLY;
        else
            w->style &= ~static_cast<DWORD>(ES_READONLY);
        return TRUE;
    case EM_GETMODIFY:
        return line ? line->isModified() : multi && multi->document()->isModified();
    case EM_SETMODIFY:
        if (line)
            line->setModified(wParam != 0);
        else if (multi)
            multi->document()->setModified(wParam != 0);
        return 0;
    case EM_GETLINECOUNT:
        return multi ? multi->document()->blockCount() : 1;
    case EM_LINEINDEX: {
        if (!multi)
            return 0;
        QTextDocument* doc = multi->document();
        const int index = static_cast<intptr_t>(wParam) < 0 ? multi->textCursor().blockNumber() : static_cast<int>(wParam);
        const QTextBlock block = doc->findBlockByNumber(index);
        return block.isValid() ? winPosition(multi->toPlainText(), block.position()) : -1;
    }
    case EM_LINEFROMCHAR: {
        if (!multi)
            return 0;
        const QString text = multi->toPlainText();
        const int position = static_cast<intptr_t>(wParam) < 0 ? multi->textCursor().position() : qtPosition(text, static_cast<LPARAM>(wParam));
        return multi->document()->findBlock(position).blockNumber();
    }
    case EM_LINELENGTH: {
        if (line)
            return line->text().size();
        if (!multi)
            return 0;
        const QString text = multi->toPlainText();
        return multi->document()->findBlock(qtPosition(text, static_cast<LPARAM>(wParam))).length() - 1;
    }
    case EM_GETLINE: {
        // El primer WORD del búfer es su tamaño; sin nulo al final
        auto* buffer = reinterpret_cast<TCHAR*>(lParam);
        if (!buffer)
            return 0;
        WORD size = 0;
        std::memcpy(&size, buffer, sizeof(WORD));
        const QString text = line ? line->text() : multi ? multi->document()->findBlockByNumber(static_cast<int>(wParam)).text() : QString();
        const tstring converted = toT(text);
        const std::size_t n = std::min<std::size_t>(converted.size(), size);
        std::copy_n(converted.data(), n, buffer);
        return static_cast<LRESULT>(n);
    }
    case EM_SCROLLCARET:
        if (multi)
            multi->ensureCursorVisible();
        return TRUE;
    case EM_CANUNDO:
        return line ? line->isUndoAvailable() : multi && multi->document()->isUndoAvailable();
    case EM_UNDO:
        if (line)
            line->undo();
        else if (multi)
            multi->undo();
        return TRUE;
    case EM_EMPTYUNDOBUFFER:
        if (multi)
            multi->document()->clearUndoRedoStacks();
        return 0;
    case EM_SETCUEBANNER: // siempre LPCWSTR
        if (line)
            line->setPlaceholderText(fromW(reinterpret_cast<const wchar_t*>(lParam)));
        else if (multi)
            multi->setPlaceholderText(fromW(reinterpret_cast<const wchar_t*>(lParam)));
        return TRUE;
    case EM_SETPASSWORDCHAR:
        if (line)
            line->setEchoMode(wParam ? QLineEdit::Password : QLineEdit::Normal);
        return 0;
    default:
        return DefWindowProc(hwndOf(w), message, wParam, lParam);
    }
}

LRESULT listMessage(Window* w, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* list = qobject_cast<QListWidget*>(w->widget);
    if (!list)
        return LB_ERR;
    const QSignalBlocker blocker(list); // los cambios del plugin no notifican
    const int count = list->count();
    const auto index = static_cast<int>(static_cast<intptr_t>(wParam));
    auto item = [&](int i) { return i >= 0 && i < count ? list->item(i) : nullptr; };
    switch (message) {
    case LB_ADDSTRING:
    case LB_INSERTSTRING: {
        const QString text = fromT(reinterpret_cast<const TCHAR*>(lParam));
        int at = count;
        if (message == LB_INSERTSTRING && index >= 0 && index <= count)
            at = index;
        else if (message == LB_ADDSTRING && (w->style & LBS_SORT))
            at = insertSorted(listItems(list), text);
        list->insertItem(at, text);
        return at;
    }
    case LB_DELETESTRING:
        if (!item(index))
            return LB_ERR;
        delete list->takeItem(index);
        return list->count();
    case LB_RESETCONTENT:
        list->clear();
        return 0;
    case LB_GETCOUNT:
        return count;
    case LB_GETCURSEL:
        if (list->selectionMode() == QAbstractItemView::SingleSelection) {
            const QList<QListWidgetItem*> selected = list->selectedItems();
            return selected.isEmpty() ? LB_ERR : list->row(selected.first());
        }
        return list->currentRow();
    case LB_SETCURSEL:
        if (index < 0) {
            list->clearSelection();
            return LB_ERR;
        }
        if (!item(index))
            return LB_ERR;
        list->setCurrentRow(index);
        list->scrollToItem(list->item(index));
        return index;
    case LB_GETSEL:
        return item(index) ? (item(index)->isSelected() ? 1 : 0) : LB_ERR;
    case LB_SETSEL: {
        const int target = static_cast<int>(lParam);
        if (target < 0) {
            for (int i = 0; i < count; ++i)
                list->item(i)->setSelected(wParam != 0);
        } else if (item(target)) {
            item(target)->setSelected(wParam != 0);
        } else {
            return LB_ERR;
        }
        return 0;
    }
    case LB_GETSELCOUNT:
        return static_cast<LRESULT>(list->selectedItems().size());
    case LB_GETSELITEMS: {
        auto* out = reinterpret_cast<int*>(lParam);
        int n = 0;
        for (int i = 0; i < count && n < index; ++i) {
            if (list->item(i)->isSelected()) {
                if (out)
                    out[n] = i;
                ++n;
            }
        }
        return n;
    }
    case LB_GETTEXT:
        if (!item(index))
            return LB_ERR;
        {
            const tstring text = toT(item(index)->text());
            if (auto* out = reinterpret_cast<TCHAR*>(lParam))
                std::copy_n(text.c_str(), text.size() + 1, out);
            return static_cast<LRESULT>(text.size());
        }
    case LB_GETTEXTLEN:
        return item(index) ? static_cast<LRESULT>(toT(item(index)->text()).size()) : LB_ERR;
    case LB_FINDSTRING:
    case LB_FINDSTRINGEXACT:
    case LB_SELECTSTRING: {
        const int found = findItem(listItems(list), index, fromT(reinterpret_cast<const TCHAR*>(lParam)), message == LB_FINDSTRINGEXACT);
        if (found >= 0 && message == LB_SELECTSTRING)
            list->setCurrentRow(found);
        return found < 0 ? LB_ERR : found;
    }
    case LB_GETITEMDATA:
        return item(index) ? static_cast<LRESULT>(item(index)->data(Qt::UserRole).toLongLong()) : LB_ERR;
    case LB_SETITEMDATA:
        if (!item(index))
            return LB_ERR;
        item(index)->setData(Qt::UserRole, static_cast<qlonglong>(lParam));
        return 0;
    case LB_GETTOPINDEX: {
        const QModelIndex top = list->indexAt(QPoint(0, 0));
        return top.isValid() ? top.row() : 0;
    }
    case LB_SETTOPINDEX:
        if (!item(index))
            return LB_ERR;
        list->scrollToItem(item(index), QAbstractItemView::PositionAtTop);
        return 0;
    case LB_SETCARETINDEX:
        if (!item(index))
            return LB_ERR;
        list->setCurrentRow(index, QItemSelectionModel::NoUpdate);
        return 0;
    case LB_GETCARETINDEX:
        return list->currentRow();
    case LB_GETITEMHEIGHT:
        return count ? list->sizeHintForRow(0) : QFontMetrics(list->font()).height();
    case LB_SETITEMHEIGHT:
    case LB_SETHORIZONTALEXTENT:
        return 0;
    default:
        return DefWindowProc(hwndOf(w), message, wParam, lParam);
    }
}

LRESULT comboMessage(Window* w, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* combo = qobject_cast<QComboBox*>(w->widget);
    if (!combo)
        return CB_ERR;
    const QSignalBlocker blocker(combo);
    const int count = combo->count();
    const auto index = static_cast<int>(static_cast<intptr_t>(wParam));
    const bool valid = index >= 0 && index < count;
    switch (message) {
    case CB_ADDSTRING:
    case CB_INSERTSTRING: {
        const QString text = fromT(reinterpret_cast<const TCHAR*>(lParam));
        int at = count;
        if (message == CB_INSERTSTRING && index >= 0 && index <= count)
            at = index;
        else if (message == CB_ADDSTRING && (w->style & CBS_SORT))
            at = insertSorted(comboItems(combo), text);
        const QString edit = combo->isEditable() ? combo->currentText() : QString();
        const int current = combo->currentIndex();
        combo->insertItem(at, text);
        // Añadir no cambia la selección (Qt elige el primero si no había)
        combo->setCurrentIndex(current < 0 ? -1 : (at <= current ? current + 1 : current));
        if (combo->isEditable())
            combo->setEditText(edit);
        return at;
    }
    case CB_DELETESTRING:
        if (!valid)
            return CB_ERR;
        combo->removeItem(index);
        return combo->count();
    case CB_RESETCONTENT:
        combo->clear();
        return CB_OKAY;
    case CB_GETCOUNT:
        return count;
    case CB_GETCURSEL:
        return combo->currentIndex();
    case CB_SETCURSEL:
        if (!valid) {
            combo->setCurrentIndex(-1);
            if (combo->isEditable())
                combo->setEditText(QString());
            return CB_ERR;
        }
        combo->setCurrentIndex(index);
        return index;
    case CB_GETLBTEXT:
        if (!valid)
            return CB_ERR;
        {
            const tstring text = toT(combo->itemText(index));
            if (auto* out = reinterpret_cast<TCHAR*>(lParam))
                std::copy_n(text.c_str(), text.size() + 1, out);
            return static_cast<LRESULT>(text.size());
        }
    case CB_GETLBTEXTLEN:
        return valid ? static_cast<LRESULT>(toT(combo->itemText(index)).size()) : CB_ERR;
    case CB_FINDSTRING:
    case CB_FINDSTRINGEXACT:
    case CB_SELECTSTRING: {
        const int found = findItem(comboItems(combo), index, fromT(reinterpret_cast<const TCHAR*>(lParam)), message == CB_FINDSTRINGEXACT);
        if (found >= 0 && message == CB_SELECTSTRING)
            combo->setCurrentIndex(found);
        return found < 0 ? CB_ERR : found;
    }
    case CB_GETITEMDATA:
        return valid ? static_cast<LRESULT>(combo->itemData(index).toLongLong()) : CB_ERR;
    case CB_SETITEMDATA:
        if (!valid)
            return CB_ERR;
        combo->setItemData(index, static_cast<qlonglong>(lParam));
        return CB_OKAY;
    case CB_SHOWDROPDOWN:
        if (wParam)
            combo->showPopup();
        else
            combo->hidePopup();
        return TRUE;
    case CB_GETDROPPEDSTATE:
        return combo->view()->isVisible();
    case CB_LIMITTEXT:
        if (QLineEdit* edit = combo->lineEdit())
            edit->setMaxLength(wParam ? static_cast<int>(wParam) : 32767);
        return TRUE;
    case CB_GETEDITSEL: {
        QLineEdit* edit = combo->lineEdit();
        const int start = edit ? (edit->hasSelectedText() ? edit->selectionStart() : edit->cursorPosition()) : 0;
        const int end = edit ? (edit->hasSelectedText() ? edit->selectionEnd() : start) : 0;
        if (auto* out = reinterpret_cast<DWORD*>(wParam))
            *out = static_cast<DWORD>(start);
        if (auto* out = reinterpret_cast<DWORD*>(lParam))
            *out = static_cast<DWORD>(end);
        return MAKELONG(static_cast<WORD>(start), static_cast<WORD>(end));
    }
    case CB_SETEDITSEL:
        if (QLineEdit* edit = combo->lineEdit()) {
            const auto start = static_cast<short>(LOWORD(lParam));
            const auto end = static_cast<short>(HIWORD(lParam));
            const int length = static_cast<int>(edit->text().size());
            if (start < 0)
                edit->deselect();
            else
                edit->setSelection(start, (end < 0 ? length : std::min<int>(end, length)) - start);
            return TRUE;
        }
        return CB_ERR;
    case CB_SETMINVISIBLE:
        combo->setMaxVisibleItems(std::max(1, index));
        return TRUE;
    case CB_SETEXTENDEDUI:
    case CB_SETITEMHEIGHT:
    case CB_SETDROPPEDWIDTH:
        return CB_OKAY;
    case CB_GETITEMHEIGHT:
        return QFontMetrics(combo->font()).height();
    default:
        return DefWindowProc(hwndOf(w), message, wParam, lParam);
    }
}

LRESULT buttonMessage(Window* w, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* button = qobject_cast<QAbstractButton*>(w->widget);
    switch (message) {
    case BM_GETCHECK:
        if (auto* check = qobject_cast<QCheckBox*>(button)) {
            switch (check->checkState()) {
            case Qt::Checked: return BST_CHECKED;
            case Qt::PartiallyChecked: return BST_INDETERMINATE;
            default: return BST_UNCHECKED;
            }
        }
        return button && button->isCheckable() && button->isChecked() ? BST_CHECKED : BST_UNCHECKED;
    case BM_SETCHECK:
        if (auto* check = qobject_cast<QCheckBox*>(button)) {
            const QSignalBlocker blocker(check);
            check->setCheckState(wParam == BST_CHECKED ? Qt::Checked : wParam == BST_INDETERMINATE ? Qt::PartiallyChecked : Qt::Unchecked);
        } else if (button && button->isCheckable()) {
            // En Qt, una opción exclusiva no se puede desmarcar
            const QSignalBlocker blocker(button);
            const bool exclusive = button->autoExclusive();
            button->setAutoExclusive(false);
            button->setChecked(wParam == BST_CHECKED);
            button->setAutoExclusive(exclusive);
        }
        return 0;
    case BM_CLICK:
        if (button)
            button->click();
        return 0;
    case BM_GETSTATE: {
        LRESULT state = buttonMessage(w, BM_GETCHECK, 0, 0);
        if (button && button->isDown())
            state |= BST_PUSHED;
        if (button && button->hasFocus())
            state |= BST_FOCUS;
        return state;
    }
    case BM_SETSTATE:
        if (button)
            button->setDown(wParam != 0);
        return 0;
    case BM_SETSTYLE:
        w->style = (w->style & ~static_cast<DWORD>(0xFFFF)) | static_cast<DWORD>(wParam & 0xFFFF);
        return 0;
    case BM_GETIMAGE:
    case BM_SETIMAGE:
        return 0;
    default:
        return DefWindowProc(hwndOf(w), message, wParam, lParam);
    }
}

// El procedimiento de los controles estándar
LRESULT CALLBACK controlProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return 0;
    switch (w->kind) {
    case Kind::Button:
        if (message >= BM_GETCHECK && message <= BM_SETIMAGE)
            return buttonMessage(w, message, wParam, lParam);
        break;
    case Kind::Edit:
        if ((message >= EM_GETSEL && message <= EM_GETLIMITTEXT) || message == EM_SETCUEBANNER)
            return editMessage(w, message, wParam, lParam);
        break;
    case Kind::ListBox:
        if (message >= LB_ADDSTRING && message <= LB_FINDSTRINGEXACT)
            return listMessage(w, message, wParam, lParam);
        break;
    case Kind::ComboBox:
        if ((message >= CB_GETEDITSEL && message <= CB_SETDROPPEDWIDTH) || message == CB_SETMINVISIBLE)
            return comboMessage(w, message, wParam, lParam);
        break;
    case Kind::Static:
        if (message >= STM_SETICON && message <= STM_GETIMAGE)
            return 0;
        break;
    default:
        break;
    }
    if (message == WM_CONTEXTMENU && w->kind == Kind::Edit) {
        // El menú de Qt del campo (el de Windows: cortar, copiar, pegar…)
        w->keyReached = true;
        if (auto* line = qobject_cast<QLineEdit*>(w->widget)) {
            std::unique_ptr<QMenu> menu(line->createStandardContextMenu());
            menu->exec(QPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        } else if (auto* multi = qobject_cast<QPlainTextEdit*>(w->widget)) {
            std::unique_ptr<QMenu> menu(multi->createStandardContextMenu());
            menu->exec(QPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        }
        return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT defDialog(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_CLOSE:
        // DefDlgProc: como pulsar Cancelar
        if (Window* w = windowOf(hwnd))
            send(w, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
        return 0;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

// El procedimiento de la clase de los diálogos: el DLGPROC del plugin y, si devuelve
// FALSE, DefDlgProc. Con TRUE, el resultado es DWLP_MSGRESULT, salvo en los mensajes que
// devuelven el valor del DLGPROC
LRESULT CALLBACK dialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return 0;
    if (w->dlgProc) {
        w->messageResult = 0;
        const INT_PTR handled = w->dlgProc(hwnd, message, wParam, lParam);
        if (handled) {
            switch (message) {
            case WM_INITDIALOG:
            case WM_CTLCOLORDLG:
            case WM_CTLCOLORSTATIC:
            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORBTN:
            case WM_CTLCOLORLISTBOX:
                return handled;
            default:
                return w->alive ? w->messageResult : 0;
            }
        }
        if (!w->alive)
            return 0;
    }
    return defDialog(hwnd, message, wParam, lParam);
}

// --- Crear ventanas --------------------------------------------------------------------------

const WindowClass* findClass(const QString& name)
{
    for (const WindowClass& c : registry().classes) {
        if (c.name.compare(name, Qt::CaseInsensitive) == 0 || (name.startsWith(QLatin1Char('#')) && name.mid(1).toUInt() == c.atom))
            return &c;
    }
    return nullptr;
}

QWidget* parentWidgetFor(HWND parent)
{
    return widgetOf(parent);
}

// El widget de un control estándar y sus notificaciones
QWidget* createControl(Window* w, const QString& className, QWidget* parent)
{
    const QString name = className.toUpper();
    const DWORD style = w->style;
    QWidget* widget = nullptr;
    if (name == QLatin1String("BUTTON")) {
        w->kind = Kind::Button;
        const DWORD type = style & BS_TYPEMASK;
        if (type == BS_GROUPBOX) {
            auto* group = new QGroupBox(parent);
            widget = group;
            group->lower();
            group->setAttribute(Qt::WA_TransparentForMouseEvents);
        } else if (type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE) {
            auto* check = new QCheckBox(parent);
            check->setTristate(type == BS_3STATE || type == BS_AUTO3STATE);
            widget = check;
            const bool automatic = type == BS_AUTOCHECKBOX || type == BS_AUTO3STATE;
            QObject::connect(check, &QAbstractButton::clicked, check, [w, check, automatic] {
                if (!automatic) {
                    // BS_CHECKBOX: el plugin la marca él mismo con BM_SETCHECK
                    const QSignalBlocker blocker(check);
                    check->setChecked(!check->isChecked());
                }
                notifyParent(w, BN_CLICKED);
            });
        } else if (type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON) {
            auto* radio = new QRadioButton(parent);
            const bool automatic = type == BS_AUTORADIOBUTTON;
            radio->setAutoExclusive(automatic);
            widget = radio;
            QObject::connect(radio, &QAbstractButton::clicked, radio, [w, radio, automatic] {
                if (!automatic) {
                    const QSignalBlocker blocker(radio);
                    radio->setChecked(false);
                }
                notifyParent(w, BN_CLICKED);
            });
        } else {
            auto* push = new QPushButton(parent);
            push->setAutoDefault(false);
            widget = push;
            QObject::connect(push, &QAbstractButton::clicked, push, [w] { notifyParent(w, BN_CLICKED); });
        }
    } else if (name == QLatin1String("EDIT")) {
        w->kind = Kind::Edit;
        if (style & ES_MULTILINE) {
            auto* multi = new QPlainTextEdit(parent);
            multi->setReadOnly(style & ES_READONLY);
            multi->setLineWrapMode((style & (WS_HSCROLL | ES_AUTOHSCROLL)) ? QPlainTextEdit::NoWrap : QPlainTextEdit::WidgetWidth);
            multi->setVerticalScrollBarPolicy((style & WS_VSCROLL) ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
            QObject::connect(multi, &QPlainTextEdit::textChanged, multi, [w] { notifyParent(w, EN_CHANGE); });
            widget = multi;
        } else {
            auto* line = new QLineEdit(parent);
            line->setReadOnly(style & ES_READONLY);
            if (style & ES_PASSWORD)
                line->setEchoMode(QLineEdit::Password);
            if (style & ES_NUMBER)
                line->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]*")), line));
            if (style & ES_CENTER)
                line->setAlignment(Qt::AlignCenter);
            else if (style & ES_RIGHT)
                line->setAlignment(Qt::AlignRight);
            QObject::connect(line, &QLineEdit::textChanged, line, [w] { notifyParent(w, EN_CHANGE); });
            widget = line;
        }
    } else if (name == QLatin1String("STATIC")) {
        w->kind = Kind::Static;
        const DWORD type = style & SS_TYPEMASK;
        if (type == SS_ETCHEDHORZ || type == SS_ETCHEDVERT || type == SS_ETCHEDFRAME || type == SS_BLACKFRAME
            || type == SS_GRAYFRAME || type == SS_WHITEFRAME) {
            auto* frame = new QFrame(parent);
            frame->setFrameShape(type == SS_ETCHEDHORZ ? QFrame::HLine : type == SS_ETCHEDVERT ? QFrame::VLine : QFrame::Box);
            frame->setFrameShadow(QFrame::Sunken);
            widget = frame;
        } else {
            auto* label = new QLabel(parent);
            label->setWordWrap(type == SS_LEFT || type == SS_CENTER || type == SS_RIGHT);
            const Qt::Alignment horizontal = type == SS_CENTER ? Qt::AlignHCenter : type == SS_RIGHT ? Qt::AlignRight : Qt::AlignLeft;
            label->setAlignment(horizontal | ((style & SS_CENTERIMAGE) ? Qt::AlignVCenter : Qt::AlignTop));
            widget = label;
        }
    } else if (name == QLatin1String("LISTBOX")) {
        w->kind = Kind::ListBox;
        auto* list = new QListWidget(parent);
        if (style & LBS_NOSEL)
            list->setSelectionMode(QAbstractItemView::NoSelection);
        else if (style & LBS_EXTENDEDSEL)
            list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        else if (style & LBS_MULTIPLESEL)
            list->setSelectionMode(QAbstractItemView::MultiSelection);
        else
            list->setSelectionMode(QAbstractItemView::SingleSelection);
        QObject::connect(list, &QListWidget::itemSelectionChanged, list, [w] {
            if (w->style & LBS_NOTIFY)
                notifyParent(w, LBN_SELCHANGE);
        });
        QObject::connect(list, &QListWidget::itemDoubleClicked, list, [w] {
            if (w->style & LBS_NOTIFY)
                notifyParent(w, LBN_DBLCLK);
        });
        widget = list;
    } else if (name == QLatin1String("COMBOBOX")) {
        w->kind = Kind::ComboBox;
        auto* combo = new QComboBox(parent);
        const DWORD type = style & 0x3;
        combo->setEditable(type == CBS_DROPDOWN || type == CBS_SIMPLE);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->setCurrentIndex(-1);
        QObject::connect(combo, &QComboBox::activated, combo, [w] { notifyParent(w, CBN_SELCHANGE); });
        QObject::connect(combo, &QComboBox::editTextChanged, combo, [w, combo] {
            if (combo->lineEdit() && combo->lineEdit()->hasFocus())
                notifyParent(w, CBN_EDITCHANGE);
        });
        widget = combo;
    }
    if (widget) {
        widget->setContextMenuPolicy(Qt::DefaultContextMenu);
        widget->installEventFilter(forwarder());
        if (auto* combo = qobject_cast<QComboBox*>(widget); combo && combo->lineEdit()) {
            combo->lineEdit()->setProperty(kHwndProperty, QVariant::fromValue(reinterpret_cast<quintptr>(hwndOf(w))));
            combo->lineEdit()->installEventFilter(forwarder());
        }
    }
    return widget;
}

void applyCommonStyle(Window* w)
{
    QWidget* widget = w->widget;
    if (!widget)
        return;
    widget->setEnabled(!(w->style & WS_DISABLED));
    if ((w->exStyle & WS_EX_TOPMOST) && widget->isWindow())
        widget->setWindowFlag(Qt::WindowStaysOnTopHint, true);
}

Window* newWindow(Kind kind, DWORD style, DWORD exStyle, HWND parent, HINSTANCE instance)
{
    auto* w = new Window;
    w->kind = kind;
    w->style = style;
    w->exStyle = exStyle;
    w->parent = parent;
    w->instance = instance;
    registry().alive.insert(w);
    if (Window* p = windowOf(parent); p && (style & WS_CHILD)) {
        p->children.push_back(w);
        if (p->placeholder) {
            delete p->placeholder;
            p->placeholder = nullptr;
        }
    }
    return w;
}

void attachWidget(Window* w, QWidget* widget)
{
    w->widget = widget;
    widget->setProperty(kHwndProperty, QVariant::fromValue(reinterpret_cast<quintptr>(hwndOf(w))));
    if (auto* host = dynamic_cast<HostWidget*>(widget))
        host->window = w;
}

HostWidget* newHostWidget(Window* w, HWND parent, bool dialog)
{
    QWidget* parentWidget = parentWidgetFor(parent);
    HostWidget* widget = nullptr;
    if ((w->style & WS_CHILD) && parentWidget) {
        widget = new HostWidget(parentWidget);
    } else {
        Qt::WindowFlags flags = Qt::Window;
        if (dialog)
            flags = Qt::Dialog;
        if (w->exStyle & WS_EX_TOOLWINDOW)
            flags = Qt::Tool;
        // El dueño de Edita: la ventana queda encima de la principal
        widget = new HostWidget(parentWidget ? parentWidget->window() : nullptr, flags);
    }
    attachWidget(w, widget);
    return widget;
}

template<typename CreateStruct, typename Ch>
CreateStruct makeCreateStruct(LPVOID param, HINSTANCE instance, HMENU menu, HWND parent, int x, int y, int cx, int cy, DWORD style,
                              const Ch* name, const Ch* className, DWORD exStyle)
{
    CreateStruct cs{};
    cs.lpCreateParams = param;
    cs.hInstance = instance;
    cs.hMenu = menu;
    cs.hwndParent = parent;
    cs.cy = cy;
    cs.cx = cx;
    cs.y = y;
    cs.x = x;
    cs.style = static_cast<LONG>(style);
    cs.lpszName = name;
    cs.lpszClass = className;
    cs.dwExStyle = exStyle;
    return cs;
}

HWND createWindow(DWORD exStyle, const QString& className, const QString& windowName, DWORD style, int x, int y, int width,
                  int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param)
{
    // Un editor de Scintilla de Edita: su HWND es el del editor (le llegan los SCI_*)
    if (className.compare(QLatin1String("Scintilla"), Qt::CaseInsensitive) == 0) {
        QWidget* parentWidget = widgetOf(parent);
        void* editor = edita_compat_createScintilla(parentWidget);
        if (!editor) {
            SetLastError(1407); // ERROR_CANNOT_FIND_WND_CLASS
            return nullptr;
        }
        edita_compat_registerForeign(editor, parent, (style & WS_CHILD) ? static_cast<int>(reinterpret_cast<intptr_t>(menu)) : 0);
        if (auto* widget = static_cast<QWidget*>(editor)) {
            if (width != CW_USEDEFAULT && width > 0)
                widget->setGeometry(x == CW_USEDEFAULT ? 0 : x, y == CW_USEDEFAULT ? 0 : y, width, height);
            widget->setVisible(style & WS_VISIBLE);
        }
        return static_cast<HWND>(editor);
    }

    const WindowClass* custom = findClass(className);
    const bool dialogClass = className == QLatin1String("#32770");
    const bool standard = !custom && !dialogClass;
    Window* w = newWindow(custom ? Kind::Custom : dialogClass ? Kind::Dialog : Kind::Generic, style, exStyle, parent, instance);
    w->className = className;
    w->id = (style & WS_CHILD) ? static_cast<int>(reinterpret_cast<intptr_t>(menu)) : 0;
    w->text = windowName;
    QWidget* widget = nullptr;
    if (standard) {
        QWidget* parentWidget = widgetOf(parent);
        widget = createControl(w, className, parentWidget);
        if (!widget) {
            registry().alive.erase(w);
            w->alive = false;
            SetLastError(1407); // ERROR_CANNOT_FIND_WND_CLASS
            return nullptr;
        }
        attachWidget(w, widget);
        w->proc = w->builtin = controlProc;
    } else {
        HostWidget* host = newHostWidget(w, parent, dialogClass);
        widget = host;
        if (custom) {
            w->proc = w->builtin = custom->proc;
            for (int offset = 0; offset < custom->extraBytes; offset += static_cast<int>(sizeof(LONG_PTR)))
                w->extra[offset] = 0;
            host->setMouseTracking(true);
            host->setFocusPolicy(Qt::StrongFocus);
        } else {
            w->proc = w->builtin = dialogClass ? dialogProc : DefWindowProcW;
        }
    }
    if (width != CW_USEDEFAULT && height != CW_USEDEFAULT && width >= 0 && height >= 0) {
        const int left = x == CW_USEDEFAULT ? 0 : x;
        const int top = y == CW_USEDEFAULT ? 0 : y;
        if (widget->isWindow()) {
            if (x != CW_USEDEFAULT)
                widget->move(left, top);
            widget->resize(width, height);
            if (auto* host = dynamic_cast<HostWidget*>(widget))
                host->preferred = QSize(width, height);
        } else {
            widget->setGeometry(left, top, width, height);
        }
    }
    setWindowText(w, windowName);
    applyCommonStyle(w);

    // WM_NCCREATE y WM_CREATE, con el CREATESTRUCT
    const tstring name = toT(windowName);
    const tstring klass = toT(className);
    CREATESTRUCT cs = makeCreateStruct<CREATESTRUCT>(param, instance, menu, parent, x, y, width, height, style, name.c_str(),
                                                     klass.c_str(), exStyle);
    if (!send(w, WM_NCCREATE, 0, reinterpret_cast<LPARAM>(&cs)) && custom) {
        DestroyWindow(hwndOf(w));
        return nullptr;
    }
    if (send(w, WM_CREATE, 0, reinterpret_cast<LPARAM>(&cs)) == -1) {
        DestroyWindow(hwndOf(w));
        return nullptr;
    }
    w->created = true;
    if (style & WS_VISIBLE)
        widget->show();
    else if (!(style & WS_CHILD))
        widget->hide();
    return hwndOf(w);
}

// --- Diálogos ----------------------------------------------------------------------------------

QString placeholderText()
{
    const QString name = QString::fromUtf8(EDITA_PLUGIN_DISPLAY_NAME);
    return spanish() ? QStringLiteral("%1: este panel es un diálogo de Windows (un recurso .rc) que Edita todavía no puede mostrar.").arg(name)
                     : QStringLiteral("%1: this panel is a Windows dialog (an .rc resource) that Edita cannot show yet.").arg(name);
}

// Un diálogo sin plantilla (los .rc todavía no se leen): vacío, con un aviso que se quita
// cuando el plugin le pone controles
HWND createDialog(HINSTANCE instance, HWND parent, DLGPROC proc, LPARAM init)
{
    Window* w = newWindow(Kind::Dialog, WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME, WS_EX_DLGMODALFRAME, parent, instance);
    w->className = QStringLiteral("#32770");
    w->dlgProc = proc;
    w->proc = w->builtin = dialogProc;
    HostWidget* widget = newHostWidget(w, parent, true);
    widget->resize(widget->preferred);
    w->placeholder = new QLabel(placeholderText(), widget);
    w->placeholder->setWordWrap(true);
    w->placeholder->setAlignment(Qt::AlignCenter);
    w->placeholder->setEnabled(false);
    w->placeholder->setGeometry(widget->rect().adjusted(12, 12, -12, -12));
    widget->hide();
    HWND hwnd = hwndOf(w);
    send(w, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), FALSE);
    send(w, WM_INITDIALOG, 0, init);
    w->created = true;
    return w->alive ? hwnd : nullptr;
}

void showMissingDialog()
{
    QString text = spanish() ? QStringLiteral("<b>%1 %2</b><br>Plugin de Notepad++ compilado para Edita.<br><br>"
                                              "Este diálogo del plugin original es de Windows y Edita todavía no puede mostrarlo.")
                             : QStringLiteral("<b>%1 %2</b><br>Notepad++ plugin built for Edita.<br><br>"
                                              "This dialog of the original plugin is a Windows dialog that Edita cannot show yet.");
    text = text.arg(QString::fromUtf8(EDITA_PLUGIN_DISPLAY_NAME), QString::fromUtf8(EDITA_PLUGIN_VERSION));
    const QString homepage = QString::fromUtf8(EDITA_PLUGIN_HOMEPAGE);
    if (!homepage.isEmpty())
        text += QStringLiteral("<br><a href=\"%1\">%1</a>").arg(homepage);
    QMessageBox::information(QApplication::activeWindow(), QString::fromUtf8(EDITA_PLUGIN_DISPLAY_NAME), text);
}

// --- Menús -------------------------------------------------------------------------------------

constexpr uint32_t kMenuMagic = 0x4D454E55; // "MENU"

struct Menu
{
    uint32_t magic = kMenuMagic;
    bool host = false;          // el menú de Edita (GetMenu(nppData._nppHandle), NPPM_GETMENUHANDLE)
    QMenu* menu = nullptr;
};

Menu* menuOf(HMENU handle)
{
    auto* m = static_cast<Menu*>(handle);
    return m && m->magic == kMenuMagic ? m : nullptr;
}

Menu* hostMenu()
{
    static auto* instance = [] {
        auto* m = new Menu;
        m->host = true;
        return m;
    }();
    return instance;
}

QAction* findAction(QMenu* menu, UINT item, UINT flags)
{
    if (!menu)
        return nullptr;
    const QList<QAction*> actions = menu->actions();
    if (flags & MF_BYPOSITION)
        return item < static_cast<UINT>(actions.size()) ? actions[static_cast<qsizetype>(item)] : nullptr;
    for (QAction* action : actions) {
        if (action->menu()) {
            if (QAction* found = findAction(action->menu(), item, flags))
                return found;
        } else if (!action->isSeparator() && action->data().toULongLong() == item) {
            return action;
        }
    }
    return nullptr;
}

QAction* makeAction(QMenu* owner, UINT flags, UINT_PTR id, const QString& text)
{
    QAction* action = nullptr;
    if (flags & MF_SEPARATOR) {
        action = new QAction(owner);
        action->setSeparator(true);
    } else if (flags & MF_POPUP) {
        Menu* sub = menuOf(reinterpret_cast<HMENU>(id));
        if (!sub || !sub->menu)
            return nullptr;
        sub->menu->setTitle(text);
        action = sub->menu->menuAction();
    } else {
        action = new QAction(text, owner);
        action->setData(static_cast<qulonglong>(id));
    }
    if (flags & MF_CHECKED) {
        action->setCheckable(true);
        action->setChecked(true);
    }
    action->setEnabled(!(flags & (MF_GRAYED | MF_DISABLED)));
    return action;
}

// --- Temporizadores ----------------------------------------------------------------------------

void fireTimer(HWND hwnd, UINT_PTR id, TIMERPROC proc)
{
    if (proc) {
        proc(hwnd, WM_TIMER, id, GetTickCount());
        return;
    }
    if (Window* w = windowOf(hwnd))
        send(w, WM_TIMER, id, 0);
}

QScreen* screenFor(const QPoint& point, DWORD flags)
{
    if (QScreen* screen = QGuiApplication::screenAt(point))
        return screen;
    if (flags == MONITOR_DEFAULTTONULL)
        return nullptr;
    if (flags == MONITOR_DEFAULTTOPRIMARY)
        return QGuiApplication::primaryScreen();
    QScreen* nearest = QGuiApplication::primaryScreen();
    qint64 best = -1;
    for (QScreen* screen : QGuiApplication::screens()) {
        const QPoint center = screen->geometry().center();
        const qint64 dx = center.x() - point.x();
        const qint64 dy = center.y() - point.y();
        const qint64 distance = dx * dx + dy * dy;
        if (best < 0 || distance < best) {
            best = distance;
            nearest = screen;
        }
    }
    return nearest;
}

RECT rectOf(const QRect& r)
{
    return RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
}

COLORREF colorRef(const QColor& c)
{
    return RGB(c.red(), c.green(), c.blue());
}

QColor systemColor(int index)
{
    const QPalette palette = QApplication::palette();
    switch (index) {
    case COLOR_WINDOW: return palette.color(QPalette::Base);
    case COLOR_WINDOWTEXT:
    case COLOR_MENUTEXT:
    case COLOR_CAPTIONTEXT: return palette.color(QPalette::Text);
    case COLOR_BTNFACE:
    case COLOR_MENU:
    case COLOR_MENUBAR:
    case COLOR_APPWORKSPACE: return palette.color(QPalette::Window);
    case COLOR_BTNTEXT: return palette.color(QPalette::ButtonText);
    case COLOR_HIGHLIGHT:
    case COLOR_MENUHILIGHT:
    case COLOR_ACTIVECAPTION: return palette.color(QPalette::Highlight);
    case COLOR_HIGHLIGHTTEXT: return palette.color(QPalette::HighlightedText);
    case COLOR_GRAYTEXT:
    case COLOR_INACTIVECAPTIONTEXT: return palette.color(QPalette::Disabled, QPalette::Text);
    case COLOR_BTNSHADOW: return palette.color(QPalette::Mid);
    case COLOR_3DDKSHADOW: return palette.color(QPalette::Dark);
    case COLOR_BTNHIGHLIGHT:
    case COLOR_3DLIGHT: return palette.color(QPalette::Light);
    case COLOR_INFOBK: return palette.color(QPalette::ToolTipBase);
    case COLOR_INFOTEXT: return palette.color(QPalette::ToolTipText);
    case COLOR_HOTLIGHT: return palette.color(QPalette::Link);
    default: return palette.color(QPalette::Window);
    }
}

void destroyWindow(Window* w)
{
    if (!w || !w->alive)
        return;
    HWND hwnd = hwndOf(w);
    send(w, WM_DESTROY, 0, 0);
    // Los hijos después del padre, como en Windows
    const std::vector<Window*> children = w->children;
    for (Window* child : children)
        destroyWindow(child);
    send(w, WM_NCDESTROY, 0, 0);
    for (auto& [id, timer] : w->timers)
        delete timer;
    w->timers.clear();
    if (Window* parent = windowOf(w->parent)) {
        auto& siblings = parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), w), siblings.end());
    }
    if (registry().capture == hwnd) {
        if (w->widget)
            w->widget->releaseMouse();
        registry().capture = nullptr;
    }
    w->alive = false;
    registry().alive.erase(w);
    if (w->modalLoop)
        w->modalLoop->quit();
    if (QWidget* widget = w->widget) {
        widget->setProperty(kHwndProperty, QVariant());
        if (auto* host = dynamic_cast<HostWidget*>(widget))
            host->window = nullptr;
        widget->hide();
        widget->deleteLater();
    }
}

} // namespace

// =============================================================================================
// API
// =============================================================================================

extern "C" {

// --- Enlace con npp_compat.cpp ------------------------------------------------------------------

BOOL edita_compat_isWindow(HWND hwnd)
{
    return windowOf(hwnd) ? TRUE : FALSE;
}

LRESULT edita_compat_windowSend(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return send(windowOf(hwnd), message, wParam, lParam);
}

void edita_compat_windowPost(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    QMetaObject::invokeMethod(qApp, [hwnd, message, wParam, lParam] {
        if (Window* w = windowOf(hwnd))
            send(w, message, wParam, lParam);
    }, Qt::QueuedConnection);
}

void edita_compat_registerForeign(void* widget, HWND parent, int id)
{
    if (!widget)
        return;
    registry().foreign[widget] = Foreign{QPointer<QWidget>(static_cast<QWidget*>(widget)), parent, id};
}

void* edita_compat_widget(HWND hwnd)
{
    return widgetOf(hwnd);
}

HWND edita_compat_hwndOfWidget(void* widget)
{
    return hwndOfWidget(static_cast<QWidget*>(widget));
}

HMENU edita_compat_hostMenu(void)
{
    return hostMenu();
}

// Sin npp_compat.cpp (las pruebas): solo las ventanas de la capa
__attribute__((weak)) LRESULT edita_compat_SendMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return edita_compat_windowSend(hwnd, message, wParam, lParam);
}

__attribute__((weak)) BOOL edita_compat_PostMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!windowOf(hwnd))
        return FALSE;
    edita_compat_windowPost(hwnd, message, wParam, lParam);
    return TRUE;
}

__attribute__((weak)) void* edita_compat_createScintilla(void*)
{
    return nullptr;
}

__attribute__((weak)) HWND edita_compat_hostHwnd(void)
{
    return nullptr;
}

// --- Clases -------------------------------------------------------------------------------------

static ATOM registerClass(const QString& name, WNDPROC proc, int extraBytes, UINT style)
{
    if (name.isEmpty() || !proc || findClass(name)) {
        SetLastError(1410); // ERROR_CLASS_ALREADY_EXISTS
        return 0;
    }
    auto& classes = registry().classes;
    WindowClass c;
    c.name = name;
    c.proc = proc;
    c.extraBytes = extraBytes;
    c.style = style;
    c.atom = static_cast<ATOM>(0xC000 + classes.size());
    classes.push_back(c);
    return c.atom;
}

ATOM RegisterClassA(const WNDCLASSA* wc) { return wc ? registerClass(resourceName(wc->lpszClassName, fromA), wc->lpfnWndProc, wc->cbWndExtra, wc->style) : 0; }
ATOM RegisterClassW(const WNDCLASSW* wc) { return wc ? registerClass(resourceName(wc->lpszClassName, fromW), wc->lpfnWndProc, wc->cbWndExtra, wc->style) : 0; }
ATOM RegisterClassExA(const WNDCLASSEXA* wc) { return wc ? registerClass(resourceName(wc->lpszClassName, fromA), wc->lpfnWndProc, wc->cbWndExtra, wc->style) : 0; }
ATOM RegisterClassExW(const WNDCLASSEXW* wc) { return wc ? registerClass(resourceName(wc->lpszClassName, fromW), wc->lpfnWndProc, wc->cbWndExtra, wc->style) : 0; }

static BOOL unregisterClass(const QString& name)
{
    auto& classes = registry().classes;
    const auto it = std::find_if(classes.begin(), classes.end(), [&](const WindowClass& c) { return c.name.compare(name, Qt::CaseInsensitive) == 0; });
    if (it == classes.end())
        return FALSE;
    it->name.clear(); // el átomo no se reutiliza
    return TRUE;
}

BOOL UnregisterClassA(LPCSTR name, HINSTANCE) { return unregisterClass(resourceName(name, fromA)); }
BOOL UnregisterClassW(LPCWSTR name, HINSTANCE) { return unregisterClass(resourceName(name, fromW)); }

int GetClassNameA(HWND hwnd, LPSTR name, int size)
{
    Window* w = windowOf(hwnd);
    return w ? copyTo(w->className.toStdString(), name, size) : 0;
}

int GetClassNameW(HWND hwnd, LPWSTR name, int size)
{
    Window* w = windowOf(hwnd);
    return w ? copyTo(w->className.toStdWString(), name, size) : 0;
}

// --- Ventanas -----------------------------------------------------------------------------------

HWND CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR windowName, DWORD style, int x, int y, int width, int height,
                     HWND parent, HMENU menu, HINSTANCE instance, LPVOID param)
{
    return createWindow(exStyle, resourceName(className, fromA), fromA(windowName), style, x, y, width, height, parent, menu, instance, param);
}

HWND CreateWindowExW(DWORD exStyle, LPCWSTR className, LPCWSTR windowName, DWORD style, int x, int y, int width, int height,
                     HWND parent, HMENU menu, HINSTANCE instance, LPVOID param)
{
    return createWindow(exStyle, resourceName(className, fromW), fromW(windowName), style, x, y, width, height, parent, menu, instance, param);
}

BOOL DestroyWindow(HWND hwnd)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return FALSE;
    destroyWindow(w);
    return TRUE;
}

// El procedimiento por defecto. Las cadenas son TCHAR, el tipo de las ventanas del plugin
LRESULT DefWindowProcW(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return 0;
    switch (message) {
    case WM_NCCREATE:
        return TRUE;
    case WM_SETTEXT:
        setWindowText(w, fromT(reinterpret_cast<const TCHAR*>(lParam)));
        return TRUE;
    case WM_GETTEXT:
        return copyText(windowText(w), reinterpret_cast<TCHAR*>(lParam), static_cast<int>(wParam));
    case WM_GETTEXTLENGTH:
        return static_cast<LRESULT>(toT(windowText(w)).size());
    case WM_SETFONT:
        w->font = reinterpret_cast<HFONT>(wParam);
        if (w->widget && wParam)
            w->widget->setFont(fontOf(w->font));
        return 0;
    case WM_GETFONT:
        return reinterpret_cast<LRESULT>(w->font);
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_CONTEXTMENU:
        if ((w->style & WS_CHILD) && windowOf(w->parent))
            return send(windowOf(w->parent), message, wParam, lParam);
        return 0;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_SYSCHAR:
        w->keyReached = true;
        return 0;
    default:
        return 0;
    }
}

LRESULT DefWindowProcA(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) { return DefWindowProcW(hwnd, message, wParam, lParam); }
LRESULT DefDlgProcW(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) { return defDialog(hwnd, message, wParam, lParam); }
LRESULT DefDlgProcA(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) { return defDialog(hwnd, message, wParam, lParam); }

LRESULT CallWindowProcW(WNDPROC proc, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return proc ? proc(hwnd, message, wParam, lParam) : 0;
}

LRESULT CallWindowProcA(WNDPROC proc, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return CallWindowProcW(proc, hwnd, message, wParam, lParam);
}

LONG_PTR GetWindowLongPtrW(HWND hwnd, int index)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return 0;
    if (w->kind == Kind::Dialog && index >= 0) {
        switch (index) {
        case DWLP_MSGRESULT: return w->messageResult;
        case DWLP_DLGPROC: return reinterpret_cast<LONG_PTR>(w->dlgProc);
        case DWLP_USER: return w->dialogUser;
        default: break;
        }
    }
    switch (index) {
    case GWLP_WNDPROC: return reinterpret_cast<LONG_PTR>(w->proc);
    case GWLP_HINSTANCE: return reinterpret_cast<LONG_PTR>(w->instance);
    case GWLP_HWNDPARENT: return reinterpret_cast<LONG_PTR>(w->parent);
    case GWLP_ID: return w->id;
    case GWLP_USERDATA: return w->userData;
    case GWL_STYLE: {
        DWORD style = w->style & ~static_cast<DWORD>(WS_VISIBLE | WS_DISABLED);
        if (w->widget && w->widget->isVisible())
            style |= WS_VISIBLE;
        if (w->widget && !w->widget->isEnabled())
            style |= WS_DISABLED;
        return static_cast<LONG_PTR>(style);
    }
    case GWL_EXSTYLE: return static_cast<LONG_PTR>(w->exStyle);
    default: {
        const auto it = w->extra.find(index);
        return it != w->extra.end() ? it->second : 0;
    }
    }
}

LONG_PTR SetWindowLongPtrW(HWND hwnd, int index, LONG_PTR value)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return 0;
    const LONG_PTR previous = GetWindowLongPtrW(hwnd, index);
    if (w->kind == Kind::Dialog && index >= 0) {
        switch (index) {
        case DWLP_MSGRESULT: w->messageResult = value; return previous;
        case DWLP_DLGPROC: w->dlgProc = reinterpret_cast<DLGPROC>(value); return previous;
        case DWLP_USER: w->dialogUser = value; return previous;
        default: break;
        }
    }
    switch (index) {
    case GWLP_WNDPROC: w->proc = reinterpret_cast<WNDPROC>(value); break;
    case GWLP_HINSTANCE: w->instance = reinterpret_cast<HINSTANCE>(value); break;
    case GWLP_HWNDPARENT: w->parent = reinterpret_cast<HWND>(value); break;
    case GWLP_ID: w->id = static_cast<int>(value); break;
    case GWLP_USERDATA: w->userData = value; break;
    case GWL_STYLE: {
        const auto style = static_cast<DWORD>(value);
        w->style = style;
        if (w->widget) {
            w->widget->setEnabled(!(style & WS_DISABLED));
            if (w->kind == Kind::Edit) {
                if (auto* line = qobject_cast<QLineEdit*>(w->widget))
                    line->setReadOnly(style & ES_READONLY);
            }
        }
        break;
    }
    case GWL_EXSTYLE: w->exStyle = static_cast<DWORD>(value); break;
    default: w->extra[index] = value; break;
    }
    return previous;
}

LONG_PTR GetWindowLongPtrA(HWND hwnd, int index) { return GetWindowLongPtrW(hwnd, index); }
LONG_PTR SetWindowLongPtrA(HWND hwnd, int index, LONG_PTR value) { return SetWindowLongPtrW(hwnd, index, value); }
LONG GetWindowLongW(HWND hwnd, int index) { return static_cast<LONG>(GetWindowLongPtrW(hwnd, index)); }
LONG GetWindowLongA(HWND hwnd, int index) { return static_cast<LONG>(GetWindowLongPtrW(hwnd, index)); }
LONG SetWindowLongW(HWND hwnd, int index, LONG value) { return static_cast<LONG>(SetWindowLongPtrW(hwnd, index, value)); }
LONG SetWindowLongA(HWND hwnd, int index, LONG value) { return static_cast<LONG>(SetWindowLongPtrW(hwnd, index, value)); }

static BOOL setProperty(HWND hwnd, const QString& name, HANDLE data)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return FALSE;
    w->properties[name.toLower()] = data;
    return TRUE;
}

static HANDLE getProperty(HWND hwnd, const QString& name, bool remove)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return nullptr;
    const auto it = w->properties.find(name.toLower());
    if (it == w->properties.end())
        return nullptr;
    HANDLE value = it->second;
    if (remove)
        w->properties.erase(it);
    return value;
}

BOOL SetPropA(HWND hwnd, LPCSTR name, HANDLE data) { return setProperty(hwnd, resourceName(name, fromA), data); }
BOOL SetPropW(HWND hwnd, LPCWSTR name, HANDLE data) { return setProperty(hwnd, resourceName(name, fromW), data); }
HANDLE GetPropA(HWND hwnd, LPCSTR name) { return getProperty(hwnd, resourceName(name, fromA), false); }
HANDLE GetPropW(HWND hwnd, LPCWSTR name) { return getProperty(hwnd, resourceName(name, fromW), false); }
HANDLE RemovePropA(HWND hwnd, LPCSTR name) { return getProperty(hwnd, resourceName(name, fromA), true); }
HANDLE RemovePropW(HWND hwnd, LPCWSTR name) { return getProperty(hwnd, resourceName(name, fromW), true); }

// --- Diálogos ------------------------------------------------------------------------------------

HWND edita_compat_CreateDialogParam(HINSTANCE instance, const void*, HWND parent, void* proc, LPARAM init)
{
    return createDialog(instance, parent, reinterpret_cast<DLGPROC>(proc), init);
}

INT_PTR edita_compat_DialogBoxParam(HINSTANCE, const void*, HWND, void*, LPARAM)
{
    // Un diálogo modal vacío no sirve de nada: se avisa
    showMissingDialog();
    return IDCANCEL;
}

HWND CreateDialogIndirectParamA(HINSTANCE instance, LPCDLGTEMPLATE, HWND parent, DLGPROC proc, LPARAM init)
{
    return createDialog(instance, parent, proc, init);
}

HWND CreateDialogIndirectParamW(HINSTANCE instance, LPCDLGTEMPLATE, HWND parent, DLGPROC proc, LPARAM init)
{
    return createDialog(instance, parent, proc, init);
}

INT_PTR DialogBoxIndirectParamA(HINSTANCE, LPCDLGTEMPLATE, HWND, DLGPROC, LPARAM)
{
    showMissingDialog();
    return IDCANCEL;
}

INT_PTR DialogBoxIndirectParamW(HINSTANCE, LPCDLGTEMPLATE, HWND, DLGPROC, LPARAM)
{
    showMissingDialog();
    return IDCANCEL;
}

BOOL EndDialog(HWND hwnd, INT_PTR result)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return FALSE;
    w->modalResult = result;
    if (w->modalLoop)
        w->modalLoop->quit();
    if (w->widget)
        w->widget->hide();
    return TRUE;
}

BOOL IsDialogMessageA(HWND, LPMSG) { return FALSE; }
BOOL IsDialogMessageW(HWND, LPMSG) { return FALSE; }

HWND GetDlgItem(HWND dialog, int id)
{
    if (Window* w = windowOf(dialog)) {
        for (Window* child : w->children) {
            if (child->alive && child->id == id)
                return hwndOf(child);
        }
    }
    for (const auto& [handle, info] : registry().foreign) {
        if (info.widget && info.parent == dialog && info.id == id && dialog)
            return static_cast<HWND>(handle);
    }
    return nullptr;
}

int GetDlgCtrlID(HWND hwnd)
{
    if (Window* w = windowOf(hwnd))
        return w->id;
    const auto& foreign = registry().foreign;
    const auto it = foreign.find(hwnd);
    return it != foreign.end() ? it->second.id : 0;
}

BOOL SetWindowTextW(HWND hwnd, LPCWSTR text)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return FALSE;
    const tstring converted = toT(fromW(text));
    return send(w, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(converted.c_str())) ? TRUE : FALSE;
}

BOOL SetWindowTextA(HWND hwnd, LPCSTR text)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return FALSE;
    const tstring converted = toT(fromA(text));
    return send(w, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(converted.c_str())) ? TRUE : FALSE;
}

static QString textOf(HWND hwnd)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return {};
    const auto length = static_cast<std::size_t>(send(w, WM_GETTEXTLENGTH, 0, 0));
    tstring buffer(length + 1, TCHAR(0));
    send(w, WM_GETTEXT, buffer.size(), reinterpret_cast<LPARAM>(buffer.data()));
    return fromT(buffer.c_str());
}

int GetWindowTextW(HWND hwnd, LPWSTR text, int size) { return copyTo(textOf(hwnd).toStdWString(), text, size); }
int GetWindowTextA(HWND hwnd, LPSTR text, int size) { return copyTo(textOf(hwnd).toStdString(), text, size); }
int GetWindowTextLengthW(HWND hwnd) { return static_cast<int>(textOf(hwnd).toStdWString().size()); }
int GetWindowTextLengthA(HWND hwnd) { return static_cast<int>(textOf(hwnd).toUtf8().size()); }

BOOL SetDlgItemTextA(HWND dialog, int id, LPCSTR text) { return SetWindowTextA(GetDlgItem(dialog, id), text); }
BOOL SetDlgItemTextW(HWND dialog, int id, LPCWSTR text) { return SetWindowTextW(GetDlgItem(dialog, id), text); }
UINT GetDlgItemTextA(HWND dialog, int id, LPSTR text, int size) { return static_cast<UINT>(GetWindowTextA(GetDlgItem(dialog, id), text, size)); }
UINT GetDlgItemTextW(HWND dialog, int id, LPWSTR text, int size) { return static_cast<UINT>(GetWindowTextW(GetDlgItem(dialog, id), text, size)); }

BOOL SetDlgItemInt(HWND dialog, int id, UINT value, BOOL isSigned)
{
    const QString text = isSigned ? QString::number(static_cast<int>(value)) : QString::number(value);
    return SetWindowTextW(GetDlgItem(dialog, id), text.toStdWString().c_str());
}

UINT GetDlgItemInt(HWND dialog, int id, BOOL* translated, BOOL isSigned)
{
    const QString text = textOf(GetDlgItem(dialog, id)).trimmed();
    bool ok = false;
    const long long value = text.toLongLong(&ok);
    const bool valid = ok && (isSigned || value >= 0) && value <= (isSigned ? INT_MAX : static_cast<long long>(UINT_MAX))
                       && value >= (isSigned ? INT_MIN : 0);
    if (translated)
        *translated = valid ? TRUE : FALSE;
    return valid ? static_cast<UINT>(value) : 0u;
}

LRESULT SendDlgItemMessageW(HWND dialog, int id, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND item = GetDlgItem(dialog, id);
    return item ? edita_compat_SendMessage(item, message, wParam, lParam) : 0;
}

LRESULT SendDlgItemMessageA(HWND dialog, int id, UINT message, WPARAM wParam, LPARAM lParam)
{
    return SendDlgItemMessageW(dialog, id, message, wParam, lParam);
}

BOOL CheckDlgButton(HWND dialog, int id, UINT check)
{
    Window* w = windowOf(GetDlgItem(dialog, id));
    if (!w)
        return FALSE;
    send(w, BM_SETCHECK, check, 0);
    return TRUE;
}

UINT IsDlgButtonChecked(HWND dialog, int id)
{
    Window* w = windowOf(GetDlgItem(dialog, id));
    return w ? static_cast<UINT>(send(w, BM_GETCHECK, 0, 0)) : 0u;
}

BOOL CheckRadioButton(HWND dialog, int first, int last, int check)
{
    for (int id = first; id <= last; ++id) {
        if (Window* w = windowOf(GetDlgItem(dialog, id)))
            send(w, BM_SETCHECK, id == check ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    return TRUE;
}

// --- Geometría, visibilidad, jerarquía y foco --------------------------------------------------

BOOL ShowWindow(HWND hwnd, int command)
{
    QWidget* widget = widgetOf(hwnd);
    if (!widget)
        return FALSE;
    const BOOL wasVisible = widget->isVisible() ? TRUE : FALSE;
    switch (command) {
    case SW_HIDE: widget->hide(); break;
    case SW_SHOWMINIMIZED:
    case SW_MINIMIZE: widget->showMinimized(); break;
    case SW_SHOWMAXIMIZED: widget->showMaximized(); break;
    case SW_RESTORE: widget->showNormal(); break;
    case SW_SHOWNOACTIVATE:
    case SW_SHOWNA:
        widget->setAttribute(Qt::WA_ShowWithoutActivating);
        widget->show();
        break;
    default:
        widget->show();
        if (widget->isWindow())
            widget->raise();
        break;
    }
    return wasVisible;
}

BOOL UpdateWindow(HWND hwnd)
{
    QWidget* widget = widgetOf(hwnd);
    if (widget)
        widget->update();
    return widget ? TRUE : FALSE;
}

BOOL InvalidateRect(HWND hwnd, const RECT*, BOOL)
{
    return UpdateWindow(hwnd);
}

BOOL RedrawWindow(HWND hwnd, const RECT*, void*, UINT)
{
    return UpdateWindow(hwnd);
}

BOOL MoveWindow(HWND hwnd, int x, int y, int width, int height, BOOL)
{
    QWidget* widget = widgetOf(hwnd);
    if (!widget)
        return FALSE;
    if (widget->isWindow()) {
        widget->move(x, y);
        widget->resize(std::max(0, width), std::max(0, height));
    } else {
        widget->setGeometry(x, y, std::max(0, width), std::max(0, height));
    }
    return TRUE;
}

BOOL SetWindowPos(HWND hwnd, HWND after, int x, int y, int width, int height, UINT flags)
{
    QWidget* widget = widgetOf(hwnd);
    if (!widget)
        return FALSE;
    if (!(flags & SWP_NOMOVE))
        widget->move(x, y);
    if (!(flags & SWP_NOSIZE))
        widget->resize(std::max(0, width), std::max(0, height));
    if (!(flags & SWP_NOZORDER) && widget->isWindow()) {
        if (after == HWND_TOPMOST || after == HWND_NOTOPMOST) {
            const bool visible = widget->isVisible();
            widget->setWindowFlag(Qt::WindowStaysOnTopHint, after == HWND_TOPMOST);
            if (visible)
                widget->show();
        } else if (after == HWND_TOP) {
            widget->raise();
        } else if (after == HWND_BOTTOM) {
            widget->lower();
        }
    }
    if (flags & SWP_SHOWWINDOW)
        widget->show();
    if (flags & SWP_HIDEWINDOW)
        widget->hide();
    return TRUE;
}

BOOL GetWindowRect(HWND hwnd, LPRECT rect)
{
    if (!rect)
        return FALSE;
    if (hwnd == desktopHwnd()) {
        *rect = rectOf(QGuiApplication::primaryScreen()->virtualGeometry());
        return TRUE;
    }
    QWidget* widget = widgetOf(hwnd);
    if (!widget) {
        *rect = RECT{0, 0, 0, 0};
        return FALSE;
    }
    if (widget->isWindow()) {
        *rect = rectOf(widget->frameGeometry());
    } else {
        const QPoint topLeft = widget->mapToGlobal(QPoint(0, 0));
        *rect = rectOf(QRect(topLeft, widget->size()));
    }
    return TRUE;
}

BOOL GetClientRect(HWND hwnd, LPRECT rect)
{
    if (!rect)
        return FALSE;
    if (hwnd == desktopHwnd()) {
        const QRect screen = QGuiApplication::primaryScreen()->virtualGeometry();
        *rect = RECT{0, 0, screen.width(), screen.height()};
        return TRUE;
    }
    QWidget* widget = widgetOf(hwnd);
    *rect = widget ? RECT{0, 0, widget->width(), widget->height()} : RECT{0, 0, 0, 0};
    return widget ? TRUE : FALSE;
}

BOOL ClientToScreen(HWND hwnd, LPPOINT point)
{
    QWidget* widget = widgetOf(hwnd);
    if (!widget || !point)
        return FALSE;
    const QPoint p = widget->mapToGlobal(QPoint(static_cast<int>(point->x), static_cast<int>(point->y)));
    point->x = p.x();
    point->y = p.y();
    return TRUE;
}

BOOL ScreenToClient(HWND hwnd, LPPOINT point)
{
    QWidget* widget = widgetOf(hwnd);
    if (!widget || !point)
        return FALSE;
    const QPoint p = widget->mapFromGlobal(QPoint(static_cast<int>(point->x), static_cast<int>(point->y)));
    point->x = p.x();
    point->y = p.y();
    return TRUE;
}

int MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count)
{
    QWidget* source = widgetOf(from);
    QWidget* target = widgetOf(to);
    const QPoint origin = source ? source->mapToGlobal(QPoint(0, 0)) : QPoint(0, 0);
    const QPoint destination = target ? target->mapToGlobal(QPoint(0, 0)) : QPoint(0, 0);
    const QPoint delta = origin - destination;
    for (UINT i = 0; points && i < count; ++i) {
        points[i].x += delta.x();
        points[i].y += delta.y();
    }
    return static_cast<int>(MAKELONG(static_cast<WORD>(static_cast<short>(delta.x())), static_cast<WORD>(static_cast<short>(delta.y()))));
}

// Sin marco de Windows: el rectángulo del cliente es el de la ventana
BOOL AdjustWindowRect(LPRECT rect, DWORD, BOOL) { return rect ? TRUE : FALSE; }
BOOL AdjustWindowRectEx(LPRECT rect, DWORD, BOOL, DWORD) { return rect ? TRUE : FALSE; }

HWND GetParent(HWND hwnd)
{
    if (Window* w = windowOf(hwnd))
        return w->parent;
    const auto& foreign = registry().foreign;
    const auto it = foreign.find(hwnd);
    return it != foreign.end() ? it->second.parent : nullptr;
}

HWND SetParent(HWND hwnd, HWND parent)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return nullptr;
    HWND previous = w->parent;
    if (Window* old = windowOf(previous)) {
        auto& siblings = old->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), w), siblings.end());
    }
    w->parent = parent;
    if (Window* p = windowOf(parent); p && (w->style & WS_CHILD))
        p->children.push_back(w);
    if (QWidget* widget = w->widget) {
        const bool visible = widget->isVisible();
        widget->setParent(widgetOf(parent), (w->style & WS_CHILD) ? Qt::Widget : widget->windowFlags());
        widget->setVisible(visible);
    }
    return previous;
}

HWND GetAncestor(HWND hwnd, UINT flags)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return nullptr;
    if (flags == GA_PARENT)
        return w->parent;
    Window* root = w;
    while ((root->style & WS_CHILD) && windowOf(root->parent))
        root = windowOf(root->parent);
    if (flags == GA_ROOTOWNER) {
        while (windowOf(root->parent))
            root = windowOf(root->parent);
    }
    return hwndOf(root);
}

HWND GetWindow(HWND hwnd, UINT command)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return nullptr;
    if (command == GW_CHILD)
        return w->children.empty() ? nullptr : hwndOf(w->children.front());
    if (command == GW_OWNER)
        return (w->style & WS_CHILD) ? nullptr : w->parent;
    Window* parent = windowOf(w->parent);
    if (!parent || !(w->style & WS_CHILD))
        return nullptr;
    const auto& siblings = parent->children;
    const auto it = std::find(siblings.begin(), siblings.end(), w);
    switch (command) {
    case GW_HWNDFIRST: return hwndOf(siblings.front());
    case GW_HWNDLAST: return hwndOf(siblings.back());
    case GW_HWNDNEXT: return it + 1 < siblings.end() ? hwndOf(*(it + 1)) : nullptr;
    case GW_HWNDPREV: return it > siblings.begin() ? hwndOf(*(it - 1)) : nullptr;
    default: return nullptr;
    }
}

static bool enumChildren(Window* w, WNDENUMPROC proc, LPARAM lParam)
{
    const std::vector<Window*> children = w->children;
    for (Window* child : children) {
        if (!child->alive)
            continue;
        if (!proc(hwndOf(child), lParam))
            return false;
        if (!enumChildren(child, proc, lParam))
            return false;
    }
    return true;
}

BOOL EnumChildWindows(HWND parent, WNDENUMPROC proc, LPARAM lParam)
{
    Window* w = windowOf(parent);
    if (!w || !proc)
        return FALSE;
    enumChildren(w, proc, lParam);
    return TRUE;
}

BOOL IsWindow(HWND hwnd)
{
    return windowOf(hwnd) || widgetOf(hwnd) ? TRUE : FALSE;
}

BOOL IsWindowVisible(HWND hwnd)
{
    QWidget* widget = widgetOf(hwnd);
    return widget && widget->isVisible() ? TRUE : FALSE;
}

BOOL IsWindowEnabled(HWND hwnd)
{
    QWidget* widget = widgetOf(hwnd);
    return widget && widget->isEnabled() ? TRUE : FALSE;
}

BOOL EnableWindow(HWND hwnd, BOOL enable)
{
    QWidget* widget = widgetOf(hwnd);
    if (!widget)
        return FALSE;
    const BOOL wasDisabled = widget->isEnabled() ? FALSE : TRUE;
    widget->setEnabled(enable != FALSE);
    if (Window* w = windowOf(hwnd)) {
        if (enable)
            w->style &= ~static_cast<DWORD>(WS_DISABLED);
        else
            w->style |= WS_DISABLED;
    }
    return wasDisabled;
}

BOOL IsChild(HWND parent, HWND hwnd)
{
    for (Window* w = windowOf(hwnd); w && (w->style & WS_CHILD); w = windowOf(w->parent)) {
        if (w->parent == parent)
            return TRUE;
    }
    return FALSE;
}

BOOL IsIconic(HWND hwnd)
{
    QWidget* widget = widgetOf(hwnd);
    return widget && widget->isMinimized() ? TRUE : FALSE;
}

BOOL IsZoomed(HWND hwnd)
{
    QWidget* widget = widgetOf(hwnd);
    return widget && widget->isMaximized() ? TRUE : FALSE;
}

HWND GetFocus(void)
{
    return hwndOfWidget(QApplication::focusWidget());
}

HWND SetFocus(HWND hwnd)
{
    HWND previous = GetFocus();
    if (QWidget* widget = widgetOf(hwnd))
        widget->setFocus(Qt::OtherFocusReason);
    return previous;
}

HWND GetActiveWindow(void)
{
    return hwndOfWidget(QApplication::activeWindow());
}

HWND SetActiveWindow(HWND hwnd)
{
    HWND previous = GetActiveWindow();
    if (QWidget* widget = widgetOf(hwnd))
        widget->window()->activateWindow();
    return previous;
}

HWND GetForegroundWindow(void)
{
    return GetActiveWindow();
}

BOOL SetForegroundWindow(HWND hwnd)
{
    QWidget* widget = widgetOf(hwnd);
    if (!widget)
        return FALSE;
    widget->window()->raise();
    widget->window()->activateWindow();
    return TRUE;
}

BOOL BringWindowToTop(HWND hwnd)
{
    QWidget* widget = widgetOf(hwnd);
    if (!widget)
        return FALSE;
    widget->raise();
    return TRUE;
}

HWND GetDesktopWindow(void)
{
    return desktopHwnd();
}

HWND SetCapture(HWND hwnd)
{
    HWND previous = registry().capture;
    if (QWidget* widget = widgetOf(hwnd)) {
        widget->grabMouse();
        registry().capture = hwnd;
    }
    return previous;
}

BOOL ReleaseCapture(void)
{
    if (QWidget* widget = widgetOf(registry().capture))
        widget->releaseMouse();
    registry().capture = nullptr;
    return TRUE;
}

HWND GetCapture(void)
{
    return registry().capture;
}

// --- Temporizadores --------------------------------------------------------------------------------

UINT_PTR SetTimer(HWND hwnd, UINT_PTR id, UINT elapse, TIMERPROC proc)
{
    Window* w = windowOf(hwnd);
    if (hwnd && !w)
        return 0;
    auto& timers = w ? w->timers : registry().threadTimers;
    if (!w)
        id = ++registry().nextTimer;
    if (QTimer* existing = timers[id]) {
        delete existing;
    }
    auto* timer = new QTimer;
    timer->setInterval(static_cast<int>(std::max<UINT>(elapse, 10)));
    QObject::connect(timer, &QTimer::timeout, timer, [hwnd, id, proc] { fireTimer(hwnd, id, proc); });
    timer->start();
    timers[id] = timer;
    return w ? (id ? id : 1) : id;
}

BOOL KillTimer(HWND hwnd, UINT_PTR id)
{
    Window* w = windowOf(hwnd);
    if (hwnd && !w)
        return FALSE;
    auto& timers = w ? w->timers : registry().threadTimers;
    const auto it = timers.find(id);
    if (it == timers.end())
        return FALSE;
    it->second->stop();
    it->second->deleteLater();
    timers.erase(it);
    return TRUE;
}

// --- Sistema -----------------------------------------------------------------------------------

int GetSystemMetrics(int index)
{
    QScreen* primary = QGuiApplication::primaryScreen();
    const QRect screen = primary ? primary->geometry() : QRect(0, 0, 1920, 1080);
    const QRect available = primary ? primary->availableGeometry() : screen;
    const QRect virtualScreen = primary ? primary->virtualGeometry() : screen;
    QStyle* style = QApplication::style();
    const int scrollBar = style ? style->pixelMetric(QStyle::PM_ScrollBarExtent) : 16;
    const int caption = style ? style->pixelMetric(QStyle::PM_TitleBarHeight) : 23;
    switch (index) {
    case SM_CXSCREEN: return screen.width();
    case SM_CYSCREEN: return screen.height();
    case SM_CXFULLSCREEN:
    case SM_CXMAXIMIZED: return available.width();
    case SM_CYFULLSCREEN:
    case SM_CYMAXIMIZED: return available.height();
    case SM_XVIRTUALSCREEN: return virtualScreen.left();
    case SM_YVIRTUALSCREEN: return virtualScreen.top();
    case SM_CXVIRTUALSCREEN: return virtualScreen.width();
    case SM_CYVIRTUALSCREEN: return virtualScreen.height();
    case SM_CXVSCROLL:
    case SM_CYHSCROLL:
    case SM_CYVSCROLL:
    case SM_CXHSCROLL:
    case SM_CYVTHUMB:
    case SM_CXHTHUMB: return scrollBar;
    case SM_CYCAPTION: return caption;
    case SM_CYSMCAPTION: return std::max(16, caption - 4);
    case SM_CYMENU: return QFontMetrics(QApplication::font()).height() + 8;
    case SM_CXBORDER:
    case SM_CYBORDER: return 1;
    case SM_CXDLGFRAME:
    case SM_CYDLGFRAME:
    case SM_CXEDGE:
    case SM_CYEDGE: return 2;
    case SM_CXFRAME:
    case SM_CYFRAME: return 4;
    case SM_CXPADDEDBORDER: return 4;
    case SM_CXICON:
    case SM_CYICON:
    case SM_CXCURSOR:
    case SM_CYCURSOR: return 32;
    case SM_CXSMICON:
    case SM_CYSMICON:
    case SM_CXMENUCHECK:
    case SM_CYMENUCHECK: return 16;
    case SM_CXSIZE:
    case SM_CYSIZE: return caption;
    case SM_CXMIN: return 112;
    case SM_CYMIN: return 27;
    case SM_CXMINTRACK: return 112;
    case SM_CYMINTRACK: return 27;
    case SM_CXDOUBLECLK:
    case SM_CYDOUBLECLK: return 4;
    case SM_CMONITORS: return static_cast<int>(QGuiApplication::screens().size());
    case SM_MOUSEWHEELPRESENT: return 1;
    case SM_SWAPBUTTON:
    case SM_REMOTESESSION: return 0;
    default: return 0;
    }
}

extern "C++" {
template<typename LogFont>
static void fillLogFont(const QFont& font, LogFont* out)
{
    const LOGFONTW wide = logFontOf(font);
    std::memcpy(out, &wide, offsetof(LOGFONTW, lfFaceName));
    using Ch = std::remove_reference_t<decltype(out->lfFaceName[0])>;
    if constexpr (sizeof(Ch) == 1)
        copyTo(font.family().toStdString(), out->lfFaceName, LF_FACESIZE);
    else
        copyTo(font.family().toStdWString(), out->lfFaceName, LF_FACESIZE);
}
}

extern "C++" {
template<typename Metrics>
static BOOL systemParameters(UINT action, UINT, PVOID data)
{
    if (!data)
        return FALSE;
    switch (action) {
    case SPI_GETWORKAREA: {
        QScreen* primary = QGuiApplication::primaryScreen();
        *static_cast<RECT*>(data) = rectOf(primary ? primary->availableGeometry() : QRect());
        return TRUE;
    }
    case SPI_GETWHEELSCROLLLINES:
        *static_cast<UINT*>(data) = static_cast<UINT>(QApplication::wheelScrollLines());
        return TRUE;
    case SPI_GETNONCLIENTMETRICS: {
        auto* metrics = static_cast<Metrics*>(data);
        const QFont font = QApplication::font();
        metrics->iBorderWidth = 1;
        metrics->iScrollWidth = metrics->iScrollHeight = GetSystemMetrics(SM_CXVSCROLL);
        metrics->iCaptionWidth = metrics->iCaptionHeight = GetSystemMetrics(SM_CYCAPTION);
        metrics->iSmCaptionWidth = metrics->iSmCaptionHeight = GetSystemMetrics(SM_CYSMCAPTION);
        metrics->iMenuWidth = metrics->iMenuHeight = GetSystemMetrics(SM_CYMENU);
        fillLogFont(font, &metrics->lfCaptionFont);
        fillLogFont(font, &metrics->lfSmCaptionFont);
        fillLogFont(font, &metrics->lfMenuFont);
        fillLogFont(font, &metrics->lfStatusFont);
        fillLogFont(font, &metrics->lfMessageFont);
        return TRUE;
    }
    case SPI_GETFONTSMOOTHING:
        *static_cast<BOOL*>(data) = TRUE;
        return TRUE;
    default:
        return FALSE;
    }
}
}

BOOL SystemParametersInfoA(UINT action, UINT param, PVOID data, UINT)
{
    if (action == SPI_GETICONTITLELOGFONT && data) {
        fillLogFont(QApplication::font(), static_cast<LOGFONTA*>(data));
        return TRUE;
    }
    return systemParameters<NONCLIENTMETRICSA>(action, param, data);
}

BOOL SystemParametersInfoW(UINT action, UINT param, PVOID data, UINT)
{
    if (action == SPI_GETICONTITLELOGFONT && data) {
        fillLogFont(QApplication::font(), static_cast<LOGFONTW*>(data));
        return TRUE;
    }
    return systemParameters<NONCLIENTMETRICSW>(action, param, data);
}

HMONITOR MonitorFromWindow(HWND hwnd, DWORD flags)
{
    QWidget* widget = widgetOf(hwnd);
    if (widget && widget->screen())
        return widget->screen();
    return flags == MONITOR_DEFAULTTONULL ? nullptr : QGuiApplication::primaryScreen();
}

HMONITOR MonitorFromRect(LPCRECT rect, DWORD flags)
{
    if (!rect)
        return nullptr;
    const QPoint center(static_cast<int>((rect->left + rect->right) / 2), static_cast<int>((rect->top + rect->bottom) / 2));
    return screenFor(center, flags);
}

HMONITOR MonitorFromPoint(POINT point, DWORD flags)
{
    return screenFor(QPoint(static_cast<int>(point.x), static_cast<int>(point.y)), flags);
}

extern "C++" {
template<typename InfoEx>
static BOOL monitorInfo(HMONITOR monitor, LPMONITORINFO info)
{
    auto* screen = static_cast<QScreen*>(monitor);
    if (!screen || !info || !QGuiApplication::screens().contains(screen))
        return FALSE;
    info->rcMonitor = rectOf(screen->geometry());
    info->rcWork = rectOf(screen->availableGeometry());
    info->dwFlags = screen == QGuiApplication::primaryScreen() ? MONITORINFOF_PRIMARY : 0;
    if (info->cbSize >= sizeof(InfoEx)) {
        auto* ex = reinterpret_cast<InfoEx*>(info);
        using Ch = std::remove_reference_t<decltype(ex->szDevice[0])>;
        if constexpr (sizeof(Ch) == 1)
            copyTo(screen->name().toStdString(), ex->szDevice, CCHDEVICENAME);
        else
            copyTo(screen->name().toStdWString(), ex->szDevice, CCHDEVICENAME);
    }
    return TRUE;
}
}

BOOL GetMonitorInfoA(HMONITOR monitor, LPMONITORINFO info) { return monitorInfo<MONITORINFOEXA>(monitor, info); }
BOOL GetMonitorInfoW(HMONITOR monitor, LPMONITORINFO info) { return monitorInfo<MONITORINFOEXW>(monitor, info); }

// Las coordenadas son píxeles lógicos de Qt: para el plugin, 96 ppp
UINT GetDpiForWindow(HWND) { return USER_DEFAULT_SCREEN_DPI; }
UINT GetDpiForSystem(void) { return USER_DEFAULT_SCREEN_DPI; }

BOOL GetCursorPos(LPPOINT point)
{
    if (!point)
        return FALSE;
    const QPoint p = QCursor::pos();
    point->x = p.x();
    point->y = p.y();
    return TRUE;
}

BOOL SetCursorPos(int x, int y)
{
    QCursor::setPos(x, y);
    return TRUE;
}

// Los cursores del sistema son sus números (IDC_*); IDC_WAIT pone el reloj de Qt
HCURSOR SetCursor(HCURSOR cursor)
{
    static HCURSOR current = reinterpret_cast<HCURSOR>(static_cast<uintptr_t>(32512));
    HCURSOR previous = current;
    current = cursor;
    const auto id = reinterpret_cast<uintptr_t>(cursor);
    Registry& r = registry();
    if (id == 32514 || id == 32650) {
        if (r.cursorOverrides == 0) {
            QGuiApplication::setOverrideCursor(id == 32514 ? Qt::WaitCursor : Qt::BusyCursor);
            ++r.cursorOverrides;
        }
    } else {
        while (r.cursorOverrides > 0) {
            QGuiApplication::restoreOverrideCursor();
            --r.cursorOverrides;
        }
    }
    return previous;
}

HCURSOR LoadCursorA(HINSTANCE instance, LPCSTR name) { return !instance && IS_INTRESOURCE(name) ? reinterpret_cast<HCURSOR>(const_cast<char*>(name)) : nullptr; }
HCURSOR LoadCursorW(HINSTANCE instance, LPCWSTR name) { return !instance && IS_INTRESOURCE(name) ? reinterpret_cast<HCURSOR>(const_cast<wchar_t*>(name)) : nullptr; }
HICON LoadIconA(HINSTANCE instance, LPCSTR name) { return !instance && IS_INTRESOURCE(name) ? reinterpret_cast<HICON>(const_cast<char*>(name)) : nullptr; }
HICON LoadIconW(HINSTANCE instance, LPCWSTR name) { return !instance && IS_INTRESOURCE(name) ? reinterpret_cast<HICON>(const_cast<wchar_t*>(name)) : nullptr; }
HANDLE LoadImageA(HINSTANCE, LPCSTR, UINT, int, int, UINT) { return nullptr; }
HANDLE LoadImageW(HINSTANCE, LPCWSTR, UINT, int, int, UINT) { return nullptr; }
HBITMAP LoadBitmapA(HINSTANCE, LPCSTR) { return nullptr; }
HBITMAP LoadBitmapW(HINSTANCE, LPCWSTR) { return nullptr; }
BOOL DestroyIcon(HICON) { return TRUE; }
BOOL DestroyCursor(HCURSOR) { return TRUE; }

// --- Recursos ----------------------------------------------------------------------------------------

HRSRC FindResourceA(HMODULE, LPCSTR, LPCSTR)
{
    SetLastError(1814); // ERROR_RESOURCE_NAME_NOT_FOUND
    return nullptr;
}

HRSRC FindResourceW(HMODULE, LPCWSTR, LPCWSTR)
{
    SetLastError(1814);
    return nullptr;
}

HGLOBAL LoadResource(HMODULE, HRSRC) { return nullptr; }
LPVOID LockResource(HGLOBAL resource) { return resource; }
DWORD SizeofResource(HMODULE, HRSRC) { return 0; }
BOOL FreeResource(HGLOBAL) { return FALSE; }

int LoadStringA(HINSTANCE, UINT, LPSTR buffer, int size)
{
    if (buffer && size > 0)
        buffer[0] = '\0';
    return 0;
}

int LoadStringW(HINSTANCE, UINT, LPWSTR buffer, int size)
{
    if (buffer && size > 0)
        buffer[0] = L'\0';
    return 0;
}

// --- Menús ---------------------------------------------------------------------------------------------

HMENU CreatePopupMenu(void)
{
    auto* m = new Menu;
    m->menu = new QMenu;
    return m;
}

HMENU CreateMenu(void)
{
    return CreatePopupMenu();
}

BOOL DestroyMenu(HMENU handle)
{
    Menu* m = menuOf(handle);
    if (!m || m->host)
        return FALSE;
    delete m->menu;
    m->menu = nullptr;
    m->magic = 0;
    delete m;
    return TRUE;
}

static BOOL insertMenu(HMENU handle, UINT position, UINT flags, UINT_PTR id, const QString& text, bool append)
{
    Menu* m = menuOf(handle);
    if (!m || !m->menu)
        return FALSE;
    QAction* action = makeAction(m->menu, flags, id, text);
    if (!action)
        return FALSE;
    QAction* before = append ? nullptr : findAction(m->menu, position, flags & MF_BYPOSITION);
    m->menu->insertAction(before, action);
    return TRUE;
}

BOOL AppendMenuA(HMENU menu, UINT flags, UINT_PTR id, LPCSTR text)
{
    return insertMenu(menu, 0, flags, id, (flags & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW)) ? QString() : fromA(text), true);
}

BOOL AppendMenuW(HMENU menu, UINT flags, UINT_PTR id, LPCWSTR text)
{
    return insertMenu(menu, 0, flags, id, (flags & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW)) ? QString() : fromW(text), true);
}

BOOL InsertMenuA(HMENU menu, UINT position, UINT flags, UINT_PTR id, LPCSTR text)
{
    return insertMenu(menu, position, flags, id, (flags & MF_SEPARATOR) ? QString() : fromA(text), false);
}

BOOL InsertMenuW(HMENU menu, UINT position, UINT flags, UINT_PTR id, LPCWSTR text)
{
    return insertMenu(menu, position, flags, id, (flags & MF_SEPARATOR) ? QString() : fromW(text), false);
}

BOOL RemoveMenu(HMENU handle, UINT position, UINT flags)
{
    Menu* m = menuOf(handle);
    QAction* action = m ? findAction(m->menu, position, flags) : nullptr;
    if (!action)
        return FALSE;
    m->menu->removeAction(action);
    return TRUE;
}

BOOL DeleteMenu(HMENU handle, UINT position, UINT flags)
{
    Menu* m = menuOf(handle);
    QAction* action = m ? findAction(m->menu, position, flags) : nullptr;
    if (!action)
        return FALSE;
    m->menu->removeAction(action);
    if (!action->menu())
        delete action;
    return TRUE;
}

DWORD CheckMenuItem(HMENU handle, UINT item, UINT check)
{
    Menu* m = menuOf(handle);
    if (!m)
        return static_cast<DWORD>(-1);
    if (m->host) {
        // El menú de Edita: las órdenes del plugin (por su cmdID)
        if (check & MF_BYPOSITION)
            return static_cast<DWORD>(-1);
        edita_compat_SendMessage(edita_compat_hostHwnd(), WM_USER + 1000 + 40 /* NPPM_SETMENUITEMCHECK */, item,
                                 (check & MF_CHECKED) ? TRUE : FALSE);
        return MF_UNCHECKED;
    }
    QAction* action = findAction(m->menu, item, check);
    if (!action)
        return static_cast<DWORD>(-1);
    const DWORD previous = action->isChecked() ? MF_CHECKED : MF_UNCHECKED;
    action->setCheckable(true);
    action->setChecked(check & MF_CHECKED);
    return previous;
}

BOOL EnableMenuItem(HMENU handle, UINT item, UINT enable)
{
    Menu* m = menuOf(handle);
    if (!m || m->host)
        return m ? MF_ENABLED : -1;
    QAction* action = findAction(m->menu, item, enable);
    if (!action)
        return -1;
    const BOOL previous = action->isEnabled() ? MF_ENABLED : MF_GRAYED;
    action->setEnabled(!(enable & (MF_GRAYED | MF_DISABLED)));
    return previous;
}

UINT GetMenuState(HMENU handle, UINT item, UINT flags)
{
    Menu* m = menuOf(handle);
    QAction* action = m && !m->host ? findAction(m->menu, item, flags) : nullptr;
    if (!action)
        return static_cast<UINT>(-1);
    UINT state = 0;
    if (action->isChecked())
        state |= MF_CHECKED;
    if (!action->isEnabled())
        state |= MF_GRAYED;
    if (action->isSeparator())
        state |= MF_SEPARATOR;
    if (action->menu())
        state |= MF_POPUP;
    return state;
}

int GetMenuItemCount(HMENU handle)
{
    Menu* m = menuOf(handle);
    return m && m->menu ? static_cast<int>(m->menu->actions().size()) : -1;
}

UINT GetMenuItemID(HMENU handle, int position)
{
    Menu* m = menuOf(handle);
    QAction* action = m && position >= 0 ? findAction(m->menu, static_cast<UINT>(position), MF_BYPOSITION) : nullptr;
    if (!action || action->menu())
        return static_cast<UINT>(-1);
    return action->isSeparator() ? 0 : static_cast<UINT>(action->data().toULongLong());
}

HMENU GetSubMenu(HMENU handle, int position)
{
    Menu* m = menuOf(handle);
    QAction* action = m && position >= 0 ? findAction(m->menu, static_cast<UINT>(position), MF_BYPOSITION) : nullptr;
    if (!action || !action->menu())
        return nullptr;
    // El Menu que lo creó (CreatePopupMenu) es el que guarda el QMenu
    auto* sub = new Menu;
    sub->menu = action->menu();
    return sub;
}

HMENU GetMenu(HWND hwnd)
{
    return hwnd && hwnd == edita_compat_hostHwnd() ? hostMenu() : nullptr;
}

BOOL DrawMenuBar(HWND) { return TRUE; }

static UINT_PTR trackMenu(HMENU handle, UINT flags, int x, int y, HWND hwnd)
{
    Menu* m = menuOf(handle);
    if (!m || !m->menu)
        return 0;
    QAction* chosen = m->menu->exec(QPoint(x, y));
    const UINT_PTR id = chosen && !chosen->menu() && !chosen->isSeparator() ? static_cast<UINT_PTR>(chosen->data().toULongLong()) : 0;
    if (!(flags & TPM_RETURNCMD) && id && hwnd && !(flags & TPM_NONOTIFY))
        edita_compat_PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(static_cast<WORD>(id), 0), 0);
    return id;
}

BOOL TrackPopupMenu(HMENU menu, UINT flags, int x, int y, int, HWND hwnd, const RECT*)
{
    const UINT_PTR id = trackMenu(menu, flags, x, y, hwnd);
    return (flags & TPM_RETURNCMD) ? static_cast<BOOL>(id) : TRUE;
}

BOOL TrackPopupMenuEx(HMENU menu, UINT flags, int x, int y, HWND hwnd, void*)
{
    return TrackPopupMenu(menu, flags, x, y, 0, hwnd, nullptr);
}

// --- GDI sin dibujo --------------------------------------------------------------------------------------

HDC GetDC(HWND hwnd)
{
    auto* dc = new DeviceContext;
    dc->widget = widgetOf(hwnd);
    return dc;
}

HDC GetWindowDC(HWND hwnd) { return GetDC(hwnd); }

int ReleaseDC(HWND, HDC handle)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return 0;
    dc->magic = 0;
    delete dc;
    return 1;
}

BOOL DeleteDC(HDC dc) { return ReleaseDC(nullptr, dc) ? TRUE : FALSE; }

HGDIOBJ SelectObject(HDC handle, HGDIOBJ object)
{
    DeviceContext* dc = deviceContext(handle);
    GdiObject* o = gdiObject(object);
    if (!dc || !o)
        return nullptr;
    if (o->type != GdiObject::Type::Font)
        return GetStockObject(o->type == GdiObject::Type::Brush ? WHITE_BRUSH : BLACK_PEN);
    HGDIOBJ previous = dc->font ? dc->font : GetStockObject(SYSTEM_FONT);
    dc->font = object;
    return previous;
}

BOOL DeleteObject(HGDIOBJ object)
{
    GdiObject* o = gdiObject(object);
    if (!o)
        return FALSE;
    if (!o->stock) {
        o->magic = 0;
        delete o;
    }
    return TRUE;
}

HGDIOBJ GetStockObject(int index)
{
    static std::map<int, GdiObject*> stock;
    GdiObject*& o = stock[index];
    if (!o) {
        o = new GdiObject;
        o->stock = true;
        if (index == OEM_FIXED_FONT || index == ANSI_FIXED_FONT || index == SYSTEM_FIXED_FONT) {
            o->type = GdiObject::Type::Font;
            o->font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        } else if (index >= OEM_FIXED_FONT && index <= DEFAULT_GUI_FONT) {
            o->type = GdiObject::Type::Font;
            o->font = QApplication::font();
        } else if (index == WHITE_PEN || index == BLACK_PEN || index == NULL_PEN || index == DC_PEN) {
            o->type = GdiObject::Type::Pen;
            o->color = index == WHITE_PEN ? 0xFFFFFF : 0;
        } else {
            o->type = GdiObject::Type::Brush;
            static const COLORREF colors[] = {0xFFFFFF, 0xC0C0C0, 0x808080, 0x404040, 0x000000, 0};
            o->color = index >= 0 && index <= NULL_BRUSH ? colors[index] : 0xFFFFFF;
        }
        o->logFont = logFontOf(o->font);
    }
    return o;
}

static int getObject(HANDLE object, int size, LPVOID buffer, bool wide)
{
    GdiObject* o = gdiObject(object);
    if (!o)
        return 0;
    if (o->type == GdiObject::Type::Font) {
        const int needed = wide ? static_cast<int>(sizeof(LOGFONTW)) : static_cast<int>(sizeof(LOGFONTA));
        if (!buffer)
            return needed;
        if (size < needed)
            return 0;
        if (wide)
            fillLogFont(o->font, static_cast<LOGFONTW*>(buffer));
        else
            fillLogFont(o->font, static_cast<LOGFONTA*>(buffer));
        // La altura con la que se creó (fillLogFont la deduce de la fuente)
        static_cast<LOGFONTW*>(buffer)->lfHeight = o->logFont.lfHeight;
        return needed;
    }
    return 0;
}

int GetObjectA(HANDLE object, int size, LPVOID buffer) { return getObject(object, size, buffer, false); }
int GetObjectW(HANDLE object, int size, LPVOID buffer) { return getObject(object, size, buffer, true); }

HFONT CreateFontW(int height, int, int, int, int weight, DWORD italic, DWORD underline, DWORD strikeOut, DWORD, DWORD, DWORD,
                  DWORD, DWORD, LPCWSTR face)
{
    const QFont font = fontFromLogFont(height, weight, italic, underline, strikeOut, fromW(face));
    LOGFONTW lf = logFontOf(font);
    lf.lfHeight = height;
    return makeFont(font, lf);
}

HFONT CreateFontA(int height, int width, int escapement, int orientation, int weight, DWORD italic, DWORD underline,
                  DWORD strikeOut, DWORD charSet, DWORD outPrecision, DWORD clipPrecision, DWORD quality,
                  DWORD pitchAndFamily, LPCSTR face)
{
    const std::wstring wideFace = fromA(face).toStdWString();
    return CreateFontW(height, width, escapement, orientation, weight, italic, underline, strikeOut, charSet, outPrecision,
                       clipPrecision, quality, pitchAndFamily, face ? wideFace.c_str() : nullptr);
}

HFONT CreateFontIndirectW(const LOGFONTW* lf)
{
    if (!lf)
        return nullptr;
    const QFont font = fontFromLogFont(lf->lfHeight, lf->lfWeight, lf->lfItalic, lf->lfUnderline, lf->lfStrikeOut,
                                       fromW(lf->lfFaceName));
    return makeFont(font, *lf);
}

HFONT CreateFontIndirectA(const LOGFONTA* lf)
{
    if (!lf)
        return nullptr;
    LOGFONTW wide{};
    std::memcpy(&wide, lf, offsetof(LOGFONTA, lfFaceName));
    copyTo(fromA(lf->lfFaceName).toStdWString(), wide.lfFaceName, LF_FACESIZE);
    return CreateFontIndirectW(&wide);
}

HBRUSH CreateSolidBrush(COLORREF color)
{
    auto* o = new GdiObject;
    o->type = GdiObject::Type::Brush;
    o->color = color;
    return o;
}

HPEN CreatePen(int, int, COLORREF color)
{
    auto* o = new GdiObject;
    o->type = GdiObject::Type::Pen;
    o->color = color;
    return o;
}

DWORD GetSysColor(int index)
{
    return colorRef(systemColor(index));
}

HBRUSH GetSysColorBrush(int index)
{
    static std::map<int, GdiObject*> brushes;
    GdiObject*& o = brushes[index];
    if (!o) {
        o = new GdiObject;
        o->type = GdiObject::Type::Brush;
        o->stock = true;
    }
    o->color = GetSysColor(index);
    return o;
}

COLORREF SetTextColor(HDC handle, COLORREF color)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return CLR_INVALID;
    const COLORREF previous = dc->textColor;
    dc->textColor = color;
    return previous;
}

COLORREF GetTextColor(HDC handle)
{
    DeviceContext* dc = deviceContext(handle);
    return dc ? dc->textColor : CLR_INVALID;
}

COLORREF SetBkColor(HDC handle, COLORREF color)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return CLR_INVALID;
    const COLORREF previous = dc->backgroundColor;
    dc->backgroundColor = color;
    return previous;
}

COLORREF GetBkColor(HDC handle)
{
    DeviceContext* dc = deviceContext(handle);
    return dc ? dc->backgroundColor : CLR_INVALID;
}

int SetBkMode(HDC handle, int mode)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return 0;
    const int previous = dc->backgroundMode;
    dc->backgroundMode = mode;
    return previous;
}

int GetDeviceCaps(HDC, int index)
{
    QScreen* primary = QGuiApplication::primaryScreen();
    switch (index) {
    case LOGPIXELSX:
    case LOGPIXELSY: return USER_DEFAULT_SCREEN_DPI;
    case HORZRES: return primary ? primary->geometry().width() : 1920;
    case VERTRES: return primary ? primary->geometry().height() : 1080;
    case BITSPIXEL: return primary ? primary->depth() : 32;
    default: return 0;
    }
}

static QFont dcFont(HDC handle)
{
    DeviceContext* dc = deviceContext(handle);
    if (dc && dc->font)
        return fontOf(dc->font);
    return dc && dc->widget ? dc->widget->font() : QApplication::font();
}

static BOOL textExtent(HDC dc, const QString& text, LPSIZE size)
{
    if (!size)
        return FALSE;
    const QFontMetrics metrics(dcFont(dc));
    size->cx = metrics.horizontalAdvance(text);
    size->cy = metrics.height();
    return TRUE;
}

BOOL GetTextExtentPoint32A(HDC dc, LPCSTR text, int length, LPSIZE size)
{
    return textExtent(dc, text ? QString::fromUtf8(text, length) : QString(), size);
}

BOOL GetTextExtentPoint32W(HDC dc, LPCWSTR text, int length, LPSIZE size)
{
    return textExtent(dc, text ? QString::fromWCharArray(text, length) : QString(), size);
}

extern "C++" {
template<typename Metrics>
static BOOL textMetrics(HDC dc, Metrics* out)
{
    if (!out)
        return FALSE;
    const QFont font = dcFont(dc);
    const QFontMetrics metrics(font);
    std::memset(out, 0, sizeof(*out));
    out->tmHeight = metrics.height();
    out->tmAscent = metrics.ascent();
    out->tmDescent = metrics.descent();
    out->tmInternalLeading = std::max(0, metrics.height() - (font.pixelSize() > 0 ? font.pixelSize() : metrics.ascent()));
    out->tmExternalLeading = metrics.leading();
    out->tmAveCharWidth = metrics.averageCharWidth();
    out->tmMaxCharWidth = metrics.maxWidth();
    out->tmWeight = font.bold() ? FW_BOLD : FW_NORMAL;
    out->tmDigitizedAspectX = out->tmDigitizedAspectY = USER_DEFAULT_SCREEN_DPI;
    out->tmFirstChar = 32;
    out->tmLastChar = 255;
    out->tmDefaultChar = '?';
    out->tmBreakChar = ' ';
    out->tmItalic = font.italic() ? 1 : 0;
    out->tmUnderlined = font.underline() ? 1 : 0;
    out->tmStruckOut = font.strikeOut() ? 1 : 0;
    out->tmPitchAndFamily = font.fixedPitch() ? 0 : 1; // en Windows, el bit 0 es «de ancho variable»
    out->tmCharSet = DEFAULT_CHARSET;
    return TRUE;
}
}

BOOL GetTextMetricsA(HDC dc, LPTEXTMETRICA metrics) { return textMetrics(dc, metrics); }
BOOL GetTextMetricsW(HDC dc, LPTEXTMETRICW metrics) { return textMetrics(dc, metrics); }

int MulDiv(int number, int numerator, int denominator)
{
    if (denominator == 0)
        return -1;
    const long long product = static_cast<long long>(number) * numerator;
    // Redondea al más cercano y, en el medio, hacia fuera del cero, como Windows
    const bool negative = (product < 0) != (denominator < 0);
    const long long magnitude = product < 0 ? -product : product;
    const long long divisor = denominator < 0 ? -static_cast<long long>(denominator) : denominator;
    const long long rounded = (magnitude + divisor / 2) / divisor;
    const long long result = negative ? -rounded : rounded;
    return result > INT_MAX || result < INT_MIN ? -1 : static_cast<int>(result);
}

} // extern "C"
