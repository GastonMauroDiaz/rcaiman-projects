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

#ifndef POINTLAYER_H
#define POINTLAYER_H

#include "layer.h"
#include "layerstyle.h"
#include "pointcollection.h"

#include <QSet>

class PointItem;
class QGraphicsScene;

class PointLayer : public Layer
{
    Q_OBJECT

public:
    explicit PointLayer(const QString &path, QGraphicsScene *scene, QObject *parent = nullptr);
    ~PointLayer() override;

    const QVector<QGraphicsItem*> &buildItems(QGraphicsScene *scene) override;
    bool supportsReloadFromDisk() const override;
    bool reloadFromDisk(QString *errorMessage = nullptr) override;

    const PointCollection &collection() const { return m_collection; }
    PointLayerStyle style() const { return m_style; }
    void setStyle(const PointLayerStyle &style);
    void setColor(const QColor &color);
    void setSize(double size);
    void setSymbol(PointSymbol symbol);
    void setLabelField(const QString &field);
    void setLabelSize(double size);

    void setEditMode(bool enabled);
    bool editMode() const { return m_editMode; }
    void setActive(bool active) override;

    void cancelEdit();

    bool saveCsv(QString *errorMessage = nullptr);

    void addPoint(double row, double col);

    // Selection
    int selectedPointIndex() const { return m_primarySelectedPointIndex; }
    int primarySelectedPointIndex() const { return m_primarySelectedPointIndex; }
    const QSet<int> &selectedPointIndices() const { return m_selectedPointIndices; }
    const PointRecord *selectedPoint() const;
    void selectPoint(int index);
    void togglePointSelection(int index);
    void setSelectedPointIndices(const QSet<int> &indices, int primaryIndex = -1);
    void clearSelection();
    void invertSelection();
    void deleteSelectedPoints();

signals:
    void selectionChanged();
    void collectionChanged();

private slots:
    void onPointClicked(int index, Qt::KeyboardModifiers modifiers);
    void onPointMoved(int index, QPointF pos);
    void onPointDeleted(int index);

private:
    void rebuildItems(QGraphicsScene *scene);
    void applyStyleToItems();
    void applyStyleToPointItem(int index);
    QString labelTextForPoint(int index) const;
    QColor selectionColor() const;
    void clearPointItems();
    bool ensureBackup();
    void removeBackup();

    PointCollection m_collection;
    PointLayerStyle m_style;
    QGraphicsScene *m_scene = nullptr;
    bool m_editMode = false;
    QString m_backupPath;
    QSet<int> m_selectedPointIndices;
    int m_primarySelectedPointIndex = -1;
};

#endif // POINTLAYER_H
