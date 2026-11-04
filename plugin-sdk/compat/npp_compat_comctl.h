/* This file is part of Edita.
 * Copyright (C) 2026 Edita contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Capa de compatibilidad v2, parte 4 (la incluye npp_compat_user.h; el código está en
 * npp_window.cpp):
 *   - bucles de mensajes propios: GetMessage, PeekMessage, DispatchMessage,
 *     TranslateMessage, PostQuitMessage, PostThreadMessage y WaitMessage sobre el bucle de
 *     eventos de Qt. Lo que llega por PostMessage a una ventana se entrega mientras el
 *     plugin espera en GetMessage o mira con PeekMessage; WM_QUIT y PostThreadMessage van a
 *     la cola del hilo, que es la que devuelven;
 *   - dibujo: WM_PAINT (y WM_ERASEBKGND) en las clases registradas, BeginPaint/EndPaint
 *     sobre un QPainter, y las funciones de GDI de dibujo: rellenos, marcos, rectángulos,
 *     elipses, líneas, polígonos, texto (TextOut, ExtTextOut, DrawText), plumas, pinceles,
 *     DC en memoria (CreateCompatibleDC, CreateCompatibleBitmap) y BitBlt/StretchBlt;
 *   - RECT: PtInRect, InflateRect, OffsetRect, IntersectRect, UnionRect…;
 *   - las macros de windowsx.h (Edit_SetText, ComboBox_AddString, Button_GetCheck…);
 *   - controles comunes: pestañas (SysTabControl32), listas con columnas (SysListView32,
 *     en modo informe), árboles (SysTreeView32), barras de progreso (msctls_progress32),
 *     deslizadores (msctls_trackbar32) y tooltips (tooltips_class32), con sus mensajes,
 *     sus macros (TabCtrl_*, ListView_*, TreeView_*) y sus notificaciones (WM_NOTIFY).
 * Todavía no: dibujo propio de los controles (NM_CUSTOMDRAW, WM_DRAWITEM), listas
 * virtuales (LVS_OWNERDATA), listas de imágenes, edición de etiquetas y arrastrar.
 */

#ifndef EDITA_NPP_COMPAT_COMCTL_H
#define EDITA_NPP_COMPAT_COMCTL_H

/* --- Mensajes -------------------------------------------------------------------- */
#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001
#define PM_NOYIELD 0x0002
#define QS_ALLINPUT 0x04FF
#define QS_ALLEVENTS 0x04BF

/* --- Dibujo ---------------------------------------------------------------------- */
#define SRCCOPY 0x00CC0020
#define SRCPAINT 0x00EE0086
#define SRCAND 0x008800C6
#define SRCINVERT 0x00660046
#define PATCOPY 0x00F00021
#define BLACKNESS 0x00000042
#define WHITENESS 0x00FF0062
#define ETO_OPAQUE 0x0002
#define ETO_CLIPPED 0x0004
#define DT_TOP 0x00000000
#define DT_LEFT 0x00000000
#define DT_CENTER 0x00000001
#define DT_RIGHT 0x00000002
#define DT_VCENTER 0x00000004
#define DT_BOTTOM 0x00000008
#define DT_WORDBREAK 0x00000010
#define DT_SINGLELINE 0x00000020
#define DT_EXPANDTABS 0x00000040
#define DT_NOCLIP 0x00000100
#define DT_CALCRECT 0x00000400
#define DT_NOPREFIX 0x00000800
#define DT_END_ELLIPSIS 0x00008000
#define DT_PATH_ELLIPSIS 0x00004000
#define DT_WORD_ELLIPSIS 0x00040000
#define DT_HIDEPREFIX 0x00100000
#define PS_DASHDOT 3
#define PS_DASHDOTDOT 4
#define PS_INSIDEFRAME 6
#define TA_LEFT 0
#define TA_RIGHT 2
#define TA_CENTER 6
#define TA_TOP 0
#define TA_BOTTOM 8
#define TA_BASELINE 24
#define BDR_RAISEDOUTER 0x0001
#define BDR_SUNKENOUTER 0x0002
#define BDR_RAISEDINNER 0x0004
#define BDR_SUNKENINNER 0x0008
#define EDGE_RAISED (BDR_RAISEDOUTER | BDR_RAISEDINNER)
#define EDGE_SUNKEN (BDR_SUNKENOUTER | BDR_SUNKENINNER)
#define EDGE_ETCHED (BDR_SUNKENOUTER | BDR_RAISEDINNER)
#define EDGE_BUMP (BDR_RAISEDOUTER | BDR_SUNKENINNER)
#define BF_LEFT 0x0001
#define BF_TOP 0x0002
#define BF_RIGHT 0x0004
#define BF_BOTTOM 0x0008
#define BF_RECT (BF_LEFT | BF_TOP | BF_RIGHT | BF_BOTTOM)
#define BF_ADJUST 0x2000
#define DIB_RGB_COLORS 0
#define BI_RGB 0
#define GDI_ERROR 0xFFFFFFFFU
#define CLR_NONE 0xFFFFFFFF
#define CLR_DEFAULT 0xFF000000
#define OBJ_PEN 1
#define OBJ_BRUSH 2
#define OBJ_FONT 6
#define OBJ_BITMAP 7
#define LPSTR_TEXTCALLBACKA ((LPSTR)(LONG_PTR)-1)
#define LPSTR_TEXTCALLBACKW ((LPWSTR)(LONG_PTR)-1)
#ifdef UNICODE
#define LPSTR_TEXTCALLBACK LPSTR_TEXTCALLBACKW
#else
#define LPSTR_TEXTCALLBACK LPSTR_TEXTCALLBACKA
#endif
#define I_CHILDRENCALLBACK (-1)
#define I_IMAGECALLBACK (-1)
#define I_IMAGENONE (-2)

/* --- Controles comunes: clases ---------------------------------------------------- */
#define WC_TABCONTROLA "SysTabControl32"
#define WC_TABCONTROLW L"SysTabControl32"
#define WC_LISTVIEWA "SysListView32"
#define WC_LISTVIEWW L"SysListView32"
#define WC_TREEVIEWA "SysTreeView32"
#define WC_TREEVIEWW L"SysTreeView32"
#define PROGRESS_CLASSA "msctls_progress32"
#define PROGRESS_CLASSW L"msctls_progress32"
#define TRACKBAR_CLASSA "msctls_trackbar32"
#define TRACKBAR_CLASSW L"msctls_trackbar32"
#define TOOLTIPS_CLASSA "tooltips_class32"
#define TOOLTIPS_CLASSW L"tooltips_class32"
#ifdef UNICODE
#define WC_TABCONTROL WC_TABCONTROLW
#define WC_LISTVIEW WC_LISTVIEWW
#define WC_TREEVIEW WC_TREEVIEWW
#define PROGRESS_CLASS PROGRESS_CLASSW
#define TRACKBAR_CLASS TRACKBAR_CLASSW
#define TOOLTIPS_CLASS TOOLTIPS_CLASSW
#else
#define WC_TABCONTROL WC_TABCONTROLA
#define WC_LISTVIEW WC_LISTVIEWA
#define WC_TREEVIEW WC_TREEVIEWA
#define PROGRESS_CLASS PROGRESS_CLASSA
#define TRACKBAR_CLASS TRACKBAR_CLASSA
#define TOOLTIPS_CLASS TOOLTIPS_CLASSA
#endif
#define ICC_LISTVIEW_CLASSES 0x00000001
#define ICC_TREEVIEW_CLASSES 0x00000002
#define ICC_BAR_CLASSES 0x00000004
#define ICC_TAB_CLASSES 0x00000008
#define ICC_PROGRESS_CLASS 0x00000020
#define ICC_STANDARD_CLASSES 0x00004000
#define ICC_LINK_CLASS 0x00008000

