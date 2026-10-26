// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lsp/JsonRpc.h"

#include <QJsonDocument>
#include <QList>

namespace edita::lsp {

void JsonRpcStream::feed(const QByteArray& data)
{
    _buffer += data;
}

std::optional<QJsonObject> JsonRpcStream::next()
{
    for (;;) {
        const qsizetype headerEnd = _buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return std::nullopt;
        qsizetype length = -1;
        for (const QByteArray& line : _buffer.left(headerEnd).split('\n')) {
            const QByteArray header = line.trimmed();
            if (header.toLower().startsWith("content-length:")) {
                bool ok = false;
                length = header.mid(15).trimmed().toLongLong(&ok);
                if (!ok)
                    length = -1;
            }
        }
        const qsizetype bodyStart = headerEnd + 4;
        if (length < 0) { // una cabecera sin longitud: se salta
            _buffer.remove(0, bodyStart);
            continue;
        }
        if (_buffer.size() < bodyStart + length)
            return std::nullopt;
        const QByteArray body = _buffer.mid(bodyStart, length);
        _buffer.remove(0, bodyStart + length);
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
        if (error.error == QJsonParseError::NoError && doc.isObject())
            return doc.object();
    }
}

QByteArray JsonRpcStream::encode(const QJsonObject& message)
{
    QJsonObject full = message;
    full.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    const QByteArray body = QJsonDocument(full).toJson(QJsonDocument::Compact);
    return "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
}

} // namespace edita::lsp
