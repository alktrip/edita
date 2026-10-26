// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Mensajes JSON-RPC 2.0 con la cabecera del Language Server Protocol
// («Content-Length: N\r\n\r\n» y N bytes de JSON en UTF-8).

#pragma once

#include <QByteArray>
#include <QJsonObject>

#include <optional>

namespace edita::lsp {

class JsonRpcStream
{
public:
    // Añade lo que llegó del servidor (puede ser medio mensaje o varios)
    void feed(const QByteArray& data);
    // El siguiente mensaje completo, si lo hay. Un JSON que no se entiende se descarta
    std::optional<QJsonObject> next();
    bool hasPending() const { return !_buffer.isEmpty(); }

    static QByteArray encode(const QJsonObject& message);

private:
    QByteArray _buffer;
};

} // namespace edita::lsp
