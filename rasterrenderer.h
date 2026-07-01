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

    const BandStats &bandStats(const RasterData &data, int band) const;
    void computeRange(const RasterData &data, int band,
                      double &outMin, double &outMax) const;
    void computeRenderRange(const RasterData &data, int band,
                            const RasterRenderStyle &style,
                            double &outMin, double &outMax) const;

    mutable std::vector<BandStats> m_bandStatsCache;
};

#endif // RASTERRENDERER_H
