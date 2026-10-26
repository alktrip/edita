// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/ProblemsPanel.h"

#include "ui/FileIcons.h"

#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QPainter>
#include <QSet>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace edita {

namespace {

constexpr int kRolePath = Qt::UserRole;
constexpr int kRoleLine = Qt::UserRole + 1;
constexpr int kRoleCharacter = Qt::UserRole + 2;

} // namespace

ProblemsPanel::ProblemsPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("problemsPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    _tree = new QTreeWidget(this);
    _tree->setObjectName(QStringLiteral("problemsTree"));
    _tree->setColumnCount(2);
    _tree->setHeaderLabels({tr("Problem"), tr("Position")});
    _tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    _tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _tree->header()->setStretchLastSection(false);
    _tree->setUniformRowHeights(true);
    _tree->setRootIsDecorated(true);
    layout->addWidget(_tree);
    connect(_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        if (item && item->parent())
            emit activated(item->data(0, kRolePath).toString(), item->data(0, kRoleLine).toInt(), item->data(0, kRoleCharacter).toInt());
    });
}

QIcon ProblemsPanel::severityIcon(int severity)
{
    // Un círculo rojo con ×, un triángulo amarillo con ! o un círculo azul con i
    static const QColor kColors[] = {QColor(0xE5, 0x14, 0x00), QColor(0xE5, 0x14, 0x00), QColor(0xD8, 0x9B, 0x00),
                                     QColor(0x1A, 0x85, 0xFF), QColor(0x80, 0x80, 0x80)};
    const int s = std::clamp(severity, 1, 4);
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(kColors[s]);
    if (s == 2) {
        const QPointF triangle[] = {QPointF(16, 2), QPointF(30, 29), QPointF(2, 29)};
        p.drawPolygon(triangle, 3);
    } else {
        p.drawEllipse(QRectF(2, 2, 28, 28));
    }
    QPen pen(Qt::white, 3.5, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    if (s == 1) {
        p.drawLine(QPointF(11, 11), QPointF(21, 21));
        p.drawLine(QPointF(21, 11), QPointF(11, 21));
    } else if (s == 2) {
        p.drawLine(QPointF(16, 11), QPointF(16, 19));
        p.drawPoint(QPointF(16, 25));
    } else {
        p.drawPoint(QPointF(16, 9));
        p.drawLine(QPointF(16, 14), QPointF(16, 23));
    }
    return QIcon(pixmap);
}

void ProblemsPanel::setDiagnostics(const QMap<QString, std::vector<LspManager::Diagnostic>>& diagnostics)
{
    // Se conservan los archivos plegados
    QSet<QString> collapsed;
    for (int i = 0; i < _tree->topLevelItemCount(); ++i) {
        if (!_tree->topLevelItem(i)->isExpanded())
            collapsed.insert(_tree->topLevelItem(i)->data(0, kRolePath).toString());
    }
    _tree->clear();
    for (auto it = diagnostics.begin(); it != diagnostics.end(); ++it) {
        const QString& path = it.key();
        auto* file = new QTreeWidgetItem(_tree);
        file->setText(0, QStringLiteral("%1  %2").arg(QFileInfo(path).fileName(), QDir::toNativeSeparators(QFileInfo(path).absolutePath())));
        file->setText(1, QString::number(it.value().size()));
        file->setIcon(0, fileicons::forFile(path));
        file->setData(0, kRolePath, path);
        file->setToolTip(0, path);
        for (const LspManager::Diagnostic& d : it.value()) {
            auto* item = new QTreeWidgetItem(file);
            QString text = d.message.section(QLatin1Char('\n'), 0, 0);
            QString source = d.source;
            if (!d.code.isEmpty())
                source += QStringLiteral("(%1)").arg(d.code);
            if (!source.isEmpty())
                text += QStringLiteral("  ") + source;
            item->setText(0, text);
            item->setToolTip(0, d.message);
            item->setIcon(0, severityIcon(d.severity));
            item->setText(1, tr("Ln %1, Col %2").arg(d.line + 1).arg(d.character + 1));
            item->setData(0, kRolePath, path);
            item->setData(0, kRoleLine, d.line);
            item->setData(0, kRoleCharacter, d.character);
        }
        file->setExpanded(!collapsed.contains(path));
    }
}

} // namespace edita
