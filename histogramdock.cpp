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

#include "histogramdock.h"
#include "imagelayer.h"
#include "rasterdata.h"
#include "rasterrenderer.h"

#include <QApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr int HistogramBins = 256;
constexpr qsizetype MaxHistogramSamples = 262144;

QString formatValue(double value)
{
    return QString::number(value, 'g', 6);
}

double normalizedPosition(double value, double minValue, double maxValue)
{
    const double range = maxValue - minValue;
    if (range <= 0.0 || !std::isfinite(value))
        return 0.0;
    return std::clamp((value - minValue) / range, 0.0, 1.0);
}

template<typename T>
void collectSampledValues(const RasterData &data,
                          int band,
                          QVector<double> *values)
{
    const T *ptr = data.bandPointer<T>(band);
    if (!ptr || !values)
        return;

    const qsizetype total =
        static_cast<qsizetype>(data.width()) * data.height();
    if (total <= MaxHistogramSamples) {
        values->reserve(static_cast<int>(total));
        for (qsizetype i = 0; i < total; ++i) {
            const double value = static_cast<double>(ptr[i]);
            if (std::isfinite(value))
                values->append(value);
        }
        return;
    }

    const double aspect = static_cast<double>(data.width())
        / static_cast<double>(qMax(1, data.height()));
    int sampleRows = qBound(
        1,
        static_cast<int>(std::sqrt(
            static_cast<double>(MaxHistogramSamples) / qMax(0.000001, aspect))),
        data.height());
    int sampleCols = qBound(
        1,
        static_cast<int>(
            static_cast<double>(MaxHistogramSamples) / sampleRows),
        data.width());

    while (static_cast<qsizetype>(sampleRows) * sampleCols
           > MaxHistogramSamples) {
        if (sampleCols >= sampleRows && sampleCols > 1)
            --sampleCols;
        else if (sampleRows > 1)
            --sampleRows;
        else
            break;
    }

    values->reserve(sampleRows * sampleCols);
    const double rowScale =
        static_cast<double>(data.height()) / static_cast<double>(sampleRows);
    const double colScale =
        static_cast<double>(data.width()) / static_cast<double>(sampleCols);
    for (int sampleRow = 0; sampleRow < sampleRows; ++sampleRow) {
        const int row = qBound(
            0,
            static_cast<int>((sampleRow + 0.5) * rowScale),
            data.height() - 1);
        const qsizetype rowOffset =
            static_cast<qsizetype>(row) * data.width();
        for (int sampleCol = 0; sampleCol < sampleCols; ++sampleCol) {
            const int col = qBound(
                0,
                static_cast<int>((sampleCol + 0.5) * colScale),
                data.width() - 1);
            const double value = static_cast<double>(ptr[rowOffset + col]);
            if (std::isfinite(value))
                values->append(value);
        }
    }
}

QVector<double> sampledValuesForBand(const RasterData &data, int band)
{
    QVector<double> values;
    switch (data.dataType()) {
    case RasterData::DataType::UInt8:
        collectSampledValues<quint8>(data, band, &values);
        break;
    case RasterData::DataType::UInt16:
        collectSampledValues<quint16>(data, band, &values);
        break;
    case RasterData::DataType::Int16:
        collectSampledValues<qint16>(data, band, &values);
        break;
    case RasterData::DataType::UInt32:
        collectSampledValues<quint32>(data, band, &values);
        break;
    case RasterData::DataType::Int32:
        collectSampledValues<qint32>(data, band, &values);
        break;
    case RasterData::DataType::Float32:
        collectSampledValues<float>(data, band, &values);
        break;
    case RasterData::DataType::Float64:
        collectSampledValues<double>(data, band, &values);
        break;
    default:
        break;
    }
    std::sort(values.begin(), values.end());
    return values;
}

}

HistogramWidget::HistogramWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(160);
    setMouseTracking(true);
    setToolTip(QStringLiteral(
        "The histogram is an approximation based on regular grid sampling "
        "over the image plane."
        ));
}

void HistogramWidget::setLayer(ImageLayer *layer)
{
    if (m_layer != layer) {
        m_series.clear();
        m_displayMin = 0.0;
        m_displayMax = 1.0;
        m_dragHandle = Handle::None;
    }
    m_layer = layer;
    rebuild();
}