/* --- Notificaciones generales (WM_NOTIFY) ------------------------------------------ */
#define NM_FIRST (0U - 0U)
#define NM_CLICK (NM_FIRST - 2)
#define NM_DBLCLK (NM_FIRST - 3)
#define NM_RETURN (NM_FIRST - 4)
#define NM_RCLICK (NM_FIRST - 5)
#define NM_RDBLCLK (NM_FIRST - 6)
#define NM_SETFOCUS (NM_FIRST - 7)
#define NM_KILLFOCUS (NM_FIRST - 8)
#define NM_CUSTOMDRAW (NM_FIRST - 12)
#define CDRF_DODEFAULT 0x00000000
#define CDRF_NEWFONT 0x00000002
#define CDRF_SKIPDEFAULT 0x00000004
#define CDRF_NOTIFYITEMDRAW 0x00000020
#define CDRF_NOTIFYSUBITEMDRAW 0x00000020
#define CDDS_PREPAINT 0x00000001
#define CDDS_ITEM 0x00010000
#define CDDS_ITEMPREPAINT (CDDS_ITEM | CDDS_PREPAINT)

/* --- Pestañas ---------------------------------------------------------------------- */
#define TCM_FIRST 0x1300
#define TCM_GETITEMCOUNT (TCM_FIRST + 4)
#define TCM_GETITEMA (TCM_FIRST + 5)
#define TCM_SETITEMA (TCM_FIRST + 6)
#define TCM_INSERTITEMA (TCM_FIRST + 7)
#define TCM_DELETEITEM (TCM_FIRST + 8)
#define TCM_DELETEALLITEMS (TCM_FIRST + 9)
#define TCM_GETITEMRECT (TCM_FIRST + 10)
#define TCM_GETCURSEL (TCM_FIRST + 11)
#define TCM_SETCURSEL (TCM_FIRST + 12)
#define TCM_ADJUSTRECT (TCM_FIRST + 40)
#define TCM_GETCURFOCUS (TCM_FIRST + 47)
#define TCM_SETCURFOCUS (TCM_FIRST + 48)
#define TCM_GETITEMW (TCM_FIRST + 60)
#define TCM_SETITEMW (TCM_FIRST + 61)
#define TCM_INSERTITEMW (TCM_FIRST + 62)
#define TCN_FIRST (0U - 550U)
#define TCN_KEYDOWN (TCN_FIRST - 0)
#define TCN_SELCHANGE (TCN_FIRST - 1)
#define TCN_SELCHANGING (TCN_FIRST - 2)
#define TCIF_TEXT 0x0001
#define TCIF_IMAGE 0x0002
#define TCIF_PARAM 0x0008
#define TCIF_STATE 0x0010
#define TCS_BOTTOM 0x0002
#define TCS_MULTILINE 0x0200
#define TCS_FIXEDWIDTH 0x0400
#define TCS_FOCUSNEVER 0x8000

/* --- Listas con columnas ----------------------------------------------------------- */
#define LVM_FIRST 0x1000
#define LVM_GETBKCOLOR (LVM_FIRST + 0)
#define LVM_SETBKCOLOR (LVM_FIRST + 1)
#define LVM_SETIMAGELIST (LVM_FIRST + 3)
#define LVM_GETITEMCOUNT (LVM_FIRST + 4)
#define LVM_GETITEMA (LVM_FIRST + 5)
#define LVM_SETITEMA (LVM_FIRST + 6)
#define LVM_INSERTITEMA (LVM_FIRST + 7)
#define LVM_DELETEITEM (LVM_FIRST + 8)
#define LVM_DELETEALLITEMS (LVM_FIRST + 9)
#define LVM_GETNEXTITEM (LVM_FIRST + 12)
#define LVM_FINDITEMA (LVM_FIRST + 13)
#define LVM_GETITEMRECT (LVM_FIRST + 14)
#define LVM_HITTEST (LVM_FIRST + 18)
#define LVM_ENSUREVISIBLE (LVM_FIRST + 19)
#define LVM_SCROLL (LVM_FIRST + 20)
#define LVM_REDRAWITEMS (LVM_FIRST + 21)
#define LVM_GETCOLUMNA (LVM_FIRST + 25)
#define LVM_SETCOLUMNA (LVM_FIRST + 26)
#define LVM_INSERTCOLUMNA (LVM_FIRST + 27)
#define LVM_DELETECOLUMN (LVM_FIRST + 28)
#define LVM_GETCOLUMNWIDTH (LVM_FIRST + 29)
#define LVM_SETCOLUMNWIDTH (LVM_FIRST + 30)
#define LVM_GETHEADER (LVM_FIRST + 31)
#define LVM_SETTEXTCOLOR (LVM_FIRST + 36)
#define LVM_SETTEXTBKCOLOR (LVM_FIRST + 38)
#define LVM_GETTOPINDEX (LVM_FIRST + 39)
#define LVM_GETCOUNTPERPAGE (LVM_FIRST + 40)
#define LVM_UPDATE (LVM_FIRST + 42)
#define LVM_SETITEMSTATE (LVM_FIRST + 43)
#define LVM_GETITEMSTATE (LVM_FIRST + 44)
#define LVM_GETITEMTEXTA (LVM_FIRST + 45)
#define LVM_SETITEMTEXTA (LVM_FIRST + 46)
#define LVM_SETITEMCOUNT (LVM_FIRST + 47)
#define LVM_SORTITEMS (LVM_FIRST + 48)
#define LVM_GETSELECTEDCOUNT (LVM_FIRST + 50)
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#define LVM_GETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 55)
#define LVM_GETSELECTIONMARK (LVM_FIRST + 66)
#define LVM_SETSELECTIONMARK (LVM_FIRST + 67)
#define LVM_GETITEMW (LVM_FIRST + 75)
#define LVM_SETITEMW (LVM_FIRST + 76)
#define LVM_INSERTITEMW (LVM_FIRST + 77)
#define LVM_FINDITEMW (LVM_FIRST + 83)
#define LVM_GETCOLUMNW (LVM_FIRST + 95)
#define LVM_SETCOLUMNW (LVM_FIRST + 96)
#define LVM_INSERTCOLUMNW (LVM_FIRST + 97)
#define LVM_GETITEMTEXTW (LVM_FIRST + 115)
#define LVM_SETITEMTEXTW (LVM_FIRST + 116)
#define LVN_FIRST (0U - 100U)
#define LVN_ITEMCHANGING (LVN_FIRST - 0)
#define LVN_ITEMCHANGED (LVN_FIRST - 1)
#define LVN_INSERTITEM (LVN_FIRST - 2)
#define LVN_DELETEITEM (LVN_FIRST - 3)
#define LVN_DELETEALLITEMS (LVN_FIRST - 4)
#define LVN_COLUMNCLICK (LVN_FIRST - 8)
#define LVN_ITEMACTIVATE (LVN_FIRST - 14)
#define LVN_KEYDOWN (LVN_FIRST - 55)
#define LVN_GETDISPINFOA (LVN_FIRST - 50)
#define LVN_GETDISPINFOW (LVN_FIRST - 77)
#define LVIF_TEXT 0x00000001
#define LVIF_IMAGE 0x00000002
#define LVIF_PARAM 0x00000004
#define LVIF_STATE 0x00000008
#define LVIF_INDENT 0x00000010
#define LVIF_NORECOMPUTE 0x00000800
#define LVIS_FOCUSED 0x0001
#define LVIS_SELECTED 0x0002
#define LVIS_CUT 0x0004
#define LVIS_DROPHILITED 0x0008
#define LVIS_STATEIMAGEMASK 0xF000
#define INDEXTOSTATEIMAGEMASK(i) ((i) << 12)
#define LVNI_ALL 0x0000
#define LVNI_FOCUSED 0x0001
#define LVNI_SELECTED 0x0002
#define LVS_ICON 0x0000
#define LVS_REPORT 0x0001
#define LVS_SMALLICON 0x0002
#define LVS_LIST 0x0003
#define LVS_TYPEMASK 0x0003
#define LVS_SINGLESEL 0x0004
#define LVS_SHOWSELALWAYS 0x0008
#define LVS_SORTASCENDING 0x0010
#define LVS_SORTDESCENDING 0x0020
#define LVS_NOLABELWRAP 0x0080
#define LVS_EDITLABELS 0x0200
#define LVS_OWNERDATA 0x1000
#define LVS_NOCOLUMNHEADER 0x4000
#define LVS_NOSORTHEADER 0x8000
#define LVS_EX_GRIDLINES 0x00000001
#define LVS_EX_CHECKBOXES 0x00000004
#define LVS_EX_TRACKSELECT 0x00000008
#define LVS_EX_HEADERDRAGDROP 0x00000010
#define LVS_EX_FULLROWSELECT 0x00000020
#define LVS_EX_INFOTIP 0x00000400
#define LVS_EX_LABELTIP 0x00004000
#define LVS_EX_DOUBLEBUFFER 0x00010000
#define LVCF_FMT 0x0001
#define LVCF_WIDTH 0x0002
#define LVCF_TEXT 0x0004
#define LVCF_SUBITEM 0x0008
#define LVCFMT_LEFT 0x0000
#define LVCFMT_RIGHT 0x0001
#define LVCFMT_CENTER 0x0002
#define LVSCW_AUTOSIZE (-1)
#define LVSCW_AUTOSIZE_USEHEADER (-2)
#define LVFI_PARAM 0x0001
#define LVFI_STRING 0x0002
#define LVFI_PARTIAL 0x0008
#define LVHT_NOWHERE 0x0001
#define LVHT_ONITEMLABEL 0x0004
#define LVHT_ONITEM 0x000E
#define LVIR_BOUNDS 0
#define LVIR_LABEL 2

