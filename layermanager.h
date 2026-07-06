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

#ifndef LAYERMANAGER_H
#define LAYERMANAGER_H

#include <QObject>
#include <QVector>
#include <memory>

class Layer;
class QGraphicsScene;

class LayerManager : public QObject
{
    Q_OBJECT

public:
    explicit LayerManager(QObject *parent = nullptr);
    ~LayerManager() override;

    int layerCount() const;
    Layer *layerAt(int index) const;
    int indexOf(Layer *layer) const;

    Layer *activeLayer() const;
    void setActiveLayer(Layer *layer);

    void addLayer(Layer *layer);   // toma ownership
    void removeLayer(int index);   // elimina y borra
    void clearAll();                // remueve todas las capas
    void moveUp(int index);
    void moveDown(int index);
    void moveLayer(int from, int to);
    void setVisibility(int index, bool visible);

    // Call after scene is ready or changes
    void attachToScene(QGraphicsScene *scene);
    
    QGraphicsScene *scene() const { return m_scene; }

signals:
    void layersChanged();    // lista o visibilidad cambió
    void activeLayerChanged(Layer *layer);

private:
    void rebuildZOrder();

    QGraphicsScene *m_scene = nullptr;
    QVector<Layer*> m_layers;  // propietario
    Layer *m_activeLayer = nullptr;
};

#endif // LAYERMANAGER_H
