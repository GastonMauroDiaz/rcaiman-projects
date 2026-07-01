#include "imagelayer.h"
#include "tiffreader.h"

#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QMessageBox>
#include <QTimer>
#include <QWidget>
#include <QtGlobal>

#include <utility>

ImageLayer::ImageLayer(const QString &filePath, QObject *parent)
    : Layer(QFileInfo(filePath).fileName(), filePath, parent)
    , m_renderRefreshTimer(new QTimer(this))
{
    m_renderRefreshTimer->setSingleShot(true);
    m_renderRefreshTimer->setInterval(60);
    connect(m_renderRefreshTimer, &QTimer::timeout,
            this, &ImageLayer::refreshRenderedImageNow);

    QString errorMessage;
    m_rasterData = TiffReader::load(filePath, &errorMessage);
    if (m_rasterData.isNull()) {
        qWarning() << "ImageLayer: could not load" << filePath << errorMessage;
    }
}

const QVector<QGraphicsItem*> &ImageLayer::buildItems(QGraphicsScene *scene)
{
    if (m_itemsBuilt)
        return m_items;

    if (m_rasterData.isNull())
        return m_items;

    const QImage image = m_renderer.render(m_rasterData, m_style.render);
    if (image.isNull()) {
        qWarning() << "ImageLayer: could not render" << sourcePath();
        return m_items;
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    m_pixmapItem = scene->addPixmap(pixmap);
    m_pixmapItem->setVisible(isVisible());
    applyStyleToItem();
    m_items.append(m_pixmapItem);

    m_itemsBuilt = true;
    return m_items;
}

bool ImageLayer::supportsReloadFromDisk() const
{
    return true;
}

bool ImageLayer::reloadFromDisk(QString *errorMessage)
{
    QString loadError;
    RasterData reloaded = TiffReader::load(sourcePath(), &loadError);
    if (reloaded.isNull()) {
        if (errorMessage)
            *errorMessage = loadError.isEmpty()
                ? QStringLiteral("Could not reload image.")
                : loadError;
        return false;
    }

    const QImage image = m_renderer.render(reloaded, m_style.render);
    if (image.isNull()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not render reloaded image.");
        return false;
    }

    m_rasterData = std::move(reloaded);
    ++m_dataRevision;
    if (m_pixmapItem) {
        m_pixmapItem->setPixmap(QPixmap::fromImage(image));
        applyStyleToItem();
    }

    emit dataChanged();
    return true;
}

void ImageLayer::setStyle(const ImageLayerStyle &style)
{
    setStyleInternal(style, false);
}

void ImageLayer::setStyleInternal(const ImageLayerStyle &style,
                                  bool deferRender)
{
    const bool opacityChanged = !qFuzzyCompare(m_style.opacity, style.opacity);
    const bool renderChanged =
        m_style.render.mode != style.render.mode ||
        m_style.render.colorRamp != style.render.colorRamp ||
        m_style.render.stretchMode != style.render.stretchMode ||
        m_style.render.tailTrim != style.render.tailTrim ||
        m_style.render.trimLeftTail != style.render.trimLeftTail ||
        m_style.render.trimRightTail != style.render.trimRightTail ||
        !qFuzzyCompare(m_style.render.trimPercent, style.render.trimPercent) ||
        m_style.render.manualRangeEnabled != style.render.manualRangeEnabled ||
        !qFuzzyCompare(m_style.render.manualMin, style.render.manualMin) ||
        !qFuzzyCompare(m_style.render.manualMax, style.render.manualMax) ||
        !qFuzzyCompare(m_style.render.gamma, style.render.gamma) ||
        m_style.render.band != style.render.band ||
        m_style.render.redBand != style.render.redBand ||
        m_style.render.greenBand != style.render.greenBand ||
        m_style.render.blueBand != style.render.blueBand;

    if (!opacityChanged && !renderChanged)
        return;

    m_style = style;
    if (renderChanged) {
        if (deferRender)
            scheduleRenderRefresh();
        else
            refreshRenderedImageNow();
    } else {
        applyStyleToItem();
    }
    emit styleChanged();
}

void ImageLayer::setOpacity(double opacity)
{
    ImageLayerStyle next = m_style;
    next.opacity = qBound(0.0, opacity, 1.0);
    setStyle(next);
}

void ImageLayer::setRenderStyle(const RasterRenderStyle &renderStyle)
{
    ImageLayerStyle next = m_style;
    next.render = renderStyle;
    setStyle(next);
}

void ImageLayer::setColorRamp(ColorRamp ramp)
{
    RasterRenderStyle next = m_style.render;
    next.colorRamp = ramp;
    setRenderStyle(next);
}

void ImageLayer::setStretchMode(StretchMode mode)
{
    RasterRenderStyle next = m_style.render;
    next.stretchMode = mode;
    setRenderStyle(next);
}

void ImageLayer::setTailTrim(bool enabled)
{
    RasterRenderStyle next = m_style.render;
    next.tailTrim = enabled;
    setRenderStyle(next);
}

void ImageLayer::setTrimLeftTail(bool enabled)
{
    RasterRenderStyle next = m_style.render;
    next.trimLeftTail = enabled;
    setRenderStyle(next);
}

void ImageLayer::setTrimRightTail(bool enabled)
{
    RasterRenderStyle next = m_style.render;
    next.trimRightTail = enabled;
    setRenderStyle(next);
}

void ImageLayer::setTrimPercent(double percent)
{
    RasterRenderStyle next = m_style.render;
    next.trimPercent = qBound(0.0, percent, 49.0);
    ImageLayerStyle style = m_style;
    style.render = next;
    setStyleInternal(style, true);
}

void ImageLayer::setManualRangeEnabled(bool enabled)
{
    RasterRenderStyle next = m_style.render;
    next.manualRangeEnabled = enabled;
    setRenderStyle(next);
}

void ImageLayer::setManualRange(double minValue, double maxValue)
{
    RasterRenderStyle next = m_style.render;
    next.manualMin = minValue;
    next.manualMax = maxValue;
    setRenderStyle(next);
}

void ImageLayer::setManualRangeDeferred(double minValue, double maxValue)
{
    RasterRenderStyle next = m_style.render;
    next.manualMin = minValue;
    next.manualMax = maxValue;
    ImageLayerStyle style = m_style;
    style.render = next;
    setStyleInternal(style, true);
}

void ImageLayer::setManualRange(bool enabled, double minValue, double maxValue)
{
    RasterRenderStyle next = m_style.render;
    next.manualRangeEnabled = enabled;
    next.manualMin = minValue;
    next.manualMax = maxValue;
    setRenderStyle(next);
}

void ImageLayer::setGamma(double gamma)
{
    RasterRenderStyle next = m_style.render;
    next.gamma = qBound(1.0, gamma, 4.0);
    ImageLayerStyle style = m_style;
    style.render = next;
    setStyleInternal(style, true);
}

void ImageLayer::setGammaDeferred(double gamma)
{
    setGamma(gamma);
}

void ImageLayer::setRenderBand(int band)
{
    RasterRenderStyle next = m_style.render;
    next.band = qBound(0, band, qMax(0, m_rasterData.bands() - 1));
    setRenderStyle(next);
}

void ImageLayer::setRenderMode(RasterRenderMode mode)
{
    RasterRenderStyle next = m_style.render;
    next.mode = mode;
    setRenderStyle(next);
}

void ImageLayer::setRgbBands(int redBand, int greenBand, int blueBand)
{
    RasterRenderStyle next = m_style.render;
    const int maxBand = qMax(0, m_rasterData.bands() - 1);
    next.redBand = qBound(0, redBand, maxBand);
    next.greenBand = qBound(0, greenBand, maxBand);
    next.blueBand = qBound(0, blueBand, maxBand);
    setRenderStyle(next);
}

void ImageLayer::applyStyleToItem()
{
    if (m_pixmapItem)
        m_pixmapItem->setOpacity(m_style.opacity);
}

void ImageLayer::scheduleRenderRefresh()
{
    m_renderRefreshPending = true;
    if (m_renderRefreshTimer)
        m_renderRefreshTimer->start();
}

void ImageLayer::refreshRenderedImageNow()
{
    if (m_renderRefreshTimer)
        m_renderRefreshTimer->stop();
    m_renderRefreshPending = false;
    refreshRenderedImage();
}

void ImageLayer::refreshRenderedImage()
{
    if (!m_pixmapItem)
        return;

    const QImage image = m_renderer.render(m_rasterData, m_style.render);
    if (image.isNull()) {
        qWarning() << "ImageLayer: could not render" << sourcePath();
        return;
    }

    m_pixmapItem->setPixmap(QPixmap::fromImage(image));
    applyStyleToItem();
}
