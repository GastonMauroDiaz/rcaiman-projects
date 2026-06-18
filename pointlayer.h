#ifndef POINTLAYER_H
#define POINTLAYER_H

#include "layer.h"
#include "pointcollection.h"

class PointLayer : public Layer
{
    Q_OBJECT

public:
    explicit PointLayer(const QString &path, QObject *parent = nullptr);

    const QVector<QGraphicsItem*> &buildItems(QGraphicsScene *scene) override;

    const PointCollection &collection() const { return m_collection; }

private:
    PointCollection m_collection;
};

#endif // POINTLAYER_H