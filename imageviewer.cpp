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

#include "imageviewer.h"
#include "imagelayer.h"
#include "layermanager.h"
#include "layer.h"
#include "pointitem.h"
#include "pointlayer.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMutexLocker>
#include <QPainter>
#include <QRubberBand>
#include <QScrollBar>
#include <QSplitter>
#include <QUrl>

#include <cmath>

namespace {
QCursor editPointsCursor()
{
    return QCursor(Qt::CrossCursor);
}

QCursor zoomCursor(ZoomCursorDirection direction)
{
    QPixmap themePixmap =
        QIcon::fromTheme(
            direction == ZoomCursorDirection::Out
                ? QIcon::ThemeIcon::ZoomOut
                : QIcon::ThemeIcon::ZoomIn)
            .pixmap(32, 32);
    if (!themePixmap.isNull())
        return QCursor(themePixmap, 15, 15);

    QPixmap pixmap(64, 64);
    pixmap.setDevicePixelRatio(2.0);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(35, 35, 35));
    pen.setWidthF(4.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(13.5, 13.5), 7.0, 7.0);
    painter.drawLine(QPointF(19.0, 19.0), QPointF(27.0, 27.0));
    if (direction == ZoomCursorDirection::In)
        painter.drawLine(QPointF(13.5, 9.5), QPointF(13.5, 17.5));
    painter.drawLine(QPointF(9.5, 13.5), QPointF(17.5, 13.5));
    return QCursor(pixmap, 13, 13);
}

bool belongsToPointItem(QGraphicsItem *item)
{
    for (QGraphicsItem *current = item; current; current = current->parentItem()) {
        if (dynamic_cast<PointItem*>(current))
            return true;
    }
    return false;
}

bool containsPointItem(const QList<QGraphicsItem*> &items)
{
    for (QGraphicsItem *item : items) {
        if (belongsToPointItem(item))
            return true;
    }
    return false;
}

}

