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

#ifndef PROJECTFILESYSTEMMODEL_H
#define PROJECTFILESYSTEMMODEL_H

#include <QFileSystemModel>

class QFileInfo;

class ProjectFileSystemModel : public QFileSystemModel
{
    Q_OBJECT

public:
    explicit ProjectFileSystemModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    void setProjectRoot(const QString &path);
    void refreshDirectory(const QString &path);

private:
    bool isDirectProjectChild(const QFileInfo &info) const;
    QString collectionClass(const QString &directoryPath) const;
    int relevantFileCount(const QFileInfo &info,
                          const QString &collectionClass) const;

    QString m_projectRoot;
};

#endif // PROJECTFILESYSTEMMODEL_H
