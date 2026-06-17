#include "imageviewer.h"
#include "tiffreader.h"

#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QMessageBox>
#include <QPen>
#include <QColor>

ImageViewer::ImageViewer(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setDragMode(QGraphicsView::NoDrag);
    setFocusPolicy(Qt::StrongFocus);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
}

void ImageViewer::setPointCollection(const PointCollection &collection)
{
    // Remove existing point items
    for (QGraphicsEllipseItem *item : std::as_const(m_pointItems)) {
        delete item;
    }
    m_pointItems.clear();

    if (!collection.isValid())
        return;

    const double width = 3.0;
    const double height = 3.0;
    const QPointF offset(-width / 2.0, -height / 2.0);

    for (const PointRecord &rec : collection.points()) {
        auto *ellipse = new QGraphicsEllipseItem(offset.x(), offset.y(), width, height);
        ellipse->setPen(QPen(Qt::red));
        ellipse->setBrush(QBrush(Qt::red));
        // scene_x = col, scene_y = row
        ellipse->setPos(rec.col, rec.row);
        m_scene->addItem(ellipse);
        m_pointItems.append(ellipse);
    }
}

void ImageViewer::setImage(const QString &fileName)
{
    QString errorMessage;
    RasterData data = TiffReader::load(fileName, &errorMessage);

    if (data.isNull()) {
        QMessageBox::warning(this, "Open Image",
                             QStringLiteral("Could not load image:\n%1\n\n%2")
                                 .arg(fileName, errorMessage));
        return;
    }

    m_rasterData = std::move(data);
    updateDisplay();
}

void ImageViewer::updateDisplay()
{
    if (m_rasterData.isNull())
        return;

    const QImage image = m_renderer.renderAuto(m_rasterData);
    if (image.isNull()) {
        QMessageBox::warning(this, "Render Error", "Could not render image.");
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    m_scene->clear();
    for (QGraphicsEllipseItem *item : std::as_const(m_pointItems)) {
        delete item;
    }
    m_pointItems.clear();
    m_imageItem = m_scene->addPixmap(pixmap);
    m_scene->setSceneRect(m_imageItem->boundingRect());
    m_autoFitImage = true;
    fitImageToView();
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

void ImageViewer::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePanActive = true;
        updateDragMode();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void ImageViewer::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePanActive = false;
        updateDragMode();
        event->accept();
        return;
    }
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
}

void ImageViewer::fitImageToView()
{
    if (!m_imageItem)
        return;
    resetTransform();
    fitInView(m_imageItem, Qt::KeepAspectRatio);
}

void ImageViewer::updateDragMode()
{
    const bool shouldPan = m_panModeEnabled || m_spacePanActive;
    setDragMode(shouldPan ? QGraphicsView::ScrollHandDrag : QGraphicsView::NoDrag);
    viewport()->setCursor(shouldPan ? Qt::OpenHandCursor : Qt::ArrowCursor);
}