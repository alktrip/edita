/* This file is part of Edita.
 * Copyright (C) 2026 Edita contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ventanas de Win32 para los plugins de Notepad++ (la incluye npp_compat.h; el código
 * está en npp_window.cpp). Un HWND de la capa es una ventana con su QWidget, su
 * procedimiento (WNDPROC o DLGPROC), sus datos (GWLP_USERDATA, DWLP_USER, propiedades)
 * y sus temporizadores, y los mensajes llegan como en Windows: SendMessage llama al
 * procedimiento, que deja lo que no trata a DefWindowProc. Hay:
 *   - controles estándar: BUTTON (botón, casilla, opción, grupo), EDIT, STATIC, LISTBOX
 *     y COMBOBOX, con sus mensajes (BM_*, EM_*, LB_*, CB_*) y sus notificaciones
 *     (WM_COMMAND con BN_CLICKED, EN_CHANGE, LBN_SELCHANGE, CBN_SELCHANGE…), "Scintilla"
 *     (un editor de Edita) y clases registradas (RegisterClass), con subclassing
 *     (SetWindowLongPtr(GWLP_WNDPROC)) que también ve el teclado;
 *   - diálogos: CreateDialogParam y DialogBoxParam con WM_INITDIALOG, Intro e Esc
 *     (IDOK/IDCANCEL), EndDialog y DefDlgProc. Los diálogos de los recursos .rc todavía
 *     no se leen: un diálogo sin plantilla queda vacío, con un aviso;
 *   - geometría, visibilidad, foco, texto, temporizadores, menús emergentes, monitores,
 *     métricas del sistema y una parte de GDI sin dibujo: fuentes, medidas de texto y DPI.
 * Las ventanas de Edita (nppData._nppHandle, los editores) también sirven donde se
 * pide un HWND para medir o como padre. No hay WM_PAINT ni dibujo (BeginPaint, FillRect…).
 * Las coordenadas son píxeles lógicos de Qt; los DPI son siempre 96.
 */

#ifndef EDITA_NPP_COMPAT_USER_H
#define EDITA_NPP_COMPAT_USER_H

/* --- Estilos ------------------------------------------------------------------------ */
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_CHILD 0x40000000L
#define WS_MINIMIZE 0x20000000L
#define WS_VISIBLE 0x10000000L
#define WS_DISABLED 0x08000000L
#define WS_CLIPSIBLINGS 0x04000000L
#define WS_CLIPCHILDREN 0x02000000L
#define WS_MAXIMIZE 0x01000000L
#define WS_CAPTION 0x00C00000L
#define WS_BORDER 0x00800000L
#define WS_DLGFRAME 0x00400000L
#define WS_VSCROLL 0x00200000L
#define WS_HSCROLL 0x00100000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_SIZEBOX WS_THICKFRAME
#define WS_GROUP 0x00020000L
#define WS_TABSTOP 0x00010000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_CHILDWINDOW WS_CHILD
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_POPUPWINDOW (WS_POPUP | WS_BORDER | WS_SYSMENU)
#define WS_EX_DLGMODALFRAME 0x00000001L
#define WS_EX_NOPARENTNOTIFY 0x00000004L
#define WS_EX_TOPMOST 0x00000008L
#define WS_EX_ACCEPTFILES 0x00000010L
#define WS_EX_TRANSPARENT 0x00000020L
#define WS_EX_TOOLWINDOW 0x00000080L
#define WS_EX_WINDOWEDGE 0x00000100L
#define WS_EX_CLIENTEDGE 0x00000200L
#define WS_EX_CONTEXTHELP 0x00000400L
#define WS_EX_RIGHT 0x00001000L
#define WS_EX_LEFT 0x00000000L
#define WS_EX_RTLREADING 0x00002000L
#define WS_EX_LEFTSCROLLBAR 0x00004000L
#define WS_EX_CONTROLPARENT 0x00010000L
#define WS_EX_STATICEDGE 0x00020000L
#define WS_EX_APPWINDOW 0x00040000L
#define WS_EX_LAYERED 0x00080000L
#define WS_EX_LAYOUTRTL 0x00400000L
#define WS_EX_COMPOSITED 0x02000000L
#define WS_EX_NOACTIVATE 0x08000000L
#define WS_EX_OVERLAPPEDWINDOW (WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE)

#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002
#define CS_DBLCLKS 0x0008
#define CS_OWNDC 0x0020
#define CS_CLASSDC 0x0040
#define CS_PARENTDC 0x0080
#define CS_NOCLOSE 0x0200
#define CS_SAVEBITS 0x0800
#define CS_GLOBALCLASS 0x4000
#define CS_DROPSHADOW 0x00020000
#define CW_USEDEFAULT ((int)0x80000000)

#define BS_PUSHBUTTON 0x00000000L
#define BS_DEFPUSHBUTTON 0x00000001L
#define BS_CHECKBOX 0x00000002L
#define BS_AUTOCHECKBOX 0x00000003L
#define BS_RADIOBUTTON 0x00000004L
#define BS_3STATE 0x00000005L
#define BS_AUTO3STATE 0x00000006L
#define BS_GROUPBOX 0x00000007L
#define BS_USERBUTTON 0x00000008L
#define BS_AUTORADIOBUTTON 0x00000009L
#define BS_PUSHBOX 0x0000000AL
#define BS_OWNERDRAW 0x0000000BL
#define BS_SPLITBUTTON 0x0000000CL
#define BS_TYPEMASK 0x0000000FL
#define BS_LEFTTEXT 0x00000020L
#define BS_TEXT 0x00000000L
#define BS_ICON 0x00000040L
#define BS_BITMAP 0x00000080L
#define BS_LEFT 0x00000100L
#define BS_RIGHT 0x00000200L
#define BS_CENTER 0x00000300L
#define BS_TOP 0x00000400L
#define BS_BOTTOM 0x00000800L
#define BS_VCENTER 0x00000C00L
#define BS_PUSHLIKE 0x00001000L
#define BS_MULTILINE 0x00002000L
#define BS_NOTIFY 0x00004000L
#define BS_FLAT 0x00008000L
#define ES_LEFT 0x0000L
#define ES_CENTER 0x0001L
#define ES_RIGHT 0x0002L
#define ES_MULTILINE 0x0004L
#define ES_UPPERCASE 0x0008L
#define ES_LOWERCASE 0x0010L
#define ES_PASSWORD 0x0020L
#define ES_AUTOVSCROLL 0x0040L
#define ES_AUTOHSCROLL 0x0080L
#define ES_NOHIDESEL 0x0100L
#define ES_OEMCONVERT 0x0400L
#define ES_READONLY 0x0800L
#define ES_WANTRETURN 0x1000L
#define ES_NUMBER 0x2000L
#define SS_LEFT 0x00000000L
#define SS_CENTER 0x00000001L
#define SS_RIGHT 0x00000002L
#define SS_ICON 0x00000003L
#define SS_BLACKRECT 0x00000004L
#define SS_GRAYRECT 0x00000005L
#define SS_WHITERECT 0x00000006L
#define SS_BLACKFRAME 0x00000007L
#define SS_GRAYFRAME 0x00000008L
#define SS_WHITEFRAME 0x00000009L
#define SS_SIMPLE 0x0000000BL
#define SS_LEFTNOWORDWRAP 0x0000000CL
#define SS_OWNERDRAW 0x0000000DL
#define SS_BITMAP 0x0000000EL
#define SS_ETCHEDHORZ 0x00000010L
#define SS_ETCHEDVERT 0x00000011L
#define SS_ETCHEDFRAME 0x00000012L
#define SS_TYPEMASK 0x0000001FL
#define SS_NOPREFIX 0x00000080L
#define SS_NOTIFY 0x00000100L
#define SS_CENTERIMAGE 0x00000200L
#define SS_SUNKEN 0x00001000L
#define SS_ENDELLIPSIS 0x00004000L
#define SS_PATHELLIPSIS 0x00008000L
#define SS_WORDELLIPSIS 0x0000C000L
#define LBS_NOTIFY 0x0001L
#define LBS_SORT 0x0002L
#define LBS_NOREDRAW 0x0004L
#define LBS_MULTIPLESEL 0x0008L
#define LBS_OWNERDRAWFIXED 0x0010L
#define LBS_HASSTRINGS 0x0040L
#define LBS_USETABSTOPS 0x0080L
#define LBS_NOINTEGRALHEIGHT 0x0100L
#define LBS_MULTICOLUMN 0x0200L
#define LBS_WANTKEYBOARDINPUT 0x0400L
#define LBS_EXTENDEDSEL 0x0800L
#define LBS_DISABLENOSCROLL 0x1000L
#define LBS_NODATA 0x2000L
#define LBS_NOSEL 0x4000L
#define LBS_STANDARD (LBS_NOTIFY | LBS_SORT | WS_VSCROLL | WS_BORDER)
#define CBS_SIMPLE 0x0001L
#define CBS_DROPDOWN 0x0002L
#define CBS_DROPDOWNLIST 0x0003L
#define CBS_OWNERDRAWFIXED 0x0010L
#define CBS_AUTOHSCROLL 0x0040L
#define CBS_OEMCONVERT 0x0080L
#define CBS_SORT 0x0100L
#define CBS_HASSTRINGS 0x0200L
#define CBS_NOINTEGRALHEIGHT 0x0400L
#define CBS_DISABLENOSCROLL 0x0800L
#define CBS_UPPERCASE 0x2000L
#define CBS_LOWERCASE 0x4000L
#define DS_3DLOOK 0x0004L
#define DS_FIXEDSYS 0x0008L
#define DS_SETFONT 0x0040L
#define DS_MODALFRAME 0x0080L
#define DS_CONTROL 0x0400L
#define DS_CENTER 0x0800L
#define DS_SHELLFONT (DS_SETFONT | DS_FIXEDSYS)

