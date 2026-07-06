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

#ifndef PROJECTCLEANUPDIALOG_H
#define PROJECTCLEANUPDIALOG_H

#include "project.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;

class ProjectCleanupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProjectCleanupDialog(const ProjectCleanupPlan &plan,
                                  const QString &currentDescription,
                                  QWidget *parent = nullptr);

    ProjectCleanupPlan selectedPlan() const;
    QString selectedDescription() const;

private:
    void applyProfile(int profileIndex);
    void setCustomProfile();
    void updateSummary();
    void updateDescriptionValidation();

    ProjectCleanupPlan m_plan;
    QVector<QCheckBox*> m_checkBoxes;
    QComboBox *m_profileCombo = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QCheckBox *m_keepDescription = nullptr;
    QLineEdit *m_newDescription = nullptr;
    QLabel *m_descriptionWarning = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    QString m_currentDescription;
    bool m_updatingProfile = false;
};

#endif // PROJECTCLEANUPDIALOG_H
