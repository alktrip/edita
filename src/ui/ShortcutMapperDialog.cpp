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

#include "ui/ShortcutMapperDialog.h"

#include "ui/MainWindow.h"
#include "ui/Prompter.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace edita {

namespace {

using Kind = ShortcutEntry::Kind;

QString keysText(const std::vector<KeyCombo>& keys)
{
    QStringList texts;
    for (const KeyCombo& key : keys)
        texts << key.toKeySequence().toString(QKeySequence::NativeText);
    return texts.join(QStringLiteral("  /  "));
}

// La tecla para una orden (ShortcutMapper::modify → Shortcut::run)
class KeyCaptureDialog : public QDialog
{
public:
    KeyCaptureDialog(const QString& command, const KeyCombo& current, QWidget* parent)
        : QDialog(parent)
    {
        setWindowTitle(ShortcutMapperDialog::tr("Shortcut"));
        auto* edit = new QKeySequenceEdit(current.toKeySequence(), this);
        edit->setMaximumSequenceLength(1);
        edit->setClearButtonEnabled(true);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(edit, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence& sequence) {
            _combo = KeyCombo::fromKeySequence(sequence);
        });
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(command));
        layout->addWidget(edit);
        layout->addWidget(new QLabel(ShortcutMapperDialog::tr("Press the new key combination.")));
        layout->addWidget(buttons);
        _combo = current;
        edit->setFocus();
    }
    KeyCombo combo() const { return _combo; }

private:
    KeyCombo _combo;
};

} // namespace

ShortcutMapperDialog::ShortcutMapperDialog(MainWindow& window, Prompter& prompter, QWidget* parent)
    : QDialog(parent)
    , _window(window)
    , _prompter(prompter)
{
    setWindowTitle(tr("Shortcut Mapper"));
    _tabs = new QTabWidget;
    _tabs->setObjectName(QStringLiteral("tabs"));
    const QString titles[] = {tr("Main menu"), tr("Macros"), tr("Run commands"), tr("Plugin commands"), tr("Scintilla commands")};
    for (int i = 0; i < 5; ++i) {
        const auto kind = static_cast<Kind>(i);
        auto* table = new QTableWidget(0, 3);
        table->setObjectName(QStringLiteral("table%1").arg(i));
        table->setHorizontalHeaderLabels({tr("Name"), tr("Shortcut"),
                                          kind == Kind::Plugin ? tr("Plugin") : kind == Kind::RunCommands ? tr("Command") : tr("Category")});
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->verticalHeader()->hide();
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        connect(table, &QTableWidget::cellDoubleClicked, this, [this] { modifySelected(); });
        _tables[i] = table;
        _tabs->addTab(table, titles[i]);
    }
    table(Kind::Scintilla)->setColumnHidden(2, true);

    _filter = new QLineEdit;
    _filter->setObjectName(QStringLiteral("filter"));
    _filter->setPlaceholderText(tr("Filter"));
    _filter->setClearButtonEnabled(true);
    connect(_filter, &QLineEdit::textChanged, this, &ShortcutMapperDialog::setFilter);

    auto* modify = new QPushButton(tr("Modify"));
    modify->setObjectName(QStringLiteral("modify"));
    auto* clearButton = new QPushButton(tr("Clear"));
    clearButton->setObjectName(QStringLiteral("clear"));
    _delete = new QPushButton(tr("Delete"));
    _delete->setObjectName(QStringLiteral("delete"));
    auto* close = new QPushButton(tr("Close"));
    connect(modify, &QPushButton::clicked, this, [this] { modifySelected(); });
    connect(clearButton, &QPushButton::clicked, this, [this] {
        if (const ShortcutEntry* entry = selected())
            clear(entry->kind, entry->id);
    });
    connect(_delete, &QPushButton::clicked, this, [this] {
        const ShortcutEntry* entry = selected();
        if (entry && entry->kind == Kind::Macros
            && _prompter.confirm(this, tr("Delete macro"), tr("Delete the macro \"%1\"?").arg(entry->name))) {
            _window.deleteSavedMacro(entry->id.toInt());
            refresh();
        } else if (entry && entry->kind == Kind::RunCommands
                   && _prompter.confirm(this, tr("Delete command"), tr("Delete the command \"%1\"?").arg(entry->name))) {
            _window.deleteUserCommand(entry->id.toInt());
            refresh();
        }
    });
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        _delete->setEnabled(index == static_cast<int>(Kind::Macros) || index == static_cast<int>(Kind::RunCommands));
    });
    _delete->setEnabled(false);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(modify);
    buttons->addWidget(clearButton);
    buttons->addWidget(_delete);
    buttons->addStretch();
    buttons->addWidget(close);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(_filter);
    layout->addWidget(_tabs, 1);
    layout->addLayout(buttons);
    refresh();
    resize(760, 560);
}

