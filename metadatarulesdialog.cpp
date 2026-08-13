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

#include "metadatarulesdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

MetadataRulesDialog::MetadataRulesDialog(
    const QList<MetadataRuleAnalysis> &analysis,
    QWidget *parent)
    : QDialog(parent)
    , m_analysis(analysis)
{
    setWindowTitle("Build Metadata Rules");
    resize(1180, 680);
    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(
        "Select the metadata items that identify this calibration.", this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *toolsLayout = new QHBoxLayout;
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter tags...");
    toolsLayout->addWidget(m_filterEdit, 1);
    m_filterCountLabel = new QLabel(this);
    toolsLayout->addWidget(m_filterCountLabel);
    auto *moveTopButton = new QPushButton("Move to Top", this);
    auto *moveBottomButton = new QPushButton("Move to Bottom", this);
    toolsLayout->addWidget(moveTopButton);
    toolsLayout->addWidget(moveBottomButton);
    layout->addLayout(toolsLayout);

    m_table = new QTableWidget(analysis.size(), 9, this);
    m_table->setHorizontalHeaderLabels({
        "Use", "Group", "Tag", "Distinct values", "Presence",
        "Constant", "Rule ID", "Label", "Expected"
    });
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    for (int row = 0; row < analysis.size(); ++row) {
        const MetadataRuleAnalysis &entry = analysis[row];
        auto *use = new QCheckBox(m_table);
        m_table->setCellWidget(row, 0, use);
        m_table->setItem(row, 1, new QTableWidgetItem(entry.group));
        m_table->setItem(row, 2, new QTableWidgetItem(entry.tag));
        m_table->setItem(row, 3, new QTableWidgetItem(entry.values.join(" | ")));
        m_table->setItem(
            row, 4,
            new QTableWidgetItem(
                QString("%1/%2").arg(entry.presence).arg(entry.sourceCount)));
        m_table->setItem(
            row, 5,
            new QTableWidgetItem(entry.values.size() == 1 ? "Yes" : "No"));
        m_table->setItem(
            row, 6,
            new QTableWidgetItem(MetadataRegistry::snakeCase(entry.tag)));
        m_table->setItem(row, 7, new QTableWidgetItem(entry.tag));
        m_table->setItem(
            row, 8,
            new QTableWidgetItem(entry.values.size() == 1
                                     ? entry.values.first()
                                     : QString()));
    }
    m_table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::Stretch);
    layout->addWidget(m_table);
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &MetadataRulesDialog::applyFilter);
    connect(moveTopButton, &QPushButton::clicked,
            this, &MetadataRulesDialog::moveSelectedRowsToTop);
    connect(moveBottomButton, &QPushButton::clicked,
            this, &MetadataRulesDialog::moveSelectedRowsToBottom);
    updateFilterCount();

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &MetadataRulesDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &MetadataRulesDialog::reject);
    layout->addWidget(buttons);
}

QList<MetadataRule> MetadataRulesDialog::selectedRules() const
{
    QList<MetadataRule> rules;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto *use =
            qobject_cast<QCheckBox *>(m_table->cellWidget(row, 0));
        if (!use || !use->isChecked())
            continue;
        rules.append({
            m_table->item(row, 6)->text().trimmed(),
            m_table->item(row, 1)->text(),
            m_table->item(row, 2)->text(),
            m_table->item(row, 7)->text().trimmed(),
            m_table->item(row, 8)->text().trimmed()
        });
    }
    return rules;
}

void MetadataRulesDialog::accept()
{
    const QList<MetadataRule> rules = selectedRules();
    if (rules.isEmpty()) {
        QMessageBox::warning(
            this, "Metadata Rules", "Select at least one metadata item.");
        return;
    }

    QSet<QString> ids;
    QSet<QString> keys;
    const QRegularExpression idPattern("^[a-z][a-z0-9_]*$");
    for (int i = 0; i < rules.size(); ++i) {
        const MetadataRule &rule = rules[i];
        if (!idPattern.match(rule.id).hasMatch() || ids.contains(rule.id)) {
            QMessageBox::warning(
                this, "Metadata Rules",
                "Rule IDs must be unique snake_case identifiers.");
            return;
        }
        if (keys.contains(rule.key())) {
            QMessageBox::warning(
                this, "Metadata Rules",
                "The same group and tag cannot be selected twice.");
            return;
        }
        if (rule.label.isEmpty() || rule.expected.isEmpty()) {
            QMessageBox::warning(
                this, "Metadata Rules",
                "Every selected rule needs a label and expected value.");
            return;
        }

        const int sourceRow = [&]() {
            for (int row = 0; row < m_analysis.size(); ++row) {
                if (m_analysis[row].group == rule.group
                    && m_analysis[row].tag == rule.tag)
                    return row;
            }
            return -1;
        }();
        if (sourceRow >= 0 && m_analysis[sourceRow].values.size() > 1
            && !m_analysis[sourceRow].values.contains(rule.expected)) {
            QMessageBox::warning(
                this, "Metadata Rules",
                QString("Choose an observed expected value for '%1'.")
                    .arg(rule.id));
            return;
        }
        ids.insert(rule.id);
        keys.insert(rule.key());
    }
    QDialog::accept();
}

