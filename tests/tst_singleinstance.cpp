// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 18: una sola instancia (winmain.cpp de N++). La segunda le pasa los archivos
// a la primera y termina; con -multiInst o "siempre una ventana nueva", no.

#include "FakePrompter.h"
#include "core/ConfigXml.h"
#include "core/Preferences.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/SingleInstance.h"

#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <future>

using namespace edita;
using edita::test::FakePrompter;

class TestSingleInstance : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;

    QString file(const QString& name, const QByteArray& text = "texto\n")
    {
        const QString path = _tmp.filePath(name);
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(text);
        return path;
    }

    // send() espera la respuesta: desde otro hilo, para que este atienda al servidor
    static bool sendAsync(const QString& name, const InstanceRequest& request)
    {
        auto result = std::async(std::launch::async, [=] { return InstanceServer::send(name, request); });
        while (result.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready)
            QCoreApplication::processEvents();
        return result.get();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("XDG_RUNTIME_DIR", _tmp.path().toUtf8());
    }

    void requestTravelsAsJson()
    {
        InstanceRequest request;
        request.files = {QStringLiteral("/a/b.txt"), QStringLiteral("/c/ñ d.md")};
        request.line = 12;
        request.column = 3;
        InstanceRequest back;
        QVERIFY(InstanceRequest::fromJson(request.toJson(), back));
        QCOMPARE(back.files, request.files);
        QCOMPARE(back.line, 12);
        QCOMPARE(back.column, 3);
        QVERIFY(request.toJson().endsWith('\n')); // una línea por petición
        QVERIFY(!InstanceRequest::fromJson("no es json", back));

        // Un socket por configuración, en $XDG_RUNTIME_DIR
        const QString a = InstanceServer::socketName(QStringLiteral("/home/x/.config/edita"));
        QCOMPARE(a, InstanceServer::socketName(QStringLiteral("/home/x/.config/edita")));
        QVERIFY(a != InstanceServer::socketName(QStringLiteral("/tmp/otra/edita")));
        QVERIFY(a.startsWith(_tmp.path()));
    }

    void serverReceivesRequests()
    {
        const QString name = _tmp.filePath("prueba.sock");
        QVERIFY(!InstanceServer::isRunning(name));
        QVERIFY(!InstanceServer::send(name, InstanceRequest{}, 300)); // nadie escucha
        {
            InstanceServer server;
            QVERIFY(server.listen(name));
            QVERIFY(server.isListening());
            QVERIFY(InstanceServer::isRunning(name));
            // Una segunda no puede escuchar en el mismo socket
            InstanceServer second;
            QVERIFY(!second.listen(name));

            QSignalSpy received(&server, &InstanceServer::requestReceived);
            InstanceRequest request;
            request.files = {QStringLiteral("/tmp/uno.txt")};
            request.line = 4;
            QVERIFY(sendAsync(name, request));
            QTRY_COMPARE(received.size(), 1);
            const auto got = received.first().first().value<InstanceRequest>();
            QCOMPARE(got.files, request.files);
            QCOMPARE(got.line, 4);
        }
        QVERIFY(!InstanceServer::isRunning(name)); // al cerrarse, deja de escuchar
    }

    void staleSocketIsReplaced()
    {
        // Una instancia que terminó mal deja el archivo del socket
        const QString name = _tmp.filePath("viejo.sock");
        QProcess python;
        python.start(QStringLiteral("python3"),
                     {QStringLiteral("-c"), QStringLiteral("import socket,sys; s=socket.socket(socket.AF_UNIX); s.bind(sys.argv[1])"), name});
        if (!python.waitForFinished(10000) || python.exitCode() != 0)
            QSKIP("no se pudo crear un socket abandonado con python3");
        QVERIFY(QFileInfo::exists(name));
        QVERIFY(!InstanceServer::isRunning(name));
        InstanceServer server;
        QVERIFY(server.listen(name));
    }

    void windowOpensWhatTheOtherInstanceSends()
    {
        MainWindow w;
        auto prompter = std::make_unique<FakePrompter>();
        w.setPrompter(std::move(prompter));
        InstanceServer server;
        const QString name = _tmp.filePath("ventana.sock");
        QVERIFY(server.listen(name));
        connect(&server, &InstanceServer::requestReceived, &w,
                [&w](const InstanceRequest& r) { w.openFromOtherInstance(r, true); });
        InstanceRequest request;
        request.files = {file("otra.txt", "uno\ndos\ntres\ncuatro\n")};
        request.line = 3;
        QVERIFY(sendAsync(name, request));
        QTRY_VERIFY(w.documents().buffer(w.currentBuffer())->filePath() == request.files.first());
        QCOMPARE(w.editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(w.editor()->sci(SCI_GETCURRENTPOS))), sptr_t{2});
    }

    void preferencePersists()
    {
        ConfigXml config;
        config.load(_tmp.filePath("prefs.xml"));
        QCOMPARE(loadPreferences(config).multiInstSetting, MultiInst::Mono); // por defecto, como N++
        Preferences p;
        p.multiInstSetting = MultiInst::Always;
        storePreferences(config, p);
        QCOMPARE(config.guiConfig(QStringLiteral("multiInst"), QStringLiteral("setting")), QStringLiteral("2"));
        QCOMPARE(loadPreferences(config).multiInstSetting, MultiInst::Always);
    }

    void secondProcessHandsOverAndExits()
    {
        // Dos Edita de verdad, sin pantalla y con su propia configuración
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), _tmp.filePath("procesos"));
        env.insert(QStringLiteral("XDG_RUNTIME_DIR"), _tmp.path());
        const QString socket = InstanceServer::socketName(_tmp.filePath("procesos/edita"));

        QProcess first;
        first.setProcessEnvironment(env);
        first.start(QStringLiteral(EDITA_BINARY), {QStringLiteral("--nosession"), file("a.txt")});
        QVERIFY(first.waitForStarted());
        QTRY_VERIFY2_WITH_TIMEOUT(InstanceServer::isRunning(socket),
                                  qPrintable(QStringLiteral("socket %1; estado %2; salida: %3")
                                                 .arg(socket).arg(first.state())
                                                 .arg(QString::fromLocal8Bit(first.readAllStandardError() + first.readAllStandardOutput()))),
                                  15000);

        // La segunda le pasa b.txt a la primera y termina enseguida, sin error
        QProcess second;
        second.setProcessEnvironment(env);
        second.start(QStringLiteral(EDITA_BINARY), {file("b.txt"), QStringLiteral("-n"), QStringLiteral("1")});
        QVERIFY(second.waitForFinished(15000));
        QCOMPARE(second.exitStatus(), QProcess::NormalExit);
        QCOMPARE(second.exitCode(), 0);

        // Con -multiInst se queda abierta (y no le quita el socket a la primera)
        QProcess third;
        third.setProcessEnvironment(env);
        third.start(QStringLiteral(EDITA_BINARY), {QStringLiteral("--multiInst"), file("c.txt")});
        QVERIFY(third.waitForStarted());
        QVERIFY(!third.waitForFinished(2000));
        QVERIFY(InstanceServer::isRunning(socket));
        third.kill();
        third.waitForFinished();
        first.kill();
        first.waitForFinished();
    }
};

QTEST_MAIN(TestSingleInstance)
#include "tst_singleinstance.moc"
