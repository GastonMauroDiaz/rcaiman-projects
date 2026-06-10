#include "imageviewer.h"

#include <QGraphicsScene>
#include <QPixmap>
#include <QDebug>

ImageViewer::ImageViewer(QWidget *parent)
    : QGraphicsView(parent)
{
}

void ImageViewer::setImage(const QString &fileName)
{
    auto *scene = new QGraphicsScene(this);

    QPixmap pixmap(fileName);

    scene->addPixmap(pixmap);

    setScene(scene);

    fitInView(
        scene->itemsBoundingRect(),
        Qt::KeepAspectRatio
        );
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
   qDebug() << "wheelEvent";

    constexpr double zoomFactor = 1.15;

    if (event->angleDelta().y() > 0) {
        scale(zoomFactor, zoomFactor);
    } else {
        scale(1.0 / zoomFactor, 1.0 / zoomFactor);
    }
}
