#include "layer.h"

#include <QGraphicsScene>

Layer::Layer(const QString &name, const QString &sourcePath, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_sourcePath(sourcePath)
    , m_flickerTimer(new QTimer(this))
{
    connect(m_flickerTimer, &QTimer::timeout, this, &Layer::onFlickerTick);
}

Layer::~Layer()
{
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

bool Layer::isActive() const
{
    return m_active;
}

void Layer::setActive(bool active)
{
    m_active = active;
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
    return m_zOrder;
}

void Layer::setZOrder(int zOrder)
{
    m_zOrder = zOrder;
    for (QGraphicsItem *item : std::as_const(m_items))
        item->setZValue(m_zOrder);
}

bool Layer::supportsReloadFromDisk() const
{
    return false;
}

bool Layer::reloadFromDisk(QString *errorMessage)
{
    if (errorMessage)
        *errorMessage = QStringLiteral("This layer does not support reload from disk.");
    return false;
}

void Layer::addToScene(QGraphicsScene *scene)
{
    if (!scene)
        return;
    for (QGraphicsItem *item : std::as_const(m_items)) {
        if (item->scene() != scene) {
            scene->addItem(item);
        }
        item->setZValue(m_zOrder);
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
    m_items.clear();
    m_itemsBuilt = false;
}

// --- Flicker ---

void Layer::setFlickerEnabled(bool enabled)
{
    if (m_flickerEnabled == enabled)
        return;

    m_flickerEnabled = enabled;

    if (enabled) {
        // Save current visibility before starting flicker
        m_visibleBeforeFlicker = m_visible;
        const int intervalMs = qMax(50, static_cast<int>(m_flickerIntervalSec * 1000.0));
        m_flickerTimer->start(intervalMs);
    } else {
        m_flickerTimer->stop();
        // Restore visibility to what it was before flicker started
        setVisible(m_visibleBeforeFlicker);
    }

    emit flickerChanged(m_flickerEnabled);
}

bool Layer::flickerEnabled() const
{
    return m_flickerEnabled;
}

void Layer::setFlickerInterval(double seconds)
{
    m_flickerIntervalSec = qBound(0.1, seconds, 10.0);
    if (m_flickerEnabled) {
        const int intervalMs = qMax(50, static_cast<int>(m_flickerIntervalSec * 1000.0));
        m_flickerTimer->setInterval(intervalMs);
    }
}

void Layer::onFlickerTick()
{
    // Toggle visibility while preserving m_visibleBeforeFlicker
    setVisible(!m_visible);
}
