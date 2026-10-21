// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 4: motor de búsqueda (port de FindReplaceDlg) y buscar en archivos.
// Cada caso se ejecuta con los dos backends (vista de Scintilla y documento sin
// vista del hilo de trabajo) y deben dar exactamente lo mismo.

#include "core/Codec.h"
#include "core/Locale.h"
#include "core/TextFile.h"
#include "search/FileSearch.h"
#include "search/SearchEngine.h"
#include "search/SearchTarget.h"

#include <ScintillaEditBase.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace edita;
using namespace std::string_literals;

Q_DECLARE_METATYPE(edita::SearchMode)

namespace {

struct ViewHolder
{
    ScintillaEditBase view;
    ViewSearchTarget target{view};
    explicit ViewHolder(const std::string& text)
    {
        view.send(SCI_SETCODEPAGE, SC_CP_UTF8);
        view.send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(text.c_str()));
        // SCI_SETTEXT corta en el primer NUL: el texto de los tests no lleva
    }
    std::string text() const { return target.text(0, target.length()); }
};

SearchOptions regex(const std::string& pattern, const std::string& replacement = {})
{
    SearchOptions o;
    o.pattern = pattern;
    o.replacement = replacement;
    o.mode = SearchMode::Regex;
    return o;
}

// Todas las coincidencias de todo el documento (Buscar todo)
std::vector<std::string> allMatches(SearchTarget& target, const SearchOptions& options)
{
    std::vector<std::string> out;
    const RangeResult r = processRange(target, options, RangeOperation::FindAll, 0, target.length(),
                                       [&](SciPos s, SciPos e) { out.push_back(target.text(s, e)); });
    if (r.invalidRegex)
        out.push_back("<invalid>");
    return out;
}

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QStringList relative(const QString& root, const QStringList& files)
{
    QStringList out;
    for (const QString& f : files)
        out << QDir(root).relativeFilePath(f);
    return out;
}

} // namespace

class TestSearch : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    QString _tree;

