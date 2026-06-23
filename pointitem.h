#ifndef POINTITEM_H
#define POINTITEM_H

#include "layerstyle.h"

#include <QObject>
#include <QGraphicsEllipseItem>
#include <QStyleOptionGraphicsItem>
#include <QPointF>

class QPainter;

class PointItem : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT

public:
    explicit PointItem(int index, QGraphicsItem *parent = nullptr);

    void setPointIndex(int index);
    int pointIndex() const { return m_pointIndex; }

    void setMovable(bool movable);
    void setSymbol(PointSymbol symbol);

signals:
    void clicked(int index);
    void positionChanged(int index, QPointF pos);
    void deleteRequested(int index);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    int m_pointIndex;
    PointSymbol m_symbol = PointSymbol::Circle;
    bool m_dragging = false;
    QPointF m_dragStartPos;
};

#endif // POINTITEM_H
