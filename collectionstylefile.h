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

#ifndef COLLECTIONSTYLEFILE_H
#define COLLECTIONSTYLEFILE_H

#include "layerstyle.h"

#include <QString>

struct CollectionStyleData
{
    enum class Kind {
        None,
        Point,
        Image
    };

    Kind kind = Kind::None;
    PointLayerStyle pointStyle;
    ImageLayerStyle imageStyle;
};

class CollectionStyleFile
{
public:
    static QString filePath(const QString &collectionDirectory);
    static CollectionStyleData load(const QString &collectionDirectory);
    static bool save(const QString &collectionDirectory,
                     const CollectionStyleData &data,
                     QString *errorMessage = nullptr);
    static bool remove(const QString &collectionDirectory,
                       QString *errorMessage = nullptr);
};

#endif // COLLECTIONSTYLEFILE_H
