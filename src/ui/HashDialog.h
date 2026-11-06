// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Herramientas › MD5 / SHA-1 / SHA-256 / SHA-512 (HashFromTextDlg y HashFromFilesDlg de N++,
// MISC/md5): el resumen de un texto (también línea a línea) o de varios archivos.

#pragma once

#include <QCryptographicHash>
#include <QDialog>
#include <QStringList>

class QCheckBox;
class QPlainTextEdit;

namespace edita {

class Prompter;

class HashDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { Text, Files };

    HashDialog(Mode mode, QCryptographicHash::Algorithm algorithm, Prompter& prompter, QWidget* parent = nullptr);

    static QString algorithmName(QCryptographicHash::Algorithm algorithm); // «MD5», «SHA-256»…
    // El resumen en hexadecimal de los bytes UTF-8 del texto
    static QString digest(const QByteArray& data, QCryptographicHash::Algorithm algorithm);
    // El del texto, o uno por línea («Treat each line as a separate string»; las vacías, vacías)
    static QString digestText(const QString& text, QCryptographicHash::Algorithm algorithm, bool eachLine);
    // «resumen  archivo» por archivo, como sha256sum (y el error, si no se pudo leer)
    static QString digestFiles(const QStringList& paths, QCryptographicHash::Algorithm algorithm);

    // Para las pruebas
    void setInputText(const QString& text);
    void setEachLine(bool on);
    void setFiles(const QStringList& paths);
    QString result() const;

private:
    void update();

    Mode _mode;
    QCryptographicHash::Algorithm _algorithm;
    Prompter& _prompter;
    QPlainTextEdit* _input = nullptr;
    QPlainTextEdit* _output = nullptr;
    QCheckBox* _eachLine = nullptr;
    QStringList _files;
};

} // namespace edita
