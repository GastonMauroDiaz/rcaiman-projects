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

#ifndef POINTGRIDCREATIONDIALOG_H
#define POINTGRIDCREATIONDIALOG_H

#include <QDialog>
#include <QString>
#include <QVector>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class PointGridCreationDialog : public QDialog
{
    Q_OBJECT

public:
    struct Candidate {
        int id = -1;
        QString maskLabel;
        QString maskPath;
        QString pointCollectionLabel;
        QString pointCollectionPath;
    };

    struct Task {
        int candidateId = -1;
        int stride = 1;
        QString outputFileName;
    };

    explicit PointGridCreationDialog(const QVector<Candidate> &candidates,
                                     int preferredCandidateId,
                                     QWidget *parent = nullptr);

    QVector<Task> tasks() const;

private:
    void populateMaskCombo(int preferredCandidateId);
    void populatePointCombo();
    void addTask();
    void addAllTasks();
    void addTaskForCandidate(const Candidate &candidate, int stride);
    void prefixOutputs();
    void removeSelectedTasks();
    void updateButtons();
    QString outputValidationError() const;
    bool taskExists(int candidateId) const;
    Candidate candidateById(int candidateId) const;

    QVector<Candidate> m_candidates;
    QComboBox *m_maskCombo = nullptr;
    QComboBox *m_pointCombo = nullptr;
    QSpinBox *m_strideSpin = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLineEdit *m_prefixEdit = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_addAllButton = nullptr;
    QPushButton *m_prefixButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
};

#endif // POINTGRIDCREATIONDIALOG_H
