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

#ifndef COLLECTIONVALIDATIONDIALOG_H
#define COLLECTIONVALIDATIONDIALOG_H

#include "collectionvalidation.h"

#include <QDialog>
#include <QSet>

class QLabel;
class QPushButton;
class QTableWidget;

class CollectionValidationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CollectionValidationDialog(
        const CollectionValidationRun &run,
        QWidget *parent = nullptr);

signals:
    void reviewRequested(const QString &rawPath,
                         const QString &registryPath);

private:
    void done(int result) override;
    void updateReviewButton();
    void reviewSelectedRow();
    void markRowReviewed(int row);

    CollectionValidationRun m_run;
    QTableWidget *m_table = nullptr;
    QPushButton *m_reviewButton = nullptr;
    QSet<int> m_reviewedRows;
    bool m_closeNoticeShown = false;
};

#endif