/* --- Mensajes ------------------------------------------------------------------------- */
#define WM_GETMINMAXINFO 0x0024
#define WM_NEXTDLGCTL 0x0028
#define WM_DRAWITEM 0x002B
#define WM_MEASUREITEM 0x002C
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED 0x0047
#define WM_COPYDATA 0x004A
#define WM_HELP 0x0053
#define WM_SYSCOLORCHANGE 0x0015
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR 0x0020
#define WM_MOUSEACTIVATE 0x0021
#define WM_NCCREATE 0x0081
#define WM_NCDESTROY 0x0082
#define WM_NCHITTEST 0x0084
#define WM_NCPAINT 0x0085
#define WM_GETDLGCODE 0x0087
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_SYSCHAR 0x0106
#define WM_INITMENUPOPUP 0x0117
#define WM_MENUSELECT 0x011F
#define WM_CTLCOLOREDIT 0x0133
#define WM_CTLCOLORLISTBOX 0x0134
#define WM_CTLCOLORBTN 0x0135
#define WM_CTLCOLORDLG 0x0136
#define WM_CTLCOLORSTATIC 0x0138
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MOUSEHWHEEL 0x020E
#define WM_SIZING 0x0214
#define WM_CAPTURECHANGED 0x0215
#define WM_MOVING 0x0216
#define WM_ENTERSIZEMOVE 0x0231
#define WM_EXITSIZEMOVE 0x0232
#define WM_MOUSELEAVE 0x02A3
#define WM_DPICHANGED 0x02E0
#define WM_PRINTCLIENT 0x0318
#define WM_THEMECHANGED 0x031A
#define SIZE_RESTORED 0
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2
#define WA_INACTIVE 0
#define WA_ACTIVE 1
#define WA_CLICKACTIVE 2
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_SHIFT 0x0004
#define MK_CONTROL 0x0008
#define MK_MBUTTON 0x0010
#define WHEEL_DELTA 120
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wp) ((short)HIWORD(wp))
#define GET_KEYSTATE_WPARAM(wp) (LOWORD(wp))
#define DLGC_WANTARROWS 0x0001
#define DLGC_WANTTAB 0x0002
#define DLGC_WANTALLKEYS 0x0004
#define DLGC_WANTMESSAGE 0x0004
#define DLGC_HASSETSEL 0x0008
#define DLGC_DEFPUSHBUTTON 0x0010
#define DLGC_UNDEFPUSHBUTTON 0x0020
#define DLGC_RADIOBUTTON 0x0040
#define DLGC_WANTCHARS 0x0080
#define DLGC_STATIC 0x0100
#define DLGC_BUTTON 0x2000