/* --- Árboles ------------------------------------------------------------------------ */
/* HTREEITEM: en npp_compat.h */
#define TV_FIRST 0x1100
#define TVM_INSERTITEMA (TV_FIRST + 0)
#define TVM_DELETEITEM (TV_FIRST + 1)
#define TVM_EXPAND (TV_FIRST + 2)
#define TVM_GETITEMRECT (TV_FIRST + 4)
#define TVM_GETCOUNT (TV_FIRST + 5)
#define TVM_SETIMAGELIST (TV_FIRST + 9)
#define TVM_GETNEXTITEM (TV_FIRST + 10)
#define TVM_SELECTITEM (TV_FIRST + 11)
#define TVM_GETITEMA (TV_FIRST + 12)
#define TVM_SETITEMA (TV_FIRST + 13)
#define TVM_GETVISIBLECOUNT (TV_FIRST + 16)
#define TVM_HITTEST (TV_FIRST + 17)
#define TVM_SORTCHILDREN (TV_FIRST + 19)
#define TVM_ENSUREVISIBLE (TV_FIRST + 20)
#define TVM_SETBKCOLOR (TV_FIRST + 29)
#define TVM_SETTEXTCOLOR (TV_FIRST + 30)
#define TVM_GETITEMSTATE (TV_FIRST + 39)
#define TVM_INSERTITEMW (TV_FIRST + 50)
#define TVM_GETITEMW (TV_FIRST + 62)
#define TVM_SETITEMW (TV_FIRST + 63)
#define TVN_FIRST (0U - 400U)
#define TVN_SELCHANGINGA (TVN_FIRST - 1)
#define TVN_SELCHANGEDA (TVN_FIRST - 2)
#define TVN_ITEMEXPANDINGA (TVN_FIRST - 5)
#define TVN_ITEMEXPANDEDA (TVN_FIRST - 6)
#define TVN_DELETEITEMA (TVN_FIRST - 9)
#define TVN_KEYDOWN (TVN_FIRST - 12)
#define TVN_SELCHANGINGW (TVN_FIRST - 50)
#define TVN_SELCHANGEDW (TVN_FIRST - 51)
#define TVN_ITEMEXPANDINGW (TVN_FIRST - 54)
#define TVN_ITEMEXPANDEDW (TVN_FIRST - 55)
#define TVN_DELETEITEMW (TVN_FIRST - 58)
#define TVI_ROOT ((HTREEITEM)(ULONG_PTR) - 0x10000)
#define TVI_FIRST ((HTREEITEM)(ULONG_PTR) - 0x0FFFF)
#define TVI_LAST ((HTREEITEM)(ULONG_PTR) - 0x0FFFE)
#define TVI_SORT ((HTREEITEM)(ULONG_PTR) - 0x0FFFD)
#define TVGN_ROOT 0x0000
#define TVGN_NEXT 0x0001
#define TVGN_PREVIOUS 0x0002
#define TVGN_PARENT 0x0003
#define TVGN_CHILD 0x0004
#define TVGN_FIRSTVISIBLE 0x0005
#define TVGN_NEXTVISIBLE 0x0006
#define TVGN_PREVIOUSVISIBLE 0x0007
#define TVGN_DROPHILITE 0x0008
#define TVGN_CARET 0x0009
#define TVGN_LASTVISIBLE 0x000A
#define TVE_COLLAPSE 0x0001
#define TVE_EXPAND 0x0002
#define TVE_TOGGLE 0x0003
#define TVIF_TEXT 0x0001
#define TVIF_IMAGE 0x0002
#define TVIF_PARAM 0x0004
#define TVIF_STATE 0x0008
#define TVIF_HANDLE 0x0010
#define TVIF_SELECTEDIMAGE 0x0020
#define TVIF_CHILDREN 0x0040
#define TVIS_SELECTED 0x0002
#define TVIS_BOLD 0x0010
#define TVIS_EXPANDED 0x0020
#define TVIS_STATEIMAGEMASK 0xF000
#define TVS_HASBUTTONS 0x0001
#define TVS_HASLINES 0x0002
#define TVS_LINESATROOT 0x0004
#define TVS_EDITLABELS 0x0008
#define TVS_SHOWSELALWAYS 0x0020
#define TVS_CHECKBOXES 0x0100
#define TVS_FULLROWSELECT 0x1000
#define TVC_UNKNOWN 0x0000
#define TVC_BYMOUSE 0x0001
#define TVC_BYKEYBOARD 0x0002
#define TVHT_NOWHERE 0x0001
#define TVHT_ONITEMLABEL 0x0004
#define TVHT_ONITEM 0x0046

