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

#ifndef METADATARULESDIALOG_H
#define METADATARULESDIALOG_H

#include "metadataregistry.h"

#include <QDialog>

class QLineEdit;
class QTableWidget;

class MetadataRulesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MetadataRulesDialog(
        const QList<MetadataRuleAnalysis> &analysis,
        QWidget *parent = nullptr);

    QString draftId() const;
    QList<MetadataRule> selectedRules() const;

protected:
    void accept() override;

private:
    QList<MetadataRuleAnalysis> m_analysis;
    QLineEdit *m_draftId = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif
