// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 3: langs.xml, stylers.xml, temas, detección de lenguaje y aplicación de
// lexers y estilos a Scintilla, con los archivos reales de Notepad++.

#include "core/ConfigXml.h"
#include "core/LangType.h"
#include "core/Paths.h"
#include "syntax/LanguageDetection.h"
#include "syntax/LanguageRegistry.h"
#include "syntax/StyleSheet.h"
#include "syntax/SyntaxManager.h"
#include "syntax/SyntaxStyler.h"

#include <ScintillaEditBase.h>
#include <ILexer.h>
#include <Lexilla.h>
#include <SciLexer.h>

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <set>

using namespace edita;
using namespace std::string_literals;

Q_DECLARE_METATYPE(edita::LangType)

namespace {

QString dataFile(const QString& name)
{
    return paths::findDataFile(name);
}

// "RRGGBB" del XML → formato de Scintilla (0x00BBGGRR)
int bgr(unsigned rgbValue)
{
    return static_cast<int>(((rgbValue >> 16) & 0xFF) | (rgbValue & 0xFF00) | ((rgbValue & 0xFF) << 16));
}

sptr_t styleAt(const ScintillaEditBase& view, const char* text, std::string_view word)
{
    const std::string_view all(text);
    return view.send(SCI_GETSTYLEAT, all.find(word));
}

} // namespace

class TestSyntax : public QObject
{
    Q_OBJECT

private:
    LanguageRegistry _langs;
    StyleSheet _styles;

    void applyAndColourise(ScintillaEditBase& view, LangType lang, const char* text)
    {
        view.send(SCI_SETCODEPAGE, SC_CP_UTF8);
        view.sends(SCI_SETTEXT, 0, text);
        SyntaxStyler(_langs, _styles).apply(view, lang);
        view.send(SCI_COLOURISE, 0, -1);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(!dataFile("langs.model.xml").isEmpty());
        QString error;
        QVERIFY2(_langs.load(dataFile("langs.model.xml"), &error), qPrintable(error));
        QVERIFY2(_styles.load(dataFile("stylers.model.xml"), &error), qPrintable(error));
    }

    // --- Criterio: todo se lee y cada lenguaje tiene lexer ------------------------

    void allLanguagesAndLexerTypesParse()
    {
        QCOMPARE(_langs.languages().size(), std::size_t{96});
        QCOMPARE(_styles.lexers().size(), std::size_t{93});
        QVERIFY(_styles.globalStyles().size() > 40);
    }

    void everyLanguageNameIsKnown()
    {
        for (const LanguageDefinition& def : _langs.languages())
            QVERIFY2(isKnownLanguageName(def.name), def.name.c_str());
        for (const LexerStyles& lexer : _styles.lexers()) {
            // "nfo" es el estilo de L_ASCII; los demás son nombres de lenguaje
            QVERIFY2(lexer.name == "nfo" || isKnownLanguageName(lexer.name), lexer.name.c_str());
        }
    }

    void everyLanguageHasALexer()
    {
        // Sin lexer propio a propósito: texto normal, arte ASCII (usa estilos, no
        // lexer), UDL y lexers externos (fuera del MVP)
        const std::set<LangType> withoutLexer{LangType::Text, LangType::Ascii, LangType::User, LangType::External};
        for (int i = 0; i < kLangTypeCount; ++i) {
            const auto lang = static_cast<LangType>(i);
            const LanguageInfo& info = languageInfo(lang);
            if (withoutLexer.contains(lang)) {
                continue;
            }
            Scintilla::ILexer5* lexer = CreateLexer(info.lexerId);
            QVERIFY2(lexer != nullptr, info.name);
            lexer->Release();
        }
    }

    void everyLanguageCanBeAppliedAndColourised()
    {
        // Ninguna combinación debe fallar ni dejar el documento sin estilo por defecto
        ScintillaEditBase view;
        for (int i = 0; i < kLangTypeCount; ++i) {
            const auto lang = static_cast<LangType>(i);
            applyAndColourise(view, lang, "int x = 1; // comentario\n<tag attr=\"v\">texto</tag>\n# otro\n");
            QVERIFY2(view.send(SCI_GETSTATUS) == SC_STATUS_OK, languageInfo(lang).name);
        }
    }

    // --- Criterio: colores de stylers.xml en el texto --------------------------------

