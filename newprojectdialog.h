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

#ifndef NEWPROJECTDIALOG_H
#define NEWPROJECTDIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget *parent = nullptr);

    QString parentDirectory() const;
    QString folderName() const;
    QString description() const;

private:
    void browseParentDirectory();
    void updateCreateButton();

    QLineEdit *m_parentDirectory = nullptr;
    QLineEdit *m_folderName = nullptr;
    QLineEdit *m_description = nullptr;
    QLabel *m_descriptionWarning = nullptr;
    QPushButton *m_createButton = nullptr;
};

#endif // NEWPROJECTDIALOG_H
