#ifndef POINTITEM_H
#define POINTITEM_H

#include "layerstyle.h"

#include <QObject>
#include <QGraphicsEllipseItem>
#include <QStyleOptionGraphicsItem>
#include <QPointF>

class QPainter;
class PointLabelItem;

class PointItem : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT

public:
    explicit PointItem(int index, QGraphicsItem *parent = nullptr);

    void setPointIndex(int index);
    int pointIndex() const { return m_pointIndex; }

    void setMovable(bool movable);
    void setSymbol(PointSymbol symbol);
    void setLabel(const QString &text, double pointSize);

signals:
    void clicked(int index, Qt::KeyboardModifiers modifiers);
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
    PointLabelItem *m_labelItem = nullptr;
    bool m_dragging = false;
    QPointF m_dragStartPos;
};

#endif // POINTITEM_H
