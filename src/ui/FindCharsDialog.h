// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Buscar › Buscar caracteres en un rango (FindCharsInRangeDlg de N++): los no ASCII, los ASCII o
// un rango propio de códigos. En Edita los códigos son Unicode (N++: 0-255, los bytes)

#pragma once

#include <QDialog>

class QCheckBox;
class QRadioButton;
class QSpinBox;

namespace edita {

class FindCharsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FindCharsDialog(QWidget* parent = nullptr);

    // El rango elegido (código Unicode)
    char32_t low() const;
    char32_t high() const;
    bool forward() const;
    bool wrap() const;

signals:
    void findRequested();

private:
    QRadioButton* _nonAscii = nullptr;
    QRadioButton* _ascii = nullptr;
    QRadioButton* _custom = nullptr;
    QSpinBox* _from = nullptr;
    QSpinBox* _to = nullptr;
    QRadioButton* _up = nullptr;
    QCheckBox* _wrap = nullptr;
};

} // namespace edita
