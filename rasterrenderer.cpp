#include "rasterrenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

// ============================================================================
// Range computation — template definitions come FIRST
// ============================================================================

template<typename T>
static void computeMinMaxForBand(const RasterData &data, int band,
                                  double &outMin, double &outMax)
{
    const T *ptr = data.bandPointer<T>(band);
    if (!ptr) {
        outMin = 0.0;
        outMax = 1.0;
        return;
    }

    const qsizetype n = static_cast<qsizetype>(data.width()) * data.height();
    double mn = std::numeric_limits<double>::infinity();
    double mx = -std::numeric_limits<double>::infinity();

    for (qsizetype i = 0; i < n; ++i) {
        const double v = static_cast<double>(ptr[i]);
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }

    outMin = mn;
    outMax = mx;
}

// ============================================================================
// Typed RGB rendering — templates come FIRST
// ============================================================================

template<typename T>
static void renderRgbLines(const RasterData &data,
                           int bandR, int bandG, int bandB,
                           double minR, double maxR,
                           double minG, double maxG,
                           double minB, double maxB,
                           QImage &image)
{
    const T *r = data.bandPointer<T>(bandR);
    const T *g = data.bandPointer<T>(bandG);
    const T *b = data.bandPointer<T>(bandB);
    if (!r || !g || !b)
        return;

    const double rangeR = maxR - minR;
    const double rangeG = maxG - minG;
    const double rangeB = maxB - minB;

    uchar *bits = image.bits();
    const int bytesPerLine = image.bytesPerLine();

    for (int y = 0; y < data.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(bits + y * bytesPerLine);
        for (int x = 0; x < data.width(); ++x) {
            const qsizetype idx = static_cast<qsizetype>(y) * data.width() + x;
            double vr = (static_cast<double>(r[idx]) - minR) / rangeR;
            double vg = (static_cast<double>(g[idx]) - minG) / rangeG;
            double vb = (static_cast<double>(b[idx]) - minB) / rangeB;
            vr = std::clamp(vr, 0.0, 1.0);
            vg = std::clamp(vg, 0.0, 1.0);
            vb = std::clamp(vb, 0.0, 1.0);
            line[x] = qRgb(static_cast<int>(vr * 255.0 + 0.5),
                           static_cast<int>(vg * 255.0 + 0.5),
                           static_cast<int>(vb * 255.0 + 0.5));
        }
    }
}

template<typename T>
static void renderGrayLines(const RasterData &data,
                            int band,
                            double minVal, double maxVal,
                            QImage &image)
{
    const T *ptr = data.bandPointer<T>(band);
    if (!ptr)
        return;

    const double range = maxVal - minVal;
    uchar *bits = image.bits();
    const int bytesPerLine = image.bytesPerLine();

    for (int y = 0; y < data.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(bits + y * bytesPerLine);
        for (int x = 0; x < data.width(); ++x) {
            const qsizetype idx = static_cast<qsizetype>(y) * data.width() + x;
            double v = (static_cast<double>(ptr[idx]) - minVal) / range;
            v = std::clamp(v, 0.0, 1.0);
            const int gray = static_cast<int>(v * 255.0 + 0.5);
            line[x] = qRgb(gray, gray, gray);
        }
    }
}

// ============================================================================
// Render dispatch: RGB
// ============================================================================

