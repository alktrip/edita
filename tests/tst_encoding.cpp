// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 1: tabla de codepages, detección (port de Utf8_16_Read/EncodingMapper),
// conversión por bloques y fin de línea. Sin archivos ni UI.

#include "core/Codec.h"
#include "core/Encoding.h"
#include "core/EncodingDetection.h"
#include "core/Eol.h"

#include <QTest>

#include <iconv.h>

#include <string>

using namespace edita;
using namespace std::string_literals;

Q_DECLARE_METATYPE(edita::UniMode)
Q_DECLARE_METATYPE(edita::EolType)

namespace {

// Decodifica `bytes` en trozos de `chunk` bytes
std::string decodeInChunks(std::string_view bytes, const TextEncoding& enc, std::size_t chunk)
{
    Decoder decoder(enc);
    std::string out;
    for (std::size_t pos = 0; pos < bytes.size(); pos += chunk) {
        const std::size_t len = std::min(chunk, bytes.size() - pos);
        decoder.decode(bytes.substr(pos, len), pos + len == bytes.size(), out);
    }
    if (bytes.empty())
        decoder.decode({}, true, out);
    return out;
}

std::string encodeInChunks(std::string_view utf8, const TextEncoding& enc, std::size_t chunk)
{
    Encoder encoder(enc);
    std::string out;
    for (std::size_t pos = 0; pos < utf8.size(); pos += chunk) {
        const std::size_t len = std::min(chunk, utf8.size() - pos);
        encoder.encode(utf8.substr(pos, len), pos + len == utf8.size(), out);
    }
    if (utf8.empty())
        encoder.encode({}, true, out);
    return out;
}

const std::string kMultilingual = "Año ñandú, déjà vu — Съешь ещё 日本語 中文 한국어 😀𝄞\n";

} // namespace

class TestEncoding : public QObject
{
    Q_OBJECT

private slots:
    // --- Tabla de EncodingMapper ---------------------------------------------

