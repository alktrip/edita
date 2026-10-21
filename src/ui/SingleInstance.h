// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
//
// Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team).
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <QObject>
#include <QStringList>

class QLocalServer;

namespace edita {

// Lo que una instancia le pide a la que ya está abierta (el WM_COPYDATA de N++:
// COPYDATA_PARAMS + COPYDATA_FILENAMESW): abrir archivos, con rutas completas, e
// ir a una línea y columna
struct InstanceRequest
{
    QStringList files;
    int line = -1;   // -n
    int column = -1; // -c

    QByteArray toJson() const;
    static bool fromJson(const QByteArray& json, InstanceRequest& request);
};

// Una instancia por usuario y carpeta de configuración (winmain.cpp de N++: la
// segunda busca la ventana de la primera, le pasa los archivos y termina). En
// Linux, con un socket local en $XDG_RUNTIME_DIR.
class InstanceServer : public QObject
{
    Q_OBJECT

public:
    explicit InstanceServer(QObject* parent = nullptr);
    ~InstanceServer() override;

    // El socket de esta configuración (~/.config/<id>): dos configuraciones
    // distintas (p. ej. XDG_CONFIG_HOME en pruebas) no se mezclan
    static QString socketName(const QString& configDir);

    // Empieza a escuchar; si quedó un socket de una instancia que ya no existe, lo
    // quita. Falso si hay otra instancia escuchando (en Linux, la ruta del socket
    // no puede pasar de unos 100 caracteres).
    bool listen(const QString& name);
    bool isListening() const;

    // Pasa la petición a la instancia que escucha en `name`; falso si no hay
    // ninguna (o no responde en `timeoutMs`)
    static bool send(const QString& name, const InstanceRequest& request, int timeoutMs = 2000);
    // ¿Hay una instancia escuchando?
    static bool isRunning(const QString& name, int timeoutMs = 500);

signals:
    void requestReceived(const InstanceRequest& request);

private:
    void accept();

    QLocalServer* _server = nullptr;
};

} // namespace edita

Q_DECLARE_METATYPE(edita::InstanceRequest)
