#include "imageviewer.h"
#include "tiffreader.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QMessageBox>

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
    clearOverlays();
    m_pointCollection = collection;
    if (collection.isValid())
        addOverlay(collection);
}

void ImageViewer::addOverlay(const Overlay &overlay)
{
    overlay.addToScene(m_scene);
}

void ImageViewer::clearOverlays()
{
    for (QGraphicsItem *item : std::as_const(m_overlayItems)) {
        delete item;
    }
    m_overlayItems.clear();
    m_pointCollection = PointCollection();
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
    clearOverlays();
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