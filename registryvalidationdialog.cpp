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

#include "registryvalidationdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

RegistryValidationDialog::RegistryValidationDialog(
    const QList<MetadataRule> &rules,
    const QList<MetadataObservation> &observations,
    QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Validate Image Metadata");
    resize(1120, 620);
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(
        "Exact matches require no action. Similar items and different values "
        "must be accepted explicitly.", this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_table = new QTableWidget(rules.size(), 6, this);
    m_table->setHorizontalHeaderLabels({
        "Rule", "Expected key", "Expected value", "Observed item",
        "Accept item equivalence", "Accept value equivalence"
    });
    m_table->setAlternatingRowColors(true);

    for (int rowIndex = 0; rowIndex < rules.size(); ++rowIndex) {
        const MetadataRule &rule = rules[rowIndex];
        Row row;
        row.rule = rule;
        for (const MetadataObservation &observation : observations) {
            if (observation.key() == rule.key())
                row.choices.append(observation);
        }
        if (row.choices.size() > 1) {
            std::stable_sort(
                row.choices.begin(), row.choices.end(),
                [&rule](const MetadataObservation &left,
                        const MetadataObservation &right) {
                    return left.value == rule.expected
                        && right.value != rule.expected;
                });
        }
        if (row.choices.isEmpty())
            row.choices = MetadataRegistry::candidates(rule, observations);

        m_table->setItem(rowIndex, 0, new QTableWidgetItem(rule.label));
        m_table->setItem(rowIndex, 1, new QTableWidgetItem(rule.key()));
        m_table->setItem(rowIndex, 2, new QTableWidgetItem(rule.expected));

        row.observed = new QComboBox(m_table);
        if (row.choices.isEmpty()) {
            row.observed->addItem("No candidate available");
            row.observed->setEnabled(false);
        } else {
            for (const MetadataObservation &choice : row.choices) {
                row.observed->addItem(
                    QString("%1 = %2").arg(choice.key(), choice.value));
            }
        }
        m_table->setCellWidget(rowIndex, 3, row.observed);

        row.itemEquivalence = new QCheckBox(m_table);
        row.valueEquivalence = new QCheckBox(m_table);
        m_table->setCellWidget(rowIndex, 4, row.itemEquivalence);
        m_table->setCellWidget(rowIndex, 5, row.valueEquivalence);

        m_rows.append(row);
        connect(row.observed, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, rowIndex]() { updateRow(rowIndex); });
        connect(row.itemEquivalence, &QCheckBox::toggled,
                this, [this, rowIndex]() { updateRow(rowIndex); });
        connect(row.valueEquivalence, &QCheckBox::toggled,
                this, [this, rowIndex]() { updateRow(rowIndex); });
        updateRow(rowIndex);
    }

    m_table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::Stretch);
    layout->addWidget(m_table);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *rejectButton =
        buttons->addButton("Save rejection", QDialogButtonBox::ActionRole);
    m_acceptButton =
        buttons->addButton("Accept", QDialogButtonBox::AcceptRole);
    connect(rejectButton, &QPushButton::clicked, this, [this]() {
        m_acceptedDecision = false;
        QDialog::accept();
    });
    connect(m_acceptButton, &QPushButton::clicked, this, [this]() {
        m_acceptedDecision = true;
        QDialog::accept();
    });
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    layout->addWidget(buttons);
    updateAcceptEnabled();
}

QList<MetadataRuleDecision> RegistryValidationDialog::decisions() const
{
    QList<MetadataRuleDecision> result;
    for (const Row &row : m_rows) {
        MetadataRuleDecision decision;
        decision.rule = row.rule;
        if (!row.choices.isEmpty()) {
            const MetadataObservation &choice =
                row.choices[qMax(0, row.observed->currentIndex())];
            decision.observedGroup = choice.group;
            decision.observedTag = choice.tag;
            decision.observedValue = choice.value;
        }
        decision.itemEquivalence = row.itemEquivalence->isChecked();
        decision.valueEquivalence = row.valueEquivalence->isChecked();
        result.append(decision);
    }
    return result;
}

bool RegistryValidationDialog::acceptedDecision() const
{
    return m_acceptedDecision;
}

void RegistryValidationDialog::updateRow(int rowIndex)
{
    Row &row = m_rows[rowIndex];
    const bool hasChoice = !row.choices.isEmpty();
    const MetadataObservation choice =
        hasChoice ? row.choices[qMax(0, row.observed->currentIndex())]
                  : MetadataObservation();
    const bool itemDiffers = !hasChoice || choice.key() != row.rule.key();
    const bool valueDiffers = !hasChoice || choice.value != row.rule.expected;

    row.itemEquivalence->setEnabled(hasChoice && itemDiffers);
    if (!itemDiffers)
        row.itemEquivalence->setChecked(false);
    row.valueEquivalence->setEnabled(hasChoice && valueDiffers);
    if (!valueDiffers)
        row.valueEquivalence->setChecked(false);

    QString status = "Exact match";
    if (!hasChoice)
        status = "Missing";
    else if (itemDiffers && valueDiffers)
        status = "Item and value differ";
    else if (itemDiffers)
        status = "Item differs";
    else if (valueDiffers)
        status = "Value differs";
    m_table->item(rowIndex, 0)->setToolTip(status);
    updateAcceptEnabled();
}

bool RegistryValidationDialog::rowResolved(const Row &row) const
{
    if (row.choices.isEmpty())
        return false;
    const MetadataObservation &choice =
        row.choices[qMax(0, row.observed->currentIndex())];
    const bool itemResolved =
        choice.key() == row.rule.key() || row.itemEquivalence->isChecked();
    const bool valueResolved =
        choice.value == row.rule.expected
        || row.valueEquivalence->isChecked();
    return itemResolved && valueResolved;
}

void RegistryValidationDialog::updateAcceptEnabled()
{
    if (!m_acceptButton)
        return;
    bool enabled = !m_rows.isEmpty();
    for (const Row &row : m_rows)
        enabled = enabled && rowResolved(row);
    m_acceptButton->setEnabled(enabled);
}
