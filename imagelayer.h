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

#ifndef IMAGELAYER_H
#define IMAGELAYER_H

#include "layer.h"
#include "layerstyle.h"
#include "rasterdata.h"
#include "rasterrenderer.h"

#include <QPointF>

class QTimer;

class ImageLayer : public Layer
{
    Q_OBJECT

public:
    explicit ImageLayer(const QString &filePath, QObject *parent = nullptr);
    ~ImageLayer() override;

    const QVector<QGraphicsItem*> &buildItems(QGraphicsScene *scene) override;

    const RasterData &rasterData() const { return m_rasterData; }
    RasterRenderer &renderer() { return m_renderer; }
    QGraphicsPixmapItem *pixmapItem() const { return m_pixmapItem; }
    ImageLayerStyle style() const { return m_style; }
    quint64 dataRevision() const { return m_dataRevision; }
    bool supportsReloadFromDisk() const override;
    bool reloadFromDisk(QString *errorMessage = nullptr) override;
    bool isEditableBinaryMask() const;
    bool binaryMaskEditMode() const { return m_binaryMaskEditMode; }
    bool setBinaryMaskEditMode(bool enabled, QString *errorMessage = nullptr);
    bool cancelBinaryMaskEdit(QString *errorMessage = nullptr);
    bool paintBinaryMaskAtScenePos(const QPointF &scenePos,
                                   int brushSize,
                                   bool erase);
    bool invertConnectedBinaryMaskRegionAtScenePos(
        const QPointF &scenePos,
        int *changedPixelCount = nullptr,
        QString *errorMessage = nullptr);
    bool saveBinaryMask(QString *errorMessage = nullptr);
    void setStyle(const ImageLayerStyle &style);
    void setOpacity(double opacity);
    void setBlendMode(ImageBlendMode blendMode);
    void setRenderStyle(const RasterRenderStyle &renderStyle);
    void setColorRamp(ColorRamp ramp);
    void setStretchMode(StretchMode mode);
    void setTailTrim(bool enabled);
    void setTrimLeftTail(bool enabled);
    void setTrimRightTail(bool enabled);
    void setTrimPercent(double percent);
    void setManualRangeEnabled(bool enabled);
    void setManualRange(double minValue, double maxValue);
    void setManualRange(bool enabled, double minValue, double maxValue);
    void setManualRangeDeferred(double minValue, double maxValue);
    void setGamma(double gamma);
    void setGammaDeferred(double gamma);
    void setRenderBand(int band);
    void setRenderMode(RasterRenderMode mode);
    void setRgbBands(int redBand, int greenBand, int blueBand);
    void refreshRenderedImageNow();

private:
    void setStyleInternal(const ImageLayerStyle &style, bool deferRender);
    void scheduleRenderRefresh();
    void applyStyleToItem();
    void refreshRenderedImage();
    bool ensureBinaryMaskBackup(QString *errorMessage = nullptr);
    void removeBinaryMaskBackup();

    RasterData m_rasterData;
    RasterRenderer m_renderer;
    ImageLayerStyle m_style;
    QGraphicsPixmapItem *m_pixmapItem = nullptr;
    quint64 m_dataRevision = 0;
    QTimer *m_renderRefreshTimer = nullptr;
    bool m_renderRefreshPending = false;
    bool m_binaryMaskEditMode = false;
    QString m_binaryMaskBackupPath;
};

#endif // IMAGELAYER_H
