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

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

namespace edita {

// Preferencias › Asociación de archivos (RegExtDlg de N++). N++ escribe en el registro de
// Windows (HKEY_CLASSES_ROOT\.ext = Notepad++_file y guarda el valor de antes en
// Notepad++_backup). En Linux se asocia el tipo MIME de la extensión, según XDG:
//   - $XDG_CONFIG_HOME/mimeapps.list: [Default Applications] tipo=edita.desktop (y el
//     programa de antes se guarda para devolverlo al quitar la asociación) y
//     [Added Associations], para «Abrir con»;
//   - si la extensión no es de un tipo de texto (o no tiene tipo), un tipo propio,
//     application/x-edita-<ext>, en $XDG_DATA_HOME/mime/packages/edita-associations.xml,
//     y update-mime-database;
//   - si Edita no está instalado (el AppImage), su edita.desktop en
//     $XDG_DATA_HOME/applications, con la ruta del AppImage.
// Todo es del usuario: no hace falta ser administrador, al contrario que en N++.
// Las extensiones asociadas se apuntan en $XDG_CONFIG_HOME/edita/fileAssociations.xml.
namespace file_association {

// Los grupos de extensiones de N++ (defExtArray); el último, «customize», va vacío: es
// el campo para escribir otra extensión
struct ExtensionGroup
{
    QString name;
    QStringList extensions;
};
const std::vector<ExtensionGroup>& extensionGroups();

// Dónde y cómo escribe. current() usa las variables XDG del entorno
struct Environment
{
    QString dataHome;     // $XDG_DATA_HOME (~/.local/share)
    QString configHome;   // $XDG_CONFIG_HOME (~/.config)
    QStringList dataDirs; // $XDG_DATA_DIRS (/usr/local/share:/usr/share): ¿está Edita instalado?
    QString appId;        // edita
    QString launcher;     // lo que abre los archivos: $APPIMAGE o el ejecutable
    QString iconFile;     // el ícono para el edita.desktop propio
    // Ejecuta update-mime-database y update-desktop-database; false si no se pudo
    std::function<bool(const QString& program, const QStringList& arguments)> run;

    static Environment current();
    QString desktopId() const { return appId + QStringLiteral(".desktop"); }
};

struct Association
{
    QString extension; // .txt
    QString mimeType;  // text/plain
    QString previous;  // el programa por omisión de antes en mimeapps.list (vacío: ninguno)
    bool ownMimeType = false;
};

// ".TXT", "txt" → ".txt"; vacío si no vale (espacios, comodines, barras…)
QString normalizeExtension(const QString& extension);
// El valor de Exec= de un .desktop para esta ruta (comillas y escapes de la especificación)
QString desktopExec(const QString& program);

class FileAssociations
{
public:
    explicit FileAssociations(Environment env = Environment::current());

    const Environment& environment() const { return _env; }
    QList<Association> registered() const { return _registered; }
    bool isRegistered(const QString& extension) const;

    // El tipo que se asociaría: el primero de texto que conoce el sistema para la extensión
    // o, si no hay, el propio (application/x-edita-…)
    QString mimeTypeFor(const QString& extension, bool* own = nullptr) const;

    // Asociar o quitar; false con `error` si algo no se pudo escribir
    bool add(const QString& extension, QString* error = nullptr);
    bool remove(const QString& extension, QString* error = nullptr);

    // Al arrancar: si el edita.desktop propio apunta a un AppImage que ya no está (se
    // movió o se cambió por otra versión), lo apunta al de ahora. true si lo reescribió
    bool refreshLauncher();

    QString recordFile() const;
    QString mimeAppsFile() const;
    QString mimePackageFile() const;
    QString desktopFile() const; // el propio, en $XDG_DATA_HOME/applications
    // ¿Hay un edita.desktop instalado (en $XDG_DATA_DIRS)?
    bool installedDesktopFile() const;

private:
    void load();
    bool save(QString* error) const;
    // Escribe los tipos propios de `associations` (o borra el paquete si no hay) y
    // ejecuta update-mime-database
    bool writeMimePackage(const QList<Association>& associations, QString* error) const;
    bool writeDesktopFile(QString* error) const;

    Environment _env;
    QList<Association> _registered;
};

} // namespace file_association
} // namespace edita
