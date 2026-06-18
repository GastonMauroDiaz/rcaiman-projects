#include "imageviewer.h"
#include "imagelayer.h"
#include "layermanager.h"
#include "layer.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMessageBox>

ImageViewer::ImageViewer(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
    , m_layerManager(new LayerManager(this))
{
    setScene(m_scene);
    m_layerManager->attachToScene(m_scene);

    connect(m_layerManager, &LayerManager::layersChanged, this, [this]() {
        m_imageLayer = nullptr;
        for (int i = 0; i < m_layerManager->layerCount(); ++i) {
            if (auto *img = qobject_cast<ImageLayer*>(m_layerManager->layerAt(i))) {
                m_imageLayer = img;
                break;
            }
        }
    });
    setDragMode(QGraphicsView::NoDrag);
    setFocusPolicy(Qt::StrongFocus);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
}

void ImageViewer::setImage(const QString &fileName)
{
    m_layerManager->clearAll();

    auto *imageLayer = new ImageLayer(fileName);
    if (imageLayer->rasterData().isNull()) {
        delete imageLayer;
        QMessageBox::warning(this, "Open Image",
                             QStringLiteral("Could not load image:\n%1").arg(fileName));
        return;
    }

    m_imageLayer = imageLayer;
    m_layerManager->addLayer(imageLayer);

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
    if (!m_imageLayer || !m_imageLayer->pixmapItem())
        return;
    resetTransform();
    fitInView(m_imageLayer->pixmapItem(), Qt::KeepAspectRatio);
}

void ImageViewer::updateDragMode()
{
    const bool shouldPan = m_panModeEnabled || m_spacePanActive;
    setDragMode(shouldPan ? QGraphicsView::ScrollHandDrag : QGraphicsView::NoDrag);
    viewport()->setCursor(shouldPan ? Qt::OpenHandCursor : Qt::ArrowCursor);
}