// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/HashDialog.h"

#include "ui/Prompter.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace edita {

HashDialog::HashDialog(Mode mode, QCryptographicHash::Algorithm algorithm, Prompter& prompter, QWidget* parent)
    : QDialog(parent)
    , _mode(mode)
    , _algorithm(algorithm)
    , _prompter(prompter)
{
    setObjectName(QStringLiteral("hashDialog"));
    const QString name = algorithmName(algorithm);
    setWindowTitle(mode == Mode::Text ? tr("Generate %1 digest").arg(name) : tr("Generate %1 digest from files").arg(name));
    auto* layout = new QVBoxLayout(this);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (mode == Mode::Text) {
        _eachLine = new QCheckBox(tr("Treat each line as a &separate string"), this);
        _eachLine->setObjectName(QStringLiteral("hashEachLine"));
        layout->addWidget(_eachLine);
        connect(_eachLine, &QCheckBox::toggled, this, &HashDialog::update);
    } else {
        auto* choose = new QPushButton(tr("Choose files to &generate %1…").arg(name), this);
        choose->setObjectName(QStringLiteral("hashChooseFiles"));
        auto* row = new QHBoxLayout;
        row->addWidget(choose);
        row->addStretch(1);
        layout->addLayout(row);
        connect(choose, &QPushButton::clicked, this, [this] {
            const QStringList chosen = _prompter.openFileNames(this, QDir::homePath(), tr("All types (*)"));
            if (!chosen.isEmpty())
                setFiles(chosen);
        });
    }
    _input = new QPlainTextEdit(this);
    _input->setObjectName(QStringLiteral("hashInput"));
    _input->setFont(mono);
    _input->setReadOnly(mode == Mode::Files);
    _input->setLineWrapMode(QPlainTextEdit::NoWrap);
    if (mode == Mode::Text)
        _input->setPlaceholderText(tr("Type or paste the text"));
    layout->addWidget(_input, 1);
    _output = new QPlainTextEdit(this);
    _output->setObjectName(QStringLiteral("hashOutput"));
    _output->setFont(mono);
    _output->setReadOnly(true);
    _output->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(_output, 1);
    connect(_input, &QPlainTextEdit::textChanged, this, &HashDialog::update);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton* copy = buttons->addButton(tr("Co&py to Clipboard"), QDialogButtonBox::ActionRole);
    copy->setObjectName(QStringLiteral("hashCopy"));
    connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(result()); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    resize(640, 420);
}

QString HashDialog::algorithmName(QCryptographicHash::Algorithm algorithm)
{
    switch (algorithm) {
    case QCryptographicHash::Md5: return QStringLiteral("MD5");
    case QCryptographicHash::Sha1: return QStringLiteral("SHA-1");
    case QCryptographicHash::Sha256: return QStringLiteral("SHA-256");
    case QCryptographicHash::Sha512: return QStringLiteral("SHA-512");
    default: return QStringLiteral("?");
    }
}

QString HashDialog::digest(const QByteArray& data, QCryptographicHash::Algorithm algorithm)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, algorithm).toHex());
}

QString HashDialog::digestText(const QString& text, QCryptographicHash::Algorithm algorithm, bool eachLine)
{
    if (text.isEmpty())
        return {};
    if (!eachLine)
        return digest(text.toUtf8(), algorithm);
    QStringList lines;
    for (const QString& line : text.split(QLatin1Char('\n')))
        lines << (line.isEmpty() ? QString() : digest(line.toUtf8(), algorithm));
    return lines.join(QLatin1Char('\n'));
}

QString HashDialog::digestFiles(const QStringList& paths, QCryptographicHash::Algorithm algorithm)
{
    QStringList lines;
    for (const QString& path : paths) {
        QFile file(path);
        QCryptographicHash hash(algorithm);
        if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file))
            lines << tr("%1: cannot be read").arg(path);
        else
            lines << QStringLiteral("%1  %2").arg(QString::fromLatin1(hash.result().toHex()), path);
    }
    return lines.join(QLatin1Char('\n'));
}

void HashDialog::setInputText(const QString& text)
{
    _input->setPlainText(text);
}

void HashDialog::setEachLine(bool on)
{
    if (_eachLine)
        _eachLine->setChecked(on);
}

void HashDialog::setFiles(const QStringList& paths)
{
    _files = paths;
    _input->setPlainText(paths.join(QLatin1Char('\n')));
    update();
}

QString HashDialog::result() const
{
    return _output->toPlainText();
}

void HashDialog::update()
{
    _output->setPlainText(_mode == Mode::Text ? digestText(_input->toPlainText(), _algorithm, _eachLine && _eachLine->isChecked())
                                              : digestFiles(_files, _algorithm));
}

} // namespace edita
