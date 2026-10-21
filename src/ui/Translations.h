// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
//
// Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team).
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <QString>
#include <QStringList>

#include <vector>

namespace edita::i18n {

// Un idioma de la interfaz: código ("es") y su nombre en ese idioma ("Español")
struct Language
{
    QString code;
    QString nativeName;
};

// El inglés (el idioma del código) y los que tienen traducción en el binario
// (:/edita/i18n/edita_<código>.qm), ordenados por nombre
std::vector<Language> availableLanguages();

// El idioma que se usará: `preference` si hay traducción para él; si está vacía,
// el primero de `uiLanguages` (los del sistema, "es-MX", "en-US"…) que la tenga;
// si no, inglés
QString resolveLanguage(const QString& preference, const QStringList& uiLanguages);

// Carga las traducciones de Edita y las de Qt (botones y diálogos estándar) para
// `code`, quitando las anteriores, y fija ese idioma en QLocale. Los plugins con
// traducciones propias leen el idioma de QLocale().
void install(const QString& code);

// El idioma instalado ("en" hasta que se llame a install)
QString currentLanguage();

// Si todavía no se instaló ningún idioma, el inglés (sus plurales: "1 hit", "2 hits")
void ensureInstalled();

// Lo que hay que llamar al arrancar: el idioma de la preferencia o el del sistema
void installFromPreference(const QString& preference);

} // namespace edita::i18n
