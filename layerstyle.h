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
    double trimPercent = 2.0;
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