void ShortcutMapperDialog::showKind(Kind kind)
{
    _tabs->setCurrentIndex(static_cast<int>(kind));
}

QTableWidget* ShortcutMapperDialog::table(Kind kind) const
{
    return _tables[static_cast<int>(kind)];
}

void ShortcutMapperDialog::refresh()
{
    _entries = _window.shortcutEntries();
    int rows[5] = {};
    for (QTableWidget* t : _tables)
        t->setRowCount(0);
    for (std::size_t i = 0; i < _entries.size(); ++i) {
        const ShortcutEntry& e = _entries[i];
        QTableWidget* t = table(e.kind);
        const int row = rows[static_cast<int>(e.kind)]++;
        t->insertRow(row);
        auto* name = new QTableWidgetItem(e.name);
        name->setData(Qt::UserRole, static_cast<qulonglong>(i));
        t->setItem(row, 0, name);
        t->setItem(row, 1, new QTableWidgetItem(keysText(e.keys)));
        t->setItem(row, 2, new QTableWidgetItem(e.category));
    }
    setFilter(_filter->text());
}

void ShortcutMapperDialog::setFilter(const QString& text)
{
    // El filtro de N++: por el nombre de la orden
    for (QTableWidget* t : _tables) {
        for (int row = 0; row < t->rowCount(); ++row)
            t->setRowHidden(row, !text.isEmpty() && !t->item(row, 0)->text().contains(text, Qt::CaseInsensitive));
    }
}

const ShortcutEntry* ShortcutMapperDialog::selected() const
{
    const QTableWidget* t = _tables[_tabs->currentIndex()];
    const int row = t->currentRow();
    if (row < 0 || !t->item(row, 0))
        return nullptr;
    return &_entries[t->item(row, 0)->data(Qt::UserRole).toULongLong()];
}

void ShortcutMapperDialog::modifySelected()
{
    const ShortcutEntry* entry = selected();
    if (!entry)
        return;
    KeyCaptureDialog capture(entry->name, entry->keys.empty() ? KeyCombo{} : entry->keys.front(), this);
    if (capture.exec() == QDialog::Accepted) {
        const ShortcutEntry copy = *entry;
        if (capture.combo().isEmpty())
            clear(copy.kind, copy.id);
        else
            assign(copy.kind, copy.id, capture.combo());
    }
}

bool ShortcutMapperDialog::assign(Kind kind, const QString& id, const KeyCombo& combo)
{
    if (combo.isEmpty())
        return false;
    // Otra orden con la misma tecla: N++ la marca como conflicto; aquí se ofrece quitársela
    for (const ShortcutEntry& other : _entries) {
        if ((other.kind == kind && other.id == id) || std::find(other.keys.begin(), other.keys.end(), combo) == other.keys.end())
            continue;
        const QString key = combo.toKeySequence().toString(QKeySequence::NativeText);
        if (!_prompter.confirm(this, tr("Shortcut conflict"),
                               tr("%1 is already used by \"%2\".\nAssign it here and remove it from there?").arg(key, other.name)))
            return false;
        std::vector<KeyCombo> rest = other.keys;
        rest.erase(std::remove(rest.begin(), rest.end(), combo), rest.end());
        _window.setShortcutKeys(other.kind, other.id, rest);
    }
    const bool ok = _window.setShortcutKeys(kind, id, {combo});
    refresh();
    return ok;
}

bool ShortcutMapperDialog::clear(Kind kind, const QString& id)
{
    const bool ok = _window.setShortcutKeys(kind, id, {});
    refresh();
    return ok;
}

} // namespace edita
