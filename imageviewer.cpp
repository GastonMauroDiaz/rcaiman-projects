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
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QHelpEvent>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineF>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMutexLocker>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRubberBand>
#include <QScrollBar>
#include <QSplitter>
#include <QStyle>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
void drawMeasurementLock(QPainter *painter, const QRectF &rect, bool locked)
{
    painter->save();
    painter->translate(rect.topLeft());
    painter->scale(rect.width() / 20, rect.height() / 20);
    painter->setPen(QPen(QColor("#4b5563"), 1.5, Qt::SolidLine, Qt::RoundCap));
    painter->setBrush(Qt::NoBrush);
    QPainterPath shackle;
    shackle.moveTo(locked ? 6 : 10, 10);
    shackle.lineTo(locked ? 6 : 10, 6);
    shackle.cubicTo(locked ? 6 : 10, 1, locked ? 14 : 18, 1, locked ? 14 : 18, 6);
    if (locked)
        shackle.lineTo(14, 10);
    painter->drawPath(shackle);
    painter->setBrush(locked ? QColor("#c9cfd6") : QColor(Qt::white));
    painter->drawRoundedRect(QRectF(4, 10, 12, 8), 1.5, 1.5);
    painter->setBrush(QColor("#4b5563"));
    painter->drawEllipse(QPointF(10, 13), 0.7, 0.7);
    painter->drawLine(QPointF(10, 13), QPointF(10, 15));
    painter->restore();
}

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

class ActivePanelIndicatorOverlay : public QWidget
{
public:
    explicit ActivePanelIndicatorOverlay(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAutoFillBackground(false);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        const QRect borderRect = rect().adjusted(2, 2, -3, -3);
        QPen contrastPen(QColor(0, 0, 0, 150), 4.0);
        contrastPen.setJoinStyle(Qt::MiterJoin);
        painter.setPen(contrastPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(borderRect);

        QPen accentPen(palette().color(QPalette::Highlight), 2.0);
        accentPen.setJoinStyle(Qt::MiterJoin);
        painter.setPen(accentPen);
        painter.drawRect(borderRect);
    }
};

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
        bool referenceChanged = false;
        if (!m_coordinateReferenceInitialized && m_imageLayer) {
            m_coordinateReferenceImage = m_imageLayer;
            m_coordinateReferenceInitialized = true;
            referenceChanged = true;
        }
        emit coordinateReferenceImagesChanged();
        if (referenceChanged)
            emit coordinateReferenceImageChanged();
        if (m_cursorInside)
            emitCursorCoordinateInfo(m_lastMousePos);
        setPointItemsInteractionBlocked(
            panInteractionActive() || zoomInteractionActive()
            || m_measureDistanceEnabled || m_maskEditEnabled);
        if (m_layerManager->layerCount() == 0
            && m_measureDistanceEnabled) {
            setMeasureDistanceEnabled(false);
        }
        updateAutoBackground();
        updateEmptyStateOverlay();
    });
    setDragMode(QGraphicsView::NoDrag);
    setFocusPolicy(Qt::StrongFocus);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_activePanelIndicatorOverlay =
        new ActivePanelIndicatorOverlay(this);
    m_activePanelIndicatorOverlay->setGeometry(viewport()->geometry());
    m_activePanelIndicatorOverlay->hide();

    m_emptyStateOverlay = new QWidget(viewport());
    auto *emptyLayout = new QVBoxLayout(m_emptyStateOverlay);
    emptyLayout->setContentsMargins(18, 14, 18, 14);
    emptyLayout->setSpacing(8);

    auto *emptyTitle = new QLabel("Empty viewer", m_emptyStateOverlay);
    QFont emptyTitleFont = emptyTitle->font();
    emptyTitleFont.setBold(true);
    emptyTitle->setFont(emptyTitleFont);
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);

    auto *emptyBody = new QLabel(
        "Drop a TIFF here or close this panel.", m_emptyStateOverlay);
    emptyBody->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyBody);

    auto *closePanelButton = new QPushButton(
        QIcon::fromTheme(QIcon::ThemeIcon::WindowClose),
        "Close panel",
        m_emptyStateOverlay);
    connect(closePanelButton, &QPushButton::clicked,
            this, [this]() { emit closeRequested(); });
    emptyLayout->addWidget(closePanelButton, 0, Qt::AlignCenter);

    m_emptyStateOverlay->adjustSize();
    updateEmptyStateOverlay();
    connect(qApp, &QGuiApplication::applicationStateChanged,
            this, [this](Qt::ApplicationState state) {
                if (state != Qt::ApplicationActive)
                    clearTemporaryInteractionState();
            });
    updateAutoBackground();
}

