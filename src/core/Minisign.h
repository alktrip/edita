// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Firmas Ed25519 con el formato de minisign (https://jedisct1.github.io/minisign/), las
// del catálogo de plugins (alktrip/edita-plugins, scripts/catalog.py). Con OpenSSL.
//
//   clave pública:  "untrusted comment: …\n" + base64("Ed" + id[8] + clave[32])
//   firma (.minisig):
//     untrusted comment: …
//     base64("ED" + id[8] + firma[64])     firma del BLAKE2b-512 del archivo («Ed»: del archivo)
//     trusted comment: …
//     base64(firma[64])                    firma de firma[64] + el comentario de confianza

#pragma once

#include <QByteArray>
#include <QString>

namespace edita::minisign {

struct PublicKey
{
    QByteArray keyId; // 8 bytes
    QByteArray key;   // 32 bytes
    bool isValid() const { return keyId.size() == 8 && key.size() == 32; }
};

// El archivo .pub o solo su línea en base64
bool parsePublicKey(const QString& text, PublicKey& key, QString* error = nullptr);

// Comprueba la firma como minisign -V; devuelve el comentario de confianza
bool verify(const QByteArray& data, const QByteArray& signature, const PublicKey& key, QString* trustedComment = nullptr,
            QString* error = nullptr);

// Firma `data` (prehasheada, «ED») con una semilla Ed25519 de 32 bytes; para las pruebas
QByteArray sign(const QByteArray& data, const QByteArray& keyId, const QByteArray& seed, const QString& trustedComment);
// La clave pública de una semilla, en el formato de minisign
QString publicKeyFromSeed(const QByteArray& keyId, const QByteArray& seed);

// El identificador como lo muestra minisign (entero de 64 bits little endian, en hexadecimal)
QString keyIdText(const QByteArray& keyId);

} // namespace edita::minisign
