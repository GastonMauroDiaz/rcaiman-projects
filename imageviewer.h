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
#include <QList>
#include <QLineF>
#include <QMutex>
#include <QMouseEvent>
#include <QPointF>
#include <QPointer>
#include <QPolygonF>
#include <QRubberBand>
#include <QResizeEvent>
#include <QSet>
#include <QWheelEvent>

#include <array>

class QGraphicsScene;
class Layer;
class LayerManager;
class ImageLayer;
class PointLayer;
class QPainter;
class QDoubleSpinBox;
class QLabel;
class QHideEvent;
class QFormLayout;

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

struct CursorCoordinateInfo {
    bool valid = false;
    double x = 0.0;
    double y = 0.0;
    double yDown = 0.0;
    bool yAvailable = false;
    int row = -1;
    int col = -1;
};

struct MeasurementCircle {
    bool valid = false;
    bool unstable = false;
    QPointF center;
    double radius = 0;
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
    void setMeasureDistanceEnabled(bool enabled);
    bool measureDistanceEnabled() const { return m_measureDistanceEnabled; }
    bool measurementVisible() const { return m_measurementVisible; }
    QPointF measurementVertex(int vertex) const;
    void setMeasurementVertex(int vertex, const QPointF &position);
    double measurementSideLength(int side) const;
    double measurementAngleDegrees(int vertex) const;
    bool measurementVertexLocked(int vertex) const;
    void setMeasurementVertexLocked(int vertex, bool locked);
    bool measurementTranslationAllowed() const;
    bool canEditMeasurementSide(int side) const;
    bool setMeasurementSideLength(int side, double length);
    MeasurementCircle measurementCircumcircle() const;
    bool measurementCircleVisible() const { return m_measurementCircleVisible; }
    void setMeasurementCircleVisible(bool visible);
    double zoomPercent() const;
    void setZoomPercent(double percent);
    void setActivePanelIndicator(bool active);

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
    QList<ImageLayer*> imageLayers() const;
    ImageLayer *coordinateReferenceImage() const;
    void setCoordinateReferenceImage(ImageLayer *layer);
    QString primarySourcePath() const { return m_primarySourcePath; }
    void setPrimarySourcePath(const QString &path);

signals:
    void pixelInfoChanged(const PixelInfo &info);
    void cursorCoordinateInfoChanged(const CursorCoordinateInfo &info);
    void coordinateReferenceImagesChanged();
    void coordinateReferenceImageChanged();
    void closeRequested();
    void reloadRequested();
    void activatedByClick();
    void imageLayerFileDropped(const QString &path);
    void pointLayerFileDropped(const QString &path);
    void zoomPercentChanged(double percent);
    void maskBrushSizeChanged(int size);
    void maskConnectedRegionInverted(int pixelCount);
    void measurementChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void hideEvent(QHideEvent *event) override;
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
    enum class MeasurementDragTarget {
        None = -1,
        VertexA = 0,
        VertexB = 1,
        VertexC = 2,
        Triangle = 3
    };

    struct MeasurementLabel {
        QString text;
        QRectF rect;
        int vertex = -1;
        int side = -1;
    };

