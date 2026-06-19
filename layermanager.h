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
    void setVisibility(int index, bool visible);

    // Call after scene is ready or changes
    void attachToScene(QGraphicsScene *scene);

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