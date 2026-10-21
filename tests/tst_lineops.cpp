// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 12: operaciones con líneas sin Scintilla (Sorters.h, changeCase y
// wsTabConvert de N++).

#include "core/LineOperations.h"

#include <QTest>

#include <algorithm>

using namespace edita;
using Lines = std::vector<std::string>;

namespace {

Lines sorted(Lines lines, SortKind kind, bool descending = false, const Lines& keys = {})
{
    SortRequest request;
    request.kind = kind;
    request.descending = descending;
    request.seed = 7;
    const std::ptrdiff_t failed = sortLines(lines, keys, request);
    return failed < 0 ? lines : Lines{"<falló en " + std::to_string(failed) + ">"};
}

// Columna visual de cada carácter: tabulaciones expandidas con ancho 4
std::string expanded(const std::string& line)
{
    return convertTabs(line, 4, TabConversion::TabsToSpaces);
}

} // namespace

class TestLineOps : public QObject
{
    Q_OBJECT

private slots:
    void lexicographicSorts()
    {
        const Lines lines{"pera", "Manzana", "uva", "banana"};
        QCOMPARE(sorted(lines, SortKind::Lexicographic), (Lines{"Manzana", "banana", "pera", "uva"}));
        QCOMPARE(sorted(lines, SortKind::Lexicographic, true), (Lines{"uva", "pera", "banana", "Manzana"}));
        QCOMPARE(sorted(lines, SortKind::LexicographicCaseInsensitive), (Lines{"banana", "Manzana", "pera", "uva"}));
        QCOMPARE(sorted(lines, SortKind::LexicographicCaseInsensitive, true), (Lines{"uva", "pera", "Manzana", "banana"}));
        QCOMPARE(sorted({"b", "a", "c"}, SortKind::Locale), (Lines{"a", "b", "c"}));
        // Por columnas: se ordena por la clave, las líneas van enteras
        QCOMPARE(sorted({"x 3", "y 1", "z 2"}, SortKind::Lexicographic, false, {"3", "1", "2"}), (Lines{"y 1", "z 2", "x 3"}));
    }

    void integerSortLikeNotepadPlusPlus()
    {
        // Números dentro del texto, negativos, ceros a la izquierda y números enormes
        QCOMPARE(sorted({"10", "9", "-5", "abc", "2"}, SortKind::Integer), (Lines{"-5", "2", "9", "10", "abc"}));
        QCOMPARE(sorted({"file10", "file9", "file1"}, SortKind::Integer), (Lines{"file1", "file9", "file10"}));
        QVERIFY(compareWithIntegers("007", "7") < 0);
        QVERIFY(compareWithIntegers("-10", "-9") < 0);
        QVERIFY(compareWithIntegers("123456789012345678901234567890", "123456789012345678901234567891") < 0);
        QCOMPARE(compareWithIntegers("a1b", "a1b"), 0);
        QCOMPARE(sorted({"3", "20", "100"}, SortKind::Integer, true), (Lines{"100", "20", "3"}));
    }

    void decimalSorts()
    {
        // Las vacías van al principio (al final si es descendente)
        QCOMPARE(sorted({"1,5", "-2", "10", "", "3,25"}, SortKind::DecimalComma), (Lines{"", "-2", "1,5", "3,25", "10"}));
        QCOMPARE(sorted({"1,5", "-2", "10", "", "3,25"}, SortKind::DecimalComma, true),
                 (Lines{"10", "3,25", "1,5", "-2", ""}));
        QCOMPARE(sorted({"2.5 kg", " 1.25", "10"}, SortKind::DecimalDot), (Lines{" 1.25", "2.5 kg", "10"}));
        // Sin cifras al principio cuenta como vacía (como en N++); un "-" suelto no es un número
        QCOMPARE(sorted({"2", "abc", "1"}, SortKind::DecimalDot), (Lines{"abc", "1", "2"}));
        Lines lines{"1.5", "-", "2"};
        const Lines original = lines;
        QCOMPARE(sortLines(lines, {}, SortRequest{SortKind::DecimalDot, false, 0}), std::ptrdiff_t{1});
        QCOMPARE(lines, original);
    }

    void lengthReverseAndRandom()
    {
        QCOMPARE(sorted({"ccc", "a", "ññ"}, SortKind::Length), (Lines{"a", "ññ", "ccc"})); // en caracteres, no bytes
        QCOMPARE(sorted({"ccc", "a", "bb"}, SortKind::Length, true), (Lines{"ccc", "bb", "a"}));
        QCOMPARE(sorted({"1", "2", "3"}, SortKind::Reverse), (Lines{"3", "2", "1"}));
        const Lines lines{"1", "2", "3", "4", "5", "6", "7", "8"};
        Lines random = sorted(lines, SortKind::Random);
        QCOMPARE(sorted(lines, SortKind::Random), random); // misma semilla, mismo orden
        std::sort(random.begin(), random.end());
        QCOMPARE(random, lines);
    }

