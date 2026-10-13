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

#include "collectionautoload.h"
#include "collectionstylefile.h"
#include "rasterdata.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
bool isImageCollection(const QString &collectionClass)
{
    return collectionClass == "preprocessed_image_collection"
        || collectionClass == "processed_image_collection";
}

bool isAutoloadCollection(const QString &collectionClass)
{
    return isImageCollection(collectionClass)
        || collectionClass == "point_collection"
        || collectionClass == "segmentation_collection";
}

AutoloadLayerKind layerKindFor(const QString &collectionClass)
{
    return collectionClass == "point_collection"
        ? AutoloadLayerKind::Points
        : AutoloadLayerKind::Image;
}

int priorityFor(const QString &collectionClass)
{
    if (collectionClass == "point_collection")
        return 600;
    if (collectionClass == "processed_image_collection")
        return 500;
    if (collectionClass == "segmentation_collection")
        return 400;
    if (collectionClass == "preprocessed_image_collection")
        return 100;
    return 0;
}

QString normalizedPath(const QString &path)
{
    QFileInfo info(path);
    QString normalized = info.canonicalFilePath();
    if (normalized.isEmpty())
        normalized = info.absoluteFilePath();
    normalized = QDir::cleanPath(QDir::fromNativeSeparators(normalized));
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

QString normalizedReference(const QString &projectRootPath,
                            const QString &reference)
{
    const QString resolved = Project::resolveReference(projectRootPath, reference);
    return resolved.isEmpty() ? QString() : normalizedPath(resolved);
}

QString matchingArtifact(const Collection &collection,
                         const QString &baseName,
                         const QString &openedFilePath)
{
    QDir dir(collection.dirPath);
    if (!dir.exists())
        return {};

    QStringList filters;
    if (collection.collectionClass == "point_collection") {
        filters = {"*.csv"};
    } else {
        filters = {"*.tif", "*.tiff", "*.png", "*.jpg", "*.jpeg", "*.bmp"};
    }

    QStringList matches;
    const QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);
    for (const QFileInfo &file : files) {
        if (file.completeBaseName().compare(baseName, Qt::CaseInsensitive) == 0)
            matches.append(file.absoluteFilePath());
    }

    if (matches.isEmpty())
        return {};

    const QString normalizedOpened = normalizedPath(openedFilePath);
    for (const QString &match : std::as_const(matches)) {
        if (normalizedPath(match) == normalizedOpened)
            return match;
    }

    if (matches.size() > 1) {
        qWarning() << "Autoload: multiple matching artifacts in"
                   << collection.dirPath << "for" << baseName
                   << "- using" << matches.first();
    }
    return matches.first();
}

bool isOpenedFileInCollection(const Collection &collection,
                              const QString &openedFilePath,
                              const QString &baseName)
{
    const QFileInfo openedInfo(openedFilePath);
    if (normalizedPath(openedInfo.absolutePath())
        != normalizedPath(collection.dirPath)) {
        return false;
    }

    const QString artifact = matchingArtifact(collection, baseName, openedFilePath);
    return !artifact.isEmpty()
        && normalizedPath(artifact) == normalizedPath(openedFilePath);
}

bool referencesCollection(const QString &projectRootPath,
                          const Collection &collection,
                          const QString &targetCollectionPath)
{
    const QString framePath =
        normalizedReference(projectRootPath, collection.imageFrame);
    return !framePath.isEmpty() && framePath == targetCollectionPath;
}

template<typename T>
bool hasExactlyTwoClasses(const RasterData &data, int band)
{
    const T *values = data.bandPointer<T>(band);
    if (!values)
        return false;

    QSet<double> classes;
    const qsizetype count = static_cast<qsizetype>(data.width()) * data.height();
    for (qsizetype i = 0; i < count; ++i) {
        const double value = static_cast<double>(values[i]);
        if (!std::isfinite(value))
            continue;

        classes.insert(value);
        if (classes.size() > 2)
            return false;
    }

    return classes.size() == 2;
}

bool rasterHasExactlyTwoClasses(const RasterData &data)
{
    if (data.isNull() || data.bands() < 1)
        return false;

    switch (data.dataType()) {
    case RasterData::DataType::UInt8:
        return hasExactlyTwoClasses<uint8_t>(data, 0);
    case RasterData::DataType::UInt16:
        return hasExactlyTwoClasses<uint16_t>(data, 0);
    case RasterData::DataType::Int16:
        return hasExactlyTwoClasses<int16_t>(data, 0);
    case RasterData::DataType::UInt32:
        return hasExactlyTwoClasses<uint32_t>(data, 0);
    case RasterData::DataType::Int32:
        return hasExactlyTwoClasses<int32_t>(data, 0);
    case RasterData::DataType::Float32:
        return hasExactlyTwoClasses<float>(data, 0);
    case RasterData::DataType::Float64:
        return hasExactlyTwoClasses<double>(data, 0);
    default:
        return false;
    }
}
}

