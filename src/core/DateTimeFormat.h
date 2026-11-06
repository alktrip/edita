// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Editar › Insertar › Fecha y hora (IDM_EDIT_INSERT_DATETIME_* de N++)

#pragma once

#include <QDateTime>
#include <QString>

namespace edita {

// El formato personalizado de N++ (el de GetDateFormat/GetTimeFormat de Windows: yyyy, MM, dd,
// HH, mm, ss, ddd, MMMM, tt…; el texto entre comillas simples, tal cual), con los nombres de los
// días y los meses en el idioma del sistema
QString formatDateTime(const QString& format, const QDateTime& when);

// Corta o larga, en el formato del sistema: la hora (sin segundos) y la fecha, o al revés
QString localDateTime(const QDateTime& when, bool longFormat, bool dateFirst);

} // namespace edita
