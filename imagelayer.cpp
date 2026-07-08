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

#include "imagelayer.h"
#include "tiffreader.h"

#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QMessageBox>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTimer>
#include <QWidget>
#include <QtGlobal>

#include <utility>

namespace {

class BlendedPixmapItem : public QGraphicsPixmapItem
{
public:
    explicit BlendedPixmapItem(const QPixmap &pixmap)
        : QGraphicsPixmapItem(pixmap)
    {
    }

    void setLayerOpacity(double opacity)
    {
        const double bounded = qBound(0.0, opacity, 1.0);
        if (qFuzzyCompare(m_opacity, bounded))
            return;
        m_opacity = bounded;
        update();
    }

    void setBlendMode(ImageBlendMode blendMode)
    {
        if (m_blendMode == blendMode)
            return;
        m_blendMode = blendMode;
        update();
    }

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget) override
    {
        Q_UNUSED(option);
        Q_UNUSED(widget);

        const QPixmap currentPixmap = pixmap();
        if (currentPixmap.isNull())
            return;

        painter->save();
        painter->setOpacity(m_opacity);
        if (m_blendMode == ImageBlendMode::Multiply)
            painter->setCompositionMode(QPainter::CompositionMode_Multiply);
        else if (m_blendMode == ImageBlendMode::Maximum)
            painter->setCompositionMode(QPainter::CompositionMode_Lighten);
        else
            painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter->drawPixmap(offset(), currentPixmap);
        painter->restore();
    }

private:
    double m_opacity = 1.0;
    ImageBlendMode m_blendMode = ImageBlendMode::Normal;
};

}

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
    auto *item = new BlendedPixmapItem(pixmap);
    scene->addItem(item);
    m_pixmapItem = item;
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

    RasterRenderer previewRenderer;
    const QImage image = previewRenderer.render(reloaded, m_style.render);
    if (image.isNull()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not render reloaded image.");
        return false;
    }

    m_rasterData = std::move(reloaded);
    m_renderer.clearCache();
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
    const bool blendModeChanged = m_style.blendMode != style.blendMode;
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

    if (!opacityChanged && !blendModeChanged && !renderChanged)
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

void ImageLayer::setBlendMode(ImageBlendMode blendMode)
{
    ImageLayerStyle next = m_style;
    next.blendMode = blendMode;
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
    if (!m_pixmapItem)
        return;

    if (auto *item = dynamic_cast<BlendedPixmapItem*>(m_pixmapItem)) {
        item->setLayerOpacity(m_style.opacity);
        item->setBlendMode(m_style.blendMode);
    } else {
        m_pixmapItem->setOpacity(m_style.opacity);
    }
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
