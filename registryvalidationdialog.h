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

#ifndef REGISTRYVALIDATIONDIALOG_H
#define REGISTRYVALIDATIONDIALOG_H

#include "metadataregistry.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

class RegistryValidationDialog : public QDialog
{
    Q_OBJECT

public:
    RegistryValidationDialog(
        const QList<MetadataRule> &rules,
        const QList<MetadataObservation> &observations,
        QWidget *parent = nullptr);

    QList<MetadataRuleDecision> decisions() const;
    bool acceptedDecision() const;

private:
    struct Row {
        MetadataRule rule;
        QList<MetadataObservation> choices;
        QComboBox *observed = nullptr;
        QCheckBox *itemEquivalence = nullptr;
        QCheckBox *valueEquivalence = nullptr;
        QLabel *status = nullptr;
    };

    void updateRow(int row);
    void updateAcceptEnabled();
    bool rowResolved(const Row &row) const;

    QList<Row> m_rows;
    QTableWidget *m_table = nullptr;
    QPushButton *m_acceptButton = nullptr;
    bool m_acceptedDecision = false;
};

#endif
