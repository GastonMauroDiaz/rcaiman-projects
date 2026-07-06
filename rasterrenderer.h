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

#ifndef RASTERRENDERER_H
#define RASTERRENDERER_H

#include "layerstyle.h"
#include "rasterdata.h"
#include <QImage>
#include <vector>

class RasterRenderer
{
public:
    RasterRenderer() = default;

    QImage renderRgb(const RasterData &data,
                     int bandR, int bandG, int bandB,
                     double minR, double maxR,
                     double minG, double maxG,
                     double minB, double maxB,
                     StretchMode stretchMode,
                     double gamma) const;

    QImage renderSingle(const RasterData &data,
                        int band,
                        double minVal, double maxVal) const;

    QImage render(const RasterData &data, const RasterRenderStyle &style) const;
    QImage renderAuto(const RasterData &data) const;
    void rawRange(const RasterData &data, int band,
                  double &outMin, double &outMax) const;
    void renderRange(const RasterData &data, int band,
                     const RasterRenderStyle &style,
                     double &outMin, double &outMax) const;
    void renderRgbRange(const RasterData &data,
                        int bandR, int bandG, int bandB,
                        const RasterRenderStyle &style,
                        double &outMin, double &outMax) const;
    void clearCache();

private:
    struct BandStats {
        const RasterData *data = nullptr;
        int band = -1;
        RasterData::DataType dataType = RasterData::DataType::Invalid;
        int width = 0;
        int height = 0;
        double min = 0.0;
        double max = 1.0;
        std::vector<double> sortedSamples;
    };

    struct RgbStats {
        const RasterData *data = nullptr;
        int bandR = -1;
        int bandG = -1;
        int bandB = -1;
        RasterData::DataType dataType = RasterData::DataType::Invalid;
        int width = 0;
        int height = 0;
        double min = 0.0;
        double max = 1.0;
        std::vector<double> sortedSamples;
    };

    const BandStats &bandStats(const RasterData &data, int band) const;
    const RgbStats &rgbStats(const RasterData &data,
                             int bandR, int bandG, int bandB) const;
    void computeRange(const RasterData &data, int band,
                      double &outMin, double &outMax) const;
    void computeRenderRange(const RasterData &data, int band,
                            const RasterRenderStyle &style,
                            double &outMin, double &outMax) const;
    void computeRgbRenderRange(const RasterData &data,
                               int bandR, int bandG, int bandB,
                               const RasterRenderStyle &style,
                               double &outMin, double &outMax) const;

    mutable std::vector<BandStats> m_bandStatsCache;
    mutable std::vector<RgbStats> m_rgbStatsCache;
};

#endif // RASTERRENDERER_H
