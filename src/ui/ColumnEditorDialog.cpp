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

#include "ui/ColumnEditorDialog.h"

#include "core/ConfigXml.h"
#include "ui/EditCommands.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QVBoxLayout>

namespace edita {

namespace {

const QString kConfig = QStringLiteral("ColumnEditor");

// Un campo en rojo hasta que se corrige (sendValidationErrorMessage de N++)
void markInvalid(QLineEdit* edit, bool invalid)
{
    edit->setStyleSheet(invalid ? QStringLiteral("QLineEdit { border: 1px solid #d1242f; }") : QString());
    if (invalid)
        edit->setFocus();
}

} // namespace

ColumnEditorDialog::ColumnEditorDialog(ScintillaEditBase& view, ConfigXml& config, QWidget* parent)
    : QDialog(parent)
    , _view(view)
    , _config(config)
{
    setWindowTitle(tr("Column / Multi-Selection Editor"));

    _textMode = new QRadioButton(tr("&Text to Insert"));
    _textMode->setObjectName(QStringLiteral("textMode"));
    _text = new QLineEdit;
    _text->setObjectName(QStringLiteral("text"));
    auto* textBox = new QVBoxLayout;
    textBox->addWidget(_textMode);
    textBox->addWidget(_text);

    _numberMode = new QRadioButton(tr("&Number to Insert"));
    _numberMode->setObjectName(QStringLiteral("numberMode"));
    _initial = new QLineEdit;
    _initial->setObjectName(QStringLiteral("initial"));
    _increase = new QLineEdit;
    _increase->setObjectName(QStringLiteral("increase"));
    _repeat = new QLineEdit;
    _repeat->setObjectName(QStringLiteral("repeat"));
    _leading = new QComboBox;
    _leading->setObjectName(QStringLiteral("leading"));
    _leading->addItem(tr("None"), static_cast<int>(NumberLeading::None));
    _leading->addItem(tr("Zeros"), static_cast<int>(NumberLeading::Zeros));
    _leading->addItem(tr("Spaces"), static_cast<int>(NumberLeading::Spaces));
    auto* numberForm = new QFormLayout;
    numberForm->addRow(tr("&Initial number:"), _initial);
    numberForm->addRow(tr("Increase b&y:"), _increase);
    numberForm->addRow(tr("&Repeat:"), _repeat);
    numberForm->addRow(tr("&Leading:"), _leading);

    auto* format = new QGroupBox(tr("Format"));
    _decimal = new QRadioButton(tr("&Dec"));
    _decimal->setObjectName(QStringLiteral("decimal"));
    _hex = new QRadioButton(tr("&Hex"));
    _hex->setObjectName(QStringLiteral("hex"));
    _octal = new QRadioButton(tr("&Oct"));
    _octal->setObjectName(QStringLiteral("octal"));
    _binary = new QRadioButton(tr("&Bin"));
    _binary->setObjectName(QStringLiteral("binary"));
    _hexCase = new QComboBox;
    _hexCase->setObjectName(QStringLiteral("hexCase"));
    _hexCase->addItems({QStringLiteral("a-f"), QStringLiteral("A-F")});
    auto* formatGrid = new QGridLayout(format);
    formatGrid->addWidget(_decimal, 0, 0);
    formatGrid->addWidget(_hex, 0, 1);
    formatGrid->addWidget(_hexCase, 0, 2);
    formatGrid->addWidget(_octal, 1, 0);
    formatGrid->addWidget(_binary, 1, 1);
    auto* formatGroup = new QButtonGroup(this);
    for (QRadioButton* b : {_decimal, _hex, _octal, _binary})
        formatGroup->addButton(b);

    auto* modeGroup = new QButtonGroup(this);
    modeGroup->addButton(_textMode);
    modeGroup->addButton(_numberMode);
    connect(_textMode, &QRadioButton::toggled, this, &ColumnEditorDialog::updateEnabled);
    connect(_hex, &QRadioButton::toggled, this, &ColumnEditorDialog::updateEnabled);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (insert())
            accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(textBox);
    layout->addSpacing(8);
    layout->addWidget(_numberMode);
    layout->addLayout(numberForm);
    layout->addWidget(format);
    layout->addWidget(buttons);

    load();
    updateEnabled();
}

void ColumnEditorDialog::updateEnabled()
{
    const bool text = _textMode->isChecked();
    _text->setEnabled(text);
    for (QWidget* w : std::initializer_list<QWidget*>{_initial, _increase, _repeat, _leading, _decimal, _hex, _octal, _binary})
        w->setEnabled(!text);
    _hexCase->setEnabled(!text && _hex->isChecked());
}

NumberBase ColumnEditorDialog::base() const
{
    if (_hex->isChecked())
        return _hexCase->currentIndex() == 1 ? NumberBase::HexUpper : NumberBase::Hex;
    if (_octal->isChecked())
        return NumberBase::Octal;
    if (_binary->isChecked())
        return NumberBase::Binary;
    return NumberBase::Decimal;
}

void ColumnEditorDialog::load()
{
    // Lo último usado; la primera vez, números desde 1 de 1 en 1, como en N++
    auto get = [this](const char* attr, const QString& fallback) {
        const QString value = _config.guiConfig(kConfig, QString::fromLatin1(attr));
        return value.isEmpty() ? fallback : value;
    };
    (get("choice", QStringLiteral("text")) == QLatin1String("number") ? _numberMode : _textMode)->setChecked(true);
    _text->setText(_config.guiConfig(kConfig, QStringLiteral("text")));
    _initial->setText(get("initial", QStringLiteral("1")));
    _increase->setText(get("increment", QStringLiteral("1")));
    _repeat->setText(get("repeat", QStringLiteral("1")));
    const int format = get("formatChoice", QStringLiteral("0")).toInt();
    QRadioButton* const formats[] = {_decimal, _hex, _octal, _binary, _hex};
    formats[std::clamp(format, 0, 4)]->setChecked(true);
    _hexCase->setCurrentIndex(format == 4 ? 1 : 0);
    _leading->setCurrentIndex(std::clamp(get("leadingChoice", QStringLiteral("0")).toInt(), 0, 2));
}

void ColumnEditorDialog::save() const
{
    auto set = [this](const char* attr, const QString& value) { _config.setGuiConfig(kConfig, QString::fromLatin1(attr), value); };
    set("choice", _textMode->isChecked() ? QStringLiteral("text") : QStringLiteral("number"));
    set("text", _text->text());
    set("initial", _initial->text());
    set("increment", _increase->text());
    set("repeat", _repeat->text());
    // formatChoice de N++: 0 dec, 1 hex, 2 oct, 3 bin (4: hex en mayúsculas)
    const NumberBase b = base();
    const int format = b == NumberBase::Hex ? 1 : b == NumberBase::Octal ? 2 : b == NumberBase::Binary ? 3 : b == NumberBase::HexUpper ? 4 : 0;
    set("formatChoice", QString::number(format));
    set("leadingChoice", QString::number(_leading->currentIndex()));
    _config.save();
}

bool ColumnEditorDialog::insert()
{
    std::vector<std::string> texts;
    const std::size_t count = columnInsertCount(_view);
    if (_textMode->isChecked()) {
        texts.assign(count, _text->text().toStdString());
    } else {
        ColumnNumbers params;
        params.base = base();
        params.leading = static_cast<NumberLeading>(_leading->currentData().toInt());
        long long repeat = 1;
        bool ok = true;
        for (auto [edit, value] : {std::pair{_initial, &params.initial}, std::pair{_increase, &params.increase},
                                   std::pair{_repeat, &repeat}}) {
            const bool valid = parseNumber(edit->text().trimmed().toStdString(), params.base, *value);
            markInvalid(edit, !valid);
            ok = ok && valid;
        }
        if (!ok)
            return false;
        params.repeat = static_cast<int>(std::clamp(repeat, 1LL, 1000000LL));
        texts = columnNumbers(params, count);
    }
    save();
    columnInsert(_view, texts);
    return true;
}

} // namespace edita
