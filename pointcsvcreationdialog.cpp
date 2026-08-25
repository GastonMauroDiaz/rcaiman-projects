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

#include "pointcsvcreationdialog.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <memory>

namespace {

constexpr int BaseNameRole = Qt::UserRole + 1;
constexpr int CustomItemRole = Qt::UserRole + 2;

QListWidgetItem *makeBaseNameItem(const QString &baseName)
{
    auto *item = new QListWidgetItem(baseName);
    item->setData(BaseNameRole, baseName);
    item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
    return item;
}

QString cleanCustomBaseName(QString value, const QStringList &suffixes)
{
    value = value.trimmed();
    for (const QString &suffix : suffixes) {
        if (value.endsWith("." + suffix, Qt::CaseInsensitive)) {
            value = QFileInfo(value).completeBaseName();
            break;
        }
    }
    return value.trimmed();
}

bool hasPathSeparator(const QString &value)
{
    return value.contains('/') || value.contains('\\');
}

}

PointCsvCreationDialog::PointCsvCreationDialog(
    const QString &collectionName,
    const QStringList &availableBaseNames,
    QWidget *parent)
    : PointCsvCreationDialog(Options{}, collectionName, availableBaseNames,
                             parent)
{
}

PointCsvCreationDialog::PointCsvCreationDialog(
    const Options &options,
    const QString &collectionName,
    const QStringList &availableBaseNames,
    QWidget *parent)
    : QDialog(parent)
    , m_options(options)
{
    setWindowTitle(m_options.windowTitle);
    setMinimumWidth(620);
    resize(760, 460);

    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(m_options.description, this);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *listsLayout = new QHBoxLayout;
    auto *availableLayout = new QVBoxLayout;
    auto *selectedLayout = new QVBoxLayout;
    auto *buttonLayout = new QVBoxLayout;

    auto *availableLabel = new QLabel("Available", this);
    m_availableList = new QListWidget(this);
    m_availableList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_availableList->setDragEnabled(true);
    m_availableList->setAcceptDrops(true);
    m_availableList->setDropIndicatorShown(true);
    m_availableList->setDragDropMode(QAbstractItemView::DragDrop);
    m_availableList->setDefaultDropAction(Qt::MoveAction);
    availableLayout->addWidget(availableLabel);
    availableLayout->addWidget(m_availableList);

    auto *selectedLabel = new QLabel("To create", this);
    m_selectedList = new QListWidget(this);
    m_selectedList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_selectedList->setDragEnabled(true);
    m_selectedList->setAcceptDrops(true);
    m_selectedList->setDropIndicatorShown(true);
    m_selectedList->setDragDropMode(QAbstractItemView::DragDrop);
    m_selectedList->setDefaultDropAction(Qt::MoveAction);
    selectedLayout->addWidget(selectedLabel);
    selectedLayout->addWidget(m_selectedList);

    auto *addButton = new QPushButton("Add >", this);
    auto *removeButton = new QPushButton("< Remove", this);
    auto *addAllButton = new QPushButton("Add all", this);
    auto *clearButton = new QPushButton("Clear", this);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(removeButton);
    buttonLayout->addSpacing(12);
    buttonLayout->addWidget(addAllButton);
    buttonLayout->addWidget(clearButton);
    buttonLayout->addStretch(1);

    listsLayout->addLayout(availableLayout, 1);
    listsLayout->addLayout(buttonLayout);
    listsLayout->addLayout(selectedLayout, 1);
    layout->addLayout(listsLayout);

    m_summaryLabel = new QLabel(this);
    layout->addWidget(m_summaryLabel);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *createButton = m_buttons->addButton(
        "Create", QDialogButtonBox::AcceptRole);
    createButton->setEnabled(false);
    layout->addWidget(m_buttons);

    if (!collectionName.isEmpty())
        addAvailableBaseName(collectionName);
    if (m_options.allowCustomName)
        addCustomItem();
    for (const QString &baseName : availableBaseNames) {
        if (baseName.compare(collectionName, Qt::CaseInsensitive) != 0)
            addAvailableBaseName(baseName);
    }

    connect(addButton, &QPushButton::clicked,
            this, &PointCsvCreationDialog::addSelected);
    connect(removeButton, &QPushButton::clicked,
            this, &PointCsvCreationDialog::removeSelected);
    connect(addAllButton, &QPushButton::clicked,
            this, &PointCsvCreationDialog::addAll);
    connect(clearButton, &QPushButton::clicked,
            this, &PointCsvCreationDialog::clearSelected);
    connect(m_availableList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *) { addSelected(); });
    connect(m_selectedList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *) { removeSelected(); });
    connect(m_availableList->model(), &QAbstractItemModel::rowsRemoved,
            this, &PointCsvCreationDialog::updateSummary);
    connect(m_selectedList->model(), &QAbstractItemModel::rowsInserted,
            this, &PointCsvCreationDialog::updateSummary);
    connect(m_selectedList->model(), &QAbstractItemModel::rowsRemoved,
            this, &PointCsvCreationDialog::updateSummary);
    connect(m_buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    connect(createButton, &QAbstractButton::clicked,
            this, &QDialog::accept);

    updateSummary();
}

