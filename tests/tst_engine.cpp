// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 0: comprueba que el motor de edición es el mismo que el de Notepad++:
// Scintilla 5 en UTF-8, Lexilla con los lexers propios de N++ y la regex de
// Boost conectada mediante SCI_OWNREGEX.

#include "core/Locale.h"
#include "ui/EditorView.h"

#include <BoostRegexSearch.h>
#include <ILexer.h>
#include <Lexilla.h>
#include <SciLexer.h>

#include <QTest>

#include <memory>
#include <string>

using edita::EditorView;

namespace {

// Busca `pattern` en todo el documento con los flags que usa Notepad++ para
// "Buscar siguiente" en modo regex (FindReplaceDlg.cpp, processFindNext).
// Devuelve la posición de inicio o -1.
sptr_t regexFind(const EditorView& view, const char* pattern, int extraFlags = 0)
{
    const int flags = SCFIND_REGEXP | SCFIND_POSIX | SCFIND_REGEXP_EMPTYMATCH_ALL
        | SCFIND_REGEXP_SKIPCRLFASONE | extraFlags;
    view.sci(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(flags));
    view.sci(SCI_SETTARGETRANGE, 0, view.sci(SCI_GETLENGTH));
    const auto len = static_cast<uptr_t>(std::char_traits<char>::length(pattern));
    return view.sci(SCI_SEARCHINTARGET, len, reinterpret_cast<sptr_t>(pattern));
}

} // namespace

class TestEngine : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // La aplicación hace lo mismo en main()
        QVERIFY(!edita::initGlobalLocale().empty());
    }

    void utf8IsDefaultCodePage()
    {
        EditorView view;
        QCOMPARE(view.sci(SCI_GETCODEPAGE), static_cast<sptr_t>(SC_CP_UTF8));
    }

    void lexillaHasNppLexers_data()
    {
        QTest::addColumn<QString>("name");
        // Estándar de Lexilla usados por N++ y los tres que añade N++
        for (const char* n : {"cpp", "python", "hypertext", "xml", "bash", "json",
                              "searchResult", "user", "objc"})
            QTest::newRow(n) << QString::fromLatin1(n);
    }

    void lexillaHasNppLexers()
    {
        QFETCH(QString, name);
        Scintilla::ILexer5* lexer = CreateLexer(name.toLatin1().constData());
        QVERIFY2(lexer != nullptr, qPrintable(name));
        lexer->Release();
    }

    void lexerColourisesKeywords()
    {
        EditorView view;
        QVERIFY(view.setLexerByName("cpp"));
        view.sends(SCI_SETKEYWORDS, 0, "int return");
        view.setText("int x; // c");
        view.sci(SCI_COLOURISE, 0, -1);
        QCOMPARE(view.sci(SCI_GETSTYLEAT, 0), static_cast<sptr_t>(SCE_C_WORD));
        QCOMPARE(view.sci(SCI_GETSTYLEAT, 4), static_cast<sptr_t>(SCE_C_IDENTIFIER));
        QCOMPARE(view.sci(SCI_GETSTYLEAT, 7), static_cast<sptr_t>(SCE_C_COMMENTLINE));
    }

    void unknownLexerIsRejected()
    {
        EditorView view;
        QVERIFY(!view.setLexerByName("no-existe"));
    }

    // RESearch (el motor nativo de Scintilla) no admite lookbehind: si esto
    // encuentra la coincidencia, la búsqueda pasa por Boost.Regex.
    void boostRegexLookbehind()
    {
        EditorView view;
        view.setText("xb ab");
        QCOMPARE(regexFind(view, "(?<=a)b"), sptr_t{4});
    }

    void boostRegexLineBreakEscape()
    {
        EditorView view;
        view.setText("uno\r\ndos");
        QCOMPARE(regexFind(view, "o\\Rd"), sptr_t{2});
        QCOMPARE(view.sci(SCI_GETTARGETEND), sptr_t{6});
    }

    void boostRegexRecursion()
    {
        // El patrón de PowerEditor/installer/filesForTesting/regexGlobalTest.xml
        EditorView view;
        view.setText("f(a(b)c) g");
        QCOMPARE(regexFind(view, "\\((?>[^()]|(?R))*\\)"), sptr_t{1});
        QCOMPARE(view.sci(SCI_GETTARGETEND), sptr_t{8});
    }

    void boostRegexDotMatchesNewlineFlag()
    {
        EditorView view;
        view.setText("a\nb");
        QCOMPARE(regexFind(view, "a.b"), sptr_t{-1});
        QCOMPARE(regexFind(view, "a.b", SCFIND_REGEXP_DOTMATCHESNL), sptr_t{0});
    }

    void boostRegexUtf8()
    {
        EditorView view;
        view.setText("año ñu");
        // \b y \w deben reconocer ñ como letra (ver core/Locale.h);
        // la posición se mide en bytes UTF-8
        QCOMPARE(regexFind(view, "\\bñ\\w"), sptr_t{5});
        QCOMPARE(regexFind(view, "[[:alpha:]]+o"), sptr_t{0});
        QCOMPARE(view.sci(SCI_GETTARGETEND), sptr_t{4});
    }

    void undoRedo()
    {
        EditorView view;
        view.setText("hola");
        view.sci(SCI_EMPTYUNDOBUFFER);
        view.sci(SCI_DOCUMENTEND);
        view.sends(SCI_ADDTEXT, 6, " mundo");
        QCOMPARE(view.text(), std::string("hola mundo"));
        view.sci(SCI_UNDO);
        QCOMPARE(view.text(), std::string("hola"));
        view.sci(SCI_REDO);
        QCOMPARE(view.text(), std::string("hola mundo"));
    }
};

QTEST_MAIN(TestEngine)
#include "tst_engine.moc"
