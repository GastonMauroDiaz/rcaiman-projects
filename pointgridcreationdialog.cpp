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

#include "pointgridcreationdialog.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QItemSelectionModel>

#include <algorithm>

namespace {
constexpr int CandidateIdRole = Qt::UserRole + 1;
constexpr int StrideRole = Qt::UserRole + 2;

QString normalizedPath(const QString &path)
{
    QString normalized = QFileInfo(path).absoluteFilePath();
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

QString outputNameForMask(const QString &maskPath)
{
    return QFileInfo(maskPath).completeBaseName() + QStringLiteral(".csv");
}

QString normalizedOutputName(QString name)
{
    name = name.trimmed();
    if (!name.isEmpty()
        && !name.contains('/')
        && !name.contains('\\')
        && QFileInfo(name).suffix().isEmpty()) {
        name += QStringLiteral(".csv");
    }
    return name;
}

QString outputNameError(const QString &name)
{
    if (name.isEmpty())
        return QStringLiteral("Output filename cannot be empty.");
    if (name.contains('/') || name.contains('\\'))
        return QStringLiteral("Output must be a filename, not a path.");
    if (!name.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
        return QStringLiteral("Output filename must end in .csv.");

    const QString baseName = name.left(name.size() - 4).trimmed();
    if (baseName.isEmpty())
        return QStringLiteral("Output filename needs a basename before .csv.");
    if (baseName.contains('<') || baseName.contains('>')
        || baseName.contains(':') || baseName.contains('"')
        || baseName.contains('|') || baseName.contains('?')
        || baseName.contains('*')) {
        return QStringLiteral("Output filename contains an invalid character.");
    }
    if (baseName.endsWith('.') || baseName.endsWith(' '))
        return QStringLiteral("Output filename cannot end with a dot or space.");
    return {};
}

bool prefixIsValid(const QString &prefix)
{
    if (prefix.trimmed().isEmpty())
        return false;
    return !prefix.contains('/') && !prefix.contains('\\')
        && !prefix.contains('<') && !prefix.contains('>')
        && !prefix.contains(':') && !prefix.contains('"')
        && !prefix.contains('|') && !prefix.contains('?')
        && !prefix.contains('*');
}
}

PointGridCreationDialog::PointGridCreationDialog(
    const QVector<Candidate> &candidates,
    int preferredCandidateId,
    QWidget *parent)
    : QDialog(parent)
    , m_candidates(candidates)
{
    setWindowTitle("Create grid");
    setMinimumWidth(720);
    resize(820, 460);

    auto *layout = new QVBoxLayout(this);

    auto *description = new QLabel(
        "Create one or more point CSV grids from binary masks. "
        "Each row will be saved in its selected point collection.",
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *topLayout = new QHBoxLayout;
    auto *form = new QFormLayout;
    m_maskCombo = new QComboBox(this);
    m_pointCombo = new QComboBox(this);
    m_strideSpin = new QSpinBox(this);
    m_strideSpin->setRange(1, 10000);
    m_strideSpin->setValue(1);
    form->addRow("Mask:", m_maskCombo);
    form->addRow("Point collection:", m_pointCombo);
    form->addRow("Stride:", m_strideSpin);
    topLayout->addLayout(form, 1);

    m_addButton = new QPushButton("Add", this);
    m_addAllButton = new QPushButton("Add all", this);
    m_addButton->setMinimumWidth(96);
    m_addAllButton->setMinimumWidth(96);
    topLayout->addWidget(m_addButton, 0, Qt::AlignBottom);
    topLayout->addWidget(m_addAllButton, 0, Qt::AlignBottom);
    layout->addLayout(topLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(
        {"Mask", "Point collection", "Stride", "Output"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(
        QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    layout->addWidget(m_table, 1);

    auto *tableButtonsLayout = new QHBoxLayout;
    m_removeButton = new QPushButton("Remove row", this);
    auto *clearButton = new QPushButton("Clear", this);
    auto *prefixLabel = new QLabel("Prefix:", this);
    m_prefixEdit = new QLineEdit("grid_", this);
    m_prefixEdit->setMaximumWidth(120);
    m_prefixButton = new QPushButton("Apply to all", this);
    tableButtonsLayout->addWidget(m_removeButton);
    tableButtonsLayout->addWidget(clearButton);
    tableButtonsLayout->addStretch(1);
    tableButtonsLayout->addWidget(prefixLabel);
    tableButtonsLayout->addWidget(m_prefixEdit);
    tableButtonsLayout->addWidget(m_prefixButton);
    layout->addLayout(tableButtonsLayout);

    m_summaryLabel = new QLabel(this);
    layout->addWidget(m_summaryLabel);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *createButton = m_buttons->addButton(
        "Create", QDialogButtonBox::AcceptRole);
    createButton->setEnabled(false);
    layout->addWidget(m_buttons);

    populateMaskCombo(preferredCandidateId);
    populatePointCombo();

    connect(m_maskCombo,
            static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { populatePointCombo(); });
    connect(m_pointCombo,
            static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { updateButtons(); });
    connect(m_addButton, &QPushButton::clicked,
            this, &PointGridCreationDialog::addTask);
    connect(m_addAllButton, &QPushButton::clicked,
            this, &PointGridCreationDialog::addAllTasks);
    connect(m_removeButton, &QPushButton::clicked,
            this, &PointGridCreationDialog::removeSelectedTasks);
    connect(m_prefixButton, &QPushButton::clicked,
            this, &PointGridCreationDialog::prefixOutputs);
    connect(m_prefixEdit, &QLineEdit::textChanged,
            this, [this]() { updateButtons(); });
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        m_table->setRowCount(0);
        updateButtons();
    });
    connect(m_table->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            [this]() { updateButtons(); });
    connect(m_table, &QTableWidget::itemChanged,
            this, [this](QTableWidgetItem *item) {
                if (!item || item->column() != 3)
                    return;
                const QString normalized =
                    normalizedOutputName(item->text());
                if (normalized != item->text()) {
                    const QSignalBlocker blocker(m_table);
                    item->setText(normalized);
                }
                updateButtons();
            });
    connect(m_buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    connect(createButton, &QAbstractButton::clicked,
            this, &QDialog::accept);

    if (preferredCandidateId >= 0)
        addTask();
    updateButtons();
}

void PointGridCreationDialog::prefixOutputs()
{
    const QString prefix = m_prefixEdit->text().trimmed();
    if (!prefixIsValid(prefix))
        return;

    const QSignalBlocker blocker(m_table);
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *outputItem = m_table->item(row, 3);
        if (!outputItem)
            continue;

        const QString outputName =
            normalizedOutputName(outputItem->text());
        if (!outputName.startsWith(prefix, Qt::CaseInsensitive))
            outputItem->setText(prefix + outputName);
    }
    updateButtons();
}

QVector<PointGridCreationDialog::Task> PointGridCreationDialog::tasks() const
{
    QVector<Task> result;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *item = m_table->item(row, 0);
        const QTableWidgetItem *outputItem = m_table->item(row, 3);
        if (!item || !outputItem)
            continue;
        result.append(Task{
            item->data(CandidateIdRole).toInt(),
            item->data(StrideRole).toInt(),
            normalizedOutputName(outputItem->text())
        });
    }
    return result;
}

void PointGridCreationDialog::populateMaskCombo(int preferredCandidateId)
{
    m_maskCombo->clear();

    QString preferredPath;
    if (preferredCandidateId >= 0)
        preferredPath = normalizedPath(candidateById(preferredCandidateId).maskPath);

    int preferredIndex = -1;
    QStringList seenPaths;
    for (const Candidate &candidate : std::as_const(m_candidates)) {
        const QString pathKey = normalizedPath(candidate.maskPath);
        if (pathKey.isEmpty() || seenPaths.contains(pathKey))
            continue;
        seenPaths.append(pathKey);
        m_maskCombo->addItem(candidate.maskLabel, candidate.maskPath);
        if (preferredIndex < 0 && pathKey == preferredPath)
            preferredIndex = m_maskCombo->count() - 1;
    }

    if (m_maskCombo->count() > 0)
        m_maskCombo->setCurrentIndex(qMax(0, preferredIndex));
}

void PointGridCreationDialog::populatePointCombo()
{
    m_pointCombo->clear();

    const QString maskPath = m_maskCombo->currentData().toString();
    const QString maskKey = normalizedPath(maskPath);
    QStringList seenCollections;

    for (const Candidate &candidate : std::as_const(m_candidates)) {
        if (normalizedPath(candidate.maskPath) != maskKey)
            continue;

        const QString collectionKey = normalizedPath(candidate.pointCollectionPath);
        if (collectionKey.isEmpty() || seenCollections.contains(collectionKey))
            continue;

        seenCollections.append(collectionKey);
        m_pointCombo->addItem(candidate.pointCollectionLabel, candidate.id);
    }
    updateButtons();
}

void PointGridCreationDialog::addTask()
{
    const int candidateId = m_pointCombo->currentData().toInt();
    if (candidateId < 0 || taskExists(candidateId))
        return;

    const Candidate candidate = candidateById(candidateId);
    if (candidate.id < 0)
        return;

    addTaskForCandidate(candidate, m_strideSpin->value());
}

void PointGridCreationDialog::addAllTasks()
{
    const int stride = m_strideSpin->value();
    for (const Candidate &candidate : std::as_const(m_candidates)) {
        if (candidate.id >= 0 && !taskExists(candidate.id))
            addTaskForCandidate(candidate, stride);
    }
}

void PointGridCreationDialog::addTaskForCandidate(
    const Candidate &candidate,
    int stride)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto *maskItem = new QTableWidgetItem(candidate.maskLabel);
    maskItem->setData(CandidateIdRole, candidate.id);
    maskItem->setData(StrideRole, stride);
    maskItem->setFlags(maskItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, 0, maskItem);
    auto *collectionItem =
        new QTableWidgetItem(candidate.pointCollectionLabel);
    collectionItem->setFlags(
        collectionItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, 1, collectionItem);
    auto *strideItem = new QTableWidgetItem(QString::number(stride));
    strideItem->setFlags(strideItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, 2, strideItem);
    m_table->setItem(
        row, 3,
        new QTableWidgetItem(outputNameForMask(candidate.maskPath)));
    m_table->resizeColumnsToContents();
    updateButtons();
}

void PointGridCreationDialog::removeSelectedTasks()
{
    QVector<int> rows;
    const QModelIndexList indexes =
        m_table->selectionModel()->selectedRows();
    rows.reserve(indexes.size());
    for (const QModelIndex &index : indexes)
        rows.append(index.row());
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

    for (int i = rows.size() - 1; i >= 0; --i)
        m_table->removeRow(rows.at(i));
    updateButtons();
}

void PointGridCreationDialog::updateButtons()
{
    const int count = m_table->rowCount();
    const QString validationError = outputValidationError();
    if (m_summaryLabel) {
        if (!validationError.isEmpty()) {
            m_summaryLabel->setText(validationError);
            m_summaryLabel->setStyleSheet("color: #9f2f2a;");
        } else {
            m_summaryLabel->setText(
                count == 1
                    ? QStringLiteral("1 grid CSV file will be created.")
                    : QStringLiteral("%1 grid CSV files will be created.")
                          .arg(count));
            m_summaryLabel->setStyleSheet(QString());
        }
    }

    if (m_removeButton)
        m_removeButton->setEnabled(!m_table->selectionModel()
                                        ->selectedRows().isEmpty());

    const int currentCandidateId = m_pointCombo->currentData().toInt();
    const bool canAdd = currentCandidateId >= 0
        && !taskExists(currentCandidateId);
    if (m_addButton)
        m_addButton->setEnabled(canAdd);

    bool canAddAll = false;
    for (const Candidate &candidate : std::as_const(m_candidates)) {
        if (candidate.id >= 0 && !taskExists(candidate.id)) {
            canAddAll = true;
            break;
        }
    }
    if (m_addAllButton)
        m_addAllButton->setEnabled(canAddAll);

    if (m_prefixButton && m_prefixEdit) {
        const QString prefix = m_prefixEdit->text().trimmed();
        bool canPrefix = false;
        if (prefixIsValid(prefix)) {
            for (int row = 0; row < m_table->rowCount(); ++row) {
                const QTableWidgetItem *outputItem = m_table->item(row, 3);
                if (outputItem
                    && !outputItem->text().trimmed().startsWith(
                        prefix, Qt::CaseInsensitive)) {
                    canPrefix = true;
                    break;
                }
            }
        }
        m_prefixButton->setEnabled(canPrefix);
    }

    if (m_buttons) {
        const QList<QAbstractButton*> buttons = m_buttons->buttons();
        for (QAbstractButton *button : buttons) {
            if (m_buttons->buttonRole(button) == QDialogButtonBox::AcceptRole)
                button->setEnabled(count > 0 && validationError.isEmpty());
        }
    }
}

QString PointGridCreationDialog::outputValidationError() const
{
    QSet<QString> outputPaths;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *candidateItem = m_table->item(row, 0);
        const QTableWidgetItem *outputItem = m_table->item(row, 3);
        if (!candidateItem || !outputItem)
            return QStringLiteral("Every row needs an output filename.");

        const QString outputName = normalizedOutputName(outputItem->text());
        const QString error = outputNameError(outputName);
        if (!error.isEmpty())
            return error;

        const Candidate candidate = candidateById(
            candidateItem->data(CandidateIdRole).toInt());
        const QString outputPath = normalizedPath(
            QDir(candidate.pointCollectionPath).filePath(outputName));
        if (outputPaths.contains(outputPath)) {
            return QStringLiteral(
                "Two rows would create the same output file.");
        }
        outputPaths.insert(outputPath);
    }
    return {};
}

bool PointGridCreationDialog::taskExists(int candidateId) const
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *item = m_table->item(row, 0);
        if (item && item->data(CandidateIdRole).toInt() == candidateId)
            return true;
    }
    return false;
}

PointGridCreationDialog::Candidate
PointGridCreationDialog::candidateById(int candidateId) const
{
    for (const Candidate &candidate : std::as_const(m_candidates)) {
        if (candidate.id == candidateId)
            return candidate;
    }
    return {};
}
