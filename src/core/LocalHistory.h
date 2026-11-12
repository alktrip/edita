// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Historial local (fase 83; no existe en N++): cada vez que se guarda un archivo se queda una
// copia fechada, para compararla con el documento o volver a ella. N++ solo tiene la copia
// .bak junto al archivo (Preferencias › Copia de seguridad), sin límites ni forma de verla.
//
// Las copias van en <configuración>/localHistory/<hash de la ruta>/, con un archivo «path»
// que dice de qué archivo son y una copia por versión con los bytes tal cual se guardaron.
// No se usa la carpeta de la nube (cloud/choice): el historial es de este equipo

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

namespace edita {

struct LocalHistoryLimits
{
    int maxVersions = 50;                  // por archivo
    int maxDays = 30;                      // las más viejas se borran (la última se queda siempre)
    qint64 maxFileBytes = 2 * 1024 * 1024; // los archivos más grandes no se guardan
};

class LocalHistory
{
public:
    struct Version
    {
        QString id; // el nombre de la copia
        QDateTime when;
        qint64 size = 0;
    };

    explicit LocalHistory(QString root);
    // <configuración base>/localHistory
    static QString defaultRoot();
    const QString& root() const { return _root; }

    // Guarda `bytes` como la versión de `path` en `when`. No guarda nada (y devuelve false) si
    // es igual a la última versión o si pasa del límite de tamaño. Después borra lo que sobra
    bool record(const QString& path, const QByteArray& bytes, const QDateTime& when, const LocalHistoryLimits& limits);
    // Lo mismo con lo que hay en el disco
    bool recordFile(const QString& path, const QDateTime& when, const LocalHistoryLimits& limits);

    bool hasHistory(const QString& path) const;
    // De la más nueva a la más vieja
    QList<Version> versions(const QString& path) const;
    QByteArray read(const QString& path, const Version& version) const;

    // El archivo cambió de nombre o de carpeta: su historial lo sigue (si el destino ya tenía
    // uno, se juntan)
    void rename(const QString& from, const QString& to);
    void removeVersion(const QString& path, const Version& version);
    void clear(const QString& path);
    // Borra las versiones de más y las viejas
    void prune(const QString& path, const LocalHistoryLimits& limits, const QDateTime& now);

    // Cuánto ocupa todo el historial, en bytes
    qint64 totalSize() const;

private:
    QString dirOf(const QString& path) const;

    QString _root;
};

} // namespace edita
