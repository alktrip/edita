// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 82: la API de plugins queda fija para toda la 1.x. Un plugin compilado con el SDK de
// la 1.0 tiene que cargar y funcionar igual en cualquier 1.x: los números de los mensajes y
// las notificaciones, la forma de las estructuras y los números de lenguajes y codificaciones
// no cambian. Se pueden añadir cosas (con su línea aquí); cambiar o quitar una es romper la
// API, y eso solo puede pasar en Edita 2.0 (con EDITA_PLUGIN_API_VERSION 2)

#include "AppInfo.h"
#include "FakePrompter.h"
#include "core/Encoding.h"
#include "core/LangType.h"
#include "ui/MainWindow.h"

#include <edita_plugin.h>

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

#include <cstddef>

using namespace edita;
using edita::test::FakePrompter;

namespace {

struct Frozen
{
    const char* name;
    long long value;
};

// Los #define de edita_plugin.h en la 1.0, con su valor
constexpr Frozen kDefines[] = {
        {"EDITA_PLUGIN_API_VERSION", 1},
        {"EDITA_MENU_ITEM_SIZE", 64},
        {"EDITA_WM_USER", 1024},
        {"NPPMSG", 2024},
        {"NPPM_GETCURRENTSCINTILLA", 2028},
        {"NPPM_GETCURRENTLANGTYPE", 2029},
        {"NPPM_SETCURRENTLANGTYPE", 2030},
        {"NPPM_GETNBOPENFILES", 2031},
        {"ALL_OPEN_FILES", 0},
        {"PRIMARY_VIEW", 1},
        {"SECOND_VIEW", 2},
        {"NPPM_SETSTATUSBAR", 2048},
        {"STATUSBAR_DOC_TYPE", 0},
        {"STATUSBAR_DOC_SIZE", 1},
        {"STATUSBAR_CUR_POS", 2},
        {"STATUSBAR_EOF_FORMAT", 3},
        {"STATUSBAR_UNICODE_TYPE", 4},
        {"STATUSBAR_TYPING_MODE", 5},
        {"NPPM_RELOADFILE", 2060},
        {"NPPM_SWITCHTOFILE", 2061},
        {"NPPM_SAVECURRENTFILE", 2062},
        {"NPPM_SAVEALLFILES", 2063},
        {"NPPM_SETMENUITEMCHECK", 2064},
        {"NPPM_GETPLUGINSCONFIGDIR", 2070},
        {"NPPM_MENUCOMMAND", 2072},
        {"NPPM_GETNPPVERSION", 2074},
        {"NPPM_GETFULLPATHFROMBUFFERID", 2082},
        {"NPPM_GETCURRENTBUFFERID", 2084},
        {"NPPM_GETBUFFERLANGTYPE", 2088},
        {"NPPM_SETBUFFERLANGTYPE", 2089},
        {"NPPM_DOOPEN", 2101},
        {"NPPM_GETCURRENTVIEW", 2112},
        {"NPPM_GETCURRENTDOCINDEX", 2047},
        {"MAIN_VIEW", 0},
        {"SUB_VIEW", 1},
        {"NPPM_ACTIVATEDOC", 2052},
        {"NPPM_MAKECURRENTBUFFERDIRTY", 2068},
        {"NPPM_GETPOSFROMBUFFERID", 2081},
        {"NPPM_GETBUFFERIDFROMPOS", 2083},
        {"NPPM_RELOADBUFFERID", 2085},
        {"NPPM_GETBUFFERENCODING", 2090},
        {"NPPM_SETBUFFERENCODING", 2091},
        {"NPPM_GETBUFFERFORMAT", 2092},
        {"NPPM_SETBUFFERFORMAT", 2093},
        {"NPPM_SAVECURRENTFILEAS", 2102},
        {"NPPM_SAVEFILE", 2118},
        {"NPPM_GETLANGUAGENAME", 2107},
        {"NPPM_GETLANGUAGEDESC", 2108},
        {"NPPM_GETNBUSERLANG", 2046},
        {"NPPM_LAUNCHFINDINFILESDLG", 2053},
        {"NPPM_GETNBSESSIONFILES", 2037},
        {"NPPM_SAVECURRENTSESSION", 2040},
        {"NPPM_LOADSESSION", 2058},
        {"NPPM_CREATESCINTILLAHANDLE", 2044},
        {"NPPM_DESTROYSCINTILLAHANDLE", 2045},
        {"NPPM_GETMENUHANDLE", 2049},
        {"NPPPLUGINMENU", 0},
        {"NPPMAINMENU", 1},
        {"NPPM_HIDETABBAR", 2075},
        {"NPPM_ISTABBARHIDDEN", 2076},
        {"NPPM_HIDEMENU", 2096},
        {"NPPM_ISMENUHIDDEN", 2097},
        {"NPPM_HIDESTATUSBAR", 2098},
        {"NPPM_ISSTATUSBARHIDDEN", 2099},
        {"NPPM_GETEDITORDEFAULTFOREGROUNDCOLOR", 2114},
        {"NPPM_GETEDITORDEFAULTBACKGROUNDCOLOR", 2115},
        {"NPPM_ISDARKMODEENABLED", 2131},
        {"NPPM_ISAUTOINDENTON", 2129},
        {"NPPM_GETCURRENTCMDLINE", 2133},
        {"NPPM_GETBOOKMARKID", 2135},
        {"NPPM_ALLOCATECMDID", 2105},
        {"NPPM_ALLOCATEMARKER", 2106},
        {"NPPM_ALLOCATEINDICATOR", 2137},
        {"NPPM_GETPLUGINHOMEPATH", 2121},
        {"NPPM_GETSETTINGSONCLOUDPATH", 2122},
        {"NPPM_GETNATIVELANGFILENAME", 2140},
        {"NPPM_ADDSCNMODIFIEDFLAGS", 2141},
        {"NPPM_CREATELEXER", 2134},
        {"NPPM_MSGTOPLUGIN", 2071},
        {"NPPM_DMMSHOW", 2054},
        {"NPPM_DMMHIDE", 2055},
        {"NPPM_DMMUPDATEDISPINFO", 2056},
        {"NPPM_DMMREGASDCKDLG", 2057},
        {"NPPM_DMMGETPLUGINHWNDBYNAME", 2067},
        {"DWS_ICONTAB", 1},
        {"DWS_ICONBAR", 2},
        {"DWS_ADDINFO", 4},
        {"DWS_DF_CONT_LEFT", 0},
        {"DWS_DF_CONT_RIGHT", 268435456},
        {"DWS_DF_CONT_TOP", 536870912},
        {"DWS_DF_CONT_BOTTOM", 805306368},
        {"DWS_DF_FLOATING", 2147483648},
        {"RUNCOMMAND_USER", 4024},
        {"NPPM_GETFULLCURRENTPATH", 4025},
        {"NPPM_GETCURRENTDIRECTORY", 4026},
        {"NPPM_GETFILENAME", 4027},
        {"NPPM_GETNAMEPART", 4028},
        {"NPPM_GETEXTPART", 4029},
        {"NPPM_GETCURRENTWORD", 4030},
        {"NPPM_GETCURRENTLINE", 4032},
        {"NPPM_GETCURRENTCOLUMN", 4033},
        {"NPPM_GETNPPDIRECTORY", 4031},
        {"NPPN_FIRST", 1000},
        {"NPPN_READY", 1001},
        {"NPPN_FILEBEFORECLOSE", 1003},
        {"NPPN_FILEOPENED", 1004},
        {"NPPN_FILECLOSED", 1005},
        {"NPPN_FILEBEFOREOPEN", 1006},
        {"NPPN_FILEBEFORESAVE", 1007},
        {"NPPN_FILESAVED", 1008},
        {"NPPN_SHUTDOWN", 1009},
        {"NPPN_BUFFERACTIVATED", 1010},
        {"NPPN_LANGCHANGED", 1011},
        {"NPPN_READONLYCHANGED", 1016},
        {"NPPN_BEFORESHUTDOWN", 1019},
        {"NPPN_CANCELSHUTDOWN", 1020},
        {"NPPN_FILEBEFORERENAME", 1021},
        {"NPPN_FILERENAMECANCEL", 1022},
        {"NPPN_FILERENAMED", 1023},
        {"NPPN_FILEBEFOREDELETE", 1024},
        {"NPPN_FILEDELETEFAILED", 1025},
        {"NPPN_FILEDELETED", 1026},
};

// El valor de hoy de cada nombre: el preprocesador los resuelve aquí, no en la tabla de arriba
#define EDITA_NOW(name) {#name, static_cast<long long>(name)}
const Frozen kCurrent[] = {
    EDITA_NOW(EDITA_PLUGIN_API_VERSION),
    EDITA_NOW(EDITA_MENU_ITEM_SIZE),
    EDITA_NOW(EDITA_WM_USER),
    EDITA_NOW(NPPMSG),
    EDITA_NOW(NPPM_GETCURRENTSCINTILLA),
    EDITA_NOW(NPPM_GETCURRENTLANGTYPE),
    EDITA_NOW(NPPM_SETCURRENTLANGTYPE),
    EDITA_NOW(NPPM_GETNBOPENFILES),
    EDITA_NOW(ALL_OPEN_FILES),
    EDITA_NOW(PRIMARY_VIEW),
    EDITA_NOW(SECOND_VIEW),
    EDITA_NOW(NPPM_SETSTATUSBAR),
    EDITA_NOW(STATUSBAR_DOC_TYPE),
    EDITA_NOW(STATUSBAR_DOC_SIZE),
    EDITA_NOW(STATUSBAR_CUR_POS),
    EDITA_NOW(STATUSBAR_EOF_FORMAT),
    EDITA_NOW(STATUSBAR_UNICODE_TYPE),
    EDITA_NOW(STATUSBAR_TYPING_MODE),
    EDITA_NOW(NPPM_RELOADFILE),
    EDITA_NOW(NPPM_SWITCHTOFILE),
    EDITA_NOW(NPPM_SAVECURRENTFILE),
    EDITA_NOW(NPPM_SAVEALLFILES),
    EDITA_NOW(NPPM_SETMENUITEMCHECK),
    EDITA_NOW(NPPM_GETPLUGINSCONFIGDIR),
    EDITA_NOW(NPPM_MENUCOMMAND),
    EDITA_NOW(NPPM_GETNPPVERSION),
    EDITA_NOW(NPPM_GETFULLPATHFROMBUFFERID),
    EDITA_NOW(NPPM_GETCURRENTBUFFERID),
    EDITA_NOW(NPPM_GETBUFFERLANGTYPE),
    EDITA_NOW(NPPM_SETBUFFERLANGTYPE),
    EDITA_NOW(NPPM_DOOPEN),
    EDITA_NOW(NPPM_GETCURRENTVIEW),
    EDITA_NOW(NPPM_GETCURRENTDOCINDEX),
    EDITA_NOW(MAIN_VIEW),
    EDITA_NOW(SUB_VIEW),
    EDITA_NOW(NPPM_ACTIVATEDOC),
    EDITA_NOW(NPPM_MAKECURRENTBUFFERDIRTY),
    EDITA_NOW(NPPM_GETPOSFROMBUFFERID),
    EDITA_NOW(NPPM_GETBUFFERIDFROMPOS),
    EDITA_NOW(NPPM_RELOADBUFFERID),
    EDITA_NOW(NPPM_GETBUFFERENCODING),
    EDITA_NOW(NPPM_SETBUFFERENCODING),
    EDITA_NOW(NPPM_GETBUFFERFORMAT),
    EDITA_NOW(NPPM_SETBUFFERFORMAT),
    EDITA_NOW(NPPM_SAVECURRENTFILEAS),
    EDITA_NOW(NPPM_SAVEFILE),
    EDITA_NOW(NPPM_GETLANGUAGENAME),
    EDITA_NOW(NPPM_GETLANGUAGEDESC),
    EDITA_NOW(NPPM_GETNBUSERLANG),
    EDITA_NOW(NPPM_LAUNCHFINDINFILESDLG),
    EDITA_NOW(NPPM_GETNBSESSIONFILES),
    EDITA_NOW(NPPM_SAVECURRENTSESSION),
    EDITA_NOW(NPPM_LOADSESSION),
    EDITA_NOW(NPPM_CREATESCINTILLAHANDLE),
    EDITA_NOW(NPPM_DESTROYSCINTILLAHANDLE),
    EDITA_NOW(NPPM_GETMENUHANDLE),
    EDITA_NOW(NPPPLUGINMENU),
    EDITA_NOW(NPPMAINMENU),
    EDITA_NOW(NPPM_HIDETABBAR),
    EDITA_NOW(NPPM_ISTABBARHIDDEN),
    EDITA_NOW(NPPM_HIDEMENU),
    EDITA_NOW(NPPM_ISMENUHIDDEN),
    EDITA_NOW(NPPM_HIDESTATUSBAR),
    EDITA_NOW(NPPM_ISSTATUSBARHIDDEN),
    EDITA_NOW(NPPM_GETEDITORDEFAULTFOREGROUNDCOLOR),
    EDITA_NOW(NPPM_GETEDITORDEFAULTBACKGROUNDCOLOR),
    EDITA_NOW(NPPM_ISDARKMODEENABLED),
    EDITA_NOW(NPPM_ISAUTOINDENTON),
    EDITA_NOW(NPPM_GETCURRENTCMDLINE),
    EDITA_NOW(NPPM_GETBOOKMARKID),
    EDITA_NOW(NPPM_ALLOCATECMDID),
    EDITA_NOW(NPPM_ALLOCATEMARKER),
    EDITA_NOW(NPPM_ALLOCATEINDICATOR),
    EDITA_NOW(NPPM_GETPLUGINHOMEPATH),
    EDITA_NOW(NPPM_GETSETTINGSONCLOUDPATH),
    EDITA_NOW(NPPM_GETNATIVELANGFILENAME),
    EDITA_NOW(NPPM_ADDSCNMODIFIEDFLAGS),
    EDITA_NOW(NPPM_CREATELEXER),
    EDITA_NOW(NPPM_MSGTOPLUGIN),
    EDITA_NOW(NPPM_DMMSHOW),
    EDITA_NOW(NPPM_DMMHIDE),
    EDITA_NOW(NPPM_DMMUPDATEDISPINFO),
    EDITA_NOW(NPPM_DMMREGASDCKDLG),
    EDITA_NOW(NPPM_DMMGETPLUGINHWNDBYNAME),
    EDITA_NOW(DWS_ICONTAB),
    EDITA_NOW(DWS_ICONBAR),
    EDITA_NOW(DWS_ADDINFO),
    EDITA_NOW(DWS_DF_CONT_LEFT),
    EDITA_NOW(DWS_DF_CONT_RIGHT),
    EDITA_NOW(DWS_DF_CONT_TOP),
    EDITA_NOW(DWS_DF_CONT_BOTTOM),
    EDITA_NOW(DWS_DF_FLOATING),
    EDITA_NOW(RUNCOMMAND_USER),
    EDITA_NOW(NPPM_GETFULLCURRENTPATH),
    EDITA_NOW(NPPM_GETCURRENTDIRECTORY),
    EDITA_NOW(NPPM_GETFILENAME),
    EDITA_NOW(NPPM_GETNAMEPART),
    EDITA_NOW(NPPM_GETEXTPART),
    EDITA_NOW(NPPM_GETCURRENTWORD),
    EDITA_NOW(NPPM_GETCURRENTLINE),
    EDITA_NOW(NPPM_GETCURRENTCOLUMN),
    EDITA_NOW(NPPM_GETNPPDIRECTORY),
    EDITA_NOW(NPPN_FIRST),
    EDITA_NOW(NPPN_READY),
    EDITA_NOW(NPPN_FILEBEFORECLOSE),
    EDITA_NOW(NPPN_FILEOPENED),
    EDITA_NOW(NPPN_FILECLOSED),
    EDITA_NOW(NPPN_FILEBEFOREOPEN),
    EDITA_NOW(NPPN_FILEBEFORESAVE),
    EDITA_NOW(NPPN_FILESAVED),
    EDITA_NOW(NPPN_SHUTDOWN),
    EDITA_NOW(NPPN_BUFFERACTIVATED),
    EDITA_NOW(NPPN_LANGCHANGED),
    EDITA_NOW(NPPN_READONLYCHANGED),
    EDITA_NOW(NPPN_BEFORESHUTDOWN),
    EDITA_NOW(NPPN_CANCELSHUTDOWN),
    EDITA_NOW(NPPN_FILEBEFORERENAME),
    EDITA_NOW(NPPN_FILERENAMECANCEL),
    EDITA_NOW(NPPN_FILERENAMED),
    EDITA_NOW(NPPN_FILEBEFOREDELETE),
    EDITA_NOW(NPPN_FILEDELETEFAILED),
    EDITA_NOW(NPPN_FILEDELETED),
};
#undef EDITA_NOW

struct FrozenLang
{
    LangType lang;
    int value;
};

// Los números de lenguaje de N++ (L_*) que ven los plugins
constexpr FrozenLang kLangs[] = {
        {LangType::Text, 0}, // L_TEXT
        {LangType::Php, 1}, // L_PHP
        {LangType::C, 2}, // L_C
        {LangType::Cpp, 3}, // L_CPP
        {LangType::CSharp, 4}, // L_CS
        {LangType::ObjC, 5}, // L_OBJC
        {LangType::Java, 6}, // L_JAVA
        {LangType::Rc, 7}, // L_RC
        {LangType::Html, 8}, // L_HTML
        {LangType::Xml, 9}, // L_XML
        {LangType::Makefile, 10}, // L_MAKEFILE
        {LangType::Pascal, 11}, // L_PASCAL
        {LangType::Batch, 12}, // L_BATCH
        {LangType::Ini, 13}, // L_INI
        {LangType::Ascii, 14}, // L_ASCII
        {LangType::User, 15}, // L_USER
        {LangType::Asp, 16}, // L_ASP
        {LangType::Sql, 17}, // L_SQL
        {LangType::Vb, 18}, // L_VB
        {LangType::JsEmbedded, 19}, // L_JS_EMBEDDED
        {LangType::Css, 20}, // L_CSS
        {LangType::Perl, 21}, // L_PERL
        {LangType::Python, 22}, // L_PYTHON
        {LangType::Lua, 23}, // L_LUA
        {LangType::Tex, 24}, // L_TEX
        {LangType::Fortran, 25}, // L_FORTRAN
        {LangType::Bash, 26}, // L_BASH
        {LangType::Flash, 27}, // L_FLASH
        {LangType::Nsis, 28}, // L_NSIS
        {LangType::Tcl, 29}, // L_TCL
        {LangType::Lisp, 30}, // L_LISP
        {LangType::Scheme, 31}, // L_SCHEME
        {LangType::Asm, 32}, // L_ASM
        {LangType::Diff, 33}, // L_DIFF
        {LangType::Props, 34}, // L_PROPS
        {LangType::PostScript, 35}, // L_PS
        {LangType::Ruby, 36}, // L_RUBY
        {LangType::Smalltalk, 37}, // L_SMALLTALK
        {LangType::Vhdl, 38}, // L_VHDL
        {LangType::Kix, 39}, // L_KIX
        {LangType::AutoIt, 40}, // L_AU3
        {LangType::Caml, 41}, // L_CAML
        {LangType::Ada, 42}, // L_ADA
        {LangType::Verilog, 43}, // L_VERILOG
        {LangType::Matlab, 44}, // L_MATLAB
        {LangType::Haskell, 45}, // L_HASKELL
        {LangType::Inno, 46}, // L_INNO
        {LangType::SearchResult, 47}, // L_SEARCHRESULT
        {LangType::CMake, 48}, // L_CMAKE
        {LangType::Yaml, 49}, // L_YAML
        {LangType::Cobol, 50}, // L_COBOL
        {LangType::Gui4Cli, 51}, // L_GUI4CLI
        {LangType::D, 52}, // L_D
        {LangType::PowerShell, 53}, // L_POWERSHELL
        {LangType::R, 54}, // L_R
        {LangType::Jsp, 55}, // L_JSP
        {LangType::CoffeeScript, 56}, // L_COFFEESCRIPT
        {LangType::Json, 57}, // L_JSON
        {LangType::JavaScript, 58}, // L_JAVASCRIPT
        {LangType::Fortran77, 59}, // L_FORTRAN_77
        {LangType::BaanC, 60}, // L_BAANC
        {LangType::Srec, 61}, // L_SREC
        {LangType::IHex, 62}, // L_IHEX
        {LangType::TeHex, 63}, // L_TEHEX
        {LangType::Swift, 64}, // L_SWIFT
        {LangType::Asn1, 65}, // L_ASN1
        {LangType::Avs, 66}, // L_AVS
        {LangType::BlitzBasic, 67}, // L_BLITZBASIC
        {LangType::PureBasic, 68}, // L_PUREBASIC
        {LangType::FreeBasic, 69}, // L_FREEBASIC
        {LangType::Csound, 70}, // L_CSOUND
        {LangType::Erlang, 71}, // L_ERLANG
        {LangType::EScript, 72}, // L_ESCRIPT
        {LangType::Forth, 73}, // L_FORTH
        {LangType::Latex, 74}, // L_LATEX
        {LangType::Mmixal, 75}, // L_MMIXAL
        {LangType::Nim, 76}, // L_NIM
        {LangType::Nncrontab, 77}, // L_NNCRONTAB
        {LangType::OScript, 78}, // L_OSCRIPT
        {LangType::Rebol, 79}, // L_REBOL
        {LangType::Registry, 80}, // L_REGISTRY
        {LangType::Rust, 81}, // L_RUST
        {LangType::Spice, 82}, // L_SPICE
        {LangType::Txt2tags, 83}, // L_TXT2TAGS
        {LangType::VisualProlog, 84}, // L_VISUALPROLOG
        {LangType::TypeScript, 85}, // L_TYPESCRIPT
        {LangType::Json5, 86}, // L_JSON5
        {LangType::MsSql, 87}, // L_MSSQL
        {LangType::GdScript, 88}, // L_GDSCRIPT
        {LangType::Hollywood, 89}, // L_HOLLYWOOD
        {LangType::Go, 90}, // L_GOLANG
        {LangType::Raku, 91}, // L_RAKU
        {LangType::Toml, 92}, // L_TOML
        {LangType::Sas, 93}, // L_SAS
        {LangType::ErrorList, 94}, // L_ERRORLIST
        {LangType::EscSeq, 95}, // L_ESCSEQ
        {LangType::StructuredText, 96}, // L_STTXT
        {LangType::External, 97}, // L_EXTERNAL
};

// Forma de las estructuras en x86_64 (Linux, LP64)
static_assert(sizeof(void*) == 8, "la tabla es de 64 bits");
static_assert(sizeof(EditaData) == 32);
static_assert(offsetof(EditaData, host) == 0);
static_assert(offsetof(EditaData, mainEditor) == 8);
static_assert(offsetof(EditaData, subEditor) == 16);
static_assert(offsetof(EditaData, send) == 24);
static_assert(sizeof(EditaShortcutKey) == 4);
static_assert(offsetof(EditaShortcutKey, isCtrl) == 0);
static_assert(offsetof(EditaShortcutKey, isAlt) == 1);
static_assert(offsetof(EditaShortcutKey, isShift) == 2);
static_assert(offsetof(EditaShortcutKey, key) == 3);
static_assert(sizeof(EditaFuncItem) == 88);
static_assert(offsetof(EditaFuncItem, itemName) == 0);
static_assert(offsetof(EditaFuncItem, pFunc) == 64);
static_assert(offsetof(EditaFuncItem, cmdID) == 72);
static_assert(offsetof(EditaFuncItem, init2Check) == 76);
static_assert(offsetof(EditaFuncItem, pShKey) == 80);
static_assert(sizeof(EditaCommunicationInfo) == 24);
static_assert(offsetof(EditaCommunicationInfo, internalMsg) == 0);
static_assert(offsetof(EditaCommunicationInfo, srcModuleName) == 8);
static_assert(offsetof(EditaCommunicationInfo, info) == 16);
static_assert(sizeof(EditaDockData) == 48);
static_assert(offsetof(EditaDockData, hClient) == 0);
static_assert(offsetof(EditaDockData, pszName) == 8);
static_assert(offsetof(EditaDockData, dlgID) == 16);
static_assert(offsetof(EditaDockData, uMask) == 20);
static_assert(offsetof(EditaDockData, hIconTab) == 24);
static_assert(offsetof(EditaDockData, pszAddInfo) == 32);
static_assert(offsetof(EditaDockData, pszModuleName) == 40);
// Edita 1.1: lo que recibe edita_plugin_before_commit (fase 88)
static_assert(sizeof(EditaCommitCheck) == 32);
static_assert(offsetof(EditaCommitCheck, repository) == 0);
static_assert(offsetof(EditaCommitCheck, message) == 8);
static_assert(offsetof(EditaCommitCheck, reason) == 16);
static_assert(offsetof(EditaCommitCheck, reasonSize) == 24);
// SCNotification es de Scintilla y llega tal cual a beNotified: si una versión nueva de
// Scintilla la cambia, los plugins ya compilados leerían basura. Actualizar Scintilla en la
// 1.x solo vale si esto sigue igual
static_assert(sizeof(SCNotification) == 160);
static_assert(offsetof(SCNotification, nmhdr) == 0);
static_assert(offsetof(SCNotification, position) == 24);
static_assert(offsetof(SCNotification, ch) == 32);
static_assert(offsetof(SCNotification, modifiers) == 36);
static_assert(offsetof(SCNotification, modificationType) == 40);
static_assert(offsetof(SCNotification, text) == 48);
static_assert(offsetof(SCNotification, length) == 56);
static_assert(offsetof(SCNotification, linesAdded) == 64);
static_assert(offsetof(SCNotification, line) == 96);
static_assert(offsetof(SCNotification, listType) == 116);
static_assert(offsetof(SCNotification, characterSource) == 152);

} // namespace

