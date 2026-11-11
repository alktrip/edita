// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 1, criterio de rendimiento: un archivo de 100 MB carga en menos de 2 s
// en un build optimizado. Fase 77: un archivo de código con muchas funciones abre en
// segundos (antes, minutos), volver a su pestaña es inmediato y escribir en él no se
// traba.
// Etiqueta CTest "perf" (ctest -LE perf para saltarlo).

#include "FakePrompter.h"
#include "doc/DocumentManager.h"
#include "ui/DocTabBar.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/Panels.h"

#include <QDockWidget>
#include <QTreeWidget>

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

    // 200 000 funciones de C++, cada una con su comentario (14 MB). La ruta de navegación
    // analiza las funciones al abrirlo. Tardaba 156 s: la Lista de funciones de N++ alterna
    // dos expresiones regulares y Scintilla recompilaba la que no era la última en cada
    // coincidencia, y cada función se buscaba en todas las zonas de comentario
    void sourceFileWithManyFunctions()
    {
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        const QString cpp = _tmp.filePath("funciones.cpp");
        const QString other = _tmp.filePath("otro.txt");
        {
            QFile f(cpp);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QByteArray block;
            for (int i = 0; i < 1000; ++i)
                block += "int funcion_larga(int a, int b) { return a * b + 42; } // comentario áé\n";
            for (int i = 0; i < 200; ++i)
                f.write(block);
            QFile o(other);
            QVERIFY(o.open(QIODevice::WriteOnly));
            o.write("hola\n");
        }
        MainWindow w;
        w.setPrompter(std::make_unique<edita::test::FakePrompter>());
        Preferences p = w.preferences();
        p.breadcrumbs = true;
        w.setPreferences(p);
        w.resize(1000, 700);
        w.show();
        QElapsedTimer timer;
        timer.start();
        QVERIFY(w.openFile(cpp) != kInvalidBuffer);
        QCoreApplication::processEvents();
        const qint64 openMs = timer.elapsed();
        QVERIFY(w.openFile(other) != kInvalidBuffer);
        QCoreApplication::processEvents();
        timer.restart();
        w.pane()->tabBar()->setCurrentIndex(0);
        QCoreApplication::processEvents();
        const qint64 backMs = timer.elapsed();
        // Con la Lista de funciones abierta: usa las mismas funciones ya analizadas
        w.panelDock(QStringLiteral("functionList"))->show();
        QCoreApplication::processEvents();
        QVERIFY(w.functionListPanel()->tree()->topLevelItemCount() == 1);
        QCOMPARE(w.functionListPanel()->tree()->topLevelItem(0)->childCount(), 200000);
        w.pane()->tabBar()->setCurrentIndex(1);
        QCoreApplication::processEvents();
        timer.restart();
        w.pane()->tabBar()->setCurrentIndex(0);
        QCoreApplication::processEvents();
        const qint64 panelMs = timer.elapsed();
        // Escribir a la mitad: el autocompletado busca las palabras en todo el documento
        // (con la expresión regular de N++, 200 ms por tecla)
        w.editor()->sci(SCI_GOTOLINE, 100000);
        timer.restart();
        // (la primera tecla, además, marca el documento con cambios: la Lista de funciones
        // no tiene por qué volver a analizarlo)
        for (int i = 0; i < 50; ++i) {
            QTest::keyClick(w.editor(), Qt::Key_X);
            QCoreApplication::processEvents();
        }
        const qint64 typingMs = timer.elapsed();
        qInfo("Abrir 200 000 funciones: %lld ms; volver a su pestaña: %lld ms; con la Lista de funciones: %lld ms; "
              "50 teclas: %lld ms",
              openMs, backMs, panelMs, typingMs);
#ifdef NDEBUG
        QVERIFY2(openMs < 10000, qPrintable(QStringLiteral("%1 ms").arg(openMs)));
        QVERIFY2(backMs < 1000, qPrintable(QStringLiteral("%1 ms").arg(backMs)));
        QVERIFY2(panelMs < 3000, qPrintable(QStringLiteral("%1 ms").arg(panelMs)));
        QVERIFY2(typingMs < 3000, qPrintable(QStringLiteral("%1 ms").arg(typingMs)));
#endif
    }
};

QTEST_MAIN(TestPerf)
#include "tst_perf.moc"
