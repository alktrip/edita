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
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QThread>
#include <QElapsedTimer>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QImage>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QScrollBar>
#include <QSlider>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTimer>
#include <QTreeWidget>
#include <QWidget>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstring>
#include <deque>
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

enum class Kind { Generic, Dialog, Button, Edit, Static, ListBox, ComboBox, Custom, Tab, ListView, TreeView, Progress, Trackbar, Tooltip };

struct WindowClass
{
    QString name;
    WNDPROC proc = nullptr;
    int extraBytes = 0;
    UINT style = 0;
    ATOM atom = 0;
    HBRUSH background = nullptr; // hbrBackground: WM_ERASEBKGND de DefWindowProc
};

struct DeviceContext;

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
    bool ended = false;         // EndDialog (también antes de que empiece el bucle modal)
    QSize baseUnits{6, 13};     // unidades de diálogo de su fuente (MapDialogRect)
    bool keyReached = false;    // la cadena llegó al procedimiento de la clase
    struct Subclass
    {
        SUBCLASSPROC proc;
        UINT_PTR id;
        DWORD_PTR refData;
    };
    std::vector<Subclass> subclasses; // SetWindowSubclass (el último, el primero en recibir)
    WNDPROC subclassBase = nullptr;   // el procedimiento que había antes del primero
    // Controles comunes (fase 58)
    bool quiet = false;               // cambios del propio control: sin notificaciones
    int tabCurrent = -1;              // la pestaña elegida que ve el plugin (TCN_SELCHANGING: la anterior)
    int listColumns = 0;              // columnas insertadas (LVM_INSERTCOLUMN)
    std::vector<int> columnFormats;   // LVCFMT_* de cada columna
    DWORD listExStyle = 0;            // LVS_EX_*
    int selectionMark = -1;
    std::set<void*> treeItems;        // los HTREEITEM vivos (QTreeWidgetItem*)
    void* caretOverride = nullptr;    // TVN_SELCHANGING: la selección anterior
    bool caretOverridden = false;
    UINT treeAction = 0;              // TVC_BYMOUSE, TVC_BYKEYBOARD o TVC_UNKNOWN
    int progressStep = 10;
    std::vector<std::pair<HWND, QString>> tools; // tooltips: la ventana y su texto
    // Dibujo: el DC de WM_PAINT mientras dura (BeginPaint lo devuelve)
    DeviceContext* paintDc = nullptr;
    QRect paintRect;
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
    const EditaResource* resources = nullptr;  // los del .rc (edita_rc)
    unsigned long resourceCount = 0;
    std::map<unsigned, std::wstring> wideStrings; // LoadStringW con tamaño 0
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
    enum class Type { Font, Brush, Pen, Bitmap } type = Type::Font;
    bool stock = false;
    int stockIndex = -1;          // GetStockObject (NULL_BRUSH, DC_PEN…)
    QFont font;
    COLORREF color = 0;
    int penStyle = PS_SOLID;
    int penWidth = 0;
    bool hollow = false;          // pincel o pluma que no pinta (NULL_BRUSH, BS_NULL, PS_NULL)
    QImage image;                 // mapas de bits (CreateCompatibleBitmap)
    LOGFONTW logFont{};
};

// Un HDC: el de una ventana (con su QPainter durante WM_PAINT; fuera, sin dibujo) o uno en
// memoria (CreateCompatibleDC), que dibuja en el mapa de bits que tenga seleccionado
struct DeviceContext
{
    uint32_t magic = 0x48444330; // "HDC0"
    QPointer<QWidget> widget;
    HGDIOBJ font = nullptr;
    COLORREF textColor = 0;
    COLORREF backgroundColor = 0xFFFFFF;
    int backgroundMode = OPAQUE;
    QPainter* painter = nullptr;
    bool ownsPainter = false;     // el de un DC en memoria (sobre su mapa de bits)
    bool memory = false;
    HGDIOBJ pen = nullptr;        // nullptr: BLACK_PEN
    HGDIOBJ brush = nullptr;      // nullptr: WHITE_BRUSH
    HGDIOBJ bitmap = nullptr;
    QPoint position;              // MoveToEx / LineTo
    UINT textAlign = 0;           // TA_*
    COLORREF dcBrushColor = 0xFFFFFF;
    COLORREF dcPenColor = 0;
    int saved = 0;                // SaveDC
};

GdiObject* gdiObject(HGDIOBJ object)
{
    auto* o = static_cast<GdiObject*>(object);
    return o && o->magic == kGdiMagic ? o : nullptr;
}

// Los DC vivos: uno liberado (o el de un WM_PAINT que ya terminó) no se usa
std::set<DeviceContext*>& liveDcs()
{
    static auto* dcs = new std::set<DeviceContext*>;
    return *dcs;
}

DeviceContext* deviceContext(HDC dc)
{
    auto* d = static_cast<DeviceContext*>(dc);
    return d && liveDcs().count(d) && d->magic == 0x48444330 ? d : nullptr;
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
    void paintEvent(QPaintEvent* event) override; // WM_ERASEBKGND y WM_PAINT
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
// --- Controles comunes (fase 58) ------------------------------------------------------------
//
// Pestañas sobre QTabWidget (con páginas vacías: las páginas de verdad son las ventanas del
// plugin, que se colocan con TCM_ADJUSTRECT), listas con columnas y árboles sobre QTreeWidget,
// barras de progreso sobre QProgressBar, deslizadores sobre QSlider y tooltips sobre el
// toolTip de cada widget. Las notificaciones llegan al padre como WM_NOTIFY, con las mismas
// estructuras que en Windows (las A o las W, según UNICODE).

COLORREF colorRef(const QColor& c);
QColor systemColor(int index);

constexpr int kParamRole = Qt::UserRole + 1; // el lParam de cada elemento
constexpr int kCheckRole = Qt::UserRole + 2; // la casilla antes del cambio (LVN_ITEMCHANGED)

QString itemText(const char* text) { return text == reinterpret_cast<const char*>(-1) ? QString() : fromA(text); }
QString itemText(const wchar_t* text) { return text == reinterpret_cast<const wchar_t*>(-1) ? QString() : fromW(text); }
int copyItemText(const QString& text, char* buffer, int size) { return copyTo(text.toStdString(), buffer, size); }
int copyItemText(const QString& text, wchar_t* buffer, int size) { return copyTo(text.toStdWString(), buffer, size); }

// WM_NOTIFY al padre, con el NMHDR (primer miembro de `data`) relleno
template<typename T>
LRESULT notifyNM(Window* w, T& data, UINT code)
{
    auto* header = reinterpret_cast<NMHDR*>(&data);
    header->hwndFrom = hwndOf(w);
    header->idFrom = static_cast<UINT_PTR>(w->id);
    header->code = code;
    Window* parent = windowOf(w->parent);
    if (!parent || w->quiet)
        return 0;
    return send(parent, WM_NOTIFY, static_cast<WPARAM>(w->id), reinterpret_cast<LPARAM>(&data));
}

LRESULT notifyCode(Window* w, UINT code)
{
    NMHDR header{};
    return notifyNM(w, header, code);
}

class QuietScope
{
public:
    explicit QuietScope(Window* w) : _w(w), _was(w->quiet) { w->quiet = true; }
    ~QuietScope() { _w->quiet = _was; }
    QuietScope(const QuietScope&) = delete;
    QuietScope& operator=(const QuietScope&) = delete;

private:
    Window* _w;
    bool _was;
};

QPoint clientPoint(QAbstractScrollArea* view, const QPoint& viewportPoint)
{
    return view->viewport()->mapTo(view, viewportPoint);
}

// El ratón y el teclado de las listas y los árboles: NM_CLICK, NM_DBLCLK, NM_RCLICK,
// NM_RETURN, LVN_KEYDOWN/TVN_KEYDOWN y LVN_ITEMACTIVATE
class ComctlFilter : public QObject
{
public:
    explicit ComctlFilter(Window* w, QObject* parent) : QObject(parent), _w(w) {}
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    Window* _w;
};

// --- Pestañas -------------------------------------------------------------------------------

QTabWidget* tabWidget(Window* w) { return qobject_cast<QTabWidget*>(w->widget); }

template<typename Item>
void applyTabItem(QTabWidget* tabs, int index, const Item* item)
{
    if (item->mask & TCIF_TEXT)
        tabs->setTabText(index, itemText(item->pszText).replace(QLatin1String("&&"), QLatin1String("\x01")).remove(QLatin1Char('&')).replace(QLatin1Char('\x01'), QLatin1Char('&')));
    if (item->mask & TCIF_PARAM)
        tabs->tabBar()->setTabData(index, QVariant::fromValue(static_cast<qlonglong>(item->lParam)));
}

template<typename Item>
BOOL tabGetItem(QTabWidget* tabs, int index, Item* item)
{
    if (!item || index < 0 || index >= tabs->count())
        return FALSE;
    if (item->mask & TCIF_TEXT)
        copyItemText(tabs->tabText(index), item->pszText, item->cchTextMax);
    if (item->mask & TCIF_PARAM)
        item->lParam = static_cast<LPARAM>(tabs->tabBar()->tabData(index).toLongLong());
    if (item->mask & TCIF_STATE)
        item->dwState = 0;
    return TRUE;
}

// Lo que ocupan las pestañas arriba (o abajo, TCS_BOTTOM) y el marco de la página
QMargins tabMargins(Window* w, QTabWidget* tabs)
{
    const int frame = tabs->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, tabs);
    const int bar = std::max(tabs->tabBar()->sizeHint().height(), QFontMetrics(tabs->font()).height() + 8);
    return (w->style & TCS_BOTTOM) ? QMargins(frame, frame, frame, bar + frame) : QMargins(frame, bar + frame, frame, frame);
}

LRESULT tabMessage(Window* w, UINT message, WPARAM wParam, LPARAM lParam)
{
    QTabWidget* tabs = tabWidget(w);
    if (!tabs)
        return 0;
    const int index = static_cast<int>(wParam);
    switch (message) {
    case TCM_GETITEMCOUNT:
        return tabs->count();
    case TCM_INSERTITEMA:
    case TCM_INSERTITEMW: {
        if (!lParam)
            return -1;
        QuietScope quiet(w);
        const int at = tabs->insertTab(std::clamp(index, 0, tabs->count()), new QWidget, QString());
        if (message == TCM_INSERTITEMW)
            applyTabItem(tabs, at, reinterpret_cast<const TCITEMW*>(lParam));
        else
            applyTabItem(tabs, at, reinterpret_cast<const TCITEMA*>(lParam));
        if (w->tabCurrent < 0 || tabs->count() == 1)
            w->tabCurrent = tabs->currentIndex();
        else if (at <= w->tabCurrent)
            ++w->tabCurrent;
        return at;
    }
    case TCM_SETITEMA:
    case TCM_SETITEMW:
        if (!lParam || index < 0 || index >= tabs->count())
            return FALSE;
        if (message == TCM_SETITEMW)
            applyTabItem(tabs, index, reinterpret_cast<const TCITEMW*>(lParam));
        else
            applyTabItem(tabs, index, reinterpret_cast<const TCITEMA*>(lParam));
        return TRUE;
    case TCM_GETITEMA:
        return tabGetItem(tabs, index, reinterpret_cast<TCITEMA*>(lParam));
    case TCM_GETITEMW:
        return tabGetItem(tabs, index, reinterpret_cast<TCITEMW*>(lParam));
    case TCM_DELETEITEM: {
        if (index < 0 || index >= tabs->count())
            return FALSE;
        QuietScope quiet(w);
        QWidget* page = tabs->widget(index);
        tabs->removeTab(index);
        delete page;
        if (index == w->tabCurrent)
            w->tabCurrent = -1;
        else if (index < w->tabCurrent)
            --w->tabCurrent;
        return TRUE;
    }
    case TCM_DELETEALLITEMS: {
        QuietScope quiet(w);
        while (tabs->count()) {
            QWidget* page = tabs->widget(0);
            tabs->removeTab(0);
            delete page;
        }
        w->tabCurrent = -1;
        return TRUE;
    }
    case TCM_GETCURSEL:
    case TCM_GETCURFOCUS:
        return w->tabCurrent;
    case TCM_SETCURSEL: {
        // Sin notificaciones, como en Windows
        const int previous = w->tabCurrent;
        if (index < 0 || index >= tabs->count())
            return -1;
        QuietScope quiet(w);
        tabs->setCurrentIndex(index);
        w->tabCurrent = index;
        return previous;
    }
    case TCM_SETCURFOCUS:
        // Como si la eligiera el usuario: con TCN_SELCHANGING y TCN_SELCHANGE
        if (index >= 0 && index < tabs->count())
            tabs->setCurrentIndex(index);
        return 0;
    case TCM_GETITEMRECT: {
        auto* rect = reinterpret_cast<RECT*>(lParam);
        if (!rect || index < 0 || index >= tabs->count())
            return FALSE;
        const QRect r = tabs->tabBar()->tabRect(index).translated(tabs->tabBar()->mapTo(tabs, QPoint(0, 0)));
        *rect = RECT{r.left(), r.top(), r.right() + 1, r.bottom() + 1};
        return TRUE;
    }
    case TCM_ADJUSTRECT: {
        // FALSE: del rectángulo de la ventana al de la página; TRUE: al revés
        auto* rect = reinterpret_cast<RECT*>(lParam);
        if (!rect)
            return 0;
        const QMargins m = tabMargins(w, tabs);
        const int sign = wParam ? -1 : 1;
        rect->left += sign * m.left();
        rect->top += sign * m.top();
        rect->right -= sign * m.right();
        rect->bottom -= sign * m.bottom();
        return 0;
    }
    default:
        return 0;
    }
}

QWidget* createTabControl(Window* w, QWidget* parent)
{
    w->kind = Kind::Tab;
    auto* tabs = new QTabWidget(parent);
    tabs->setUsesScrollButtons(true);
    if (w->style & TCS_BOTTOM)
        tabs->setTabPosition(QTabWidget::South);
    if (w->style & TCS_FOCUSNEVER)
        tabs->tabBar()->setFocusPolicy(Qt::NoFocus);
    // El usuario elige: TCN_SELCHANGING (con la anterior aún elegida) y TCN_SELCHANGE
    QObject::connect(tabs, &QTabWidget::currentChanged, tabs, [w, tabs](int index) {
        if (w->quiet || index == w->tabCurrent)
            return;
        const int previous = w->tabCurrent;
        if (notifyCode(w, TCN_SELCHANGING)) {
            QuietScope quiet(w);
            tabs->setCurrentIndex(previous);
            return;
        }
        w->tabCurrent = index;
        notifyCode(w, TCN_SELCHANGE);
    });
    return tabs;
}

// --- Listas con columnas ---------------------------------------------------------------------

QTreeWidget* listView(Window* w) { return w->kind == Kind::ListView ? qobject_cast<QTreeWidget*>(w->widget) : nullptr; }

QTreeWidgetItem* listItem(Window* w, int index)
{
    QTreeWidget* list = listView(w);
    return list && index >= 0 && index < list->topLevelItemCount() ? list->topLevelItem(index) : nullptr;
}

Qt::Alignment columnAlignment(int format)
{
    switch (format & 3) {
    case LVCFMT_RIGHT: return Qt::AlignRight | Qt::AlignVCenter;
    case LVCFMT_CENTER: return Qt::AlignHCenter | Qt::AlignVCenter;
    default: return Qt::AlignLeft | Qt::AlignVCenter;
    }
}

bool listCheckable(Window* w) { return w->listExStyle & LVS_EX_CHECKBOXES; }

UINT listItemState(Window* w, QTreeWidgetItem* item, UINT mask)
{
    QTreeWidget* list = listView(w);
    UINT state = 0;
    if ((mask & LVIS_SELECTED) && item->isSelected())
        state |= LVIS_SELECTED;
    if ((mask & LVIS_FOCUSED) && list->currentItem() == item)
        state |= LVIS_FOCUSED;
    if ((mask & LVIS_STATEIMAGEMASK) && listCheckable(w))
        state |= INDEXTOSTATEIMAGEMASK(item->checkState(0) == Qt::Checked ? 2u : 1u);
    return state;
}

void setListItemState(Window* w, QTreeWidgetItem* item, UINT state, UINT mask)
{
    QTreeWidget* list = listView(w);
    if (mask & LVIS_SELECTED)
        item->setSelected(state & LVIS_SELECTED);
    if ((mask & LVIS_FOCUSED) && (state & LVIS_FOCUSED))
        list->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
    if ((mask & LVIS_STATEIMAGEMASK) && listCheckable(w)) {
        const UINT image = (state & LVIS_STATEIMAGEMASK) >> 12;
        if (image == 1 || image == 2)
            item->setCheckState(0, image == 2 ? Qt::Checked : Qt::Unchecked);
    }
}

NMLISTVIEW listNotification(Window* w, int index, int subItem = 0)
{
    NMLISTVIEW nm{};
    nm.iItem = index;
    nm.iSubItem = subItem;
    if (QTreeWidgetItem* item = listItem(w, index))
        nm.lParam = static_cast<LPARAM>(item->data(0, kParamRole).toLongLong());
    return nm;
}

void notifyItemChanged(Window* w, int index, UINT oldState, UINT newState)
{
    if (w->quiet || oldState == newState)
        return;
    NMLISTVIEW nm = listNotification(w, index);
    nm.uChanged = LVIF_STATE;
    nm.uOldState = oldState;
    nm.uNewState = newState;
    notifyNM(w, nm, LVN_ITEMCHANGED);
}

void setListText(Window* w, QTreeWidgetItem* item, int column, const QString& text)
{
    if (column < 0 || column >= std::max(1, w->listColumns))
        return;
    item->setText(column, text);
    if (column < static_cast<int>(w->columnFormats.size()))
        item->setTextAlignment(column, columnAlignment(w->columnFormats[static_cast<std::size_t>(column)]));
}

template<typename Item>
void applyListItem(Window* w, QTreeWidgetItem* item, const Item* lv)
{
    if (lv->mask & LVIF_TEXT)
        setListText(w, item, lv->iSubItem, itemText(lv->pszText));
    if (lv->iSubItem != 0)
        return;
    if (lv->mask & LVIF_PARAM)
        item->setData(0, kParamRole, QVariant::fromValue(static_cast<qlonglong>(lv->lParam)));
    if (lv->mask & LVIF_STATE)
        setListItemState(w, item, lv->state, lv->stateMask);
}