void HistogramWidget::rebuild()
{
    m_paused = histogramPaused();
    if (m_paused) {
        setComputing(false);
        update();
        return;
    }

    if (!m_layer || m_layer->rasterData().isNull()) {
        m_series.clear();
        setComputing(false);
        update();
        return;
    }

    const CacheKey key = currentCacheKey();
    if (loadFromCache(key)) {
        updateRenderRanges();
        setComputing(false);
        update();
        return;
    }

    setComputing(true);
    update();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if (m_rebuildQueued)
        return;

    m_rebuildQueued = true;
    QTimer::singleShot(0, this, [this]() {
        rebuildNow();
    });
}

void HistogramWidget::rebuildNow()
{
    m_rebuildQueued = false;
    m_paused = histogramPaused();
    if (m_paused) {
        setComputing(false);
        update();
        return;
    }

    if (!m_layer || m_layer->rasterData().isNull()) {
        m_series.clear();
        setComputing(false);
        update();
        return;
    }

    const CacheKey key = currentCacheKey();
    if (loadFromCache(key)) {
        updateRenderRanges();
        setComputing(false);
        update();
        return;
    }

    QVector<Series> series;
    const RasterRenderStyle style = m_layer->style().render;
    if (style.mode == RasterRenderMode::Rgb) {
        addSeries(&series, QStringLiteral("R"),
                  QColor(220, 70, 70), style.redBand);
        addSeries(&series, QStringLiteral("G"),
                  QColor(70, 180, 90), style.greenBand);
        addSeries(&series, QStringLiteral("B"),
                  QColor(80, 120, 230), style.blueBand);
    } else {
        addSeries(&series, QStringLiteral("Band %1").arg(style.band + 1),
                  QColor(QStringLiteral("#ffaa00")), style.band);
    }

    updateRenderRanges(&series);
    double displayMin = 0.0;
    double displayMax = 1.0;
    rebuildBins(&series, &displayMin, &displayMax);
    storeInCache(key, series, displayMin, displayMax);

    m_series = series;
    m_displayMin = displayMin;
    m_displayMax = displayMax;
    setComputing(false);
    update();
}

void HistogramWidget::addSeries(QVector<Series> *seriesList,
                                const QString &label,
                                const QColor &color,
                                int band)
{
    if (!seriesList || !m_layer)
        return;

    const RasterData &data = m_layer->rasterData();
    const int clampedBand = qBound(0, band, qMax(0, data.bands() - 1));

    Series series;
    series.label = label;
    series.color = color;
    series.band = clampedBand;
    series.bins.fill(0, HistogramBins);
    series.sortedSamples = sampledValuesForBand(data, clampedBand);
    if (!series.sortedSamples.isEmpty()) {
        series.rawMin = series.sortedSamples.first();
        series.rawMax = series.sortedSamples.last();
        if (std::abs(series.rawMax - series.rawMin) < 1.0e-10)
            series.rawMax = series.rawMin + 1.0;
    }

    seriesList->append(series);
}

void HistogramWidget::rebuildBins(QVector<Series> *seriesList,
                                  double *displayMin,
                                  double *displayMax) const
{
    if (!seriesList || seriesList->isEmpty() || !displayMin || !displayMax)
        return;

    *displayMin = seriesList->first().rawMin;
    *displayMax = seriesList->first().rawMax;
    for (const Series &series : *seriesList) {
        *displayMin = std::min(*displayMin, series.rawMin);
        *displayMax = std::max(*displayMax, series.rawMax);
    }
    if (*displayMax <= *displayMin)
        *displayMax = *displayMin + 1.0;

    for (Series &series : *seriesList) {
        series.bins.fill(0);
        for (double value : std::as_const(series.sortedSamples)) {
            const double normalized =
                normalizedPosition(value, *displayMin, *displayMax);
            const int bin = qBound(
                0,
                static_cast<int>(std::floor(normalized * HistogramBins)),
                HistogramBins - 1);
            ++series.bins[bin];
        }
    }
}

