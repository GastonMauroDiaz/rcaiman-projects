#include "pointitem.h"

#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>

PointItem::PointItem(int index, QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsEllipseItem(parent)
    , m_pointIndex(index)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable, false);  // off by default; setMovable enables it
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, false);
    setCursor(Qt::OpenHandCursor);
}

void PointItem::setPointIndex(int index)
{
    m_pointIndex = index;
}

void PointItem::setMovable(bool movable)
{
    setFlag(QGraphicsItem::ItemIsMovable, movable);
}

void PointItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(Qt::OpenHandCursor);
    QGraphicsEllipseItem::hoverEnterEvent(event);
}

void PointItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QGraphicsEllipseItem::hoverLeaveEvent(event);
}

void PointItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->modifiers() & Qt::AltModifier) {
        emit deleteRequested(m_pointIndex);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // Always emit clicked (regardless of movable state)
        emit clicked(m_pointIndex);

        if (flags() & QGraphicsItem::ItemIsMovable) {
            m_dragging = true;
            m_dragStartPos = pos();
            setCursor(Qt::ClosedHandCursor);
        }
    }

    QGraphicsEllipseItem::mousePressEvent(event);
}

void PointItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragging) {
        QGraphicsEllipseItem::mouseMoveEvent(event);
    }
}

void PointItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);

        const QPointF newPos = pos();
        if (newPos != m_dragStartPos) {
            emit positionChanged(m_pointIndex, newPos);
        }
    }

    QGraphicsEllipseItem::mouseReleaseEvent(event);
}