ImageViewer::ImageViewer(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
    , m_layerManager(new LayerManager(this))
    , m_lastMousePos(0, 0)
{
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    viewport()->installEventFilter(this);
    viewport()->setFocusPolicy(Qt::StrongFocus);
    setScene(m_scene);
    m_layerManager->attachToScene(m_scene);

    connect(m_layerManager, &LayerManager::activeLayerChanged,
            this, &ImageViewer::onActiveLayerChanged);

    connect(m_layerManager, &LayerManager::layersChanged, this, [this]() {
        m_imageLayer = nullptr;
        for (int i = 0; i < m_layerManager->layerCount(); ++i) {
            if (auto *img = qobject_cast<ImageLayer*>(m_layerManager->layerAt(i))) {
                if (!m_imageLayer)
                    m_imageLayer = img;
                connect(img, &Layer::styleChanged,
                        this, &ImageViewer::updateAutoBackground,
                        Qt::UniqueConnection);
            }
        }
        setPointItemsInteractionBlocked(
            panInteractionActive() || zoomInteractionActive() || m_maskEditEnabled);
        updateAutoBackground();
    });
    setDragMode(QGraphicsView::NoDrag);
    setFocusPolicy(Qt::StrongFocus);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    updateAutoBackground();
}

void ImageViewer::setImage(const QString &fileName)
{
    m_layerManager->clearAll();
    m_primarySourcePath = QFileInfo(fileName).absoluteFilePath();

    auto *imageLayer = new ImageLayer(fileName);
    if (imageLayer->rasterData().isNull()) {
        delete imageLayer;
        QMessageBox::warning(this, "Open Image",
                             QStringLiteral("Could not load image:\n%1").arg(fileName));
        return;
    }

    m_imageLayer = imageLayer;
    m_layerManager->addLayer(imageLayer);

    m_lastPixelInfo = PixelInfo{};
    m_autoFitImage = true;
    fitImageToView();
}

void ImageViewer::fitLoadedImage()
{
    m_lastPixelInfo = PixelInfo{};
    m_autoFitImage = true;
    fitImageToView();
}

QPointF ImageViewer::viewCenter() const
{
    return mapToScene(viewport()->rect().center());
}

void ImageViewer::restoreView(const QTransform &viewTransform,
                              const QPointF &center)
{
    m_autoFitImage = false;
    setTransform(viewTransform);
    centerOn(center);
    emitZoomPercentChanged();
}

double ImageViewer::zoomPercent() const
{
    const QTransform current = transform();
    return std::hypot(current.m11(), current.m21()) * 100.0;
}

void ImageViewer::setZoomPercent(double percent)
{
    percent = qBound(1.0, percent, 6400.0);
    const QPointF center = viewCenter();
    m_autoFitImage = false;
    const double scaleFactor = percent / 100.0;
    setTransform(QTransform::fromScale(scaleFactor, scaleFactor));
    centerOn(center);
    emitZoomPercentChanged();
    syncTransformToSiblings();
}

void ImageViewer::updateAutoBackground()
{
    QColor background = Qt::white;
    if (m_layerManager) {
        for (int i = m_layerManager->layerCount() - 1; i >= 0; --i) {
            auto *imageLayer =
                qobject_cast<ImageLayer*>(m_layerManager->layerAt(i));
            if (!imageLayer || !imageLayer->isVisible())
                continue;

            background = imageLayer->style().blendMode == ImageBlendMode::Maximum
                ? QColor(Qt::black)
                : QColor(Qt::white);
            break;
        }
    }

    if (backgroundBrush().color() != background)
        setBackgroundBrush(background);
}

void ImageViewer::zoomToPointIndices(PointLayer *layer,
                                     const QSet<int> &indices)
{
    if (!layer || indices.isEmpty())
        return;

    const QVector<PointRecord> &points = layer->collection().points();
    QVector<QPointF> positions;
    positions.reserve(indices.size());
    for (int index : indices) {
        if (index >= 0 && index < points.size())
            positions.append(QPointF(points[index].col, points[index].row));
    }
    if (positions.isEmpty())
        return;

    m_autoFitImage = false;
    if (positions.size() == 1) {
        centerOn(positions.first());
        return;
    }

    double minX = positions.first().x();
    double maxX = minX;
    double minY = positions.first().y();
    double maxY = minY;
    for (const QPointF &position : std::as_const(positions)) {
        minX = qMin(minX, position.x());
        maxX = qMax(maxX, position.x());
        minY = qMin(minY, position.y());
        maxY = qMax(maxY, position.y());
    }
    QRectF bounds(QPointF(minX, minY), QPointF(maxX, maxY));

    const double margin = qMax(10.0, qMax(bounds.width(), bounds.height()) * 0.1);
    bounds.adjust(-margin, -margin, margin, margin);
    resetTransform();
    fitInView(bounds, Qt::KeepAspectRatio);
    emitZoomPercentChanged();
}

bool ImageViewer::panModeEnabled() const
{
    return m_panModeEnabled;
}

void ImageViewer::setPanModeEnabled(bool enabled)
{
    m_panModeEnabled = enabled;
    updateDragMode();
}

bool ImageViewer::zoomModeEnabled() const
{
    return m_zoomModeEnabled;
}

void ImageViewer::setZoomModeEnabled(bool enabled)
{
    m_zoomModeEnabled = enabled;
    if (!zoomInteractionActive())
        m_zoomDragging = false;
    updateDragMode();
}

void ImageViewer::setPointEditEnabled(bool enabled)
{
    if (enabled && m_maskEditEnabled)
        setMaskEditEnabled(false);

    if (enabled) {
        if (!m_editingPointLayer && m_layerManager) {
            Layer *active = m_layerManager->activeLayer();
            m_editingPointLayer = qobject_cast<PointLayer*>(active);
        }

        if (!m_editingPointLayer)
            enabled = false;

        if (enabled && m_editingPointLayer
            && m_editingPointLayer->usesAggregateRendering()) {
            QMessageBox::information(
                this,
                "Edit Points",
                QStringLiteral(
                    "This point layer has %1 points and is displayed in a "
                    "fast preview mode.\n\n"
                    "Interactive point editing is disabled for dense point "
                    "layers to keep pan and zoom responsive.\n\n"
                    "Current fast preview limit: %2 points.")
                    .arg(m_editingPointLayer->collection().recordCount())
                    .arg(m_editingPointLayer->fastPreviewLimit()));
            enabled = false;
        }
    }

    m_pointEditEnabled = enabled;
    updateDragMode();

    if (m_editingPointLayer) {
        m_editingPointLayer->setEditMode(enabled);
        if (!enabled)
            m_editingPointLayer.clear();
    }
}

bool ImageViewer::setMaskEditEnabled(bool enabled, QString *errorMessage)
{
    if (m_maskEditEnabled == enabled)
        return true;

    if (enabled) {
        if (m_pointEditEnabled)
            setPointEditEnabled(false);

        Layer *active = m_layerManager ? m_layerManager->activeLayer() : nullptr;
        auto *maskLayer = qobject_cast<ImageLayer*>(active);
        if (!maskLayer) {
            if (errorMessage)
                *errorMessage = QStringLiteral("The active layer is not an image layer.");
            return false;
        }

        if (!maskLayer->setBinaryMaskEditMode(true, errorMessage))
            return false;

        m_editingMaskLayer = maskLayer;
        m_maskEditEnabled = true;
        m_maskPainting = false;
        m_hasLastMaskPaintScenePos = false;
        m_hasMaskLineAnchorScenePos = false;
        setMaskBrushSize(m_maskBrushSize);
        updateDragMode();
        viewport()->update();
        return true;
    }

    if (m_maskPainting)
        finishMaskStroke();

    if (m_editingMaskLayer
        && !m_editingMaskLayer->setBinaryMaskEditMode(false, errorMessage)) {
        return false;
    }

    m_maskPainting = false;
    m_maskPaintingErase = false;
    m_hasLastMaskPaintScenePos = false;
    m_hasMaskLineAnchorScenePos = false;
    m_maskBrushPreviewVisible = false;
    m_maskEditEnabled = false;
    m_editingMaskLayer.clear();
    updateDragMode();
    viewport()->update();
    return true;
}

ImageLayer *ImageViewer::editingMaskLayer() const
{
    return m_editingMaskLayer;
}

bool ImageViewer::cancelMaskEdit(QString *errorMessage)
{
    if (!m_editingMaskLayer) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No mask layer is being edited.");
        return false;
    }

    m_maskPainting = false;
    m_maskPaintingErase = false;
    m_hasLastMaskPaintScenePos = false;
    m_hasMaskLineAnchorScenePos = false;
    m_maskBrushPreviewVisible = false;
    if (!m_editingMaskLayer->cancelBinaryMaskEdit(errorMessage))
        return false;

    m_maskEditEnabled = false;
    m_editingMaskLayer.clear();
    updateDragMode();
    viewport()->update();
    return true;
}