#define BM_GETCHECK 0x00F0
#define BM_SETCHECK 0x00F1
#define BM_GETSTATE 0x00F2
#define BM_SETSTATE 0x00F3
#define BM_SETSTYLE 0x00F4
#define BM_CLICK 0x00F5
#define BM_GETIMAGE 0x00F6
#define BM_SETIMAGE 0x00F7
#define BST_PUSHED 0x0004
#define BST_FOCUS 0x0008
#define BN_CLICKED 0
#define BN_PAINT 1
#define BN_DBLCLK 5
#define BN_SETFOCUS 6
#define BN_KILLFOCUS 7
#define EM_GETSEL 0x00B0
#define EM_SETSEL 0x00B1
#define EM_GETRECT 0x00B2
#define EM_SCROLL 0x00B5
#define EM_LINESCROLL 0x00B6
#define EM_SCROLLCARET 0x00B7
#define EM_GETMODIFY 0x00B8
#define EM_SETMODIFY 0x00B9
#define EM_GETLINECOUNT 0x00BA
#define EM_LINEINDEX 0x00BB
#define EM_LINELENGTH 0x00C1
#define EM_REPLACESEL 0x00C2
#define EM_GETLINE 0x00C4
#define EM_LIMITTEXT 0x00C5
#define EM_SETLIMITTEXT EM_LIMITTEXT
#define EM_CANUNDO 0x00C6
#define EM_UNDO 0x00C7
#define EM_LINEFROMCHAR 0x00C9
#define EM_SETPASSWORDCHAR 0x00CC
#define EM_EMPTYUNDOBUFFER 0x00CD
#define EM_SETREADONLY 0x00CF
#define EM_GETLIMITTEXT 0x00D5
#define EM_SETCUEBANNER 0x1501
#define EN_SETFOCUS 0x0100
#define EN_KILLFOCUS 0x0200
#define EN_CHANGE 0x0300
#define EN_UPDATE 0x0400
#define EN_MAXTEXT 0x0501
#define STM_SETICON 0x0170
#define STM_GETICON 0x0171
#define STM_SETIMAGE 0x0172
#define STM_GETIMAGE 0x0173
#define STN_CLICKED 0
#define STN_DBLCLK 1
#define LB_OKAY 0
#define LB_ERR (-1)
#define LB_ERRSPACE (-2)
#define LB_ADDSTRING 0x0180
#define LB_INSERTSTRING 0x0181
#define LB_DELETESTRING 0x0182
#define LB_RESETCONTENT 0x0184
#define LB_SETSEL 0x0185
#define LB_SETCURSEL 0x0186
#define LB_GETSEL 0x0187
#define LB_GETCURSEL 0x0188
#define LB_GETTEXT 0x0189
#define LB_GETTEXTLEN 0x018A
#define LB_GETCOUNT 0x018B
#define LB_SELECTSTRING 0x018C
#define LB_GETTOPINDEX 0x018E
#define LB_FINDSTRING 0x018F
#define LB_GETSELCOUNT 0x0190
#define LB_GETSELITEMS 0x0191
#define LB_SETHORIZONTALEXTENT 0x0194
#define LB_SETTOPINDEX 0x0197
#define LB_GETITEMDATA 0x0199
#define LB_SETITEMDATA 0x019A
#define LB_SETCARETINDEX 0x019E
#define LB_GETCARETINDEX 0x019F
#define LB_SETITEMHEIGHT 0x01A0
#define LB_GETITEMHEIGHT 0x01A1
#define LB_FINDSTRINGEXACT 0x01A2
#define LBN_ERRSPACE (-2)
#define LBN_SELCHANGE 1
#define LBN_DBLCLK 2
#define LBN_SELCANCEL 3
#define LBN_SETFOCUS 4
#define LBN_KILLFOCUS 5
#define CB_OKAY 0
#define CB_ERR (-1)
#define CB_ERRSPACE (-2)
#define CB_GETEDITSEL 0x0140
#define CB_LIMITTEXT 0x0141
#define CB_SETEDITSEL 0x0142
#define CB_ADDSTRING 0x0143
#define CB_DELETESTRING 0x0144
#define CB_GETCOUNT 0x0146
#define CB_GETCURSEL 0x0147
#define CB_GETLBTEXT 0x0148
#define CB_GETLBTEXTLEN 0x0149
#define CB_INSERTSTRING 0x014A
#define CB_RESETCONTENT 0x014B
#define CB_FINDSTRING 0x014C
#define CB_SELECTSTRING 0x014D
#define CB_SETCURSEL 0x014E
#define CB_SHOWDROPDOWN 0x014F
#define CB_GETITEMDATA 0x0150
#define CB_SETITEMDATA 0x0151
#define CB_GETDROPPEDCONTROLRECT 0x0152
#define CB_SETITEMHEIGHT 0x0153
#define CB_GETITEMHEIGHT 0x0154
#define CB_SETEXTENDEDUI 0x0155
#define CB_GETDROPPEDSTATE 0x0157
#define CB_FINDSTRINGEXACT 0x0158
#define CB_SETDROPPEDWIDTH 0x0160
#define CB_SETMINVISIBLE 0x1701
#define CBN_ERRSPACE (-1)
#define CBN_SELCHANGE 1
#define CBN_DBLCLK 2
#define CBN_SETFOCUS 3
#define CBN_KILLFOCUS 4
#define CBN_EDITCHANGE 5
#define CBN_EDITUPDATE 6
#define CBN_DROPDOWN 7
#define CBN_CLOSEUP 8
#define CBN_SELENDOK 9
#define CBN_SELENDCANCEL 10

/* --- Índices y consultas -------------------------------------------------------------- */
#define GWLP_WNDPROC (-4)
#define GWLP_HINSTANCE (-6)
#define GWLP_HWNDPARENT (-8)
#define GWLP_ID (-12)
#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define GWLP_USERDATA (-21)
#define GWL_WNDPROC GWLP_WNDPROC
#define GWL_HINSTANCE GWLP_HINSTANCE
#define GWL_ID GWLP_ID
#define GWL_USERDATA GWLP_USERDATA
#define DWLP_MSGRESULT 0
#define DWLP_DLGPROC 8
#define DWLP_USER 16
#define DWL_MSGRESULT DWLP_MSGRESULT
#define DWL_DLGPROC DWLP_DLGPROC
#define DWL_USER DWLP_USER
#define GW_HWNDFIRST 0
#define GW_HWNDLAST 1
#define GW_HWNDNEXT 2
#define GW_HWNDPREV 3
#define GW_OWNER 4
#define GW_CHILD 5
#define GA_PARENT 1
#define GA_ROOT 2
#define GA_ROOTOWNER 3
#define SWP_NOOWNERZORDER 0x0200
#define SWP_NOREPOSITION SWP_NOOWNERZORDER
#define SWP_NOSENDCHANGING 0x0400
#define SWP_ASYNCWINDOWPOS 0x4000
#define HWND_BOTTOM ((HWND)1)
#define HWND_TOPMOST ((HWND)-1)
#define HWND_NOTOPMOST ((HWND)-2)
#define HWND_DESKTOP ((HWND)0)
#define HWND_MESSAGE ((HWND)-3)
#define RDW_INVALIDATE 0x0001
#define RDW_INTERNALPAINT 0x0002
#define RDW_ERASE 0x0004
#define RDW_VALIDATE 0x0008
#define RDW_NOINTERNALPAINT 0x0010
#define RDW_NOERASE 0x0020
#define RDW_NOCHILDREN 0x0040
#define RDW_ALLCHILDREN 0x0080
#define RDW_UPDATENOW 0x0100
#define RDW_ERASENOW 0x0200
#define RDW_FRAME 0x0400
#define RDW_NOFRAME 0x0800

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CXVSCROLL 2
#define SM_CYHSCROLL 3
#define SM_CYCAPTION 4
#define SM_CXBORDER 5
#define SM_CYBORDER 6
#define SM_CXDLGFRAME 7
#define SM_CYDLGFRAME 8
#define SM_CXFIXEDFRAME SM_CXDLGFRAME
#define SM_CYFIXEDFRAME SM_CYDLGFRAME
#define SM_CYVTHUMB 9
#define SM_CXHTHUMB 10
#define SM_CXICON 11
#define SM_CYICON 12
#define SM_CXCURSOR 13
#define SM_CYCURSOR 14
#define SM_CYMENU 15
#define SM_CXFULLSCREEN 16
#define SM_CYFULLSCREEN 17
#define SM_CYVSCROLL 20
#define SM_CXHSCROLL 21
#define SM_SWAPBUTTON 23
#define SM_CXMIN 28
#define SM_CYMIN 29
#define SM_CXSIZE 30
#define SM_CYSIZE 31
#define SM_CXFRAME 32
#define SM_CYFRAME 33
#define SM_CXSIZEFRAME SM_CXFRAME
#define SM_CYSIZEFRAME SM_CYFRAME
#define SM_CXMINTRACK 34
#define SM_CYMINTRACK 35
#define SM_CXDOUBLECLK 36
#define SM_CYDOUBLECLK 37
#define SM_CXEDGE 45
#define SM_CYEDGE 46
#define SM_CXSMICON 49
#define SM_CYSMICON 50
#define SM_CYSMCAPTION 51
#define SM_CXMAXIMIZED 61
#define SM_CYMAXIMIZED 62
#define SM_CXMENUCHECK 71
#define SM_CYMENUCHECK 72
#define SM_MOUSEWHEELPRESENT 75
#define SM_XVIRTUALSCREEN 76
#define SM_YVIRTUALSCREEN 77
#define SM_CXVIRTUALSCREEN 78
#define SM_CYVIRTUALSCREEN 79
#define SM_CMONITORS 80
#define SM_CXPADDEDBORDER 92
#define SM_REMOTESESSION 0x1000
#define SPI_GETICONTITLELOGFONT 0x001F
#define SPI_GETNONCLIENTMETRICS 0x0029
#define SPI_GETWORKAREA 0x0030
#define SPI_GETHIGHCONTRAST 0x0042
#define SPI_GETFONTSMOOTHING 0x004A
#define SPI_GETWHEELSCROLLLINES 0x0068
#define MONITOR_DEFAULTTONULL 0x00000000
#define MONITOR_DEFAULTTOPRIMARY 0x00000001
#define MONITOR_DEFAULTTONEAREST 0x00000002
#define MONITORINFOF_PRIMARY 0x00000001
#define CCHDEVICENAME 32
#define USER_DEFAULT_SCREEN_DPI 96

