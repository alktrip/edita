// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Edita dentro de Flatpak (fase 105). La sandbox no ve los programas del sistema: git, los
// servidores de lenguaje, los formateadores, make, npm o la shell del usuario. El «puente» crea
// al arrancar un lanzador por cada ejecutable del PATH del sistema, que lo ejecuta fuera con
// host-spawn (o flatpak-spawn --host), y pone su carpeta al principio del PATH: así cualquier
// QProcess de Edita o de un plugin que llame a «git» o a «clangd» llega al del sistema sin
// cambiar cada llamada. Fuera de Flatpak, nada de esto se usa.

#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

namespace edita::sandbox {

// ¿Corre en Flatpak? (/.flatpak-info; EDITA_SANDBOX=flatpak|none lo fuerza en las pruebas)
bool isFlatpak();

// La orden que ejecuta un programa fuera de la sandbox: host-spawn (con un pseudoterminal si
// la entrada es una terminal, como pide la terminal integrada) o flatpak-spawn --host
QStringList hostLauncher();

// Dónde se ve desde la sandbox una carpeta del sistema: /usr, /bin, /sbin, /lib… y /etc están
// en /run/host; el resto (la carpeta personal, /opt) en el mismo sitio
QString visibleHostDir(const QString& hostDir);

// Crea en `dir` (vacía antes) un lanzador «.edita-host-run» y un enlace a él por cada
// ejecutable de las carpetas `hostPath` (vistas desde la sandbox), salvo los de `skip`. El
// lanzador ejecuta `launcher` con el nombre del enlace y los argumentos. Devuelve cuántos
int createBridge(const QString& dir, const QStringList& hostPath, const QStringList& launcher, const QSet<QString>& skip);

// Al arrancar, en Flatpak: pregunta al sistema su PATH y su shell, crea el puente en
// $XDG_RUNTIME_DIR y lo pone al principio del PATH de Edita
void setup();

// La shell del usuario en el sistema ($SHELL fuera de la sandbox; vacía si no se sabe)
QString hostShell();

// Las carpetas de diccionarios de Hunspell del sistema, vistas desde la sandbox
QStringList hostDictionaryDirs();

} // namespace edita::sandbox