private slots:
    void initTestCase()
    {
        QVERIFY(!initGlobalLocale().empty()); // \w y [[:alpha:]] con letras no ASCII
        QVERIFY(_tmp.isValid());
        _tree = _tmp.filePath("arbol");
        writeFile(_tree + "/a.txt", "hola mundo\nadiós\n");
        writeFile(_tree + "/b.cpp", "int hola = 0;\n");
        writeFile(_tree + "/sub/c.txt", "Hola\nhola hola\n");
        writeFile(_tree + "/sub/deep/e.log", "registro: hola\n");
        writeFile(_tree + "/.oculto/d.txt", "hola escondida\n");
        writeFile(_tree + "/node_modules/f.txt", "hola dependencia\n");
        writeFile(_tree + "/Makefile", "hola:\n\techo hola\n");
    }

    // --- Modo extendido ---------------------------------------------------------------

    void extendedConversion_data()
    {
        QTest::addColumn<QByteArray>("input");
        QTest::addColumn<QByteArray>("output");
        QTest::newRow("tab y saltos") << QByteArray("a\\tb\\r\\nc") << QByteArray("a\tb\r\nc");
        QTest::newRow("barra") << QByteArray("a\\\\n") << QByteArray("a\\n");
        QTest::newRow("NUL") << QByteArray("x\\0y") << QByteArray("x\0y", 3);
        QTest::newRow("hex") << QByteArray("\\x41\\x42") << QByteArray("AB");
        QTest::newRow("hex no ASCII = carácter") << QByteArray("caf\\xE9") << QByteArray("caf\xC3\xA9");
        QTest::newRow("unicode") << QByteArray("\\u00F1\\u20AC") << QByteArray("\xC3\xB1\xE2\x82\xAC");
        QTest::newRow("decimal") << QByteArray("\\d065") << QByteArray("A");
        QTest::newRow("octal") << QByteArray("\\o101") << QByteArray("A");
        QTest::newRow("binario") << QByteArray("\\b01000001") << QByteArray("A");
        QTest::newRow("hex incompleto") << QByteArray("\\x4") << QByteArray("\\x4");
        QTest::newRow("hex inválido") << QByteArray("\\xZZ") << QByteArray("\\xZZ");
        QTest::newRow("desconocida") << QByteArray("\\q") << QByteArray("\\q");
        QTest::newRow("barra final") << QByteArray("a\\") << QByteArray("a\\");
    }

    void extendedConversion()
    {
        QFETCH(QByteArray, input);
        QFETCH(QByteArray, output);
        QCOMPARE(QByteArray::fromStdString(convertExtended(input.toStdString())), output);
    }

    // --- Regex: ≥ 25 casos, idénticos en los dos backends --------------------------------

    void regexFindAll_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("pattern");
        QTest::addColumn<bool>("matchCase");
        QTest::addColumn<bool>("wholeWord");
        QTest::addColumn<bool>("dotNewline");
        QTest::addColumn<QStringList>("expected");
        auto row = [](const char* name, const QString& text, const QString& pattern, const QStringList& expected,
                      bool matchCase = false, bool wholeWord = false, bool dotNewline = false) {
            QTest::newRow(name) << text << pattern << matchCase << wholeWord << dotNewline << expected;
        };
        row("lookbehind", "ab cb ab", "(?<=a)b", {"b", "b"});
        row("lookahead", "ab ac", "a(?=b)", {"a"});
        row("lookbehind negativo", "ab cb", "(?<!a)b", {"b"});
        row("\\R", "a\r\nb\nc\rd", "\\R", {"\r\n", "\n", "\r"});
        row("^ por línea", "uno dos\ntres", "^\\w+", {"uno", "tres"});
        row("$ por línea", "uno dos\ntres", "\\w+$", {"dos", "tres"});
        row(". sin salto", "a\nb", "a.b", {});
        row("(?s)", "a\nb", "(?s)a.b", {"a\nb"});
        row("opción .=salto", "a\nb", "a.b", {"a\nb"}, false, false, true);
        row("(?-s) gana a la opción", "a\nb", "(?-s)a.b", {}, false, false, true);
        row("recursión (?R)", "f(a(b)c) (d)", "\\((?>[^()]|(?R))*\\)", {"(a(b)c)", "(d)"});
        row("sin mayúsculas", "Hola HOLA hola", "hola", {"Hola", "HOLA", "hola"});
        row("con mayúsculas", "Hola HOLA hola", "hola", {"hola"}, true);
        // "Palabra completa" no se aplica a una regex (N++ desactiva la casilla en modo regex)
        row("palabra completa ignorada", "cat concat cat.", "cat", {"cat", "cat", "cat"}, false, true);
        row("\\b y \\w con ñ", "año ñu", "\\bñ\\w", {"ñu"});
        row("[[:alpha:]] con acentos", "año, über", "[[:alpha:]]+", {"año", "über"});
        row("cuantificador", "a123b4567", "\\d{3}", {"123", "456"});
        row("alternativa", "gato perro pez", "gato|pez", {"gato", "pez"});
        row("no codicioso", "<a><b>", "<.+?>", {"<a>", "<b>"});
        row("codicioso", "<a><b>", "<.+>", {"<a><b>"});
        row("referencia atrás", "aabbc", "(\\w)\\1", {"aa", "bb"});
        row("\\x{…} Unicode", "café", "\\x{00E9}", {"é"});
        row("\\h", "a \t b", "\\h+", {" \t "});
        row("clase negada multilínea", "a\nb", "[^x]+", {"a\nb"});
        row("^ vacío por línea", "a\nb\n\nc", "^", {"", "", "", ""});
        row("$ vacío por línea", "a\nb\n\nc", "$", {"", "", "", ""});
        row("emoji (4 bytes)", "x😀y😀", "😀", {"😀", "😀"});
        row("regex inválida", "abc", "(", {"<invalid>"});
    }

    void regexFindAll()
    {
        QFETCH(QString, text);
        QFETCH(QString, pattern);
        QFETCH(bool, matchCase);
        QFETCH(bool, wholeWord);
        QFETCH(bool, dotNewline);
        QFETCH(QStringList, expected);
        SearchOptions o = regex(pattern.toStdString());
        o.matchCase = matchCase;
        o.wholeWord = wholeWord;
        o.dotMatchesNewline = dotNewline;

        ViewHolder view(text.toStdString());
        DocumentSearchTarget doc(text.toStdString());
        const auto fromView = allMatches(view.target, o);
        const auto fromDoc = allMatches(doc, o);
        QCOMPARE(fromDoc, fromView); // los dos backends, idénticos
        QStringList got;
        for (const std::string& m : fromView)
            got << QString::fromStdString(m);
        QCOMPARE(got, expected);
    }

    void regexReplaceAll_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("pattern");
        QTest::addColumn<QString>("replacement");
        QTest::addColumn<QString>("expected");
        QTest::addColumn<int>("count");
        QTest::newRow("$1 $2") << "ana@casa" << "(\\w+)@(\\w+)" << "$2 de $1" << "casa de ana" << 1;
        QTest::newRow("\\1") << "a1b22" << "(\\d+)" << "[\\1]" << "a[1]b[22]" << 2;
        QTest::newRow("\\U…\\E") << "hola mundo" << "(\\w+)" << "\\U$1\\E!" << "HOLA! MUNDO!" << 2;
        QTest::newRow("condicional") << "a1 b" << "(\\d)|b" << "(?1N:L)" << "aN L" << 2;
        QTest::newRow("prefijo con ^") << "a\nb\n\nc" << "^" << "> " << "> a\n> b\n> \n> c" << 4;
        QTest::newRow("espacios finales") << "a  \nb \nc" << "\\h+$" << "" << "a\nb\nc" << 2;
        QTest::newRow("CRLF → LF") << "a\r\nb\r\n" << "\\r\\n" << "\\n" << "a\nb\n" << 2;
        QTest::newRow("borrar líneas vacías") << "a\n\n\nb\n" << "^\\R" << "" << "a\nb\n" << 2;
        QTest::newRow("ñ con \\w") << "año niño" << "\\w+" << "[$0]" << "[año] [niño]" << 2;
        QTest::newRow("$& todo") << "x y" << "\\w" << "<$&>" << "<x> <y>" << 2;
    }

    void regexReplaceAll()
    {
        QFETCH(QString, text);
        QFETCH(QString, pattern);
        QFETCH(QString, replacement);
        QFETCH(QString, expected);
        QFETCH(int, count);
        const SearchOptions o = regex(pattern.toStdString(), replacement.toStdString());

        ViewHolder view(text.toStdString());
        DocumentSearchTarget doc(text.toStdString());
        const RangeResult rv = processRange(view.target, o, RangeOperation::ReplaceAll, 0, view.target.length());
        const RangeResult rd = processRange(doc, o, RangeOperation::ReplaceAll, 0, doc.length());
        QCOMPARE(rv.count, count);
        QCOMPARE(rd.count, count);
        QCOMPARE(QString::fromStdString(view.text()), expected);
        QCOMPARE(QString::fromStdString(std::string(doc.allText())), expected);
    }

    void normalAndExtendedModes()
    {
        SearchOptions o;
        o.pattern = "a.b";
        ViewHolder v("axb a.b");
        QCOMPARE(allMatches(v.target, o), std::vector<std::string>{"a.b"}); // normal: el punto es literal

        SearchOptions word;
        word.pattern = "cat";
        word.wholeWord = true;
        DocumentSearchTarget words("cat concat cat.");
        QCOMPARE(allMatches(words, word), (std::vector<std::string>{"cat", "cat"}));

        SearchOptions ext;
        ext.mode = SearchMode::Extended;
        ext.pattern = "\\t";
        ext.replacement = "\\n";
        DocumentSearchTarget doc("a\tb\tc");
        QCOMPARE(processRange(doc, ext, RangeOperation::ReplaceAll, 0, doc.length()).count, 2);
        QCOMPARE(std::string(doc.allText()), "a\nb\nc"s);

        // En modo normal la "plantilla" no se interpreta
        SearchOptions literal;
        literal.pattern = "x";
        literal.replacement = "$1\\n";
        DocumentSearchTarget d2("x");
        processRange(d2, literal, RangeOperation::ReplaceAll, 0, d2.length());
        QCOMPARE(std::string(d2.allText()), "$1\\n"s);
    }

    // --- Buscar siguiente / reemplazar -----------------------------------------------

    void findNextWrapsAndReportsStatus()
    {
        ViewHolder v("uno dos uno");
        SearchOptions o;
        o.pattern = "uno";
        FindResult r = findNext(v.target, o, 0, 0);
        QCOMPARE(r.status, FindStatus::Found);
        QCOMPARE(r.start, SciPos{0});
        r = findNext(v.target, o, r.start, r.end);
        QCOMPARE(r.start, SciPos{8});
        r = findNext(v.target, o, r.start, r.end);
        QCOMPARE(r.status, FindStatus::EndReached); // dio la vuelta
        QCOMPARE(r.start, SciPos{0});

        o.wrapAround = false;
        QCOMPARE(findNext(v.target, o, 8, 11).status, FindStatus::NotFound);

        o.wrapAround = true;
        o.searchUp = true;
        r = findNext(v.target, o, 8, 11);
        QCOMPARE(r.status, FindStatus::Found);
        QCOMPARE(r.start, SciPos{0});
        r = findNext(v.target, o, 0, 3);
        QCOMPARE(r.status, FindStatus::TopReached);
        QCOMPARE(r.start, SciPos{8});

        o.pattern = "nada";
        QCOMPARE(findNext(v.target, o, 0, 0).status, FindStatus::NotFound);
        o = regex("([");
        r = findNext(v.target, o, 0, 0);
        QCOMPARE(r.status, FindStatus::InvalidRegex);
        QVERIFY(!r.regexError.empty());
    }

    void replaceNextLikeNotepadPlusPlus()
    {
        ViewHolder v("uno dos uno");
        SearchOptions o;
        o.pattern = "uno";
        o.replacement = "1";
        // La selección no es una coincidencia: solo se selecciona la siguiente
        ReplaceOutcome r = replaceNext(v.target, o, 0, 0);
        QVERIFY(!r.replaced);
        QCOMPARE(r.selStart, SciPos{0});
        QCOMPARE(r.selEnd, SciPos{3});
        // Ahora sí: se reemplaza y se selecciona la siguiente
        r = replaceNext(v.target, o, r.selStart, r.selEnd);
        QVERIFY(r.replaced);
        QCOMPARE(v.text(), "1 dos uno"s);
        QCOMPARE(r.selStart, SciPos{6});
        QCOMPARE(r.selEnd, SciPos{9});
        r = replaceNext(v.target, o, r.selStart, r.selEnd);
        QVERIFY(r.replaced);
        QCOMPARE(v.text(), "1 dos 1"s);
        QVERIFY(!r.next.found());

        // Regex con referencias en "Reemplazar" (SCI_REPLACETARGETRE)
        ViewHolder v2("x=1 y=2");
        const SearchOptions re = regex("(\\w)=(\\d)", "$2=$1");
        r = replaceNext(v2.target, re, 0, 0);
        r = replaceNext(v2.target, re, r.selStart, r.selEnd);
        QCOMPARE(v2.text(), "1=x y=2"s);
    }

    void rangeForOperations()
    {
        SearchOptions o;
        o.wrapAround = false;
        QCOMPARE(rangeForOperation(RangeOperation::ReplaceAll, o, 5, 8, 20, false), (std::pair<SciPos, SciPos>{5, 20}));
        o.searchUp = true;
        QCOMPARE(rangeForOperation(RangeOperation::ReplaceAll, o, 5, 8, 20, false), (std::pair<SciPos, SciPos>{0, 8}));
        o.wrapAround = true;
        QCOMPARE(rangeForOperation(RangeOperation::CountAll, o, 5, 8, 20, false), (std::pair<SciPos, SciPos>{0, 20}));
        o.inSelection = true;
        QCOMPARE(rangeForOperation(RangeOperation::CountAll, o, 5, 8, 20, false), (std::pair<SciPos, SciPos>{5, 8}));
        QCOMPARE(rangeForOperation(RangeOperation::ReplaceAll, o, 5, 8, 20, false), (std::pair<SciPos, SciPos>{5, 8}));
    }

    void findAllLinesMergesSameLine()
    {
        DocumentSearchTarget doc("Hola\nhola hola\nnada\n");
        SearchOptions o;
        o.pattern = "hola";
        RangeResult r;
        const auto lines = findAllLines(doc, o, 0, doc.length(), r);
        QCOMPARE(r.count, 3);
        QCOMPARE(lines.size(), std::size_t{2});
        QCOMPARE(lines[0].line, SciPos{0});
        QCOMPARE(lines[1].line, SciPos{1});
        QCOMPARE(lines[1].text, "hola hola"s);
        QCOMPARE(lines[1].segments, (std::vector<std::pair<int, int>>{{0, 4}, {5, 9}}));
        QCOMPARE(lines[1].ranges.front(), (std::pair<SciPos, SciPos>{5, 9}));
    }

    void veryLongLinesAreCut()
    {
        const std::string longLine = std::string(5000, 'x') + "aguja";
        DocumentSearchTarget doc(longLine);
        SearchOptions o;
        o.pattern = "aguja";
        RangeResult r;
        const auto lines = findAllLines(doc, o, 0, doc.length(), r);
        QCOMPARE(r.count, 1);
        QVERIFY(lines[0].text.size() < kResultLineMaxLength);
        QVERIFY(lines[0].text.ends_with(" ..."));
    }

    // --- Filtros y lista de archivos ------------------------------------------------------

    void filters()
    {
        QCOMPARE(parseFilters(""), std::vector<QString>{"*.*"});
        QCOMPARE(parseFilters("!*.log"), (std::vector<QString>{"*.*", "!*.log"}));
        QCOMPARE(parseFilters(" *.cpp   *.h "), (std::vector<QString>{"*.cpp", "*.h"}));
        QVERIFY(wildcardMatch("Makefile", "*.*"));
        QVERIFY(wildcardMatch("A.CPP", "*.cpp"));
        QVERIFY(!wildcardMatch("a.cpp.bak", "*.cpp"));
        QVERIFY(wildcardMatch("x.txt", "?.txt"));
        QVERIFY(!wildcardMatch("xy.txt", "?.txt"));
        QVERIFY(matchesFileFilters("a.txt", parseFilters("*.txt !b*")));
        QVERIFY(!matchesFileFilters("b.txt", parseFilters("*.txt !b*")));
        QVERIFY(isExcludedDirectory("node_modules", parseFilters("!\\node_modules"), 1));
        QVERIFY(!isExcludedDirectory("node_modules", parseFilters("!\\node_modules"), 2));
        QVERIFY(isExcludedDirectory("deep", parseFilters("!+/deep"), 3));
    }

    void listingOrderRecursionAndHidden()
    {
        const auto all = parseFilters("");
        QCOMPARE(relative(_tree, listMatchingFiles(_tree, all, true, false)),
                 (QStringList{"a.txt", "b.cpp", "Makefile", "node_modules/f.txt", "sub/c.txt", "sub/deep/e.log"}));
        QCOMPARE(relative(_tree, listMatchingFiles(_tree, all, true, true)).first(), QStringLiteral(".oculto/d.txt"));
        QCOMPARE(relative(_tree, listMatchingFiles(_tree, all, false, false)), (QStringList{"a.txt", "b.cpp", "Makefile"}));
        QCOMPARE(relative(_tree, listMatchingFiles(_tree, parseFilters("*.txt !\\node_modules"), true, false)),
                 (QStringList{"a.txt", "sub/c.txt"}));
        QCOMPARE(relative(_tree, listMatchingFiles(_tree, parseFilters("!*.log !+\\deep !Makefile"), true, false)),
                 (QStringList{"a.txt", "b.cpp", "node_modules/f.txt", "sub/c.txt"}));
    }

    // --- Buscar / reemplazar en archivos ---------------------------------------------------

    void findInFilesExactHits()
    {
        FileSearchRequest req;
        req.directory = _tree;
        req.filters = "*.txt *.cpp";
        req.options.pattern = "hola";
        std::vector<FileHits> results;
        std::atomic<bool> cancel{false};
        const FileSearchSummary s = runFileSearch(req, cancel, [&](FileHits&& h) { results.push_back(std::move(h)); });
        QCOMPARE(s.filesSearched, 4);
        QCOMPARE(s.filesWithHits, 4);
        QCOMPARE(s.totalHits, 6);
        QStringList summary;
        for (const FileHits& f : results) {
            for (const LineHit& l : f.lines)
                summary << QStringLiteral("%1:%2:%3").arg(QDir(_tree).relativeFilePath(f.path)).arg(l.line + 1)
                               .arg(QString::fromStdString(l.text));
        }
        QCOMPARE(summary, (QStringList{"a.txt:1:hola mundo", "b.cpp:1:int hola = 0;", "node_modules/f.txt:1:hola dependencia",
                                        "sub/c.txt:1:Hola", "sub/c.txt:2:hola hola"}));

        // Coincidir mayúsculas y sin recursión
        req.options.matchCase = true;
        req.recursive = false;
        results.clear();
        QCOMPARE(runFileSearch(req, cancel, [&](FileHits&& h) { results.push_back(std::move(h)); }).totalHits, 2);
    }

    void findInFilesUsesOpenDocumentText()
    {
        FileSearchRequest req;
        req.directory = _tree;
        req.filters = "a.txt";
        req.options.pattern = "hola";
        req.openDocuments[QDir::cleanPath(_tree + "/a.txt")] = "texto sin guardar: hola hola\n";
        std::vector<FileHits> results;
        std::atomic<bool> cancel{false};
        runFileSearch(req, cancel, [&](FileHits&& h) { results.push_back(std::move(h)); });
        QCOMPARE(results.size(), std::size_t{1});
        QCOMPARE(results[0].count, 2);
    }

    void replaceInFilesPreservesEncodingBomAndEol()
    {
        const QString dir = _tmp.filePath("reemplazo");
        const QStringList fixtures{"encoding/windows-1252.txt", "encoding/utf8-bom.txt", "encoding/utf16le-bom.txt",
                                   "encoding/shift_jis.txt", "eol/crlf.txt", "eol/cr.txt"};
        std::map<QString, std::pair<std::string, TextEncoding>> expected;
        int filesWithO = 0;
        for (const QString& fx : fixtures) {
            const QString dst = dir + "/" + QFileInfo(fx).fileName();
            writeFile(dst, readAll(QStringLiteral(EDITA_FIXTURES_DIR "/") + fx));
            std::string text;
            const LoadResult loaded = loadTextFile(dst, LoadOptions{}, text);
            std::string replaced;
            if (text.find('o') != std::string::npos)
                ++filesWithO; // el japonés no tiene ninguna
            for (const char c : text)
                replaced += (c == 'o') ? std::string("0") : std::string(1, c);
            expected[dst] = {encodeAll(replaced, loaded.encoding), loaded.encoding};
        }

        FileSearchRequest req;
        req.directory = dir;
        req.replace = true;
        req.options.pattern = "o";
        req.options.replacement = "0";
        req.options.matchCase = true;
        std::atomic<bool> cancel{false};
        const FileSearchSummary s = runFileSearch(req, cancel, {});
        QVERIFY(s.errors.isEmpty());
        QCOMPARE(s.filesWithHits, filesWithO);
        QCOMPARE(filesWithO, 5);
        for (const auto& [path, exp] : expected)
            QVERIFY2(readAll(path) == QByteArray::fromStdString(exp.first), qPrintable(path));
        QCOMPARE(readAll(dir + "/crlf.txt"), QByteArray("un0\r\nd0s\r\ntres\r\n"));
    }

    void invalidRegexInFiles()
    {
        FileSearchRequest req;
        req.directory = _tree;
        req.options = regex("(");
        std::atomic<bool> cancel{false};
        const FileSearchSummary s = runFileSearch(req, cancel, {});
        QVERIFY(s.invalidRegex);
        QVERIFY(!s.regexError.isEmpty());
    }

    void cancelOnTenThousandFilesIsFast()
    {
        const QString dir = _tmp.filePath("muchos");
        for (int i = 0; i < 10000; ++i)
            writeFile(QStringLiteral("%1/d%2/f%3.txt").arg(dir).arg(i / 500).arg(i), "línea con aguja\notra línea\n");

        FileSearchRequest req;
        req.directory = dir;
        req.options.pattern = "aguja";
        FileSearchJob job(req);
        QSignalSpy progress(&job, &FileSearchJob::progress);
        QSignalSpy finished(&job, &FileSearchJob::finished);
        job.start();
        QVERIFY(progress.wait(5000)); // ya está trabajando; el hilo principal sigue libre
        QElapsedTimer timer;
        timer.start();
        job.cancel();
        QVERIFY(finished.wait(2000));
        const qint64 elapsed = timer.elapsed();
        QVERIFY2(elapsed < 500, qPrintable(QStringLiteral("%1 ms").arg(elapsed)));
        const auto summary = finished.first().first().value<FileSearchSummary>();
        QVERIFY(summary.cancelled);
        QVERIFY(summary.filesSearched < 10000);
    }

    void jobDeliversAllHits()
    {
        FileSearchRequest req;
        req.directory = _tree;
        req.options.pattern = "hola";
        FileSearchJob job(req);
        QSignalSpy files(&job, &FileSearchJob::fileFound);
        QSignalSpy finished(&job, &FileSearchJob::finished);
        job.start();
        QVERIFY(finished.wait(5000));
        QCOMPARE(files.count(), 6); // todo menos el directorio oculto
    }
};

QTEST_MAIN(TestSearch)
#include "tst_search.moc"
