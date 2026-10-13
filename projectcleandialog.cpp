// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// This file is part of rcaiman-projects.
//
// rcaiman-projects is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// rcaiman-projects is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with rcaiman-projects. If not, see <https://www.gnu.org/licenses/>.
//
// Part of the rcaiman suite.

#include "projectcleandialog.h"
#include "collectionappearance.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString fileCountText(int count)
{
    return count == 1
        ? QStringLiteral("1 file")
        : QStringLiteral("%1 files").arg(count);
}

}

ProjectCleanDialog::ProjectCleanDialog(
    const ProjectContentCleanPlan &plan,
    QWidget *parent)
    : QDialog(parent)
    , m_plan(plan)
{
    setWindowTitle("Clean Project");
    setMinimumWidth(680);
    resize(760, 460);

    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(
        "Clean removes working files from selected folders. "
        "Collection descriptions and styles are preserved.",
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_table = new QTableWidget(m_plan.items.size(), 4, this);
    m_table->setHorizontalHeaderLabels(
        {"Clean", "Folder", "Kind", "Files"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setIconSize(QSize(14, 14));
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::Stretch);

    QStringList knownKinds = CollectionAppearance::classNames();
    knownKinds.append({"RAW files", "Sensor data", "Kind"});
    const QFontMetrics metrics(m_table->font());
    int kindTextWidth = 0;
    for (const QString &kind : knownKinds)
        kindTextWidth = qMax(kindTextWidth, metrics.horizontalAdvance(kind));
    const int markerAndMargins = m_table->iconSize().width() + 30;
    m_table->horizontalHeader()->resizeSection(
        2, kindTextWidth + markerAndMargins);

    for (int row = 0; row < m_plan.items.size(); ++row) {
        const ProjectContentCleanItem &item = m_plan.items[row];
        const bool hasFiles = !item.relativeFiles.isEmpty();

        auto *checkItem = new QTableWidgetItem;
        checkItem->setData(Qt::UserRole, row);
        checkItem->setCheckState(Qt::Unchecked);
        checkItem->setFlags(hasFiles
            ? Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable
            : Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        m_table->setItem(row, 0, checkItem);

        auto *folderItem = new QTableWidgetItem(item.label);
        auto *kindItem = new QTableWidgetItem(item.kind);
        kindItem->setIcon(CollectionAppearance::markerIcon(item.kind));
        auto *filesItem =
            new QTableWidgetItem(fileCountText(item.relativeFiles.size()));
        if (!hasFiles) {
            folderItem->setFlags(folderItem->flags() & ~Qt::ItemIsEnabled);
            kindItem->setFlags(kindItem->flags() & ~Qt::ItemIsEnabled);
            filesItem->setFlags(filesItem->flags() & ~Qt::ItemIsEnabled);
        }
        m_table->setItem(row, 1, folderItem);
        m_table->setItem(row, 2, kindItem);
        m_table->setItem(row, 3, filesItem);
    }
    layout->addWidget(m_table);

    m_summaryLabel = new QLabel(this);
    layout->addWidget(m_summaryLabel);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *cleanButton = m_buttons->addButton(
        "Clean", QDialogButtonBox::AcceptRole);
    cleanButton->setEnabled(false);
    connect(cleanButton, &QAbstractButton::clicked,
            this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    layout->addWidget(m_buttons);

    connect(m_table, &QTableWidget::itemChanged,
            this, &ProjectCleanDialog::updateSummary);
    updateSummary();
}

ProjectContentCleanPlan ProjectCleanDialog::selectedPlan() const
{
    ProjectContentCleanPlan selected = m_plan;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto *item = m_table->item(row, 0);
        if (!item)
            continue;
        const int planIndex = item->data(Qt::UserRole).toInt();
        if (planIndex < 0 || planIndex >= selected.items.size())
            continue;
        selected.items[planIndex].selected =
            item->flags().testFlag(Qt::ItemIsEnabled)
            && item->checkState() == Qt::Checked;
    }
    return selected;
}

void ProjectCleanDialog::updateSummary()
{
    const ProjectContentCleanPlan selected = selectedPlan();
    const int selectedFiles = selected.totalSelectedFiles();
    const int selectedFolders = selected.totalSelectedFolders();

    m_summaryLabel->setText(
        QStringLiteral("%1 will be removed from %2 folder(s).")
            .arg(fileCountText(selectedFiles))
            .arg(selectedFolders));

    if (!m_buttons)
        return;
    const QList<QAbstractButton*> buttons = m_buttons->buttons();
    for (QAbstractButton *button : buttons) {
        if (m_buttons->buttonRole(button) == QDialogButtonBox::AcceptRole)
            button->setEnabled(selectedFiles > 0);
    }
}