    void cppKeywordsGetStylersColours()
    {
        ScintillaEditBase view;
        const char* text = "int main() { return 0; } // fin";
        applyAndColourise(view, LangType::Cpp, text);

        // "return" está en instre1 → INSTRUCTION WORD (5); "int" en type1 → TYPE WORD (16)
        QCOMPARE(styleAt(view, text, "return"), sptr_t{SCE_C_WORD});
        QCOMPARE(styleAt(view, text, "int"), sptr_t{SCE_C_WORD2});
        QCOMPARE(styleAt(view, text, "// fin"), sptr_t{SCE_C_COMMENTLINE});

        const LexerStyles* cpp = _styles.lexer("cpp");
        QVERIFY(cpp);
        QCOMPARE(view.send(SCI_STYLEGETFORE, SCE_C_WORD), sptr_t{cpp->findById(SCE_C_WORD)->fgColor});
        QCOMPARE(view.send(SCI_STYLEGETFORE, SCE_C_WORD), sptr_t{bgr(0x0000FF)});   // azul en stylers.model.xml
        QCOMPARE(view.send(SCI_STYLEGETFORE, SCE_C_WORD2), sptr_t{bgr(0x8000FF)});  // violeta
        QCOMPARE(view.send(SCI_STYLEGETBOLD, SCE_C_WORD), sptr_t{1});               // fontStyle="1"
    }

    void otherLanguagesColourise_data()
    {
        QTest::addColumn<edita::LangType>("lang");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("word");
        QTest::addColumn<int>("style");
        QTest::newRow("python def") << LangType::Python << "def f():\n    return 1\n" << "def" << SCE_P_WORD;
        QTest::newRow("python comentario") << LangType::Python << "x = 1  # nota\n" << "# nota" << SCE_P_COMMENTLINE;
        QTest::newRow("bash if") << LangType::Bash << "if [ -f x ]; then echo; fi\n" << "if" << SCE_SH_WORD;
        QTest::newRow("html etiqueta") << LangType::Html << "<html><body></body></html>" << "body" << SCE_H_TAG;
        QTest::newRow("xml atributo") << LangType::Xml << "<a b=\"c\"/>" << "b" << SCE_H_ATTRIBUTE;
        QTest::newRow("json clave") << LangType::Json << "{\"clave\": 1}" << "\"clave\"" << SCE_JSON_PROPERTYNAME;
        QTest::newRow("javascript") << LangType::JavaScript << "function f() { return 1; }" << "function" << SCE_C_WORD;
        QTest::newRow("php en html") << LangType::Php << "<?php echo $x; ?>" << "echo" << SCE_HPHP_WORD;
        QTest::newRow("sql") << LangType::Sql << "SELECT * FROM t;" << "SELECT" << SCE_SQL_WORD;
        QTest::newRow("rust") << LangType::Rust << "fn main() {}" << "fn" << SCE_RUST_WORD;
        QTest::newRow("css") << LangType::Css << "a { color: red; }" << "color" << SCE_CSS_IDENTIFIER;
        QTest::newRow("ini sección") << LangType::Ini << "[seccion]\nclave=valor\n" << "[seccion]" << SCE_PROPS_SECTION;
        QTest::newRow("diff") << LangType::Diff << "--- a\n+++ b\n+añadida\n" << "+añadida" << SCE_DIFF_ADDED;
        QTest::newRow("makefile") << LangType::Makefile << "# comentario\nall:\n" << "# comentario" << SCE_MAKE_COMMENT;
        QTest::newRow("yaml") << LangType::Yaml << "clave: valor\n" << "clave" << SCE_YAML_IDENTIFIER;
    }

    void otherLanguagesColourise()
    {
        QFETCH(edita::LangType, lang);
        QFETCH(QString, text);
        QFETCH(QString, word);
        QFETCH(int, style);
        ScintillaEditBase view;
        const QByteArray utf8 = text.toUtf8();
        applyAndColourise(view, lang, utf8.constData());
        QCOMPARE(styleAt(view, utf8.constData(), word.toStdString()), sptr_t{style});
    }