template<typename Item>
LRESULT listInsertItem(Window* w, const Item* lv)
{
    QTreeWidget* list = listView(w);
    if (!lv)
        return -1;
    auto* item = new QTreeWidgetItem;
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren
                   | (listCheckable(w) ? Qt::ItemIsUserCheckable : Qt::ItemFlags()));
    int index = std::clamp(lv->iItem, 0, list->topLevelItemCount());
    // LVS_SORTASCENDING / LVS_SORTDESCENDING: por el texto, sin distinguir mayúsculas
    const QString text = (lv->mask & LVIF_TEXT) ? itemText(lv->pszText) : QString();
    if (w->style & (LVS_SORTASCENDING | LVS_SORTDESCENDING)) {
        const bool descending = w->style & LVS_SORTDESCENDING;
        index = 0;
        while (index < list->topLevelItemCount()) {
            const int order = list->topLevelItem(index)->text(0).compare(text, Qt::CaseInsensitive);
            if (descending ? order < 0 : order > 0)
                break;
            ++index;
        }
    }
    {
        QuietScope quiet(w);
        list->insertTopLevelItem(index, item);
        if (listCheckable(w)) {
            item->setCheckState(0, Qt::Unchecked);
            item->setData(0, kCheckRole, false);
        }
        Item first = *lv;
        first.iSubItem = 0;
        applyListItem(w, item, &first);
        if (lv->mask & LVIF_STATE)
            item->setData(0, kCheckRole, item->checkState(0) == Qt::Checked);
    }
    if (w->selectionMark >= index)
        ++w->selectionMark;
    NMLISTVIEW nm = listNotification(w, index);
    notifyNM(w, nm, LVN_INSERTITEM);
    return index;
}

template<typename Item>
BOOL listGetItem(Window* w, Item* lv)
{
    QTreeWidgetItem* item = lv ? listItem(w, lv->iItem) : nullptr;
    if (!item)
        return FALSE;
    if (lv->mask & LVIF_TEXT)
        copyItemText(item->text(lv->iSubItem), lv->pszText, lv->cchTextMax);
    if (lv->mask & LVIF_PARAM)
        lv->lParam = static_cast<LPARAM>(item->data(0, kParamRole).toLongLong());
    if (lv->mask & LVIF_STATE)
        lv->state = listItemState(w, item, lv->stateMask);
    if (lv->mask & LVIF_IMAGE)
        lv->iImage = -1;
    return TRUE;
}

template<typename Column>
LRESULT listInsertColumn(Window* w, int at, const Column* column)
{
    QTreeWidget* list = listView(w);
    if (!column)
        return -1;
    const int index = std::clamp(at, 0, w->listColumns);
    const QString title = (column->mask & LVCF_TEXT) ? itemText(column->pszText) : QString();
    const int format = (column->mask & LVCF_FMT) ? column->fmt : LVCFMT_LEFT;
    if (w->listColumns > 0) {
        // Las de la derecha se desplazan una: también sus textos en cada elemento
        list->setColumnCount(w->listColumns + 1);
        QTreeWidgetItem* header = list->headerItem();
        for (int c = w->listColumns; c > index; --c) {
            header->setText(c, header->text(c - 1));
            header->setTextAlignment(c, Qt::Alignment(header->textAlignment(c - 1)));
            list->setColumnWidth(c, list->columnWidth(c - 1));
            for (int i = 0; i < list->topLevelItemCount(); ++i) {
                QTreeWidgetItem* item = list->topLevelItem(i);
                item->setText(c, item->text(c - 1));
                item->setTextAlignment(c, Qt::Alignment(item->textAlignment(c - 1)));
            }
        }
        for (int i = 0; i < list->topLevelItemCount(); ++i)
            list->topLevelItem(i)->setText(index, QString());
    }
    w->columnFormats.insert(w->columnFormats.begin() + index, format);
    ++w->listColumns;
    list->headerItem()->setText(index, title);
    list->headerItem()->setTextAlignment(index, columnAlignment(format));
    if (column->mask & LVCF_WIDTH)
        list->setColumnWidth(index, column->cx);
    return index;
}

template<typename Column>
BOOL listGetColumn(Window* w, int index, Column* column)
{
    QTreeWidget* list = listView(w);
    if (!column || index < 0 || index >= w->listColumns)
        return FALSE;
    if (column->mask & LVCF_TEXT)
        copyItemText(list->headerItem()->text(index), column->pszText, column->cchTextMax);
    if (column->mask & LVCF_WIDTH)
        column->cx = list->columnWidth(index);
    if (column->mask & LVCF_FMT)
        column->fmt = w->columnFormats[static_cast<std::size_t>(index)];
    if (column->mask & LVCF_SUBITEM)
        column->iSubItem = index;
    return TRUE;
}

template<typename Column>
BOOL listSetColumn(Window* w, int index, const Column* column)
{
    QTreeWidget* list = listView(w);
    if (!column || index < 0 || index >= w->listColumns)
        return FALSE;
    if (column->mask & LVCF_TEXT)
        list->headerItem()->setText(index, itemText(column->pszText));
    if (column->mask & LVCF_WIDTH)
        list->setColumnWidth(index, column->cx);
    if (column->mask & LVCF_FMT) {
        w->columnFormats[static_cast<std::size_t>(index)] = column->fmt;
        list->headerItem()->setTextAlignment(index, columnAlignment(column->fmt));
    }
    return TRUE;
}

template<typename Find>
LRESULT listFindItem(Window* w, int start, const Find* find)
{
    QTreeWidget* list = listView(w);
    if (!find)
        return -1;
    const QString text = itemText(find->psz);
    const int count = list->topLevelItemCount();
    for (int i = std::max(0, start + 1); i < count; ++i) {
        QTreeWidgetItem* item = list->topLevelItem(i);
        if ((find->flags & LVFI_PARAM) && item->data(0, kParamRole).toLongLong() == static_cast<qlonglong>(find->lParam))
            return i;
        if (find->flags & (LVFI_STRING | LVFI_PARTIAL)) {
            const bool match = (find->flags & LVFI_PARTIAL) ? item->text(0).startsWith(text, Qt::CaseInsensitive)
                                                             : item->text(0).compare(text, Qt::CaseInsensitive) == 0;
            if (match)
                return i;
        }
    }
    return -1;
}

void listDeleteItem(Window* w, int index)
{
    QTreeWidget* list = listView(w);
    QTreeWidgetItem* item = listItem(w, index);
    if (!item)
        return;
    // LVN_DELETEITEM antes: el plugin suele liberar ahí lo que apunta su lParam
    NMLISTVIEW nm = listNotification(w, index);
    notifyNM(w, nm, LVN_DELETEITEM);
    QuietScope quiet(w);
    delete list->takeTopLevelItem(index);
    if (w->selectionMark == index)
        w->selectionMark = -1;
    else if (w->selectionMark > index)
        --w->selectionMark;
}

LRESULT listViewMessage(Window* w, UINT message, WPARAM wParam, LPARAM lParam)
{
    QTreeWidget* list = listView(w);
    if (!list)
        return 0;
    const int index = static_cast<int>(static_cast<intptr_t>(wParam));
    switch (message) {
    case LVM_GETITEMCOUNT:
        return list->topLevelItemCount();
    case LVM_INSERTITEMA:
        return listInsertItem(w, reinterpret_cast<const LVITEMA*>(lParam));
    case LVM_INSERTITEMW:
        return listInsertItem(w, reinterpret_cast<const LVITEMW*>(lParam));
    case LVM_SETITEMA:
    case LVM_SETITEMW: {
        if (!lParam)
            return FALSE;
        const int item = message == LVM_SETITEMW ? reinterpret_cast<const LVITEMW*>(lParam)->iItem
                                                 : reinterpret_cast<const LVITEMA*>(lParam)->iItem;
        QTreeWidgetItem* target = listItem(w, item);
        if (!target)
            return FALSE;
        if (message == LVM_SETITEMW)
            applyListItem(w, target, reinterpret_cast<const LVITEMW*>(lParam));
        else
            applyListItem(w, target, reinterpret_cast<const LVITEMA*>(lParam));
        return TRUE;
    }
    case LVM_GETITEMA:
        return listGetItem(w, reinterpret_cast<LVITEMA*>(lParam));
    case LVM_GETITEMW:
        return listGetItem(w, reinterpret_cast<LVITEMW*>(lParam));
    case LVM_SETITEMTEXTA:
    case LVM_SETITEMTEXTW: {
        QTreeWidgetItem* item = listItem(w, index);
        if (!item || !lParam)
            return FALSE;
        if (message == LVM_SETITEMTEXTW) {
            const auto* lv = reinterpret_cast<const LVITEMW*>(lParam);
            setListText(w, item, lv->iSubItem, itemText(lv->pszText));
        } else {
            const auto* lv = reinterpret_cast<const LVITEMA*>(lParam);
            setListText(w, item, lv->iSubItem, itemText(lv->pszText));
        }
        return TRUE;
    }
    case LVM_GETITEMTEXTA:
    case LVM_GETITEMTEXTW: {
        QTreeWidgetItem* item = listItem(w, index);
        if (!lParam)
            return 0;
        if (message == LVM_GETITEMTEXTW) {
            auto* lv = reinterpret_cast<LVITEMW*>(lParam);
            return copyItemText(item ? item->text(lv->iSubItem) : QString(), lv->pszText, lv->cchTextMax);
        }
        auto* lv = reinterpret_cast<LVITEMA*>(lParam);
        return copyItemText(item ? item->text(lv->iSubItem) : QString(), lv->pszText, lv->cchTextMax);
    }
    case LVM_SETITEMSTATE: {
        // wParam -1: todos los elementos
        const auto* lv = reinterpret_cast<const LVITEMW*>(lParam);
        if (!lv)
            return FALSE;
        if (index == -1) {
            for (int i = 0; i < list->topLevelItemCount(); ++i)
                setListItemState(w, list->topLevelItem(i), lv->state, lv->stateMask);
            return TRUE;
        }
        QTreeWidgetItem* item = listItem(w, index);
        if (!item)
            return FALSE;
        setListItemState(w, item, lv->state, lv->stateMask);
        return TRUE;
    }
    case LVM_GETITEMSTATE: {
        QTreeWidgetItem* item = listItem(w, index);
        return item ? listItemState(w, item, static_cast<UINT>(lParam)) : 0;
    }
    case LVM_GETNEXTITEM: {
        const UINT flags = LOWORD(lParam);
        for (int i = index + 1; i < list->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = list->topLevelItem(i);
            if ((flags & LVNI_SELECTED) && !item->isSelected())
                continue;
            if ((flags & LVNI_FOCUSED) && list->currentItem() != item)
                continue;
            return i;
        }
        return -1;
    }
    case LVM_GETSELECTEDCOUNT: {
        int count = 0;
        for (int i = 0; i < list->topLevelItemCount(); ++i)
            count += list->topLevelItem(i)->isSelected() ? 1 : 0;
        return count;
    }
    case LVM_DELETEITEM:
        if (!listItem(w, index))
            return FALSE;
        listDeleteItem(w, index);
        return TRUE;
    case LVM_DELETEALLITEMS: {
        // LVN_DELETEALLITEMS; si el plugin no devuelve TRUE, LVN_DELETEITEM de cada uno
        NMLISTVIEW nm = listNotification(w, -1);
        const bool skipEach = notifyNM(w, nm, LVN_DELETEALLITEMS);
        while (list->topLevelItemCount()) {
            if (skipEach) {
                QuietScope quiet(w);
                delete list->takeTopLevelItem(list->topLevelItemCount() - 1);
            } else {
                listDeleteItem(w, list->topLevelItemCount() - 1);
            }
        }
        w->selectionMark = -1;
        return TRUE;
    }
    case LVM_INSERTCOLUMNA:
        return listInsertColumn(w, index, reinterpret_cast<const LVCOLUMNA*>(lParam));
    case LVM_INSERTCOLUMNW:
        return listInsertColumn(w, index, reinterpret_cast<const LVCOLUMNW*>(lParam));
    case LVM_GETCOLUMNA:
        return listGetColumn(w, index, reinterpret_cast<LVCOLUMNA*>(lParam));
    case LVM_GETCOLUMNW:
        return listGetColumn(w, index, reinterpret_cast<LVCOLUMNW*>(lParam));
    case LVM_SETCOLUMNA:
        return listSetColumn(w, index, reinterpret_cast<const LVCOLUMNA*>(lParam));
    case LVM_SETCOLUMNW:
        return listSetColumn(w, index, reinterpret_cast<const LVCOLUMNW*>(lParam));
    case LVM_DELETECOLUMN: {
        if (index < 0 || index >= w->listColumns)
            return FALSE;
        QTreeWidgetItem* header = list->headerItem();
        for (int c = index; c + 1 < w->listColumns; ++c) {
            header->setText(c, header->text(c + 1));
            list->setColumnWidth(c, list->columnWidth(c + 1));
            for (int i = 0; i < list->topLevelItemCount(); ++i)
                list->topLevelItem(i)->setText(c, list->topLevelItem(i)->text(c + 1));
        }
        w->columnFormats.erase(w->columnFormats.begin() + index);
        --w->listColumns;
        list->setColumnCount(std::max(1, w->listColumns));
        return TRUE;
    }
    case LVM_GETCOLUMNWIDTH:
        return index >= 0 && index < std::max(1, w->listColumns) ? list->columnWidth(index) : 0;
    case LVM_SETCOLUMNWIDTH: {
        const int width = static_cast<short>(LOWORD(lParam));
        if (index < 0 || index >= std::max(1, w->listColumns))
            return FALSE;
        if (width == LVSCW_AUTOSIZE || width == LVSCW_AUTOSIZE_USEHEADER) {
            list->resizeColumnToContents(index);
            if (width == LVSCW_AUTOSIZE_USEHEADER) {
                // La última columna llega hasta el borde, como en Windows
                const int header = QFontMetrics(list->header()->font()).horizontalAdvance(list->headerItem()->text(index)) + 20;
                int widthNeeded = std::max(list->columnWidth(index), header);
                if (index == std::max(1, w->listColumns) - 1) {
                    int used = 0;
                    for (int c = 0; c < index; ++c)
                        used += list->columnWidth(c);
                    widthNeeded = std::max(widthNeeded, list->viewport()->width() - used);
                }
                list->setColumnWidth(index, widthNeeded);
            }
        } else {
            list->setColumnWidth(index, width);
        }
        return TRUE;
    }
    case LVM_GETHEADER:
        return 0;
    case LVM_SETEXTENDEDLISTVIEWSTYLE: {
        const DWORD previous = w->listExStyle;
        const DWORD mask = wParam ? static_cast<DWORD>(wParam) : 0xFFFFFFFF;
        w->listExStyle = (w->listExStyle & ~mask) | (static_cast<DWORD>(lParam) & mask);
        if ((previous ^ w->listExStyle) & LVS_EX_CHECKBOXES) {
            QuietScope quiet(w);
            for (int i = 0; i < list->topLevelItemCount(); ++i) {
                QTreeWidgetItem* item = list->topLevelItem(i);
                if (listCheckable(w)) {
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    item->setCheckState(0, Qt::Unchecked);
                    item->setData(0, kCheckRole, false);
                } else {
                    item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
                    item->setData(0, Qt::CheckStateRole, QVariant());
                }
            }
        }
        list->setMouseTracking(w->listExStyle & LVS_EX_TRACKSELECT);
        return static_cast<LRESULT>(previous);
    }
    case LVM_GETEXTENDEDLISTVIEWSTYLE:
        return static_cast<LRESULT>(w->listExStyle);
    case LVM_FINDITEMA:
        return listFindItem(w, index, reinterpret_cast<const LVFINDINFOA*>(lParam));
    case LVM_FINDITEMW:
        return listFindItem(w, index, reinterpret_cast<const LVFINDINFOW*>(lParam));
    case LVM_GETITEMRECT: {
        auto* rect = reinterpret_cast<RECT*>(lParam);
        QTreeWidgetItem* item = listItem(w, index);
        if (!rect || !item)
            return FALSE;
        QRect r = list->visualItemRect(item);
        if (rect->left == LVIR_LABEL) {
            r.setLeft(list->header()->sectionViewportPosition(0));
            r.setWidth(list->columnWidth(0));
        }
        const QPoint offset = clientPoint(list, QPoint(0, 0));
        r.translate(offset);
        *rect = RECT{r.left(), r.top(), r.right() + 1, r.bottom() + 1};
        return TRUE;
    }
    case LVM_HITTEST: {
        auto* info = reinterpret_cast<LVHITTESTINFO*>(lParam);
        if (!info)
            return -1;
        const QPoint point = list->viewport()->mapFrom(list, QPoint(static_cast<int>(info->pt.x), static_cast<int>(info->pt.y)));
        QTreeWidgetItem* item = list->itemAt(point);
        info->iItem = item ? list->indexOfTopLevelItem(item) : -1;
        info->iSubItem = item ? std::max(0, list->columnAt(point.x())) : 0;
        info->flags = item ? LVHT_ONITEMLABEL : LVHT_NOWHERE;
        return info->iItem;
    }
    case LVM_ENSUREVISIBLE:
        if (QTreeWidgetItem* item = listItem(w, index))
            list->scrollToItem(item);
        return TRUE;
    case LVM_GETTOPINDEX: {
        QTreeWidgetItem* top = list->itemAt(QPoint(1, 1));
        return top ? list->indexOfTopLevelItem(top) : 0;
    }
    case LVM_GETCOUNTPERPAGE: {
        const int row = list->topLevelItemCount() ? std::max(1, list->visualItemRect(list->topLevelItem(0)).height())
                                                  : QFontMetrics(list->font()).height() + 4;
        return std::max(1, list->viewport()->height() / row);
    }
    case LVM_SORTITEMS: {
        auto compare = reinterpret_cast<PFNLVCOMPARE>(lParam);
        if (!compare)
            return FALSE;
        QuietScope quiet(w);
        QList<QTreeWidgetItem*> items;
        while (list->topLevelItemCount())
            items << list->takeTopLevelItem(0);
        std::stable_sort(items.begin(), items.end(), [compare, wParam](QTreeWidgetItem* a, QTreeWidgetItem* b) {
            return compare(static_cast<LPARAM>(a->data(0, kParamRole).toLongLong()),
                           static_cast<LPARAM>(b->data(0, kParamRole).toLongLong()), static_cast<LPARAM>(wParam))
                < 0;
        });
        list->addTopLevelItems(items);
        return TRUE;
    }
    case LVM_GETSELECTIONMARK:
        return w->selectionMark;
    case LVM_SETSELECTIONMARK: {
        const int previous = w->selectionMark;
        w->selectionMark = static_cast<int>(lParam);
        return previous;
    }
    case LVM_SETBKCOLOR:
    case LVM_SETTEXTBKCOLOR: {
        if (static_cast<COLORREF>(lParam) == CLR_NONE)
            return TRUE;
        QPalette palette = list->palette();
        const COLORREF c = static_cast<COLORREF>(lParam);
        palette.setColor(QPalette::Base, QColor(GetRValue(c), GetGValue(c), GetBValue(c)));
        list->setPalette(palette);
        return TRUE;
    }
    case LVM_SETTEXTCOLOR: {
        QPalette palette = list->palette();
        const COLORREF c = static_cast<COLORREF>(lParam);
        palette.setColor(QPalette::Text, QColor(GetRValue(c), GetGValue(c), GetBValue(c)));
        list->setPalette(palette);
        return TRUE;
    }
    case LVM_GETBKCOLOR:
        return static_cast<LRESULT>(colorRef(list->palette().color(QPalette::Base)));
    case LVM_SETITEMCOUNT:   // reserva (las listas virtuales, LVS_OWNERDATA, no están)
    case LVM_REDRAWITEMS:
    case LVM_UPDATE:
        list->viewport()->update();
        return TRUE;
    case LVM_SCROLL:
        list->horizontalScrollBar()->setValue(list->horizontalScrollBar()->value() + static_cast<int>(wParam));
        list->verticalScrollBar()->setValue(list->verticalScrollBar()->value() + static_cast<int>(lParam) / 16);
        return TRUE;
    case LVM_SETIMAGELIST:
        return 0;
    default:
        return 0;
    }
}

