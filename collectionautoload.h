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

#ifndef COLLECTIONAUTOLOAD_H
#define COLLECTIONAUTOLOAD_H

#include "layerstyle.h"
#include "project.h"

#include <QString>
#include <QVector>

class RasterData;

enum class AutoloadLayerKind {
    Image,
    Points
};

struct AutoloadCandidate
{
    Collection collection;
    QString filePath;
    AutoloadLayerKind layerKind = AutoloadLayerKind::Image;
    int priority = 0;
    bool openedFile = false;
};

class CollectionAutoloadResolver
{
public:
    QVector<AutoloadCandidate> resolve(
        const QString &projectRootPath,
        const QString &openedFilePath,
        const QVector<Collection> &collections) const;
};

class CollectionStyleResolver
{
public:
    ImageLayerStyle imageStyle(const Collection &collection,
                               const RasterData &rasterData) const;
    PointLayerStyle pointStyle(const Collection &collection,
                               int recordCount) const;
};

#endif // COLLECTIONAUTOLOAD_H