/* --- Barras de progreso y deslizadores ---------------------------------------------- */
#define PBM_SETRANGE (WM_USER + 1)
#define PBM_SETPOS (WM_USER + 2)
#define PBM_DELTAPOS (WM_USER + 3)
#define PBM_SETSTEP (WM_USER + 4)
#define PBM_STEPIT (WM_USER + 5)
#define PBM_SETRANGE32 (WM_USER + 6)
#define PBM_GETRANGE (WM_USER + 7)
#define PBM_GETPOS (WM_USER + 8)
#define PBM_SETMARQUEE (WM_USER + 10)
#define PBS_SMOOTH 0x01
#define PBS_VERTICAL 0x04
#define PBS_MARQUEE 0x08
#define TBM_GETPOS (WM_USER)
#define TBM_GETRANGEMIN (WM_USER + 1)
#define TBM_GETRANGEMAX (WM_USER + 2)
#define TBM_SETPOS (WM_USER + 5)
#define TBM_SETRANGE (WM_USER + 6)
#define TBM_SETRANGEMIN (WM_USER + 7)
#define TBM_SETRANGEMAX (WM_USER + 8)
#define TBM_SETTICFREQ (WM_USER + 20)
#define TBM_SETPAGESIZE (WM_USER + 21)
#define TBM_SETLINESIZE (WM_USER + 23)
#define TBS_HORZ 0x0000
#define TBS_VERT 0x0002
#define TBS_NOTICKS 0x0010
#define TB_LINEUP 0
#define TB_LINEDOWN 1
#define TB_PAGEUP 2
#define TB_PAGEDOWN 3
#define TB_THUMBPOSITION 4
#define TB_THUMBTRACK 5
#define TB_TOP 6
#define TB_BOTTOM 7
#define TB_ENDTRACK 8

/* --- Tooltips ------------------------------------------------------------------------ */
#define TTM_ACTIVATE (WM_USER + 1)
#define TTM_ADDTOOLA (WM_USER + 4)
#define TTM_DELTOOLA (WM_USER + 5)
#define TTM_UPDATETIPTEXTA (WM_USER + 12)
#define TTM_SETMAXTIPWIDTH (WM_USER + 24)
#define TTM_ADDTOOLW (WM_USER + 50)
#define TTM_DELTOOLW (WM_USER + 51)
#define TTM_UPDATETIPTEXTW (WM_USER + 57)
#define TTF_IDISHWND 0x0001
#define TTF_CENTERTIP 0x0002
#define TTF_SUBCLASS 0x0010
#define TTS_ALWAYSTIP 0x01
#define TTS_NOPREFIX 0x02
#define TTS_BALLOON 0x40

EDITA_COMPAT_BEGIN

/* --- Estructuras ------------------------------------------------------------------- */
typedef struct tagPAINTSTRUCT {
    HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore; BOOL fIncUpdate; BYTE rgbReserved[32];
} PAINTSTRUCT, *PPAINTSTRUCT, *LPPAINTSTRUCT;
typedef struct tagBITMAP {
    LONG bmType; LONG bmWidth; LONG bmHeight; LONG bmWidthBytes; WORD bmPlanes; WORD bmBitsPixel; LPVOID bmBits;
} BITMAP, *PBITMAP, *LPBITMAP;
typedef struct tagBITMAPINFOHEADER {
    DWORD biSize; LONG biWidth; LONG biHeight; WORD biPlanes; WORD biBitCount; DWORD biCompression; DWORD biSizeImage;
    LONG biXPelsPerMeter; LONG biYPelsPerMeter; DWORD biClrUsed; DWORD biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct tagRGBQUAD { BYTE rgbBlue; BYTE rgbGreen; BYTE rgbRed; BYTE rgbReserved; } RGBQUAD;
typedef struct tagBITMAPINFO { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1]; } BITMAPINFO, *LPBITMAPINFO;
typedef struct tagLOGBRUSH { UINT lbStyle; COLORREF lbColor; ULONG_PTR lbHatch; } LOGBRUSH, *LPLOGBRUSH;
#define BS_SOLID 0
#define BS_NULL 1
#define BS_HOLLOW BS_NULL

typedef struct tagTCITEMA { UINT mask; DWORD dwState; DWORD dwStateMask; LPSTR pszText; int cchTextMax; int iImage; LPARAM lParam; } TCITEMA, *LPTCITEMA;
typedef struct tagTCITEMW { UINT mask; DWORD dwState; DWORD dwStateMask; LPWSTR pszText; int cchTextMax; int iImage; LPARAM lParam; } TCITEMW, *LPTCITEMW;

typedef struct tagLVITEMA {
    UINT mask; int iItem; int iSubItem; UINT state; UINT stateMask; LPSTR pszText; int cchTextMax; int iImage; LPARAM lParam;
    int iIndent; int iGroupId; UINT cColumns; PUINT puColumns; int* piColFmt; int iGroup;
} LVITEMA, *LPLVITEMA;
typedef struct tagLVITEMW {
    UINT mask; int iItem; int iSubItem; UINT state; UINT stateMask; LPWSTR pszText; int cchTextMax; int iImage; LPARAM lParam;
    int iIndent; int iGroupId; UINT cColumns; PUINT puColumns; int* piColFmt; int iGroup;
} LVITEMW, *LPLVITEMW;
typedef struct tagLVCOLUMNA {
    UINT mask; int fmt; int cx; LPSTR pszText; int cchTextMax; int iSubItem; int iImage; int iOrder; int cxMin; int cxDefault; int cxIdeal;
} LVCOLUMNA, *LPLVCOLUMNA;
typedef struct tagLVCOLUMNW {
    UINT mask; int fmt; int cx; LPWSTR pszText; int cchTextMax; int iSubItem; int iImage; int iOrder; int cxMin; int cxDefault; int cxIdeal;
} LVCOLUMNW, *LPLVCOLUMNW;
typedef struct tagLVFINDINFOA { UINT flags; LPCSTR psz; LPARAM lParam; POINT pt; UINT vkDirection; } LVFINDINFOA, *LPLVFINDINFOA;
typedef struct tagLVFINDINFOW { UINT flags; LPCWSTR psz; LPARAM lParam; POINT pt; UINT vkDirection; } LVFINDINFOW, *LPLVFINDINFOW;
typedef struct tagLVHITTESTINFO { POINT pt; UINT flags; int iItem; int iSubItem; int iGroup; } LVHITTESTINFO, *LPLVHITTESTINFO;
typedef struct tagNMLISTVIEW {
    NMHDR hdr; int iItem; int iSubItem; UINT uNewState; UINT uOldState; UINT uChanged; POINT ptAction; LPARAM lParam;
} NMLISTVIEW, *LPNMLISTVIEW;
typedef struct tagNMITEMACTIVATE {
    NMHDR hdr; int iItem; int iSubItem; UINT uNewState; UINT uOldState; UINT uChanged; POINT ptAction; LPARAM lParam; UINT uKeyFlags;
} NMITEMACTIVATE, *LPNMITEMACTIVATE;
typedef struct tagLVKEYDOWN { NMHDR hdr; WORD wVKey; UINT flags; } NMLVKEYDOWN, *LPNMLVKEYDOWN;
typedef int (CALLBACK* PFNLVCOMPARE)(LPARAM, LPARAM, LPARAM);