MetadataRulesDialog::RowData MetadataRulesDialog::rowData(int row) const
{
    RowData data;
    const auto *use = qobject_cast<QCheckBox *>(m_table->cellWidget(row, 0));
    data.use = use && use->isChecked();
    for (int column = 1; column < m_table->columnCount(); ++column) {
        const auto *item = m_table->item(row, column);
        data.cells.append(item ? item->text() : QString());
    }
    return data;
}

void MetadataRulesDialog::setRowData(int row, const RowData &data)
{
    auto *use = new QCheckBox(m_table);
    use->setChecked(data.use);
    m_table->setCellWidget(row, 0, use);
    for (int column = 1; column < m_table->columnCount(); ++column) {
        const int cellIndex = column - 1;
        m_table->setItem(
            row, column,
            new QTableWidgetItem(
                cellIndex < data.cells.size() ? data.cells[cellIndex]
                                              : QString()));
    }
}

QList<int> MetadataRulesDialog::selectedTableRows() const
{
    QSet<int> seen;
    QList<int> rows;
    const auto selected = m_table->selectionModel()->selectedRows();
    for (const QModelIndex &index : selected) {
        if (index.column() != 0 || seen.contains(index.row()))
            continue;
        seen.insert(index.row());
        rows.append(index.row());
    }
    std::sort(rows.begin(), rows.end());
    return rows;
}

void MetadataRulesDialog::moveSelectedRows(bool toTop)
{
    const QList<int> rows = selectedTableRows();
    if (rows.isEmpty())
        return;

    const int verticalScroll = m_table->verticalScrollBar()->value();
    const int horizontalScroll = m_table->horizontalScrollBar()->value();

    QList<RowData> movedRows;
    for (int row : rows)
        movedRows.append(rowData(row));

    QList<RowData> remainingRows;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (!rows.contains(row))
            remainingRows.append(rowData(row));
    }

    QList<RowData> reorderedRows;
    if (toTop) {
        reorderedRows = movedRows;
        reorderedRows.append(remainingRows);
    } else {
        reorderedRows = remainingRows;
        reorderedRows.append(movedRows);
    }

    m_table->clearContents();
    m_table->setRowCount(reorderedRows.size());
    for (int row = 0; row < reorderedRows.size(); ++row)
        setRowData(row, reorderedRows[row]);

    m_table->clearSelection();
    const int firstMovedRow = toTop ? 0 : remainingRows.size();
    for (int offset = 0; offset < movedRows.size(); ++offset)
        m_table->selectRow(firstMovedRow + offset);
    applyFilter();
    m_table->verticalScrollBar()->setValue(verticalScroll);
    m_table->horizontalScrollBar()->setValue(horizontalScroll);
}

void MetadataRulesDialog::moveSelectedRowsToTop()
{
    moveSelectedRows(true);
}

void MetadataRulesDialog::moveSelectedRowsToBottom()
{
    moveSelectedRows(false);
}

void MetadataRulesDialog::applyFilter()
{
    const QString needle = m_filterEdit->text().trimmed();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        m_table->setRowHidden(
            row,
            !needle.isEmpty() && !rowMatchesFilter(row, needle));
    }
    updateFilterCount();
}

void MetadataRulesDialog::updateFilterCount()
{
    int visible = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (!m_table->isRowHidden(row))
            ++visible;
    }
    if (!m_filterCountLabel)
        return;
    if (visible == m_table->rowCount())
        m_filterCountLabel->setText(QString("%1 tags").arg(visible));
    else
        m_filterCountLabel->setText(
            QString("%1/%2 tags").arg(visible).arg(m_table->rowCount()));
}

bool MetadataRulesDialog::rowMatchesFilter(
    int row, const QString &needle) const
{
    for (int column = 1; column < m_table->columnCount(); ++column) {
        const auto *item = m_table->item(row, column);
        if (item && item->text().contains(needle, Qt::CaseInsensitive))
            return true;
    }
    return false;
}
