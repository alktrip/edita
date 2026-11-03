// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/Minisign.h"

#include <QCoreApplication>
#include <QStringList>

#include <openssl/evp.h>

#include <algorithm>
#include <memory>

namespace edita::minisign {

namespace {

constexpr qsizetype kSignatureSize = 64;

QString tr(const char* text)
{
    return QCoreApplication::translate("Minisign", text);
}

void setError(QString* error, const QString& text)
{
    if (error)
        *error = text;
}

const unsigned char* bytes(const QByteArray& data)
{
    return reinterpret_cast<const unsigned char*>(data.constData());
}

struct KeyDeleter
{
    void operator()(EVP_PKEY* key) const { EVP_PKEY_free(key); }
};
struct ContextDeleter
{
    void operator()(EVP_MD_CTX* context) const { EVP_MD_CTX_free(context); }
};
using Key = std::unique_ptr<EVP_PKEY, KeyDeleter>;
using Context = std::unique_ptr<EVP_MD_CTX, ContextDeleter>;

QByteArray blake2b512(const QByteArray& data)
{
    QByteArray digest(EVP_MAX_MD_SIZE, '\0');
    unsigned int size = 0;
    if (EVP_Digest(data.constData(), static_cast<size_t>(data.size()), reinterpret_cast<unsigned char*>(digest.data()), &size,
                   EVP_blake2b512(), nullptr)
        != 1)
        return {};
    digest.resize(static_cast<qsizetype>(size));
    return digest;
}

bool verifyEd25519(const QByteArray& publicKey, const QByteArray& message, const QByteArray& signature)
{
    const Key key(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, bytes(publicKey), static_cast<size_t>(publicKey.size())));
    const Context context(EVP_MD_CTX_new());
    if (!key || !context || EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1)
        return false;
    return EVP_DigestVerify(context.get(), bytes(signature), static_cast<size_t>(signature.size()), bytes(message),
                            static_cast<size_t>(message.size()))
        == 1;
}

Key privateKey(const QByteArray& seed)
{
    return Key(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, bytes(seed), static_cast<size_t>(seed.size())));
}

QByteArray signEd25519(EVP_PKEY* key, const QByteArray& message)
{
    const Context context(EVP_MD_CTX_new());
    QByteArray signature(kSignatureSize, '\0');
    size_t size = static_cast<size_t>(signature.size());
    if (!context || EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key) != 1
        || EVP_DigestSign(context.get(), reinterpret_cast<unsigned char*>(signature.data()), &size, bytes(message),
                          static_cast<size_t>(message.size()))
            != 1)
        return {};
    return signature;
}

// Base64 estricto: sin espacios ni caracteres de más
bool fromBase64(const QByteArray& text, QByteArray& out)
{
    const auto result = QByteArray::fromBase64Encoding(text.trimmed(), QByteArray::AbortOnBase64DecodingErrors);
    if (!result)
        return false;
    out = *result;
    return true;
}

} // namespace

QString keyIdText(const QByteArray& keyId)
{
    QByteArray reversed = keyId;
    std::reverse(reversed.begin(), reversed.end());
    return QString::fromLatin1(reversed.toHex().toUpper());
}

bool parsePublicKey(const QString& text, PublicKey& key, QString* error)
{
    QStringList lines;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith(QLatin1String("untrusted comment:")))
            lines << trimmed;
    }
    QByteArray raw;
    if (lines.size() != 1 || !fromBase64(lines.first().toLatin1(), raw) || raw.size() != 42 || !raw.startsWith("Ed")) {
        setError(error, tr("the public key is not a minisign Ed25519 key"));
        return false;
    }
    key.keyId = raw.mid(2, 8);
    key.key = raw.mid(10);
    return true;
}

bool verify(const QByteArray& data, const QByteArray& signature, const PublicKey& key, QString* trustedComment, QString* error)
{
    if (!key.isValid()) {
        setError(error, tr("the public key is not valid"));
        return false;
    }
    const QList<QByteArray> lines = signature.split('\n');
    const QByteArray trustedPrefix("trusted comment: ");
    if (lines.size() < 4 || !lines[0].startsWith("untrusted comment:") || !lines[2].startsWith(trustedPrefix)) {
        setError(error, tr("the signature does not have the minisign format"));
        return false;
    }
    QByteArray raw;
    QByteArray globalSignature;
    if (!fromBase64(lines[1], raw) || !fromBase64(lines[3], globalSignature) || raw.size() != 74
        || globalSignature.size() != kSignatureSize) {
        setError(error, tr("the signature does not have the minisign format"));
        return false;
    }
    const QByteArray algorithm = raw.left(2);
    const QByteArray keyId = raw.mid(2, 8);
    const QByteArray fileSignature = raw.mid(10);
    if (keyId != key.keyId) {
        setError(error, tr("the signature was made with another key (%1, not %2)").arg(keyIdText(keyId), keyIdText(key.keyId)));
        return false;
    }
    QByteArray message;
    if (algorithm == "ED")
        message = blake2b512(data);
    else if (algorithm == "Ed")
        message = data;
    if (message.isEmpty() && algorithm != "Ed") {
        setError(error, tr("unknown signature algorithm"));
        return false;
    }
    if (!verifyEd25519(key.key, message, fileSignature)) {
        setError(error, tr("the signature does not match the file"));
        return false;
    }
    QByteArray comment = lines[2].mid(trustedPrefix.size());
    if (comment.endsWith('\r'))
        comment.chop(1);
    if (!verifyEd25519(key.key, fileSignature + comment, globalSignature)) {
        setError(error, tr("the signature of the trusted comment is not valid"));
        return false;
    }
    if (trustedComment)
        *trustedComment = QString::fromUtf8(comment);
    return true;
}

QByteArray sign(const QByteArray& data, const QByteArray& keyId, const QByteArray& seed, const QString& trustedComment)
{
    const Key key = privateKey(seed);
    if (!key || keyId.size() != 8)
        return {};
    const QByteArray fileSignature = signEd25519(key.get(), blake2b512(data));
    const QByteArray comment = trustedComment.toUtf8();
    const QByteArray globalSignature = signEd25519(key.get(), fileSignature + comment);
    if (fileSignature.isEmpty() || globalSignature.isEmpty())
        return {};
    return "untrusted comment: signature from Edita test key\n" + (QByteArray("ED") + keyId + fileSignature).toBase64()
        + "\ntrusted comment: " + comment + '\n' + globalSignature.toBase64() + '\n';
}

QString publicKeyFromSeed(const QByteArray& keyId, const QByteArray& seed)
{
    const Key key = privateKey(seed);
    QByteArray publicKey(32, '\0');
    size_t size = static_cast<size_t>(publicKey.size());
    if (!key || EVP_PKEY_get_raw_public_key(key.get(), reinterpret_cast<unsigned char*>(publicKey.data()), &size) != 1)
        return {};
    return QStringLiteral("untrusted comment: minisign public key %1\n").arg(keyIdText(keyId))
        + QString::fromLatin1((QByteArray("Ed") + keyId + publicKey).toBase64()) + QLatin1Char('\n');
}

} // namespace edita::minisign
