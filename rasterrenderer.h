#ifndef RASTERRENDERER_H
#define RASTERRENDERER_H

#include "layerstyle.h"
#include "rasterdata.h"
#include <QImage>

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

private:
    void computeRange(const RasterData &data, int band,
                      double &outMin, double &outMax) const;
    void computeRenderRange(const RasterData &data, int band,
                            const RasterRenderStyle &style,
                            double &outMin, double &outMax) const;
};

#endif // RASTERRENDERER_H
