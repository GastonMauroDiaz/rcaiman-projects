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
#include "collectionappearance.h"
#include "project.h"
#include "tiffreader.h"

#include <QBrush>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QStringList>
#include <QTextStream>

#include <cmath>

namespace {
QString valueAfterColon(const QString &line)
{
    const int separator = line.indexOf(':');
    return separator >= 0 ? line.mid(separator + 1).trimmed() : QString();
}

QString unescapeYamlQuotedString(const QString &value)
{
    QString result;
    result.reserve(value.size());
    bool escaping = false;

    for (const QChar character : value) {
        if (escaping) {
            if (character == QLatin1Char('n'))
                result.append(QLatin1Char('\n'));
            else if (character == QLatin1Char('r'))
                result.append(QLatin1Char('\r'));
            else if (character == QLatin1Char('t'))
                result.append(QLatin1Char('\t'));
            else
                result.append(character);
            escaping = false;
        } else if (character == QLatin1Char('\\')) {
            escaping = true;
        } else {
            result.append(character);
        }
    }

    if (escaping)
        result.append(QLatin1Char('\\'));

    return result;
}

QString yamlValueAfterColon(const QString &line)
{
    QString value = valueAfterColon(line);
    if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"')) {
        value = value.mid(1, value.size() - 2);
        value = unescapeYamlQuotedString(value);
    }
    return value;
}

QStringList parseCsvLine(const QString &line)
{
    QStringList result;
    bool inQuotes = false;
    QString current;
    for (int i = 0; i < line.size(); ++i) {
        const QChar character = line.at(i);
        if (character == QLatin1Char('"')) {
            inQuotes = !inQuotes;
        } else if (character == QLatin1Char(',') && !inQuotes) {
            result.append(current.trimmed());
            current.clear();
        } else {
            current.append(character);
        }
    }
    result.append(current.trimmed());
    return result;
}

}