void ImageViewer::setMaskBrushSize(int size)
{
    const int bounded = qBound(1, size, maskBrushMaximum());
    if (m_maskBrushSize == bounded)
        return;
    m_maskBrushSize = bounded;
    viewport()->update();
    emit maskBrushSizeChanged(m_maskBrushSize);
}

int ImageViewer::maskBrushMaximum() const
{
    if (!m_editingMaskLayer || m_editingMaskLayer->rasterData().isNull())
        return 200;
    const RasterData &data = m_editingMaskLayer->rasterData();
    return qMax(1, qMax(data.width(), data.height()));
}

void ImageViewer::keyPressEvent(QKeyEvent *event)
{
    if (handleSpacePanKeyEvent(event))
        return;
    if (handleTemporaryZoomKeyEvent(event))
        return;
    if (handleZoomShortcutKeyEvent(event))
        return;
    if (handleMaskBrushSizeKeyEvent(event))
        return;

    QGraphicsView::keyPressEvent(event);
}

void ImageViewer::keyReleaseEvent(QKeyEvent *event)
{
    if (handleMaskBrushSizeKeyEvent(event))
        return;
    if (handleSpacePanKeyEvent(event))
        return;
    if (handleTemporaryZoomKeyEvent(event))
        return;

    QGraphicsView::keyReleaseEvent(event);
}

void ImageViewer::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (m_autoFitImage)
        fitImageToView();
}

void ImageViewer::leaveEvent(QEvent *event)
{
    m_maskBrushPreviewVisible = false;
    viewport()->update();
    QGraphicsView::leaveEvent(event);
}

void ImageViewer::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);

    if (!m_maskBrushPreviewVisible
        || !m_maskEditEnabled
        || !m_editingMaskLayer
        || panInteractionActive()
        || zoomInteractionActive()) {
        return;
    }

    const QPointF center = mapToScene(m_lastMousePos);
    const double radius = qMax(0.5, m_maskBrushSize / 2.0);
    const QRectF brushRect(
        center.x() - radius,
        center.y() - radius,
        radius * 2.0,
        radius * 2.0);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QPen shadowPen(QColor(0, 0, 0, 180), 3.0);
    shadowPen.setCosmetic(true);
    painter->setPen(shadowPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(brushRect);

    QPen lightPen(QColor(255, 255, 255, 230), 1.5);
    lightPen.setCosmetic(true);
    painter->setPen(lightPen);
    painter->drawEllipse(brushRect);
    painter->restore();
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    constexpr double zoomFactor = 1.15;
    if (event->modifiers().testFlag(Qt::AltModifier)) {
        event->ignore();
        return;
    }

    m_autoFitImage = false;

    if (event->angleDelta().y() > 0)
        scale(zoomFactor, zoomFactor);
    else
        scale(1.0 / zoomFactor, 1.0 / zoomFactor);

    emitZoomPercentChanged();
    syncTransformToSiblings();
}

void ImageViewer::fitImageToView()
{
    if (!m_imageLayer || !m_imageLayer->pixmapItem())
        return;
    resetTransform();
    fitInView(m_imageLayer->pixmapItem(), Qt::KeepAspectRatio);
    emitZoomPercentChanged();
}