void ImageViewer::setImage(const QString &fileName)
{
    m_coordinateReferenceImage.clear();
    m_coordinateReferenceInitialized = false;
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

QList<ImageLayer*> ImageViewer::imageLayers() const
{
    QList<ImageLayer*> result;
    if (!m_layerManager)
        return result;

    for (int i = 0; i < m_layerManager->layerCount(); ++i) {
        if (auto *layer = qobject_cast<ImageLayer*>(m_layerManager->layerAt(i)))
            result.append(layer);
    }
    return result;
}

ImageLayer *ImageViewer::coordinateReferenceImage() const
{
    return m_coordinateReferenceImage.data();
}

void ImageViewer::setCoordinateReferenceImage(ImageLayer *layer)
{
    if (layer && !imageLayers().contains(layer))
        return;
    if (m_coordinateReferenceInitialized
        && m_coordinateReferenceImage == layer) {
        return;
    }

    closeMeasurementEditor();
    m_coordinateReferenceImage = layer;
    m_coordinateReferenceInitialized = true;
    emit coordinateReferenceImageChanged();
    if (m_cursorInside)
        emitCursorCoordinateInfo(m_lastMousePos);
}

void ImageViewer::setPrimarySourcePath(const QString &path)
{
    if (path.isEmpty()) {
        m_primarySourcePath.clear();
        return;
    }

    m_primarySourcePath = QFileInfo(path).absoluteFilePath();

    for (ImageLayer *layer : imageLayers()) {
        const QString sourcePath =
            QFileInfo(layer->sourcePath()).absoluteFilePath();
#ifdef Q_OS_WIN
        const bool matches = sourcePath.compare(
            m_primarySourcePath, Qt::CaseInsensitive) == 0;
#else
        const bool matches = sourcePath == m_primarySourcePath;
#endif
        if (matches) {
            setCoordinateReferenceImage(layer);
            return;
        }
    }
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
            positions.append(
                QPointF(points[index].x, points[index].yDown));
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

void ImageViewer::setActivePanelIndicator(bool active)
{
    if (!m_activePanelIndicatorOverlay)
        return;

    m_activePanelIndicatorOverlay->setGeometry(viewport()->geometry());
    m_activePanelIndicatorOverlay->setVisible(active);
    if (active)
        m_activePanelIndicatorOverlay->raise();
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

void ImageViewer::setMeasureDistanceEnabled(bool enabled)
{
    if (m_measureDistanceEnabled == enabled)
        return;

    m_measureDistanceEnabled = enabled;
    if (enabled)
        initializeMeasurement(viewport()->rect().center());
    else
        clearMeasurement();
    updateDragMode();
    viewport()->update();
}

QPointF ImageViewer::measurementVertex(int vertex) const
{
    if (vertex < 0 || vertex >= 3)
        return {};
    return measurementCoordinates(m_measurementVertices[vertex]);
}

QPointF ImageViewer::measurementCoordinates(const QPointF &point) const
{
    ImageLayer *reference = coordinateReferenceImage();
    if (reference && reference->pixmapItem()) {
        const auto *item = reference->pixmapItem();
        return item->mapFromScene(point) - item->boundingRect().topLeft();
    }
    return point;
}

QPointF ImageViewer::measurementScenePosition(const QPointF &position) const
{
    ImageLayer *reference = coordinateReferenceImage();
    if (reference && reference->pixmapItem()) {
        const auto *item = reference->pixmapItem();
        return item->mapToScene(position + item->boundingRect().topLeft());
    }
    return position;
}

double ImageViewer::measurementSideLength(int side) const
{
    if (!m_measurementVisible || side < 0 || side >= 3)
        return qQNaN();
    return QLineF(measurementVertex(side), measurementVertex((side + 1) % 3)).length();
}

double ImageViewer::measurementAngleDegrees(int vertex) const
{
    if (vertex < 0 || vertex >= 3)
        return qQNaN();
    const QPointF origin = measurementVertex(vertex);
    const QLineF first(origin, measurementVertex((vertex + 1) % 3));
    const QLineF second(origin, measurementVertex((vertex + 2) % 3));
    if (!m_measurementVisible || first.length() <= 1e-9 || second.length() <= 1e-9)
        return qQNaN();
    const double angle = first.angleTo(second);
    return qMin(angle, 360.0 - angle);
}

void ImageViewer::setMeasurementVertex(int vertex, const QPointF &position)
{
    if (!m_measurementVisible || vertex < 0 || vertex >= 3 || measurementVertexLocked(vertex)
        || !std::isfinite(position.x()) || !std::isfinite(position.y())) {
        return;
    }

    m_measurementVertices[vertex] = measurementScenePosition(position);
    viewport()->update();
    emit measurementChanged();
}

void ImageViewer::translateMeasurement(const QPointF &delta)
{
    if (!measurementTranslationAllowed())
        return;
    for (QPointF &point : m_measurementVertices)
        point += delta;
    viewport()->update();
    emit measurementChanged();
}

bool ImageViewer::measurementVertexLocked(int vertex) const
{
    return vertex >= 0 && vertex < 3 && m_measurementLocks[vertex];
}

void ImageViewer::setMeasurementVertexLocked(int vertex, bool locked)
{
    if (vertex < 0 || vertex >= 3 || m_measurementLocks[vertex] == locked)
        return;
    closeMeasurementEditor();
    m_measureDragging = false;
    m_measureDragTarget = MeasurementDragTarget::None;
    m_measurementLocks[vertex] = locked;
    updateMeasurementCursor(m_lastMousePos);
    viewport()->update();
    emit measurementChanged();
}

bool ImageViewer::measurementTranslationAllowed() const
{
    return m_measurementVisible && !m_measurementLocks[0]
        && !m_measurementLocks[1] && !m_measurementLocks[2];
}

bool ImageViewer::canEditMeasurementSide(int side) const
{
    return m_measurementVisible && side >= 0 && side < 3
        && m_measurementLocks[side] != m_measurementLocks[(side + 1) % 3]
        && measurementSideLength(side) > 1e-9;
}

bool ImageViewer::setMeasurementSideLength(int side, double length)
{
    if (!canEditMeasurementSide(side) || !std::isfinite(length) || length <= 0)
        return false;
    const int next = (side + 1) % 3;
    const int fixed = m_measurementLocks[side] ? side : next;
    const int moving = fixed == side ? next : side;
    const QPointF direction = m_measurementVertices[moving] - m_measurementVertices[fixed];
    const QPointF position = m_measurementVertices[fixed]
        + direction * (length / measurementSideLength(side));
    if (!std::isfinite(position.x()) || !std::isfinite(position.y()))
        return false;
    m_measurementVertices[moving] = position;
    viewport()->update();
    emit measurementChanged();
    return true;
}

MeasurementCircle ImageViewer::measurementCircumcircle() const
{
    MeasurementCircle circle;
    if (!m_measurementVisible)
        return circle;
    const QPointF a = measurementVertex(0);
    QPointF u = measurementVertex(1) - a;
    QPointF v = measurementVertex(2) - a;
    const double scale = qMax(qMax(std::abs(u.x()), std::abs(u.y())),
                              qMax(std::abs(v.x()), std::abs(v.y())));
    if (!std::isfinite(scale) || scale == 0)
        return circle;
    // Translate and scale first to avoid squaring large absolute coordinates.
    u /= scale;
    v /= scale;
    const double cross = u.x() * v.y() - u.y() * v.x();
    const double u2 = QPointF::dotProduct(u, u);
    const double v2 = QPointF::dotProduct(v, v);
    const double longest2 = qMax(qMax(u2, v2), QPointF::dotProduct(u - v, u - v));
    if (std::abs(cross) <= 64 * std::numeric_limits<double>::epsilon() * longest2)
        return circle;
    const QPointF offset((u2 * v.y() - v2 * u.y()) / (2 * cross),
                         (u.x() * v2 - v.x() * u2) / (2 * cross));
    circle.center = a + offset * scale;
    circle.radius = std::hypot(offset.x(), offset.y()) * scale;
    circle.valid = std::isfinite(circle.center.x()) && std::isfinite(circle.center.y())
        && std::isfinite(circle.radius) && circle.radius > 0;
    // Altitude / longest side below 1:1000 indicates strong geometric sensitivity.
    circle.unstable = circle.valid && std::abs(cross) / longest2 < 1e-3;
    return circle;
}

void ImageViewer::setMeasurementCircleVisible(bool visible)
{
    if (m_measurementCircleVisible == visible)
        return;
    closeMeasurementEditor();
    m_measurementCircleVisible = visible;
    viewport()->update();
    emit measurementChanged();
}

QPolygonF ImageViewer::measurementScreenPolygon() const
{
    QPolygonF polygon;
    for (const QPointF &point : m_measurementVertices)
        polygon.append(viewportTransform().map(point));
    return polygon;
}

QList<ImageViewer::MeasurementLabel> ImageViewer::measurementLabels() const
{
    QList<MeasurementLabel> labels;
    if (!m_measurementVisible)
        return labels;

    const QPolygonF polygon = measurementScreenPolygon();
    const QPointF center = (polygon[0] + polygon[1] + polygon[2]) / 3;
    QList<QPointF> reservedPoints = polygon;
    const MeasurementCircle circle = measurementCircumcircle();
    if (m_measurementCircleVisible && circle.valid)
        reservedPoints.append(viewportTransform().map(measurementScenePosition(circle.center)));
    const QFontMetrics metrics(viewport()->font());
    const QLocale locale;
    const auto number = [&locale](double value) {
        QString text = locale.toString(value, 'f', 3);
        while (text.endsWith('0'))
            text.chop(1);
        if (text.endsWith(locale.decimalPoint()))
            text.chop(locale.decimalPoint().size());
        return text;
    };
    const auto vertexName = [](int index) { return QChar('A' + index); };
    const auto place = [&](const QString &text, const QPointF &anchor,
                           bool outside, int vertex, int side = -1) {
        const QSize textSize = metrics.size(0, text);
        const QSizeF size(qMin<double>(textSize.width() + 12 + (vertex >= 0 ? 24 : 0),
                                       qMax(16, viewport()->width() - 8)),
                          qMax(textSize.height() + 6, vertex >= 0 ? 24 : 0));
        QPointF position = anchor;
        if (outside) {
            QPointF direction = anchor - center;
            const double length = std::hypot(direction.x(), direction.y());
            direction = length > 1e-6 ? direction / length : QPointF(-1, 1);
            position += direction * 16;
            if (direction.x() < 0)
                position.rx() -= size.width();
            if (direction.y() < 0)
                position.ry() -= size.height();
        } else {
            position -= QPointF(size.width() / 2, size.height() / 2);
        }

        QRectF best;
        double bestScore = std::numeric_limits<double>::max();
        for (int y = -3; y <= 3; ++y) {
            for (int x = -2; x <= 2; ++x) {
                QRectF candidate(position + QPointF(x * 36, y * (size.height() + 4)), size);
                candidate.moveLeft(qBound(4.0, candidate.left(),
                    qMax(4.0, viewport()->width() - size.width() - 4)));
                candidate.moveTop(qBound(4.0, candidate.top(),
                    qMax(4.0, viewport()->height() - size.height() - 4)));
                double score = QLineF(candidate.topLeft(), position).length();
                for (const MeasurementLabel &placed : labels) {
                    const QRectF overlap = candidate.intersected(
                        placed.rect.adjusted(-3, -3, 3, 3));
                    score += overlap.width() * overlap.height() * 100;
                }
                for (const QPointF &point : reservedPoints) {
                    if (candidate.adjusted(-8, -8, 8, 8).contains(point))
                        score += 100000;
                }
                if (outside && polygon.containsPoint(candidate.center(), Qt::OddEvenFill))
                    score += 10000;
                if (score < bestScore) {
                    bestScore = score;
                    best = candidate;
                }
            }
        }
        labels.append({text, best, vertex, side});
    };

    // Reserve the angle labels near their corners before placing longer text.
    for (int vertex = 0; vertex < 3; ++vertex) {
        const QLineF first(polygon[vertex], polygon[(vertex + 1) % 3]);
        const QLineF second(polygon[vertex], polygon[(vertex + 2) % 3]);
        const double angle = measurementAngleDegrees(vertex);
        double span = first.angleTo(second);
        if (span > 180)
            span -= 360;
        const double radius = qMin(30.0, qMin(first.length(), second.length()) * 0.2);
        const QPointF anchor = polygon[vertex]
            + QLineF::fromPolar(radius + 18, first.angle() + span / 2).p2();
        place(std::isfinite(angle)
                  ? QStringLiteral("%1%2").arg(angle, 0, 'f', 1).arg(QChar(0x00b0))
                  : QStringLiteral("--"),
              anchor, false, -1);
    }
    for (int vertex = 0; vertex < 3; ++vertex) {
        const QPointF point = measurementVertex(vertex);
        place(QStringLiteral("%1: x=%2, y_down=%3")
                  .arg(vertexName(vertex)).arg(number(point.x()), number(point.y())),
              polygon[vertex], true, vertex);
    }
    for (int side = 0; side < 3; ++side) {
        const int next = (side + 1) % 3;
        place(QStringLiteral("%1%2: %3 px")
                  .arg(vertexName(side)).arg(vertexName(next))
                  .arg(measurementSideLength(side), 0, 'f', 1),
              (polygon[side] + polygon[next]) / 2, true, -1, side);
    }
    if (m_measurementCircleVisible) {
        QString text = circle.valid
            ? QStringLiteral("O: x=%1, y_down=%2\nr=%3 px")
                  .arg(number(circle.center.x()), number(circle.center.y()))
                  .arg(circle.radius, 0, 'f', 1)
            : QStringLiteral("Circumcircle undefined:\naligned or coincident vertices");
        if (circle.unstable)
            text += QStringLiteral("\nNearly aligned vertices: center unstable");
        const QSize textSize = metrics.size(0, text);
        place(text, QPointF(textSize.width() / 2.0 + 14,
                           viewport()->height() - textSize.height() / 2.0 - 14), false, -1);
    }
    return labels;
}

int ImageViewer::measurementLabelVertexAt(const QPoint &viewportPos) const
{
    for (const MeasurementLabel &label : measurementLabels()) {
        if (label.vertex >= 0 && label.rect.contains(viewportPos))
            return label.vertex;
    }
    return -1;
}

QRectF ImageViewer::measurementLockRect(const MeasurementLabel &label) const
{
    return label.vertex < 0 ? QRectF()
        : QRectF(label.rect.right() - 23, label.rect.center().y() - 10, 20, 20);
}

int ImageViewer::measurementLockAt(const QPoint &viewportPos) const
{
    for (const MeasurementLabel &label : measurementLabels()) {
        if (measurementLockRect(label).contains(viewportPos))
            return label.vertex;
    }
    return -1;
}

int ImageViewer::measurementLabelSideAt(const QPoint &viewportPos) const
{
    for (const MeasurementLabel &label : measurementLabels()) {
        if (label.side >= 0 && label.rect.contains(viewportPos))
            return label.side;
    }
    return -1;
}

void ImageViewer::drawMeasurementCircumcircle(QPainter *painter) const
{
    if (!m_measurementCircleVisible)
        return;
    const MeasurementCircle circle = measurementCircumcircle();
    if (!circle.valid)
        return;
    const auto screenPoint = [this](const QPointF &point) {
        return viewportTransform().map(measurementScenePosition(point));
    };
    const QPointF center = screenPoint(circle.center);
    const QPointF xAxis = screenPoint(circle.center + QPointF(circle.radius, 0)) - center;
    const QPointF yAxis = screenPoint(circle.center + QPointF(0, circle.radius)) - center;
    const double size = std::hypot(xAxis.x(), xAxis.y()) + std::hypot(yAxis.x(), yAxis.y());
    if (!std::isfinite(size) || size <= 0)
        return;
    const QRectF clip = QRectF(viewport()->rect()).adjusted(-3, -3, 3, 3);
    const double pi = std::acos(-1.0);
    QList<double> angles{0, 2 * pi};
    const auto intersections = [&](double a, double b, double value) {
        const double amplitude = std::hypot(a, b);
        if (amplitude == 0 || std::abs(value) > amplitude)
            return;
        const double phase = std::atan2(b, a);
        const double offset = std::acos(qBound(-1.0, value / amplitude, 1.0));
        for (double angle : {phase - offset, phase + offset}) {
            angle = std::fmod(angle + 2 * pi, 2 * pi);
            angles.append(angle);
        }
    };
    intersections(xAxis.x(), yAxis.x(), clip.left() - center.x());
    intersections(xAxis.x(), yAxis.x(), clip.right() - center.x());
    intersections(xAxis.y(), yAxis.y(), clip.top() - center.y());
    intersections(xAxis.y(), yAxis.y(), clip.bottom() - center.y());
    std::sort(angles.begin(), angles.end());
    const auto sample = [&](double angle) {
        return center + xAxis * std::cos(angle) + yAxis * std::sin(angle);
    };
    const double step = qMin(0.2, std::sqrt(2.0 / size));
    QPainterPath path;
    // Sample only arcs inside the viewport, keeping even very large circles bounded.
    for (int i = 1; i < angles.size(); ++i) {
        const double start = angles[i - 1];
        const double end = angles[i];
        if (!clip.contains(sample((start + end) / 2)))
            continue;
        const int steps = static_cast<int>(qBound(1.0, std::ceil((end - start) / step), 2048.0));
        for (int j = 0; j <= steps; ++j) {
            QPointF point = sample(start + (end - start) * j / steps);
            point.setX(qBound(clip.left(), point.x(), clip.right()));
            point.setY(qBound(clip.top(), point.y(), clip.bottom()));
            if (j == 0)
                path.moveTo(point);
            else
                path.lineTo(point);
        }
    }
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(QColor(0, 0, 0, 170), 3.5));
    painter->drawPath(path);
    painter->setPen(QPen(QColor("#5ac8d8"), 1.5, Qt::DashLine));
    painter->drawPath(path);
    if (clip.contains(center)) {
        QPainterPath target;
        target.addEllipse(center, 5, 5);
        target.moveTo(center + QPointF(-9, 0));
        target.lineTo(center + QPointF(9, 0));
        target.moveTo(center + QPointF(0, -9));
        target.lineTo(center + QPointF(0, 9));
        painter->setPen(QPen(QColor(0, 0, 0, 220), 4));
        painter->drawPath(target);
        painter->setPen(QPen(QColor("#5ac8d8"), 1.5));
        painter->drawPath(target);
        painter->setPen(Qt::black);
        painter->drawText(center + QPointF(11, -7), "O");
        painter->setPen(Qt::white);
        painter->drawText(center + QPointF(10, -8), "O");
    }
}

void ImageViewer::openMeasurementVertexEditor(int vertex, const QPoint &viewportPos)
{
    if (!m_measurementVisible || vertex < 0 || vertex >= 3 || measurementVertexLocked(vertex))
        return;
    if (!m_measurementEditor) {
        auto *editor = new QFrame(viewport());
        editor->setObjectName(QStringLiteral("measurementEditor"));
        editor->setFrameShape(QFrame::StyledPanel);
        editor->setAutoFillBackground(true);
        auto *layout = new QVBoxLayout(editor);
        layout->setContentsMargins(8, 6, 8, 6);
        auto *header = new QHBoxLayout;
        m_measurementEditorTitle = new QLabel(editor);
        header->addWidget(m_measurementEditorTitle);
        header->addStretch();
        auto *apply = new QToolButton(editor);
        apply->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
        apply->setToolTip("Apply measurement changes");
        auto *cancel = new QToolButton(editor);
        cancel->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
        cancel->setToolTip("Cancel measurement changes");
        header->addWidget(apply);
        header->addWidget(cancel);
        layout->addLayout(header);
        auto *form = new QFormLayout;
        m_measurementEditorForm = form;
        const auto makeSpin = [editor](const QString &name) {
            auto *spin = new QDoubleSpinBox(editor);
            spin->setObjectName(name);
            spin->setRange(-1e9, 1e9);
            spin->setDecimals(3);
            spin->setKeyboardTracking(false);
            spin->setMinimumWidth(120);
            return spin;
        };
        m_measurementXSpin = makeSpin(QStringLiteral("measurementVertexX"));
        m_measurementYDownSpin = makeSpin(QStringLiteral("measurementVertexYDown"));
        m_measurementLengthSpin = makeSpin(QStringLiteral("measurementSideLength"));
        m_measurementLengthSpin->setRange(0.001, 1e9);
        m_measurementLengthSpin->setSuffix(" px");
        form->addRow("x:", m_measurementXSpin);
        form->addRow("y_down:", m_measurementYDownSpin);
        form->addRow("Length:", m_measurementLengthSpin);
        layout->addLayout(form);
        m_measurementEditor = editor;
        editor->installEventFilter(this);
        for (QWidget *child : editor->findChildren<QWidget*>())
            child->installEventFilter(this);
        connect(apply, &QToolButton::clicked, this, &ImageViewer::commitMeasurementEditor);
        connect(cancel, &QToolButton::clicked, this, [this]() {
            closeMeasurementEditor();
            viewport()->setFocus();
        });
    }

    m_editingMeasurementVertex = vertex;
    m_editingMeasurementSide = -1;
    m_measureDragging = false;
    m_measureDragTarget = MeasurementDragTarget::None;
    const QPointF point = measurementVertex(vertex);
    m_measurementEditorTitle->setText(QStringLiteral("Vertex %1").arg(QChar('A' + vertex)));
    m_measurementEditor->setAccessibleName(m_measurementEditorTitle->text());
    m_measurementEditorForm->setRowVisible(m_measurementXSpin, true);
    m_measurementEditorForm->setRowVisible(m_measurementYDownSpin, true);
    m_measurementEditorForm->setRowVisible(m_measurementLengthSpin, false);
    m_measurementXSpin->setValue(point.x());
    m_measurementYDownSpin->setValue(point.y());
    positionMeasurementEditor(viewportPos);
    m_measurementXSpin->setFocus();
    m_measurementXSpin->selectAll();
}

void ImageViewer::openMeasurementSideEditor(int side, const QPoint &viewportPos)
{
    if (!canEditMeasurementSide(side))
        return;
    const int next = (side + 1) % 3;
    const int fixed = m_measurementLocks[side] ? side : next;
    openMeasurementVertexEditor(fixed == side ? next : side, viewportPos);
    m_editingMeasurementVertex = -1;
    m_editingMeasurementSide = side;
    m_measurementEditorTitle->setText(QStringLiteral("%1%2 (%3 fixed)")
        .arg(QChar('A' + side)).arg(QChar('A' + next)).arg(QChar('A' + fixed)));
    m_measurementEditor->setAccessibleName(m_measurementEditorTitle->text());
    m_measurementEditorForm->setRowVisible(m_measurementXSpin, false);
    m_measurementEditorForm->setRowVisible(m_measurementYDownSpin, false);
    m_measurementEditorForm->setRowVisible(m_measurementLengthSpin, true);
    m_measurementLengthSpin->setValue(measurementSideLength(side));
    positionMeasurementEditor(viewportPos);
    m_measurementLengthSpin->setFocus();
    m_measurementLengthSpin->selectAll();
}

void ImageViewer::positionMeasurementEditor(const QPoint &viewportPos)
{
    m_measurementEditor->adjustSize();
    m_measurementEditor->move(
        qBound(4, viewportPos.x() + 10,
               qMax(4, viewport()->width() - m_measurementEditor->width() - 4)),
        qBound(4, viewportPos.y() + 10,
               qMax(4, viewport()->height() - m_measurementEditor->height() - 4)));
    m_measurementEditor->show();
    m_measurementEditor->raise();
}

void ImageViewer::closeMeasurementEditor()
{
    m_editingMeasurementVertex = -1;
    m_editingMeasurementSide = -1;
    if (m_measurementEditor)
        m_measurementEditor->hide();
}

void ImageViewer::commitMeasurementEditor()
{
    if (m_editingMeasurementSide >= 0) {
        if (!m_measurementLengthSpin->hasAcceptableInput())
            return;
        m_measurementLengthSpin->interpretText();
        if (setMeasurementSideLength(m_editingMeasurementSide, m_measurementLengthSpin->value())) {
            closeMeasurementEditor();
            viewport()->setFocus();
        }
        return;
    }
    const int vertex = m_editingMeasurementVertex;
    if (vertex < 0)
        return;
    if (!m_measurementXSpin->hasAcceptableInput()
        || !m_measurementYDownSpin->hasAcceptableInput()) {
        return;
    }
    m_measurementXSpin->interpretText();
    m_measurementYDownSpin->interpretText();
    const QPointF position(m_measurementXSpin->value(), m_measurementYDownSpin->value());
    closeMeasurementEditor();
    setMeasurementVertex(vertex, position);
    viewport()->setFocus();
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
    if (handleMeasureDistanceKeyEvent(event))
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
    closeMeasurementEditor();
    QGraphicsView::resizeEvent(event);
    if (m_activePanelIndicatorOverlay) {
        m_activePanelIndicatorOverlay->setGeometry(viewport()->geometry());
        m_activePanelIndicatorOverlay->raise();
    }
    updateEmptyStateOverlay();
    if (m_autoFitImage)
        fitImageToView();
}

void ImageViewer::hideEvent(QHideEvent *event)
{
    closeMeasurementEditor();
    clearTemporaryInteractionState();
    QGraphicsView::hideEvent(event);
}

void ImageViewer::updateEmptyStateOverlay()
{
    if (!m_emptyStateOverlay || !m_layerManager)
        return;

    const bool empty = m_layerManager->layerCount() == 0;
    m_emptyStateOverlay->setVisible(empty);
    if (!empty)
        return;

    m_emptyStateOverlay->adjustSize();
    const int x = qMax(0,
        (viewport()->width() - m_emptyStateOverlay->width()) / 2);
    const int y = qMax(0,
        (viewport()->height() - m_emptyStateOverlay->height()) / 2);
    m_emptyStateOverlay->move(x, y);
    m_emptyStateOverlay->raise();
}

void ImageViewer::leaveEvent(QEvent *event)
{
    m_maskBrushPreviewVisible = false;
    viewport()->update();
    m_cursorInside = false;
    emit cursorCoordinateInfoChanged(CursorCoordinateInfo{});
    QGraphicsView::leaveEvent(event);
}

void ImageViewer::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);

    if (m_measurementVisible) {
        const QPolygonF polygon = measurementScreenPolygon();
        painter->save();
        painter->resetTransform();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setFont(viewport()->font());
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 14));
        painter->drawPolygon(polygon);
        drawMeasurementCircumcircle(painter);

        const auto drawOutlinedPath = [painter](const QPainterPath &path) {
            QPen shadowPen(QColor(0, 0, 0, 190), 5.0);
            shadowPen.setCapStyle(Qt::RoundCap);
            painter->setPen(shadowPen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(path);
            QPen linePen(QColor(255, 255, 255, 245), 2.0);
            linePen.setCapStyle(Qt::RoundCap);
            painter->setPen(linePen);
            painter->drawPath(path);
        };

        QPainterPath outline;
        outline.addPolygon(polygon);
        outline.closeSubpath();
        drawOutlinedPath(outline);

        for (int vertex = 0; vertex < 3; ++vertex) {
            if (!std::isfinite(measurementAngleDegrees(vertex)))
                continue;
            const QLineF first(polygon[vertex], polygon[(vertex + 1) % 3]);
            const QLineF second(polygon[vertex], polygon[(vertex + 2) % 3]);
            const double radius = qMin(30.0, qMin(first.length(), second.length()) * 0.2);
            double span = first.angleTo(second);
            if (span > 180)
                span -= 360;
            const QRectF arcRect(polygon[vertex] - QPointF(radius, radius),
                                 QSizeF(radius * 2, radius * 2));
            QPainterPath arc;
            arc.arcMoveTo(arcRect, first.angle());
            arc.arcTo(arcRect, first.angle(), span);
            drawOutlinedPath(arc);
        }

        painter->setPen(QPen(QColor(0, 0, 0, 210), 2.0));
        painter->setBrush(QColor(255, 255, 255, 245));
        constexpr double endpointRadius = 4.5;
        for (const QPointF &point : polygon)
            painter->drawEllipse(point, endpointRadius, endpointRadius);

        const QFontMetrics metrics(painter->font());
        for (const MeasurementLabel &label : measurementLabels()) {
            painter->setPen(QPen(QColor(0, 0, 0, 180), 1.0));
            painter->setBrush(QColor(255, 255, 255, 235));
            painter->drawRoundedRect(label.rect, 3.0, 3.0);
            painter->setPen(QColor(25, 25, 25));
            QRectF textRect = label.rect;
            if (label.vertex >= 0)
                textRect.adjust(0, 0, -24, 0);
            QStringList lines = label.text.split('\n');
            for (QString &line : lines)
                line = metrics.elidedText(line, Qt::ElideMiddle,
                    qMax(1, static_cast<int>(textRect.width()) - 12));
            painter->drawText(textRect, Qt::AlignCenter, lines.join('\n'));
            if (label.vertex >= 0)
                drawMeasurementLock(painter, measurementLockRect(label), measurementVertexLocked(label.vertex));
        }
        painter->restore();
    }

    if (!m_maskBrushPreviewVisible
        || !m_maskEditEnabled
        || !m_editingMaskLayer
        || m_measureDistanceEnabled
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
        shouldPan || zoomInteractionActive()
        || m_measureDistanceEnabled || m_maskEditEnabled);
    if (shouldPan) {
        viewport()->setCursor(Qt::OpenHandCursor);
    } else if (zoomInteractionActive()) {
        viewport()->setCursor(zoomCursor(m_zoomCursorDirection));
    } else if (m_measureDistanceEnabled) {
        viewport()->setCursor(Qt::CrossCursor);
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

    setSplitSpacePanActive(event->type() == QEvent::KeyPress);
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

    setSplitTemporaryZoomActive(event->type() == QEvent::KeyPress);
    keyEvent->accept();
    return true;
}