QWidget* createListView(Window* w, QWidget* parent)
{
    w->kind = Kind::ListView;
    auto* list = new QTreeWidget(parent);
    list->setRootIsDecorated(false);
    list->setItemsExpandable(false);
    list->setUniformRowHeights(true);
    list->setAllColumnsShowFocus(true);
    list->setColumnCount(1);
    list->setSelectionMode((w->style & LVS_SINGLESEL) ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection);
    list->setSelectionBehavior(QAbstractItemView::SelectRows);
    list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const bool report = (w->style & LVS_TYPEMASK) == LVS_REPORT;
    list->setHeaderHidden(!report || (w->style & LVS_NOCOLUMNHEADER));
    list->header()->setSectionsClickable(!(w->style & LVS_NOSORTHEADER));
    list->header()->setStretchLastSection(false);
    list->header()->setSectionsMovable(false);
    list->headerItem()->setText(0, QString());
    // La selección y el foco: LVN_ITEMCHANGED de cada elemento que cambia (también si lo
    // cambia el plugin, como en Windows)
    QObject::connect(list->selectionModel(), &QItemSelectionModel::selectionChanged, list,
                     [w, list](const QItemSelection& selected, const QItemSelection& deselected) {
                         if (w->quiet)
                             return;
                         auto rows = [](const QItemSelection& selection) {
                             std::set<int> out;
                             for (const QModelIndex& i : selection.indexes())
                                 out.insert(i.row());
                             return out;
                         };
                         for (int row : rows(deselected)) {
                             if (QTreeWidgetItem* item = list->topLevelItem(row)) {
                                 const UINT now = listItemState(w, item, LVIS_SELECTED | LVIS_FOCUSED);
                                 notifyItemChanged(w, row, now | LVIS_SELECTED, now);
                             }
                         }
                         for (int row : rows(selected)) {
                             if (QTreeWidgetItem* item = list->topLevelItem(row)) {
                                 const UINT now = listItemState(w, item, LVIS_SELECTED | LVIS_FOCUSED);
                                 notifyItemChanged(w, row, now & ~static_cast<UINT>(LVIS_SELECTED), now);
                                 w->selectionMark = row;
                             }
                         }
                     });
    QObject::connect(list, &QTreeWidget::currentItemChanged, list, [w, list](QTreeWidgetItem* current, QTreeWidgetItem* previous) {
        if (w->quiet)
            return;
        if (previous) {
            const UINT now = listItemState(w, previous, LVIS_SELECTED);
            notifyItemChanged(w, list->indexOfTopLevelItem(previous), now | LVIS_FOCUSED, now);
        }
        if (current) {
            const UINT now = listItemState(w, current, LVIS_SELECTED);
            notifyItemChanged(w, list->indexOfTopLevelItem(current), now, now | LVIS_FOCUSED);
        }
    });
    // Las casillas (LVS_EX_CHECKBOXES): el cambio de la imagen de estado
    QObject::connect(list, &QTreeWidget::itemChanged, list, [w, list](QTreeWidgetItem* item, int column) {
        if (column != 0 || !listCheckable(w))
            return;
        const bool checked = item->checkState(0) == Qt::Checked;
        const bool before = item->data(0, kCheckRole).toBool();
        if (checked == before)
            return;
        item->setData(0, kCheckRole, checked);
        if (w->quiet)
            return;
        const UINT mask = LVIS_SELECTED | LVIS_FOCUSED;
        const UINT other = listItemState(w, item, mask);
        notifyItemChanged(w, list->indexOfTopLevelItem(item), other | INDEXTOSTATEIMAGEMASK(before ? 2u : 1u),
                          other | INDEXTOSTATEIMAGEMASK(checked ? 2u : 1u));
    });
    QObject::connect(list->header(), &QHeaderView::sectionClicked, list, [w](int column) {
        NMLISTVIEW nm = listNotification(w, -1, column);
        notifyNM(w, nm, LVN_COLUMNCLICK);
    });
    auto* filter = new ComctlFilter(w, list);
    list->viewport()->installEventFilter(filter);
    list->installEventFilter(filter);
    return list;
}

// --- Árboles -----------------------------------------------------------------------------------

QTreeWidget* treeView(Window* w) { return w->kind == Kind::TreeView ? qobject_cast<QTreeWidget*>(w->widget) : nullptr; }

QTreeWidgetItem* treeItem(Window* w, HTREEITEM handle)
{
    return handle && w->treeItems.count(handle) ? static_cast<QTreeWidgetItem*>(handle) : nullptr;
}

HTREEITEM treeHandle(QTreeWidgetItem* item) { return item; }

UINT treeItemState(Window* w, QTreeWidgetItem* item)
{
    QTreeWidget* tree = treeView(w);
    UINT state = 0;
    if (item->isSelected() || tree->currentItem() == item)
        state |= TVIS_SELECTED;
    if (item->isExpanded())
        state |= TVIS_EXPANDED;
    if (item->font(0).bold())
        state |= TVIS_BOLD;
    if (w->style & TVS_CHECKBOXES)
        state |= INDEXTOSTATEIMAGEMASK(item->checkState(0) == Qt::Checked ? 2u : 1u);
    return state;
}

template<typename Item>
void applyTreeItem(Window* w, QTreeWidgetItem* item, const Item* tv)
{
    if (tv->mask & TVIF_TEXT)
        item->setText(0, itemText(tv->pszText));
    if (tv->mask & TVIF_PARAM)
        item->setData(0, kParamRole, QVariant::fromValue(static_cast<qlonglong>(tv->lParam)));
    if (tv->mask & TVIF_CHILDREN)
        item->setChildIndicatorPolicy(tv->cChildren ? QTreeWidgetItem::ShowIndicator
                                                    : QTreeWidgetItem::DontShowIndicatorWhenChildless);
    if (tv->mask & TVIF_STATE) {
        QuietScope quiet(w);
        if (tv->stateMask & TVIS_BOLD) {
            QFont font = item->font(0);
            font.setBold(tv->state & TVIS_BOLD);
            item->setFont(0, font);
        }
        if (tv->stateMask & TVIS_EXPANDED)
            item->setExpanded(tv->state & TVIS_EXPANDED);
        if ((tv->stateMask & TVIS_SELECTED) && (tv->state & TVIS_SELECTED))
            item->setSelected(true);
        if ((tv->stateMask & TVIS_STATEIMAGEMASK) && (w->style & TVS_CHECKBOXES)) {
            const UINT image = (tv->state & TVIS_STATEIMAGEMASK) >> 12;
            if (image == 1 || image == 2)
                item->setCheckState(0, image == 2 ? Qt::Checked : Qt::Unchecked);
        }
    }
}

template<typename Item>
BOOL treeGetItem(Window* w, Item* tv)
{
    QTreeWidgetItem* item = tv ? treeItem(w, tv->hItem) : nullptr;
    if (!item)
        return FALSE;
    if (tv->mask & TVIF_TEXT)
        copyItemText(item->text(0), tv->pszText, tv->cchTextMax);
    if (tv->mask & TVIF_PARAM)
        tv->lParam = static_cast<LPARAM>(item->data(0, kParamRole).toLongLong());
    if (tv->mask & TVIF_STATE)
        tv->state = treeItemState(w, item) & (tv->stateMask ? tv->stateMask : 0xFFFFFFFF);
    if (tv->mask & TVIF_CHILDREN)
        tv->cChildren = item->childCount() > 0 || item->childIndicatorPolicy() == QTreeWidgetItem::ShowIndicator ? 1 : 0;
    if (tv->mask & TVIF_IMAGE)
        tv->iImage = 0;
    if (tv->mask & TVIF_SELECTEDIMAGE)
        tv->iSelectedImage = 0;
    return TRUE;
}

template<typename Insert>
HTREEITEM treeInsertItem(Window* w, const Insert* is)
{
    QTreeWidget* tree = treeView(w);
    if (!is)
        return nullptr;
    QTreeWidgetItem* parent = (is->hParent == TVI_ROOT || !is->hParent) ? tree->invisibleRootItem() : treeItem(w, is->hParent);
    if (!parent)
        return nullptr;
    const QString text = (is->item.mask & TVIF_TEXT) ? itemText(is->item.pszText) : QString();
    int position = parent->childCount();
    if (is->hInsertAfter == TVI_FIRST) {
        position = 0;
    } else if (is->hInsertAfter == TVI_SORT) {
        position = 0;
        while (position < parent->childCount() && parent->child(position)->text(0).compare(text, Qt::CaseInsensitive) <= 0)
            ++position;
    } else if (is->hInsertAfter && is->hInsertAfter != TVI_LAST) {
        if (QTreeWidgetItem* after = treeItem(w, is->hInsertAfter); after && after->parent() == (parent == tree->invisibleRootItem() ? nullptr : parent))
            position = parent->indexOfChild(after) + 1;
    }
    auto* item = new QTreeWidgetItem;
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | ((w->style & TVS_CHECKBOXES) ? Qt::ItemIsUserCheckable : Qt::ItemFlags()));
    {
        QuietScope quiet(w);
        parent->insertChild(position, item);
        w->treeItems.insert(item);
        if (w->style & TVS_CHECKBOXES)
            item->setCheckState(0, Qt::Unchecked);
    }
    applyTreeItem(w, item, &is->item);
    return treeHandle(item);
}

NMTREEVIEW treeNotification(Window* w, QTreeWidgetItem* oldItem, QTreeWidgetItem* newItem, UINT action)
{
    NMTREEVIEW nm{};
    nm.action = action;
    auto fill = [w](TVITEM& tv, QTreeWidgetItem* item) {
        tv.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
        if (!item)
            return;
        tv.hItem = treeHandle(item);
        tv.lParam = static_cast<LPARAM>(item->data(0, kParamRole).toLongLong());
        tv.state = treeItemState(w, item);
    };
    fill(nm.itemOld, oldItem);
    fill(nm.itemNew, newItem);
    return nm;
}

void treeDeleteItem(Window* w, QTreeWidgetItem* item)
{
    // Los hijos primero; TVN_DELETEITEM de cada uno (el plugin libera ahí su lParam)
    while (item->childCount())
        treeDeleteItem(w, item->child(item->childCount() - 1));
    NMTREEVIEW nm = treeNotification(w, item, nullptr, 0);
    notifyNM(w, nm, TVN_DELETEITEM);
    w->treeItems.erase(item);
    if (w->caretOverride == item)
        w->caretOverride = nullptr;
    QuietScope quiet(w);
    delete item;
}

QTreeWidgetItem* treeNext(QTreeWidgetItem* item, int step)
{
    QTreeWidgetItem* parent = item->parent() ? item->parent() : item->treeWidget()->invisibleRootItem();
    const int index = parent->indexOfChild(item) + step;
    return index >= 0 && index < parent->childCount() ? parent->child(index) : nullptr;
}

int treeCount(QTreeWidgetItem* item)
{
    int count = item->childCount();
    for (int i = 0; i < item->childCount(); ++i)
        count += treeCount(item->child(i));
    return count;
}

LRESULT treeViewMessage(Window* w, UINT message, WPARAM wParam, LPARAM lParam)
{
    QTreeWidget* tree = treeView(w);
    if (!tree)
        return 0;
    auto handleArg = [lParam] { return reinterpret_cast<HTREEITEM>(lParam); };
    switch (message) {
    case TVM_INSERTITEMA:
        return reinterpret_cast<LRESULT>(treeInsertItem(w, reinterpret_cast<const TVINSERTSTRUCTA*>(lParam)));
    case TVM_INSERTITEMW:
        return reinterpret_cast<LRESULT>(treeInsertItem(w, reinterpret_cast<const TVINSERTSTRUCTW*>(lParam)));
    case TVM_DELETEITEM: {
        if (handleArg() == TVI_ROOT || !handleArg()) {
            while (tree->topLevelItemCount())
                treeDeleteItem(w, tree->topLevelItem(tree->topLevelItemCount() - 1));
            return TRUE;
        }
        QTreeWidgetItem* item = treeItem(w, handleArg());
        if (!item)
            return FALSE;
        treeDeleteItem(w, item);
        return TRUE;
    }
    case TVM_EXPAND: {
        // Sin TVN_ITEMEXPANDING ni TVN_ITEMEXPANDED, como en Windows
        QTreeWidgetItem* item = treeItem(w, handleArg());
        if (!item)
            return FALSE;
        QuietScope quiet(w);
        const UINT code = static_cast<UINT>(wParam) & 3;
        item->setExpanded(code == TVE_EXPAND ? true : code == TVE_COLLAPSE ? false : !item->isExpanded());
        return TRUE;
    }
    case TVM_GETCOUNT:
        return treeCount(tree->invisibleRootItem());
    case TVM_GETVISIBLECOUNT: {
        const int row = QFontMetrics(tree->font()).height() + 4;
        return std::max(1, tree->viewport()->height() / row);
    }
    case TVM_GETNEXTITEM: {
        QTreeWidgetItem* item = treeItem(w, handleArg());
        QTreeWidgetItem* result = nullptr;
        switch (static_cast<UINT>(wParam)) {
        case TVGN_ROOT: result = tree->topLevelItem(0); break;
        case TVGN_NEXT: result = item ? treeNext(item, 1) : nullptr; break;
        case TVGN_PREVIOUS: result = item ? treeNext(item, -1) : nullptr; break;
        case TVGN_PARENT: result = item ? item->parent() : nullptr; break;
        case TVGN_CHILD: result = item ? item->child(0) : tree->topLevelItem(0); break;
        case TVGN_CARET:
            result = w->caretOverridden ? static_cast<QTreeWidgetItem*>(w->caretOverride) : tree->currentItem();
            break;
        case TVGN_FIRSTVISIBLE: result = tree->itemAt(QPoint(1, 1)); break;
        case TVGN_NEXTVISIBLE: result = item ? tree->itemBelow(item) : nullptr; break;
        case TVGN_PREVIOUSVISIBLE: result = item ? tree->itemAbove(item) : nullptr; break;
        case TVGN_LASTVISIBLE: {
            result = tree->topLevelItem(tree->topLevelItemCount() - 1);
            while (result && result->isExpanded() && result->childCount())
                result = result->child(result->childCount() - 1);
            break;
        }
        default: break;
        }
        return reinterpret_cast<LRESULT>(result ? treeHandle(result) : nullptr);
    }
    case TVM_SELECTITEM: {
        QTreeWidgetItem* item = treeItem(w, handleArg());
        switch (static_cast<UINT>(wParam)) {
        case TVGN_CARET:
            w->treeAction = TVC_UNKNOWN;
            if (item) {
                for (QTreeWidgetItem* p = item->parent(); p; p = p->parent()) {
                    QuietScope quiet(w);
                    p->setExpanded(true);
                }
                tree->setCurrentItem(item);
                tree->scrollToItem(item);
            } else {
                tree->setCurrentItem(nullptr);
                tree->clearSelection();
            }
            return TRUE;
        case TVGN_FIRSTVISIBLE:
            if (item)
                tree->scrollToItem(item, QAbstractItemView::PositionAtTop);
            return item ? TRUE : FALSE;
        default:
            return TRUE; // TVGN_DROPHILITE: sin resaltado de arrastrar
        }
    }
    case TVM_GETITEMA:
        return treeGetItem(w, reinterpret_cast<TVITEMA*>(lParam));
    case TVM_GETITEMW:
        return treeGetItem(w, reinterpret_cast<TVITEMW*>(lParam));
    case TVM_SETITEMA:
    case TVM_SETITEMW: {
        if (!lParam)
            return FALSE;
        if (message == TVM_SETITEMW) {
            const auto* tv = reinterpret_cast<const TVITEMW*>(lParam);
            QTreeWidgetItem* item = treeItem(w, tv->hItem);
            if (!item)
                return FALSE;
            applyTreeItem(w, item, tv);
        } else {
            const auto* tv = reinterpret_cast<const TVITEMA*>(lParam);
            QTreeWidgetItem* item = treeItem(w, tv->hItem);
            if (!item)
                return FALSE;
            applyTreeItem(w, item, tv);
        }
        return TRUE;
    }
    case TVM_GETITEMSTATE: {
        QTreeWidgetItem* item = treeItem(w, reinterpret_cast<HTREEITEM>(wParam));
        return item ? (treeItemState(w, item) & static_cast<UINT>(lParam)) : 0;
    }
    case TVM_ENSUREVISIBLE: {
        QTreeWidgetItem* item = treeItem(w, handleArg());
        if (!item)
            return FALSE;
        QuietScope quiet(w);
        for (QTreeWidgetItem* p = item->parent(); p; p = p->parent())
            p->setExpanded(true);
        tree->scrollToItem(item);
        return TRUE;
    }
    case TVM_SORTCHILDREN: {
        QTreeWidgetItem* item = handleArg() == TVI_ROOT || !handleArg() ? tree->invisibleRootItem() : treeItem(w, handleArg());
        if (!item)
            return FALSE;
        QuietScope quiet(w);
        item->sortChildren(0, Qt::AscendingOrder);
        return TRUE;
    }
    case TVM_GETITEMRECT: {
        auto* rect = reinterpret_cast<RECT*>(lParam);
        HTREEITEM handle = nullptr;
        if (rect)
            std::memcpy(&handle, rect, sizeof(handle)); // el HTREEITEM, en los primeros bytes del RECT
        QTreeWidgetItem* item = treeItem(w, handle);
        if (!item)
            return FALSE;
        QRect r = tree->visualItemRect(item);
        if (!r.isValid())
            return FALSE;
        if (wParam) // solo el texto
            r.setWidth(std::min(r.width(), QFontMetrics(item->font(0)).horizontalAdvance(item->text(0)) + 6));
        r.translate(clientPoint(tree, QPoint(0, 0)));
        *rect = RECT{r.left(), r.top(), r.right() + 1, r.bottom() + 1};
        return TRUE;
    }
    case TVM_HITTEST: {
        auto* info = reinterpret_cast<TVHITTESTINFO*>(lParam);
        if (!info)
            return 0;
        QTreeWidgetItem* item = tree->itemAt(tree->viewport()->mapFrom(tree, QPoint(static_cast<int>(info->pt.x), static_cast<int>(info->pt.y))));
        info->hItem = item ? treeHandle(item) : nullptr;
        info->flags = item ? TVHT_ONITEMLABEL : TVHT_NOWHERE;
        return reinterpret_cast<LRESULT>(info->hItem);
    }
    case TVM_SETBKCOLOR:
    case TVM_SETTEXTCOLOR: {
        const COLORREF c = static_cast<COLORREF>(lParam);
        if (c == CLR_INVALID)
            return static_cast<LRESULT>(CLR_INVALID);
        QPalette palette = tree->palette();
        const QPalette::ColorRole role = message == TVM_SETBKCOLOR ? QPalette::Base : QPalette::Text;
        const COLORREF previous = colorRef(palette.color(role));
        palette.setColor(role, QColor(GetRValue(c), GetGValue(c), GetBValue(c)));
        tree->setPalette(palette);
        return static_cast<LRESULT>(previous);
    }
    case TVM_SETIMAGELIST:
        return 0;
    default:
        return 0;
    }
}

