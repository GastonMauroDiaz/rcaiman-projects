// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// This file is part of rcaiman-projects.
//
// rcaiman-projects is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// rcaiman-projects is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with rcaiman-projects. If not, see <https://www.gnu.org/licenses/>.
//
// Part of the rcaiman suite.

#include "pointitem.h"

#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainterPath>
#include <QPainter>

namespace {
QCursor pointInteractionCursor()
{
    return QCursor(Qt::PointingHandCursor);
}

QCursor pointDragCursor()
{
    return QCursor(Qt::ClosedHandCursor);
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

void PointItem::setSelectionHalo(bool enabled, const QColor &color)
{
    if (m_selectionHaloEnabled == enabled
        && m_selectionHaloColor == color) {
        return;
    }

    m_selectionHaloEnabled = enabled;
    m_selectionHaloColor = color;
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
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = rect();
    const double symbolSize = qMin(r.width(), r.height());

    if (m_symbol == PointSymbol::Circle) {
        if (m_selectionHaloEnabled && m_selectionHaloColor.isValid()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QBrush(m_selectionHaloColor));
            painter->drawEllipse(r);

            const double inset = qMax(1.5, symbolSize * 0.16);
            painter->setBrush(brush());
            painter->drawEllipse(r.adjusted(inset, inset, -inset, -inset));
            return;
        }

        painter->setPen(pen());
        painter->setBrush(brush());
        painter->drawEllipse(r);
        return;
    }

    QPen crossPen = pen();
    crossPen.setCapStyle(Qt::RoundCap);
    crossPen.setWidthF(qMax(0.25, symbolSize * 0.18));
    const QPointF center = r.center();
    const double half = symbolSize / 2.0;
    const double arm = half * 0.82;

    if (m_selectionHaloEnabled && m_selectionHaloColor.isValid()) {
        QPen haloPen(m_selectionHaloColor);
        haloPen.setCapStyle(Qt::RoundCap);
        haloPen.setWidthF(qMax(crossPen.widthF() + 2.0, symbolSize * 0.34));
        painter->setPen(haloPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(center.x() - arm, center.y()),
                          QPointF(center.x() + arm, center.y()));
        painter->drawLine(QPointF(center.x(), center.y() - arm),
                          QPointF(center.x(), center.y() + arm));
    }

    painter->setPen(crossPen);
    painter->setBrush(Qt::NoBrush);
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
            setCursor(pointDragCursor());
        }
    }

    QGraphicsEllipseItem::mousePressEvent(event);
}

void PointItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragging) {
        setCursor(pointDragCursor());
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