void ImageViewer::setSpacePanActive(bool active)
{
    if (m_spacePanActive == active)
        return;

    if (active)
        closeMeasurementEditor();
    if (active && m_measureDragging) {
        m_measureDragging = false;
        m_measureDragTarget = MeasurementDragTarget::None;
    }
    m_spacePanActive = active;
    updateDragMode();
}

void ImageViewer::setSplitSpacePanActive(bool active)
{
    auto *splitter = qobject_cast<QSplitter*>(parentWidget());
    if (!splitter) {
        setSpacePanActive(active);
        return;
    }

    for (int i = 0; i < splitter->count(); ++i) {
        if (auto *viewer = qobject_cast<ImageViewer*>(splitter->widget(i)))
            viewer->setSpacePanActive(active);
    }
}

void ImageViewer::setTemporaryZoomActive(bool active)
{
    if (m_temporaryZoomActive == active)
        return;

    if (active)
        closeMeasurementEditor();
    if (active && m_measureDragging) {
        m_measureDragging = false;
        m_measureDragTarget = MeasurementDragTarget::None;
    }
    m_temporaryZoomActive = active;
    if (!zoomInteractionActive())
        m_zoomDragging = false;
    updateDragMode();
}

void ImageViewer::setSplitTemporaryZoomActive(bool active)
{
    auto *splitter = qobject_cast<QSplitter*>(parentWidget());
    if (!splitter) {
        setTemporaryZoomActive(active);
        return;
    }

    for (int i = 0; i < splitter->count(); ++i) {
        if (auto *viewer = qobject_cast<ImageViewer*>(splitter->widget(i)))
            viewer->setTemporaryZoomActive(active);
    }
}

