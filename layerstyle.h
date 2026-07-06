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

#ifndef LAYERSTYLE_H
#define LAYERSTYLE_H

#include <QColor>
#include <QString>

enum class ColorRamp {
    Gray,
    RedBlue,
    GreenMagenta,
    Rainbow,
    Categorical
};

enum class RasterRenderMode {
    SingleBand,
    Rgb
};

enum class StretchMode {
    None,
    Linear,
    Gamma
};

enum class PointSymbol {
    Circle,
    Cross
};

struct PointLayerStyle
{
    QColor color = Qt::yellow;
    double size = 6.0;
    PointSymbol symbol = PointSymbol::Circle;
    QString labelField;
    double labelSize = 10.0;
};

struct RasterRenderStyle
{
    RasterRenderMode mode = RasterRenderMode::SingleBand;
    ColorRamp colorRamp = ColorRamp::Gray;
    StretchMode stretchMode = StretchMode::Linear;
    bool tailTrim = true;
    bool trimLeftTail = true;
    bool trimRightTail = true;
    double trimPercent = 2.0;
    bool manualRangeEnabled = false;
    double manualMin = 0.0;
    double manualMax = 1.0;
    double gamma = 2.2;
    int band = 0;
    int redBand = 0;
    int greenBand = 1;
    int blueBand = 2;
};

struct ImageLayerStyle
{
    double opacity = 1.0;
    RasterRenderStyle render;
};

#endif // LAYERSTYLE_H
