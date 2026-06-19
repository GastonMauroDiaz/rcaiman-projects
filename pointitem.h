#ifndef POINTITEM_H
#define POINTITEM_H

#include <QObject>
#include <QGraphicsEllipseItem>
#include <QPointF>

class PointItem : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT

public:
    explicit PointItem(int index, QGraphicsItem *parent = nullptr);

    void setPointIndex(int index);
    int pointIndex() const { return m_pointIndex; }

    void setMovable(bool movable);

signals:
    void clicked(int index);
    void positionChanged(int index, QPointF pos);
    void deleteRequested(int index);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    int m_pointIndex;
    bool m_dragging = false;
    QPointF m_dragStartPos;
};

#endif // POINTITEM_H