void ImageViewer::clearTemporaryInteractionState()
{
    closeMeasurementEditor();
    m_measureDragging = false;
    m_measureDragTarget = MeasurementDragTarget::None;
    setSpacePanActive(false);
    setTemporaryZoomActive(false);
}

void ImageViewer::clearMeasurement()
{
    closeMeasurementEditor();
    m_measureDragging = false;
    m_measurementVisible = false;
    m_measurementLocks.fill(false);
    m_measureDragTarget = MeasurementDragTarget::None;
    updateMeasurementCursor(m_lastMousePos);
    viewport()->update();
    emit measurementChanged();
}

void ImageViewer::initializeMeasurement(const QPoint &viewportPos)
{
    const double armSize = qBound(16.0,
        qMin(viewport()->width(), viewport()->height()) * 0.32, 180.0);
    const QPointF start = QPointF(viewportPos) + QPointF(-armSize / 3, armSize / 3);
    const QTransform inverse = viewportTransform().inverted();
    m_measurementVertices = {inverse.map(start),
                            inverse.map(start + QPointF(armSize, 0)),
                            inverse.map(start + QPointF(0, -armSize))};
    m_measurementLocks.fill(false);
    m_measurementVisible = true;
    viewport()->update();
    emit measurementChanged();
}

void ImageViewer::moveMeasurementHandle(const QPoint &viewportPos)
{
    const QPointF scenePos = mapToScene(viewportPos);
    const QPointF delta = scenePos - m_measureDragLastScenePos;
    m_measureDragLastScenePos = scenePos;
    if (m_measureDragTarget == MeasurementDragTarget::Triangle) {
        translateMeasurement(delta);
    } else if (m_measureDragTarget >= MeasurementDragTarget::VertexA
               && m_measureDragTarget <= MeasurementDragTarget::VertexC) {
        const int vertex = static_cast<int>(m_measureDragTarget);
        if (measurementVertexLocked(vertex))
            return;
        m_measurementVertices[vertex] += delta;
        viewport()->update();
        emit measurementChanged();
    }
}

