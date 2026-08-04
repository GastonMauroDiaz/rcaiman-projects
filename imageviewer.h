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

#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMutex>
#include <QMouseEvent>
#include <QPointF>
#include <QPointer>
#include <QRubberBand>
#include <QResizeEvent>
#include <QSet>
#include <QWheelEvent>

class QGraphicsScene;
class Layer;
class LayerManager;
class ImageLayer;
class PointLayer;
class QPainter;

enum class ZoomCursorDirection {
    In,
    Out
};

struct PixelInfo {
    int row = -1;
    int col = -1;
    int bandCount = 0;
    QVector<QVariant> bandValues;
};

class ImageViewer : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ImageViewer(QWidget *parent = nullptr);

    void setImage(const QString &fileName);
    void fitLoadedImage();
    QPointF viewCenter() const;
    QTransform viewTransform() const { return transform(); }
    void restoreView(const QTransform &transform, const QPointF &center);
    void zoomToPointIndices(PointLayer *layer, const QSet<int> &indices);
    void setPanModeEnabled(bool enabled);
    bool panModeEnabled() const;
    void setZoomModeEnabled(bool enabled);
    bool zoomModeEnabled() const;
    double zoomPercent() const;
    void setZoomPercent(double percent);

    void setPointEditEnabled(bool enabled);
    bool pointEditEnabled() const { return m_pointEditEnabled; }
    PointLayer *editingPointLayer() const { return m_editingPointLayer; }
    bool setMaskEditEnabled(bool enabled, QString *errorMessage = nullptr);
    bool maskEditEnabled() const { return m_maskEditEnabled; }
    ImageLayer *editingMaskLayer() const;
    bool cancelMaskEdit(QString *errorMessage = nullptr);
    void setMaskBrushSize(int size);
    int maskBrushSize() const { return m_maskBrushSize; }
    int maskBrushMaximum() const;

    LayerManager *layerManager() const { return m_layerManager; }
    ImageLayer *imageLayer() const { return m_imageLayer; }
    QString primarySourcePath() const { return m_primarySourcePath; }
    void setPrimarySourcePath(const QString &path) { m_primarySourcePath = path; }

signals:
    void pixelInfoChanged(const PixelInfo &info);
    void closeRequested();
    void reloadRequested();
    void activatedByClick();
    void imageLayerFileDropped(const QString &path);
    void pointLayerFileDropped(const QString &path);
    void zoomPercentChanged(double percent);
    void maskBrushSizeChanged(int size);
    void maskConnectedRegionInverted(int pixelCount);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

public:
    static void setSyncEnabled(bool enabled);
    static bool syncEnabled();
    static void setSyncZoomEnabled(bool enabled);
    static bool syncZoomEnabled();

private slots:
    void onActiveLayerChanged(Layer *layer);

private:
    void fitImageToView();
    void updateDragMode();
    void syncToSiblings(int dx, int dy);
    bool panInteractionActive() const;
    bool zoomInteractionActive() const;
    bool handleSpacePanKeyEvent(QEvent *event);
    bool handleTemporaryZoomKeyEvent(QEvent *event);
    bool handleResetZoomKeyEvent(QEvent *event);
    bool handleMaskBrushSizeKeyEvent(QKeyEvent *event);
    void setPointItemsInteractionBlocked(bool blocked);
    void applyDragZoom(const QPoint &pos);
    bool paintMaskAtViewportPos(const QPoint &pos, bool erase);
    bool paintMaskStrokeToViewportPos(const QPoint &pos, bool erase);
    bool paintMaskLineToViewportPos(const QPoint &pos, bool erase);
    void finishMaskStroke();
    void syncTransformToSiblings();
    void emitZoomPercentChanged();
    PointLayer *boxSelectionPointLayer() const;
    void beginPointBoxSelection(const QPoint &pos);
    void updatePointBoxSelection(const QPoint &pos);
    void finishPointBoxSelection(const QPoint &pos);
    void setZoomCursorDirection(ZoomCursorDirection direction);
    void updateAutoBackground();

    bool m_panModeEnabled = false;
    bool m_spacePanActive = false;
    bool m_zoomModeEnabled = false;
    bool m_temporaryZoomActive = false;
    bool m_zoomDragging = false;
    bool m_pointBoxSelecting = false;
    bool m_autoFitImage = false;
    bool m_pointEditEnabled = false;
    bool m_maskEditEnabled = false;
    bool m_maskPainting = false;
    bool m_maskPaintingErase = false;
    bool m_hasLastMaskPaintScenePos = false;
    bool m_hasMaskLineAnchorScenePos = false;
    bool m_maskBrushPreviewVisible = false;
    int m_maskBrushSize = 9;
    int m_maskBrushRepeatDirection = 0;
    int m_maskBrushRepeatCount = 0;
    PixelInfo m_lastPixelInfo;
    QPoint m_lastMousePos;
    QGraphicsScene *m_scene = nullptr;
    LayerManager *m_layerManager = nullptr;
    ImageLayer *m_imageLayer = nullptr;
    QString m_primarySourcePath;
    QPointer<PointLayer> m_editingPointLayer;
    QPointer<ImageLayer> m_editingMaskLayer;
    QPointF m_lastMaskPaintScenePos;
    QPointF m_maskLineAnchorScenePos;
    QPoint m_zoomDragLastPos;
    ZoomCursorDirection m_zoomCursorDirection = ZoomCursorDirection::In;
    QPoint m_pointBoxOrigin;
    QRect m_pointBoxRect;
    Qt::KeyboardModifiers m_pointBoxModifiers;
    QPointer<QRubberBand> m_pointBoxRubberBand;
    static bool ms_syncEnabled;
    static bool ms_syncZoomEnabled;
    bool m_syncingScroll = false;
    QMutex m_syncMutex;
};

#endif // IMAGEVIEWER_H
