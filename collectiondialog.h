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

#ifndef COLLECTIONDIALOG_H
#define COLLECTIONDIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QVector>

#include "project.h"

class QLabel;

class CollectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CollectionDialog(const QString &projectRootPath,
                              const QVector<Collection> &existingCollections,
                              const QString &preselectedDirectory = QString(),
                              QWidget *parent = nullptr);

    QString selectedDirectory() const;
    QString collectionClass() const;
    QString parent() const;
    QString related() const;
    QString registry() const;
    QString script() const;

private slots:
    void onClassChanged(int index);
    void updateOkButton();
    void browseRegistry();
    void browseScript();

private:
    void setupUI();
    void updateFieldVisibility();
    void updateContractText();
    void populateParentCombo();
    void populateRelatedCombo();
    QStringList compatibleClasses(const QString &targetClass, bool asParent) const;
    bool hasCompatibleCollection(const QStringList &classes) const;

    QString m_projectRootPath;
    QVector<Collection> m_existingCollections;

    QComboBox *m_typeCombo = nullptr;
    QComboBox *m_parentCombo = nullptr;
    QComboBox *m_relatedCombo = nullptr;
    QLineEdit *m_registryEdit = nullptr;
    QLineEdit *m_scriptEdit = nullptr;
    QLineEdit *m_directoryEdit = nullptr;
    QWidget *m_parentWidget = nullptr;
    QWidget *m_relatedWidget = nullptr;
    QWidget *m_registryWidget = nullptr;
    QWidget *m_scriptWidget = nullptr;
    QLabel *m_contractLabel = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

#endif // COLLECTIONDIALOG_H
