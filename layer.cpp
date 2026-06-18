#include "layer.h"

#include <QGraphicsScene>

Layer::Layer(const QString &name, const QString &sourcePath, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_sourcePath(sourcePath)
{
}

Layer::~Layer()
{
    // m_items son propiedad de QGraphicsScene, no se borran aquí
    m_items.clear();
}

QString Layer::name() const
{
    return m_name;
}

void Layer::rename(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged(m_name);
    }
}

QString Layer::sourcePath() const
{
    return m_sourcePath;
}

bool Layer::isVisible() const
{
    return m_visible;
}

void Layer::setVisible(bool visible)
{
    m_visible = visible;
    for (QGraphicsItem *item : std::as_const(m_items)) {
        item->setVisible(m_visible);
    }
    emit visibilityChanged(m_visible);
}

int Layer::zOrder() const
{
    return -1; // calculado externamente por el manager
}

void Layer::addToScene(QGraphicsScene *scene)
{
    if (!scene)
        return;
    for (QGraphicsItem *item : std::as_const(m_items)) {
        if (item->scene() != scene) {
            scene->addItem(item);
        }
        item->setVisible(m_visible);
    }
}

void Layer::removeFromScene(QGraphicsScene *scene)
{
    if (!scene)
        return;
    for (QGraphicsItem *item : std::as_const(m_items)) {
        scene->removeItem(item);
    }
}

void Layer::clearCachedItems()
{
    // NO borramos los items porque QGraphicsScene es propietario.
    // Simplemente olvidamos las referencias.
    m_items.clear();
    m_itemsBuilt = false;
}