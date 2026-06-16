#include "imageviewer.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QPixmap>

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

void ImageViewer::setImage(const QString &fileName)
{
    QPixmap pixmap(fileName);
    if (pixmap.isNull()) {
        return;
    }

    m_scene->clear();
    m_imageItem = m_scene->addPixmap(pixmap);
    m_scene->setSceneRect(m_imageItem->boundingRect());

    resetTransform();
    fitInView(
        m_imageItem,
        Qt::KeepAspectRatio
        );
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

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    constexpr double zoomFactor = 1.15;

    if (event->angleDelta().y() > 0) {
        scale(zoomFactor, zoomFactor);
    } else {
        scale(1.0 / zoomFactor, 1.0 / zoomFactor);
    }
}

void ImageViewer::updateDragMode()
{
    const bool shouldPan = m_panModeEnabled || m_spacePanActive;

    setDragMode(shouldPan ? QGraphicsView::ScrollHandDrag : QGraphicsView::NoDrag);
    viewport()->setCursor(shouldPan ? Qt::OpenHandCursor : Qt::ArrowCursor);
}
