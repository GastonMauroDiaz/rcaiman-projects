#include "histogramdock.h"
#include "imagelayer.h"
#include "rasterdata.h"
#include "rasterrenderer.h"

#include <QPainter>
#include <QPaintEvent>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {
constexpr int HistogramBins = 256;

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
}

HistogramWidget::HistogramWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(160);
    setMouseTracking(true);
    setToolTip(QStringLiteral(
        "RGB histograms are overlaid on a shared raw-value axis. "
        "This is not an average. Manual range is shared by R, G, and B."));
}

void HistogramWidget::setLayer(ImageLayer *layer)
{
    m_layer = layer;
    rebuild();
}

void HistogramWidget::rebuild()
{
    m_series.clear();

    if (!m_layer || m_layer->rasterData().isNull()) {
        update();
        return;
    }

    const RasterRenderStyle style = m_layer->style().render;
    if (style.mode == RasterRenderMode::Rgb) {
        addSeries(QStringLiteral("R"), QColor(220, 70, 70), style.redBand);
        addSeries(QStringLiteral("G"), QColor(70, 180, 90), style.greenBand);
        addSeries(QStringLiteral("B"), QColor(80, 120, 230), style.blueBand);
    } else {
        addSeries(QStringLiteral("Band %1").arg(style.band + 1),
                  QColor(QStringLiteral("#ffaa00")), style.band);
    }

    rebuildBins();
    update();
}

void HistogramWidget::addSeries(const QString &label,
                                const QColor &color,
                                int band)
{
    const RasterData &data = m_layer->rasterData();
    const int clampedBand = qBound(0, band, qMax(0, data.bands() - 1));
    RasterRenderer renderer;

    Series series;
    series.label = label;
    series.color = color;
    series.band = clampedBand;
    series.bins.fill(0, HistogramBins);
    renderer.rawRange(data, clampedBand, series.rawMin, series.rawMax);
    renderer.renderRange(data, clampedBand, m_layer->style().render,
                         series.renderMin, series.renderMax);

    m_series.append(series);
}

void HistogramWidget::rebuildBins()
{
    if (!m_layer || m_series.isEmpty())
        return;

    m_displayMin = m_series.first().rawMin;
    m_displayMax = m_series.first().rawMax;
    for (const Series &series : m_series) {
        m_displayMin = std::min(m_displayMin, series.rawMin);
        m_displayMax = std::max(m_displayMax, series.rawMax);
    }
    if (m_displayMax <= m_displayMin)
        m_displayMax = m_displayMin + 1.0;

    const RasterData &data = m_layer->rasterData();
    const double range = m_displayMax - m_displayMin;
    if (range <= 0.0)
        return;

    for (Series &series : m_series)
        series.bins.fill(0);

    for (Series &series : m_series) {
        const int band = qBound(0, series.band, qMax(0, data.bands() - 1));
        for (int row = 0; row < data.height(); ++row) {
            for (int col = 0; col < data.width(); ++col) {
                bool ok = false;
                const double value =
                    data.sample(band, row, col).toDouble(&ok);
                if (!ok || !std::isfinite(value))
                    continue;

                const double normalized =
                    normalizedPosition(value, m_displayMin, m_displayMax);
                const int bin = qBound(
                    0,
                    static_cast<int>(normalized * (HistogramBins - 1)),
                    HistogramBins - 1);
                ++series.bins[bin];
            }
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
    if (!m_layer || !m_layer->style().render.manualRangeEnabled
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
    if (!m_layer || m_dragHandle == Handle::None || m_series.isEmpty())
        return;

    const double epsilon = (m_displayMax - m_displayMin) / 1000000.0;
    const double minGap = epsilon > 0.0 ? epsilon : 1.0e-9;
    RasterRenderStyle style = m_layer->style().render;
    double value = std::clamp(valueForX(x), m_displayMin, m_displayMax);

    if (m_dragHandle == Handle::Minimum) {
        value = std::min(value, style.manualMax - minGap);
        m_layer->setManualRange(value, style.manualMax);
    } else if (m_dragHandle == Handle::Maximum) {
        value = std::max(value, style.manualMin + minGap);
        m_layer->setManualRange(style.manualMin, value);
    }
}

void HistogramWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), palette().base());

    const QRect textRect = rect().adjusted(8, 6, -8, -8);
    const QString summary = summaryText();
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
                const double xNorm = static_cast<double>(i)
                    / static_cast<double>(series.bins.size() - 1);
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

    if (m_layer && m_layer->style().render.manualRangeEnabled) {
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
    setWidget(m_widget);
}

void HistogramDock::setLayer(ImageLayer *layer)
{
    m_widget->setLayer(layer);
}
