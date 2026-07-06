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
