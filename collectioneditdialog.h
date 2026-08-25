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

#ifndef COLLECTIONEDITDIALOG_H
#define COLLECTIONEDITDIALOG_H

#include "project.h"

#include <QDialog>
#include <QVector>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QWidget;

class CollectionEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CollectionEditDialog(const QString &projectRootPath,
                                  const Collection &collection,
                                  const QVector<Collection> &collections,
                                  QWidget *parent = nullptr);

    QString alias() const;
    QString parentReference() const;
    QString relatedReference() const;
    QString registry() const;
    QString script() const;

private slots:
    void updateOkButton();
    void browseRegistry();
    void browseScript();

private:
    void setupUI();
    void updateFieldVisibility();
    void populateParentCombo();
    void populateRelatedCombo();
    void setComboToReference(QComboBox *combo,
                             const QString &reference);
    QStringList compatibleClasses(bool asParent) const;
    QString collectionLabel(const Collection &collection) const;
    bool hasCompatibleCollection(const QStringList &classes) const;

    QString m_projectRootPath;
    Collection m_collection;
    QVector<Collection> m_collections;

    QLineEdit *m_aliasEdit = nullptr;
    QComboBox *m_parentCombo = nullptr;
    QComboBox *m_relatedCombo = nullptr;
    QLineEdit *m_registryEdit = nullptr;
    QLineEdit *m_scriptEdit = nullptr;
    QWidget *m_parentWidget = nullptr;
    QWidget *m_relatedWidget = nullptr;
    QWidget *m_registryWidget = nullptr;
    QWidget *m_scriptWidget = nullptr;
    QLabel *m_contractLabel = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

#endif // COLLECTIONEDITDIALOG_H