HistogramWidget::CacheKey HistogramWidget::currentCacheKey() const
{
    CacheKey key;
    if (!m_layer)
        return key;

    const RasterData &data = m_layer->rasterData();
    const int maxBand = qMax(0, data.bands() - 1);
    const RasterRenderStyle style = m_layer->style().render;
    key.layer = m_layer;
    key.dataRevision = m_layer->dataRevision();
    key.mode = static_cast<int>(style.mode);
    if (style.mode == RasterRenderMode::Rgb) {
        key.band0 = qBound(0, style.redBand, maxBand);
        key.band1 = qBound(0, style.greenBand, maxBand);
        key.band2 = qBound(0, style.blueBand, maxBand);
    } else {
        key.band0 = qBound(0, style.band, maxBand);
    }
    return key;
}

bool HistogramWidget::cacheKeyMatches(const CacheKey &left,
                                      const CacheKey &right) const
{
    return left.layer == right.layer
        && left.dataRevision == right.dataRevision
        && left.mode == right.mode
        && left.band0 == right.band0
        && left.band1 == right.band1
        && left.band2 == right.band2;
}

bool HistogramWidget::loadFromCache(const CacheKey &key)
{
    for (const CacheEntry &entry : std::as_const(m_cache)) {
        if (!entry.key.layer)
            continue;
        if (!cacheKeyMatches(entry.key, key))
            continue;
        m_series = entry.series;
        m_displayMin = entry.displayMin;
        m_displayMax = entry.displayMax;
        m_computing = false;
        return true;
    }
    return false;
}

void HistogramWidget::storeInCache(const CacheKey &key,
                                   const QVector<Series> &series,
                                   double displayMin,
                                   double displayMax)
{
    for (CacheEntry &entry : m_cache) {
        if (cacheKeyMatches(entry.key, key)) {
            entry.series = series;
            entry.displayMin = displayMin;
            entry.displayMax = displayMax;
            trimCache();
            return;
        }
    }

    CacheEntry entry;
    entry.key = key;
    entry.series = series;
    entry.displayMin = displayMin;
    entry.displayMax = displayMax;
    m_cache.append(entry);
    trimCache();
}

void HistogramWidget::trimCache()
{
    for (int i = m_cache.size() - 1; i >= 0; --i) {
        if (!m_cache[i].key.layer)
            m_cache.removeAt(i);
    }

    constexpr int MaximumCacheEntries = 16;
    while (m_cache.size() > MaximumCacheEntries)
        m_cache.removeFirst();
}

bool HistogramWidget::histogramPaused() const
{
    return m_layer && m_layer->binaryMaskEditMode();
}

void HistogramWidget::setComputing(bool computing)
{
    if (m_computing == computing)
        return;

    m_computing = computing;
    if (m_computing) {
        emit computationStarted(QStringLiteral(
            "Computing histogram from sampled image values..."));
    } else {
        emit computationFinished(QStringLiteral("Histogram ready"));
    }
}

void HistogramWidget::updateRenderRanges()
{
    updateRenderRanges(&m_series);
}

void HistogramWidget::updateRenderRanges(QVector<Series> *seriesList) const
{
    if (!m_layer)
        return;

    const RasterRenderStyle style = m_layer->style().render;
    RasterRenderer &renderer = m_layer->renderer();
    if (style.mode == RasterRenderMode::Rgb) {
        double sharedMin = 0.0;
        double sharedMax = 1.0;
        renderer.renderRgbRange(
            m_layer->rasterData(),
            style.redBand,
            style.greenBand,
            style.blueBand,
            style,
            sharedMin,
            sharedMax);
        for (Series &series : *seriesList) {
            series.renderMin = sharedMin;
            series.renderMax = sharedMax;
        }
        return;
    }

    for (Series &series : *seriesList) {
        renderer.renderRange(
            m_layer->rasterData(),
            series.band,
            style,
            series.renderMin,
            series.renderMax);

        if (series.renderMax <= series.renderMin
            || !std::isfinite(series.renderMin)
            || !std::isfinite(series.renderMax)) {
            series.renderMin = std::isfinite(series.renderMin)
                ? series.renderMin : 0.0;
            series.renderMax = series.renderMin + 1.0;
        }
    }
}

QString HistogramWidget::summaryText() const
{
    if (m_series.isEmpty())
        return QStringLiteral("No image layer selected");

    QStringList lines;
    for (const Series &series : m_series) {
        lines.append(
            QStringLiteral("%1 raw %2 - %3; render %4 - %5")
                .arg(series.label,
                     formatValue(series.rawMin),
                     formatValue(series.rawMax),
                     formatValue(series.renderMin),
                     formatValue(series.renderMax)));
    }
    return lines.join('\n');
}