QWidget* createTreeView(Window* w, QWidget* parent)
{
    w->kind = Kind::TreeView;
    auto* tree = new QTreeWidget(parent);
    tree->setHeaderHidden(true);
    tree->setColumnCount(1);
    tree->setRootIsDecorated(w->style & TVS_LINESATROOT);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->setExpandsOnDoubleClick(true);
    tree->setAllColumnsShowFocus(w->style & TVS_FULLROWSELECT);
    // La selección: TVN_SELCHANGING (TreeView_GetSelection aún da la anterior; con TRUE no
    // cambia) y TVN_SELCHANGED
    QObject::connect(tree, &QTreeWidget::currentItemChanged, tree, [w, tree](QTreeWidgetItem* current, QTreeWidgetItem* previous) {
        if (w->quiet)
            return;
        const UINT action = w->treeAction;
        w->treeAction = TVC_UNKNOWN;
        NMTREEVIEW nm = treeNotification(w, previous, current, action);
        w->caretOverride = previous;
        w->caretOverridden = true;
        const LRESULT refused = notifyNM(w, nm, TVN_SELCHANGING);
        w->caretOverridden = false;
        if (refused) {
            QuietScope quiet(w);
            tree->setCurrentItem(previous);
            return;
        }
        nm = treeNotification(w, previous, current, action);
        notifyNM(w, nm, TVN_SELCHANGED);
    });
    // Expandir (también para rellenar los hijos al vuelo): TVN_ITEMEXPANDING, con TRUE no
    // se expande, y TVN_ITEMEXPANDED
    auto expansion = [w, tree](QTreeWidgetItem* item, bool expanded) {
        if (w->quiet)
            return;
        NMTREEVIEW nm = treeNotification(w, nullptr, item, expanded ? TVE_EXPAND : TVE_COLLAPSE);
        if (notifyNM(w, nm, TVN_ITEMEXPANDING)) {
            QuietScope quiet(w);
            item->setExpanded(!expanded);
            return;
        }
        if (expanded && item->childCount() == 0)
            item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);
        nm = treeNotification(w, nullptr, item, expanded ? TVE_EXPAND : TVE_COLLAPSE);
        notifyNM(w, nm, TVN_ITEMEXPANDED);
        tree->viewport()->update();
    };
    QObject::connect(tree, &QTreeWidget::itemExpanded, tree, [expansion](QTreeWidgetItem* item) { expansion(item, true); });
    QObject::connect(tree, &QTreeWidget::itemCollapsed, tree, [expansion](QTreeWidgetItem* item) { expansion(item, false); });
    auto* filter = new ComctlFilter(w, tree);
    tree->viewport()->installEventFilter(filter);
    tree->installEventFilter(filter);
    return tree;
}

bool ComctlFilter::eventFilter(QObject* object, QEvent* event)
{
    Window* w = _w;
    auto* view = qobject_cast<QTreeWidget*>(w->widget);
    if (!view || !w->alive || w->quiet)
        return false;
    const bool onViewport = object == view->viewport();
    const bool list = w->kind == Kind::ListView;
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        if (onViewport && !list)
            w->treeAction = TVC_BYMOUSE;
        break;
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick: {
        if (!onViewport)
            break;
        auto* mouse = static_cast<QMouseEvent*>(event);
        const bool right = mouse->button() == Qt::RightButton;
        if (mouse->button() != Qt::LeftButton && !right)
            break;
        const bool doubleClick = event->type() == QEvent::MouseButtonDblClick;
        const UINT code = doubleClick ? (right ? NM_RDBLCLK : NM_DBLCLK) : (right ? NM_RCLICK : NM_CLICK);
        const QPoint point = mouse->position().toPoint();
        if (list) {
            QTreeWidgetItem* item = view->itemAt(point);
            NMITEMACTIVATE nm{};
            nm.iItem = item ? view->indexOfTopLevelItem(item) : -1;
            nm.iSubItem = item ? std::max(0, view->columnAt(point.x())) : 0;
            const QPoint client = clientPoint(view, point);
            nm.ptAction = POINT{client.x(), client.y()};
            if (item)
                nm.lParam = static_cast<LPARAM>(item->data(0, kParamRole).toLongLong());
            notifyNM(w, nm, code);
            if (doubleClick && !right && item) {
                NMITEMACTIVATE activate = nm;
                notifyNM(w, activate, LVN_ITEMACTIVATE);
            }
        } else {
            notifyCode(w, code);
        }
        break;
    }
    case QEvent::KeyPress: {
        if (onViewport)
            break;
        auto* key = static_cast<QKeyEvent*>(event);
        const WORD vk = static_cast<WORD>(virtualKey(key));
        if (list) {
            NMLVKEYDOWN nm{};
            nm.wVKey = vk;
            notifyNM(w, nm, LVN_KEYDOWN);
        } else {
            w->treeAction = TVC_BYKEYBOARD;
            NMTVKEYDOWN nm{};
            nm.wVKey = vk;
            notifyNM(w, nm, TVN_KEYDOWN);
        }
        if (vk == VK_RETURN) {
            notifyCode(w, NM_RETURN);
            if (list && view->currentItem()) {
                NMITEMACTIVATE nm{};
                nm.iItem = view->indexOfTopLevelItem(view->currentItem());
                notifyNM(w, nm, LVN_ITEMACTIVATE);
            }
        }
        break;
    }
    default:
        break;
    }
    return false;
}

// --- Barras de progreso, deslizadores y tooltips ----------------------------------------------------

LRESULT progressMessage(Window* w, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* bar = qobject_cast<QProgressBar*>(w->widget);
    if (!bar)
        return 0;
    const int value = bar->value() < bar->minimum() ? bar->minimum() : bar->value();
    switch (message) {
    case PBM_SETRANGE: {
        const LRESULT previous = MAKELPARAM(bar->minimum(), bar->maximum());
        bar->setRange(LOWORD(lParam), HIWORD(lParam));
        return previous;
    }
    case PBM_SETRANGE32: {
        const LRESULT previous = MAKELPARAM(bar->minimum(), bar->maximum());
        bar->setRange(static_cast<int>(wParam), static_cast<int>(lParam));
        return previous;
    }
    case PBM_GETRANGE: {
        struct Range { int low; int high; };
        if (auto* range = reinterpret_cast<Range*>(lParam)) {
            range->low = bar->minimum();
            range->high = bar->maximum();
        }
        return wParam ? bar->minimum() : bar->maximum();
    }
    case PBM_SETPOS:
        bar->setValue(std::clamp(static_cast<int>(wParam), bar->minimum(), bar->maximum()));
        return value;
    case PBM_DELTAPOS:
        bar->setValue(std::clamp(value + static_cast<int>(wParam), bar->minimum(), bar->maximum()));
        return value;
    case PBM_SETSTEP: {
        const int previous = w->progressStep;
        w->progressStep = static_cast<int>(wParam);
        return previous;
    }
    case PBM_STEPIT: {
        int next = value + w->progressStep;
        if (next > bar->maximum())
            next = bar->minimum() + (next - bar->maximum()); // da la vuelta, como en Windows
        bar->setValue(std::clamp(next, bar->minimum(), bar->maximum()));
        return value;
    }
    case PBM_GETPOS:
        return value;
    case PBM_SETMARQUEE:
        if (wParam)
            bar->setRange(0, 0);
        else
            bar->setRange(0, 100);
        return TRUE;
    default:
        return 0;
    }
}

LRESULT trackbarMessage(Window* w, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* slider = qobject_cast<QSlider*>(w->widget);
    if (!slider)
        return 0;
    QuietScope quiet(w);
    switch (message) {
    case TBM_GETPOS: return slider->sliderPosition();
    case TBM_GETRANGEMIN: return slider->minimum();
    case TBM_GETRANGEMAX: return slider->maximum();
    case TBM_SETPOS: slider->setValue(static_cast<int>(lParam)); return 0;
    case TBM_SETRANGE: slider->setRange(LOWORD(lParam), HIWORD(lParam)); return 0;
    case TBM_SETRANGEMIN: slider->setMinimum(static_cast<int>(lParam)); return 0;
    case TBM_SETRANGEMAX: slider->setMaximum(static_cast<int>(lParam)); return 0;
    case TBM_SETTICFREQ: slider->setTickInterval(static_cast<int>(wParam)); return 0;
    case TBM_SETPAGESIZE: {
        const int previous = slider->pageStep();
        slider->setPageStep(static_cast<int>(lParam));
        return previous;
    }
    case TBM_SETLINESIZE: {
        const int previous = slider->singleStep();
        slider->setSingleStep(static_cast<int>(lParam));
        return previous;
    }
    default: return 0;
    }
}

template<typename Info>
LRESULT tooltipMessage(Window* w, const Info* info, bool remove)
{
    if (!info)
        return FALSE;
    HWND target = (info->uFlags & TTF_IDISHWND) ? reinterpret_cast<HWND>(info->uId) : info->hwnd;
    QWidget* widget = widgetOf(target);
    if (!widget)
        return FALSE;
    QString text;
    if (!remove && info->lpszText) {
        if (IS_INTRESOURCE(info->lpszText)) {
            TCHAR buffer[1024] = {};
            LoadString(info->hinst, static_cast<UINT>(reinterpret_cast<uintptr_t>(info->lpszText)), buffer, 1024);
            text = fromT(buffer);
        } else {
            text = itemText(info->lpszText);
        }
    }
    auto& tools = w->tools;
    tools.erase(std::remove_if(tools.begin(), tools.end(), [target](const auto& t) { return t.first == target; }), tools.end());
    if (!remove)
        tools.emplace_back(target, text);
    widget->setToolTip(remove ? QString() : text);
    return TRUE;
}

QWidget* createSmallControl(Window* w, const QString& name, QWidget* parent)
{
    if (name == QLatin1String("MSCTLS_PROGRESS32")) {
        w->kind = Kind::Progress;
        auto* bar = new QProgressBar(parent);
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setTextVisible(false);
        if (w->style & PBS_VERTICAL)
            bar->setOrientation(Qt::Vertical);
        if (w->style & PBS_MARQUEE)
            bar->setRange(0, 0);
        return bar;
    }
    if (name == QLatin1String("MSCTLS_TRACKBAR32")) {
        w->kind = Kind::Trackbar;
        auto* slider = new QSlider((w->style & TBS_VERT) ? Qt::Vertical : Qt::Horizontal, parent);
        slider->setRange(0, 100);
        slider->setTickPosition((w->style & TBS_NOTICKS) ? QSlider::NoTicks : QSlider::TicksBelow);
        // WM_HSCROLL / WM_VSCROLL al padre, con el código y la posición
        auto scroll = [w, slider](int code) {
            Window* parentWindow = windowOf(w->parent);
            if (!parentWindow || w->quiet)
                return;
            const UINT message = slider->orientation() == Qt::Horizontal ? WM_HSCROLL : WM_VSCROLL;
            send(parentWindow, message, MAKEWPARAM(static_cast<WORD>(code), static_cast<WORD>(slider->sliderPosition())),
                 reinterpret_cast<LPARAM>(hwndOf(w)));
        };
        QObject::connect(slider, &QSlider::actionTriggered, slider, [scroll](int action) {
            switch (action) {
            case QAbstractSlider::SliderSingleStepAdd: scroll(TB_LINEDOWN); scroll(TB_ENDTRACK); break;
            case QAbstractSlider::SliderSingleStepSub: scroll(TB_LINEUP); scroll(TB_ENDTRACK); break;
            case QAbstractSlider::SliderPageStepAdd: scroll(TB_PAGEDOWN); scroll(TB_ENDTRACK); break;
            case QAbstractSlider::SliderPageStepSub: scroll(TB_PAGEUP); scroll(TB_ENDTRACK); break;
            case QAbstractSlider::SliderToMinimum: scroll(TB_TOP); scroll(TB_ENDTRACK); break;
            case QAbstractSlider::SliderToMaximum: scroll(TB_BOTTOM); scroll(TB_ENDTRACK); break;
            case QAbstractSlider::SliderMove: scroll(TB_THUMBTRACK); break;
            default: break;
            }
        });
        QObject::connect(slider, &QSlider::sliderReleased, slider, [scroll] {
            scroll(TB_THUMBPOSITION);
            scroll(TB_ENDTRACK);
        });
        return slider;
    }
    if (name == QLatin1String("TOOLTIPS_CLASS32")) {
        // No se ve: sus herramientas son el toolTip de cada ventana
        w->kind = Kind::Tooltip;
        auto* hidden = new QWidget(nullptr);
        hidden->setAttribute(Qt::WA_DontShowOnScreen);
        return hidden;
    }
    return nullptr;
}

LRESULT commonControlMessage(Window* w, UINT message, WPARAM wParam, LPARAM lParam, bool* handled)
{
    *handled = true;
    switch (w->kind) {
    case Kind::Tab:
        if (message >= TCM_FIRST && message < TCM_FIRST + 0x100)
            return tabMessage(w, message, wParam, lParam);
        break;
    case Kind::ListView:
        if (message >= LVM_FIRST && message < LVM_FIRST + 0x100)
            return listViewMessage(w, message, wParam, lParam);
        break;
    case Kind::TreeView:
        if (message >= TV_FIRST && message < TV_FIRST + 0x100)
            return treeViewMessage(w, message, wParam, lParam);
        break;
    case Kind::Progress:
        if (message > WM_USER && message <= WM_USER + 10)
            return progressMessage(w, message, wParam, lParam);
        break;
    case Kind::Trackbar:
        if (message >= WM_USER && message <= WM_USER + 30)
            return trackbarMessage(w, message, wParam, lParam);
        break;
    case Kind::Tooltip:
        switch (message) {
        case TTM_ADDTOOLA:
        case TTM_UPDATETIPTEXTA:
            return tooltipMessage(w, reinterpret_cast<const TTTOOLINFOA*>(lParam), false);
        case TTM_ADDTOOLW:
        case TTM_UPDATETIPTEXTW:
            return tooltipMessage(w, reinterpret_cast<const TTTOOLINFOW*>(lParam), false);
        case TTM_DELTOOLA:
            return tooltipMessage(w, reinterpret_cast<const TTTOOLINFOA*>(lParam), true);
        case TTM_DELTOOLW:
            return tooltipMessage(w, reinterpret_cast<const TTTOOLINFOW*>(lParam), true);
        case TTM_ACTIVATE:
        case TTM_SETMAXTIPWIDTH:
            return 0;
        default:
            break;
        }
        break;
    default:
        break;
    }
    *handled = false;
    return 0;
}

QWidget* createCommonControl(Window* w, const QString& name, QWidget* parent)
{
    if (name == QLatin1String("SYSTABCONTROL32"))
        return createTabControl(w, parent);
    if (name == QLatin1String("SYSLISTVIEW32"))
        return createListView(w, parent);
    if (name == QLatin1String("SYSTREEVIEW32"))
        return createTreeView(w, parent);
    return createSmallControl(w, name, parent);
}

LRESULT CALLBACK controlProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return 0;
    bool common = false;
    const LRESULT commonResult = commonControlMessage(w, message, wParam, lParam, &common);
    if (common)
        return commonResult;
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
    if (!widget)
        widget = createCommonControl(w, name, parent);
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

// --- Recursos y plantillas de diálogo ----------------------------------------------------

const EditaResource* findResource(unsigned type, const QString& name)
{
    const Registry& r = registry();
    bool numeric = false;
    unsigned id = 0;
    if (name.startsWith(QLatin1Char('#'))) {
        id = name.mid(1).toUInt(&numeric);
    }
    for (unsigned long i = 0; i < r.resourceCount; ++i) {
        const EditaResource& res = r.resources[i];
        if (res.type != type)
            continue;
        if (numeric ? (!res.name && res.id == id) : (res.name && name.compare(QString::fromUtf8(res.name), Qt::CaseInsensitive) == 0))
            return &res;
    }
    return nullptr;
}

bool isResource(const void* handle)
{
    const Registry& r = registry();
    return r.resources && handle >= static_cast<const void*>(r.resources)
           && handle < static_cast<const void*>(r.resources + r.resourceCount);
}