typedef struct tagTVITEMA {
    UINT mask; HTREEITEM hItem; UINT state; UINT stateMask; LPSTR pszText; int cchTextMax; int iImage; int iSelectedImage;
    int cChildren; LPARAM lParam;
} TVITEMA, *LPTVITEMA;
typedef struct tagTVITEMW {
    UINT mask; HTREEITEM hItem; UINT state; UINT stateMask; LPWSTR pszText; int cchTextMax; int iImage; int iSelectedImage;
    int cChildren; LPARAM lParam;
} TVITEMW, *LPTVITEMW;
typedef struct tagTVITEMEXA {
    UINT mask; HTREEITEM hItem; UINT state; UINT stateMask; LPSTR pszText; int cchTextMax; int iImage; int iSelectedImage;
    int cChildren; LPARAM lParam; int iIntegral; UINT uStateEx; HWND hwnd; int iExpandedImage; int iReserved;
} TVITEMEXA, *LPTVITEMEXA;
typedef struct tagTVITEMEXW {
    UINT mask; HTREEITEM hItem; UINT state; UINT stateMask; LPWSTR pszText; int cchTextMax; int iImage; int iSelectedImage;
    int cChildren; LPARAM lParam; int iIntegral; UINT uStateEx; HWND hwnd; int iExpandedImage; int iReserved;
} TVITEMEXW, *LPTVITEMEXW;
typedef struct tagTVINSERTSTRUCTA {
    HTREEITEM hParent; HTREEITEM hInsertAfter;
    union { TVITEMEXA itemex; TVITEMA item; };
} TVINSERTSTRUCTA, *LPTVINSERTSTRUCTA;
typedef struct tagTVINSERTSTRUCTW {
    HTREEITEM hParent; HTREEITEM hInsertAfter;
    union { TVITEMEXW itemex; TVITEMW item; };
} TVINSERTSTRUCTW, *LPTVINSERTSTRUCTW;
typedef struct tagNMTREEVIEWA { NMHDR hdr; UINT action; TVITEMA itemOld; TVITEMA itemNew; POINT ptDrag; } NMTREEVIEWA, *LPNMTREEVIEWA;
typedef struct tagNMTREEVIEWW { NMHDR hdr; UINT action; TVITEMW itemOld; TVITEMW itemNew; POINT ptDrag; } NMTREEVIEWW, *LPNMTREEVIEWW;
typedef struct tagTVKEYDOWN { NMHDR hdr; WORD wVKey; UINT flags; } NMTVKEYDOWN, *LPNMTVKEYDOWN;
typedef struct tagTVHITTESTINFO { POINT pt; UINT flags; HTREEITEM hItem; } TVHITTESTINFO, *LPTVHITTESTINFO;
typedef struct tagTCKEYDOWN { NMHDR hdr; WORD wVKey; UINT flags; } NMTCKEYDOWN;

typedef struct tagTOOLINFOA {
    UINT cbSize; UINT uFlags; HWND hwnd; UINT_PTR uId; RECT rect; HINSTANCE hinst; LPSTR lpszText; LPARAM lParam; void* lpReserved;
} TTTOOLINFOA, *LPTTTOOLINFOA;
typedef struct tagTOOLINFOW {
    UINT cbSize; UINT uFlags; HWND hwnd; UINT_PTR uId; RECT rect; HINSTANCE hinst; LPWSTR lpszText; LPARAM lParam; void* lpReserved;
} TTTOOLINFOW, *LPTTTOOLINFOW;

typedef struct tagNMCUSTOMDRAWINFO {
    NMHDR hdr; DWORD dwDrawStage; HDC hdc; RECT rc; DWORD_PTR dwItemSpec; UINT uItemState; LPARAM lItemlParam;
} NMCUSTOMDRAW, *LPNMCUSTOMDRAW;

#ifdef UNICODE
typedef TCITEMW TCITEM;
typedef LVITEMW LVITEM;
typedef LVCOLUMNW LVCOLUMN;
typedef LVFINDINFOW LVFINDINFO;
typedef TVITEMW TVITEM;
typedef TVITEMEXW TVITEMEX;
typedef TVINSERTSTRUCTW TVINSERTSTRUCT;
typedef NMTREEVIEWW NMTREEVIEW;
typedef TTTOOLINFOW TTTOOLINFO;
#else
typedef TCITEMA TCITEM;
typedef LVITEMA LVITEM;
typedef LVCOLUMNA LVCOLUMN;
typedef LVFINDINFOA LVFINDINFO;
typedef TVITEMA TVITEM;
typedef TVITEMEXA TVITEMEX;
typedef TVINSERTSTRUCTA TVINSERTSTRUCT;
typedef NMTREEVIEWA NMTREEVIEW;
typedef TTTOOLINFOA TTTOOLINFO;
#endif
typedef TCITEM* LPTCITEM;
typedef LVITEM* LPLVITEM;
typedef LVCOLUMN* LPLVCOLUMN;
typedef TVITEM* LPTVITEM;
typedef TVINSERTSTRUCT* LPTVINSERTSTRUCT;
typedef NMTREEVIEW* LPNMTREEVIEW;
typedef TTTOOLINFO TOOLINFO;
typedef LVITEM LV_ITEM;
typedef LVCOLUMN LV_COLUMN;
typedef TVITEM TV_ITEM;
typedef TVINSERTSTRUCT TV_INSERTSTRUCT;
typedef NMTREEVIEW NM_TREEVIEW;
typedef NMLISTVIEW NM_LISTVIEW;
typedef TCITEM TC_ITEM;

/* --- Bucles de mensajes --------------------------------------------------------------- */
BOOL GetMessageA(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax);
BOOL GetMessageW(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax);
BOOL PeekMessageA(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax, UINT remove);
BOOL PeekMessageW(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax, UINT remove);
LRESULT DispatchMessageA(const MSG* msg);
LRESULT DispatchMessageW(const MSG* msg);
BOOL TranslateMessage(const MSG* msg);
void PostQuitMessage(int exitCode);
BOOL PostThreadMessageA(DWORD threadId, UINT message, WPARAM wParam, LPARAM lParam);
BOOL PostThreadMessageW(DWORD threadId, UINT message, WPARAM wParam, LPARAM lParam);
BOOL WaitMessage(void);
DWORD GetQueueStatus(UINT flags);
DWORD MsgWaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL waitAll, DWORD milliseconds, DWORD wakeMask);

/* --- Dibujo ------------------------------------------------------------------------- */
HDC BeginPaint(HWND hwnd, LPPAINTSTRUCT paint);
BOOL EndPaint(HWND hwnd, const PAINTSTRUCT* paint);
BOOL ValidateRect(HWND hwnd, const RECT* rect);
BOOL GetUpdateRect(HWND hwnd, LPRECT rect, BOOL erase);
int FillRect(HDC dc, const RECT* rect, HBRUSH brush);
int FrameRect(HDC dc, const RECT* rect, HBRUSH brush);
BOOL InvertRect(HDC dc, const RECT* rect);
BOOL DrawFocusRect(HDC dc, const RECT* rect);
BOOL DrawEdge(HDC dc, LPRECT rect, UINT edge, UINT flags);
BOOL Rectangle(HDC dc, int left, int top, int right, int bottom);
BOOL RoundRect(HDC dc, int left, int top, int right, int bottom, int width, int height);
BOOL Ellipse(HDC dc, int left, int top, int right, int bottom);
BOOL MoveToEx(HDC dc, int x, int y, LPPOINT previous);
BOOL LineTo(HDC dc, int x, int y);
BOOL Polyline(HDC dc, const POINT* points, int count);
BOOL Polygon(HDC dc, const POINT* points, int count);
COLORREF SetPixel(HDC dc, int x, int y, COLORREF color);
BOOL TextOutA(HDC dc, int x, int y, LPCSTR text, int length);
BOOL TextOutW(HDC dc, int x, int y, LPCWSTR text, int length);
BOOL ExtTextOutA(HDC dc, int x, int y, UINT options, const RECT* rect, LPCSTR text, UINT length, const INT* dx);
BOOL ExtTextOutW(HDC dc, int x, int y, UINT options, const RECT* rect, LPCWSTR text, UINT length, const INT* dx);
int DrawTextA(HDC dc, LPCSTR text, int length, LPRECT rect, UINT format);
int DrawTextW(HDC dc, LPCWSTR text, int length, LPRECT rect, UINT format);
UINT SetTextAlign(HDC dc, UINT align);
HDC CreateCompatibleDC(HDC dc);
HBITMAP CreateCompatibleBitmap(HDC dc, int width, int height);
HBITMAP CreateBitmap(int width, int height, UINT planes, UINT bitsPerPixel, const void* bits);
BOOL BitBlt(HDC dest, int x, int y, int width, int height, HDC source, int sx, int sy, DWORD rop);
BOOL StretchBlt(HDC dest, int x, int y, int width, int height, HDC source, int sx, int sy, int sw, int sh, DWORD rop);
BOOL PatBlt(HDC dc, int x, int y, int width, int height, DWORD rop);
int SaveDC(HDC dc);
BOOL RestoreDC(HDC dc, int saved);
int IntersectClipRect(HDC dc, int left, int top, int right, int bottom);
HBRUSH CreateBrushIndirect(const LOGBRUSH* brush);
COLORREF SetDCBrushColor(HDC dc, COLORREF color);
COLORREF SetDCPenColor(HDC dc, COLORREF color);
HGDIOBJ GetCurrentObject(HDC dc, UINT type);