ImageViewer::MeasurementDragTarget ImageViewer::measurementEndpointAt(
    const QPoint &viewportPos, bool includeLocked) const
{
    if (!m_measurementVisible)
        return MeasurementDragTarget::None;

    constexpr double HitRadius = 10.0;
    const QPointF cursorPos(viewportPos);
    MeasurementDragTarget closest = MeasurementDragTarget::None;
    double closestDistance = HitRadius;
    const auto consider = [&](const QPointF &point, MeasurementDragTarget target) {
        if (!includeLocked && measurementVertexLocked(static_cast<int>(target)))
            return;
        const double distance = QLineF(cursorPos, viewportTransform().map(point)).length();
        if (distance <= HitRadius
            && (closest == MeasurementDragTarget::None || distance < closestDistance)) {
            closestDistance = distance;
            closest = target;
        }
    };
    consider(m_measurementVertices[1], MeasurementDragTarget::VertexB);
    consider(m_measurementVertices[2], MeasurementDragTarget::VertexC);
    consider(m_measurementVertices[0], MeasurementDragTarget::VertexA);
    return closest;
}

void ImageViewer::updateMeasurementCursor(const QPoint &viewportPos)
{
    if (!m_measureDistanceEnabled
        || panInteractionActive()
        || zoomInteractionActive()) {
        return;
    }

    Qt::CursorShape cursor = Qt::CrossCursor;
    if (m_measureDragging) {
        cursor = m_measureDragTarget == MeasurementDragTarget::Triangle
            ? Qt::ClosedHandCursor : Qt::SizeAllCursor;
    } else if (measurementLockAt(viewportPos) >= 0) {
        cursor = Qt::PointingHandCursor;
    } else if (measurementEndpointAt(viewportPos, true) != MeasurementDragTarget::None) {
        cursor = measurementVertexLocked(static_cast<int>(measurementEndpointAt(viewportPos, true)))
            ? Qt::ForbiddenCursor : Qt::SizeAllCursor;
    } else if (m_measurementVisible) {
        bool onLabel = false;
        for (const MeasurementLabel &label : measurementLabels()) {
            if (label.rect.contains(viewportPos)) {
                const bool editable = (label.vertex >= 0 && !measurementVertexLocked(label.vertex))
                    || (label.side >= 0 && canEditMeasurementSide(label.side));
                cursor = editable ? Qt::IBeamCursor : Qt::ArrowCursor;
                onLabel = true;
                break;
            }
        }
        if (!onLabel && measurementScreenPolygon().containsPoint(viewportPos, Qt::OddEvenFill))
            cursor = measurementTranslationAllowed() ? Qt::OpenHandCursor : Qt::ForbiddenCursor;
    }
    viewport()->setCursor(cursor);
}