QVector<AutoloadCandidate> CollectionAutoloadResolver::resolve(
    const QString &projectRootPath,
    const QString &openedFilePath,
    const QVector<Collection> &collections) const
{
    const QFileInfo openedInfo(openedFilePath);
    const QString baseName = openedInfo.completeBaseName();

    Collection anchorCollection;
    bool hasAnchor = false;
    for (const Collection &collection : collections) {
        if (collection.collectionClass != "preprocessed_image_collection")
            continue;
        if (isOpenedFileInCollection(collection, openedFilePath, baseName)) {
            anchorCollection = collection;
            hasAnchor = true;
            break;
        }
    }

    if (!hasAnchor)
        return {};

    const QString anchorPath = normalizedPath(anchorCollection.dirPath);
    QVector<AutoloadCandidate> result;
    for (const Collection &collection : collections) {
        if (!isAutoloadCollection(collection.collectionClass))
            continue;

        const QString collectionPath = normalizedPath(collection.dirPath);
        bool include = collectionPath == anchorPath;

        if (!include) {
            const QString collectionClass = collection.collectionClass;
            if (collectionClass != "preprocessed_image_collection") {
                include = referencesCollection(
                    projectRootPath, collection, anchorPath);
            }
        }

        if (!include)
            continue;

        const QString artifact =
            matchingArtifact(collection, baseName, openedFilePath);
        if (artifact.isEmpty())
            continue;

        AutoloadCandidate candidate;
        candidate.collection = collection;
        candidate.filePath = artifact;
        candidate.layerKind = layerKindFor(collection.collectionClass);
        candidate.priority = priorityFor(collection.collectionClass);
        candidate.openedFile =
            normalizedPath(artifact) == normalizedPath(openedFilePath);
        result.append(candidate);
    }

    std::sort(result.begin(), result.end(),
              [](const AutoloadCandidate &left, const AutoloadCandidate &right) {
                  if (left.priority != right.priority)
                      return left.priority < right.priority;
                  return left.collection.dirName.compare(
                      right.collection.dirName, Qt::CaseInsensitive) > 0;
              });

    return result;
}

ImageLayerStyle CollectionStyleResolver::imageStyle(
    const Collection &collection,
    const RasterData &rasterData) const
{
    ImageLayerStyle style;

    if (collection.collectionClass == "preprocessed_image_collection") {
        style.render.stretchMode = StretchMode::None;
        style.render.tailTrim = false;
        style.render.mode = RasterRenderMode::Rgb;
        style.render.redBand = 0;
        style.render.greenBand = 1;
        style.render.blueBand = 2;
    } else if (collection.collectionClass == "processed_image_collection") {
        style.render.stretchMode = StretchMode::None;
        style.render.tailTrim = false;
        style.render.mode = RasterRenderMode::SingleBand;
        style.render.colorRamp = ColorRamp::GreenMagenta;
    } else if (collection.collectionClass == "segmentation_collection") {
        style.render.stretchMode = StretchMode::None;
        style.render.tailTrim = false;
        style.render.mode = RasterRenderMode::SingleBand;
        style.render.colorRamp = rasterHasExactlyTwoClasses(rasterData)
            ? ColorRamp::Gray
            : ColorRamp::Categorical;
    }

    const CollectionStyleData persisted =
        CollectionStyleFile::load(collection.dirPath);
    if (persisted.kind == CollectionStyleData::Kind::Image)
        return persisted.imageStyle;
    return style;
}

PointLayerStyle CollectionStyleResolver::pointStyle(
    const Collection &collection,
    int recordCount) const
{
    Q_UNUSED(collection);

    PointLayerStyle style;
    if (recordCount == 1) {
        style.symbol = PointSymbol::Circle;
        style.color = Qt::yellow;
        style.size = 20.0;
    } else {
        style.symbol = PointSymbol::Cross;
        style.color = Qt::magenta;
        style.size = 10.0;
    }
    const CollectionStyleData persisted =
        CollectionStyleFile::load(collection.dirPath);
    if (persisted.kind == CollectionStyleData::Kind::Point)
        return persisted.pointStyle;
    return style;
}
