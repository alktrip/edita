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

#include "core/ConfigXml.h"
#include "core/LangType.h"
#include "syntax/LanguageRegistry.h"
#include "syntax/StyleSheet.h"
#include "syntax/UserLanguages.h"
#include "syntax/SyntaxStyler.h"

#include <QObject>
#include <QStringList>

#include <string_view>
#include <vector>

class ScintillaEditBase;

namespace edita {

// Configuración de sintaxis de la aplicación: langs.xml, stylers.xml o el tema
// elegido, y config.xml para recordar el tema. Equivale a la parte de
// NppParameters::load que prepara esos archivos:
//   - En el primer arranque copia langs.model.xml y stylers.model.xml al
//     directorio del usuario (~/.config/edita/), como N++ con %APPDATA%.
//   - Si el archivo del usuario está corrupto usa el modelo y lo avisa (N++
//     ofrece recuperarlo).
class SyntaxManager : public QObject
{
    Q_OBJECT

public:
    struct Theme
    {
        QString name; // nombre visible (el del archivo sin .xml)
        QString path; // vacío = stylers.xml del usuario (tema por defecto)
    };

    explicit SyntaxManager(const QString& configDir, QObject* parent = nullptr);

    // Prepara los archivos y lo carga todo. Devuelve los avisos (vacío si todo fue bien).
    QStringList initialize();
    // Vuelve a leer langs.xml y los estilos activos (tras editarlos a mano)
    QStringList reload();

    // config.xml (lo lee initialize()); otros módulos guardan aquí sus entradas
    ConfigXml& config() { return _config; }

    const LanguageRegistry& languages() const { return _languages; }
    const StyleSheet& styles() const { return _styles; }
    StyleSheet& styles() { return _styles; }

    QString langsPath() const;
    QString stylersPath() const;
    QString configPath() const;
    // Archivo de estilos en uso: stylers.xml o el tema
    QString activeStylePath() const;
    QString activeThemePath() const { return _themePath; }

    // Temas disponibles: el predeterminado y los .xml de themes/ (datos de la
    // aplicación y ~/.config/edita/themes), ordenados por nombre
    std::vector<Theme> themes() const;
    // Cambia de tema y lo recuerda en config.xml. Ruta vacía = predeterminado.
    bool setTheme(const QString& path, QString* error = nullptr);

    // Opciones de vista que dependen de las preferencias; si cambian se emite stylesChanged
    const StylerOptions& options() const { return _options; }
    void setOptions(const StylerOptions& options);

    // Aplica lexer y estilos a una vista (SyntaxStyler::apply). Con LangType::User,
    // `userLanguage` es el nombre del lenguaje del usuario.
    void apply(ScintillaEditBase& view, LangType lang, const QString& userLanguage = {}) const;

    // Lenguajes definidos por el usuario (userDefineLang.xml y userDefineLangs/)
    UserLanguages& userLanguages() { return _userLanguages; }
    const UserLanguages& userLanguages() const { return _userLanguages; }
    // Nombre del lenguaje del usuario para un archivo (vacío si ninguno)
    QString detectUserLanguage(const QString& filePath) const;
    // Modo oscuro: entre dos lenguajes del usuario para la misma extensión, el "darkModeTheme"
    void setDarkMode(bool dark) { _darkMode = dark; }
    // Tras cambiar un lenguaje del usuario: hay que volver a aplicar los estilos
    void notifyUserLanguagesChanged() { emit stylesChanged(); }

    // Lenguaje para un archivo (DocumentManager::LanguageDetector)
    LangType detect(const QString& filePath, std::string_view textStart) const;

    // ¿Es uno de los archivos de configuración en uso? (al guardarlo se recarga)
    bool isActiveConfigFile(const QString& path) const;

signals:
    // Cambiaron langs.xml, los estilos o el tema: hay que volver a aplicar
    void stylesChanged();

private:
    QString ensureUserFile(const QString& userName, const QString& modelName, QStringList& warnings) const;
    QStringList loadAll();

    QString _configDir;
    ConfigXml _config;
    LanguageRegistry _languages;
    StyleSheet _styles;
    UserLanguages _userLanguages;
    QString _themePath;
    bool _darkMode = false;
    StylerOptions _options;
};

} // namespace edita
