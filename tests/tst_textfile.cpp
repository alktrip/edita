// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 1: carga y guardado de archivos reales (tests/fixtures). Criterios:
// la detección coincide con la tabla esperada y cargar + guardar sin editar
// produce un archivo idéntico byte a byte.

#include "core/TextFile.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace edita;
using namespace std::string_literals;

Q_DECLARE_METATYPE(edita::EolType)

namespace {

QString fixture(const QString& relative)
{
    return QStringLiteral(EDITA_FIXTURES_DIR "/") + relative;
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

std::string reference(const QString& name)
{
    return readAll(fixture(QStringLiteral("encoding/reference/%1.txt").arg(name))).toStdString();
}

LoadOptions withBlockSize(std::size_t blockSize)
{
    LoadOptions o;
    o.blockSize = blockSize;
    return o;
}

} // namespace

class TestTextFile : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        QVERIFY(QFileInfo::exists(fixture("encoding/utf8.txt")));
    }

    // Tabla de detección. Los resultados son los de Notepad++ porque se usan su
    // uchardet, su tabla de alias y sus reglas; las columnas "nota" explican
    // los casos en que N++ no acierta y se conserva su comportamiento.
    void detectAndRoundTrip_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<int>("codepage");
        QTest::addColumn<int>("mode");      // UniMode si codepage == -1
        QTest::addColumn<QString>("ref");   // texto de referencia; vacío = no comparar

        const int none = int(UniMode::Utf8);
        QTest::newRow("UTF-8") << "utf8.txt" << -1 << none << "multi";
        QTest::newRow("UTF-8 BOM") << "utf8-bom.txt" << -1 << int(UniMode::Utf8Bom) << "multi";
        QTest::newRow("UTF-16 LE BOM") << "utf16le-bom.txt" << -1 << int(UniMode::Utf16LeBom) << "multi";
        QTest::newRow("UTF-16 BE BOM") << "utf16be-bom.txt" << -1 << int(UniMode::Utf16BeBom) << "multi";
        QTest::newRow("UTF-16 LE sin BOM") << "utf16le-nobom.txt" << -1 << int(UniMode::Utf16LeNoBom) << "ascii";
        QTest::newRow("ASCII") << "ascii.txt" << -1 << none << "ascii";
        QTest::newRow("vacío") << "empty.txt" << -1 << none << "";
        QTest::newRow("Windows-1252") << "windows-1252.txt" << 1252 << 0 << "spanish-euro";
        // uchardet (el de N++) no distingue Latin-9 de 1252: € (0xA4) se lee como ¤
        QTest::newRow("ISO-8859-15 → 1252") << "iso-8859-15.txt" << 1252 << 0 << "";
        QTest::newRow("Windows-1251") << "windows-1251.txt" << 1251 << 0 << "russian";
        // Texto corto con muchas letras raras: el uchardet de N++ lo toma por
        // x-mac-cyrillic. Se conserva el comportamiento (y el archivo no se altera).
        QTest::newRow("pangrama 1251 → Mac cirílico") << "russian-pangram-1251.txt" << 10007 << 0 << "";
        // uchardet devuelve "KOI8-R", que no está en la tabla de N++ ("koi8_r"):
        // acaba en el codepage de respaldo, igual que en N++
        QTest::newRow("KOI8-R → respaldo") << "koi8-r.txt" << 1252 << 0 << "";
        QTest::newRow("Shift-JIS") << "shift_jis.txt" << 932 << 0 << "japanese";
        QTest::newRow("GB2312") << "gb2312.txt" << 936 << 0 << "chinese-s";
        QTest::newRow("Big5") << "big5.txt" << 950 << 0 << "chinese-t";
        QTest::newRow("EUC-KR") << "euc-kr.txt" << 51949 << 0 << "korean";
        QTest::newRow("ISO-8859-7") << "iso-8859-7.txt" << 28597 << 0 << "greek";
        QTest::newRow("UTF-8 inválido") << "invalid-utf8.txt" << 1252 << 0 << "";
        QTest::newRow("todos los bytes altos") << "all-high-bytes.bin" << 1252 << 0 << "";
        QTest::newRow("con NUL") << "with-nul.bin" << 1252 << 0 << "";
    }

    void detectAndRoundTrip()
    {
        QFETCH(QString, file);
        QFETCH(int, codepage);
        QFETCH(int, mode);
        QFETCH(QString, ref);

        const QString path = fixture("encoding/" + file);
        std::string text;
        const LoadResult loaded = loadTextFile(path, LoadOptions{}, text);
        QVERIFY2(loaded.ok(), qPrintable(loaded.errorString));

        const TextEncoding expected{codepage, static_cast<UniMode>(mode)};
        QCOMPARE(QString::fromStdString(encodingDisplayName(loaded.encoding)),
                 QString::fromStdString(encodingDisplayName(expected)));
        QVERIFY(loaded.encoding == expected);
        QCOMPARE(loaded.invalidSequences, std::size_t{0});
        if (!ref.isEmpty())
            QCOMPARE(text, reference(ref));

        // Cargar con bloques diminutos da el mismo texto (caracteres partidos entre
        // bloques). La detección mira solo el primer bloque, así que se fuerza el
        // codepage; en Unicode (-1) deciden el BOM o el contenido.
        LoadOptions small = withBlockSize(7);
        small.forcedCodepage = loaded.encoding.codepage;
        std::string textSmallBlocks;
        QVERIFY(loadTextFile(path, small, textSmallBlocks).ok());
        QCOMPARE(textSmallBlocks, text);

        // Guardar sin cambios → idéntico byte a byte
        const QString out = _tmp.filePath("rt-" + file);
        const SaveResult saved = saveTextFile(out, text, loaded.encoding);
        QVERIFY2(saved.ok(), qPrintable(saved.errorString));
        QCOMPARE(saved.unmappableChars, std::size_t{0});
        QCOMPARE(readAll(out), readAll(path));
    }

    void forcedCodepageReinterprets()
    {
        // "Codificación > Juegos de caracteres > ISO 8859-15" sobre el archivo Latin-9
        LoadOptions options;
        options.forcedCodepage = 28605;
        std::string text;
        const LoadResult loaded = loadTextFile(fixture("encoding/iso-8859-15.txt"), options, text);
        QVERIFY(loaded.ok());
        QVERIFY(loaded.encoding == TextEncoding::fromCodepage(28605));
        QCOMPARE(text, reference("spanish-euro"));

        options.forcedCodepage = 20866; // KOI8-R
        QVERIFY(loadTextFile(fixture("encoding/koi8-r.txt"), options, text).ok());
        QCOMPARE(text, reference("russian"));
    }

    void bomWinsOverForcedCodepage()
    {
        LoadOptions options;
        options.forcedCodepage = 1252;
        std::string text;
        const LoadResult loaded = loadTextFile(fixture("encoding/utf8-bom.txt"), options, text);
        QVERIFY(loaded.encoding == TextEncoding::unicode(UniMode::Utf8Bom));
        QCOMPARE(text, reference("multi"));
    }

    void fallbackCodepageIsConfigurable()
    {
        LoadOptions options;
        options.detection.autoDetect = false;
        options.detection.fallbackCodepage = 1251;
        std::string text;
        const LoadResult loaded = loadTextFile(fixture("encoding/windows-1251.txt"), options, text);
        QVERIFY(loaded.encoding == TextEncoding::fromCodepage(1251));
        QCOMPARE(text, reference("russian"));
    }

    void eolDetection_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<edita::EolType>("eol");
        QTest::addColumn<bool>("detected");
        QTest::newRow("CRLF") << "crlf.txt" << EolType::Windows << true;
        QTest::newRow("LF") << "lf.txt" << EolType::Unix << true;
        QTest::newRow("CR") << "cr.txt" << EolType::MacOs << true;
        QTest::newRow("mixto, LF primero") << "mixed-lf-first.txt" << EolType::Unix << true;
        QTest::newRow("mixto, CRLF primero") << "mixed-crlf-first.txt" << EolType::Windows << true;
        QTest::newRow("CR al final") << "cr-at-end.txt" << EolType::MacOs << true;
        QTest::newRow("sin fin de línea → por defecto") << "none.txt" << EolType::Unix << false;
    }

    void eolDetection()
    {
        QFETCH(QString, file);
        QFETCH(edita::EolType, eol);
        QFETCH(bool, detected);
        const QString path = fixture("eol/" + file);
        for (std::size_t block : {kLoadBlockSize, std::size_t{1}, std::size_t{4}}) {
            std::string text;
            const LoadResult loaded = loadTextFile(path, withBlockSize(block), text);
            QVERIFY(loaded.ok());
            QCOMPARE(loaded.eol, eol);
            QCOMPARE(loaded.eolDetected, detected);
        }
        // El texto conserva los finales de línea originales (mixtos incluidos)
        std::string text;
        loadTextFile(path, LoadOptions{}, text);
        QCOMPARE(QByteArray::fromStdString(text), readAll(path));
    }

    void defaultEolForFilesWithoutLineBreaks()
    {
        LoadOptions options;
        options.defaultEol = EolType::Windows;
        std::string text;
        QCOMPARE(loadTextFile(fixture("eol/none.txt"), options, text).eol, EolType::Windows);
    }

    void convertEncodingAndEolOnSave()
    {
        std::string text;
        loadTextFile(fixture("encoding/windows-1252.txt"), LoadOptions{}, text);

        // Convertir a UTF-8 con BOM y CRLF
        const QString out = _tmp.filePath("convertido.txt");
        QVERIFY(saveTextFile(out, convertEol(text, EolType::Windows), TextEncoding::unicode(UniMode::Utf8Bom)).ok());
        const QByteArray expected = "\xEF\xBB\xBF" + QByteArray::fromStdString(convertEol(reference("spanish-euro"), EolType::Windows));
        QCOMPARE(readAll(out), expected);

        std::string back;
        const LoadResult loaded = loadTextFile(out, LoadOptions{}, back);
        QVERIFY(loaded.encoding == TextEncoding::unicode(UniMode::Utf8Bom));
        QCOMPARE(loaded.eol, EolType::Windows);
    }

    void unmappableCharsAreReported()
    {
        const QString out = _tmp.filePath("latin1.txt");
        const SaveResult saved = saveTextFile(out, "Precio: 5 € — 中", TextEncoding::fromCodepage(28591));
        QVERIFY(saved.ok());
        QCOMPARE(saved.unmappableChars, std::size_t{3});
        QCOMPARE(readAll(out), QByteArray("Precio: 5 ? ? ?"));
    }

    // --- Errores: nunca excepción ni cierre -------------------------------------

    void missingFile()
    {
        std::string text;
        const LoadResult r = loadTextFile(_tmp.filePath("no-existe.txt"), LoadOptions{}, text);
        QCOMPARE(r.error, FileError::NotFound);
        QVERIFY(!r.errorString.isEmpty());
    }

    void directoryIsNotAFile()
    {
        std::string text;
        QCOMPARE(loadTextFile(_tmp.path(), LoadOptions{}, text).error, FileError::IsDirectory);
        QCOMPARE(saveTextFile(_tmp.path(), "x", TextEncoding::utf8()).error, FileError::IsDirectory);
    }

    void unreadableFile()
    {
        if (geteuid() == 0)
            QSKIP("root puede leer cualquier archivo");
        const QString path = _tmp.filePath("sin-permiso.txt");
        QVERIFY(saveTextFile(path, "secreto", TextEncoding::utf8()).ok());
        QVERIFY(QFile::setPermissions(path, QFileDevice::Permissions{}));
        std::string text;
        QCOMPARE(loadTextFile(path, LoadOptions{}, text).error, FileError::PermissionDenied);
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }

    void saveIntoMissingDirectoryFails()
    {
        const SaveResult r = saveTextFile(_tmp.filePath("no/existe/x.txt"), "x", TextEncoding::utf8());
        QVERIFY(!r.ok());
        QVERIFY(!r.errorString.isEmpty());
    }

    void unsupportedCodepage()
    {
        LoadOptions options;
        options.forcedCodepage = 720; // OEM 720: no existe en glibc
        std::string text;
        QCOMPARE(loadTextFile(fixture("encoding/ascii.txt"), options, text).error, FileError::UnsupportedEncoding);
        QCOMPARE(saveTextFile(_tmp.filePath("cp720.txt"), "x", TextEncoding::fromCodepage(720)).error,
                 FileError::UnsupportedEncoding);
    }

    // --- Propiedades del guardado ------------------------------------------------

    void savePreservesPermissions()
    {
        const QString path = _tmp.filePath("permisos.sh");
        QVERIFY(saveTextFile(path, "#!/bin/sh\n", TextEncoding::utf8()).ok());
        const auto perms = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
            | QFileDevice::ReadGroup;
        QVERIFY(QFile::setPermissions(path, perms));
        QVERIFY(saveTextFile(path, "#!/bin/sh\necho hola\n", TextEncoding::utf8()).ok());
        QCOMPARE(QFile::permissions(path) & ~(QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser), perms);
    }

    void saveWritesThroughSymlink()
    {
        const QString target = _tmp.filePath("real.txt");
        const QString link = _tmp.filePath("enlace.txt");
        QVERIFY(saveTextFile(target, "antes", TextEncoding::utf8()).ok());
        QVERIFY(QFile::link(target, link));
        QVERIFY(saveTextFile(link, "después", TextEncoding::utf8()).ok());
        QVERIFY(QFileInfo(link).isSymLink());
        QCOMPARE(readAll(target), QByteArray("después"));
    }

    void saveEmptyDocument()
    {
        const QString path = _tmp.filePath("vacio.txt");
        QVERIFY(saveTextFile(path, "", TextEncoding::utf8()).ok());
        QCOMPARE(QFileInfo(path).size(), qint64{0});
        QVERIFY(saveTextFile(path, "", TextEncoding::unicode(UniMode::Utf16LeBom)).ok());
        QCOMPARE(readAll(path), QByteArray("\xFF\xFE"));
    }

    void sinkCanAbort()
    {
        const LoadResult r = loadTextFile(fixture("encoding/utf8.txt"), withBlockSize(16),
            [](std::string_view) { return false; });
        QCOMPARE(r.error, FileError::Aborted);
    }
};

QTEST_GUILESS_MAIN(TestTextFile)
#include "tst_textfile.moc"
