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
                m_imageLayer = img;
                break;
            }
        }
        setPointItemsInteractionBlocked(panInteractionActive());
    });
    setDragMode(QGraphicsView::NoDrag);
    setFocusPolicy(Qt::StrongFocus);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
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
    if (enabled) {
        if (!m_editingPointLayer && m_layerManager) {
            Layer *active = m_layerManager->activeLayer();
            m_editingPointLayer = qobject_cast<PointLayer*>(active);
        }

        if (!m_editingPointLayer)
            enabled = false;
    }

    m_pointEditEnabled = enabled;
    updateDragMode();

    if (m_editingPointLayer) {
        m_editingPointLayer->setEditMode(enabled);
        if (!enabled)
            m_editingPointLayer.clear();
    }
}

void ImageViewer::keyPressEvent(QKeyEvent *event)
{
    if (handleSpacePanKeyEvent(event))
        return;
    if (handleTemporaryZoomKeyEvent(event))
        return;
    if (handleResetZoomKeyEvent(event))
        return;

    QGraphicsView::keyPressEvent(event);
}

void ImageViewer::keyReleaseEvent(QKeyEvent *event)
{
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

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    constexpr double zoomFactor = 1.15;
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
    setPointItemsInteractionBlocked(shouldPan || zoomInteractionActive());
    if (shouldPan) {
        viewport()->setCursor(Qt::OpenHandCursor);
    } else if (zoomInteractionActive()) {
        viewport()->setCursor(zoomCursor(m_zoomCursorDirection));
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

bool ImageViewer::handleResetZoomKeyEvent(QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return false;

    auto *keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->key() != Qt::Key_0
        || keyEvent->isAutoRepeat()
        || keyEvent->modifiers() != Qt::NoModifier) {
        return false;
    }

    setZoomPercent(100.0);
    keyEvent->accept();
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

    if (!panInteractionActive()
        && !zoomInteractionActive()
        && event->button() == Qt::LeftButton
        && event->modifiers().testFlag(Qt::ControlModifier)) {
        QGraphicsItem *itemUnder = itemAt(event->pos());
        if (!belongsToPointItem(itemUnder) && boxSelectionPointLayer()) {
            beginPointBoxSelection(event->pos());
            event->accept();
            return;
        }
    }

    if (m_pointEditEnabled && !panInteractionActive() && event->button() == Qt::LeftButton) {
        const QPointF scenePos = mapToScene(event->pos());
        QGraphicsItem *itemUnder = itemAt(event->pos());
        const bool isPointItem = belongsToPointItem(itemUnder);
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

void ImageViewer::mouseMoveEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    if (m_pointBoxSelecting) {
        updatePointBoxSelection(event->pos());
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
    if (m_pointEditEnabled && m_editingPointLayer)
        return m_editingPointLayer;

    Layer *active = m_layerManager ? m_layerManager->activeLayer() : nullptr;
    return qobject_cast<PointLayer*>(active);
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
        if (handleResetZoomKeyEvent(event))
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
    if (menu.exec(event->globalPos()) == closeAct)
        emit closeRequested();
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
            // Add image as a new layer on top of existing content
            auto *imgLayer = new ImageLayer(path);
            if (imgLayer->rasterData().isNull()) {
                delete imgLayer;
                continue;
            }
            m_layerManager->addLayer(imgLayer);
            if (m_primarySourcePath.isEmpty())
                m_primarySourcePath = QFileInfo(path).absoluteFilePath();
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
