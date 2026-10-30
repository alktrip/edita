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

#include "ui/Prompter.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QInputDialog>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>

namespace edita {

Prompter::SaveChoice DialogPrompter::askSaveChanges(QWidget* parent, const QString& documentName, bool offerAll)
{
    QMessageBox box(QMessageBox::Question, QCoreApplication::translate("Prompter", "Save"),
                    QCoreApplication::translate("Prompter", "Save changes to \"%1\"?").arg(documentName), QMessageBox::NoButton, parent);
    QPushButton* save = box.addButton(QCoreApplication::translate("Prompter", "&Save"), QMessageBox::AcceptRole);
    QPushButton* discard = box.addButton(QCoreApplication::translate("Prompter", "&Don't Save"), QMessageBox::DestructiveRole);
    QPushButton* saveAll = offerAll ? box.addButton(QCoreApplication::translate("Prompter", "Sa&ve All"), QMessageBox::AcceptRole) : nullptr;
    QPushButton* discardAll = offerAll ? box.addButton(QCoreApplication::translate("Prompter", "Don't Save &Any"), QMessageBox::DestructiveRole) : nullptr;
    QPushButton* cancel = box.addButton(QCoreApplication::translate("Prompter", "Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(save);
    box.setEscapeButton(cancel);
    box.exec();

    const QAbstractButton* clicked = box.clickedButton();
    if (clicked == save)
        return SaveChoice::Save;
    if (clicked == discard)
        return SaveChoice::Discard;
    if (saveAll && clicked == saveAll)
        return SaveChoice::SaveAll;
    if (discardAll && clicked == discardAll)
        return SaveChoice::DiscardAll;
    return SaveChoice::Cancel;
}

bool DialogPrompter::confirm(QWidget* parent, const QString& title, const QString& text)
{
    return QMessageBox::question(parent, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        == QMessageBox::Yes;
}

void DialogPrompter::warn(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox::warning(parent, title, text);
}

QStringList DialogPrompter::openFileNames(QWidget* parent, const QString& directory, const QString& filters)
{
    return QFileDialog::getOpenFileNames(parent, QCoreApplication::translate("Prompter", "Open"), directory, filters);
}

QString DialogPrompter::saveFileName(QWidget* parent, const QString& suggestedPath, const QString& filters,
                                     const QString& selectedFilter)
{
    QString selected = selectedFilter;
    return QFileDialog::getSaveFileName(parent, QCoreApplication::translate("Prompter", "Save As"), suggestedPath, filters, &selected);
}

Prompter::UnmappableChoice DialogPrompter::askUnmappable(QWidget* parent, const QString& documentName,
                                                         std::size_t count, const QString& encodingName)
{
    QMessageBox box(QMessageBox::Warning, QCoreApplication::translate("Prompter", "Unrepresentable characters"),
                    QCoreApplication::translate("Prompter", "\"%1\" contains %2 character(s) that do not exist in %3.\nIf you save in %3 they will "
                       "be written as \"?\" and lost.")
                        .arg(documentName, QString::number(count), encodingName),
                    QMessageBox::NoButton, parent);
    QPushButton* convert = box.addButton(QCoreApplication::translate("Prompter", "Convert to &UTF-8 and Save"), QMessageBox::AcceptRole);
    QPushButton* anyway = box.addButton(QCoreApplication::translate("Prompter", "Save in %1 Anyway").arg(encodingName), QMessageBox::DestructiveRole);
    QPushButton* cancel = box.addButton(QCoreApplication::translate("Prompter", "Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(convert);
    box.setEscapeButton(cancel);
    box.exec();
    if (box.clickedButton() == convert)
        return UnmappableChoice::ConvertToUtf8;
    if (box.clickedButton() == anyway)
        return UnmappableChoice::SaveAnyway;
    return UnmappableChoice::Cancel;
}

bool DialogPrompter::askNameAndShortcut(QWidget* parent, const QString& title, QString& name, QKeySequence& shortcut)
{
    // MacroShortcut::doDialog (macros y órdenes de Ejecutar): nombre y combinación de teclas
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    auto* nameEdit = new QLineEdit(name, &dialog);
    auto* keyEdit = new QKeySequenceEdit(shortcut, &dialog);
    keyEdit->setMaximumSequenceLength(1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QCoreApplication::translate("Prompter", "Save"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("Prompter", "Cancel"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(nameEdit, &QLineEdit::textChanged, &dialog, [buttons](const QString& text) {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!text.trimmed().isEmpty());
    });
    buttons->button(QDialogButtonBox::Ok)->setEnabled(!name.trimmed().isEmpty());
    auto* form = new QFormLayout(&dialog);
    form->addRow(QCoreApplication::translate("Prompter", "Name:"), nameEdit);
    form->addRow(QCoreApplication::translate("Prompter", "Shortcut:"), keyEdit);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    name = nameEdit->text().trimmed();
    shortcut = keyEdit->keySequence();
    return true;
}

bool DialogPrompter::askText(QWidget* parent, const QString& title, const QString& label, QString& text)
{
    bool ok = false;
    const QString answer = QInputDialog::getText(parent, title, label, QLineEdit::Normal, text, &ok);
    if (ok)
        text = answer;
    return ok;
}

} // namespace edita