    void duplicatesAndEmptyLines()
    {
        Lines lines{"a", "b", "a", "a", "c", "b"};
        Lines consecutive = lines;
        QVERIFY(removeAnyDuplicateLines(lines));
        QCOMPARE(lines, (Lines{"a", "b", "c"}));
        QVERIFY(!removeAnyDuplicateLines(lines));
        QVERIFY(removeConsecutiveDuplicateLines(consecutive));
        QCOMPARE(consecutive, (Lines{"a", "b", "a", "c", "b"}));

        Lines blank{"a", "", " \t", "b", ""};
        Lines blank2 = blank;
        QVERIFY(removeEmptyLines(blank, false));
        QCOMPARE(blank, (Lines{"a", " \t", "b"}));
        QVERIFY(removeEmptyLines(blank2, true));
        QCOMPARE(blank2, (Lines{"a", "b"}));
    }

    void caseConversions()
    {
        QCOMPARE(changeCase("ñandú Ärger", TextCase::Upper), QStringLiteral("ÑANDÚ ÄRGER"));
        QCOMPARE(changeCase("ÑANDÚ", TextCase::Lower), QStringLiteral("ñandú"));
        QCOMPARE(changeCase("hello wORLD don't", TextCase::ProperForce), QStringLiteral("Hello World Don't"));
        QCOMPARE(changeCase("hello wORLD", TextCase::ProperBlend), QStringLiteral("Hello WORLD"));
        QCOMPARE(changeCase("hELLO. how are you? i am fine", TextCase::SentenceForce),
                 QStringLiteral("Hello. How are you? I am fine"));
        QCOMPARE(changeCase("hello NASA. bye", TextCase::SentenceBlend), QStringLiteral("Hello NASA. Bye"));
        // Una línea en blanco también empieza oración; un solo salto de línea no
        QCOMPARE(changeCase("uno\ndos\n\ntres", TextCase::SentenceForce), QStringLiteral("Uno\ndos\n\nTres"));
        QCOMPARE(changeCase("AbC ñ", TextCase::Invert), QStringLiteral("aBc Ñ"));
        const QString random = changeCase("abcdefghij", TextCase::Random, 3);
        QCOMPARE(random, changeCase("abcdefghij", TextCase::Random, 3));
        QCOMPARE(random.toLower(), QStringLiteral("abcdefghij"));
    }

    void tabsAndSpaces()
    {
        QCOMPARE(convertTabs("\ta\tb", 4, TabConversion::TabsToSpaces), std::string("    a   b"));
        QCOMPARE(convertTabs("ñ\tx", 4, TabConversion::TabsToSpaces), std::string("ñ   x")); // UTF-8: una columna
        QCOMPARE(convertTabs("        x    y", 4, TabConversion::SpacesToTabsLeading), std::string("\t\tx    y"));
        QCOMPARE(convertTabs("    x", 4, TabConversion::SpacesToTabsAll), std::string("\tx"));
        QCOMPARE(convertTabs("a b", 4, TabConversion::SpacesToTabsAll), std::string("a b")); // un espacio suelto se queda

        // La conversión no mueve nada en pantalla
        for (const std::string line : {"        x    y", "ab      cd  e", "  \t  x", "x   \ty    z"}) {
            QCOMPARE(expanded(convertTabs(line, 4, TabConversion::SpacesToTabsAll)), expanded(line));
            QCOMPARE(expanded(convertTabs(line, 4, TabConversion::SpacesToTabsLeading)), expanded(line));
        }

        // El cursor sigue junto al mismo carácter
        std::ptrdiff_t caret = 2; // antes de la "b" en "a\tb"
        QCOMPARE(convertTabs("a\tb", 4, TabConversion::TabsToSpaces, &caret), std::string("a   b"));
        QCOMPARE(caret, std::ptrdiff_t{4});
        caret = 3;
        QCOMPARE(convertTabs("abc", 4, TabConversion::TabsToSpaces, &caret), std::string("abc"));
        QCOMPARE(caret, std::ptrdiff_t{3}); // sin cambios, no se toca
    }
};

QTEST_GUILESS_MAIN(TestLineOps)
#include "tst_lineops.moc"
