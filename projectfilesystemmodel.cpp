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

#include "projectfilesystemmodel.h"
#include "project.h"

#include <QBrush>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {
QString valueAfterColon(const QString &line)
{
    const int separator = line.indexOf(':');
    return separator >= 0 ? line.mid(separator + 1).trimmed() : QString();
}
}

ProjectFileSystemModel::ProjectFileSystemModel(QObject *parent)
    : QFileSystemModel(parent)
{
    auto refreshParent = [this](const QModelIndex &parent) {
        if (parent.isValid())
            emit dataChanged(parent, parent, {Qt::DisplayRole});
    };

    connect(this, &QAbstractItemModel::rowsInserted,
            this, [refreshParent](const QModelIndex &parent, int, int) {
                refreshParent(parent);
            });
    connect(this, &QAbstractItemModel::rowsRemoved,
            this, [refreshParent](const QModelIndex &parent, int, int) {
                refreshParent(parent);
            });
}

QVariant ProjectFileSystemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0)
        return QFileSystemModel::data(index, role);

    const QFileInfo info = fileInfo(index);
    if (role == ProjectFileMarkedRole)
        return isMarkedOrInherited(info);
    if (role == ProjectFileDirectlyMarkedRole)
        return m_fileMarks.contains(projectRelativePath(info));

    if (!info.isDir())
        return QFileSystemModel::data(index, role);

    const bool directProjectChild = isDirectProjectChild(info);
    const bool reserved = directProjectChild
        && Project::reservedDirectories().contains(
            info.fileName(), Qt::CaseInsensitive);

    QString className;
    if (directProjectChild)
        className = collectionClass(info.absoluteFilePath());
    const bool otherCollection = className == "other";
    const bool countedCollection =
        className == "preprocessed_image_collection"
        || className == "processed_image_collection"
        || className == "segmentation_collection"
        || className == "point_collection";

    if (role == Qt::ForegroundRole && otherCollection)
        return QBrush(QColor(170, 90, 0));

    if (role == Qt::ForegroundRole && reserved)
        return QBrush(QColor(35, 105, 190));

    if (role == Qt::ForegroundRole && countedCollection)
        return QBrush(QColor(0, 85, 0));

    if (role == Qt::ToolTipRole
        && reserved
        && info.fileName().compare(
            ".rcaiman", Qt::CaseInsensitive) == 0) {
        return QStringLiteral(
            ".rcaiman stores the application's session state.");
    }

    if (role != Qt::DisplayRole)
        return QFileSystemModel::data(index, role);

    if (otherCollection)
        return QFileSystemModel::data(index, role);

    const bool countedReserved =
        reserved
        && (info.fileName().compare("raw_files", Qt::CaseInsensitive) == 0
            || info.fileName().compare("sensor_data", Qt::CaseInsensitive) == 0);
    if (className.isEmpty() && !countedReserved)
        return QFileSystemModel::data(index, role);

    const int count = relevantFileCount(info, className);
    return QStringLiteral("%1 (%2)").arg(info.fileName()).arg(count);
}

void ProjectFileSystemModel::setProjectRoot(const QString &path)
{
    m_projectRoot = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

void ProjectFileSystemModel::setFileMarks(const QSet<QString> &marks)
{
    m_fileMarks = marks;
}

bool ProjectFileSystemModel::isMarkedOrInherited(const QFileInfo &info) const
{
    QString relative = projectRelativePath(info);
    if (relative.isEmpty())
        return false;

    relative = QDir::cleanPath(QDir::fromNativeSeparators(relative));
    while (!relative.isEmpty() && relative != ".") {
        if (m_fileMarks.contains(relative))
            return true;
        const int slash = relative.lastIndexOf('/');
        if (slash < 0)
            break;
        relative = relative.left(slash);
    }
    return false;
}

bool ProjectFileSystemModel::hasMarkedDescendant(const QFileInfo &info) const
{
    if (!info.isDir())
        return false;

    QString relative = projectRelativePath(info);
    if (relative.isEmpty())
        return false;

    relative = QDir::cleanPath(QDir::fromNativeSeparators(relative));
    const QString prefix = relative == "."
        ? QString()
        : relative + QLatin1Char('/');
    for (const QString &mark : m_fileMarks) {
        if (prefix.isEmpty() || mark.startsWith(prefix))
            return true;
    }
    return false;
}

void ProjectFileSystemModel::refreshDirectory(const QString &path)
{
    const QModelIndex directoryIndex = index(path);
    if (directoryIndex.isValid())
        emit dataChanged(directoryIndex, directoryIndex,
                         {Qt::DisplayRole, ProjectFileMarkedRole,
                          ProjectFileDirectlyMarkedRole,
                          Qt::ForegroundRole, Qt::ToolTipRole});
}

bool ProjectFileSystemModel::isDirectProjectChild(const QFileInfo &info) const
{
    if (m_projectRoot.isEmpty())
        return false;

    const QString parentPath =
        QDir::cleanPath(info.absoluteDir().absolutePath());
#ifdef Q_OS_WIN
    return parentPath.compare(m_projectRoot, Qt::CaseInsensitive) == 0;
#else
    return parentPath == m_projectRoot;
#endif
}

QString ProjectFileSystemModel::projectRelativePath(
    const QFileInfo &info) const
{
    if (m_projectRoot.isEmpty())
        return {};

    const QString relative = QDir(m_projectRoot).relativeFilePath(
        info.absoluteFilePath());
    if (relative == "." || relative.startsWith("../"))
        return {};
    return QDir::fromNativeSeparators(relative);
}

QString ProjectFileSystemModel::collectionClass(
    const QString &directoryPath) const
{
    QFile file(QDir(directoryPath).filePath("collection.yaml"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.startsWith("class:"))
            return valueAfterColon(line);
    }
    return {};
}

int ProjectFileSystemModel::relevantFileCount(
    const QFileInfo &info,
    const QString &className) const
{
    QStringList filters;
    if (info.fileName().compare("raw_files", Qt::CaseInsensitive) == 0) {
        filters = {
            "*.CR2", "*.CR3", "*.NEF", "*.ARW", "*.DNG", "*.RAF",
            "*.ORF", "*.RW2", "*.PEF", "*.SRW", "*.RAW", "*.DCR", "*.KDC"
        };
    } else if (info.fileName().compare("sensor_data", Qt::CaseInsensitive) == 0
               || className == "preprocessed_image_collection"
               || className == "processed_image_collection"
               || className == "segmentation_collection") {
        filters = {"*.tif", "*.tiff", "*.TIF", "*.TIFF"};
    } else if (className == "point_collection") {
        filters = {"*.csv", "*.CSV"};
    } else {
        return 0;
    }

    return QDir(info.absoluteFilePath())
        .entryList(filters, QDir::Files | QDir::NoSymLinks)
        .size();
}
