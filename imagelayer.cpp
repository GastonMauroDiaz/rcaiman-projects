#include "imagelayer.h"
#include "tiffreader.h"

#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QMessageBox>
#include <QWidget>
#include <QtGlobal>

ImageLayer::ImageLayer(const QString &filePath, QObject *parent)
    : Layer(QFileInfo(filePath).fileName(), filePath, parent)
{
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

void ImageLayer::setStyle(const ImageLayerStyle &style)
{
    const bool opacityChanged = !qFuzzyCompare(m_style.opacity, style.opacity);
    const bool renderChanged =
        m_style.render.mode != style.render.mode ||
        m_style.render.colorRamp != style.render.colorRamp ||
        m_style.render.stretchMode != style.render.stretchMode ||
        m_style.render.tailTrim != style.render.tailTrim ||
        !qFuzzyCompare(m_style.render.trimPercent, style.render.trimPercent) ||
        !qFuzzyCompare(m_style.render.gamma, style.render.gamma) ||
        m_style.render.band != style.render.band ||
        m_style.render.redBand != style.render.redBand ||
        m_style.render.greenBand != style.render.greenBand ||
        m_style.render.blueBand != style.render.blueBand;

    if (!opacityChanged && !renderChanged)
        return;

    m_style = style;
    if (renderChanged)
        refreshRenderedImage();
    else
        applyStyleToItem();
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

void ImageLayer::setTrimPercent(double percent)
{
    RasterRenderStyle next = m_style.render;
    next.trimPercent = qBound(0.0, percent, 49.0);
    setRenderStyle(next);
}

void ImageLayer::setGamma(double gamma)
{
    RasterRenderStyle next = m_style.render;
    next.gamma = qBound(1.0, gamma, 4.0);
    setRenderStyle(next);
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
