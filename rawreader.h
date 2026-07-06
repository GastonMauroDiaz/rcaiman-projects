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

#ifndef RAWREADER_H
#define RAWREADER_H

#include <array>

#include <QString>

struct RawBlackLevels
{
    bool hasBlack = false;
    double black = 0.0;
    bool hasCblack = false;
    std::array<double, 4> cblack = {0.0, 0.0, 0.0, 0.0};
};

class RawReader
{
public:
    // Extract raw sensor data from a RAW file and write it as a single-band
    // UInt16 TIFF (no demosaicing, no white balance, no corrections).
    // Equivalent to dcraw -D -4 -T.
    static bool extractSensorData(const QString &rawFilePath,
                                  const QString &tiffOutputPath,
                                  QString *errorMessage = nullptr);
    static bool readBlackLevels(const QString &rawFilePath,
                                RawBlackLevels *blackLevels,
                                QString *errorMessage = nullptr);
};

#endif // RAWREADER_H