    void charsetNames_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<int>("codepage");
        QTest::newRow("utf-8") << "UTF-8" << kCodepageUtf8;
        QTest::newRow("utf8") << "utf8" << kCodepageUtf8;
        QTest::newRow("windows-1252") << "windows-1252" << 1252;
        QTest::newRow("WINDOWS-1251 mayúsculas") << "WINDOWS-1251" << 1251;
        QTest::newRow("Shift_JIS (uchardet)") << "Shift_JIS" << 932;
        QTest::newRow("GB18030 (uchardet)") << "GB18030" << 936;
        QTest::newRow("Big5 (uchardet)") << "Big5" << 950;
        QTest::newRow("EUC-KR (uchardet)") << "EUC-KR" << 51949;
        QTest::newRow("ISO-8859-7 (uchardet)") << "ISO-8859-7" << 28597;
        QTest::newRow("latin1") << "latin1" << 28591;
        QTest::newRow("x-mac-cyrillic") << "x-mac-cyrillic" << 10007;
        QTest::newRow("TIS-620") << "TIS-620" << 874;
        // Rarezas de la tabla de N++ que se conservan a propósito
        QTest::newRow("KOI8-R no coincide con koi8_r") << "KOI8-R" << -1;
        QTest::newRow("EUC-JP no está") << "EUC-JP" << -1;
        QTest::newRow("vacío") << "" << -1;
        QTest::newRow("desconocido") << "klingon" << -1;
    }

    void charsetNames()
    {
        QFETCH(QString, name);
        QFETCH(int, codepage);
        QCOMPARE(codepageFromCharsetName(name.toStdString()), codepage);
    }

    void everyCodepageIsAvailableInIconv()
    {
        QCOMPARE(knownCodepages().size(), std::size_t{46});
        for (const int cp : knownCodepages()) {
            const char* name = iconvNameForCodepage(cp);
            if (cp == 720) { // OEM 720 (árabe DOS): glibc no lo trae
                QVERIFY(name == nullptr);
                QVERIFY(!Decoder(TextEncoding::fromCodepage(cp)).isValid());
                continue;
            }
            QVERIFY2(name != nullptr, qPrintable(QString::number(cp)));
            iconv_t cd = iconv_open("UTF-8", name);
            QVERIFY2(cd != reinterpret_cast<iconv_t>(-1), name);
            iconv_close(cd);
        }
    }

    void displayNames()
    {
        QCOMPARE(encodingDisplayName(TextEncoding::utf8()), "UTF-8"s);
        QCOMPARE(encodingDisplayName(TextEncoding::unicode(UniMode::Utf8Bom)), "UTF-8-BOM"s);
        QCOMPARE(encodingDisplayName(TextEncoding::unicode(UniMode::Utf16LeBom)), "UTF-16 LE BOM"s);
        QCOMPARE(encodingDisplayName(TextEncoding::fromCodepage(1252)), "Windows-1252"s);
        QCOMPARE(encodingDisplayName(TextEncoding::fromCodepage(932)), "Shift-JIS"s);
    }

    // --- Utf8_16_Read::determineEncoding ----------------------------------------

    void unicodeMode_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<edita::UniMode>("mode");
        QTest::newRow("vacío") << QByteArray() << UniMode::Ascii7;
        QTest::newRow("ascii") << QByteArray("hola\n") << UniMode::Ascii7;
        QTest::newRow("utf-8") << QByteArray("a\xC3\xB1o") << UniMode::Utf8;
        QTest::newRow("utf-8 4 bytes") << QByteArray("\xF0\x9F\x98\x80") << UniMode::Utf8;
        QTest::newRow("latin-1") << QByteArray("caf\xE9") << UniMode::Ansi;
        QTest::newRow("continuación suelta") << QByteArray("a\x80" "b") << UniMode::Ansi;
        QTest::newRow("NUL") << QByteArray("a\0b", 3) << UniMode::Ansi;
        QTest::newRow("utf-8 truncado") << QByteArray("a\xC3") << UniMode::Ansi;
        QTest::newRow("BOM utf-8") << QByteArray("\xEF\xBB\xBFx") << UniMode::Utf8Bom;
        QTest::newRow("BOM utf-16 le") << QByteArray("\xFF\xFEx\0", 4) << UniMode::Utf16LeBom;
        QTest::newRow("BOM utf-16 be") << QByteArray("\xFE\xFF\0x", 4) << UniMode::Utf16BeBom;
        QTest::newRow("utf-16 le sin BOM") << QByteArray("h\0o\0l\0a\0", 8) << UniMode::Utf16LeNoBom;
        QTest::newRow("impar no es utf-16") << QByteArray("h\0o\0l", 5) << UniMode::Ansi;
    }

    void unicodeMode()
    {
        QFETCH(QByteArray, bytes);
        QFETCH(edita::UniMode, mode);
        QCOMPARE(detectUnicodeMode(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size()))), mode);
    }

    void truncatedUtf8InPartialBlockIsStillUtf8()
    {
        // El primer bloque de un archivo más grande puede cortar un carácter
        QCOMPARE(detectUnicodeMode("a\xC3", /*isWholeFile*/ false), UniMode::Utf8);
    }

    void detectionRules()
    {
        const DetectionOptions defaults;
        // BOM manda aunque se fuerce un codepage
        QCOMPARE(detectEncoding("\xEF\xBB\xBFhola", true, 1252, defaults), TextEncoding::unicode(UniMode::Utf8Bom));
        // Forzado sin BOM
        QCOMPARE(detectEncoding("hola", true, 1251, defaults), TextEncoding::fromCodepage(1251));
        // Forzar UTF-8 equivale a UTF-8 sin BOM
        QCOMPARE(detectEncoding("hola", true, kCodepageUtf8, defaults), TextEncoding::utf8());
        // ASCII → UTF-8 sin BOM (UTF-8 como codepage del sistema)
        QCOMPARE(detectEncoding("hola", true, -1, defaults), TextEncoding::utf8());
        // 8 bits sin detección → codepage de respaldo
        DetectionOptions noDetect;
        noDetect.autoDetect = false;
        noDetect.fallbackCodepage = 1250;
        QCOMPARE(detectEncoding("caf\xE9", true, -1, noDetect), TextEncoding::fromCodepage(1250));
        QCOMPARE(detectEncoding("a\xC3\xB1o", true, -1, noDetect), TextEncoding::utf8());
    }

    // --- Conversión por bloques -----------------------------------------------

    void decodeIsIndependentOfChunkSize_data()
    {
        QTest::addColumn<int>("codepage");
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("text");
        const QString multi = QString::fromStdString(kMultilingual);
        QTest::newRow("utf-8") << -1 << int(UniMode::Utf8) << multi;
        QTest::newRow("utf-8 bom") << -1 << int(UniMode::Utf8Bom) << multi;
        QTest::newRow("utf-16 le bom") << -1 << int(UniMode::Utf16LeBom) << multi;
        QTest::newRow("utf-16 be bom") << -1 << int(UniMode::Utf16BeBom) << multi;
        QTest::newRow("utf-16 be sin bom") << -1 << int(UniMode::Utf16BeNoBom) << multi;
        QTest::newRow("shift-jis") << 932 << 0 << QStringLiteral("日本語のテキスト、カタカナ。ｶﾅ\n");
        QTest::newRow("gbk") << 936 << 0 << QStringLiteral("中文测试文本，简体字。\n");
        QTest::newRow("big5") << 950 << 0 << QStringLiteral("繁體中文測試。\n");
        QTest::newRow("euc-kr") << 51949 << 0 << QStringLiteral("한국어 시험 문장입니다.\n");
        QTest::newRow("windows-1252") << 1252 << 0 << QStringLiteral("Año €uro “comillas” — fin\n");
    }

    void decodeIsIndependentOfChunkSize()
    {
        QFETCH(int, codepage);
        QFETCH(int, mode);
        QFETCH(QString, text);
        const TextEncoding enc{codepage, static_cast<UniMode>(mode)};
        const std::string utf8 = text.toStdString();

        std::size_t unmappable = 99;
        const std::string bytes = encodeAll(utf8, enc, &unmappable);
        QCOMPARE(unmappable, std::size_t{0});

        for (std::size_t chunk : {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{5}, std::size_t{7}, std::size_t{4096}}) {
            QCOMPARE(decodeInChunks(bytes, enc, chunk), utf8);
            QCOMPARE(encodeInChunks(utf8, enc, chunk), bytes);
        }
    }

    void bomsAreWrittenAndSkipped()
    {
        QCOMPARE(encodeAll("a", TextEncoding::unicode(UniMode::Utf8Bom)), "\xEF\xBB\xBF" "a"s);
        QCOMPARE(encodeAll("a", TextEncoding::unicode(UniMode::Utf16LeBom)), "\xFF\xFE" "a\0"s);
        QCOMPARE(encodeAll("a", TextEncoding::unicode(UniMode::Utf16BeBom)), "\xFE\xFF\0" "a"s);
        QCOMPARE(encodeAll("a", TextEncoding::unicode(UniMode::Utf16LeNoBom)), "a\0"s);
        QCOMPARE(encodeAll("", TextEncoding::unicode(UniMode::Utf8Bom)), "\xEF\xBB\xBF"s);
        QCOMPARE(decodeAll("\xEF\xBB\xBF" "a", TextEncoding::unicode(UniMode::Utf8Bom)), "a"s);
        QCOMPARE(decodeAll("\xFF\xFE" "a\0"s, TextEncoding::unicode(UniMode::Utf16LeBom)), "a"s);
    }

    void utf8PassesThroughUnvalidated()
    {
        // Como N++: bytes inválidos en un documento UTF-8 se conservan tal cual
        const std::string raw = "ok \xE9\xFF ok";
        QCOMPARE(decodeAll(raw, TextEncoding::utf8()), raw);
        QCOMPARE(encodeAll(raw, TextEncoding::utf8()), raw);
    }

    void singleByteCodepagesRoundTripEveryByte()
    {
        // Incluye los bytes que cada codepage no define (0x81, 0x8D… en 1252;
        // 0xAA, 0xD2, 0xFF en 1253…), que glibc rechaza y Windows conserva.
        // Abrir y guardar no debe cambiar nada.
        std::string all;
        for (int b = 0; b < 256; ++b)
            all.push_back(static_cast<char>(b));
        for (const int cp : knownCodepages()) {
            if (cp == 720 || cp == 932 || cp == 936 || cp == 949 || cp == 950 || cp == 51949)
                continue; // no disponible / multibyte
            std::size_t invalid = 99;
            const std::string utf8 = decodeAll(all, TextEncoding::fromCodepage(cp), &invalid);
            std::size_t unmappable = 99;
            const std::string back = encodeAll(utf8, TextEncoding::fromCodepage(cp), &unmappable);
            QVERIFY2(back == all, qPrintable(QString::number(cp)));
            QCOMPARE(invalid, std::size_t{0});
            QCOMPARE(unmappable, std::size_t{0});
        }
        QCOMPARE(decodeAll("\x81", TextEncoding::fromCodepage(1252)), "\xC2\x81"s);
        QCOMPARE(decodeAll("\x80", TextEncoding::fromCodepage(1252)), "\xE2\x82\xAC"s); // €
        QCOMPARE(decodeAll("\xAA", TextEncoding::fromCodepage(1253)), "\xEF\x9E\xAA"s); // U+F7AA
    }

    void composingCodepagesKeepOrderAndLastCharacter()
    {
        // glibc compone en CP1255/CP1258 y retiene el último carácter: no debe
        // perderse al final ni desordenarse junto a un byte sin definir
        QCOMPARE(decodeAll("A", TextEncoding::fromCodepage(1258)), "A"s);
        QCOMPARE(decodeAll("\xFA\xFB", TextEncoding::fromCodepage(1255)), "\xD7\xAA\xEF\x9F\xBB"s); // ת + U+F7FB
        for (const std::size_t chunk : {std::size_t{1}, std::size_t{2}}) {
            const std::string bytes = "A\xEC" "b\x8D" "c"; // A + acento combinante, byte sin definir
            QCOMPARE(encodeInChunks(decodeInChunks(bytes, TextEncoding::fromCodepage(1258), chunk),
                                    TextEncoding::fromCodepage(1258), chunk), bytes);
        }
    }

    void unmappableCharactersBecomeQuestionMarks()
    {
        std::size_t unmappable = 0;
        QCOMPARE(encodeAll("a€中b", TextEncoding::fromCodepage(28591), &unmappable), "a??b"s);
        QCOMPARE(unmappable, std::size_t{2});
        // Un carácter partido al final del documento
        QCOMPARE(encodeAll("a\xC3", TextEncoding::fromCodepage(1252), &unmappable), "a?"s);
        QCOMPARE(unmappable, std::size_t{1});
    }

    void invalidBytesBecomeReplacementCharacter()
    {
        std::size_t invalid = 0;
        // 0x80 suelto no es válido en Shift-JIS como primer byte de un par
        const std::string out = decodeAll("a\x82", TextEncoding::fromCodepage(932), &invalid);
        QCOMPARE(out, "a\xEF\xBF\xBD"s); // carácter incompleto al final
        QCOMPARE(invalid, std::size_t{1});
    }

    // --- Fin de línea -------------------------------------------------------------

    void eolDetection_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<edita::EolType>("eol");
        QTest::newRow("crlf") << QByteArray("a\r\nb\n") << EolType::Windows;
        QTest::newRow("lf") << QByteArray("a\nb\r\n") << EolType::Unix;
        QTest::newRow("cr") << QByteArray("a\rb\n") << EolType::MacOs;
        QTest::newRow("cr al final") << QByteArray("a\r") << EolType::MacOs;
        QTest::newRow("sin fin de línea") << QByteArray("abc") << EolType::Unknown;
        QTest::newRow("vacío") << QByteArray() << EolType::Unknown;
    }

    void eolDetection()
    {
        QFETCH(QByteArray, text);
        QFETCH(edita::EolType, eol);
        QCOMPARE(detectEol(text.toStdString()), eol);
    }

    void eolDetectorHandlesCrLfSplitAcrossChunks()
    {
        EolDetector detector;
        detector.feed("linea\r");
        QVERIFY(!detector.isDecided());
        detector.feed("\nsiguiente");
        QCOMPARE(detector.finish(EolType::Unknown), EolType::Windows);
    }

    void eolConversion()
    {
        const std::string mixed = "a\r\nb\nc\rd";
        QCOMPARE(convertEol(mixed, EolType::Unix), "a\nb\nc\nd"s);
        QCOMPARE(convertEol(mixed, EolType::Windows), "a\r\nb\r\nc\r\nd"s);
        QCOMPARE(convertEol(mixed, EolType::MacOs), "a\rb\rc\rd"s);
        QCOMPARE(convertEol("", EolType::Windows), ""s);
    }
};

QTEST_GUILESS_MAIN(TestEncoding)
#include "tst_encoding.moc"
