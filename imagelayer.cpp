#include "imagelayer.h"
#include "tiffreader.h"

#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QMessageBox>
#include <QWidget>

ImageLayer::ImageLayer(const QString &filePath, QObject *parent)
    : Layer(QFileInfo(filePath).fileName(), filePath, parent)
{
    QString errorMessage;
    m_rasterData = TiffReader::load(filePath, &errorMessage);
    if (m_rasterData.isNull()) {
        qWarning() << "ImageLayer: could not load" << filePath << errorMessage;
    }
}

const QVector<QGraphicsItem*> &ImageLayer::buildItems(QGraphicsScene *scene)
{
    if (m_itemsBuilt)
        return m_items;

    if (m_rasterData.isNull())
        return m_items;

    const QImage image = m_renderer.renderAuto(m_rasterData);
    if (image.isNull()) {
        qWarning() << "ImageLayer: could not render" << sourcePath();
        return m_items;
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    m_pixmapItem = scene->addPixmap(pixmap);
    m_pixmapItem->setVisible(isVisible());
    m_items.append(m_pixmapItem);

    m_itemsBuilt = true;
    return m_items;
}