    void fitImageToView();
    void updateDragMode();
    void syncToSiblings(int dx, int dy);
    bool panInteractionActive() const;
    bool zoomInteractionActive() const;
    bool handleSpacePanKeyEvent(QEvent *event);
    bool handleTemporaryZoomKeyEvent(QEvent *event);
    bool handleZoomShortcutKeyEvent(QEvent *event);
    bool handleMeasureDistanceKeyEvent(QKeyEvent *event);
    bool handleMaskBrushSizeKeyEvent(QKeyEvent *event);
    void setSpacePanActive(bool active);
    void setSplitSpacePanActive(bool active);
    void setTemporaryZoomActive(bool active);
    void setSplitTemporaryZoomActive(bool active);
    void clearTemporaryInteractionState();
    void clearMeasurement();
    void initializeMeasurement(const QPoint &viewportPos);
    void translateMeasurement(const QPointF &delta);
    void moveMeasurementHandle(const QPoint &viewportPos);
    MeasurementDragTarget measurementEndpointAt(
        const QPoint &viewportPos, bool includeLocked = false) const;
    void updateMeasurementCursor(const QPoint &viewportPos);
    QPolygonF measurementScreenPolygon() const;
    QList<MeasurementLabel> measurementLabels() const;
    int measurementLabelVertexAt(const QPoint &viewportPos) const;
    int measurementLabelSideAt(const QPoint &viewportPos) const;
    int measurementLockAt(const QPoint &viewportPos) const;
    QRectF measurementLockRect(const MeasurementLabel &label) const;
    QPointF measurementCoordinates(const QPointF &scenePosition) const;
    QPointF measurementScenePosition(const QPointF &position) const;
    void drawMeasurementCircumcircle(QPainter *painter) const;
    void openMeasurementSideEditor(int side, const QPoint &viewportPos);
    void positionMeasurementEditor(const QPoint &viewportPos);
    void openMeasurementVertexEditor(int vertex, const QPoint &viewportPos);
    void closeMeasurementEditor();
    void commitMeasurementEditor();
    void setPointItemsInteractionBlocked(bool blocked);
    void applyDragZoom(const QPoint &pos);
    bool paintMaskAtViewportPos(const QPoint &pos, bool erase);
    bool paintMaskStrokeToViewportPos(const QPoint &pos, bool erase);
    bool paintMaskLineToViewportPos(const QPoint &pos, bool erase);
    void finishMaskStroke();
    void syncTransformToSiblings();
    void emitZoomPercentChanged();
    void emitCursorCoordinateInfo(const QPoint &viewportPos);
    PointLayer *boxSelectionPointLayer() const;
    void beginPointBoxSelection(const QPoint &pos);
    void updatePointBoxSelection(const QPoint &pos);
    void finishPointBoxSelection(const QPoint &pos);
    void setZoomCursorDirection(ZoomCursorDirection direction);
    void updateAutoBackground();
    void updateEmptyStateOverlay();

    bool m_panModeEnabled = false;
    bool m_spacePanActive = false;
    bool m_zoomModeEnabled = false;
    bool m_temporaryZoomActive = false;
    bool m_zoomDragging = false;
    bool m_measureDistanceEnabled = false;
    bool m_measureDragging = false;
    bool m_measurementVisible = false;
    MeasurementDragTarget m_measureDragTarget =
        MeasurementDragTarget::None;
    bool m_pointBoxSelecting = false;
    bool m_autoFitImage = false;
    bool m_pointEditEnabled = false;
    bool m_maskEditEnabled = false;
    bool m_maskPainting = false;
    bool m_maskPaintingErase = false;
    bool m_hasLastMaskPaintScenePos = false;
    bool m_hasMaskLineAnchorScenePos = false;
    bool m_maskBrushPreviewVisible = false;
    bool m_cursorInside = false;
    bool m_coordinateReferenceInitialized = false;
    int m_maskBrushSize = 9;
    int m_maskBrushRepeatDirection = 0;
    int m_maskBrushRepeatCount = 0;
    PixelInfo m_lastPixelInfo;
    QPoint m_lastMousePos;
    QGraphicsScene *m_scene = nullptr;
    QWidget *m_activePanelIndicatorOverlay = nullptr;
    QWidget *m_emptyStateOverlay = nullptr;
    LayerManager *m_layerManager = nullptr;
    ImageLayer *m_imageLayer = nullptr;
    QPointer<ImageLayer> m_coordinateReferenceImage;
    QString m_primarySourcePath;
    QPointer<PointLayer> m_editingPointLayer;
    QPointer<ImageLayer> m_editingMaskLayer;
    QPointF m_lastMaskPaintScenePos;
    QPointF m_maskLineAnchorScenePos;
    QPoint m_zoomDragLastPos;
    std::array<QPointF, 3> m_measurementVertices;
    std::array<bool, 3> m_measurementLocks = {false, false, false};
    bool m_measurementCircleVisible = true;
    QPointF m_measureDragLastScenePos;
    QWidget *m_measurementEditor = nullptr;
    QFormLayout *m_measurementEditorForm = nullptr;
    QLabel *m_measurementEditorTitle = nullptr;
    QDoubleSpinBox *m_measurementXSpin = nullptr;
    QDoubleSpinBox *m_measurementYDownSpin = nullptr;
    QDoubleSpinBox *m_measurementLengthSpin = nullptr;
    int m_editingMeasurementVertex = -1;
    int m_editingMeasurementSide = -1;
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