void ImageViewer::updateDragMode()
{
    const bool shouldPan = panInteractionActive();
    setDragMode(shouldPan ? QGraphicsView::ScrollHandDrag : QGraphicsView::NoDrag);
    setPointItemsInteractionBlocked(
        shouldPan || zoomInteractionActive() || m_maskEditEnabled);
    if (shouldPan) {
        viewport()->setCursor(Qt::OpenHandCursor);
    } else if (zoomInteractionActive()) {
        viewport()->setCursor(zoomCursor(m_zoomCursorDirection));
    } else if (m_maskEditEnabled) {
        viewport()->setCursor(Qt::CrossCursor);
    } else if (m_pointEditEnabled) {
        viewport()->setCursor(editPointsCursor());
    } else {
        viewport()->setCursor(Qt::ArrowCursor);
    }
}

bool ImageViewer::panInteractionActive() const
{
    return m_panModeEnabled || m_spacePanActive;
}

bool ImageViewer::zoomInteractionActive() const
{
    const bool controlDown =
        QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
    return !controlDown
        && !panInteractionActive()
        && (m_zoomModeEnabled || m_temporaryZoomActive);
}

bool ImageViewer::handleSpacePanKeyEvent(QEvent *event)
{
    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)
        return false;

    auto *keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->key() != Qt::Key_Space || keyEvent->isAutoRepeat())
        return false;

    m_spacePanActive = (event->type() == QEvent::KeyPress);
    updateDragMode();
    keyEvent->accept();
    return true;
}

bool ImageViewer::handleTemporaryZoomKeyEvent(QEvent *event)
{
    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)
        return false;

    auto *keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->key() != Qt::Key_Z || keyEvent->isAutoRepeat())
        return false;

    m_temporaryZoomActive = (event->type() == QEvent::KeyPress);
    if (!zoomInteractionActive())
        m_zoomDragging = false;
    updateDragMode();
    keyEvent->accept();
    return true;
}

bool ImageViewer::handleZoomShortcutKeyEvent(QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return false;

    auto *keyEvent = static_cast<QKeyEvent*>(event);
    const Qt::KeyboardModifiers realModifiers =
        keyEvent->modifiers() & ~Qt::KeypadModifier;
    if (keyEvent->isAutoRepeat() || realModifiers != Qt::NoModifier)
        return false;

    if (keyEvent->key() == Qt::Key_0) {
        setZoomPercent(100.0);
        keyEvent->accept();
        return true;
    }

    if (keyEvent->key() == Qt::Key_9) {
        fitLoadedImage();
        syncTransformToSiblings();
        keyEvent->accept();
        return true;
    }

    return false;
}

bool ImageViewer::handleMaskBrushSizeKeyEvent(QKeyEvent *event)
{
    if (!m_maskEditEnabled
        || (event->type() != QEvent::KeyPress
            && event->type() != QEvent::KeyRelease)) {
        return false;
    }

    int direction = 0;
    if (event->text() == "["
        || event->key() == Qt::Key_BracketLeft) {
        direction = -1;
    } else if (event->text() == "]"
               || event->key() == Qt::Key_BracketRight) {
        direction = 1;
    }
    if (direction == 0)
        return false;

    if (event->type() == QEvent::KeyRelease) {
        if (m_maskBrushRepeatDirection == direction) {
            m_maskBrushRepeatDirection = 0;
            m_maskBrushRepeatCount = 0;
        }
        event->accept();
        return true;
    }

    if (!event->isAutoRepeat()
        || m_maskBrushRepeatDirection != direction) {
        m_maskBrushRepeatDirection = direction;
        m_maskBrushRepeatCount = 0;
    } else {
        ++m_maskBrushRepeatCount;
    }

    const int baseStep =
        qMax(1, static_cast<int>(std::round(m_maskBrushSize * 0.1)));
    const int acceleration =
        qMin(12, 1 + m_maskBrushRepeatCount / 4);
    const int step = baseStep * acceleration;
    setMaskBrushSize(m_maskBrushSize + direction * step);
    event->accept();
    return true;
}

void ImageViewer::setPointItemsInteractionBlocked(bool blocked)
{
    if (!m_scene)
        return;

    for (QGraphicsItem *item : m_scene->items()) {
        auto *pointItem = dynamic_cast<PointItem*>(item);
        if (!pointItem)
            continue;

        pointItem->setAcceptedMouseButtons(blocked ? Qt::NoButton : Qt::AllButtons);
        pointItem->setAcceptHoverEvents(!blocked);
        if (blocked)
            pointItem->unsetCursor();
    }
}

