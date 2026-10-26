// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Prompter para tests: contesta con respuestas programadas y registra qué se
// preguntó. Si se pregunta algo sin respuesta programada, el test falla.

#pragma once

#include "ui/Prompter.h"

#include <QQueue>
#include <QStringList>
#include <QtTest>

namespace edita::test {

class FakePrompter : public Prompter
{
public:
    QQueue<SaveChoice> saveChoices;
    QQueue<bool> confirmations;
    QQueue<QStringList> openNames;
    QQueue<QString> saveNames;
    QQueue<UnmappableChoice> unmappableChoices;
    QQueue<std::pair<QString, QKeySequence>> macroNames; // nombre vacío = cancelar
    QQueue<QString> texts;                                // vacío = cancelar

    QStringList asked;      // registro de preguntas, en orden
    QStringList warnings;
    std::size_t lastUnmappableCount = 0;

    SaveChoice askSaveChanges(QWidget*, const QString& documentName, bool offerAll) override
    {
        asked << QStringLiteral("save:%1%2").arg(documentName, offerAll ? QStringLiteral(":all") : QString());
        if (saveChoices.isEmpty()) {
            QTest::qFail("askSaveChanges sin respuesta programada", __FILE__, __LINE__);
            return SaveChoice::Cancel;
        }
        return saveChoices.dequeue();
    }

    bool confirm(QWidget*, const QString& title, const QString&) override
    {
        asked << QStringLiteral("confirm:%1").arg(title);
        if (confirmations.isEmpty()) {
            QTest::qFail("confirm sin respuesta programada", __FILE__, __LINE__);
            return false;
        }
        return confirmations.dequeue();
    }

    void warn(QWidget*, const QString& title, const QString& text) override
    {
        warnings << QStringLiteral("%1: %2").arg(title, text);
    }

    QString lastFilters;
    QString lastSelectedFilter;

    QStringList openFileNames(QWidget*, const QString&, const QString& filters) override
    {
        asked << QStringLiteral("open");
        lastFilters = filters;
        return openNames.isEmpty() ? QStringList() : openNames.dequeue();
    }

    QString saveFileName(QWidget*, const QString& suggestedPath, const QString& filters,
                         const QString& selectedFilter) override
    {
        asked << QStringLiteral("saveAs:%1").arg(suggestedPath);
        lastFilters = filters;
        lastSelectedFilter = selectedFilter;
        return saveNames.isEmpty() ? QString() : saveNames.dequeue();
    }

    UnmappableChoice askUnmappable(QWidget*, const QString&, std::size_t count, const QString&) override
    {
        asked << QStringLiteral("unmappable");
        lastUnmappableCount = count;
        if (unmappableChoices.isEmpty()) {
            QTest::qFail("askUnmappable sin respuesta programada", __FILE__, __LINE__);
            return UnmappableChoice::Cancel;
        }
        return unmappableChoices.dequeue();
    }

    bool askMacroName(QWidget*, QString& name, QKeySequence& shortcut) override
    {
        asked << QStringLiteral("macroName");
        if (macroNames.isEmpty()) {
            QTest::qFail("askMacroName sin respuesta programada", __FILE__, __LINE__);
            return false;
        }
        const auto answer = macroNames.dequeue();
        name = answer.first;
        shortcut = answer.second;
        return !name.isEmpty();
    }

    bool askText(QWidget*, const QString& title, const QString&, QString& text) override
    {
        asked << title;
        if (texts.isEmpty()) {
            QTest::qFail("askText sin respuesta programada", __FILE__, __LINE__);
            return false;
        }
        text = texts.dequeue();
        return !text.isEmpty();
    }
};

} // namespace edita::test
