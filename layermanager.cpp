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

#include "layermanager.h"
#include "layer.h"

#include <QGraphicsScene>

LayerManager::LayerManager(QObject *parent)
    : QObject(parent)
{
}

LayerManager::~LayerManager()
{
    qDeleteAll(m_layers);
    m_layers.clear();
}

int LayerManager::layerCount() const
{
    return m_layers.size();
}

Layer *LayerManager::layerAt(int index) const
{
    if (index < 0 || index >= m_layers.size())
        return nullptr;
    return m_layers.at(index);
}

int LayerManager::indexOf(Layer *layer) const
{
    return m_layers.indexOf(layer);
}

Layer *LayerManager::activeLayer() const
{
    return m_activeLayer;
}

void LayerManager::setActiveLayer(Layer *layer)
{
    if (m_activeLayer == layer)
        return;
    if (m_activeLayer)
        m_activeLayer->setActive(false);
    m_activeLayer = layer;
    if (m_activeLayer)
        m_activeLayer->setActive(true);
    emit activeLayerChanged(m_activeLayer);
}

void LayerManager::addLayer(Layer *layer)
{
    if (!layer)
        return;

    m_layers.insert(0, layer);
    layer->setParent(this);

    if (m_scene) {
        layer->buildItems(m_scene);
        rebuildZOrder();
        layer->addToScene(m_scene);
    }

    if (m_activeLayer)
        m_activeLayer->setActive(false);
    m_activeLayer = layer;
    m_activeLayer->setActive(true);
    emit layersChanged();
    emit activeLayerChanged(m_activeLayer);
}

void LayerManager::removeLayer(int index)
{
    if (index < 0 || index >= m_layers.size())
        return;

    Layer *layer = m_layers.takeAt(index);
    const bool activeChanged = (m_activeLayer == layer);

    if (activeChanged) {
        m_activeLayer->setActive(false);
        m_activeLayer = nullptr;
        if (!m_layers.isEmpty()) {
            const int nextIndex = qMin(index, m_layers.size() - 1);
            m_activeLayer = m_layers.at(nextIndex);
            m_activeLayer->setActive(true);
        }
    }

    if (m_scene)
        layer->removeFromScene(m_scene);

    delete layer;
    rebuildZOrder();
    emit layersChanged();
    if (activeChanged)
        emit activeLayerChanged(m_activeLayer);
}

void LayerManager::clearAll()
{
    const bool hadActiveLayer = (m_activeLayer != nullptr);

    if (m_scene) {
        for (Layer *layer : std::as_const(m_layers)) {
            layer->removeFromScene(m_scene);
        }
        m_scene->clear();  // destroys all QGraphicsItems
    }

    if (m_activeLayer)
        m_activeLayer->setActive(false);
    m_activeLayer = nullptr;
    qDeleteAll(m_layers);
    m_layers.clear();
    emit layersChanged();
    if (hadActiveLayer)
        emit activeLayerChanged(nullptr);
}

void LayerManager::moveUp(int index)
{
    if (index <= 0 || index >= m_layers.size())
        return;

    m_layers.swapItemsAt(index, index - 1);
    rebuildZOrder();
    emit layersChanged();
}

void LayerManager::moveDown(int index)
{
    if (index < 0 || index >= m_layers.size() - 1)
        return;

    m_layers.swapItemsAt(index, index + 1);
    rebuildZOrder();
    emit layersChanged();
}

void LayerManager::moveLayer(int from, int to)
{
    if (from < 0 || from >= m_layers.size()
        || to < 0 || to >= m_layers.size() || from == to) {
        return;
    }

    m_layers.move(from, to);
    rebuildZOrder();
    emit layersChanged();
}

void LayerManager::setVisibility(int index, bool visible)
{
    Layer *layer = layerAt(index);
    if (!layer || layer->isVisible() == visible)
        return;

    layer->setVisible(visible);
    emit layersChanged();
}

void LayerManager::attachToScene(QGraphicsScene *scene)
{
    m_scene = scene;
    for (Layer *layer : m_layers) {
        layer->buildItems(m_scene);
        rebuildZOrder();
        layer->addToScene(m_scene);
    }
    emit layersChanged();
}

void LayerManager::rebuildZOrder()
{
    if (!m_scene)
        return;

    const int count = m_layers.size();
    for (int i = 0; i < count; ++i) {
        Layer *layer = m_layers[i];
        layer->setZOrder(count - 1 - i);
    }
}