bool ImageViewer::handleMeasureDistanceKeyEvent(QKeyEvent *event)
{
    if (!m_measureDistanceEnabled
        || event->key() != Qt::Key_Escape
        || event->isAutoRepeat()) {
        return false;
    }

    clearMeasurement();
    event->accept();
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

    if (m_measureDistanceEnabled
        && !panInteractionActive()
        && !zoomInteractionActive()
        && event->button() == Qt::LeftButton) {
        closeMeasurementEditor();
        if (!m_measurementVisible)
            initializeMeasurement(event->pos());
        const int lock = measurementLockAt(event->pos());
        if (lock >= 0) {
            setMeasurementVertexLocked(lock, !measurementVertexLocked(lock));
            event->accept();
            return;
        }
        m_measureDragTarget = measurementEndpointAt(event->pos());
        if (m_measureDragTarget == MeasurementDragTarget::None) {
            bool onLabel = false;
            for (const MeasurementLabel &label : measurementLabels())
                onLabel |= label.rect.contains(event->pos());
            if (!onLabel && measurementTranslationAllowed()
                && measurementScreenPolygon().containsPoint(event->pos(), Qt::OddEvenFill))
                m_measureDragTarget = MeasurementDragTarget::Triangle;
        }
        m_measureDragging = m_measureDragTarget != MeasurementDragTarget::None;
        m_measureDragLastScenePos = mapToScene(event->pos());
        updateMeasurementCursor(event->pos());
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
                m_editingPointLayer->addPoint(scenePos.x(), scenePos.y());
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

    if (m_measureDistanceEnabled
        && !panInteractionActive()
        && !zoomInteractionActive()
        && event->button() == Qt::LeftButton) {
        if (measurementLockAt(event->pos()) >= 0) {
            event->accept();
            return;
        }
        const int side = measurementLabelSideAt(event->pos());
        if (side >= 0) {
            openMeasurementSideEditor(side, event->pos());
            event->accept();
            return;
        }
        int vertex = measurementLabelVertexAt(event->pos());
        if (vertex < 0) {
            const MeasurementDragTarget handle = measurementEndpointAt(event->pos());
            if (handle >= MeasurementDragTarget::VertexA
                && handle <= MeasurementDragTarget::VertexC) {
                vertex = static_cast<int>(handle);
            }
        }
        if (vertex >= 0)
            openMeasurementVertexEditor(vertex, event->pos());
        event->accept();
        return;
    }

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
    closeMeasurementEditor();
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

void ImageViewer::emitCursorCoordinateInfo(const QPoint &viewportPos)
{
    CursorCoordinateInfo info;
    if (!viewport() || !viewport()->rect().contains(viewportPos)) {
        emit cursorCoordinateInfoChanged(info);
        return;
    }

    const QPointF scenePos = mapToScene(viewportPos);
    QPointF coordinatePos = scenePos;
    ImageLayer *reference = m_coordinateReferenceImage.data();
    if (reference && reference->pixmapItem()) {
        const QRectF bounds = reference->pixmapItem()->boundingRect();
        const QPointF imagePos =
            reference->pixmapItem()->mapFromScene(scenePos);
        coordinatePos = QPointF(
            imagePos.x() - bounds.left(),
            imagePos.y() - bounds.top());
    }

    info.valid = true;
    info.x = coordinatePos.x();
    info.yDown = coordinatePos.y();
    if (info.x >= 0.0 && info.yDown >= 0.0) {
        info.col = static_cast<int>(std::floor(info.x)) + 1;
        info.row = static_cast<int>(std::floor(info.yDown)) + 1;
    }

    if (reference && reference->rasterData().height() > 0) {
        info.yAvailable = true;
        info.y = reference->rasterData().height() - info.yDown;
    }

    emit cursorCoordinateInfoChanged(info);
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    m_cursorInside = true;
    emitCursorCoordinateInfo(event->pos());
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

    if (m_measureDragging
        && event->buttons().testFlag(Qt::LeftButton)
        && !panInteractionActive()
        && !zoomInteractionActive()) {
        moveMeasurementHandle(event->pos());
        event->accept();
        return;
    }

    if (m_measureDistanceEnabled
        && !panInteractionActive()
        && !zoomInteractionActive()) {
        updateMeasurementCursor(event->pos());
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
    if (m_measureDragging && event->button() == Qt::LeftButton) {
        if (!panInteractionActive() && !zoomInteractionActive())
            moveMeasurementHandle(event->pos());
        m_measureDragging = false;
        m_measureDragTarget = MeasurementDragTarget::None;
        updateMeasurementCursor(event->pos());
        viewport()->update();
        event->accept();
        return;
    }

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
        const QPointF point(points[i].x, points[i].yDown);
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
    auto *widget = qobject_cast<QWidget*>(obj);
    if (m_measurementEditor && m_measurementEditor->isVisible() && widget
        && (widget == m_measurementEditor || m_measurementEditor->isAncestorOf(widget))) {
        if (event->type() == QEvent::MouseButtonPress)
            emit activatedByClick();
        if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent*>(event)->key();
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                commitMeasurementEditor();
                return true;
            }
            if (key == Qt::Key_Escape) {
                closeMeasurementEditor();
                viewport()->setFocus();
                return true;
            }
        }
    }
    if (obj == viewport()) {
        if (event->type() == QEvent::ToolTip && m_measurementVisible
            && !panInteractionActive() && !zoomInteractionActive()) {
            auto *help = static_cast<QHelpEvent*>(event);
            QString text;
            const int lock = measurementLockAt(help->pos());
            int vertex = measurementLabelVertexAt(help->pos());
            if (vertex < 0)
                vertex = static_cast<int>(measurementEndpointAt(help->pos(), true));
            const int side = measurementLabelSideAt(help->pos());
            if (lock >= 0) {
                text = QStringLiteral("%1 vertex %2")
                    .arg(measurementVertexLocked(lock) ? "Unlock" : "Lock").arg(QChar('A' + lock));
            } else if (vertex >= 0) {
                text = measurementVertexLocked(vertex)
                    ? QStringLiteral("Vertex %1 is locked. Unlock it to move or edit it.").arg(QChar('A' + vertex))
                    : QStringLiteral("Drag to move this vertex. Double-click to edit x and y_down.");
            } else if (side >= 0) {
                text = canEditMeasurementSide(side)
                    ? QStringLiteral("Double-click to edit this length. The locked endpoint stays fixed.")
                    : measurementSideLength(side) <= 1e-9
                        ? QStringLiteral("Separate the coincident vertices to define a direction.")
                        : QStringLiteral("Lock exactly one endpoint to edit this length.");
            } else {
                for (const MeasurementLabel &label : measurementLabels()) {
                    if (label.rect.contains(help->pos()))
                        text = label.text;
                }
                if (text.isEmpty() && measurementScreenPolygon().containsPoint(help->pos(), Qt::OddEvenFill))
                    text = measurementTranslationAllowed()
                        ? "Drag inside the triangle to move it."
                        : "Unlock all three vertices to move the whole triangle.";
            }
            if (text.isEmpty())
                QToolTip::hideText();
            else
                QToolTip::showText(help->globalPos(), text, viewport());
            return true;
        }
        if ((event->type() == QEvent::Resize
             || event->type() == QEvent::Move)
            && m_activePanelIndicatorOverlay) {
            m_activePanelIndicatorOverlay->setGeometry(viewport()->geometry());
            m_activePanelIndicatorOverlay->raise();
        }
        if (event->type() == QEvent::Enter
            && (!m_measurementEditor || !m_measurementEditor->isVisible())) {
            setFocus(Qt::MouseFocusReason);
            viewport()->setFocus(Qt::MouseFocusReason);
        }
        if (handleSpacePanKeyEvent(event))
            return true;
        if (handleTemporaryZoomKeyEvent(event))
            return true;
        if ((event->type() == QEvent::KeyPress)
            && handleMeasureDistanceKeyEvent(
                static_cast<QKeyEvent*>(event))) {
            return true;
        }
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
    closeMeasurementEditor();
    QGraphicsView::scrollContentsBy(dx, dy);

    // Viewport-positioned labels cannot reuse the pixels copied by scrolling.
    if (m_measurementVisible)
        viewport()->update();

    if (m_syncingScroll || !ms_syncEnabled)
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