QStringList PointCsvCreationDialog::selectedBaseNames() const
{
    QStringList baseNames;
    for (int row = 0; row < m_selectedList->count(); ++row) {
        const QListWidgetItem *item = m_selectedList->item(row);
        const QString baseName = item->data(BaseNameRole).toString();
        if (!baseName.isEmpty())
            baseNames.append(baseName);
    }
    baseNames.removeDuplicates();
    return baseNames;
}

void PointCsvCreationDialog::addSelected()
{
    const QList<QListWidgetItem*> selected = m_availableList->selectedItems();
    for (QListWidgetItem *item : selected) {
        if (item->data(CustomItemRole).toBool()) {
            addCustomName();
            continue;
        }
        const int row = m_availableList->row(item);
        std::unique_ptr<QListWidgetItem> moved(m_availableList->takeItem(row));
        m_selectedList->addItem(moved.release());
    }
    updateSummary();
}

void PointCsvCreationDialog::removeSelected()
{
    const QList<QListWidgetItem*> selected = m_selectedList->selectedItems();
    for (QListWidgetItem *item : selected) {
        const int row = m_selectedList->row(item);
        std::unique_ptr<QListWidgetItem> moved(m_selectedList->takeItem(row));
        m_availableList->addItem(moved.release());
    }
    updateSummary();
}

void PointCsvCreationDialog::addAll()
{
    for (int row = m_availableList->count() - 1; row >= 0; --row) {
        QListWidgetItem *item = m_availableList->item(row);
        if (!item || item->data(CustomItemRole).toBool())
            continue;
        std::unique_ptr<QListWidgetItem> moved(m_availableList->takeItem(row));
        m_selectedList->insertItem(0, moved.release());
    }
    updateSummary();
}

void PointCsvCreationDialog::clearSelected()
{
    for (int row = m_selectedList->count() - 1; row >= 0; --row) {
        std::unique_ptr<QListWidgetItem> moved(m_selectedList->takeItem(row));
        m_availableList->addItem(moved.release());
    }
    updateSummary();
}

void PointCsvCreationDialog::addCustomName()
{
    bool ok = false;
    const QString baseName = cleanCustomBaseName(
        QInputDialog::getText(
        this,
        m_options.customWindowTitle,
        m_options.customPrompt,
        QLineEdit::Normal,
        QString(),
        &ok),
        m_options.customSuffixes);
    if (!ok || baseName.isEmpty())
        return;
    if (hasPathSeparator(baseName)) {
        QMessageBox::warning(
            this,
            m_options.customWindowTitle,
            "Enter a filename, not a path.");
        return;
    }
    if (!addBaseNameToSelected(baseName)) {
        QMessageBox::information(
            this,
            m_options.customWindowTitle,
            m_options.duplicateMessage);
    }
    updateSummary();
}

void PointCsvCreationDialog::updateSummary()
{
    const int count = selectedBaseNames().size();
    m_summaryLabel->setText(
        count == 1
            ? m_options.summarySingular
            : m_options.summaryPlural.arg(count));

    if (!m_buttons)
        return;
    const QList<QAbstractButton*> buttons = m_buttons->buttons();
    for (QAbstractButton *button : buttons) {
        if (m_buttons->buttonRole(button) == QDialogButtonBox::AcceptRole)
            button->setEnabled(count > 0);
    }
}

bool PointCsvCreationDialog::addBaseNameToSelected(const QString &baseName)
{
    const QString cleanBaseName =
        cleanCustomBaseName(baseName, m_options.customSuffixes);
    if (cleanBaseName.isEmpty() || selectedContains(cleanBaseName))
        return false;
    m_selectedList->addItem(makeBaseNameItem(cleanBaseName));
    return true;
}

bool PointCsvCreationDialog::selectedContains(const QString &baseName) const
{
    for (int row = 0; row < m_selectedList->count(); ++row) {
        const QListWidgetItem *item = m_selectedList->item(row);
        if (item->data(BaseNameRole).toString().compare(
                baseName, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void PointCsvCreationDialog::addAvailableBaseName(const QString &baseName)
{
    if (baseName.trimmed().isEmpty())
        return;
    for (int row = 0; row < m_availableList->count(); ++row) {
        const QListWidgetItem *item = m_availableList->item(row);
        if (item->data(BaseNameRole).toString().compare(
                baseName, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    m_availableList->addItem(makeBaseNameItem(baseName));
}

void PointCsvCreationDialog::addCustomItem()
{
    auto *item = new QListWidgetItem("Custom name...");
    item->setData(CustomItemRole, true);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_availableList->addItem(item);
}