/* --- RECT ------------------------------------------------------------------------------ */
BOOL SetRect(LPRECT rect, int left, int top, int right, int bottom);
BOOL SetRectEmpty(LPRECT rect);
BOOL CopyRect(LPRECT dest, const RECT* source);
BOOL IsRectEmpty(const RECT* rect);
BOOL PtInRect(const RECT* rect, POINT point);
BOOL InflateRect(LPRECT rect, int dx, int dy);
BOOL OffsetRect(LPRECT rect, int dx, int dy);
BOOL IntersectRect(LPRECT dest, const RECT* a, const RECT* b);
BOOL UnionRect(LPRECT dest, const RECT* a, const RECT* b);
BOOL SubtractRect(LPRECT dest, const RECT* a, const RECT* b);
BOOL EqualRect(const RECT* a, const RECT* b);

EDITA_COMPAT_END

#define GetMessage EDITA_COMPAT_AW(GetMessage)
#define PeekMessage EDITA_COMPAT_AW(PeekMessage)
#define DispatchMessage EDITA_COMPAT_AW(DispatchMessage)
#define PostThreadMessage EDITA_COMPAT_AW(PostThreadMessage)
#define TextOut EDITA_COMPAT_AW(TextOut)
#define ExtTextOut EDITA_COMPAT_AW(ExtTextOut)
#define DrawText EDITA_COMPAT_AW(DrawText)
#define TCM_GETITEM EDITA_COMPAT_AW(TCM_GETITEM)
#define TCM_SETITEM EDITA_COMPAT_AW(TCM_SETITEM)
#define TCM_INSERTITEM EDITA_COMPAT_AW(TCM_INSERTITEM)
#define LVM_GETITEM EDITA_COMPAT_AW(LVM_GETITEM)
#define LVM_SETITEM EDITA_COMPAT_AW(LVM_SETITEM)
#define LVM_INSERTITEM EDITA_COMPAT_AW(LVM_INSERTITEM)
#define LVM_FINDITEM EDITA_COMPAT_AW(LVM_FINDITEM)
#define LVM_GETCOLUMN EDITA_COMPAT_AW(LVM_GETCOLUMN)
#define LVM_SETCOLUMN EDITA_COMPAT_AW(LVM_SETCOLUMN)
#define LVM_INSERTCOLUMN EDITA_COMPAT_AW(LVM_INSERTCOLUMN)
#define LVM_GETITEMTEXT EDITA_COMPAT_AW(LVM_GETITEMTEXT)
#define LVM_SETITEMTEXT EDITA_COMPAT_AW(LVM_SETITEMTEXT)
#define LVN_GETDISPINFO EDITA_COMPAT_AW(LVN_GETDISPINFO)
#define TVM_INSERTITEM EDITA_COMPAT_AW(TVM_INSERTITEM)
#define TVM_GETITEM EDITA_COMPAT_AW(TVM_GETITEM)
#define TVM_SETITEM EDITA_COMPAT_AW(TVM_SETITEM)
#define TVN_SELCHANGING EDITA_COMPAT_AW(TVN_SELCHANGING)
#define TVN_SELCHANGED EDITA_COMPAT_AW(TVN_SELCHANGED)
#define TVN_ITEMEXPANDING EDITA_COMPAT_AW(TVN_ITEMEXPANDING)
#define TVN_ITEMEXPANDED EDITA_COMPAT_AW(TVN_ITEMEXPANDED)
#define TVN_DELETEITEM EDITA_COMPAT_AW(TVN_DELETEITEM)
#define TTM_ADDTOOL EDITA_COMPAT_AW(TTM_ADDTOOL)
#define TTM_DELTOOL EDITA_COMPAT_AW(TTM_DELTOOL)
#define TTM_UPDATETIPTEXT EDITA_COMPAT_AW(TTM_UPDATETIPTEXT)

/* --- Macros de commctrl.h ---------------------------------------------------------------- */
#define EDITA_SEND(h, m, w, l) SendMessage((h), (m), (WPARAM)(w), (LPARAM)(l))
#ifdef __cplusplus
#define EDITA_ZERO_INIT {}
#else
#define EDITA_ZERO_INIT {0}
#endif
#define TabCtrl_GetItemCount(h) (int)EDITA_SEND(h, TCM_GETITEMCOUNT, 0, 0)
#define TabCtrl_GetItem(h, i, item) (BOOL)EDITA_SEND(h, TCM_GETITEM, i, item)
#define TabCtrl_SetItem(h, i, item) (BOOL)EDITA_SEND(h, TCM_SETITEM, i, item)
#define TabCtrl_InsertItem(h, i, item) (int)EDITA_SEND(h, TCM_INSERTITEM, i, item)
#define TabCtrl_DeleteItem(h, i) (BOOL)EDITA_SEND(h, TCM_DELETEITEM, i, 0)
#define TabCtrl_DeleteAllItems(h) (BOOL)EDITA_SEND(h, TCM_DELETEALLITEMS, 0, 0)
#define TabCtrl_GetItemRect(h, i, rc) (BOOL)EDITA_SEND(h, TCM_GETITEMRECT, i, rc)
#define TabCtrl_GetCurSel(h) (int)EDITA_SEND(h, TCM_GETCURSEL, 0, 0)
#define TabCtrl_SetCurSel(h, i) (int)EDITA_SEND(h, TCM_SETCURSEL, i, 0)
#define TabCtrl_GetCurFocus(h) (int)EDITA_SEND(h, TCM_GETCURFOCUS, 0, 0)
#define TabCtrl_SetCurFocus(h, i) EDITA_SEND(h, TCM_SETCURFOCUS, i, 0)
#define TabCtrl_AdjustRect(h, larger, rc) (int)EDITA_SEND(h, TCM_ADJUSTRECT, larger, rc)

#define ListView_GetItemCount(h) (int)EDITA_SEND(h, LVM_GETITEMCOUNT, 0, 0)
#define ListView_GetItem(h, item) (BOOL)EDITA_SEND(h, LVM_GETITEM, 0, item)
#define ListView_SetItem(h, item) (BOOL)EDITA_SEND(h, LVM_SETITEM, 0, item)
#define ListView_InsertItem(h, item) (int)EDITA_SEND(h, LVM_INSERTITEM, 0, item)
#define ListView_DeleteItem(h, i) (BOOL)EDITA_SEND(h, LVM_DELETEITEM, i, 0)
#define ListView_DeleteAllItems(h) (BOOL)EDITA_SEND(h, LVM_DELETEALLITEMS, 0, 0)
#define ListView_GetNextItem(h, i, flags) (int)EDITA_SEND(h, LVM_GETNEXTITEM, i, MAKELPARAM((flags), 0))
#define ListView_FindItem(h, start, info) (int)EDITA_SEND(h, LVM_FINDITEM, start, info)
#define ListView_GetItemRect(h, i, rc, code) \
    (BOOL)(((rc) ? (void)((rc)->left = (code)) : (void)0), EDITA_SEND(h, LVM_GETITEMRECT, i, rc))