void ImageViewer::mousePressEvent(QMouseEvent *event)
{
    emit activatedByClick();
    m_lastMousePos = event->pos();

    if (zoomInteractionActive() && event->button() == Qt::LeftButton) {
        m_zoomDragging = true;
        m_zoomDragLastPos = event->pos();
        m_zoomCursorDirection = ZoomCursorDirection::In;
        m_autoFitImage = false;
        viewport()->setCursor(zoomCursor(m_zoomCursorDirection));
        event->accept();
        return;
    }

    if (m_maskEditEnabled
        && !panInteractionActive()
        && !zoomInteractionActive()
        && event->button() == Qt::LeftButton) {
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            event->accept();
            return;
        }
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            const bool erase =
                event->modifiers().testFlag(Qt::AltModifier);
            paintMaskLineToViewportPos(event->pos(), erase);
            finishMaskStroke();
            event->accept();
            return;
        }
        m_maskPainting = true;
        m_maskPaintingErase =
            event->modifiers().testFlag(Qt::AltModifier);
        m_hasLastMaskPaintScenePos = false;
        paintMaskAtViewportPos(event->pos(), m_maskPaintingErase);
        event->accept();
        return;
    }

    if (!panInteractionActive()
        && !zoomInteractionActive()
        && !m_maskEditEnabled
        && event->button() == Qt::LeftButton
        && event->modifiers().testFlag(Qt::ControlModifier)) {
        if (!containsPointItem(items(event->pos()))
            && boxSelectionPointLayer()) {
            beginPointBoxSelection(event->pos());
            event->accept();
            return;
        }
    }

    if (m_pointEditEnabled && !panInteractionActive() && event->button() == Qt::LeftButton) {
        const QPointF scenePos = mapToScene(event->pos());
        const bool isPointItem = containsPointItem(items(event->pos()));
        if (!isPointItem) {
            if (m_editingPointLayer) {
                m_editingPointLayer->addPoint(scenePos.y(), scenePos.x());
                event->accept();
                return;
            }
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void ImageViewer::mouseDoubleClickEvent(QMouseEvent *event)
{
    emit activatedByClick();
    m_lastMousePos = event->pos();

    if (m_maskEditEnabled
        && m_editingMaskLayer
        && !panInteractionActive()
        && !zoomInteractionActive()
        && event->button() == Qt::LeftButton
        && event->modifiers().testFlag(Qt::ControlModifier)) {
        int changedPixels = 0;
        QString errorMessage;
        if (!m_editingMaskLayer->invertConnectedBinaryMaskRegionAtScenePos(
                mapToScene(event->pos()), &changedPixels, &errorMessage)) {
            if (!errorMessage.isEmpty()) {
                QMessageBox::warning(
                    this, "Edit mask", errorMessage);
            }
            event->accept();
            return;
        }

        if (!m_editingMaskLayer->saveBinaryMask(&errorMessage)) {
            QMessageBox::warning(
                this, "Edit mask",
                errorMessage.isEmpty()
                    ? QStringLiteral("Could not save mask changes.")
                    : errorMessage);
            event->accept();
            return;
        }

        emit maskConnectedRegionInverted(changedPixels);
        event->accept();
        return;
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}

void ImageViewer::applyDragZoom(const QPoint &pos)
{
    const QPoint delta = pos - m_zoomDragLastPos;
    const int zoomDelta = delta.x() - delta.y();
    if (zoomDelta == 0)
        return;

    setZoomCursorDirection(
        zoomDelta > 0 ? ZoomCursorDirection::In
                      : ZoomCursorDirection::Out);

    const QPointF center = viewCenter();
    const double factor = std::pow(1.01, zoomDelta);
    scale(factor, factor);
    centerOn(center);
    m_zoomDragLastPos = pos;
    emitZoomPercentChanged();
    syncTransformToSiblings();
}

void ImageViewer::syncTransformToSiblings()
{
    if (!ms_syncEnabled || !ms_syncZoomEnabled)
        return;

    const QTransform t = transform();
    if (QSplitter *splitter = qobject_cast<QSplitter*>(parentWidget())) {
        for (int i = 0; i < splitter->count(); ++i) {
            if (ImageViewer *sib = qobject_cast<ImageViewer*>(splitter->widget(i))) {
                if (sib != this) {
                    sib->m_syncingScroll = true;
                    sib->setTransform(t);
                    sib->m_syncingScroll = false;
                    sib->emitZoomPercentChanged();
                }
            }
        }
    }
}

void ImageViewer::emitZoomPercentChanged()
{
    emit zoomPercentChanged(zoomPercent());
}

bool ImageViewer::paintMaskAtViewportPos(const QPoint &pos, bool erase)
{
    if (!m_maskEditEnabled || !m_editingMaskLayer)
        return false;

    const QPointF scenePos = mapToScene(pos);
    m_lastMaskPaintScenePos = scenePos;
    m_hasLastMaskPaintScenePos = true;
    return m_editingMaskLayer->paintBinaryMaskAtScenePos(
        scenePos, m_maskBrushSize, erase);
}

bool ImageViewer::paintMaskStrokeToViewportPos(const QPoint &pos, bool erase)
{
    if (!m_maskEditEnabled || !m_editingMaskLayer)
        return false;

    const QPointF scenePos = mapToScene(pos);
    if (!m_hasLastMaskPaintScenePos) {
        m_lastMaskPaintScenePos = scenePos;
        m_hasLastMaskPaintScenePos = true;
        return m_editingMaskLayer->paintBinaryMaskAtScenePos(
            scenePos, m_maskBrushSize, erase);
    }

    const QPointF delta = scenePos - m_lastMaskPaintScenePos;
    const double distance = std::hypot(delta.x(), delta.y());
    const double spacing = qMax(1.0, m_maskBrushSize / 3.0);
    const int steps = qMax(1, static_cast<int>(std::ceil(distance / spacing)));
    bool changed = false;
    for (int step = 1; step <= steps; ++step) {
        const double t = static_cast<double>(step) / steps;
        const QPointF interpolated =
            m_lastMaskPaintScenePos + delta * t;
        changed |= m_editingMaskLayer->paintBinaryMaskAtScenePos(
            interpolated, m_maskBrushSize, erase);
    }

    m_lastMaskPaintScenePos = scenePos;
    return changed;
}

bool ImageViewer::paintMaskLineToViewportPos(const QPoint &pos, bool erase)
{
    if (!m_maskEditEnabled || !m_editingMaskLayer)
        return false;

    const QPointF scenePos = mapToScene(pos);
    if (!m_hasMaskLineAnchorScenePos) {
        m_maskLineAnchorScenePos = scenePos;
        m_hasMaskLineAnchorScenePos = true;
        return paintMaskAtViewportPos(pos, erase);
    }

    m_lastMaskPaintScenePos = m_maskLineAnchorScenePos;
    m_hasLastMaskPaintScenePos = true;
    const bool changed = paintMaskStrokeToViewportPos(pos, erase);
    m_maskLineAnchorScenePos = scenePos;
    m_hasMaskLineAnchorScenePos = true;
    return changed;
}

void ImageViewer::finishMaskStroke()
{
    if (!m_maskPainting && !m_hasLastMaskPaintScenePos)
        return;

    m_maskPainting = false;
    if (m_hasLastMaskPaintScenePos) {
        m_maskLineAnchorScenePos = m_lastMaskPaintScenePos;
        m_hasMaskLineAnchorScenePos = true;
    }
    m_hasLastMaskPaintScenePos = false;
    if (!m_editingMaskLayer)
        return;

    QString errorMessage;
    if (!m_editingMaskLayer->saveBinaryMask(&errorMessage)) {
        QMessageBox::warning(
            this, "Edit mask",
            errorMessage.isEmpty()
                ? QStringLiteral("Could not save mask changes.")
                : errorMessage);
    }
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    const bool brushPreviewWasVisible = m_maskBrushPreviewVisible;
    m_maskBrushPreviewVisible =
        m_maskEditEnabled
        && m_editingMaskLayer
        && viewport()->rect().contains(event->pos());
    if (m_maskBrushPreviewVisible || brushPreviewWasVisible)
        viewport()->update();

    if (m_pointBoxSelecting) {
        updatePointBoxSelection(event->pos());
        event->accept();
        return;
    }

    if (m_maskPainting && event->buttons().testFlag(Qt::LeftButton)) {
        if (!panInteractionActive() && !zoomInteractionActive())
            paintMaskStrokeToViewportPos(event->pos(), m_maskPaintingErase);
        event->accept();
        return;
    }

    if (m_zoomDragging && zoomInteractionActive()) {
        applyDragZoom(event->pos());
        event->accept();
        return;
    }

    PixelInfo info;
    info.row = -1;
    info.col = -1;
    info.bandCount = 0;

    Layer *active = m_layerManager ? m_layerManager->activeLayer() : nullptr;
    auto *img = qobject_cast<ImageLayer*>(active);
    if (!img) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    if (img->pixmapItem()) {
        const QPointF scenePos = mapToScene(event->pos());
        const QRectF bounds = img->pixmapItem()->boundingRect();
        if (bounds.contains(scenePos)) {
            const int col = static_cast<int>(scenePos.x() - bounds.left());
            const int row = static_cast<int>(scenePos.y() - bounds.top());
            const RasterData &data = img->rasterData();
            if (row >= 0 && row < data.height() && col >= 0 && col < data.width()) {
                info.row = row;
                info.col = col;
                info.bandCount = data.bands();
                info.bandValues.reserve(data.bands());
                for (int b = 0; b < data.bands(); ++b)
                    info.bandValues.append(data.sample(b, row, col));
            }
        }
    }

    m_lastPixelInfo = info;
    emit pixelInfoChanged(info);

    QGraphicsView::mouseMoveEvent(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_maskPainting && event->button() == Qt::LeftButton) {
        if (!panInteractionActive() && !zoomInteractionActive())
            paintMaskStrokeToViewportPos(event->pos(), m_maskPaintingErase);
        finishMaskStroke();
        event->accept();
        return;
    }

    if (m_pointBoxSelecting && event->button() == Qt::LeftButton) {
        finishPointBoxSelection(event->pos());
        event->accept();
        return;
    }

    if (m_zoomDragging && event->button() == Qt::LeftButton) {
        m_zoomDragging = false;
        m_zoomCursorDirection = ZoomCursorDirection::In;
        updateDragMode();
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

PointLayer *ImageViewer::boxSelectionPointLayer() const
{
    if (m_pointEditEnabled && m_editingPointLayer) {
        if (m_editingPointLayer->usesAggregateRendering())
            return nullptr;
        return m_editingPointLayer;
    }

    Layer *active = m_layerManager ? m_layerManager->activeLayer() : nullptr;
    auto *pointLayer = qobject_cast<PointLayer*>(active);
    if (pointLayer && pointLayer->usesAggregateRendering())
        return nullptr;
    return pointLayer;
}

void ImageViewer::beginPointBoxSelection(const QPoint &pos)
{
    if (!m_pointBoxRubberBand)
        m_pointBoxRubberBand = new QRubberBand(QRubberBand::Rectangle, viewport());

    m_pointBoxSelecting = true;
    m_pointBoxOrigin = pos;
    m_pointBoxModifiers = QApplication::keyboardModifiers();
    m_pointBoxRect = QRect(pos, QSize());
    m_pointBoxRubberBand->setGeometry(m_pointBoxRect);
    m_pointBoxRubberBand->show();
}

void ImageViewer::updatePointBoxSelection(const QPoint &pos)
{
    if (!m_pointBoxSelecting || !m_pointBoxRubberBand)
        return;

    m_pointBoxRect = QRect(m_pointBoxOrigin, pos).normalized();
    m_pointBoxRubberBand->setGeometry(m_pointBoxRect);
}

void ImageViewer::finishPointBoxSelection(const QPoint &pos)
{
    updatePointBoxSelection(pos);
    if (m_pointBoxRubberBand)
        m_pointBoxRubberBand->hide();
    m_pointBoxSelecting = false;

    PointLayer *layer = boxSelectionPointLayer();
    if (!layer)
        return;

    if (m_pointBoxRect.width() < 4 && m_pointBoxRect.height() < 4)
        return;

    const QRectF sceneRect = mapToScene(m_pointBoxRect).boundingRect();
    QSet<int> selected;
    const QVector<PointRecord> &points = layer->collection().points();
    for (int i = 0; i < points.size(); ++i) {
        const QPointF point(points[i].col, points[i].row);
        if (sceneRect.contains(point))
            selected.insert(i);
    }

    if (m_pointBoxModifiers.testFlag(Qt::ShiftModifier))
        selected.unite(layer->selectedPointIndices());

    const int primary = selected.isEmpty() ? -1 : *selected.constBegin();
    layer->setSelectedPointIndices(selected, primary);
}

void ImageViewer::setZoomCursorDirection(ZoomCursorDirection direction)
{
    if (m_zoomCursorDirection == direction)
        return;
    m_zoomCursorDirection = direction;
    if (zoomInteractionActive())
        viewport()->setCursor(zoomCursor(m_zoomCursorDirection));
}

// --- Drag & Drop ---
// QGraphicsView's viewport() receives drop events first.
// We use an event filter on the viewport to intercept them,
// because setAcceptDrops(true) on QGraphicsView alone is not
// sufficient: the viewport widget overrides drop behavior.

bool ImageViewer::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == viewport()) {
        if (event->type() == QEvent::Enter) {
            setFocus(Qt::MouseFocusReason);
            viewport()->setFocus(Qt::MouseFocusReason);
        }
        if (handleSpacePanKeyEvent(event))
            return true;
        if (handleTemporaryZoomKeyEvent(event))
            return true;
        if (handleZoomShortcutKeyEvent(event))
            return true;
        if (event->type() == QEvent::DragEnter) {
            auto *de = static_cast<QDragEnterEvent*>(event);
            dragEnterEvent(de);
            return de->isAccepted();
        }
        if (event->type() == QEvent::DragMove) {
            // Must accept DragMove for Drop to complete after DragEnter
            auto *dm = static_cast<QDragMoveEvent*>(event);
            dm->acceptProposedAction();
            return true;
        }
        if (event->type() == QEvent::Drop) {
            auto *de = static_cast<QDropEvent*>(event);
            dropEvent(de);
            return true;
        }
    }
    return QGraphicsView::eventFilter(obj, event);
}

void ImageViewer::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const QStringList imgExts = {"tif", "tiff", "png", "jpg", "jpeg", "bmp"};
    const QStringList csvExts = {"csv"};

    for (const QUrl &url : event->mimeData()->urls()) {
        const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
        if (imgExts.contains(suffix) || csvExts.contains(suffix)) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

bool ImageViewer::ms_syncEnabled = false;
bool ImageViewer::ms_syncZoomEnabled = true;

void ImageViewer::setSyncEnabled(bool enabled)
{
    ms_syncEnabled = enabled;
}

bool ImageViewer::syncEnabled()
{
    return ms_syncEnabled;
}

void ImageViewer::setSyncZoomEnabled(bool enabled)
{
    ms_syncZoomEnabled = enabled;
}

bool ImageViewer::syncZoomEnabled()
{
    return ms_syncZoomEnabled;
}

void ImageViewer::scrollContentsBy(int dx, int dy)
{
    if (m_syncingScroll) {
        QGraphicsView::scrollContentsBy(dx, dy);
        return;
    }

    QGraphicsView::scrollContentsBy(dx, dy);

    if (!ms_syncEnabled)
        return;

    syncToSiblings(dx, dy);
}

void ImageViewer::syncToSiblings(int dx, int dy)
{
    if (QSplitter *splitter = qobject_cast<QSplitter*>(parentWidget())) {
        for (int i = 0; i < splitter->count(); ++i) {
            if (ImageViewer *sibling = qobject_cast<ImageViewer*>(splitter->widget(i))) {
                if (sibling != this) {
                    sibling->m_syncingScroll = true;
                    sibling->horizontalScrollBar()->setValue(
                        sibling->horizontalScrollBar()->value() - dx); // Negar dx
                    sibling->verticalScrollBar()->setValue(
                        sibling->verticalScrollBar()->value() - dy);  // Negar dy
                    sibling->m_syncingScroll = false;
                }
            }
        }
    }
}

void ImageViewer::onActiveLayerChanged(Layer *layer)
{
    if (auto *activeImageLayer = qobject_cast<ImageLayer*>(layer)) {
        m_imageLayer = activeImageLayer;
    } else if (!m_imageLayer && m_layerManager) {
        for (int i = 0; i < m_layerManager->layerCount(); ++i) {
            if (auto *img = qobject_cast<ImageLayer*>(m_layerManager->layerAt(i))) {
                m_imageLayer = img;
                break;
            }
        }
    }

    PixelInfo info;
    info.row = m_lastPixelInfo.row;
    info.col = m_lastPixelInfo.col;

    if (m_imageLayer && m_lastPixelInfo.row >= 0) {
        const RasterData &data = m_imageLayer->rasterData();
        const int r = m_lastPixelInfo.row;
        const int c = m_lastPixelInfo.col;
        if (r >= 0 && r < data.height() && c >= 0 && c < data.width()) {
            info.bandCount = data.bands();
            info.bandValues.reserve(data.bands());
            for (int b = 0; b < data.bands(); ++b)
                info.bandValues.append(data.sample(b, r, c));
        }
    }

    emit pixelInfoChanged(info);
}

void ImageViewer::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *closeAct = menu.addAction("Close panel");
    QAction *reloadAct = menu.addAction("Reload");
    QAction *selected = menu.exec(event->globalPos());
    if (selected == closeAct)
        emit closeRequested();
    else if (selected == reloadAct)
        emit reloadRequested();
}

void ImageViewer::dropEvent(QDropEvent *event)
{
    const QStringList imgExts = {"tif", "tiff", "png", "jpg", "jpeg", "bmp"};
    const QStringList csvExts = {"csv"};

    for (const QUrl &url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (path.isEmpty())
            continue;

        const QString suffix = QFileInfo(path).suffix().toLower();

        if (imgExts.contains(suffix)) {
            emit imageLayerFileDropped(path);
        } else if (csvExts.contains(suffix)) {
            if (!m_imageLayer) {
                QMessageBox::information(this, "Add Points",
                    "Load an image first before adding a point layer.");
                continue;
            }
            emit pointLayerFileDropped(path);
        }
    }
    event->acceptProposedAction();
}