// Lectura de una plantilla binaria (alineada como en Windows)
class TemplateReader
{
public:
    TemplateReader(const unsigned char* data) : _start(data), _p(data) {}
    unsigned word()
    {
        uint16_t v = 0;
        std::memcpy(&v, _p, sizeof v);
        _p += sizeof v;
        return v;
    }
    uint32_t dword()
    {
        uint32_t v = 0;
        std::memcpy(&v, _p, sizeof v);
        _p += sizeof v;
        return v;
    }
    int shortValue() { return static_cast<int16_t>(word()); }
    unsigned byte() { return *_p++; }
    void align()
    {
        while ((_p - _start) % 4)
            ++_p;
    }
    // Una cadena UTF-16 terminada en nulo
    QString text()
    {
        std::u16string units;
        for (unsigned unit = word(); unit; unit = word())
            units += static_cast<char16_t>(unit);
        return QString::fromStdU16String(units);
    }
    // sz_Or_Ord: nada, un número (#n) o una cadena
    QString nameOrOrdinal()
    {
        uint16_t first = 0;
        std::memcpy(&first, _p, sizeof first);
        if (first == 0) {
            _p += 2;
            return {};
        }
        if (first == 0xFFFF) {
            _p += 2;
            return QStringLiteral("#%1").arg(word());
        }
        return text();
    }
    void skip(std::size_t bytes) { _p += bytes; }

private:
    const unsigned char* _start;
    const unsigned char* _p;
};

struct TemplateItem
{
    DWORD style = 0;
    DWORD exStyle = 0;
    int x = 0, y = 0, cx = 0, cy = 0;
    int id = 0;
    QString className;
    QString title;
};

struct DialogTemplate
{
    DWORD style = 0;
    DWORD exStyle = 0;
    int x = 0, y = 0, cx = 0, cy = 0;
    QString menu;
    QString className;
    QString title;
    int pointSize = 0;
    QString face;
    std::vector<TemplateItem> items;
};

QString standardClass(const QString& name)
{
    static const char* const kAtoms[] = {"BUTTON", "EDIT", "STATIC", "LISTBOX", "SCROLLBAR", "COMBOBOX"};
    if (name.startsWith(QLatin1Char('#'))) {
        const unsigned atom = name.mid(1).toUInt();
        if (atom >= 0x80 && atom <= 0x85)
            return QString::fromLatin1(kAtoms[atom - 0x80]);
    }
    return name;
}

// DLGTEMPLATE o DLGTEMPLATEEX (la firma 0xFFFF lo distingue)
DialogTemplate parseTemplate(const void* data)
{
    DialogTemplate t;
    TemplateReader r(static_cast<const unsigned char*>(data));
    uint16_t version[2] = {0, 0};
    std::memcpy(version, data, sizeof version);
    const bool extended = version[0] == 1 && version[1] == 0xFFFF;
    unsigned count = 0;
    if (extended) {
        r.word();
        r.word();
        r.dword(); // helpID
        t.exStyle = r.dword();
        t.style = r.dword();
    } else {
        t.style = r.dword();
        t.exStyle = r.dword();
    }
    count = r.word();
    t.x = r.shortValue();
    t.y = r.shortValue();
    t.cx = r.shortValue();
    t.cy = r.shortValue();
    t.menu = r.nameOrOrdinal();
    t.className = r.nameOrOrdinal();
    t.title = r.text();
    if (t.style & (DS_SETFONT | DS_SHELLFONT)) {
        t.pointSize = static_cast<int>(r.word());
        if (extended) {
            r.word(); // weight
            r.byte(); // italic
            r.byte(); // charset
        }
        t.face = r.text();
    }
    for (unsigned i = 0; i < count; ++i) {
        r.align();
        TemplateItem item;
        if (extended) {
            r.dword(); // helpID
            item.exStyle = r.dword();
            item.style = r.dword();
        } else {
            item.style = r.dword();
            item.exStyle = r.dword();
        }
        item.x = r.shortValue();
        item.y = r.shortValue();
        item.cx = r.shortValue();
        item.cy = r.shortValue();
        item.id = extended ? static_cast<int>(static_cast<int32_t>(r.dword())) : static_cast<int>(static_cast<int16_t>(r.word()));
        item.className = standardClass(r.nameOrOrdinal());
        item.title = r.nameOrOrdinal();
        if (item.title.startsWith(QLatin1Char('#')))
            item.title.clear(); // un icono o un mapa de bits por número
        r.skip(r.word());       // datos de creación
        t.items.push_back(item);
    }
    return t;
}

// Las unidades de diálogo de una fuente: el ancho medio de las letras y el alto, como Windows
QSize dialogBaseUnits(const QFont& font)
{
    const QFontMetrics metrics(font);
    const int width = metrics.horizontalAdvance(QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"));
    return QSize((width / 26 + 1) / 2, metrics.height());
}

QRect toPixels(const QSize& base, int x, int y, int cx, int cy)
{
    return QRect(MulDiv(x, base.width(), 4), MulDiv(y, base.height(), 8), MulDiv(cx, base.width(), 4), MulDiv(cy, base.height(), 8));
}

HWND createWindow(DWORD exStyle, const QString& className, const QString& windowName, DWORD style, int x, int y, int width,
                  int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param);

// Un diálogo de una plantilla, con sus controles; WM_INITDIALOG al final
HWND createDialogFromTemplate(HINSTANCE instance, const DialogTemplate& t, HWND parent, DLGPROC proc, LPARAM init, bool modal)
{
    Window* w = newWindow(Kind::Dialog, t.style, t.exStyle, parent, instance);
    w->className = QStringLiteral("#32770");
    w->dlgProc = proc;
    w->proc = w->builtin = dialogProc;
    HostWidget* widget = newHostWidget(w, parent, true);
    w->baseUnits = dialogBaseUnits(widget->font());
    const QRect geometry = toPixels(w->baseUnits, t.x, t.y, t.cx, t.cy);
    widget->resize(geometry.size());
    widget->preferred = geometry.size();
    if (!widget->isWindow()) {
        widget->move(geometry.topLeft());
    } else {
        // Centrado sobre su dueño (o en la pantalla), como DS_CENTER
        QWidget* owner = widgetOf(parent);
        const QRect area = owner ? owner->window()->frameGeometry()
                                 : (QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->availableGeometry() : QRect());
        if (area.isValid())
            widget->move(area.center() - QPoint(geometry.width() / 2, geometry.height() / 2));
        if (modal)
            widget->setWindowModality(Qt::ApplicationModal);
    }
    w->text = t.title;
    widget->setWindowTitle(t.title);
    widget->hide();
    HWND hwnd = hwndOf(w);

    HWND firstTabStop = nullptr;
    for (const TemplateItem& item : t.items) {
        const QRect r = toPixels(w->baseUnits, item.x, item.y, item.cx, item.cy);
        const DWORD style = item.style | WS_CHILD;
        HWND control = createWindow(item.exStyle, item.className, item.title, style, r.x(), r.y(), r.width(), r.height(), hwnd,
                                    reinterpret_cast<HMENU>(static_cast<intptr_t>(item.id)), instance, nullptr);
        if (!control) {
            // Una clase que la capa no tiene (lista con columnas, árbol…): un recuadro con su
            // nombre, con el mismo id, para que el diálogo se vea completo
            control = createWindow(0, QStringLiteral("STATIC"), QStringLiteral("[%1]").arg(item.className),
                                   WS_CHILD | SS_CENTER | SS_CENTERIMAGE | SS_SUNKEN | (item.style & WS_VISIBLE), r.x(), r.y(),
                                   r.width(), r.height(), hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(item.id)), instance,
                                   nullptr);
            EnableWindow(control, FALSE);
            continue;
        }
        if (Window* c = windowOf(control); c && c->kind == Kind::ComboBox && c->widget) {
            // En el .rc, la altura de un combo es la de su lista desplegada
            c->widget->resize(r.width(), c->widget->sizeHint().height());
        }
        if (!firstTabStop && (style & WS_TABSTOP) && !(style & WS_DISABLED))
            firstTabStop = control;
    }
    if (t.style & WS_CHILD)
        widget->setVisible(t.style & WS_VISIBLE);
    if (send(w, WM_INITDIALOG, reinterpret_cast<WPARAM>(firstTabStop), init) && firstTabStop && w->alive)
        SetFocus(firstTabStop);
    if (!w->alive)
        return nullptr;
    w->created = true;
    if ((t.style & WS_VISIBLE) && !modal && !w->ended)
        widget->show();
    return hwnd;
}

INT_PTR runModal(HWND hwnd)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return -1;
    if (!w->ended && w->widget) {
        w->widget->show();
        w->widget->raise();
        w->widget->activateWindow();
        QEventLoop loop;
        w->modalLoop = &loop;
        loop.exec();
        w->modalLoop = nullptr;
    }
    const INT_PTR result = w->modalResult;
    DestroyWindow(hwnd);
    return result;
}

template<typename Ch>
QString resourceArgument(const Ch* name)
{
    if (IS_INTRESOURCE(name))
        return QStringLiteral("#%1").arg(static_cast<unsigned>(reinterpret_cast<uintptr_t>(name)));
    if constexpr (sizeof(Ch) == 1)
        return QString::fromUtf8(reinterpret_cast<const char*>(name));
    else
        return QString::fromWCharArray(reinterpret_cast<const wchar_t*>(name));
}

// LoadMenuIndirect: la plantilla clásica (versión 0)
void buildMenu(TemplateReader& r, HMENU menu)
{
    for (;;) {
        const unsigned flags = r.word();
        const bool popup = flags & MF_POPUP;
        const unsigned id = popup ? 0 : r.word();
        const QString text = r.text();
        const std::wstring wide = text.toStdWString();
        const UINT keep = flags & (MF_CHECKED | MF_GRAYED | MF_DISABLED | MF_MENUBARBREAK | MF_MENUBREAK);
        if (popup) {
            HMENU sub = CreatePopupMenu();
            buildMenu(r, sub);
            AppendMenuW(menu, MF_POPUP | keep, reinterpret_cast<UINT_PTR>(sub), wide.c_str());
        } else if (id == 0 && text.isEmpty() && !(flags & ~0x80u)) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        } else {
            AppendMenuW(menu, MF_STRING | keep, id, wide.c_str());
        }
        if (flags & 0x80) // MF_END
            break;
    }
}

// --- SetWindowSubclass ----------------------------------------------------------------------

// En qué subclase está cada llamada (DefSubclassProc pasa a la siguiente)
thread_local std::vector<std::pair<Window*, int>> t_subclassCalls;

LRESULT callSubclass(Window* w, int level, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (level < 0 || level >= static_cast<int>(w->subclasses.size()))
        return CallWindowProcW(w->subclassBase, hwnd, message, wParam, lParam);
    const Window::Subclass subclass = w->subclasses[static_cast<std::size_t>(level)];
    t_subclassCalls.emplace_back(w, level);
    const LRESULT result = subclass.proc(hwnd, message, wParam, lParam, subclass.id, subclass.refData);
    t_subclassCalls.pop_back();
    return result;
}

LRESULT CALLBACK subclassDispatch(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return 0;
    return callSubclass(w, static_cast<int>(w->subclasses.size()) - 1, hwnd, message, wParam, lParam);
}

// --- Diálogos comunes ---------------------------------------------------------------------------

template<typename Ch>
QString fromChars(const Ch* s)
{
    if (!s)
        return {};
    if constexpr (sizeof(Ch) == 1)
        return QString::fromUtf8(reinterpret_cast<const char*>(s));
    else
        return QString::fromWCharArray(reinterpret_cast<const wchar_t*>(s));
}

template<typename Ch>
std::basic_string<Ch> toChars(const QString& s)
{
    if constexpr (sizeof(Ch) == 1)
        return s.toStdString();
    else
        return s.toStdWString();
}

// "Texto\0*.txt;*.log\0Todo\0*.*\0\0" → los filtros de Qt
template<typename Ch>
QStringList fileFilters(const Ch* filter)
{
    QStringList filters;
    if (!filter)
        return filters;
    const Ch* p = filter;
    while (*p) {
        const QString description = fromChars(p);
        while (*p)
            ++p;
        ++p;
        if (!*p)
            break;
        QString patterns = fromChars(p);
        while (*p)
            ++p;
        ++p;
        patterns.replace(QLatin1Char(';'), QLatin1Char(' '));
        if (patterns == QLatin1String("*.*"))
            patterns = QStringLiteral("*");
        filters << QStringLiteral("%1 (%2)").arg(description, patterns);
    }
    return filters;
}

DWORD g_commonDialogError = 0;

template<typename Ofn>
BOOL fileDialog(Ofn* ofn, bool save)
{
    using Ch = std::remove_const_t<std::remove_pointer_t<decltype(ofn->lpstrFile)>>;
    g_commonDialogError = 0;
    if (!ofn || !ofn->lpstrFile || ofn->nMaxFile == 0)
        return FALSE;
    const QStringList filters = fileFilters(ofn->lpstrFilter);
    const int index = ofn->nFilterIndex > 0 ? static_cast<int>(ofn->nFilterIndex) - 1 : 0;
    QString selectedFilter = index < filters.size() ? filters[index] : QString();
    const QString initialFile = QString(fromChars(ofn->lpstrFile)).replace(QLatin1Char('\\'), QLatin1Char('/'));
    QString start = QString(fromChars(ofn->lpstrInitialDir)).replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (!initialFile.isEmpty())
        start = QFileInfo(initialFile).isRelative() && !start.isEmpty() ? QDir(start).filePath(initialFile) : initialFile;
    QWidget* owner = widgetOf(ofn->hwndOwner);
    const QString title = fromChars(ofn->lpstrTitle);
    QFileDialog::Options options;
    if (!save || !(ofn->Flags & OFN_OVERWRITEPROMPT))
        options |= QFileDialog::DontConfirmOverwrite;
    QStringList files;
    if (save) {
        QString file = QFileDialog::getSaveFileName(owner, title, start, filters.join(QStringLiteral(";;")), &selectedFilter, options);
        const QString defaultExtension = fromChars(ofn->lpstrDefExt);
        if (!file.isEmpty() && !defaultExtension.isEmpty() && QFileInfo(file).suffix().isEmpty())
            file += QLatin1Char('.') + defaultExtension;
        if (!file.isEmpty())
            files << file;
    } else if (ofn->Flags & OFN_ALLOWMULTISELECT) {
        files = QFileDialog::getOpenFileNames(owner, title, start, filters.join(QStringLiteral(";;")), &selectedFilter, options);
    } else {
        const QString file = QFileDialog::getOpenFileName(owner, title, start, filters.join(QStringLiteral(";;")), &selectedFilter, options);
        if (!file.isEmpty())
            files << file;
    }
    if (files.isEmpty())
        return FALSE;
    // Varios: "carpeta\0nombre1\0nombre2\0\0", como el diálogo «Explorer» de Windows
    std::basic_string<Ch> result;
    QString first = files.first();
    if (files.size() > 1) {
        result = toChars<Ch>(QFileInfo(first).absolutePath());
        result += Ch(0);
        for (const QString& f : files) {
            result += toChars<Ch>(QFileInfo(f).fileName());
            result += Ch(0);
        }
    } else {
        result = toChars<Ch>(first);
        result += Ch(0);
    }
    result += Ch(0);
    if (result.size() > ofn->nMaxFile) {
        g_commonDialogError = FNERR_BUFFERTOOSMALL;
        // Como Windows: los dos primeros bytes dicen el tamaño que hace falta
        if (ofn->nMaxFile * sizeof(Ch) >= 2) {
            const auto needed = static_cast<uint16_t>(std::min<std::size_t>(result.size(), 0xFFFF));
            std::memcpy(ofn->lpstrFile, &needed, sizeof needed);
        }
        return FALSE;
    }
    std::copy_n(result.data(), result.size(), ofn->lpstrFile);
    const std::basic_string<Ch> path = toChars<Ch>(first);
    const std::basic_string<Ch> name = toChars<Ch>(QFileInfo(first).fileName());
    ofn->nFileOffset = static_cast<WORD>(files.size() > 1 ? toChars<Ch>(QFileInfo(first).absolutePath()).size() + 1
                                                           : path.size() - name.size());
    const qsizetype dot = name.find_last_of(Ch('.')) == std::basic_string<Ch>::npos ? -1 : static_cast<qsizetype>(name.find_last_of(Ch('.')));
    ofn->nFileExtension = static_cast<WORD>(files.size() == 1 && dot >= 0 ? path.size() - name.size() + static_cast<std::size_t>(dot) + 1 : 0);
    if (ofn->lpstrFileTitle && ofn->nMaxFileTitle)
        copyTo(name, ofn->lpstrFileTitle, static_cast<int>(ofn->nMaxFileTitle));
    const qsizetype chosen = filters.indexOf(selectedFilter);
    if (chosen >= 0)
        ofn->nFilterIndex = static_cast<DWORD>(chosen + 1);
    return TRUE;
}

template<typename Cc>
BOOL chooseColor(Cc* cc)
{
    if (!cc)
        return FALSE;
    const QColor initial = (cc->Flags & CC_RGBINIT)
                               ? QColor(GetRValue(cc->rgbResult), GetGValue(cc->rgbResult), GetBValue(cc->rgbResult))
                               : QColor(Qt::white);
    const QColor color = QColorDialog::getColor(initial, widgetOf(cc->hwndOwner));
    if (!color.isValid())
        return FALSE;
    cc->rgbResult = RGB(color.red(), color.green(), color.blue());
    return TRUE;
}

constexpr uint32_t kPidlMagic = 0x5049444C; // "PIDL"

// El «ITEMIDLIST» de SHBrowseForFolder: la ruta en UTF-8, en memoria de CoTaskMemAlloc
LPITEMIDLIST makePidl(const QString& path)
{
    const QByteArray utf8 = path.toUtf8();
    auto* block = static_cast<char*>(CoTaskMemAlloc(sizeof(uint32_t) + static_cast<std::size_t>(utf8.size()) + 1));
    const uint32_t magic = kPidlMagic;
    std::memcpy(block, &magic, sizeof magic);
    std::memcpy(block + sizeof magic, utf8.constData(), static_cast<std::size_t>(utf8.size()) + 1);
    return reinterpret_cast<LPITEMIDLIST>(block);
}

QString pidlPath(PCIDLIST_ABSOLUTE pidl)
{
    if (!pidl)
        return {};
    uint32_t magic = 0;
    std::memcpy(&magic, pidl, sizeof magic);
    return magic == kPidlMagic ? QString::fromUtf8(reinterpret_cast<const char*>(pidl) + sizeof magic) : QString();
}