QRect HistogramWidget::plotRectForSummary(const QString &summary) const
{
    const QRect textRect = rect().adjusted(8, 6, -8, -8);
    const int textHeight = fontMetrics().boundingRect(
        QRect(0, 0, textRect.width(), 200),
        Qt::TextWordWrap, summary).height();

    return rect().adjusted(8, textHeight + 16, -8, -8);
}

int HistogramWidget::xForValue(double value) const
{
    if (m_series.isEmpty())
        return 0;

    const QRect plotRect = plotRectForSummary(summaryText());
    const double normalized =
        normalizedPosition(value, m_displayMin, m_displayMax);
    return plotRect.left()
        + qRound(normalized * static_cast<double>(plotRect.width() - 1));
}

double HistogramWidget::valueForX(int x) const
{
    if (m_series.isEmpty())
        return 0.0;

    const QRect plotRect = plotRectForSummary(summaryText());
    const double normalized = plotRect.width() <= 1
        ? 0.0
        : std::clamp(
              static_cast<double>(x - plotRect.left())
                  / static_cast<double>(plotRect.width() - 1),
              0.0, 1.0);
    return m_displayMin + normalized * (m_displayMax - m_displayMin);
}

HistogramWidget::Handle HistogramWidget::handleAtPosition(
    const QPoint &position) const
{
    if (m_paused || !m_layer || !m_layer->style().render.manualRangeEnabled
        || m_series.isEmpty()) {
        return Handle::None;
    }

    const QString summary = summaryText();
    const QRect plotRect = plotRectForSummary(summary);
    if (!plotRect.adjusted(-6, 0, 6, 0).contains(position))
        return Handle::None;

    const RasterRenderStyle style = m_layer->style().render;
    const int minX = xForValue(style.manualMin);
    const int maxX = xForValue(style.manualMax);
    const int minDistance = std::abs(position.x() - minX);
    const int maxDistance = std::abs(position.x() - maxX);
    constexpr int grabDistance = 6;

    if (minDistance <= grabDistance && minDistance <= maxDistance)
        return Handle::Minimum;
    if (maxDistance <= grabDistance)
        return Handle::Maximum;
    return Handle::None;
}

void HistogramWidget::updateCursorForPosition(const QPoint &position)
{
    setCursor(handleAtPosition(position) == Handle::None
                  ? Qt::ArrowCursor
                  : Qt::SizeHorCursor);
}

void HistogramWidget::applyHandleDrag(int x)
{
    if (m_paused || !m_layer || m_dragHandle == Handle::None
        || m_series.isEmpty()) {
        return;
    }

    const double epsilon = (m_displayMax - m_displayMin) / 1000000.0;
    const double minGap = epsilon > 0.0 ? epsilon : 1.0e-9;
    RasterRenderStyle style = m_layer->style().render;
    double value = std::clamp(valueForX(x), m_displayMin, m_displayMax);

    if (m_dragHandle == Handle::Minimum) {
        value = std::min(value, style.manualMax - minGap);
        m_layer->setManualRangeDeferred(value, style.manualMax);
    } else if (m_dragHandle == Handle::Maximum) {
        value = std::max(value, style.manualMin + minGap);
        m_layer->setManualRangeDeferred(style.manualMin, value);
    }
}

void HistogramWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), palette().base());

    if (m_computing) {
        painter.setPen(palette().text().color());
        painter.drawText(rect().adjusted(8, 6, -8, -8),
                         Qt::AlignLeft | Qt::AlignTop,
                         QStringLiteral("Computing histogram..."));
        return;
    }

    const bool paused = m_paused || histogramPaused();
    const QRect textRect = rect().adjusted(8, 6, -8, -8);
    const QString summary = paused
        ? (m_series.isEmpty()
               ? QStringLiteral("Histogram paused while editing mask.")
               : QStringLiteral(
                     "Histogram paused while editing mask.\n%1").arg(
                     summaryText()))
        : summaryText();
    const QRect plotRect = plotRectForSummary(summary);
    if (plotRect.height() < 40 || m_series.isEmpty()) {
        painter.setPen(palette().text().color());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop, summary);
        return;
    }

    painter.setPen(QPen(palette().mid().color(), 1));
    painter.drawRect(plotRect.adjusted(0, 0, -1, -1));

    quint64 maxCount = 0;
    for (const Series &series : m_series) {
        for (quint64 count : series.bins)
            maxCount = std::max(maxCount, count);
    }

    const double maxLog = std::log1p(static_cast<double>(maxCount));
    if (maxLog > 0.0) {
        for (const Series &series : m_series) {
            QColor color = series.color;
            color.setAlpha(m_series.size() > 1 ? 105 : 180);
            painter.setPen(QPen(color, 1));
            for (int i = 0; i < series.bins.size(); ++i) {
                const double xNorm = (static_cast<double>(i) + 0.5)
                    / static_cast<double>(series.bins.size());
                const int x = plotRect.left()
                    + qRound(xNorm * static_cast<double>(plotRect.width() - 1));
                const double yNorm =
                    std::log1p(static_cast<double>(series.bins[i])) / maxLog;
                const int y = plotRect.bottom()
                    - qRound(yNorm * static_cast<double>(plotRect.height() - 1));
                painter.drawLine(x, plotRect.bottom(), x, y);
            }
        }
    }

    for (const Series &series : m_series) {
        QColor color = series.color;
        color.setAlpha(220);
        const bool manual =
            m_layer && m_layer->style().render.manualRangeEnabled;
        painter.setPen(QPen(color, manual ? 1 : 2));
        const double minNorm =
            normalizedPosition(series.renderMin, m_displayMin, m_displayMax);
        const double maxNorm =
            normalizedPosition(series.renderMax, m_displayMin, m_displayMax);
        const int minX = plotRect.left()
            + qRound(minNorm * static_cast<double>(plotRect.width() - 1));
        const int maxX = plotRect.left()
            + qRound(maxNorm * static_cast<double>(plotRect.width() - 1));
        painter.drawLine(minX, plotRect.top(), minX, plotRect.bottom());
        painter.drawLine(maxX, plotRect.top(), maxX, plotRect.bottom());
    }

    if (!paused && m_layer && m_layer->style().render.manualRangeEnabled) {
        const RasterRenderStyle style = m_layer->style().render;
        painter.setPen(QPen(QColor(35, 35, 35), 3));
        painter.drawLine(xForValue(style.manualMin), plotRect.top(),
                         xForValue(style.manualMin), plotRect.bottom());
        painter.drawLine(xForValue(style.manualMax), plotRect.top(),
                         xForValue(style.manualMax), plotRect.bottom());
        painter.setPen(QPen(QColor(255, 255, 255), 1));
        painter.drawLine(xForValue(style.manualMin), plotRect.top(),
                         xForValue(style.manualMin), plotRect.bottom());
        painter.drawLine(xForValue(style.manualMax), plotRect.top(),
                         xForValue(style.manualMax), plotRect.bottom());
    }

    painter.setPen(palette().text().color());
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop, summary);
}

void HistogramWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragHandle != Handle::None) {
        applyHandleDrag(event->position().toPoint().x());
        return;
    }

    updateCursorForPosition(event->position().toPoint());
}

void HistogramWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    m_dragHandle = handleAtPosition(event->position().toPoint());
    if (m_dragHandle != Handle::None) {
        setCursor(Qt::SizeHorCursor);
        applyHandleDrag(event->position().toPoint().x());
        event->accept();
    }
}

void HistogramWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragHandle != Handle::None) {
        m_dragHandle = Handle::None;
        if (m_layer)
            m_layer->refreshRenderedImageNow();
        updateCursorForPosition(event->position().toPoint());
        event->accept();
    }
}

void HistogramWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (m_dragHandle == Handle::None)
        unsetCursor();
}

HistogramDock::HistogramDock(QWidget *parent)
    : QDockWidget(QStringLiteral("Histogram"), parent)
{
    setObjectName(QStringLiteral("dockHistogram"));
    m_widget = new HistogramWidget(this);
    connect(m_widget, &HistogramWidget::computationStarted,
            this, &HistogramDock::computationStarted);
    connect(m_widget, &HistogramWidget::computationFinished,
            this, &HistogramDock::computationFinished);
    setWidget(m_widget);
}

void HistogramDock::setLayer(ImageLayer *layer)
{
    m_widget->setLayer(layer);
}
