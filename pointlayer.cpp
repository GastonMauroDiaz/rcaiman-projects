#include "pointlayer.h"

#include <QDir>
#include <QFileInfo>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QPen>
#include <QColor>

PointLayer::PointLayer(const QString &path, QObject *parent)
    : Layer(QFileInfo(path).fileName(), path, parent)
{
    QFileInfo fi(path);
    if (fi.isFile() && fi.suffix().toLower() == "csv") {
        // Modo dock: cargar solo este CSV
        m_collection = PointCollection::load(path);
    } else {
        // Modo autoload u otro: cargar todos los CSV de la carpeta
        m_collection = PointCollection::loadFromDirectory(path);
    }
}

const QVector<QGraphicsItem*> &PointLayer::buildItems(QGraphicsScene *scene)
{
    if (m_itemsBuilt)
        return m_items;

    if (m_collection.isValid()) {
        // Create visual items
        const double width = 6.0;
        const double height = 6.0;
        const QPointF offset(-width / 2.0, -height / 2.0);

        for (const PointRecord &rec : m_collection.points()) {
            auto *ellipse = new QGraphicsEllipseItem(offset.x(), offset.y(), width, height);
            ellipse->setPen(QPen(Qt::yellow));
            ellipse->setBrush(QBrush(Qt::yellow));
            ellipse->setPos(rec.col, rec.row);
            ellipse->setVisible(isVisible());
            scene->addItem(ellipse);
            m_items.append(ellipse);
        }
        qDebug() << "PointLayer: loaded" << m_collection.points().size() << "points from" << sourcePath();
    } else {
        qWarning() << "PointLayer: no valid points in" << sourcePath();
    }

    m_itemsBuilt = true;
    return m_items;
}