template<typename Bi>
PIDLIST_ABSOLUTE browseForFolder(Bi* bi)
{
    if (!bi)
        return nullptr;
    const QString start = pidlPath(bi->pidlRoot);
    const QString folder = QFileDialog::getExistingDirectory(widgetOf(bi->hwndOwner), fromChars(bi->lpszTitle), start);
    if (folder.isEmpty())
        return nullptr;
    if (bi->pszDisplayName) {
        using Ch = std::remove_pointer_t<decltype(bi->pszDisplayName)>;
        copyTo(toChars<Ch>(QFileInfo(folder).fileName()), bi->pszDisplayName, MAX_PATH);
    }
    return makePidl(folder);
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

// --- Dibujo (fase 58) ----------------------------------------------------------------------------
//
// Un HDC de ventana dibuja con el QPainter de su WM_PAINT (fuera de WM_PAINT no dibuja); uno
// en memoria, con un QPainter sobre el QImage del mapa de bits que tenga seleccionado. Las
// coordenadas son las de GDI: los rectángulos excluyen el borde derecho y el de abajo.

QColor qcolor(COLORREF c)
{
    return QColor(GetRValue(c), GetGValue(c), GetBValue(c));
}

// Un HBRUSH: un objeto de la capa o (HBRUSH)(COLOR_xxx + 1), el atajo de Windows
bool brushColor(DeviceContext* dc, HGDIOBJ handle, QColor* color)
{
    const auto value = reinterpret_cast<uintptr_t>(handle);
    if (value > 0 && value <= 64) {
        *color = systemColor(static_cast<int>(value) - 1);
        return true;
    }
    GdiObject* o = gdiObject(handle);
    if (!o || o->type != GdiObject::Type::Brush || o->hollow)
        return false;
    *color = qcolor(o->stockIndex == DC_BRUSH && dc ? dc->dcBrushColor : o->color);
    return true;
}

QPen currentPen(DeviceContext* dc)
{
    GdiObject* o = gdiObject(dc->pen ? dc->pen : GetStockObject(BLACK_PEN));
    if (!o || o->type != GdiObject::Type::Pen || o->hollow || o->penStyle == PS_NULL)
        return QPen(Qt::NoPen);
    QPen pen(qcolor(o->stockIndex == DC_PEN ? dc->dcPenColor : o->color));
    pen.setWidth(std::max(1, o->penWidth));
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    switch (o->penStyle) {
    case PS_DASH: pen.setStyle(Qt::DashLine); break;
    case PS_DOT: pen.setStyle(Qt::DotLine); break;
    case PS_DASHDOT: pen.setStyle(Qt::DashDotLine); break;
    case PS_DASHDOTDOT: pen.setStyle(Qt::DashDotDotLine); break;
    default: break;
    }
    return pen;
}

QBrush currentBrush(DeviceContext* dc)
{
    QColor color;
    return brushColor(dc, dc->brush ? dc->brush : GetStockObject(WHITE_BRUSH), &color) ? QBrush(color) : QBrush(Qt::NoBrush);
}

QRect fromRect(const RECT& r)
{
    return QRect(static_cast<int>(r.left), static_cast<int>(r.top), static_cast<int>(r.right - r.left),
                 static_cast<int>(r.bottom - r.top));
}

QFont dcFont(DeviceContext* dc)
{
    if (dc->font)
        return fontOf(dc->font);
    return dc->widget ? dc->widget->font() : QApplication::font();
}

// Una figura con la pluma y el pincel del DC: el contorno queda dentro de [left, right)
template<typename Draw>
BOOL drawShape(HDC handle, int left, int top, int right, int bottom, Draw draw)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return FALSE;
    if (QPainter* p = dc->painter) {
        p->setBrush(currentBrush(dc));
        p->setPen(currentPen(dc));
        // Con una pluma de 1 px, drawRect(x, y, w - 1, h - 1) pinta de x a x + w - 1, como
        // Rectangle(x, y, x + w, y + h) de GDI; sin pluma rellena un píxel menos, como GDI
        draw(p, QRect(left, top, right - left - 1, bottom - top - 1));
    }
    return TRUE;
}

void drawTextAt(DeviceContext* dc, int x, int y, const QString& text)
{
    QPainter* p = dc->painter;
    if (!p)
        return;
    const QFont font = dcFont(dc);
    const QFontMetrics metrics(font);
    const int width = metrics.horizontalAdvance(text);
    int left = x;
    if ((dc->textAlign & TA_CENTER) == TA_CENTER)
        left = x - width / 2;
    else if (dc->textAlign & TA_RIGHT)
        left = x - width;
    int top = y;
    if ((dc->textAlign & TA_BASELINE) == TA_BASELINE)
        top = y - metrics.ascent();
    else if (dc->textAlign & TA_BOTTOM)
        top = y - metrics.height();
    if (dc->backgroundMode == OPAQUE)
        p->fillRect(QRect(left, top, width, metrics.height()), qcolor(dc->backgroundColor));
    p->setFont(font);
    p->setPen(qcolor(dc->textColor));
    p->drawText(QPoint(left, top + metrics.ascent()), text);
}

int drawTextIn(DeviceContext* dc, QString text, RECT* rect, UINT format)
{
    if (!rect)
        return 0;
    const QFont font = dcFont(dc);
    const QFontMetrics metrics(font);
    int flags = 0;
    if (format & DT_CENTER)
        flags |= Qt::AlignHCenter;
    else if (format & DT_RIGHT)
        flags |= Qt::AlignRight;
    else
        flags |= Qt::AlignLeft;
    if (format & DT_SINGLELINE) {
        flags |= Qt::TextSingleLine;
        flags |= (format & DT_VCENTER) ? Qt::AlignVCenter : (format & DT_BOTTOM) ? Qt::AlignBottom : Qt::AlignTop;
    } else {
        flags |= Qt::AlignTop;
    }
    const bool wrap = format & DT_WORDBREAK;
    if (wrap)
        flags |= Qt::TextWordWrap;
    if (format & DT_EXPANDTABS)
        flags |= Qt::TextExpandTabs;
    if (!(format & DT_NOPREFIX))
        flags |= (format & DT_HIDEPREFIX) ? Qt::TextHideMnemonic : Qt::TextShowMnemonic;
    const QRect box = fromRect(*rect);
    if (format & (DT_END_ELLIPSIS | DT_PATH_ELLIPSIS | DT_WORD_ELLIPSIS)) {
        const Qt::TextElideMode mode = (format & DT_PATH_ELLIPSIS) ? Qt::ElideMiddle : Qt::ElideRight;
        text = metrics.elidedText(text, mode, std::max(0, box.width()), flags & Qt::TextShowMnemonic);
    }
    const QRect needed = metrics.boundingRect(QRect(box.left(), box.top(), wrap ? std::max(1, box.width()) : 100000, 100000),
                                              flags & ~(static_cast<int>(Qt::AlignHCenter) | static_cast<int>(Qt::AlignRight)
                                                        | static_cast<int>(Qt::AlignVCenter) | static_cast<int>(Qt::AlignBottom)),
                                              text);
    if (format & DT_CALCRECT) {
        rect->right = rect->left + needed.width();
        rect->bottom = rect->top + needed.height();
        return needed.height();
    }
    if (QPainter* p = dc->painter) {
        p->save();
        if (!(format & DT_NOCLIP))
            p->setClipRect(box, Qt::IntersectClip);
        if (dc->backgroundMode == OPAQUE)
            p->fillRect(metrics.boundingRect(box, flags, text), qcolor(dc->backgroundColor));
        p->setFont(font);
        p->setPen(qcolor(dc->textColor));
        p->drawText(box, flags, text);
        p->restore();
    }
    return needed.height();
}

QString textArg(LPCSTR text, int length) { return text ? QString::fromUtf8(text, length < 0 ? -1 : length) : QString(); }
QString textArg(LPCWSTR text, int length)
{
    if (!text)
        return {};
    return length < 0 ? QString::fromWCharArray(text) : QString::fromWCharArray(text, length);
}

template<typename Ch>
BOOL extTextOut(HDC handle, int x, int y, UINT options, const RECT* rect, const Ch* text, UINT length)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return FALSE;
    QPainter* p = dc->painter;
    if (p && rect && (options & ETO_OPAQUE))
        p->fillRect(fromRect(*rect), qcolor(dc->backgroundColor));
    if (p && text && length) {
        p->save();
        if (rect && (options & ETO_CLIPPED))
            p->setClipRect(fromRect(*rect), Qt::IntersectClip);
        const int mode = dc->backgroundMode;
        if (rect && (options & ETO_OPAQUE))
            dc->backgroundMode = TRANSPARENT; // el fondo ya está
        drawTextAt(dc, x, y, textArg(text, static_cast<int>(length)));
        dc->backgroundMode = mode;
        p->restore();
    }
    return TRUE;
}

// La ventana con el WM_PAINT en curso (BeginPaint) y su DC
DeviceContext* paintContext(HWND hwnd)
{
    Window* w = windowOf(hwnd);
    return w ? w->paintDc : nullptr;
}

// --- La cola de mensajes del hilo (fase 58) ------------------------------------------------------

std::deque<MSG>& threadQueue()
{
    static auto* queue = new std::deque<MSG>;
    return *queue;
}

void postThreadMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    MSG msg{};
    msg.message = message;
    msg.wParam = wParam;
    msg.lParam = lParam;
    msg.time = GetTickCount();
    threadQueue().push_back(msg);
    // Despierta a quien espera en GetMessage o WaitMessage
    if (qApp)
        QMetaObject::invokeMethod(qApp, [] {}, Qt::QueuedConnection);
}

bool matchesFilter(const MSG& msg, HWND hwnd, UINT filterMin, UINT filterMax)
{
    if (msg.message == WM_QUIT)
        return true;
    if (hwnd && hwnd != reinterpret_cast<HWND>(-1) && msg.hwnd != hwnd)
        return false;
    if (hwnd == reinterpret_cast<HWND>(-1) && msg.hwnd)
        return false;
    return (filterMin == 0 && filterMax == 0) || (msg.message >= filterMin && msg.message <= filterMax);
}

bool takeMessage(LPMSG out, HWND hwnd, UINT filterMin, UINT filterMax, bool remove)
{
    auto& queue = threadQueue();
    for (auto it = queue.begin(); it != queue.end(); ++it) {
        if (!matchesFilter(*it, hwnd, filterMin, filterMax))
            continue;
        if (out)
            *out = *it;
        if (remove)
            queue.erase(it);
        return true;
    }
    return false;
}


