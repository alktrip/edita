// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Avisar de una versión nueva (lo que en N++ hace el actualizador, gup.exe). Edita no se
// instala solo: pregunta a GitHub cuál es la última versión publicada y, si es más nueva,
// lo dice y lleva a su página. Como N++, comprueba al arrancar cada `intervalDays` días, y
// lo guarda en config.xml (<GUIConfig name="noUpdate" autoUpdateMode intervalDays
// nextUpdateDate>), con la versión que el usuario decidió omitir.

#pragma once

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QUrl>

namespace edita {

class ConfigXml;

namespace update_check {

// Versiones semánticas con etapa: 0.9.0-beta.2 < 0.9.0-beta.10 < 0.9.0 < 0.9.1. Una «v»
// al principio (la de las etiquetas) se ignora. <0, 0 o >0; las que no se entienden son
// las más antiguas
int compareVersions(const QString& a, const QString& b);
bool isValidVersion(const QString& version);

// Una versión publicada (un elemento de GET /repos/<dueño>/<repo>/releases)
struct Release
{
    QString version;  // sin la «v»
    QUrl page;        // html_url: la página de la versión, con sus notas
    QUrl appImage;    // el AppImage de x86_64, si lo tiene
};
bool parseLatestRelease(const QByteArray& json, Release& release, QString* error = nullptr);
// Fase 79: la más nueva de la lista de versiones (o de un solo objeto, como el de
// releases/latest). Sin `includePrereleases` (una versión estable de Edita) no cuentan las
// betas: ni las que tienen etapa en el número (1.1.0-beta.1) ni las marcadas «prerelease».
// Los borradores y las etiquetas que no son versiones («continuous») nunca. Si no queda
// ninguna, `release.version` queda vacía: no hay nada que avisar
bool parseReleases(const QByteArray& json, bool includePrereleases, Release& release, QString* error = nullptr);

// El estado (lo que el usuario elige, comprobar al arrancar y cada cuántos días, está en
// Preferences: autoUpdateMode e intervalDays del mismo <GUIConfig>)
struct State
{
    QDate nextCheck;        // nextUpdateDate; vacía: ya toca
    QString skippedVersion; // «Omitir esta versión»
};
State readState(const ConfigXml& config);
void writeState(ConfigXml& config, const State& state);
// ¿Toca comprobar al arrancar?
bool isDue(bool onStartup, const State& state, const QDate& today);
// Tras comprobar: la próxima fecha (launchUpdater: hoy + intervalDays)
void scheduleNext(State& state, int intervalDays, const QDate& today);

// De dónde: la lista de versiones de la API de GitHub (las 30 más recientes), o EDITA_UPDATE_URL (una ruta local sirve para las pruebas;
// "none" lo desactiva). Vacía = desactivado
QUrl releaseUrl(const QString& homepage);

} // namespace update_check
} // namespace edita
