#include "imageviewer.h"
#include "imagelayer.h"
#include "layermanager.h"
#include "layer.h"
#include "pointitem.h"
#include "pointlayer.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>

ImageViewer::ImageViewer(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
    , m_layerManager(new LayerManager(this))
{
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    viewport()->installEventFilter(this);
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

void ImageViewer::setPointEditEnabled(bool enabled)
{
    m_pointEditEnabled = enabled;

    if (!m_layerManager)
        return;

    Layer *active = m_layerManager->activeLayer();
    if (auto *pointLayer = qobject_cast<PointLayer*>(active)) {
        pointLayer->setEditMode(enabled);
    }
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

void ImageViewer::mousePressEvent(QMouseEvent *event)
{
    if (m_pointEditEnabled && event->button() == Qt::LeftButton) {
        const QPointF scenePos = mapToScene(event->pos());
        QGraphicsItem *itemUnder = m_scene ? m_scene->itemAt(scenePos, QTransform()) : nullptr;
        const bool isPointItem = itemUnder && (itemUnder->type() == QGraphicsEllipseItem::Type);
        if (!isPointItem) {
            Layer *active = m_layerManager ? m_layerManager->activeLayer() : nullptr;
            if (auto *pointLayer = qobject_cast<PointLayer*>(active)) {
                pointLayer->addPoint(scenePos.y(), scenePos.x());
                event->accept();
                return;
            }
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event)
{
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

    emit pixelInfoChanged(info);
    QGraphicsView::mouseMoveEvent(event);
}

// --- Drag & Drop ---
// QGraphicsView's viewport() receives drop events first.
// We use an event filter on the viewport to intercept them,
// because setAcceptDrops(true) on QGraphicsView alone is not
// sufficient: the viewport widget overrides drop behavior.

bool ImageViewer::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == viewport()) {
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
            // Replace content of this viewer
            setImage(path);
        } else if (csvExts.contains(suffix)) {
            if (!m_imageLayer) {
                QMessageBox::information(this, "Add Points",
                    "Load an image first before adding a point layer.");
                continue;
            }
            auto *layer = new PointLayer(path);
            m_layerManager->addLayer(layer);
        }
    }
    event->acceptProposedAction();
}