// El repintado de Qt es el WM_PAINT de Windows: primero WM_ERASEBKGND (DefWindowProc rellena
// con el pincel de la clase) y después WM_PAINT, con el QPainter del widget en el DC que
// devuelve BeginPaint
void HostWidget::paintEvent(QPaintEvent* event)
{
    Window* w = window;
    if (!w || !w->alive || !w->created) {
        QWidget::paintEvent(event);
        return;
    }
    QPainter painter(this);
    auto* dc = new DeviceContext;
    dc->widget = this;
    dc->painter = &painter;
    dc->font = w->font;
    liveDcs().insert(dc);
    DeviceContext* previous = w->paintDc;
    const QRect previousRect = w->paintRect;
    w->paintDc = dc;
    w->paintRect = event->rect();
    const HWND hwnd = hwndOf(w);
    send(w, WM_ERASEBKGND, reinterpret_cast<WPARAM>(dc), 0);
    if (windowOf(hwnd))
        send(w, WM_PAINT, 0, 0);
    liveDcs().erase(dc);
    dc->magic = 0;
    delete dc;
    if (windowOf(hwnd)) {
        w->paintDc = previous;
        w->paintRect = previousRect;
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

static ATOM registerClass(const QString& name, WNDPROC proc, int extraBytes, UINT style, HBRUSH background = nullptr)
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
    c.background = background;
    c.atom = static_cast<ATOM>(0xC000 + classes.size());
    classes.push_back(c);
    return c.atom;
}

ATOM RegisterClassA(const WNDCLASSA* wc) { return wc ? registerClass(resourceName(wc->lpszClassName, fromA), wc->lpfnWndProc, wc->cbWndExtra, wc->style, wc->hbrBackground) : 0; }
ATOM RegisterClassW(const WNDCLASSW* wc) { return wc ? registerClass(resourceName(wc->lpszClassName, fromW), wc->lpfnWndProc, wc->cbWndExtra, wc->style, wc->hbrBackground) : 0; }
ATOM RegisterClassExA(const WNDCLASSEXA* wc) { return wc ? registerClass(resourceName(wc->lpszClassName, fromA), wc->lpfnWndProc, wc->cbWndExtra, wc->style, wc->hbrBackground) : 0; }
ATOM RegisterClassExW(const WNDCLASSEXW* wc) { return wc ? registerClass(resourceName(wc->lpszClassName, fromW), wc->lpfnWndProc, wc->cbWndExtra, wc->style, wc->hbrBackground) : 0; }

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
    case WM_ERASEBKGND: {
        // El pincel de la clase (hbrBackground); sin él, se ve el fondo del widget
        const WindowClass* c = findClass(w->className);
        if (!c || !c->background || !w->widget)
            return 0;
        RECT rc{0, 0, w->widget->width(), w->widget->height()};
        FillRect(reinterpret_cast<HDC>(wParam), &rc, c->background);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
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

// El nombre del recurso: MAKEINTRESOURCE(n) o una cadena, de la variante A o W (los
// diálogos de la plantilla de N++ llaman a CreateDialogParam con un TCHAR*)
HWND edita_compat_CreateDialogParam(HINSTANCE instance, const void* name, HWND parent, void* proc, LPARAM init)
{
    if (const EditaResource* res = findResource(5, resourceArgument(static_cast<const TCHAR*>(name))))
        return createDialogFromTemplate(instance, parseTemplate(res->data), parent, reinterpret_cast<DLGPROC>(proc), init, false);
    return createDialog(instance, parent, reinterpret_cast<DLGPROC>(proc), init);
}

INT_PTR edita_compat_DialogBoxParam(HINSTANCE instance, const void* name, HWND parent, void* proc, LPARAM init)
{
    if (const EditaResource* res = findResource(5, resourceArgument(static_cast<const TCHAR*>(name)))) {
        HWND hwnd = createDialogFromTemplate(instance, parseTemplate(res->data), parent, reinterpret_cast<DLGPROC>(proc), init, true);
        return hwnd ? runModal(hwnd) : -1;
    }
    // Sin plantilla, un diálogo modal vacío no sirve de nada: se avisa
    showMissingDialog();
    return IDCANCEL;
}

HWND CreateDialogIndirectParamA(HINSTANCE instance, LPCDLGTEMPLATE dialogTemplate, HWND parent, DLGPROC proc, LPARAM init)
{
    return dialogTemplate ? createDialogFromTemplate(instance, parseTemplate(dialogTemplate), parent, proc, init, false)
                          : createDialog(instance, parent, proc, init);
}

HWND CreateDialogIndirectParamW(HINSTANCE instance, LPCDLGTEMPLATE dialogTemplate, HWND parent, DLGPROC proc, LPARAM init)
{
    return CreateDialogIndirectParamA(instance, dialogTemplate, parent, proc, init);
}

INT_PTR DialogBoxIndirectParamA(HINSTANCE instance, LPCDLGTEMPLATE dialogTemplate, HWND parent, DLGPROC proc, LPARAM init)
{
    if (!dialogTemplate)
        return -1;
    HWND hwnd = createDialogFromTemplate(instance, parseTemplate(dialogTemplate), parent, proc, init, true);
    return hwnd ? runModal(hwnd) : -1;
}

INT_PTR DialogBoxIndirectParamW(HINSTANCE instance, LPCDLGTEMPLATE dialogTemplate, HWND parent, DLGPROC proc, LPARAM init)
{
    return DialogBoxIndirectParamA(instance, dialogTemplate, parent, proc, init);
}

BOOL MapDialogRect(HWND dialog, LPRECT rect)
{
    Window* w = windowOf(dialog);
    if (!w || !rect)
        return FALSE;
    const QSize base = w->baseUnits;
    rect->left = MulDiv(static_cast<int>(rect->left), base.width(), 4);
    rect->right = MulDiv(static_cast<int>(rect->right), base.width(), 4);
    rect->top = MulDiv(static_cast<int>(rect->top), base.height(), 8);
    rect->bottom = MulDiv(static_cast<int>(rect->bottom), base.height(), 8);
    return TRUE;
}

LONG GetDialogBaseUnits(void)
{
    const QSize base = dialogBaseUnits(QApplication::font());
    return MAKELONG(static_cast<WORD>(base.width()), static_cast<WORD>(base.height()));
}

BOOL EndDialog(HWND hwnd, INT_PTR result)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return FALSE;
    w->modalResult = result;
    w->ended = true;
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

// WM_PAINT ya: el repintado pendiente, si la ventana se ve y no está en su propio WM_PAINT
BOOL UpdateWindow(HWND hwnd)
{
    QWidget* widget = widgetOf(hwnd);
    if (!widget)
        return FALSE;
    Window* w = windowOf(hwnd);
    if (widget->isVisible() && !(w && w->paintDc))
        widget->repaint();
    else
        widget->update();
    return TRUE;
}

BOOL InvalidateRect(HWND hwnd, const RECT* rect, BOOL)
{
    QWidget* widget = widgetOf(hwnd);
    if (!widget)
        return hwnd ? FALSE : TRUE;
    if (rect)
        widget->update(fromRect(*rect));
    else
        widget->update();
    return TRUE;
}

BOOL RedrawWindow(HWND hwnd, const RECT* rect, void*, UINT flags)
{
    InvalidateRect(hwnd, rect, TRUE);
    if (flags & 0x0100) // RDW_UPDATENOW
        UpdateWindow(hwnd);
    return TRUE;
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

void edita_compat_setResources(const EditaResource* resources, unsigned long count)
{
    registry().resources = resources;
    registry().resourceCount = count;
}

static HRSRC findResourceByName(const QString& name, const QString& type)
{
    // Los tipos de serie son números (RT_DIALOG = #5); los propios, cadenas: no los hay
    const unsigned typeId = type.startsWith(QLatin1Char('#')) ? type.mid(1).toUInt() : 0;
    const EditaResource* res = typeId ? findResource(typeId, name) : nullptr;
    if (!res)
        SetLastError(1814); // ERROR_RESOURCE_NAME_NOT_FOUND
    return const_cast<EditaResource*>(res);
}

HRSRC FindResourceA(HMODULE, LPCSTR name, LPCSTR type) { return findResourceByName(resourceArgument(name), resourceArgument(type)); }
HRSRC FindResourceW(HMODULE, LPCWSTR name, LPCWSTR type) { return findResourceByName(resourceArgument(name), resourceArgument(type)); }

// Como en Windows: LoadResource da un handle y LockResource, los datos (de solo lectura)
HGLOBAL LoadResource(HMODULE, HRSRC resource) { return isResource(resource) ? resource : nullptr; }
LPVOID LockResource(HGLOBAL resource)
{
    if (isResource(resource))
        return const_cast<unsigned char*>(static_cast<const EditaResource*>(resource)->data);
    return resource;
}
DWORD SizeofResource(HMODULE, HRSRC resource)
{
    return isResource(resource) ? static_cast<DWORD>(static_cast<const EditaResource*>(resource)->size) : 0;
}
BOOL FreeResource(HGLOBAL) { return FALSE; }

int LoadStringA(HINSTANCE, UINT id, LPSTR buffer, int size)
{
    const EditaResource* res = findResource(6, QStringLiteral("#%1").arg(id));
    if (!buffer || size <= 0)
        return 0;
    if (!res) {
        buffer[0] = '\0';
        return 0;
    }
    return copyTo(std::string(reinterpret_cast<const char*>(res->data), res->size), buffer, size);
}

int LoadStringW(HINSTANCE, UINT id, LPWSTR buffer, int size)
{
    const EditaResource* res = findResource(6, QStringLiteral("#%1").arg(id));
    if (!buffer)
        return 0;
    if (!res) {
        if (size > 0)
            buffer[0] = L'\0';
        return 0;
    }
    std::wstring& text = registry().wideStrings[id];
    text = QString::fromUtf8(reinterpret_cast<const char*>(res->data), static_cast<qsizetype>(res->size)).toStdWString();
    if (size == 0) {
        // Como Windows: un puntero (de solo lectura) a la cadena del recurso
        *reinterpret_cast<const wchar_t**>(buffer) = text.c_str();
        return static_cast<int>(text.size());
    }
    return copyTo(text, buffer, size);
}

HMENU LoadMenuIndirectW(const void* menuTemplate)
{
    if (!menuTemplate)
        return nullptr;
    TemplateReader r(static_cast<const unsigned char*>(menuTemplate));
    const unsigned version = r.word();
    const unsigned offset = r.word();
    if (version != 0) // MENUEX: edita_rc escribe siempre la plantilla clásica
        return nullptr;
    r.skip(offset);
    HMENU menu = CreateMenu();
    buildMenu(r, menu);
    return menu;
}

HMENU LoadMenuIndirectA(const void* menuTemplate) { return LoadMenuIndirectW(menuTemplate); }

HMENU LoadMenuA(HINSTANCE, LPCSTR name)
{
    const EditaResource* res = findResource(4, resourceArgument(name));
    return res ? LoadMenuIndirectW(res->data) : nullptr;
}

HMENU LoadMenuW(HINSTANCE, LPCWSTR name)
{
    const EditaResource* res = findResource(4, resourceArgument(name));
    return res ? LoadMenuIndirectW(res->data) : nullptr;
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
    liveDcs().insert(dc);
    return dc;
}

HDC GetWindowDC(HWND hwnd) { return GetDC(hwnd); }

int ReleaseDC(HWND, HDC handle)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc || (dc->painter && !dc->ownsPainter && !dc->memory)) // el de WM_PAINT es de EndPaint
        return 0;
    liveDcs().erase(dc);
    if (dc->ownsPainter) {
        dc->painter->end();
        delete dc->painter;
    }
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
    HGDIOBJ previous = nullptr;
    switch (o->type) {
    case GdiObject::Type::Font:
        previous = dc->font ? dc->font : GetStockObject(SYSTEM_FONT);
        dc->font = object;
        break;
    case GdiObject::Type::Brush:
        previous = dc->brush ? dc->brush : GetStockObject(WHITE_BRUSH);
        dc->brush = object;
        break;
    case GdiObject::Type::Pen:
        previous = dc->pen ? dc->pen : GetStockObject(BLACK_PEN);
        dc->pen = object;
        break;
    case GdiObject::Type::Bitmap:
        // En un DC en memoria: se dibuja en su imagen
        if (!dc->memory)
            return nullptr;
        previous = dc->bitmap;
        if (dc->ownsPainter) {
            dc->painter->end();
            delete dc->painter;
            dc->painter = nullptr;
            dc->ownsPainter = false;
        }
        dc->bitmap = object;
        if (!o->stock) {
            dc->painter = new QPainter(&o->image);
            dc->ownsPainter = true;
        }
        break;
    }
    return previous;
}

BOOL DeleteObject(HGDIOBJ object)
{
    GdiObject* o = gdiObject(object);
    if (!o)
        return FALSE;
    // Un mapa de bits seleccionado en un DC no se borra (como en Windows)
    if (o->type == GdiObject::Type::Bitmap) {
        for (DeviceContext* dc : liveDcs()) {
            if (dc->bitmap == object)
                return FALSE;
        }
    }
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
        o->stockIndex = index;
        o->hollow = index == NULL_BRUSH || index == NULL_PEN;
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
    if (o->type == GdiObject::Type::Bitmap) {
        const int needed = static_cast<int>(sizeof(BITMAP));
        if (!buffer)
            return needed;
        if (size < needed)
            return 0;
        auto* bm = static_cast<BITMAP*>(buffer);
        *bm = BITMAP{};
        bm->bmWidth = o->image.width();
        bm->bmHeight = o->image.height();
        bm->bmWidthBytes = static_cast<LONG>(o->image.bytesPerLine());
        bm->bmPlanes = 1;
        bm->bmBitsPixel = 32;
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

HPEN CreatePen(int style, int width, COLORREF color)
{
    auto* o = new GdiObject;
    o->type = GdiObject::Type::Pen;
    o->color = color;
    o->penStyle = style & 0xF;
    o->penWidth = width;
    o->hollow = (style & 0xF) == PS_NULL;
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

// --- comctl32 ----------------------------------------------------------------------------------------

BOOL SetWindowSubclass(HWND hwnd, SUBCLASSPROC proc, UINT_PTR id, DWORD_PTR refData)
{
    Window* w = windowOf(hwnd);
    if (!w || !proc)
        return FALSE;
    for (Window::Subclass& existing : w->subclasses) {
        if (existing.proc == proc && existing.id == id) {
            existing.refData = refData;
            return TRUE;
        }
    }
    if (w->subclasses.empty() || w->proc != subclassDispatch) {
        w->subclassBase = w->proc;
        w->proc = subclassDispatch;
    }
    w->subclasses.push_back({proc, id, refData});
    return TRUE;
}

BOOL GetWindowSubclass(HWND hwnd, SUBCLASSPROC proc, UINT_PTR id, DWORD_PTR* refData)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return FALSE;
    for (const Window::Subclass& existing : w->subclasses) {
        if (existing.proc == proc && existing.id == id) {
            if (refData)
                *refData = existing.refData;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL RemoveWindowSubclass(HWND hwnd, SUBCLASSPROC proc, UINT_PTR id)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return FALSE;
    auto& list = w->subclasses;
    const auto it = std::find_if(list.begin(), list.end(), [&](const Window::Subclass& s) { return s.proc == proc && s.id == id; });
    if (it == list.end())
        return FALSE;
    list.erase(it);
    if (list.empty() && w->proc == subclassDispatch)
        w->proc = w->subclassBase;
    return TRUE;
}

LRESULT DefSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return 0;
    for (auto it = t_subclassCalls.rbegin(); it != t_subclassCalls.rend(); ++it) {
        if (it->first == w)
            return callSubclass(w, it->second - 1, hwnd, message, wParam, lParam);
    }
    return CallWindowProcW(w->subclassBase ? w->subclassBase : w->builtin, hwnd, message, wParam, lParam);
}

void InitCommonControls(void) {}
BOOL InitCommonControlsEx(const INITCOMMONCONTROLSEX*) { return TRUE; }

// --- Diálogos comunes ----------------------------------------------------------------------------------

BOOL GetOpenFileNameA(LPOPENFILENAMEA ofn) { return fileDialog(ofn, false); }
BOOL GetOpenFileNameW(LPOPENFILENAMEW ofn) { return fileDialog(ofn, false); }
BOOL GetSaveFileNameA(LPOPENFILENAMEA ofn) { return fileDialog(ofn, true); }
BOOL GetSaveFileNameW(LPOPENFILENAMEW ofn) { return fileDialog(ofn, true); }
DWORD CommDlgExtendedError(void) { return g_commonDialogError; }
BOOL ChooseColorA(LPCHOOSECOLORA cc) { return chooseColor(cc); }
BOOL ChooseColorW(LPCHOOSECOLORW cc) { return chooseColor(cc); }
PIDLIST_ABSOLUTE SHBrowseForFolderA(LPBROWSEINFOA bi) { return browseForFolder(bi); }
PIDLIST_ABSOLUTE SHBrowseForFolderW(LPBROWSEINFOW bi) { return browseForFolder(bi); }

BOOL SHGetPathFromIDListA(PCIDLIST_ABSOLUTE pidl, LPSTR path)
{
    const QString folder = pidlPath(pidl);
    if (folder.isEmpty() || !path)
        return FALSE;
    copyTo(folder.toStdString(), path, MAX_PATH);
    return TRUE;
}

BOOL SHGetPathFromIDListW(PCIDLIST_ABSOLUTE pidl, LPWSTR path)
{
    const QString folder = pidlPath(pidl);
    if (folder.isEmpty() || !path)
        return FALSE;
    copyTo(folder.toStdWString(), path, MAX_PATH);
    return TRUE;
}

void ILFree(LPITEMIDLIST pidl) { CoTaskMemFree(pidl); }

// --- MENUITEMINFO ---------------------------------------------------------------------------------------

// --- Bucles de mensajes (fase 58) -------------------------------------------------------------------
//
// Lo que llega a una ventana por PostMessage se entrega mientras el plugin espera aquí (el
// bucle de eventos de Qt sigue funcionando). GetMessage devuelve los mensajes de la cola del
// hilo (PostQuitMessage, PostThreadMessage) y, cada vez que Qt atiende algo, un WM_NULL: así
// un bucle que espera a que el plugin cambie una variable («while (!done && GetMessage…)»)
// la vuelve a mirar.

static BOOL getMessage(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax)
{
    if (!msg)
        return -1;
    if (takeMessage(msg, hwnd, filterMin, filterMax, true))
        return msg->message != WM_QUIT;
    if (!qApp) {
        *msg = MSG{};
        msg->message = WM_QUIT;
        return FALSE;
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents);
    if (takeMessage(msg, hwnd, filterMin, filterMax, true))
        return msg->message != WM_QUIT;
    *msg = MSG{};
    msg->message = WM_NULL;
    msg->time = GetTickCount();
    return TRUE;
}

BOOL GetMessageA(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax) { return getMessage(msg, hwnd, filterMin, filterMax); }
BOOL GetMessageW(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax) { return getMessage(msg, hwnd, filterMin, filterMax); }

static BOOL peekMessage(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax, UINT remove)
{
    if (qApp)
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    return takeMessage(msg, hwnd, filterMin, filterMax, remove & PM_REMOVE) ? TRUE : FALSE;
}

BOOL PeekMessageA(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax, UINT remove) { return peekMessage(msg, hwnd, filterMin, filterMax, remove); }
BOOL PeekMessageW(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax, UINT remove) { return peekMessage(msg, hwnd, filterMin, filterMax, remove); }

LRESULT DispatchMessageW(const MSG* msg)
{
    if (!msg || !msg->hwnd || msg->message == WM_NULL)
        return 0;
    if (msg->message == WM_TIMER && msg->lParam) {
        reinterpret_cast<TIMERPROC>(msg->lParam)(msg->hwnd, WM_TIMER, msg->wParam, msg->time);
        return 0;
    }
    return SendMessage(msg->hwnd, msg->message, msg->wParam, msg->lParam);
}

LRESULT DispatchMessageA(const MSG* msg) { return DispatchMessageW(msg); }

// El teclado llega ya traducido (WM_CHAR) a los procedimientos de la capa
BOOL TranslateMessage(const MSG*) { return FALSE; }

void PostQuitMessage(int exitCode)
{
    postThreadMessage(WM_QUIT, static_cast<WPARAM>(exitCode), 0);
}

static BOOL postThread(DWORD threadId, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (threadId != 0 && threadId != GetCurrentThreadId() && !qApp)
        return FALSE;
    // Desde otro hilo, a la cola del hilo de la interfaz
    QMetaObject::invokeMethod(qApp, [message, wParam, lParam] { postThreadMessage(message, wParam, lParam); },
                              Qt::QueuedConnection);
    return TRUE;
}

BOOL PostThreadMessageA(DWORD threadId, UINT message, WPARAM wParam, LPARAM lParam) { return postThread(threadId, message, wParam, lParam); }
BOOL PostThreadMessageW(DWORD threadId, UINT message, WPARAM wParam, LPARAM lParam) { return postThread(threadId, message, wParam, lParam); }

BOOL WaitMessage(void)
{
    if (qApp && threadQueue().empty())
        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents);
    return TRUE;
}

DWORD GetQueueStatus(UINT)
{
    return threadQueue().empty() ? 0 : MAKELONG(0x0008, 0x0008); // QS_POSTMESSAGE
}

DWORD MsgWaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL waitAll, DWORD milliseconds, DWORD)
{
    // Los objetos o un mensaje en la cola, atendiendo la interfaz mientras tanto
    QElapsedTimer timer;
    timer.start();
    for (;;) {
        if (count > 0) {
            const DWORD result = WaitForMultipleObjects(count, handles, waitAll, 0);
            if (result != WAIT_TIMEOUT)
                return result;
        }
        if (!threadQueue().empty())
            return WAIT_OBJECT_0 + count;
        if (milliseconds != INFINITE && timer.elapsed() >= static_cast<qint64>(milliseconds))
            return WAIT_TIMEOUT;
        if (qApp)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
}

// --- Dibujo (fase 58) ---------------------------------------------------------------------------------

HDC BeginPaint(HWND hwnd, LPPAINTSTRUCT paint)
{
    Window* w = windowOf(hwnd);
    if (!w)
        return nullptr;
    DeviceContext* dc = paintContext(hwnd);
    if (!dc) {
        // Fuera de un WM_PAINT: un DC que no dibuja, y se pide repintar
        dc = static_cast<DeviceContext*>(GetDC(hwnd));
        if (w->widget)
            w->widget->update();
    }
    if (paint) {
        *paint = PAINTSTRUCT{};
        paint->hdc = dc;
        const QRect r = w->paintDc ? w->paintRect : (w->widget ? w->widget->rect() : QRect());
        paint->rcPaint = RECT{r.left(), r.top(), r.right() + 1, r.bottom() + 1};
    }
    return dc;
}

BOOL EndPaint(HWND hwnd, const PAINTSTRUCT* paint)
{
    // El DC de WM_PAINT es de la capa; el que dio BeginPaint fuera de él, se libera
    if (paint && paint->hdc && paint->hdc != paintContext(hwnd))
        ReleaseDC(hwnd, paint->hdc);
    return TRUE;
}

BOOL ValidateRect(HWND, const RECT*) { return TRUE; }

BOOL GetUpdateRect(HWND hwnd, LPRECT rect, BOOL)
{
    Window* w = windowOf(hwnd);
    if (!w || !w->paintDc) {
        if (rect)
            *rect = RECT{};
        return FALSE;
    }
    if (rect)
        *rect = RECT{w->paintRect.left(), w->paintRect.top(), w->paintRect.right() + 1, w->paintRect.bottom() + 1};
    return TRUE;
}

int FillRect(HDC handle, const RECT* rect, HBRUSH brush)
{
    DeviceContext* dc = deviceContext(handle);
    QColor color;
    if (!dc || !rect || !brushColor(dc, brush, &color))
        return 0;
    if (dc->painter)
        dc->painter->fillRect(fromRect(*rect), color);
    return 1;
}

int FrameRect(HDC handle, const RECT* rect, HBRUSH brush)
{
    DeviceContext* dc = deviceContext(handle);
    QColor color;
    if (!dc || !rect || !brushColor(dc, brush, &color))
        return 0;
    if (QPainter* p = dc->painter) {
        const QRect r = fromRect(*rect);
        p->fillRect(QRect(r.left(), r.top(), r.width(), 1), color);
        p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), color);
        p->fillRect(QRect(r.left(), r.top(), 1, r.height()), color);
        p->fillRect(QRect(r.right(), r.top(), 1, r.height()), color);
    }
    return 1;
}

BOOL InvertRect(HDC handle, const RECT* rect)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc || !rect)
        return FALSE;
    if (QPainter* p = dc->painter) {
        p->save();
        p->setCompositionMode(QPainter::RasterOp_NotDestination);
        p->fillRect(fromRect(*rect), Qt::black);
        p->restore();
    }
    return TRUE;
}

BOOL DrawFocusRect(HDC handle, const RECT* rect)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc || !rect)
        return FALSE;
    if (QPainter* p = dc->painter) {
        p->save();
        QPen pen(qcolor(dc->textColor), 1, Qt::DotLine);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawRect(fromRect(*rect).adjusted(0, 0, -1, -1));
        p->restore();
    }
    return TRUE;
}

BOOL DrawEdge(HDC handle, LPRECT rect, UINT edge, UINT flags)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc || !rect)
        return FALSE;
    const QPalette palette = dc->widget ? dc->widget->palette() : QApplication::palette();
    const bool raised = edge & (BDR_RAISEDOUTER | BDR_RAISEDINNER);
    const int depth = ((edge & (BDR_RAISEDOUTER | BDR_SUNKENOUTER)) ? 1 : 0) + ((edge & (BDR_RAISEDINNER | BDR_SUNKENINNER)) ? 1 : 0);
    if (QPainter* p = dc->painter) {
        QRect r = fromRect(*rect);
        for (int i = 0; i < depth; ++i) {
            const QColor topLeft = raised ? palette.color(QPalette::Light) : palette.color(QPalette::Dark);
            const QColor bottomRight = raised ? palette.color(QPalette::Dark) : palette.color(QPalette::Light);
            if (flags & BF_TOP)
                p->fillRect(QRect(r.left(), r.top(), r.width(), 1), topLeft);
            if (flags & BF_LEFT)
                p->fillRect(QRect(r.left(), r.top(), 1, r.height()), topLeft);
            if (flags & BF_BOTTOM)
                p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), bottomRight);
            if (flags & BF_RIGHT)
                p->fillRect(QRect(r.right(), r.top(), 1, r.height()), bottomRight);
            r.adjust(1, 1, -1, -1);
        }
    }
    if (flags & BF_ADJUST) {
        if (flags & BF_LEFT)
            rect->left += depth;
        if (flags & BF_TOP)
            rect->top += depth;
        if (flags & BF_RIGHT)
            rect->right -= depth;
        if (flags & BF_BOTTOM)
            rect->bottom -= depth;
    }
    return TRUE;
}