/* --- Recursos, cursores e iconos --------------------------------------------------- */
#define RT_CURSOR MAKEINTRESOURCE(1)
#define RT_BITMAP MAKEINTRESOURCE(2)
#define RT_ICON MAKEINTRESOURCE(3)
#define RT_MENU MAKEINTRESOURCE(4)
#define RT_DIALOG MAKEINTRESOURCE(5)
#define RT_STRING MAKEINTRESOURCE(6)
#define RT_RCDATA MAKEINTRESOURCE(10)
#define RT_GROUP_CURSOR MAKEINTRESOURCE(12)
#define RT_GROUP_ICON MAKEINTRESOURCE(14)
#define RT_VERSION MAKEINTRESOURCE(16)
#define IS_INTRESOURCE(r) ((((ULONG_PTR)(r)) >> 16) == 0)
#define MAKEINTATOM(i) ((LPTSTR)((ULONG_PTR)((WORD)(i))))
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDC_IBEAM MAKEINTRESOURCE(32513)
#define IDC_WAIT MAKEINTRESOURCE(32514)
#define IDC_CROSS MAKEINTRESOURCE(32515)
#define IDC_UPARROW MAKEINTRESOURCE(32516)
#define IDC_SIZENWSE MAKEINTRESOURCE(32642)
#define IDC_SIZENESW MAKEINTRESOURCE(32643)
#define IDC_SIZEWE MAKEINTRESOURCE(32644)
#define IDC_SIZENS MAKEINTRESOURCE(32645)
#define IDC_SIZEALL MAKEINTRESOURCE(32646)
#define IDC_NO MAKEINTRESOURCE(32648)
#define IDC_HAND MAKEINTRESOURCE(32649)
#define IDC_APPSTARTING MAKEINTRESOURCE(32650)
#define IDI_APPLICATION MAKEINTRESOURCE(32512)
#define IDI_ERROR MAKEINTRESOURCE(32513)
#define IDI_QUESTION MAKEINTRESOURCE(32514)
#define IDI_WARNING MAKEINTRESOURCE(32515)
#define IDI_INFORMATION MAKEINTRESOURCE(32516)
#define IDI_HAND IDI_ERROR
#define IDI_EXCLAMATION IDI_WARNING
#define IDI_ASTERISK IDI_INFORMATION
#define IMAGE_BITMAP 0
#define IMAGE_ICON 1
#define IMAGE_CURSOR 2
#define LR_DEFAULTCOLOR 0x00000000
#define LR_MONOCHROME 0x00000001
#define LR_LOADFROMFILE 0x00000010
#define LR_LOADTRANSPARENT 0x00000020
#define LR_DEFAULTSIZE 0x00000040
#define LR_LOADMAP3DCOLORS 0x00001000
#define LR_CREATEDIBSECTION 0x00002000
#define LR_SHARED 0x00008000

/* --- Menús --------------------------------------------------------------------------- */
#define MF_INSERT 0x00000000L
#define MF_CHANGE 0x00000080L
#define MF_APPEND 0x00000100L
#define MF_DELETE 0x00000200L
#define MF_REMOVE 0x00001000L
#define MF_BYCOMMAND 0x00000000L
#define MF_BYPOSITION 0x00000400L
#define MF_SEPARATOR 0x00000800L
#define MF_ENABLED 0x00000000L
#define MF_GRAYED 0x00000001L
#define MF_DISABLED 0x00000002L
#define MF_UNCHECKED 0x00000000L
#define MF_CHECKED 0x00000008L
#define MF_STRING 0x00000000L
#define MF_BITMAP 0x00000004L
#define MF_OWNERDRAW 0x00000100L
#define MF_POPUP 0x00000010L
#define MF_MENUBARBREAK 0x00000020L
#define MF_MENUBREAK 0x00000040L
#define MF_DEFAULT 0x00001000L
#define MFS_GRAYED 0x00000003L
#define MFS_DISABLED MFS_GRAYED
#define MFS_CHECKED MF_CHECKED
#define MFS_ENABLED MF_ENABLED
#define MFS_UNCHECKED MF_UNCHECKED
#define TPM_LEFTBUTTON 0x0000L
#define TPM_RIGHTBUTTON 0x0002L
#define TPM_LEFTALIGN 0x0000L
#define TPM_CENTERALIGN 0x0004L
#define TPM_RIGHTALIGN 0x0008L
#define TPM_TOPALIGN 0x0000L
#define TPM_VCENTERALIGN 0x0010L
#define TPM_BOTTOMALIGN 0x0020L
#define TPM_NONOTIFY 0x0080L
#define TPM_RETURNCMD 0x0100L

/* --- GDI sin dibujo: fuentes, colores del sistema, medidas -------------------------- */
#define LF_FACESIZE 32
#define FW_DONTCARE 0
#define FW_THIN 100
#define FW_EXTRALIGHT 200
#define FW_LIGHT 300
#define FW_NORMAL 400
#define FW_REGULAR FW_NORMAL
#define FW_MEDIUM 500
#define FW_SEMIBOLD 600
#define FW_DEMIBOLD FW_SEMIBOLD
#define FW_BOLD 700
#define FW_EXTRABOLD 800
#define FW_HEAVY 900
#define FW_BLACK FW_HEAVY
#define ANSI_CHARSET 0
#define DEFAULT_CHARSET 1
#define SYMBOL_CHARSET 2
#define OEM_CHARSET 255
#define OUT_DEFAULT_PRECIS 0
#define OUT_TT_PRECIS 4
#define OUT_OUTLINE_PRECIS 8
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define DRAFT_QUALITY 1
#define PROOF_QUALITY 2
#define NONANTIALIASED_QUALITY 3
#define ANTIALIASED_QUALITY 4
#define CLEARTYPE_QUALITY 5
#define DEFAULT_PITCH 0
#define FIXED_PITCH 1
#define VARIABLE_PITCH 2
#define FF_DONTCARE 0x00
#define FF_ROMAN 0x10
#define FF_SWISS 0x20
#define FF_MODERN 0x30
#define FF_SCRIPT 0x40
#define FF_DECORATIVE 0x50
#define WHITE_BRUSH 0
#define LTGRAY_BRUSH 1
#define GRAY_BRUSH 2
#define DKGRAY_BRUSH 3
#define BLACK_BRUSH 4
#define NULL_BRUSH 5
#define HOLLOW_BRUSH NULL_BRUSH
#define WHITE_PEN 6
#define BLACK_PEN 7
#define NULL_PEN 8
#define OEM_FIXED_FONT 10
#define ANSI_FIXED_FONT 11
#define ANSI_VAR_FONT 12
#define SYSTEM_FONT 13
#define DEVICE_DEFAULT_FONT 14
#define SYSTEM_FIXED_FONT 16
#define DEFAULT_GUI_FONT 17
#define DC_BRUSH 18
#define DC_PEN 19
#define PS_SOLID 0
#define PS_DASH 1
#define PS_DOT 2
#define PS_NULL 5
#define TRANSPARENT 1
#define OPAQUE 2
#define CLR_INVALID 0xFFFFFFFF
#define HORZRES 8
#define VERTRES 10
#define BITSPIXEL 12
#define LOGPIXELSX 88
#define LOGPIXELSY 90
#define COLOR_SCROLLBAR 0
#define COLOR_BACKGROUND 1
#define COLOR_DESKTOP COLOR_BACKGROUND
#define COLOR_ACTIVECAPTION 2
#define COLOR_INACTIVECAPTION 3
#define COLOR_MENU 4
#define COLOR_WINDOW 5
#define COLOR_WINDOWFRAME 6
#define COLOR_MENUTEXT 7
#define COLOR_WINDOWTEXT 8
#define COLOR_CAPTIONTEXT 9
#define COLOR_ACTIVEBORDER 10
#define COLOR_INACTIVEBORDER 11
#define COLOR_APPWORKSPACE 12
#define COLOR_HIGHLIGHT 13
#define COLOR_HIGHLIGHTTEXT 14
#define COLOR_BTNFACE 15
#define COLOR_3DFACE COLOR_BTNFACE
#define COLOR_BTNSHADOW 16
#define COLOR_3DSHADOW COLOR_BTNSHADOW
#define COLOR_GRAYTEXT 17
#define COLOR_BTNTEXT 18
#define COLOR_INACTIVECAPTIONTEXT 19
#define COLOR_BTNHIGHLIGHT 20
#define COLOR_3DHIGHLIGHT COLOR_BTNHIGHLIGHT
#define COLOR_3DDKSHADOW 21
#define COLOR_3DLIGHT 22
#define COLOR_INFOTEXT 23
#define COLOR_INFOBK 24
#define COLOR_HOTLIGHT 26
#define COLOR_GRADIENTACTIVECAPTION 27
#define COLOR_GRADIENTINACTIVECAPTION 28
#define COLOR_MENUHILIGHT 29
#define COLOR_MENUBAR 30