    void defaultStyleAndGlobalStyles()
    {
        ScintillaEditBase view;
        applyAndColourise(view, LangType::Text, "hola");
        const Style* def = _styles.globalById(STYLE_DEFAULT);
        QCOMPARE(view.send(SCI_STYLEGETBACK, STYLE_DEFAULT), sptr_t{def->bgColor});
        QCOMPARE(view.send(SCI_STYLEGETSIZE, STYLE_DEFAULT), sptr_t{def->fontSize});
        const Style* ln = _styles.globalById(STYLE_LINENUMBER);
        QCOMPARE(view.send(SCI_STYLEGETBACK, STYLE_LINENUMBER), sptr_t{ln->bgColor});
        // Línea actual con el color de "Current line background colour"
        const Style* caretLine = _styles.globalByName("Current line background colour");
        QCOMPARE(view.send(SCI_GETELEMENTCOLOUR, SC_ELEMENT_CARET_LINE_BACK) & 0xFFFFFF, sptr_t{caretLine->bgColor});
        // Texto normal sin margen de plegado; C++ con él
        QCOMPARE(view.send(SCI_GETMARGINWIDTHN, 3), sptr_t{0});
        applyAndColourise(view, LangType::Cpp, "int x;");
        QVERIFY(view.send(SCI_GETMARGINWIDTHN, 3) > 0);
        QCOMPARE(view.send(SCI_MARKERSYMBOLDEFINED, SC_MARKNUM_FOLDEROPEN), sptr_t{SC_MARK_BOXMINUS});
    }

    void missingFontFallsBackToSystemMonospace()
    {
        const std::string requested = "Fuente Que No Existe 12345";
        const std::string resolved = SyntaxStyler::resolveFontName(requested);
        QCOMPARE(resolved, QFontDatabase::systemFont(QFontDatabase::FixedFont).family().toStdString());
        const QString installed = QFontDatabase::families().value(0);
        if (!installed.isEmpty())
            QCOMPARE(SyntaxStyler::resolveFontName(installed.toStdString()), installed.toStdString());
        // Las de Windows de los temas de N++, a la del sistema si se pide (aunque estén)
        const std::string system = QFontDatabase::systemFont(QFontDatabase::FixedFont).family().toStdString();
        for (const char* windows : {"Courier New", "Consolas", "Lucida Console"})
            QCOMPARE(SyntaxStyler::resolveFontName(windows, true), system);
        if (QFontDatabase::hasFamily(QStringLiteral("Courier New")))
            QCOMPARE(SyntaxStyler::resolveFontName("Courier New", false), "Courier New"s);
    }

    void userKeywordsAreAddedToLangsKeywords()
    {
        // Palabras añadidas por el usuario en stylers.xml (keywordClass + texto)
        StyleSheet custom = _styles;
        QString xml = QString::fromUtf8(readStylers());
        xml.replace(QStringLiteral("name=\"TYPE WORD\" styleID=\"16\" fgColor=\"8000FF\" bgColor=\"FFFFFF\" fontName=\"\" fontStyle=\"0\" fontSize=\"\" keywordClass=\"type1\" />"),
                    QStringLiteral("name=\"TYPE WORD\" styleID=\"16\" fgColor=\"8000FF\" bgColor=\"FFFFFF\" fontName=\"\" fontStyle=\"0\" fontSize=\"\" keywordClass=\"type1\">MiTipo</WordsStyle>"));
        QVERIFY(custom.loadFromString(xml.toStdString()));
        ScintillaEditBase view;
        const char* text = "MiTipo x;";
        view.sends(SCI_SETTEXT, 0, text);
        SyntaxStyler(_langs, custom).apply(view, LangType::Cpp);
        view.send(SCI_COLOURISE, 0, -1);
        QCOMPARE(view.send(SCI_GETSTYLEAT, 0), sptr_t{SCE_C_WORD2});
    }

    void globalOverrideForcesColours()
    {
        StyleSheet custom = _styles;
        custom.globalOverride().foreground = true;
        ScintillaEditBase view;
        view.sends(SCI_SETTEXT, 0, "int x;");
        SyntaxStyler(_langs, custom).apply(view, LangType::Cpp);
        const Style* over = custom.globalByName("Global override");
        QVERIFY(over);
        if (over->colorStyle & kColorStyleForeground)
            QCOMPARE(view.send(SCI_STYLEGETFORE, SCE_C_WORD), sptr_t{over->fgColor});
    }

    void langTabSettings()
    {
        // tabSettings de langs.xml: Python suele declarar espacios
        const LanguageDefinition* py = _langs.find(LangType::Python);
        QVERIFY(py);
        ScintillaEditBase view;
        SyntaxStyler(_langs, _styles).apply(view, LangType::Python);
        if (py->tabSettings > 0 && (py->tabSettings & 0x7F) > 0) {
            QCOMPARE(view.send(SCI_GETTABWIDTH), sptr_t{py->tabSettings & 0x7F});
            QCOMPARE(view.send(SCI_GETUSETABS), sptr_t{(py->tabSettings & 0x80) ? 0 : 1});
        } else {
            QCOMPARE(view.send(SCI_GETTABWIDTH), sptr_t{4});
        }
    }

    // --- Criterio: detección de lenguaje ------------------------------------------------

