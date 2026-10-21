// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 1, criterio de rendimiento: un archivo de 100 MB carga en menos de 2 s
// en un build optimizado. Etiqueta CTest "perf" (ctest -LE perf para saltarlo).

#include "doc/DocumentManager.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;

namespace {

constexpr qint64 kTargetSize = 100LL * 1024 * 1024;

// Escribe ~100 MB repitiendo una línea con algo de texto no ASCII
bool writeBigFile(const QString& path, const QByteArray& line)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    QByteArray block;
    while (block.size() < (1 << 20))
        block += line;
    for (qint64 written = 0; written < kTargetSize; written += block.size()) {
        if (f.write(block) != block.size())
            return false;
    }
    return true;
}

} // namespace

class TestPerf : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;

private slots:
    void load100MbUtf8()
    {
        const QString path = _tmp.filePath("grande-utf8.txt");
        QVERIFY(writeBigFile(path, "Línea de prueba con acentos: áéíóú ñ, y números 0123456789.\n"));

        DocumentManager manager;
        QElapsedTimer timer;
        timer.start();
        const BufferId id = manager.loadFile(path);
        const qint64 loadMs = timer.elapsed();
        QVERIFY(id != kInvalidBuffer);
        QVERIFY(manager.buffer(id)->encoding() == TextEncoding::utf8());
        qInfo("Carga UTF-8 de %lld MB: %lld ms", QFileInfo(path).size() >> 20, loadMs);

        timer.restart();
        QVERIFY(manager.saveBuffer(id, _tmp.filePath("grande-utf8-copia.txt"), true).ok());
        qInfo("Guardado UTF-8: %lld ms", timer.elapsed());

#ifdef NDEBUG
        QVERIFY2(loadMs < 2000, qPrintable(QStringLiteral("%1 ms").arg(loadMs)));
#else
        QSKIP("El límite de 2 s solo se exige en builds optimizados");
#endif
    }

    void load100MbWindows1252()
    {
        // Informativo: conversión por tabla de un codepage de un byte
        const QString path = _tmp.filePath("grande-1252.txt");
        QVERIFY(writeBigFile(path, "L\xEDnea de prueba con acentos: \xE1\xE9\xED\xF3\xFA \xF1, y n\xFAmeros 0123456789.\n"));

        DocumentManager manager;
        QElapsedTimer timer;
        timer.start();
        const BufferId id = manager.loadFile(path, 1252);
        const qint64 loadMs = timer.elapsed();
        QVERIFY(id != kInvalidBuffer);
        qInfo("Carga Windows-1252 de 100 MB: %lld ms", loadMs);

        timer.restart();
        QVERIFY(manager.saveBuffer(id, _tmp.filePath("grande-1252-copia.txt"), true).ok());
        qInfo("Guardado Windows-1252: %lld ms", timer.elapsed());
    }
};

QTEST_MAIN(TestPerf)
#include "tst_perf.moc"
