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

#include "rasterrenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

constexpr qsizetype MaxRenderRangeSamples = 262144;

// ============================================================================
// Range computation — template definitions come FIRST
// ============================================================================

template<typename T>
static void collectSampledValuesForBand(const RasterData &data, int band,
                                        std::vector<double> &values)
{
    const T *ptr = data.bandPointer<T>(band);
    if (!ptr)
        return;

    const qsizetype n = static_cast<qsizetype>(data.width()) * data.height();
    const qsizetype stride =
        std::max<qsizetype>(1, (n + MaxRenderRangeSamples - 1)
                                   / MaxRenderRangeSamples);
    values.reserve(static_cast<size_t>(std::min(n, MaxRenderRangeSamples)));

    for (qsizetype i = 0; i < n; i += stride) {
        const double v = static_cast<double>(ptr[i]);
        if (!std::isfinite(v))
            continue;
        values.push_back(v);
    }
}

static QRgb colorFromRamp(double value, ColorRamp ramp)
{
    value = std::clamp(value, 0.0, 1.0);
    if (ramp == ColorRamp::Rainbow) {
        const double h = value * 240.0;
        const double c = 1.0;
        const double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));

        double r = 0.0;
        double g = 0.0;
        double b = 0.0;
        if (h < 60.0) {
            r = c; g = x;
        } else if (h < 120.0) {
            r = x; g = c;
        } else if (h < 180.0) {
            g = c; b = x;
        } else {
            g = x; b = c;
        }

        return qRgb(static_cast<int>(r * 255.0 + 0.5),
                    static_cast<int>(g * 255.0 + 0.5),
                    static_cast<int>(b * 255.0 + 0.5));
    }

    if (ramp == ColorRamp::RedBlue) {
        return qRgb(static_cast<int>((1.0 - value) * 255.0 + 0.5),
                    0,
                    static_cast<int>(value * 255.0 + 0.5));
    }

    if (ramp == ColorRamp::GreenMagenta) {
        return qRgb(static_cast<int>(value * 255.0 + 0.5),
                    static_cast<int>((1.0 - value) * 180.0 + 0.5),
                    static_cast<int>(value * 255.0 + 0.5));
    }

    const int gray = static_cast<int>(value * 255.0 + 0.5);
    return qRgb(gray, gray, gray);
}

static QRgb categoricalColor(double value)
{
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));

    // SplitMix64 gives stable, well-separated colors without storing a palette.
    bits += 0x9e3779b97f4a7c15ULL;
    bits = (bits ^ (bits >> 30)) * 0xbf58476d1ce4e5b9ULL;
    bits = (bits ^ (bits >> 27)) * 0x94d049bb133111ebULL;
    bits ^= bits >> 31;

    const int hue = static_cast<int>(bits % 360ULL);
    return QColor::fromHsv(hue, 190, 230).rgb();
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
                           StretchMode stretchMode,
                           double gamma,
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
            if (stretchMode == StretchMode::Gamma) {
                const double exponent = 1.0 / std::clamp(gamma, 1.0, 4.0);
                vr = std::pow(vr, exponent);
                vg = std::pow(vg, exponent);
                vb = std::pow(vb, exponent);
            }
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

template<typename T>
static void renderRampLines(const RasterData &data,
                            int band,
                            double minVal, double maxVal,
                            ColorRamp ramp,
                            StretchMode stretchMode,
                            double gamma,
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
            const double rawValue = static_cast<double>(ptr[idx]);
            if (ramp == ColorRamp::Categorical) {
                line[x] = categoricalColor(rawValue);
                continue;
            }

            double v = range == 0.0 ? 0.0 : (rawValue - minVal) / range;
            v = std::clamp(v, 0.0, 1.0);
            if (stretchMode == StretchMode::Gamma)
                v = std::pow(v, 1.0 / std::clamp(gamma, 1.0, 4.0));
            line[x] = colorFromRamp(v, ramp);
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
                                 double minB, double maxB,
                                 StretchMode stretchMode,
                                 double gamma) const
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
        renderRgbLines<uint8_t>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, stretchMode, gamma, image);
        break;
    case RasterData::DataType::UInt16:
        renderRgbLines<uint16_t>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, stretchMode, gamma, image);
        break;
    case RasterData::DataType::Int16:
        renderRgbLines<int16_t>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, stretchMode, gamma, image);
        break;
    case RasterData::DataType::UInt32:
        renderRgbLines<uint32_t>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, stretchMode, gamma, image);
        break;
    case RasterData::DataType::Int32:
        renderRgbLines<int32_t>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, stretchMode, gamma, image);
        break;
    case RasterData::DataType::Float32:
        renderRgbLines<float>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, stretchMode, gamma, image);
        break;
    case RasterData::DataType::Float64:
        renderRgbLines<double>(data, bR, bG, bB, minR, maxR, minG, maxG, minB, maxB, stretchMode, gamma, image);
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

QImage RasterRenderer::render(const RasterData &data, const RasterRenderStyle &style) const
{
    if (data.isNull())
        return {};

    if (style.mode == RasterRenderMode::Rgb) {
        double minRgb = 0.0;
        double maxRgb = 1.0;
        computeRgbRenderRange(
            data, style.redBand, style.greenBand, style.blueBand,
            style, minRgb, maxRgb);
        return renderRgb(data, style.redBand, style.greenBand, style.blueBand,
                         minRgb, maxRgb, minRgb, maxRgb, minRgb, maxRgb,
                         style.stretchMode, style.gamma);
    }

    const int w = data.width();
    const int h = data.height();
    const int band = std::clamp(style.band, 0, data.bands() - 1);

    double minVal = 0.0;
    double maxVal = 1.0;
    computeRenderRange(data, band, style, minVal, maxVal);

    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(0);

    switch (data.dataType()) {
    case RasterData::DataType::UInt8:
        renderRampLines<uint8_t>(data, band, minVal, maxVal, style.colorRamp, style.stretchMode, style.gamma, image);
        break;
    case RasterData::DataType::UInt16:
        renderRampLines<uint16_t>(data, band, minVal, maxVal, style.colorRamp, style.stretchMode, style.gamma, image);
        break;
    case RasterData::DataType::Int16:
        renderRampLines<int16_t>(data, band, minVal, maxVal, style.colorRamp, style.stretchMode, style.gamma, image);
        break;
    case RasterData::DataType::UInt32:
        renderRampLines<uint32_t>(data, band, minVal, maxVal, style.colorRamp, style.stretchMode, style.gamma, image);
        break;
    case RasterData::DataType::Int32:
        renderRampLines<int32_t>(data, band, minVal, maxVal, style.colorRamp, style.stretchMode, style.gamma, image);
        break;
    case RasterData::DataType::Float32:
        renderRampLines<float>(data, band, minVal, maxVal, style.colorRamp, style.stretchMode, style.gamma, image);
        break;
    case RasterData::DataType::Float64:
        renderRampLines<double>(data, band, minVal, maxVal, style.colorRamp, style.stretchMode, style.gamma, image);
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
        RasterRenderStyle style;
        style.mode = RasterRenderMode::Rgb;
        style.stretchMode = StretchMode::None;
        double minRgb = 0.0;
        double maxRgb = 1.0;
        computeRgbRenderRange(data, 0, 1, 2, style, minRgb, maxRgb);
        return renderRgb(data, 0, 1, 2,
                         minRgb, maxRgb, minRgb, maxRgb, minRgb, maxRgb,
                         StretchMode::Linear, 1.0);
    }

    double mn, mx;
    computeRange(data, 0, mn, mx);
    return renderSingle(data, 0, mn, mx);
}

void RasterRenderer::rawRange(const RasterData &data, int band,
                              double &outMin, double &outMax) const
{
    computeRange(data, band, outMin, outMax);
}

void RasterRenderer::renderRange(const RasterData &data, int band,
                                 const RasterRenderStyle &style,
                                 double &outMin, double &outMax) const
{
    computeRenderRange(data, band, style, outMin, outMax);
}

void RasterRenderer::renderRgbRange(const RasterData &data,
                                    int bandR, int bandG, int bandB,
                                    const RasterRenderStyle &style,
                                    double &outMin, double &outMax) const
{
    computeRgbRenderRange(data, bandR, bandG, bandB, style, outMin, outMax);
}

void RasterRenderer::clearCache()
{
    m_bandStatsCache.clear();
    m_rgbStatsCache.clear();
}

const RasterRenderer::BandStats &RasterRenderer::bandStats(
    const RasterData &data, int band) const
{
    const int b = std::clamp(band, 0, data.bands() - 1);
    for (const BandStats &stats : m_bandStatsCache) {
        if (stats.data == &data
            && stats.band == b
            && stats.dataType == data.dataType()
            && stats.width == data.width()
            && stats.height == data.height()) {
            return stats;
        }
    }

    BandStats stats;
    stats.data = &data;
    stats.band = b;
    stats.dataType = data.dataType();
    stats.width = data.width();
    stats.height = data.height();

    switch (data.dataType()) {
    case RasterData::DataType::UInt8:
        collectSampledValuesForBand<uint8_t>(data, b, stats.sortedSamples);
        break;
    case RasterData::DataType::UInt16:
        collectSampledValuesForBand<uint16_t>(data, b, stats.sortedSamples);
        break;
    case RasterData::DataType::Int16:
        collectSampledValuesForBand<int16_t>(data, b, stats.sortedSamples);
        break;
    case RasterData::DataType::UInt32:
        collectSampledValuesForBand<uint32_t>(data, b, stats.sortedSamples);
        break;
    case RasterData::DataType::Int32:
        collectSampledValuesForBand<int32_t>(data, b, stats.sortedSamples);
        break;
    case RasterData::DataType::Float32:
        collectSampledValuesForBand<float>(data, b, stats.sortedSamples);
        break;
    case RasterData::DataType::Float64:
        collectSampledValuesForBand<double>(data, b, stats.sortedSamples);
        break;
    default:
        break;
    }

    if (!stats.sortedSamples.empty()) {
        std::sort(stats.sortedSamples.begin(), stats.sortedSamples.end());
        stats.min = stats.sortedSamples.front();
        stats.max = stats.sortedSamples.back();
        if (std::abs(stats.max - stats.min) < 1e-10)
            stats.max = stats.min + 1.0;
    }

    m_bandStatsCache.push_back(std::move(stats));
    constexpr size_t MaximumStatsEntries = 16;
    while (m_bandStatsCache.size() > MaximumStatsEntries)
        m_bandStatsCache.erase(m_bandStatsCache.begin());
    return m_bandStatsCache.back();
}

const RasterRenderer::RgbStats &RasterRenderer::rgbStats(
    const RasterData &data, int bandR, int bandG, int bandB) const
{
    const int bR = std::clamp(bandR, 0, data.bands() - 1);
    const int bG = std::clamp(bandG, 0, data.bands() - 1);
    const int bB = std::clamp(bandB, 0, data.bands() - 1);
    for (const RgbStats &stats : m_rgbStatsCache) {
        if (stats.data == &data
            && stats.bandR == bR
            && stats.bandG == bG
            && stats.bandB == bB
            && stats.dataType == data.dataType()
            && stats.width == data.width()
            && stats.height == data.height()) {
            return stats;
        }
    }

    bandStats(data, bR);
    bandStats(data, bG);
    bandStats(data, bB);
    const BandStats &statsR = bandStats(data, bR);
    const BandStats &statsG = bandStats(data, bG);
    const BandStats &statsB = bandStats(data, bB);

    RgbStats stats;
    stats.data = &data;
    stats.bandR = bR;
    stats.bandG = bG;
    stats.bandB = bB;
    stats.dataType = data.dataType();
    stats.width = data.width();
    stats.height = data.height();

    if (!statsR.sortedSamples.empty()
        && !statsG.sortedSamples.empty()
        && !statsB.sortedSamples.empty()) {
        stats.min = std::min(statsR.min, std::min(statsG.min, statsB.min));
        stats.max = std::max(statsR.max, std::max(statsG.max, statsB.max));
        stats.sortedSamples.reserve(statsR.sortedSamples.size()
                                    + statsG.sortedSamples.size()
                                    + statsB.sortedSamples.size());
        stats.sortedSamples.insert(stats.sortedSamples.end(),
                                   statsR.sortedSamples.begin(),
                                   statsR.sortedSamples.end());
        stats.sortedSamples.insert(stats.sortedSamples.end(),
                                   statsG.sortedSamples.begin(),
                                   statsG.sortedSamples.end());
        stats.sortedSamples.insert(stats.sortedSamples.end(),
                                   statsB.sortedSamples.begin(),
                                   statsB.sortedSamples.end());
        std::sort(stats.sortedSamples.begin(), stats.sortedSamples.end());
    }

    m_rgbStatsCache.push_back(std::move(stats));
    constexpr size_t MaximumStatsEntries = 16;
    while (m_rgbStatsCache.size() > MaximumStatsEntries)
        m_rgbStatsCache.erase(m_rgbStatsCache.begin());
    return m_rgbStatsCache.back();
}