#define ListView_HitTest(h, info) (int)EDITA_SEND(h, LVM_HITTEST, 0, info)
#define ListView_EnsureVisible(h, i, partial) (BOOL)EDITA_SEND(h, LVM_ENSUREVISIBLE, i, MAKELPARAM((partial), 0))
#define ListView_RedrawItems(h, first, last) (BOOL)EDITA_SEND(h, LVM_REDRAWITEMS, first, last)
#define ListView_GetColumn(h, i, col) (BOOL)EDITA_SEND(h, LVM_GETCOLUMN, i, col)
#define ListView_SetColumn(h, i, col) (BOOL)EDITA_SEND(h, LVM_SETCOLUMN, i, col)
#define ListView_InsertColumn(h, i, col) (int)EDITA_SEND(h, LVM_INSERTCOLUMN, i, col)
#define ListView_DeleteColumn(h, i) (BOOL)EDITA_SEND(h, LVM_DELETECOLUMN, i, 0)
#define ListView_GetColumnWidth(h, i) (int)EDITA_SEND(h, LVM_GETCOLUMNWIDTH, i, 0)
#define ListView_SetColumnWidth(h, i, cx) (BOOL)EDITA_SEND(h, LVM_SETCOLUMNWIDTH, i, MAKELPARAM((cx), 0))
#define ListView_GetHeader(h) (HWND)EDITA_SEND(h, LVM_GETHEADER, 0, 0)
#define ListView_SetTextColor(h, c) (BOOL)EDITA_SEND(h, LVM_SETTEXTCOLOR, 0, c)
#define ListView_SetTextBkColor(h, c) (BOOL)EDITA_SEND(h, LVM_SETTEXTBKCOLOR, 0, c)
#define ListView_SetBkColor(h, c) (BOOL)EDITA_SEND(h, LVM_SETBKCOLOR, 0, c)
#define ListView_GetTopIndex(h) (int)EDITA_SEND(h, LVM_GETTOPINDEX, 0, 0)
#define ListView_GetCountPerPage(h) (int)EDITA_SEND(h, LVM_GETCOUNTPERPAGE, 0, 0)
#define ListView_Update(h, i) (BOOL)EDITA_SEND(h, LVM_UPDATE, i, 0)
#define ListView_SetItemState(h, i, data, mask)                                     \
    do {                                                                             \
        LVITEM edita_lvi_ = EDITA_ZERO_INIT;                                                         \
        edita_lvi_.stateMask = (mask);                                               \
        edita_lvi_.state = (data);                                                   \
        EDITA_SEND(h, LVM_SETITEMSTATE, i, &edita_lvi_);                             \
    } while (0)
#define ListView_GetItemState(h, i, mask) (UINT)EDITA_SEND(h, LVM_GETITEMSTATE, i, mask)
#define ListView_SetCheckState(h, i, check) ListView_SetItemState(h, i, INDEXTOSTATEIMAGEMASK((check) ? 2 : 1), LVIS_STATEIMAGEMASK)
#define ListView_GetCheckState(h, i) ((((UINT)(EDITA_SEND(h, LVM_GETITEMSTATE, i, LVIS_STATEIMAGEMASK))) >> 12) - 1)
#define ListView_GetItemText(h, i, sub, text, size)                                 \
    do {                                                                             \
        LVITEM edita_lvi_ = EDITA_ZERO_INIT;                                                         \
        edita_lvi_.iSubItem = (sub);                                                 \
        edita_lvi_.cchTextMax = (size);                                              \
        edita_lvi_.pszText = (text);                                                 \
        EDITA_SEND(h, LVM_GETITEMTEXT, i, &edita_lvi_);                              \
    } while (0)
#define ListView_SetItemText(h, i, sub, text)                                       \
    do {                                                                             \
        LVITEM edita_lvi_ = EDITA_ZERO_INIT;                                                         \
        edita_lvi_.iSubItem = (sub);                                                 \
        edita_lvi_.pszText = (text);                                                 \
        EDITA_SEND(h, LVM_SETITEMTEXT, i, &edita_lvi_);                              \
    } while (0)
#define ListView_SetItemCount(h, n) EDITA_SEND(h, LVM_SETITEMCOUNT, n, 0)
#define ListView_SetItemCountEx(h, n, flags) EDITA_SEND(h, LVM_SETITEMCOUNT, n, flags)
#define ListView_SortItems(h, compare, param) (BOOL)EDITA_SEND(h, LVM_SORTITEMS, param, compare)
#define ListView_GetSelectedCount(h) (UINT)EDITA_SEND(h, LVM_GETSELECTEDCOUNT, 0, 0)
#define ListView_GetSelectionMark(h) (int)EDITA_SEND(h, LVM_GETSELECTIONMARK, 0, 0)
#define ListView_SetSelectionMark(h, i) (int)EDITA_SEND(h, LVM_SETSELECTIONMARK, 0, i)
#define ListView_SetExtendedListViewStyle(h, style) (DWORD)EDITA_SEND(h, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, style)
#define ListView_SetExtendedListViewStyleEx(h, mask, style) (DWORD)EDITA_SEND(h, LVM_SETEXTENDEDLISTVIEWSTYLE, mask, style)
#define ListView_GetExtendedListViewStyle(h) (DWORD)EDITA_SEND(h, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0)
#define ListView_SetImageList(h, list, type) (void*)EDITA_SEND(h, LVM_SETIMAGELIST, type, list)

#define TreeView_InsertItem(h, is) (HTREEITEM)EDITA_SEND(h, TVM_INSERTITEM, 0, is)
#define TreeView_DeleteItem(h, item) (BOOL)EDITA_SEND(h, TVM_DELETEITEM, 0, item)
#define TreeView_DeleteAllItems(h) (BOOL)EDITA_SEND(h, TVM_DELETEITEM, 0, TVI_ROOT)
#define TreeView_Expand(h, item, code) (BOOL)EDITA_SEND(h, TVM_EXPAND, code, item)
/* El HTREEITEM va en los primeros bytes del RECT (como en Windows), sin romper el alias estricto */
static inline BOOL edita_TreeView_GetItemRect(HWND h, HTREEITEM item, RECT* rc, BOOL textOnly)
{
    __builtin_memcpy(rc, &item, sizeof(item));
    return (BOOL)EDITA_SEND(h, TVM_GETITEMRECT, textOnly, rc);
}
#define TreeView_GetItemRect(h, item, rc, textOnly) edita_TreeView_GetItemRect((h), (item), (rc), (textOnly))
#define TreeView_GetCount(h) (UINT)EDITA_SEND(h, TVM_GETCOUNT, 0, 0)
#define TreeView_GetNextItem(h, item, code) (HTREEITEM)EDITA_SEND(h, TVM_GETNEXTITEM, code, item)
#define TreeView_GetChild(h, item) TreeView_GetNextItem(h, item, TVGN_CHILD)
#define TreeView_GetNextSibling(h, item) TreeView_GetNextItem(h, item, TVGN_NEXT)
#define TreeView_GetPrevSibling(h, item) TreeView_GetNextItem(h, item, TVGN_PREVIOUS)
#define TreeView_GetParent(h, item) TreeView_GetNextItem(h, item, TVGN_PARENT)
#define TreeView_GetRoot(h) TreeView_GetNextItem(h, NULL, TVGN_ROOT)
#define TreeView_GetSelection(h) TreeView_GetNextItem(h, NULL, TVGN_CARET)
#define TreeView_GetFirstVisible(h) TreeView_GetNextItem(h, NULL, TVGN_FIRSTVISIBLE)
#define TreeView_GetNextVisible(h, item) TreeView_GetNextItem(h, item, TVGN_NEXTVISIBLE)
#define TreeView_Select(h, item, code) (BOOL)EDITA_SEND(h, TVM_SELECTITEM, code, item)
#define TreeView_SelectItem(h, item) TreeView_Select(h, item, TVGN_CARET)
#define TreeView_GetItem(h, item) (BOOL)EDITA_SEND(h, TVM_GETITEM, 0, item)
#define TreeView_SetItem(h, item) (BOOL)EDITA_SEND(h, TVM_SETITEM, 0, item)
#define TreeView_GetVisibleCount(h) (UINT)EDITA_SEND(h, TVM_GETVISIBLECOUNT, 0, 0)
#define TreeView_HitTest(h, info) (HTREEITEM)EDITA_SEND(h, TVM_HITTEST, 0, info)
#define TreeView_SortChildren(h, item, recurse) (BOOL)EDITA_SEND(h, TVM_SORTCHILDREN, recurse, item)
#define TreeView_EnsureVisible(h, item) (BOOL)EDITA_SEND(h, TVM_ENSUREVISIBLE, 0, item)
#define TreeView_GetItemState(h, item, mask) (UINT)EDITA_SEND(h, TVM_GETITEMSTATE, item, mask)
#define TreeView_SetBkColor(h, c) (COLORREF)EDITA_SEND(h, TVM_SETBKCOLOR, 0, c)
#define TreeView_SetTextColor(h, c) (COLORREF)EDITA_SEND(h, TVM_SETTEXTCOLOR, 0, c)
#define TreeView_SetImageList(h, list, type) (void*)EDITA_SEND(h, TVM_SETIMAGELIST, type, list)