class TestPluginApi : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_UPDATE_URL", "none");
    }

    // La 1.x es la API 1
    void apiVersionIsOne() { QCOMPARE(EDITA_PLUGIN_API_VERSION, 1); }

    // Ningún número cambia
    void definesKeepTheirValues()
    {
        QHash<QString, long long> now;
        for (const Frozen& f : kCurrent)
            now.insert(QString::fromLatin1(f.name), f.value);
        QStringList problems;
        for (const Frozen& f : kDefines) {
            const QString name = QString::fromLatin1(f.name);
            if (!now.contains(name))
                problems << name + QStringLiteral(": ya no existe");
            else if (now.value(name) != f.value)
                problems << QStringLiteral("%1: era %2 y ahora es %3").arg(name).arg(f.value).arg(now.value(name));
        }
        QVERIFY2(problems.isEmpty(), qPrintable(QLatin1Char('\n') + problems.join(QLatin1Char('\n'))));
    }

    // Lo que se añada al SDK queda también aquí: desde ese momento es parte de la API
    void everyDefineIsFrozen()
    {
        QFile header(QStringLiteral(EDITA_SOURCE_DIR "/plugin-sdk/edita_plugin.h"));
        QVERIFY(header.open(QIODevice::ReadOnly));
        static const QRegularExpression define(QStringLiteral("^[ \\t]*#define[ \\t]+([A-Z_][A-Z0-9_]*)[ \\t]+\\S"),
                                               QRegularExpression::MultilineOption);
        QSet<QString> frozen;
        for (const Frozen& f : kDefines)
            frozen.insert(QString::fromLatin1(f.name));
        QStringList missing;
        for (auto it = define.globalMatch(QString::fromUtf8(header.readAll())); it.hasNext();) {
            const QString name = it.next().captured(1);
            if (name != QLatin1String("EDITA_PLUGIN_EXPORT") && !frozen.contains(name))
                missing << name;
        }
        QVERIFY2(missing.isEmpty(), qPrintable(QStringLiteral("falta en kDefines y kCurrent: ") + missing.join(QLatin1String(", "))));
    }

    void languageNumbersAreFrozen()
    {
        for (const FrozenLang& l : kLangs)
            QCOMPARE(static_cast<int>(l.lang), l.value);
    }

    // NPPM_GETBUFFERENCODING / NPPM_SETBUFFERENCODING (UniMode de N++)
    void encodingNumbersAreFrozen()
    {
        QCOMPARE(static_cast<int>(UniMode::Ansi), 0);
        QCOMPARE(static_cast<int>(UniMode::Utf8Bom), 1);
        QCOMPARE(static_cast<int>(UniMode::Utf16BeBom), 2);
        QCOMPARE(static_cast<int>(UniMode::Utf16LeBom), 3);
        QCOMPARE(static_cast<int>(UniMode::Utf8), 4);
        QCOMPARE(static_cast<int>(UniMode::Ascii7), 5);
        QCOMPARE(static_cast<int>(UniMode::Utf16BeNoBom), 6);
        QCOMPARE(static_cast<int>(UniMode::Utf16LeNoBom), 7);
    }

    // Un plugin puede preguntar por un mensaje que añada una 1.x posterior: en una versión
    // que no lo tiene recibe 0, como en N++
    void unknownMessagesReturnZero()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QCOMPARE(w.handlePluginMessage(NPPMSG + 999, 1, 1), intptr_t{0});
        QCOMPARE(w.handlePluginMessage(RUNCOMMAND_USER + 999, 1, 1), intptr_t{0});
    }

    // NPPM_GETNPPVERSION: (mayor << 16) | menor, para que un plugin sepa si tiene una 1.x
    void versionForPlugins()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        const QStringList parts = QString::fromUtf8(kAppVersionNumber).split(QLatin1Char('.'));
        const intptr_t version = w.handlePluginMessage(NPPM_GETNPPVERSION, 0, 0);
        QCOMPARE(version >> 16, intptr_t{parts.value(0).toInt()});
        QCOMPARE(version & 0xffff, intptr_t{parts.value(1).toInt() * 100 + parts.value(2).toInt() * 10});
    }
};

QTEST_MAIN(TestPluginApi)
#include "tst_pluginapi.moc"