BOOL Rectangle(HDC dc, int left, int top, int right, int bottom)
{
    return drawShape(dc, left, top, right, bottom, [](QPainter* p, const QRect& r) { p->drawRect(r); });
}

BOOL RoundRect(HDC dc, int left, int top, int right, int bottom, int width, int height)
{
    return drawShape(dc, left, top, right, bottom,
                     [width, height](QPainter* p, const QRect& r) { p->drawRoundedRect(r, width / 2.0, height / 2.0); });
}

BOOL Ellipse(HDC dc, int left, int top, int right, int bottom)
{
    return drawShape(dc, left, top, right, bottom, [](QPainter* p, const QRect& r) { p->drawEllipse(r); });
}

BOOL MoveToEx(HDC handle, int x, int y, LPPOINT previous)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return FALSE;
    if (previous)
        *previous = POINT{dc->position.x(), dc->position.y()};
    dc->position = QPoint(x, y);
    return TRUE;
}

BOOL LineTo(HDC handle, int x, int y)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return FALSE;
    if (QPainter* p = dc->painter) {
        // GDI no pinta el último punto; con FlatCap, Qt tampoco (currentPen)
        p->setPen(currentPen(dc));
        if (QPoint(x, y) != dc->position)
            p->drawLine(dc->position, QPoint(x, y));
    }
    dc->position = QPoint(x, y);
    return TRUE;
}

static QPolygon polygonOf(const POINT* points, int count)
{
    QPolygon polygon;
    for (int i = 0; i < count; ++i)
        polygon << QPoint(static_cast<int>(points[i].x), static_cast<int>(points[i].y));
    return polygon;
}

BOOL Polyline(HDC handle, const POINT* points, int count)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc || !points || count < 2)
        return FALSE;
    if (QPainter* p = dc->painter) {
        p->setPen(currentPen(dc));
        p->drawPolyline(polygonOf(points, count));
    }
    return TRUE;
}

BOOL Polygon(HDC handle, const POINT* points, int count)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc || !points || count < 2)
        return FALSE;
    if (QPainter* p = dc->painter) {
        p->setPen(currentPen(dc));
        p->setBrush(currentBrush(dc));
        p->drawPolygon(polygonOf(points, count));
    }
    return TRUE;
}

COLORREF SetPixel(HDC handle, int x, int y, COLORREF color)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return CLR_INVALID;
    if (dc->painter)
        dc->painter->fillRect(QRect(x, y, 1, 1), qcolor(color));
    return color;
}


BOOL TextOutA(HDC handle, int x, int y, LPCSTR text, int length)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return FALSE;
    drawTextAt(dc, x, y, textArg(text, length));
    return TRUE;
}

BOOL TextOutW(HDC handle, int x, int y, LPCWSTR text, int length)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return FALSE;
    drawTextAt(dc, x, y, textArg(text, length));
    return TRUE;
}

BOOL ExtTextOutA(HDC dc, int x, int y, UINT options, const RECT* rect, LPCSTR text, UINT length, const INT*)
{
    return extTextOut(dc, x, y, options, rect, text, length);
}

BOOL ExtTextOutW(HDC dc, int x, int y, UINT options, const RECT* rect, LPCWSTR text, UINT length, const INT*)
{
    return extTextOut(dc, x, y, options, rect, text, length);
}

int DrawTextA(HDC handle, LPCSTR text, int length, LPRECT rect, UINT format)
{
    DeviceContext* dc = deviceContext(handle);
    return dc ? drawTextIn(dc, textArg(text, length), rect, format) : 0;
}

int DrawTextW(HDC handle, LPCWSTR text, int length, LPRECT rect, UINT format)
{
    DeviceContext* dc = deviceContext(handle);
    return dc ? drawTextIn(dc, textArg(text, length), rect, format) : 0;
}

UINT SetTextAlign(HDC handle, UINT align)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return GDI_ERROR;
    const UINT previous = dc->textAlign;
    dc->textAlign = align;
    return previous;
}

HDC CreateCompatibleDC(HDC)
{
    // Como en Windows, con un mapa de bits de 1 × 1 seleccionado: es el que devuelve el
    // primer SelectObject, y volver a seleccionarlo suelta el del plugin
    static GdiObject* defaultBitmap = [] {
        auto* o = new GdiObject;
        o->type = GdiObject::Type::Bitmap;
        o->stock = true;
        o->image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
        o->image.fill(Qt::black);
        return o;
    }();
    auto* dc = new DeviceContext;
    dc->memory = true;
    dc->bitmap = defaultBitmap;
    liveDcs().insert(dc);
    return dc;
}

static HBITMAP newBitmap(int width, int height)
{
    auto* o = new GdiObject;
    o->type = GdiObject::Type::Bitmap;
    o->image = QImage(std::max(1, width), std::max(1, height), QImage::Format_ARGB32_Premultiplied);
    o->image.fill(Qt::black);
    return reinterpret_cast<HBITMAP>(o);
}

HBITMAP CreateCompatibleBitmap(HDC, int width, int height)
{
    return newBitmap(width, height);
}

HBITMAP CreateBitmap(int width, int height, UINT, UINT bitsPerPixel, const void* bits)
{
    HBITMAP bitmap = newBitmap(width, height);
    if (bits && bitsPerPixel == 32) {
        GdiObject* o = gdiObject(bitmap);
        for (int y = 0; y < o->image.height(); ++y)
            std::memcpy(o->image.scanLine(y), static_cast<const uchar*>(bits) + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4,
                        static_cast<std::size_t>(width) * 4);
    }
    return bitmap;
}

static QPainter::CompositionMode compositionOf(DWORD rop)
{
    switch (rop) {
    case SRCPAINT: return QPainter::RasterOp_SourceOrDestination;
    case SRCAND: return QPainter::RasterOp_SourceAndDestination;
    case SRCINVERT: return QPainter::RasterOp_SourceXorDestination;
    default: return QPainter::CompositionMode_SourceOver;
    }
}

BOOL StretchBlt(HDC destHandle, int x, int y, int width, int height, HDC sourceHandle, int sx, int sy, int sw, int sh, DWORD rop)
{
    DeviceContext* dest = deviceContext(destHandle);
    if (!dest)
        return FALSE;
    QPainter* p = dest->painter;
    if (!p)
        return TRUE;
    const QRect target(x, y, width, height);
    if (rop == BLACKNESS || rop == WHITENESS || rop == PATCOPY) {
        QColor color = rop == BLACKNESS ? QColor(Qt::black) : rop == WHITENESS ? QColor(Qt::white) : QColor();
        if (rop == PATCOPY && !brushColor(dest, dest->brush ? dest->brush : GetStockObject(WHITE_BRUSH), &color))
            return TRUE;
        p->fillRect(target, color);
        return TRUE;
    }
    DeviceContext* source = deviceContext(sourceHandle);
    GdiObject* bitmap = source ? gdiObject(source->bitmap) : nullptr;
    if (!bitmap || bitmap->type != GdiObject::Type::Bitmap)
        return FALSE;
    p->save();
    p->setCompositionMode(compositionOf(rop));
    p->drawImage(target, bitmap->image, QRect(sx, sy, sw, sh));
    p->restore();
    return TRUE;
}

BOOL BitBlt(HDC dest, int x, int y, int width, int height, HDC source, int sx, int sy, DWORD rop)
{
    return StretchBlt(dest, x, y, width, height, source, sx, sy, width, height, rop);
}

BOOL PatBlt(HDC dc, int x, int y, int width, int height, DWORD rop)
{
    return StretchBlt(dc, x, y, width, height, nullptr, 0, 0, 0, 0, rop == SRCCOPY ? PATCOPY : rop);
}

int SaveDC(HDC handle)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return 0;
    if (dc->painter)
        dc->painter->save();
    return ++dc->saved;
}

BOOL RestoreDC(HDC handle, int saved)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc || dc->saved == 0)
        return FALSE;
    const int target = saved < 0 ? dc->saved + saved : saved - 1;
    while (dc->saved > std::max(0, target)) {
        if (dc->painter)
            dc->painter->restore();
        --dc->saved;
    }
    return TRUE;
}

int IntersectClipRect(HDC handle, int left, int top, int right, int bottom)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return 0; // ERROR
    if (dc->painter)
        dc->painter->setClipRect(QRect(left, top, right - left, bottom - top), Qt::IntersectClip);
    return 2; // SIMPLEREGION
}

HBRUSH CreateBrushIndirect(const LOGBRUSH* brush)
{
    if (!brush)
        return nullptr;
    HBRUSH result = CreateSolidBrush(brush->lbColor);
    if (brush->lbStyle == BS_NULL)
        gdiObject(result)->hollow = true;
    return result;
}

COLORREF SetDCBrushColor(HDC handle, COLORREF color)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return CLR_INVALID;
    const COLORREF previous = dc->dcBrushColor;
    dc->dcBrushColor = color;
    return previous;
}

COLORREF SetDCPenColor(HDC handle, COLORREF color)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return CLR_INVALID;
    const COLORREF previous = dc->dcPenColor;
    dc->dcPenColor = color;
    return previous;
}

HGDIOBJ GetCurrentObject(HDC handle, UINT type)
{
    DeviceContext* dc = deviceContext(handle);
    if (!dc)
        return nullptr;
    switch (type) {
    case OBJ_PEN: return dc->pen ? dc->pen : GetStockObject(BLACK_PEN);
    case OBJ_BRUSH: return dc->brush ? dc->brush : GetStockObject(WHITE_BRUSH);
    case OBJ_FONT: return dc->font ? dc->font : GetStockObject(SYSTEM_FONT);
    case OBJ_BITMAP: return dc->bitmap;
    default: return nullptr;
    }
}

// --- RECT ------------------------------------------------------------------------------------------------

BOOL SetRect(LPRECT rect, int left, int top, int right, int bottom)
{
    if (!rect)
        return FALSE;
    *rect = RECT{left, top, right, bottom};
    return TRUE;
}

BOOL SetRectEmpty(LPRECT rect) { return SetRect(rect, 0, 0, 0, 0); }

BOOL CopyRect(LPRECT dest, const RECT* source)
{
    if (!dest || !source)
        return FALSE;
    *dest = *source;
    return TRUE;
}

BOOL IsRectEmpty(const RECT* rect) { return !rect || rect->right <= rect->left || rect->bottom <= rect->top; }

BOOL PtInRect(const RECT* rect, POINT point)
{
    return rect && point.x >= rect->left && point.x < rect->right && point.y >= rect->top && point.y < rect->bottom;
}

BOOL InflateRect(LPRECT rect, int dx, int dy)
{
    if (!rect)
        return FALSE;
    rect->left -= dx;
    rect->top -= dy;
    rect->right += dx;
    rect->bottom += dy;
    return TRUE;
}

BOOL OffsetRect(LPRECT rect, int dx, int dy)
{
    if (!rect)
        return FALSE;
    rect->left += dx;
    rect->right += dx;
    rect->top += dy;
    rect->bottom += dy;
    return TRUE;
}

BOOL IntersectRect(LPRECT dest, const RECT* a, const RECT* b)
{
    if (!dest || !a || !b)
        return FALSE;
    RECT r{std::max(a->left, b->left), std::max(a->top, b->top), std::min(a->right, b->right), std::min(a->bottom, b->bottom)};
    if (IsRectEmpty(&r)) {
        *dest = RECT{};
        return FALSE;
    }
    *dest = r;
    return TRUE;
}

BOOL UnionRect(LPRECT dest, const RECT* a, const RECT* b)
{
    if (!dest || !a || !b)
        return FALSE;
    if (IsRectEmpty(a)) {
        *dest = *b;
    } else if (IsRectEmpty(b)) {
        *dest = *a;
    } else {
        *dest = RECT{std::min(a->left, b->left), std::min(a->top, b->top), std::max(a->right, b->right), std::max(a->bottom, b->bottom)};
    }
    return !IsRectEmpty(dest);
}

BOOL SubtractRect(LPRECT dest, const RECT* a, const RECT* b)
{
    // Como en Windows: solo si b cubre a de lado a lado (si no, queda a)
    if (!dest || !a || !b)
        return FALSE;
    *dest = *a;
    if (b->top <= a->top && b->bottom >= a->bottom) {
        if (b->left <= a->left && b->right > a->left && b->right < a->right)
            dest->left = b->right;
        else if (b->right >= a->right && b->left > a->left && b->left < a->right)
            dest->right = b->left;
        else if (b->left <= a->left && b->right >= a->right)
            *dest = RECT{};
    } else if (b->left <= a->left && b->right >= a->right) {
        if (b->top <= a->top && b->bottom > a->top && b->bottom < a->bottom)
            dest->top = b->bottom;
        else if (b->bottom >= a->bottom && b->top > a->top && b->top < a->bottom)
            dest->bottom = b->top;
    }
    return !IsRectEmpty(dest);
}

BOOL EqualRect(const RECT* a, const RECT* b)
{
    return a && b && a->left == b->left && a->top == b->top && a->right == b->right && a->bottom == b->bottom;
}

} // extern "C"

namespace {

template<typename Info>
BOOL getMenuItemInfo(HMENU handle, UINT item, BOOL byPosition, Info* info)
{
    using Ch = std::remove_pointer_t<decltype(info->dwTypeData)>;
    Menu* m = menuOf(handle);
    QAction* action = m && !m->host ? findAction(m->menu, item, byPosition ? MF_BYPOSITION : MF_BYCOMMAND) : nullptr;
    if (!action || !info)
        return FALSE;
    if (info->fMask & (MIIM_FTYPE | MIIM_TYPE))
        info->fType = action->isSeparator() ? MFT_SEPARATOR : MFT_STRING;
    if (info->fMask & MIIM_STATE)
        info->fState = (action->isChecked() ? MFS_CHECKED : 0u) | (action->isEnabled() ? 0u : static_cast<UINT>(MFS_GRAYED));
    if (info->fMask & MIIM_ID)
        info->wID = action->menu() ? 0 : static_cast<UINT>(action->data().toULongLong());
    if (info->fMask & MIIM_SUBMENU) {
        info->hSubMenu = nullptr;
        if (action->menu()) {
            auto* sub = new Menu;
            sub->menu = action->menu();
            info->hSubMenu = sub;
        }
    }
    if (info->fMask & MIIM_DATA)
        info->dwItemData = static_cast<ULONG_PTR>(action->property("edita_itemData").toULongLong());
    if (info->fMask & (MIIM_STRING | MIIM_TYPE)) {
        const std::basic_string<Ch> text = toChars<Ch>(action->isSeparator() ? QString() : action->text());
        if (!info->dwTypeData || info->cch == 0) {
            info->cch = static_cast<UINT>(text.size());
        } else {
            info->cch = static_cast<UINT>(copyTo(text, info->dwTypeData, static_cast<int>(info->cch) + 1));
        }
    }
    return TRUE;
}

template<typename Info>
void applyMenuItemInfo(QAction* action, const Info* info)
{
    if (info->fMask & (MIIM_STRING | MIIM_TYPE)) {
        if (info->dwTypeData && !((info->fMask & MIIM_TYPE) && (info->fType & MFT_SEPARATOR)))
            action->setText(fromChars(info->dwTypeData));
    }
    if (info->fMask & MIIM_STATE) {
        if (info->fState & MFS_CHECKED)
            action->setCheckable(true);
        action->setChecked(info->fState & MFS_CHECKED);
        action->setEnabled(!(info->fState & MFS_GRAYED));
    }
    if (info->fMask & MIIM_ID)
        action->setData(static_cast<qulonglong>(info->wID));
    if (info->fMask & MIIM_DATA)
        action->setProperty("edita_itemData", static_cast<qulonglong>(info->dwItemData));
}

template<typename Info>
BOOL setMenuItemInfo(HMENU handle, UINT item, BOOL byPosition, const Info* info)
{
    Menu* m = menuOf(handle);
    QAction* action = m && !m->host ? findAction(m->menu, item, byPosition ? MF_BYPOSITION : MF_BYCOMMAND) : nullptr;
    if (!action || !info)
        return FALSE;
    applyMenuItemInfo(action, info);
    return TRUE;
}

template<typename Info>
BOOL insertMenuItem(HMENU handle, UINT item, BOOL byPosition, const Info* info)
{
    Menu* m = menuOf(handle);
    if (!m || !m->menu || !info)
        return FALSE;
    QAction* action = nullptr;
    const bool separator = (info->fMask & (MIIM_FTYPE | MIIM_TYPE)) && (info->fType & MFT_SEPARATOR);
    if (separator) {
        action = new QAction(m->menu);
        action->setSeparator(true);
    } else if ((info->fMask & MIIM_SUBMENU) && menuOf(info->hSubMenu) && menuOf(info->hSubMenu)->menu) {
        QMenu* sub = menuOf(info->hSubMenu)->menu;
        if (info->dwTypeData)
            sub->setTitle(fromChars(info->dwTypeData));
        action = sub->menuAction();
    } else {
        action = new QAction(m->menu);
        applyMenuItemInfo(action, info);
    }
    QAction* before = findAction(m->menu, item, byPosition ? MF_BYPOSITION : MF_BYCOMMAND);
    m->menu->insertAction(before, action);
    return TRUE;
}

} // namespace

extern "C" {

BOOL GetMenuItemInfoA(HMENU menu, UINT item, BOOL byPosition, LPMENUITEMINFOA info) { return getMenuItemInfo(menu, item, byPosition, info); }
BOOL GetMenuItemInfoW(HMENU menu, UINT item, BOOL byPosition, LPMENUITEMINFOW info) { return getMenuItemInfo(menu, item, byPosition, info); }
BOOL SetMenuItemInfoA(HMENU menu, UINT item, BOOL byPosition, LPCMENUITEMINFOA info) { return setMenuItemInfo(menu, item, byPosition, info); }
BOOL SetMenuItemInfoW(HMENU menu, UINT item, BOOL byPosition, LPCMENUITEMINFOW info) { return setMenuItemInfo(menu, item, byPosition, info); }
BOOL InsertMenuItemA(HMENU menu, UINT item, BOOL byPosition, LPCMENUITEMINFOA info) { return insertMenuItem(menu, item, byPosition, info); }
BOOL InsertMenuItemW(HMENU menu, UINT item, BOOL byPosition, LPCMENUITEMINFOW info) { return insertMenuItem(menu, item, byPosition, info); }

} // extern "C"
