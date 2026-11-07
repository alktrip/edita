// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/FindCharsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace edita {

FindCharsDialog::FindCharsDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("findCharsDialog"));
    setWindowTitle(tr("Find Characters in Range"));
    auto* range = new QGroupBox(tr("Range"));
    auto* rangeLayout = new QVBoxLayout(range);
    _nonAscii = new QRadioButton(tr("&Non-ASCII characters (128 and more)"));
    _nonAscii->setObjectName(QStringLiteral("nonAscii"));
    _ascii = new QRadioButton(tr("&ASCII characters (0 - 127)"));
    _ascii->setObjectName(QStringLiteral("ascii"));
    _custom = new QRadioButton(tr("&My range:"));
    _custom->setObjectName(QStringLiteral("customRange"));
    _nonAscii->setChecked(true);
    _from = new QSpinBox;
    _from->setObjectName(QStringLiteral("rangeFrom"));
    _to = new QSpinBox;
    _to->setObjectName(QStringLiteral("rangeTo"));
    for (QSpinBox* spin : {_from, _to}) {
        spin->setRange(0, 0x10FFFF);
        spin->setEnabled(false);
    }
    _to->setValue(255);
    auto* custom = new QHBoxLayout;
    custom->addWidget(_custom);
    custom->addWidget(_from);
    custom->addWidget(new QLabel(QStringLiteral("–")));
    custom->addWidget(_to);
    rangeLayout->addWidget(_nonAscii);
    rangeLayout->addWidget(_ascii);
    rangeLayout->addLayout(custom);
    connect(_custom, &QRadioButton::toggled, _from, &QSpinBox::setEnabled);
    connect(_custom, &QRadioButton::toggled, _to, &QSpinBox::setEnabled);

    auto* direction = new QGroupBox(tr("Direction"));
    auto* directionLayout = new QHBoxLayout(direction);
    _up = new QRadioButton(tr("&Up"));
    _up->setObjectName(QStringLiteral("up"));
    auto* down = new QRadioButton(tr("&Down"));
    down->setChecked(true);
    directionLayout->addWidget(_up);
    directionLayout->addWidget(down);
    _wrap = new QCheckBox(tr("Wra&p around"));
    _wrap->setObjectName(QStringLiteral("wrap"));
    _wrap->setChecked(true);

    auto* buttons = new QDialogButtonBox;
    QPushButton* find = buttons->addButton(tr("&Find"), QDialogButtonBox::ActionRole);
    find->setObjectName(QStringLiteral("find"));
    find->setDefault(true);
    buttons->addButton(tr("Close"), QDialogButtonBox::RejectRole);
    connect(find, &QPushButton::clicked, this, &FindCharsDialog::findRequested);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(range);
    layout->addWidget(direction);
    layout->addWidget(_wrap);
    layout->addWidget(buttons);
}

char32_t FindCharsDialog::low() const
{
    if (_custom->isChecked())
        return static_cast<char32_t>(std::min(_from->value(), _to->value()));
    return _ascii->isChecked() ? 0 : 128;
}

char32_t FindCharsDialog::high() const
{
    if (_custom->isChecked())
        return static_cast<char32_t>(std::max(_from->value(), _to->value()));
    return _ascii->isChecked() ? 127 : 0x10FFFF;
}

bool FindCharsDialog::forward() const
{
    return !_up->isChecked();
}

bool FindCharsDialog::wrap() const
{
    return _wrap->isChecked();
}

} // namespace edita
