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
#include "tiffwriter.h"

#include <QFile>
#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QMessageBox>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <QtGlobal>

#include <cmath>
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

ImageLayer::~ImageLayer()
{
    removeBinaryMaskBackup();
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
    return !m_binaryMaskEditMode;
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

bool ImageLayer::isEditableBinaryMask() const
{
    const QFileInfo info(sourcePath());
    const QString suffix = info.suffix().toLower();
    return info.isFile()
        && (suffix == QStringLiteral("tif") || suffix == QStringLiteral("tiff"))
        && m_rasterData.dataType() == RasterData::DataType::UInt8
        && m_rasterData.bands() == 1
        && m_rasterData.width() > 0
        && m_rasterData.height() > 0;
}

bool ImageLayer::setBinaryMaskEditMode(bool enabled,
                                       QString *errorMessage)
{
    if (m_binaryMaskEditMode == enabled)
        return true;

    if (enabled) {
        if (!isEditableBinaryMask()) {
            if (errorMessage)
                *errorMessage = QStringLiteral(
                    "Only single-band UInt8 TIFF masks can be edited.");
            return false;
        }
        if (!ensureBinaryMaskBackup(errorMessage))
            return false;
        m_binaryMaskEditMode = true;
        return true;
    }

    removeBinaryMaskBackup();
    m_binaryMaskEditMode = false;
    return true;
}

bool ImageLayer::cancelBinaryMaskEdit(QString *errorMessage)
{
    if (m_binaryMaskBackupPath.isEmpty()
        || !QFile::exists(m_binaryMaskBackupPath)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No mask edit backup is available.");
        return false;
    }

    const QString src = sourcePath();
    QFile::remove(src);
    if (!QFile::copy(m_binaryMaskBackupPath, src)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not restore mask backup.");
        return false;
    }

    QString loadError;
    RasterData restored = TiffReader::load(src, &loadError);
    if (restored.isNull()) {
        if (errorMessage)
            *errorMessage = loadError.isEmpty()
                ? QStringLiteral("Could not reload restored mask.")
                : loadError;
        return false;
    }

    m_rasterData = std::move(restored);
    m_renderer.clearCache();
    ++m_dataRevision;
    m_binaryMaskEditMode = false;
    removeBinaryMaskBackup();
    refreshRenderedImageNow();
    emit dataChanged();
    return true;
}

bool ImageLayer::paintBinaryMaskAtScenePos(const QPointF &scenePos,
                                           int brushSize,
                                           bool erase)
{
    if (!m_binaryMaskEditMode || !isEditableBinaryMask() || !m_pixmapItem)
        return false;

    const QPointF itemPos = m_pixmapItem->mapFromScene(scenePos);
    const int centerCol = static_cast<int>(std::floor(itemPos.x()));
    const int centerRow = static_cast<int>(std::floor(itemPos.y()));
    if (centerRow < 0 || centerRow >= m_rasterData.height()
        || centerCol < 0 || centerCol >= m_rasterData.width()) {
        return false;
    }

    brushSize = qBound(
        1,
        brushSize,
        qMax(1, qMax(m_rasterData.width(), m_rasterData.height())));
    const double radius = qMax(0.5, brushSize / 2.0);
    const double radiusSquared = radius * radius;
    const int pixelRadius = static_cast<int>(std::ceil(radius));
    const quint8 value = erase ? 0 : 1;
    bool changed = false;

    quint8 *pixels = reinterpret_cast<quint8 *>(m_rasterData.rawData().data());
    const int width = m_rasterData.width();
    const int height = m_rasterData.height();
    for (int row = qMax(0, centerRow - pixelRadius);
         row <= qMin(height - 1, centerRow + pixelRadius);
         ++row) {
        for (int col = qMax(0, centerCol - pixelRadius);
             col <= qMin(width - 1, centerCol + pixelRadius);
             ++col) {
            const double dx = (col + 0.5) - (centerCol + 0.5);
            const double dy = (row + 0.5) - (centerRow + 0.5);
            if (dx * dx + dy * dy > radiusSquared)
                continue;

            quint8 &pixel = pixels[static_cast<qsizetype>(row) * width + col];
            if (pixel == value)
                continue;
            pixel = value;
            changed = true;
        }
    }

    if (changed) {
        m_renderer.clearCache();
        ++m_dataRevision;
        refreshRenderedImageNow();
        emit dataChanged();
    }
    return changed;
}

bool ImageLayer::invertConnectedBinaryMaskRegionAtScenePos(
    const QPointF &scenePos,
    int *changedPixelCount,
    QString *errorMessage)
{
    if (changedPixelCount)
        *changedPixelCount = 0;
    if (!m_binaryMaskEditMode || !isEditableBinaryMask() || !m_pixmapItem) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No editable binary mask is active.");
        return false;
    }

    const QPointF itemPos = m_pixmapItem->mapFromScene(scenePos);
    const int seedCol = static_cast<int>(std::floor(itemPos.x()));
    const int seedRow = static_cast<int>(std::floor(itemPos.y()));
    const int width = m_rasterData.width();
    const int height = m_rasterData.height();
    if (seedRow < 0 || seedRow >= height
        || seedCol < 0 || seedCol >= width) {
        return false;
    }

    quint8 *pixels = reinterpret_cast<quint8 *>(m_rasterData.rawData().data());
    if (!pixels) {
        if (errorMessage)
            *errorMessage = QStringLiteral("The mask pixels could not be edited.");
        return false;
    }

    bool hasOne = false;
    bool has255 = false;
    const qsizetype pixelCount =
        static_cast<qsizetype>(width) * height;
    for (qsizetype offset = 0; offset < pixelCount; ++offset) {
        const quint8 value = pixels[offset];
        if (value == 1) {
            hasOne = true;
        } else if (value == 255) {
            has255 = true;
        } else if (value != 0) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "The active mask is not binary.\n\n"
                    "Binary masks must contain only 0/1 or 0/255 values.");
            }
            return false;
        }
        if (hasOne && has255) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "The active mask is not binary.\n\n"
                    "Binary masks must contain only 0/1 or 0/255 values.");
            }
            return false;
        }
    }

    const int foregroundValue = has255 ? 255 : 1;
    const int seedIndex = seedRow * width + seedCol;
    const quint8 seedValue = pixels[seedIndex];
    const quint8 replacementValue = seedValue == 0
        ? static_cast<quint8>(foregroundValue)
        : static_cast<quint8>(0);
    if (seedValue == replacementValue)
        return false;

    QVector<int> stack;
    stack.reserve(1024);
    stack.append(seedIndex);
    pixels[seedIndex] = replacementValue;
    int changed = 1;

    while (!stack.isEmpty()) {
        const int index = stack.takeLast();
        const int row = index / width;
        const int col = index - row * width;

        auto tryNeighbor = [&](int neighborIndex) {
            if (pixels[neighborIndex] != seedValue)
                return;
            pixels[neighborIndex] = replacementValue;
            stack.append(neighborIndex);
            ++changed;
        };

        if (row > 0)
            tryNeighbor(index - width);
        if (row + 1 < height)
            tryNeighbor(index + width);
        if (col > 0)
            tryNeighbor(index - 1);
        if (col + 1 < width)
            tryNeighbor(index + 1);
    }

    if (changedPixelCount)
        *changedPixelCount = changed;
    m_renderer.clearCache();
    ++m_dataRevision;
    refreshRenderedImageNow();
    emit dataChanged();
    return true;
}

