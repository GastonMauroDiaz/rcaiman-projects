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

    if (m_activeLayer == layer) {
        m_activeLayer->setActive(false);
        m_activeLayer = nullptr;
    }

    if (m_scene)
        layer->removeFromScene(m_scene);

    delete layer;
    rebuildZOrder();
    emit layersChanged();
}

void LayerManager::clearAll()
{
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
