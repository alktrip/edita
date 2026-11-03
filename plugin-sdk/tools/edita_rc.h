// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// edita_rc: los recursos de los .rc de un plugin de Notepad++ (diálogos, menús, cadenas),
// en el formato binario de Windows, en un .cpp que se compila dentro del plugin.

#pragma once

#include <cstddef>

namespace edita_rc {

struct Constant
{
    const char* name;
    long long value;
};

extern const Constant kConstants[];
extern const std::size_t kConstantCount;

} // namespace edita_rc
