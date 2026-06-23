#include "pointitem.h"

#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPixmap>

namespace {
QCursor pointInteractionCursor()
{
    QPixmap pixmap(QStringLiteral(":/icons/select-point.png"));
    if (pixmap.isNull())
        return Qt::PointingHandCursor;

    pixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return QCursor(pixmap, pixmap.width() / 2, 2);
}
}

PointItem::PointItem(int index, QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsEllipseItem(parent)
    , m_pointIndex(index)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable, false);  // off by default; setMovable enables it
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, false);
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    setCursor(pointInteractionCursor());
}

void PointItem::setPointIndex(int index)
{
    m_pointIndex = index;
}

void PointItem::setMovable(bool movable)
{
    setFlag(QGraphicsItem::ItemIsMovable, movable);
}

void PointItem::setSymbol(PointSymbol symbol)
{
    if (m_symbol == symbol)
        return;

    m_symbol = symbol;
    update();
}

void PointItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (m_symbol == PointSymbol::Circle) {
        QGraphicsEllipseItem::paint(painter, option, widget);
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing, true);
    QPen crossPen = pen();
    crossPen.setCapStyle(Qt::RoundCap);
    crossPen.setWidthF(qMax(0.25, qMin(rect().width(), rect().height()) * 0.18));
    painter->setPen(crossPen);
    painter->setBrush(Qt::NoBrush);

    const QRectF r = rect();
    const QPointF center = r.center();
    const double half = qMin(r.width(), r.height()) / 2.0;
    const double arm = half * 0.82;

    painter->drawLine(QPointF(center.x() - arm, center.y()),
                      QPointF(center.x() + arm, center.y()));
    painter->drawLine(QPointF(center.x(), center.y() - arm),
                      QPointF(center.x(), center.y() + arm));
}

void PointItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(pointInteractionCursor());
    QGraphicsEllipseItem::hoverEnterEvent(event);
}

void PointItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    unsetCursor();
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
            setCursor(pointInteractionCursor());
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
        setCursor(pointInteractionCursor());

        const QPointF newPos = pos();
        if (newPos != m_dragStartPos) {
            emit positionChanged(m_pointIndex, newPos);
        }
    }

    QGraphicsEllipseItem::mouseReleaseEvent(event);
}