bool ImageLayer::saveBinaryMask(QString *errorMessage)
{
    if (!m_binaryMaskEditMode || !isEditableBinaryMask()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No editable binary mask is active.");
        return false;
    }
    return TiffWriter::write(sourcePath(), m_rasterData, errorMessage);
}

bool ImageLayer::ensureBinaryMaskBackup(QString *errorMessage)
{
    const QString src = sourcePath();
    QFileInfo info(src);
    if (!info.isFile()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Mask source file does not exist.");
        return false;
    }

    m_binaryMaskBackupPath = src + QStringLiteral(".bak");
    if (QFile::exists(m_binaryMaskBackupPath)
        && !QFile::remove(m_binaryMaskBackupPath)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not remove old mask backup.");
        m_binaryMaskBackupPath.clear();
        return false;
    }

    if (!QFile::copy(src, m_binaryMaskBackupPath)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not create mask backup.");
        m_binaryMaskBackupPath.clear();
        return false;
    }
    return true;
}

void ImageLayer::removeBinaryMaskBackup()
{
    if (!m_binaryMaskBackupPath.isEmpty()
        && QFile::exists(m_binaryMaskBackupPath)) {
        QFile::remove(m_binaryMaskBackupPath);
    }
    m_binaryMaskBackupPath.clear();
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
