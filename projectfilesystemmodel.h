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
#include <QHash>
#include <QSet>

class QFileInfo;

constexpr int ProjectFileMarkedRole = Qt::UserRole + 100;
constexpr int ProjectFileDirectlyMarkedRole = Qt::UserRole + 101;
constexpr int ProjectExplorerGroupRole = Qt::UserRole + 102;
constexpr int ProjectExplorerGroupIdRole = Qt::UserRole + 103;
constexpr int ProjectFileImageDimensionsRole = Qt::UserRole + 104;
constexpr int ProjectFilePointCountRole = Qt::UserRole + 105;

class ProjectFileSystemModel : public QFileSystemModel
{
    Q_OBJECT

public:
    explicit ProjectFileSystemModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    void setProjectRoot(const QString &path);
    void setFileMarks(const QSet<QString> &marks);
    bool isMarkedOrInherited(const QFileInfo &info) const;
    bool hasMarkedDescendant(const QFileInfo &info) const;
    void refreshDirectory(const QString &path);
    void refreshDirectoryCount(const QString &path);
    QString collectionClassForDirectory(const QString &directoryPath) const;

private:
    struct ImageDimensionsCacheEntry {
        qint64 fileSize = -1;
        qint64 lastModified = -1;
        QString text;
    };

    struct PointCountCacheEntry {
        qint64 fileSize = -1;
        qint64 lastModified = -1;
        QString text;
    };

    bool isDirectProjectChild(const QFileInfo &info) const;
    QString projectRelativePath(const QFileInfo &info) const;
    bool isTiffFile(const QFileInfo &info) const;
    bool isCsvFile(const QFileInfo &info) const;
    QString imageDimensions(const QFileInfo &info) const;
    QString pointCount(const QFileInfo &info) const;
    QString collectionClass(const QString &directoryPath) const;
    QString collectionAlias(const QString &directoryPath) const;
    QString cachePath(const QString &path) const;
    QString cachePath(const QFileInfo &info) const;
    void clearDirectoryCache(const QString &path) const;
    void clearAllDirectoryCaches() const;
    QString appendAlias(const QString &displayText,
                        const QString &directoryPath) const;
    bool reservedDirectoryShowsEmptyLabel(const QString &name) const;
    bool collectionHasContent(const QFileInfo &info,
                              const QString &collectionClass) const;
    int directFileCount(const QFileInfo &info) const;
    int relevantFileCount(const QFileInfo &info,
                          const QString &collectionClass) const;

    QString m_projectRoot;
    QSet<QString> m_fileMarks;
    mutable QHash<QString, QString> m_collectionClassCache;
    mutable QHash<QString, QString> m_collectionAliasCache;
    mutable QHash<QString, int> m_directFileCountCache;
    mutable QHash<QString, int> m_relevantFileCountCache;
    mutable QHash<QString, ImageDimensionsCacheEntry> m_imageDimensionsCache;
    mutable QHash<QString, PointCountCacheEntry> m_pointCountCache;
};

#endif // PROJECTFILESYSTEMMODEL_H