QImage RasterRenderer::renderRgb(const RasterData &data,
                                 int bandR, int bandG, int bandB,
                                 double minR, double maxR,
                                 double minG, double maxG,
                                 double minB, double maxB) const
{
    if (data.isNull())
        return {};

    const int w = data.width();
    const int h = data.height();

    const int bR = std::clamp(bandR, 0, data.bands() - 1);
    const int bG = std::clamp(bandG, 0, data.bands() - 1);
    const int bB = std::clamp(bandB, 0, data.bands() - 1);

    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(0);

    switch (data.dataType()) {
    case RasterData::DataType::UInt8:
        renderRgbLines<uint8_t>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, image);
        break;
    case RasterData::DataType::UInt16:
        renderRgbLines<uint16_t>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, image);
        break;
    case RasterData::DataType::Int16:
        renderRgbLines<int16_t>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, image);
        break;
    case RasterData::DataType::UInt32:
        renderRgbLines<uint32_t>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, image);
        break;
    case RasterData::DataType::Int32:
        renderRgbLines<int32_t>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, image);
        break;
    case RasterData::DataType::Float32:
        renderRgbLines<float>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, image);
        break;
    case RasterData::DataType::Float64:
        renderRgbLines<double>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, image);
        break;
    default:
        break;
    }

    return image;
}

QImage RasterRenderer::renderSingle(const RasterData &data,
                                    int band,
                                    double minVal, double maxVal) const
{
    if (data.isNull())
        return {};

    const int w = data.width();
    const int h = data.height();
    const int b = std::clamp(band, 0, data.bands() - 1);

    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(0);

    switch (data.dataType()) {
    case RasterData::DataType::UInt8:
        renderGrayLines<uint8_t>(data, b, minVal, maxVal, image);
        break;
    case RasterData::DataType::UInt16:
        renderGrayLines<uint16_t>(data, b, minVal, maxVal, image);
        break;
    case RasterData::DataType::Int16:
        renderGrayLines<int16_t>(data, b, minVal, maxVal, image);
        break;
    case RasterData::DataType::UInt32:
        renderGrayLines<uint32_t>(data, b, minVal, maxVal, image);
        break;
    case RasterData::DataType::Int32:
        renderGrayLines<int32_t>(data, b, minVal, maxVal, image);
        break;
    case RasterData::DataType::Float32:
        renderGrayLines<float>(data, b, minVal, maxVal, image);
        break;
    case RasterData::DataType::Float64:
        renderGrayLines<double>(data, b, minVal, maxVal, image);
        break;
    default:
        break;
    }

    return image;
}

QImage RasterRenderer::renderAuto(const RasterData &data) const
{
    if (data.isNull())
        return {};

    if (data.bands() >= 3) {
        double min0, max0, min1, max1, min2, max2;
        computeRange(data, 0, min0, max0);
        computeRange(data, 1, min1, max1);
        computeRange(data, 2, min2, max2);
        return renderRgb(data, 0, 1, 2,
                         min0, max0, min1, max1, min2, max2);
    }

    double mn, mx;
    computeRange(data, 0, mn, mx);
    return renderSingle(data, 0, mn, mx);
}

void RasterRenderer::computeRange(const RasterData &data, int band,
                                   double &outMin, double &outMax) const
{
    const int b = std::clamp(band, 0, data.bands() - 1);

    switch (data.dataType()) {
    case RasterData::DataType::UInt8:
        computeMinMaxForBand<uint8_t>(data, b, outMin, outMax);
        break;
    case RasterData::DataType::UInt16:
        computeMinMaxForBand<uint16_t>(data, b, outMin, outMax);
        break;
    case RasterData::DataType::Int16:
        computeMinMaxForBand<int16_t>(data, b, outMin, outMax);
        break;
    case RasterData::DataType::UInt32:
        computeMinMaxForBand<uint32_t>(data, b, outMin, outMax);
        break;
    case RasterData::DataType::Int32:
        computeMinMaxForBand<int32_t>(data, b, outMin, outMax);
        break;
    case RasterData::DataType::Float32:
        computeMinMaxForBand<float>(data, b, outMin, outMax);
        break;
    case RasterData::DataType::Float64:
        computeMinMaxForBand<double>(data, b, outMin, outMax);
        break;
    default:
        outMin = 0.0;
        outMax = 1.0;
        return;
    }

    if (std::abs(outMax - outMin) < 1e-10)
        outMax = outMin + 1.0;
}