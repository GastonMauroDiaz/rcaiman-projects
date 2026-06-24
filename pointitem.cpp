#include "pointitem.h"
#include "cursorutils.h"

#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainterPath>
#include <QPainter>

namespace {
QCursor pointInteractionCursor()
{
    return highDpiCursor(
        QStringLiteral(":/icons/select-point.png"),
        QPoint(16, 2),
        Qt::PointingHandCursor);
}
}

class PointLabelItem : public QGraphicsItem
{
public:
    explicit PointLabelItem(QGraphicsItem *parent = nullptr)
        : QGraphicsItem(parent)
    {
        setAcceptedMouseButtons(Qt::NoButton);
        setAcceptHoverEvents(false);
    }

    void setText(const QString &text, double pointSize)
    {
        prepareGeometryChange();
        m_text = text;
        m_font.setPointSizeF(pointSize);

        QPainterPath path;
        path.addText(0.0, 0.0, m_font, m_text);
        m_textPath = path;
        m_bounds = m_textPath.boundingRect().adjusted(-2.0, -2.0, 2.0, 2.0);
        update();
    }

    QRectF boundingRect() const override
    {
        return m_bounds;
    }

    QPainterPath shape() const override
    {
        return m_textPath;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        painter->setRenderHint(QPainter::Antialiasing, true);

        QPen haloPen(Qt::white);
        haloPen.setWidthF(3.0);
        haloPen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(haloPen);
        painter->setBrush(Qt::white);
        painter->drawPath(m_textPath);

        painter->setPen(Qt::NoPen);
        painter->setBrush(Qt::black);
        painter->drawPath(m_textPath);
    }

private:
    QString m_text;
    QFont m_font;
    QPainterPath m_textPath;
    QRectF m_bounds;
};

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
    m_labelItem = new PointLabelItem(this);
    m_labelItem->setZValue(1.0);
    m_labelItem->hide();
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

void PointItem::setLabel(const QString &text, double pointSize)
{
    if (!m_labelItem)
        return;

    if (text.isEmpty()) {
        m_labelItem->hide();
        return;
    }

    m_labelItem->setText(text, pointSize);
    const QRectF labelBounds = m_labelItem->boundingRect();
    m_labelItem->setPos(rect().right() + 3.0,
                        rect().top() - labelBounds.bottom() - 2.0);
    m_labelItem->show();
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
        emit clicked(m_pointIndex, event->modifiers());

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
