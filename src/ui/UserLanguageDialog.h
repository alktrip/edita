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

#include "syntax/UserLanguages.h"

#include <QDialog>

#include <array>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace edita {

class Prompter;

// "Definir tu lenguaje…" (UserDefineDialog de N++): edita los lenguajes del
// usuario con las mismas pestañas que N++ (plegado, palabras clave, comentarios
// y números, operadores y delimitadores) más una de estilos. "Guardar" escribe
// el XML y la ventana vuelve a colorear los documentos de ese lenguaje.
class UserLanguageDialog : public QDialog
{
    Q_OBJECT

public:
    UserLanguageDialog(UserLanguages& languages, Prompter& prompter, QWidget* parent = nullptr);

    // Lenguaje que se está editando (vacío si no hay ninguno)
    QString currentName() const;
    void selectLanguage(const QString& name);
    // El lenguaje tal como está en el diálogo
    UserLanguage editedLanguage() const;

    // Acciones de los botones, públicas para los tests
    bool createLanguage(const QString& name);
    bool saveCurrent();
    bool removeCurrent();
    bool renameCurrent(const QString& newName);
    bool saveCurrentAs(const QString& newName);

signals:
    // Se guardó un lenguaje (o se borró): hay que volver a aplicar estilos
    void languagesChanged();

private:
    void buildFolderPage(QWidget* page);
    void buildKeywordsPage(QWidget* page);
    void buildCommentsPage(QWidget* page);
    void buildOperatorsPage(QWidget* page);
    void buildStylesPage(QWidget* page);
    void refreshLanguageList(const QString& select);
    void loadLanguage(const UserLanguage& language);
    void showStyle(int id);
    void storeStyle();
    void updateStyleButtons();
    QString askName(const QString& title, const QString& initial);

    UserLanguages& _languages;
    Prompter& _prompter;
    UserLanguage _current;  // copia de trabajo
    bool _loading = false;

    QComboBox* _languageCombo;
    QLineEdit* _extensions = nullptr;
    QCheckBox* _caseIgnored = nullptr;

    std::array<QLineEdit*, 9> _folders{};          // code1 abrir/medio/cerrar, code2…, comentario…
    QCheckBox* _foldCompact = nullptr;
    std::array<QPlainTextEdit*, 8> _keywords{};
    std::array<QCheckBox*, 8> _prefix{};
    std::array<QLineEdit*, 5> _comments{};         // línea: abrir, continuar, cerrar; bloque: abrir, cerrar
    QCheckBox* _foldComments = nullptr;
    QButtonGroup* _pureLC = nullptr;
    std::array<QLineEdit*, 7> _numbers{};          // prefijo1, prefijo2, extras1, extras2, sufijo1, sufijo2, rango
    QButtonGroup* _decimal = nullptr;
    QLineEdit* _operators1 = nullptr;
    QLineEdit* _operators2 = nullptr;
    std::array<QLineEdit*, 24> _delimiters{};      // 8 × (abrir, escape, cerrar)

    QListWidget* _styleList = nullptr;
    QPushButton* _fgButton = nullptr;
    QPushButton* _bgButton = nullptr;
    QCheckBox* _fgTransparent = nullptr;
    QCheckBox* _bgTransparent = nullptr;
    QComboBox* _fontName = nullptr;
    QSpinBox* _fontSize = nullptr;
    QCheckBox* _bold = nullptr;
    QCheckBox* _italic = nullptr;
    QCheckBox* _underline = nullptr;
    std::array<QCheckBox*, 21> _nesting{};
    QWidget* _nestingBox = nullptr;
    int _styleId = 0;
    int _fgColor = 0;
    int _bgColor = 0xFFFFFF;
};

} // namespace edita
