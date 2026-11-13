// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Detector de secretos (fase 88; no existe en Notepad++): el motor, sin interfaz. Busca en
// un texto claves de API, tokens, claves privadas y contraseñas, con reglas por formato
// (los prefijos que publican GitHub, AWS, Stripe…) y una regla general para asignaciones
// como password = "…", que además pide que el valor parezca aleatorio y no un ejemplo.

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>

namespace edita::secrets {

struct Finding
{
    QString ruleId;      // «github-token»
    QString description; // «GitHub token» (en inglés; la interfaz lo traduce)
    int line = 0;        // desde 0
    int column = 0;      // en caracteres, desde 0
    qsizetype start = 0; // en bytes UTF-8 desde el principio del texto (para Scintilla)
    qsizetype length = 0;
    QString secret;
    QString fingerprint; // regla + secreto, en SHA-256: para ignorar un hallazgo
};

struct RuleInfo
{
    QString id;
    QString description;
};
const std::vector<RuleInfo>& rules();

// Los secretos de `text` (UTF-8). `fileName` (solo el nombre) activa las reglas de los
// archivos .env. Las líneas con un marcador de permiso se saltan (allowLineMarkers)
std::vector<Finding> scan(const QByteArray& text, const QString& fileName = {});

// «edita:allow-secret», y los de gitleaks y detect-secrets, para no cambiar de costumbre
const QStringList& allowLineMarkers();

// Lo que se muestra del secreto: los primeros 4 caracteres y los 2 últimos
QString mask(const QString& secret);

// Entropía de Shannon en bits por carácter
double entropy(const QString& text);

// ¿Es un valor de ejemplo (changeme, ${VAR}, <token>, xxxx…)?
bool looksLikePlaceholder(const QString& value);

QString fingerprint(const QString& ruleId, const QString& secret);

} // namespace edita::secrets