/* --- Estructuras ------------------------------------------------------------------- */
typedef void (*TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
typedef BOOL (*WNDENUMPROC)(HWND, LPARAM);
typedef void* HMONITOR;

typedef struct tagMSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG, *LPMSG, *PMSG;
typedef struct tagCREATESTRUCTA {
    LPVOID lpCreateParams; HINSTANCE hInstance; HMENU hMenu; HWND hwndParent;
    int cy; int cx; int y; int x; LONG style; LPCSTR lpszName; LPCSTR lpszClass; DWORD dwExStyle;
} CREATESTRUCTA, *LPCREATESTRUCTA;
typedef struct tagCREATESTRUCTW {
    LPVOID lpCreateParams; HINSTANCE hInstance; HMENU hMenu; HWND hwndParent;
    int cy; int cx; int y; int x; LONG style; LPCWSTR lpszName; LPCWSTR lpszClass; DWORD dwExStyle;
} CREATESTRUCTW, *LPCREATESTRUCTW;
typedef struct tagWNDCLASSA {
    UINT style; WNDPROC lpfnWndProc; int cbClsExtra; int cbWndExtra; HINSTANCE hInstance;
    HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; LPCSTR lpszMenuName; LPCSTR lpszClassName;
} WNDCLASSA, *LPWNDCLASSA;
typedef struct tagWNDCLASSW {
    UINT style; WNDPROC lpfnWndProc; int cbClsExtra; int cbWndExtra; HINSTANCE hInstance;
    HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; LPCWSTR lpszMenuName; LPCWSTR lpszClassName;
} WNDCLASSW, *LPWNDCLASSW;
typedef struct tagWNDCLASSEXA {
    UINT cbSize; UINT style; WNDPROC lpfnWndProc; int cbClsExtra; int cbWndExtra; HINSTANCE hInstance;
    HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; LPCSTR lpszMenuName; LPCSTR lpszClassName; HICON hIconSm;
} WNDCLASSEXA, *LPWNDCLASSEXA;
typedef struct tagWNDCLASSEXW {
    UINT cbSize; UINT style; WNDPROC lpfnWndProc; int cbClsExtra; int cbWndExtra; HINSTANCE hInstance;
    HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; LPCWSTR lpszMenuName; LPCWSTR lpszClassName; HICON hIconSm;
} WNDCLASSEXW, *LPWNDCLASSEXW;
typedef struct tagWINDOWPOS { HWND hwnd; HWND hwndInsertAfter; int x; int y; int cx; int cy; UINT flags; } WINDOWPOS, *LPWINDOWPOS;
typedef struct tagMINMAXINFO { POINT ptReserved; POINT ptMaxSize; POINT ptMaxPosition; POINT ptMinTrackSize; POINT ptMaxTrackSize; } MINMAXINFO, *LPMINMAXINFO;
typedef struct tagMONITORINFO { DWORD cbSize; RECT rcMonitor; RECT rcWork; DWORD dwFlags; } MONITORINFO, *LPMONITORINFO;
#ifdef __cplusplus
/* Como en el SDK de Windows: en C++, MONITORINFOEX deriva de MONITORINFO */
typedef struct tagMONITORINFOEXA : public tagMONITORINFO { CHAR szDevice[CCHDEVICENAME]; } MONITORINFOEXA, *LPMONITORINFOEXA;
typedef struct tagMONITORINFOEXW : public tagMONITORINFO { WCHAR szDevice[CCHDEVICENAME]; } MONITORINFOEXW, *LPMONITORINFOEXW;
#else
typedef struct tagMONITORINFOEXA { DWORD cbSize; RECT rcMonitor; RECT rcWork; DWORD dwFlags; CHAR szDevice[CCHDEVICENAME]; } MONITORINFOEXA, *LPMONITORINFOEXA;
typedef struct tagMONITORINFOEXW { DWORD cbSize; RECT rcMonitor; RECT rcWork; DWORD dwFlags; WCHAR szDevice[CCHDEVICENAME]; } MONITORINFOEXW, *LPMONITORINFOEXW;
#endif
typedef struct tagLOGFONTA {
    LONG lfHeight; LONG lfWidth; LONG lfEscapement; LONG lfOrientation; LONG lfWeight;
    BYTE lfItalic; BYTE lfUnderline; BYTE lfStrikeOut; BYTE lfCharSet; BYTE lfOutPrecision;
    BYTE lfClipPrecision; BYTE lfQuality; BYTE lfPitchAndFamily; CHAR lfFaceName[LF_FACESIZE];
} LOGFONTA, *LPLOGFONTA, *PLOGFONTA;
typedef struct tagLOGFONTW {
    LONG lfHeight; LONG lfWidth; LONG lfEscapement; LONG lfOrientation; LONG lfWeight;
    BYTE lfItalic; BYTE lfUnderline; BYTE lfStrikeOut; BYTE lfCharSet; BYTE lfOutPrecision;
    BYTE lfClipPrecision; BYTE lfQuality; BYTE lfPitchAndFamily; WCHAR lfFaceName[LF_FACESIZE];
} LOGFONTW, *LPLOGFONTW, *PLOGFONTW;
typedef struct tagTEXTMETRICA {
    LONG tmHeight; LONG tmAscent; LONG tmDescent; LONG tmInternalLeading; LONG tmExternalLeading;
    LONG tmAveCharWidth; LONG tmMaxCharWidth; LONG tmWeight; LONG tmOverhang; LONG tmDigitizedAspectX;
    LONG tmDigitizedAspectY; BYTE tmFirstChar; BYTE tmLastChar; BYTE tmDefaultChar; BYTE tmBreakChar;
    BYTE tmItalic; BYTE tmUnderlined; BYTE tmStruckOut; BYTE tmPitchAndFamily; BYTE tmCharSet;
} TEXTMETRICA, *LPTEXTMETRICA;
typedef struct tagTEXTMETRICW {
    LONG tmHeight; LONG tmAscent; LONG tmDescent; LONG tmInternalLeading; LONG tmExternalLeading;
    LONG tmAveCharWidth; LONG tmMaxCharWidth; LONG tmWeight; LONG tmOverhang; LONG tmDigitizedAspectX;
    LONG tmDigitizedAspectY; WCHAR tmFirstChar; WCHAR tmLastChar; WCHAR tmDefaultChar; WCHAR tmBreakChar;
    BYTE tmItalic; BYTE tmUnderlined; BYTE tmStruckOut; BYTE tmPitchAndFamily; BYTE tmCharSet;
} TEXTMETRICW, *LPTEXTMETRICW;
typedef struct tagNONCLIENTMETRICSW {
    UINT cbSize; int iBorderWidth; int iScrollWidth; int iScrollHeight; int iCaptionWidth; int iCaptionHeight;
    LOGFONTW lfCaptionFont; int iSmCaptionWidth; int iSmCaptionHeight; LOGFONTW lfSmCaptionFont;
    int iMenuWidth; int iMenuHeight; LOGFONTW lfMenuFont; LOGFONTW lfStatusFont; LOGFONTW lfMessageFont;
    int iPaddedBorderWidth;
} NONCLIENTMETRICSW, *LPNONCLIENTMETRICSW;
typedef struct tagNONCLIENTMETRICSA {
    UINT cbSize; int iBorderWidth; int iScrollWidth; int iScrollHeight; int iCaptionWidth; int iCaptionHeight;
    LOGFONTA lfCaptionFont; int iSmCaptionWidth; int iSmCaptionHeight; LOGFONTA lfSmCaptionFont;
    int iMenuWidth; int iMenuHeight; LOGFONTA lfMenuFont; LOGFONTA lfStatusFont; LOGFONTA lfMessageFont;
    int iPaddedBorderWidth;
} NONCLIENTMETRICSA, *LPNONCLIENTMETRICSA;
#pragma pack(push, 2)
typedef struct { DWORD style; DWORD dwExtendedStyle; WORD cdit; short x; short y; short cx; short cy; } DLGTEMPLATE, *LPDLGTEMPLATE;
typedef const DLGTEMPLATE* LPCDLGTEMPLATE;
typedef struct { DWORD style; DWORD dwExtendedStyle; short x; short y; short cx; short cy; WORD id; } DLGITEMTEMPLATE, *LPDLGITEMTEMPLATE;
#pragma pack(pop)
#ifdef UNICODE
typedef CREATESTRUCTW CREATESTRUCT;
typedef LPCREATESTRUCTW LPCREATESTRUCT;
typedef WNDCLASSW WNDCLASS;
typedef WNDCLASSEXW WNDCLASSEX;
typedef MONITORINFOEXW MONITORINFOEX;
typedef LPMONITORINFOEXW LPMONITORINFOEX;
typedef LOGFONTW LOGFONT;
typedef LPLOGFONTW LPLOGFONT;
typedef TEXTMETRICW TEXTMETRIC;
typedef LPTEXTMETRICW LPTEXTMETRIC;
typedef NONCLIENTMETRICSW NONCLIENTMETRICS;
#else
typedef CREATESTRUCTA CREATESTRUCT;
typedef LPCREATESTRUCTA LPCREATESTRUCT;
typedef WNDCLASSA WNDCLASS;
typedef WNDCLASSEXA WNDCLASSEX;
typedef MONITORINFOEXA MONITORINFOEX;
typedef LPMONITORINFOEXA LPMONITORINFOEX;
typedef LOGFONTA LOGFONT;
typedef LPLOGFONTA LPLOGFONT;
typedef TEXTMETRICA TEXTMETRIC;
typedef LPTEXTMETRICA LPTEXTMETRIC;
typedef NONCLIENTMETRICSA NONCLIENTMETRICS;
#endif

/* --- Funciones (npp_window.cpp) ------------------------------------------------------ */
EDITA_COMPAT_BEGIN

/* Mensajes a las ventanas de la capa (SendMessage llega aquí si el HWND es de la capa) */
BOOL edita_compat_isWindow(HWND hwnd);
LRESULT edita_compat_windowSend(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void edita_compat_windowPost(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
/* Enlace con npp_compat.cpp (no son para el plugin) */
void edita_compat_registerForeign(void* widget, HWND parent, int id); /* una ventana de Edita (QWidget*) */
void* edita_compat_widget(HWND hwnd);                                 /* el QWidget* de un HWND */
HWND edita_compat_hwndOfWidget(void* widget);
HMENU edita_compat_hostMenu(void);                                    /* el menú de Edita */
HWND edita_compat_hostHwnd(void);                                     /* nppData._nppHandle */
BOOL edita_compat_PostMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

/* Clases y ventanas */
ATOM RegisterClassA(const WNDCLASSA* wc);
ATOM RegisterClassW(const WNDCLASSW* wc);
ATOM RegisterClassExA(const WNDCLASSEXA* wc);
ATOM RegisterClassExW(const WNDCLASSEXW* wc);
BOOL UnregisterClassA(LPCSTR name, HINSTANCE instance);
BOOL UnregisterClassW(LPCWSTR name, HINSTANCE instance);
int GetClassNameA(HWND hwnd, LPSTR name, int size);
int GetClassNameW(HWND hwnd, LPWSTR name, int size);
HWND CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR windowName, DWORD style, int x, int y, int width, int height,
                     HWND parent, HMENU menu, HINSTANCE instance, LPVOID param);
HWND CreateWindowExW(DWORD exStyle, LPCWSTR className, LPCWSTR windowName, DWORD style, int x, int y, int width, int height,
                     HWND parent, HMENU menu, HINSTANCE instance, LPVOID param);
BOOL DestroyWindow(HWND hwnd);
LRESULT DefWindowProcA(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT DefWindowProcW(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT DefDlgProcA(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT DefDlgProcW(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CallWindowProcA(WNDPROC proc, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CallWindowProcW(WNDPROC proc, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LONG_PTR GetWindowLongPtrA(HWND hwnd, int index);
LONG_PTR GetWindowLongPtrW(HWND hwnd, int index);
LONG_PTR SetWindowLongPtrA(HWND hwnd, int index, LONG_PTR value);
LONG_PTR SetWindowLongPtrW(HWND hwnd, int index, LONG_PTR value);
LONG GetWindowLongA(HWND hwnd, int index);
LONG GetWindowLongW(HWND hwnd, int index);
LONG SetWindowLongA(HWND hwnd, int index, LONG value);
LONG SetWindowLongW(HWND hwnd, int index, LONG value);
BOOL SetPropA(HWND hwnd, LPCSTR name, HANDLE data);
BOOL SetPropW(HWND hwnd, LPCWSTR name, HANDLE data);
HANDLE GetPropA(HWND hwnd, LPCSTR name);
HANDLE GetPropW(HWND hwnd, LPCWSTR name);
HANDLE RemovePropA(HWND hwnd, LPCSTR name);
HANDLE RemovePropW(HWND hwnd, LPCWSTR name);

/* Diálogos (los de los .rc todavía no existen: el diálogo queda vacío) */
HWND CreateDialogIndirectParamA(HINSTANCE instance, LPCDLGTEMPLATE dialogTemplate, HWND parent, DLGPROC proc, LPARAM init);
HWND CreateDialogIndirectParamW(HINSTANCE instance, LPCDLGTEMPLATE dialogTemplate, HWND parent, DLGPROC proc, LPARAM init);
INT_PTR DialogBoxIndirectParamA(HINSTANCE instance, LPCDLGTEMPLATE dialogTemplate, HWND parent, DLGPROC proc, LPARAM init);
INT_PTR DialogBoxIndirectParamW(HINSTANCE instance, LPCDLGTEMPLATE dialogTemplate, HWND parent, DLGPROC proc, LPARAM init);
BOOL EndDialog(HWND hwnd, INT_PTR result);
BOOL IsDialogMessageA(HWND hwnd, LPMSG msg);
BOOL IsDialogMessageW(HWND hwnd, LPMSG msg);
HWND GetDlgItem(HWND dialog, int id);
int GetDlgCtrlID(HWND hwnd);
BOOL SetDlgItemTextA(HWND dialog, int id, LPCSTR text);
BOOL SetDlgItemTextW(HWND dialog, int id, LPCWSTR text);
UINT GetDlgItemTextA(HWND dialog, int id, LPSTR text, int size);
UINT GetDlgItemTextW(HWND dialog, int id, LPWSTR text, int size);
BOOL SetDlgItemInt(HWND dialog, int id, UINT value, BOOL isSigned);
UINT GetDlgItemInt(HWND dialog, int id, BOOL* translated, BOOL isSigned);
BOOL CheckDlgButton(HWND dialog, int id, UINT check);
UINT IsDlgButtonChecked(HWND dialog, int id);
BOOL CheckRadioButton(HWND dialog, int first, int last, int check);
LRESULT SendDlgItemMessageA(HWND dialog, int id, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT SendDlgItemMessageW(HWND dialog, int id, UINT message, WPARAM wParam, LPARAM lParam);

/* Texto */
BOOL SetWindowTextA(HWND hwnd, LPCSTR text);
BOOL SetWindowTextW(HWND hwnd, LPCWSTR text);
int GetWindowTextA(HWND hwnd, LPSTR text, int size);
int GetWindowTextW(HWND hwnd, LPWSTR text, int size);
int GetWindowTextLengthA(HWND hwnd);
int GetWindowTextLengthW(HWND hwnd);

/* Geometría, visibilidad, jerarquía y foco */
BOOL ShowWindow(HWND hwnd, int command);
BOOL UpdateWindow(HWND hwnd);
BOOL InvalidateRect(HWND hwnd, const RECT* rect, BOOL erase);
BOOL RedrawWindow(HWND hwnd, const RECT* rect, void* region, UINT flags);
BOOL MoveWindow(HWND hwnd, int x, int y, int width, int height, BOOL repaint);
BOOL SetWindowPos(HWND hwnd, HWND after, int x, int y, int width, int height, UINT flags);
BOOL GetWindowRect(HWND hwnd, LPRECT rect);
BOOL GetClientRect(HWND hwnd, LPRECT rect);
BOOL ClientToScreen(HWND hwnd, LPPOINT point);
BOOL ScreenToClient(HWND hwnd, LPPOINT point);
int MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count);
BOOL AdjustWindowRect(LPRECT rect, DWORD style, BOOL menu);
BOOL AdjustWindowRectEx(LPRECT rect, DWORD style, BOOL menu, DWORD exStyle);
HWND GetParent(HWND hwnd);
HWND SetParent(HWND hwnd, HWND parent);
HWND GetAncestor(HWND hwnd, UINT flags);
HWND GetWindow(HWND hwnd, UINT command);
BOOL EnumChildWindows(HWND parent, WNDENUMPROC proc, LPARAM lParam);
BOOL IsWindow(HWND hwnd);
BOOL IsWindowVisible(HWND hwnd);
BOOL IsWindowEnabled(HWND hwnd);
BOOL EnableWindow(HWND hwnd, BOOL enable);
BOOL IsChild(HWND parent, HWND hwnd);
BOOL IsIconic(HWND hwnd);
BOOL IsZoomed(HWND hwnd);
HWND SetFocus(HWND hwnd);
HWND GetFocus(void);
HWND GetActiveWindow(void);
HWND SetActiveWindow(HWND hwnd);
HWND GetForegroundWindow(void);
BOOL SetForegroundWindow(HWND hwnd);
BOOL BringWindowToTop(HWND hwnd);
HWND GetDesktopWindow(void);
HWND SetCapture(HWND hwnd);
BOOL ReleaseCapture(void);
HWND GetCapture(void);

/* Temporizadores */
UINT_PTR SetTimer(HWND hwnd, UINT_PTR id, UINT elapse, TIMERPROC proc);
BOOL KillTimer(HWND hwnd, UINT_PTR id);

/* Sistema: pantalla, cursor, monitores, DPI */
int GetSystemMetrics(int index);
BOOL SystemParametersInfoA(UINT action, UINT param, PVOID data, UINT winIni);
BOOL SystemParametersInfoW(UINT action, UINT param, PVOID data, UINT winIni);
HMONITOR MonitorFromWindow(HWND hwnd, DWORD flags);
HMONITOR MonitorFromRect(LPCRECT rect, DWORD flags);
HMONITOR MonitorFromPoint(POINT point, DWORD flags);
BOOL GetMonitorInfoA(HMONITOR monitor, LPMONITORINFO info);
BOOL GetMonitorInfoW(HMONITOR monitor, LPMONITORINFO info);
UINT GetDpiForWindow(HWND hwnd);
UINT GetDpiForSystem(void);
BOOL GetCursorPos(LPPOINT point);
BOOL SetCursorPos(int x, int y);
HCURSOR SetCursor(HCURSOR cursor);
HCURSOR LoadCursorA(HINSTANCE instance, LPCSTR name);
HCURSOR LoadCursorW(HINSTANCE instance, LPCWSTR name);
HICON LoadIconA(HINSTANCE instance, LPCSTR name);
HICON LoadIconW(HINSTANCE instance, LPCWSTR name);
HANDLE LoadImageA(HINSTANCE instance, LPCSTR name, UINT type, int cx, int cy, UINT flags);
HANDLE LoadImageW(HINSTANCE instance, LPCWSTR name, UINT type, int cx, int cy, UINT flags);
HBITMAP LoadBitmapA(HINSTANCE instance, LPCSTR name);
HBITMAP LoadBitmapW(HINSTANCE instance, LPCWSTR name);
BOOL DestroyIcon(HICON icon);
BOOL DestroyCursor(HCURSOR cursor);

/* Recursos del .rc: todavía no hay (devuelven NULL o 0) */
HRSRC FindResourceA(HMODULE module, LPCSTR name, LPCSTR type);
HRSRC FindResourceW(HMODULE module, LPCWSTR name, LPCWSTR type);
HGLOBAL LoadResource(HMODULE module, HRSRC resource);
LPVOID LockResource(HGLOBAL resource);
DWORD SizeofResource(HMODULE module, HRSRC resource);
BOOL FreeResource(HGLOBAL resource);
int LoadStringA(HINSTANCE instance, UINT id, LPSTR buffer, int size);
int LoadStringW(HINSTANCE instance, UINT id, LPWSTR buffer, int size);

/* Menús: los emergentes (TrackPopupMenu) son de Qt; GetMenu de la ventana de Edita marca
 * las órdenes del plugin (como NPPM_SETMENUITEMCHECK) */
HMENU CreateMenu(void);
HMENU CreatePopupMenu(void);
BOOL DestroyMenu(HMENU menu);
BOOL AppendMenuA(HMENU menu, UINT flags, UINT_PTR id, LPCSTR text);
BOOL AppendMenuW(HMENU menu, UINT flags, UINT_PTR id, LPCWSTR text);
BOOL InsertMenuA(HMENU menu, UINT position, UINT flags, UINT_PTR id, LPCSTR text);
BOOL InsertMenuW(HMENU menu, UINT position, UINT flags, UINT_PTR id, LPCWSTR text);
BOOL RemoveMenu(HMENU menu, UINT position, UINT flags);
BOOL DeleteMenu(HMENU menu, UINT position, UINT flags);
DWORD CheckMenuItem(HMENU menu, UINT item, UINT check);
BOOL EnableMenuItem(HMENU menu, UINT item, UINT enable);
UINT GetMenuState(HMENU menu, UINT item, UINT flags);
int GetMenuItemCount(HMENU menu);
UINT GetMenuItemID(HMENU menu, int position);
HMENU GetSubMenu(HMENU menu, int position);
HMENU GetMenu(HWND hwnd);
BOOL DrawMenuBar(HWND hwnd);
BOOL TrackPopupMenu(HMENU menu, UINT flags, int x, int y, int reserved, HWND hwnd, const RECT* rect);
BOOL TrackPopupMenuEx(HMENU menu, UINT flags, int x, int y, HWND hwnd, void* params);

/* GDI sin dibujo */
HDC GetDC(HWND hwnd);
HDC GetWindowDC(HWND hwnd);
int ReleaseDC(HWND hwnd, HDC dc);
BOOL DeleteDC(HDC dc);
HGDIOBJ SelectObject(HDC dc, HGDIOBJ object);
BOOL DeleteObject(HGDIOBJ object);
HGDIOBJ GetStockObject(int index);
int GetObjectA(HANDLE object, int size, LPVOID buffer);
int GetObjectW(HANDLE object, int size, LPVOID buffer);
HFONT CreateFontA(int height, int width, int escapement, int orientation, int weight, DWORD italic, DWORD underline,
                  DWORD strikeOut, DWORD charSet, DWORD outPrecision, DWORD clipPrecision, DWORD quality,
                  DWORD pitchAndFamily, LPCSTR face);
HFONT CreateFontW(int height, int width, int escapement, int orientation, int weight, DWORD italic, DWORD underline,
                  DWORD strikeOut, DWORD charSet, DWORD outPrecision, DWORD clipPrecision, DWORD quality,
                  DWORD pitchAndFamily, LPCWSTR face);
HFONT CreateFontIndirectA(const LOGFONTA* font);
HFONT CreateFontIndirectW(const LOGFONTW* font);
HBRUSH CreateSolidBrush(COLORREF color);
HPEN CreatePen(int style, int width, COLORREF color);
DWORD GetSysColor(int index);
HBRUSH GetSysColorBrush(int index);
COLORREF SetTextColor(HDC dc, COLORREF color);
COLORREF GetTextColor(HDC dc);
COLORREF SetBkColor(HDC dc, COLORREF color);
COLORREF GetBkColor(HDC dc);
int SetBkMode(HDC dc, int mode);
int GetDeviceCaps(HDC dc, int index);
BOOL GetTextExtentPoint32A(HDC dc, LPCSTR text, int length, LPSIZE size);
BOOL GetTextExtentPoint32W(HDC dc, LPCWSTR text, int length, LPSIZE size);
BOOL GetTextMetricsA(HDC dc, LPTEXTMETRICA metrics);
BOOL GetTextMetricsW(HDC dc, LPTEXTMETRICW metrics);
int MulDiv(int number, int numerator, int denominator);

/* Un editor de Scintilla de Edita para CreateWindowEx("Scintilla"): npp_compat.cpp */
void* edita_compat_createScintilla(void* parentWidget);

EDITA_COMPAT_END

#define CreateWindowA(c, n, s, x, y, w, h, p, m, i, l) CreateWindowExA(0, c, n, s, x, y, w, h, p, m, i, l)
#define CreateWindowW(c, n, s, x, y, w, h, p, m, i, l) CreateWindowExW(0, c, n, s, x, y, w, h, p, m, i, l)
#define CreateWindow EDITA_COMPAT_AW(CreateWindow)
#define CreateWindowEx EDITA_COMPAT_AW(CreateWindowEx)
#define RegisterClass EDITA_COMPAT_AW(RegisterClass)
#define RegisterClassEx EDITA_COMPAT_AW(RegisterClassEx)
#define UnregisterClass EDITA_COMPAT_AW(UnregisterClass)
#define GetClassName EDITA_COMPAT_AW(GetClassName)
#define DefWindowProc EDITA_COMPAT_AW(DefWindowProc)
#define DefDlgProc EDITA_COMPAT_AW(DefDlgProc)
#define CallWindowProc EDITA_COMPAT_AW(CallWindowProc)
#define GetWindowLongPtr EDITA_COMPAT_AW(GetWindowLongPtr)
#define SetWindowLongPtr EDITA_COMPAT_AW(SetWindowLongPtr)
#define GetWindowLong EDITA_COMPAT_AW(GetWindowLong)
#define SetWindowLong EDITA_COMPAT_AW(SetWindowLong)
#define SetProp EDITA_COMPAT_AW(SetProp)
#define GetProp EDITA_COMPAT_AW(GetProp)
#define RemoveProp EDITA_COMPAT_AW(RemoveProp)
#define CreateDialogIndirectParam EDITA_COMPAT_AW(CreateDialogIndirectParam)
#define DialogBoxIndirectParam EDITA_COMPAT_AW(DialogBoxIndirectParam)
#define IsDialogMessage EDITA_COMPAT_AW(IsDialogMessage)
#define GetDlgItemText EDITA_COMPAT_AW(GetDlgItemText)
#define SendDlgItemMessage EDITA_COMPAT_AW(SendDlgItemMessage)
#define SetWindowText EDITA_COMPAT_AW(SetWindowText)
#define GetWindowText EDITA_COMPAT_AW(GetWindowText)
#define GetWindowTextLength EDITA_COMPAT_AW(GetWindowTextLength)
#define SystemParametersInfo EDITA_COMPAT_AW(SystemParametersInfo)
#define GetMonitorInfo EDITA_COMPAT_AW(GetMonitorInfo)
#define LoadCursor EDITA_COMPAT_AW(LoadCursor)
#define LoadIcon EDITA_COMPAT_AW(LoadIcon)
#define LoadImage EDITA_COMPAT_AW(LoadImage)
#define LoadBitmap EDITA_COMPAT_AW(LoadBitmap)
#define FindResource EDITA_COMPAT_AW(FindResource)
#define LoadString EDITA_COMPAT_AW(LoadString)
#define AppendMenu EDITA_COMPAT_AW(AppendMenu)
#define InsertMenu EDITA_COMPAT_AW(InsertMenu)
#define GetObject EDITA_COMPAT_AW(GetObject)
#define CreateFont EDITA_COMPAT_AW(CreateFont)
#define CreateFontIndirect EDITA_COMPAT_AW(CreateFontIndirect)
#define GetTextExtentPoint32 EDITA_COMPAT_AW(GetTextExtentPoint32)
#define GetTextMetrics EDITA_COMPAT_AW(GetTextMetrics)

#endif /* EDITA_NPP_COMPAT_USER_H */