void RasterRenderer::computeRange(const RasterData &data, int band,
                                   double &outMin, double &outMax) const
{
    const BandStats &stats = bandStats(data, band);
    if (stats.sortedSamples.empty()) {
        outMin = 0.0;
        outMax = 1.0;
        return;
    }
    outMin = stats.min;
    outMax = stats.max;
}

void RasterRenderer::computeRenderRange(const RasterData &data, int band,
                                        const RasterRenderStyle &style,
                                        double &outMin, double &outMax) const
{
    const int b = std::clamp(band, 0, data.bands() - 1);

    if (style.manualRangeEnabled) {
        outMin = style.manualMin;
        outMax = style.manualMax;
        if (!std::isfinite(outMin))
            outMin = 0.0;
        if (!std::isfinite(outMax))
            outMax = outMin + 1.0;
        if (outMax <= outMin)
            outMax = outMin + 1.0;
        return;
    }

    const BandStats &stats = bandStats(data, b);
    const std::vector<double> &values = stats.sortedSamples;

    if (values.empty()) {
        outMin = 0.0;
        outMax = 1.0;
        return;
    }

    if (!style.tailTrim
        || style.trimPercent <= 0.0
        || (!style.trimLeftTail && !style.trimRightTail)) {
        outMin = stats.min;
        outMax = stats.max;
    } else {
        const double trim = std::clamp(style.trimPercent, 0.0, 49.0) / 100.0;
        const double lastIndex = static_cast<double>(values.size() - 1);
        const size_t lowIndex =
            static_cast<size_t>(std::ceil(trim * lastIndex));
        const size_t highIndex =
            static_cast<size_t>(std::floor((1.0 - trim) * lastIndex));

        if (style.trimLeftTail && style.trimRightTail) {
            outMin = values[lowIndex];
            outMax = values[highIndex];
        } else if (style.trimLeftTail) {
            outMin = values[lowIndex];
            outMax = stats.max;
        } else {
            outMin = stats.min;
            outMax = values[highIndex];
        }
    }

    if (std::abs(outMax - outMin) < 1e-10)
        outMax = outMin + 1.0;
}

void RasterRenderer::computeRgbRenderRange(
    const RasterData &data,
    int bandR,
    int bandG,
    int bandB,
    const RasterRenderStyle &style,
    double &outMin,
    double &outMax) const
{
    if (data.isNull() || data.bands() <= 0) {
        outMin = 0.0;
        outMax = 1.0;
        return;
    }

    if (style.manualRangeEnabled) {
        outMin = style.manualMin;
        outMax = style.manualMax;
        if (!std::isfinite(outMin))
            outMin = 0.0;
        if (!std::isfinite(outMax))
            outMax = outMin + 1.0;
        if (outMax <= outMin)
            outMax = outMin + 1.0;
        return;
    }

    const RgbStats &stats = rgbStats(data, bandR, bandG, bandB);
    const std::vector<double> &values = stats.sortedSamples;

    if (values.empty()) {
        outMin = 0.0;
        outMax = 1.0;
        return;
    }

    if (!style.tailTrim
        || style.trimPercent <= 0.0
        || (!style.trimLeftTail && !style.trimRightTail)) {
        outMin = stats.min;
        outMax = stats.max;
    } else {
        const double trim = std::clamp(style.trimPercent, 0.0, 49.0) / 100.0;
        const double lastIndex = static_cast<double>(values.size() - 1);
        const size_t lowIndex =
            static_cast<size_t>(std::ceil(trim * lastIndex));
        const size_t highIndex =
            static_cast<size_t>(std::floor((1.0 - trim) * lastIndex));

        outMin = style.trimLeftTail ? values[lowIndex] : stats.min;
        outMax = style.trimRightTail ? values[highIndex] : stats.max;
    }

    if (!std::isfinite(outMin))
        outMin = 0.0;
    if (!std::isfinite(outMax))
        outMax = outMin + 1.0;
    if (std::abs(outMax - outMin) < 1e-10)
        outMax = outMin + 1.0;
}