/* --- Macros de windowsx.h ---------------------------------------------------------------- */
#define Edit_GetText(h, text, size) GetWindowText(h, text, size)
#define Edit_SetText(h, text) SetWindowText(h, text)
#define Edit_GetTextLength(h) GetWindowTextLength(h)
#define Edit_LimitText(h, n) (void)EDITA_SEND(h, EM_LIMITTEXT, n, 0)
#define Edit_SetSel(h, start, end) (void)EDITA_SEND(h, EM_SETSEL, start, end)
#define Edit_ReplaceSel(h, text) (void)EDITA_SEND(h, EM_REPLACESEL, 0, text)
#define Edit_SetReadOnly(h, ro) (BOOL)EDITA_SEND(h, EM_SETREADONLY, ro, 0)
#define Edit_GetLineCount(h) (int)EDITA_SEND(h, EM_GETLINECOUNT, 0, 0)
#define Edit_Enable(h, on) EnableWindow(h, on)
#define Button_GetCheck(h) (int)EDITA_SEND(h, BM_GETCHECK, 0, 0)
#define Button_SetCheck(h, check) (void)EDITA_SEND(h, BM_SETCHECK, check, 0)
#define Button_GetText(h, text, size) GetWindowText(h, text, size)
#define Button_SetText(h, text) SetWindowText(h, text)
#define Button_Enable(h, on) EnableWindow(h, on)
#define Static_SetText(h, text) SetWindowText(h, text)
#define Static_GetText(h, text, size) GetWindowText(h, text, size)
#define Static_Enable(h, on) EnableWindow(h, on)
#define ComboBox_AddString(h, s) (int)EDITA_SEND(h, CB_ADDSTRING, 0, s)
#define ComboBox_InsertString(h, i, s) (int)EDITA_SEND(h, CB_INSERTSTRING, i, s)
#define ComboBox_DeleteString(h, i) (int)EDITA_SEND(h, CB_DELETESTRING, i, 0)
#define ComboBox_ResetContent(h) (int)EDITA_SEND(h, CB_RESETCONTENT, 0, 0)
#define ComboBox_GetCount(h) (int)EDITA_SEND(h, CB_GETCOUNT, 0, 0)
#define ComboBox_GetCurSel(h) (int)EDITA_SEND(h, CB_GETCURSEL, 0, 0)
#define ComboBox_SetCurSel(h, i) (int)EDITA_SEND(h, CB_SETCURSEL, i, 0)
#define ComboBox_GetLBText(h, i, text) (int)EDITA_SEND(h, CB_GETLBTEXT, i, text)
#define ComboBox_GetLBTextLen(h, i) (int)EDITA_SEND(h, CB_GETLBTEXTLEN, i, 0)
#define ComboBox_FindString(h, start, s) (int)EDITA_SEND(h, CB_FINDSTRING, start, s)
#define ComboBox_FindStringExact(h, start, s) (int)EDITA_SEND(h, CB_FINDSTRINGEXACT, start, s)
#define ComboBox_SelectString(h, start, s) (int)EDITA_SEND(h, CB_SELECTSTRING, start, s)
#define ComboBox_SetItemData(h, i, data) (int)EDITA_SEND(h, CB_SETITEMDATA, i, data)
#define ComboBox_GetItemData(h, i) (LRESULT)EDITA_SEND(h, CB_GETITEMDATA, i, 0)
#define ComboBox_ShowDropdown(h, show) (BOOL)EDITA_SEND(h, CB_SHOWDROPDOWN, show, 0)
#define ComboBox_GetText(h, text, size) GetWindowText(h, text, size)
#define ComboBox_SetText(h, text) SetWindowText(h, text)
#define ComboBox_Enable(h, on) EnableWindow(h, on)
#define ListBox_AddString(h, s) (int)EDITA_SEND(h, LB_ADDSTRING, 0, s)
#define ListBox_InsertString(h, i, s) (int)EDITA_SEND(h, LB_INSERTSTRING, i, s)
#define ListBox_DeleteString(h, i) (int)EDITA_SEND(h, LB_DELETESTRING, i, 0)
#define ListBox_ResetContent(h) (BOOL)EDITA_SEND(h, LB_RESETCONTENT, 0, 0)
#define ListBox_GetCount(h) (int)EDITA_SEND(h, LB_GETCOUNT, 0, 0)
#define ListBox_GetCurSel(h) (int)EDITA_SEND(h, LB_GETCURSEL, 0, 0)
#define ListBox_SetCurSel(h, i) (int)EDITA_SEND(h, LB_SETCURSEL, i, 0)
#define ListBox_GetText(h, i, text) (int)EDITA_SEND(h, LB_GETTEXT, i, text)
#define ListBox_GetTextLen(h, i) (int)EDITA_SEND(h, LB_GETTEXTLEN, i, 0)
#define ListBox_SetItemData(h, i, data) (int)EDITA_SEND(h, LB_SETITEMDATA, i, data)
#define ListBox_GetItemData(h, i) (LRESULT)EDITA_SEND(h, LB_GETITEMDATA, i, 0)
#define ListBox_FindString(h, start, s) (int)EDITA_SEND(h, LB_FINDSTRING, start, s)
#define ListBox_FindStringExact(h, start, s) (int)EDITA_SEND(h, LB_FINDSTRINGEXACT, start, s)
#define ListBox_GetSelCount(h) (int)EDITA_SEND(h, LB_GETSELCOUNT, 0, 0)
#define ListBox_SetSel(h, select, i) (int)EDITA_SEND(h, LB_SETSEL, select, i)
#define ListBox_GetSel(h, i) (int)EDITA_SEND(h, LB_GETSEL, i, 0)
#define ListBox_Enable(h, on) EnableWindow(h, on)

#endif /* EDITA_NPP_COMPAT_COMCTL_H */