ProjectFileSystemModel::ProjectFileSystemModel(QObject *parent)
    : QFileSystemModel(parent)
{
    auto refreshParent = [this](const QModelIndex &parent) {
        clearAllDirectoryCaches();
        if (parent.isValid())
            emit dataChanged(
                parent, parent, {Qt::DisplayRole, Qt::DecorationRole});
    };

    connect(this, &QAbstractItemModel::rowsInserted,
            this, [refreshParent](const QModelIndex &parent, int, int) {
                refreshParent(parent);
            });
    connect(this, &QAbstractItemModel::rowsRemoved,
            this, [refreshParent](const QModelIndex &parent, int, int) {
                refreshParent(parent);
            });
    connect(this, &QAbstractItemModel::dataChanged,
            this, [this](const QModelIndex&, const QModelIndex&,
                         const QList<int>&) {
                clearAllDirectoryCaches();
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
    if (role == ProjectFileImageDimensionsRole)
        return imageDimensions(info);
    if (role == ProjectFilePointCountRole)
        return pointCount(info);

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

    if (role == Qt::ForegroundRole && reserved)
        return QBrush(QColor(35, 105, 190));

    if (role == Qt::DecorationRole && reserved) {
        return CollectionAppearance::reservedFolderIcon(
            directFileCount(info) > 0);
    }

    if (role == Qt::DecorationRole
        && !reserved
        && !className.isEmpty()) {
        const QIcon collectionIcon = CollectionAppearance::folderIcon(
            className, collectionHasContent(info, className));
        if (!collectionIcon.isNull())
            return collectionIcon;
    }

    if (role == Qt::ToolTipRole
        && reserved
        && info.fileName().compare(
            ".rcaiman", Qt::CaseInsensitive) == 0) {
        return QStringLiteral(
            ".rcaiman stores the application's session state.");
    }

    if (role != Qt::DisplayRole)
        return QFileSystemModel::data(index, role);

    if (otherCollection) {
        return appendAlias(
            QFileSystemModel::data(index, role).toString(),
            info.absoluteFilePath());
    }

    const bool countedReserved =
        reserved
        && (info.fileName().compare("raw_files", Qt::CaseInsensitive) == 0
            || info.fileName().compare("sensor_data", Qt::CaseInsensitive) == 0);
    if (reserved && reservedDirectoryShowsEmptyLabel(info.fileName())
        && directFileCount(info) == 0) {
        return appendAlias(
            QStringLiteral("%1 (empty)").arg(info.fileName()),
            info.absoluteFilePath());
    }
    if (className.isEmpty() && !countedReserved)
        return QFileSystemModel::data(index, role);

    const int count = relevantFileCount(info, className);
    return appendAlias(
        QStringLiteral("%1 (%2)").arg(info.fileName()).arg(count),
        info.absoluteFilePath());
}

void ProjectFileSystemModel::setProjectRoot(const QString &path)
{
    m_projectRoot = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    clearAllDirectoryCaches();
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
    clearDirectoryCache(path);
    const QModelIndex directoryIndex = index(path);
    if (directoryIndex.isValid())
        emit dataChanged(directoryIndex, directoryIndex,
                         {Qt::DisplayRole, ProjectFileMarkedRole,
                          ProjectFileDirectlyMarkedRole,
                          ProjectFileImageDimensionsRole,
                          ProjectFilePointCountRole,
                          Qt::ForegroundRole, Qt::DecorationRole,
                          Qt::ToolTipRole});
}

void ProjectFileSystemModel::refreshDirectoryCount(const QString &path)
{
    const QString key = cachePath(path);
    m_directFileCountCache.remove(key);

    const QString prefix = key + QLatin1Char('|');
    const QStringList relevantKeys = m_relevantFileCountCache.keys();
    for (const QString &relevantKey : relevantKeys) {
        if (relevantKey.startsWith(prefix))
            m_relevantFileCountCache.remove(relevantKey);
    }

    const QModelIndex directoryIndex = index(path);
    if (directoryIndex.isValid())
        emit dataChanged(
            directoryIndex, directoryIndex,
            {Qt::DisplayRole, Qt::DecorationRole});
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

bool ProjectFileSystemModel::isTiffFile(const QFileInfo &info) const
{
    if (!info.isFile())
        return false;

    const QString suffix = info.suffix();
    return suffix.compare("tif", Qt::CaseInsensitive) == 0
        || suffix.compare("tiff", Qt::CaseInsensitive) == 0;
}

bool ProjectFileSystemModel::isCsvFile(const QFileInfo &info) const
{
    return info.isFile()
        && info.suffix().compare("csv", Qt::CaseInsensitive) == 0;
}

QString ProjectFileSystemModel::imageDimensions(
    const QFileInfo &info) const
{
    if (!isTiffFile(info))
        return {};

    const QString key = cachePath(info);
    const qint64 fileSize = info.size();
    const qint64 lastModified = info.lastModified().toMSecsSinceEpoch();
    const auto cached = m_imageDimensionsCache.constFind(key);
    if (cached != m_imageDimensionsCache.constEnd()
        && cached->fileSize == fileSize
        && cached->lastModified == lastModified) {
        return cached->text;
    }

    QString text;
    const TiffImageInfo tiffInfo =
        TiffReader::imageInfo(info.absoluteFilePath());
    if (tiffInfo.isValid()) {
        text = QStringLiteral("%1 x %2 pixels")
            .arg(tiffInfo.width)
            .arg(tiffInfo.height);
    }

    m_imageDimensionsCache.insert(
        key,
        ImageDimensionsCacheEntry{fileSize, lastModified, text});
    return text;
}

QString ProjectFileSystemModel::pointCount(const QFileInfo &info) const
{
    if (!isCsvFile(info))
        return {};

    const QString key = cachePath(info);
    const qint64 fileSize = info.size();
    const qint64 lastModified = info.lastModified().toMSecsSinceEpoch();
    const auto cached = m_pointCountCache.constFind(key);
    if (cached != m_pointCountCache.constEnd()
        && cached->fileSize == fileSize
        && cached->lastModified == lastModified) {
        return cached->text;
    }

    QString text;
    QFile file(info.absoluteFilePath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        const QString header = stream.readLine();
        const QStringList columns = parseCsvLine(header);
        const bool validHeader = columns.size() >= 2
            && columns.at(0) == QStringLiteral("x")
            && columns.at(1) == QStringLiteral("y_down");
        bool validRows = validHeader;
        int count = 0;
        while (validRows && !stream.atEnd()) {
            const QString line = stream.readLine();
            if (line.trimmed().isEmpty())
                continue;

            const QStringList values = parseCsvLine(line);
            if (values.size() < 2) {
                validRows = false;
                break;
            }

            bool xOk = false;
            bool yDownOk = false;
            const double x = values.at(0).toDouble(&xOk);
            const double yDown = values.at(1).toDouble(&yDownOk);
            if (!xOk || !yDownOk
                || !std::isfinite(x) || !std::isfinite(yDown)) {
                validRows = false;
                break;
            }
            ++count;
        }

        if (validRows) {
            text = QStringLiteral("%1 point%2")
                .arg(count)
                .arg(count == 1 ? "" : "s");
        }
    }

    m_pointCountCache.insert(
        key,
        PointCountCacheEntry{fileSize, lastModified, text});
    return text;
}

QString ProjectFileSystemModel::collectionClass(
    const QString &directoryPath) const
{
    const QString key = cachePath(directoryPath);
    if (m_collectionClassCache.contains(key))
        return m_collectionClassCache.value(key);

    QString className;
    QFile file(QDir(directoryPath).filePath("collection.yaml"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_collectionClassCache.insert(key, className);
        return {};
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.startsWith("class:")) {
            className = valueAfterColon(line);
            break;
        }
    }
    m_collectionClassCache.insert(key, className);
    return className;
}

QString ProjectFileSystemModel::collectionClassForDirectory(
    const QString &directoryPath) const
{
    return collectionClass(directoryPath);
}

QString ProjectFileSystemModel::collectionAlias(
    const QString &directoryPath) const
{
    const QString key = cachePath(directoryPath);
    if (m_collectionAliasCache.contains(key))
        return m_collectionAliasCache.value(key);

    QString alias;
    QFile file(QDir(directoryPath).filePath("collection.yaml"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_collectionAliasCache.insert(key, alias);
        return {};
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.startsWith("alias:")) {
            alias = yamlValueAfterColon(line).trimmed();
            break;
        }
    }
    m_collectionAliasCache.insert(key, alias);
    return alias;
}

QString ProjectFileSystemModel::cachePath(const QString &path) const
{
    QString normalized = QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

QString ProjectFileSystemModel::cachePath(const QFileInfo &info) const
{
    return cachePath(info.absoluteFilePath());
}

void ProjectFileSystemModel::clearDirectoryCache(const QString &path) const
{
    const QString key = cachePath(path);
    m_collectionClassCache.remove(key);
    m_collectionAliasCache.remove(key);
    m_directFileCountCache.remove(key);
    m_imageDimensionsCache.remove(key);
    m_pointCountCache.remove(key);

    const QString prefix = key + QLatin1Char('|');
    const QStringList relevantKeys = m_relevantFileCountCache.keys();
    for (const QString &relevantKey : relevantKeys) {
        if (relevantKey.startsWith(prefix))
            m_relevantFileCountCache.remove(relevantKey);
    }
}

void ProjectFileSystemModel::clearAllDirectoryCaches() const
{
    m_collectionClassCache.clear();
    m_collectionAliasCache.clear();
    m_directFileCountCache.clear();
    m_relevantFileCountCache.clear();
    m_imageDimensionsCache.clear();
    m_pointCountCache.clear();
}

QString ProjectFileSystemModel::appendAlias(
    const QString &displayText,
    const QString &directoryPath) const
{
    const QString alias = collectionAlias(directoryPath);
    if (alias.isEmpty())
        return displayText;
    return QStringLiteral("%1 | %2").arg(displayText, alias);
}

bool ProjectFileSystemModel::reservedDirectoryShowsEmptyLabel(
    const QString &name) const
{
    return name.compare("registry_drafts", Qt::CaseInsensitive) == 0
        || name.compare("registry_reviews", Qt::CaseInsensitive) == 0
        || name.compare("registry_validations", Qt::CaseInsensitive) == 0
        || name.compare("scripts", Qt::CaseInsensitive) == 0;
}

bool ProjectFileSystemModel::collectionHasContent(
    const QFileInfo &info,
    const QString &collectionClass) const
{
    if (collectionClass != "other")
        return relevantFileCount(info, collectionClass) > 0;

    int contentFiles = directFileCount(info);
    const QDir directory(info.absoluteFilePath());
    if (QFileInfo::exists(directory.filePath("collection.yaml")))
        --contentFiles;
    if (QFileInfo::exists(directory.filePath("style.yaml")))
        --contentFiles;
    return contentFiles > 0;
}

int ProjectFileSystemModel::directFileCount(const QFileInfo &info) const
{
    const QString key = cachePath(info);
    if (m_directFileCountCache.contains(key))
        return m_directFileCountCache.value(key);

    const int count = QDir(info.absoluteFilePath())
        .entryList(QDir::Files | QDir::NoSymLinks)
        .size();
    m_directFileCountCache.insert(key, count);
    return count;
}

int ProjectFileSystemModel::relevantFileCount(
    const QFileInfo &info,
    const QString &className) const
{
    const QString key = cachePath(info) + QLatin1Char('|') + className;
    if (m_relevantFileCountCache.contains(key))
        return m_relevantFileCountCache.value(key);

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
        m_relevantFileCountCache.insert(key, 0);
        return 0;
    }

    const int count = QDir(info.absoluteFilePath())
        .entryList(filters, QDir::Files | QDir::NoSymLinks)
        .size();
    m_relevantFileCountCache.insert(key, count);
    return count;
}