    void detection_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<edita::LangType>("lang");
        QTest::newRow("foo.py") << "/x/foo.py" << QByteArray() << LangType::Python;
        QTest::newRow("FOO.CPP mayúsculas") << "/x/FOO.CPP" << QByteArray() << LangType::Cpp;
        QTest::newRow("main.c") << "/x/main.c" << QByteArray() << LangType::C;
        QTest::newRow("index.html") << "/x/index.html" << QByteArray() << LangType::Html;
        QTest::newRow("app.js") << "/x/app.js" << QByteArray() << LangType::JavaScript;
        QTest::newRow("app.ts") << "/x/app.ts" << QByteArray() << LangType::TypeScript;
        QTest::newRow("data.json") << "/x/data.json" << QByteArray() << LangType::Json;
        QTest::newRow("Makefile") << "/x/Makefile" << QByteArray() << LangType::Makefile;
        QTest::newRow("GNUmakefile") << "/x/GNUmakefile" << QByteArray() << LangType::Makefile;
        QTest::newRow("CMakeLists.txt") << "/x/CMakeLists.txt" << QByteArray() << LangType::CMake;
        QTest::newRow("PKGBUILD") << "/x/PKGBUILD" << QByteArray() << LangType::Bash;
        QTest::newRow("Rakefile") << "/x/Rakefile" << QByteArray() << LangType::Ruby;
        QTest::newRow("shebang bash") << "/x/script" << QByteArray("#!/bin/bash\necho") << LangType::Bash;
        QTest::newRow("shebang env python") << "/x/script" << QByteArray("#!/usr/bin/env python3\n") << LangType::Python;
        QTest::newRow("shebang node") << "/x/cli" << QByteArray("#!/usr/bin/env node\n") << LangType::JavaScript;
        QTest::newRow("shebang desconocido") << "/x/s" << QByteArray("#!/usr/bin/awk -f\n") << LangType::Text;
        QTest::newRow("<?xml") << "/x/sin-extension" << QByteArray("<?xml version=\"1.0\"?>\n<a/>") << LangType::Xml;
        QTest::newRow("<!DOCTYPE html") << "/x/pagina" << QByteArray("\n  <!DOCTYPE html>\n") << LangType::Html;
        QTest::newRow("<?php") << "/x/p" << QByteArray("<?php echo 1;") << LangType::Php;
        QTest::newRow("la extensión manda") << "/x/a.py" << QByteArray("<?xml version=\"1.0\"?>") << LangType::Python;
        // .txt es "texto normal", y entonces N++ mira el contenido
        QTest::newRow(".txt con shebang") << "/x/a.txt" << QByteArray("#!/bin/bash\n") << LangType::Bash;
        QTest::newRow("sin nada") << "/x/notas" << QByteArray("hola") << LangType::Text;
    }

    void detection()
    {
        QFETCH(QString, path);
        QFETCH(QByteArray, text);
        QFETCH(edita::LangType, lang);
        QCOMPARE(detectLanguage(path, text.toStdString(), _langs, _styles), lang);
    }

    void userExtensionInStylersWins()
    {
        // En el Configurador de estilos de N++ el usuario puede asociar extensiones
        QString xml = QString::fromUtf8(readStylers());
        xml.replace(QStringLiteral("<LexerType name=\"python\" desc=\"Python\" ext=\"\">"),
                    QStringLiteral("<LexerType name=\"python\" desc=\"Python\" ext=\"miext txt\">"));
        StyleSheet custom;
        QVERIFY(custom.loadFromString(xml.toStdString()));
        QCOMPARE(detectLanguage("/x/a.miext", {}, _langs, custom), LangType::Python);
        QCOMPARE(detectLanguage("/x/a.txt", {}, _langs, custom), LangType::Python);
    }

    // --- Criterio: recarga y temas ------------------------------------------------------

    void managerCopiesModelsOnFirstRunAndReloads()
    {
        QTemporaryDir config;
        SyntaxManager manager(config.path());
        QCOMPARE(manager.initialize(), QStringList());
        QVERIFY(QFileInfo::exists(config.filePath("langs.xml")));
        QVERIFY(QFileInfo::exists(config.filePath("stylers.xml")));

        // Cambiar un color en el stylers.xml del usuario y recargar
        QFile f(config.filePath("stylers.xml"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QByteArray content = f.readAll();
        f.close();
        const QByteArray from = "name=\"INSTRUCTION WORD\" styleID=\"5\" fgColor=\"0000FF\"";
        QVERIFY(content.contains(from));
        content.replace(from, "name=\"INSTRUCTION WORD\" styleID=\"5\" fgColor=\"00AA00\"");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(content);
        f.close();

        QSignalSpy changed(&manager, &SyntaxManager::stylesChanged);
        QCOMPARE(manager.reload(), QStringList());
        QCOMPARE(changed.count(), 1);
        ScintillaEditBase view;
        manager.apply(view, LangType::Cpp);
        QCOMPARE(view.send(SCI_STYLEGETFORE, SCE_C_WORD), sptr_t{bgr(0x00AA00)});
        QVERIFY(manager.isActiveConfigFile(config.filePath("stylers.xml")));
    }

    void corruptUserFileFallsBackToModel()
    {
        QTemporaryDir config;
        QFile bad(config.filePath("langs.xml"));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("<NotepadPlus><Languages><Language name=");
        bad.close();
        SyntaxManager manager(config.path());
        const QStringList warnings = manager.initialize();
        QCOMPARE(warnings.size(), 1);
        QCOMPARE(manager.languages().languages().size(), std::size_t{96});
    }

    void themesAreListedAppliedAndRemembered()
    {
        QTemporaryDir config;
        {
            SyntaxManager manager(config.path());
            manager.initialize();
            const auto themes = manager.themes();
            QCOMPARE(themes.front().path, QString()); // predeterminado
            const auto deepBlack = std::find_if(themes.begin(), themes.end(),
                                                [](const SyntaxManager::Theme& t) { return t.name == "Deep Black"; });
            QVERIFY(deepBlack != themes.end());
            QCOMPARE(themes.size(), std::size_t{31}); // predeterminado + 22 temas de N++ + los 2 de Edita + 6 modernos
            QVERIFY(manager.setTheme(deepBlack->path));

            StyleSheet expected;
            QVERIFY(expected.load(deepBlack->path));
            ScintillaEditBase view;
            manager.apply(view, LangType::Cpp);
            QCOMPARE(view.send(SCI_STYLEGETBACK, STYLE_DEFAULT), sptr_t{expected.globalById(STYLE_DEFAULT)->bgColor});
        }
        // Otra sesión recuerda el tema (config.xml)
        SyntaxManager again(config.path());
        again.initialize();
        QVERIFY(again.activeThemePath().endsWith(QStringLiteral("Deep Black.xml")));
        QVERIFY(again.setTheme(QString()));
        QCOMPARE(again.activeStylePath(), again.stylersPath());
    }

    void themeSavedWithAnOldPathIsFoundByName()
    {
        // El AppImage monta sus datos en una carpeta distinta en cada arranque
        // (/tmp/.mount_…): la ruta guardada ya no existe, pero el tema sí
        QTemporaryDir config;
        {
            SyntaxManager manager(config.path());
            manager.initialize();
            ConfigXml xml;
            QVERIFY(xml.load(manager.configPath()));
            xml.setGuiConfig(QStringLiteral("stylerTheme"), QStringLiteral("path"),
                             QStringLiteral("/tmp/.mount_EditaXYZ/usr/share/edita/themes/Edita Light.xml"));
            QVERIFY(xml.save());
        }
        SyntaxManager manager(config.path());
        const QStringList warnings = manager.initialize();
        QVERIFY2(warnings.filter(QStringLiteral("no longer exists")).isEmpty(), qPrintable(warnings.join('\n')));
        QVERIFY(QFileInfo::exists(manager.activeThemePath()));
        QCOMPARE(QFileInfo(manager.activeThemePath()).fileName(), QStringLiteral("Edita Light.xml"));
        // Y se guarda la ruta buena
        SyntaxManager again(config.path());
        QVERIFY(again.initialize().filter(QStringLiteral("no longer exists")).isEmpty());
        QCOMPARE(again.activeThemePath(), manager.activeThemePath());

        // Un tema que de verdad ya no está: aviso y el predeterminado
        {
            ConfigXml xml;
            QVERIFY(xml.load(manager.configPath()));
            xml.setGuiConfig(QStringLiteral("stylerTheme"), QStringLiteral("path"), QStringLiteral("/nada/Inexistente.xml"));
            QVERIFY(xml.save());
        }
        SyntaxManager missing(config.path());
        QCOMPARE(missing.initialize().filter(QStringLiteral("no longer exists")).size(), 1);
        QCOMPARE(missing.activeThemePath(), QString());
    }

private:
    QByteArray readStylers()
    {
        QFile f(dataFile("stylers.model.xml"));
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }
};

QTEST_MAIN(TestSyntax)
#include "tst_syntax.moc"
