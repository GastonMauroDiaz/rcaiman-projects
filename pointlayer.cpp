#include "pointlayer.h"
#include "pointitem.h"

#include <QDir>
#include <QFileInfo>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QMessageBox>
#include <QPen>
#include <QBrush>
#include <QColor>

PointLayer::PointLayer(const QString &path, QGraphicsScene *scene, QObject *parent)
    : Layer(QFileInfo(path).fileName(), path, parent)
    , m_scene(scene)
{
    QFileInfo fi(path);
    if (fi.isFile() && fi.suffix().toLower() == "csv") {
        m_collection = PointCollection::load(path);
    } else {
        m_collection = PointCollection::loadFromDirectory(path);
    }
}

const QVector<QGraphicsItem*> &PointLayer::buildItems(QGraphicsScene *scene)
{
    if (m_itemsBuilt)
        return m_items;

    rebuildItems(scene);
    m_itemsBuilt = true;
    return m_items;
}

void PointLayer::rebuildItems(QGraphicsScene *scene)
{
    clearPointItems();

    const double width = 6.0;
    const double height = 6.0;
    const QPointF offset(-width / 2.0, -height / 2.0);

    const QVector<PointRecord> &points = m_collection.points();
    for (int i = 0; i < points.size(); ++i) {
        const PointRecord &rec = points[i];

        auto *pointItem = new PointItem(i);
        pointItem->setMovable(m_editMode);  // only movable in edit mode

        // Always connect clicked
        connect(pointItem, &PointItem::clicked,
                this, &PointLayer::onPointClicked, Qt::QueuedConnection);

        // Only connect edit signals in edit mode
        if (m_editMode) {
            connect(pointItem, &PointItem::positionChanged,
                    this, &PointLayer::onPointMoved, Qt::QueuedConnection);
            connect(pointItem, &PointItem::deleteRequested,
                    this, &PointLayer::onPointDeleted, Qt::QueuedConnection);
        }

        pointItem->setRect(QRectF(offset.x(), offset.y(), width, height));
        pointItem->setPen(QPen(Qt::yellow));
        pointItem->setBrush(QBrush(Qt::yellow));
        pointItem->setPos(rec.col, rec.row);
        pointItem->setVisible(isVisible());
        scene->addItem(pointItem);
        m_items.append(pointItem);
    }

    qDebug() << "PointLayer: built" << m_items.size() << "points (editMode=" << m_editMode << ")";
}

void PointLayer::clearPointItems()
{
    for (QGraphicsItem *item : std::as_const(m_items)) {
        if (item->scene())
            item->scene()->removeItem(item);
        delete item;
    }
    m_items.clear();
}

void PointLayer::setEditMode(bool enabled)
{
    if (m_editMode == enabled)
        return;

    m_editMode = enabled;

    if (enabled) {
        if (!ensureBackup()) {
            m_editMode = false;
            return;
        }
    } else {
        removeBackup();
        clearSelection();
    }

    // Rebuild items with new mode (rebuildItems calls clearPointItems internally)
    if (m_scene) {
        m_itemsBuilt = false;
        rebuildItems(m_scene);
        m_itemsBuilt = true;
    }
}

bool PointLayer::ensureBackup()
{
    const QString src = sourcePath();
    QFileInfo fi(src);
    if (!fi.isFile() || fi.suffix().toLower() != "csv")
        return false;

    m_backupPath = src + ".bak";

    // Remove existing backup (overwrite)
    if (QFile::exists(m_backupPath))
        QFile::remove(m_backupPath);

    if (!QFile::copy(src, m_backupPath)) {
        QMessageBox::warning(nullptr, "Edit Points",
                             QStringLiteral("Could not create backup file: %1").arg(m_backupPath));
        m_backupPath.clear();
        return false;
    }

    return true;
}

void PointLayer::removeBackup()
{
    if (!m_backupPath.isEmpty() && QFile::exists(m_backupPath))
        QFile::remove(m_backupPath);
    m_backupPath.clear();
}

void PointLayer::cancelEdit()
{
    if (m_backupPath.isEmpty() || !QFile::exists(m_backupPath))
        return;

    // Restore collection from backup
    m_collection = PointCollection::load(m_backupPath);

    // Overwrite original CSV with backup
    const QString src = sourcePath();
    QFile::remove(src);
    QFile::rename(m_backupPath, src);

    // Clean backup and deactivate edit mode
    m_backupPath.clear();
    m_editMode = false;
    clearSelection();

    // Rebuild items
    if (m_scene) {
        m_itemsBuilt = false;
        rebuildItems(m_scene);
        m_itemsBuilt = true;
    }
}

bool PointLayer::saveCsv(QString *errorMessage)
{
    const QString src = sourcePath();
    QFileInfo fi(src);
    if (!fi.isFile() || fi.suffix().toLower() != "csv") {
        if (errorMessage)
            *errorMessage = QStringLiteral("Source path is not a CSV file: %1").arg(src);
        return false;
    }

    return m_collection.save(src, errorMessage);
}

// --- Selection ---

const PointRecord *PointLayer::selectedPoint() const
{
    if (m_selectedPointIndex < 0 || m_selectedPointIndex >= m_collection.points().size())
        return nullptr;
    return &m_collection.points()[m_selectedPointIndex];
}

void PointLayer::selectPoint(int index)
{
    if (index < -1 || index >= m_collection.points().size())
        return;

    m_selectedPointIndex = index;
    emit selectedPointChanged(m_selectedPointIndex);
}

void PointLayer::clearSelection()
{
    if (m_selectedPointIndex >= 0) {
        m_selectedPointIndex = -1;
        emit selectedPointChanged(-1);
    }
}

// --- Slots ---

void PointLayer::onPointClicked(int index)
{
    selectPoint(index);
}

void PointLayer::addPoint(double row, double col)
{
    // Backup the collection and selection for potential revert
    const PointCollection backupCollection = m_collection;
    const int backupSelection = m_selectedPointIndex;

    m_collection.addPoint(row, col);

    QString errorMsg;
    if (!saveCsv(&errorMsg)) {
        m_collection = backupCollection;
        m_selectedPointIndex = backupSelection;
        QMessageBox::warning(nullptr, "Edit Points",
                             QStringLiteral("Failed to save point: %1").arg(errorMsg));
        return;
    }

    // Rebuild items
    if (m_scene) {
        m_itemsBuilt = false;
        rebuildItems(m_scene);
        m_itemsBuilt = true;
    }
}

void PointLayer::onPointMoved(int index, QPointF pos)
{
    if (index < 0 || index >= m_collection.points().size())
        return;

    const PointCollection backupCollection = m_collection;
    const int backupSelection = m_selectedPointIndex;

    m_collection.movePoint(index, pos.y(), pos.x());  // row=y, col=x

    QString errorMsg;
    if (!saveCsv(&errorMsg)) {
        m_collection = backupCollection;
        m_selectedPointIndex = backupSelection;
        QMessageBox::warning(nullptr, "Edit Points",
                             QStringLiteral("Failed to save moved point: %1").arg(errorMsg));
    }

    // Rebuild items (indices may be fine, but position in collection changed)
    if (m_scene) {
        m_itemsBuilt = false;
        rebuildItems(m_scene);
        m_itemsBuilt = true;
    }
}

void PointLayer::onPointDeleted(int index)
{
    if (index < 0 || index >= m_collection.points().size())
        return;

    // Backup the full state before modifying
    const PointCollection backupCollection = m_collection;
    const int backupSelection = m_selectedPointIndex;

    // Adjust selection index BEFORE removing
    if (m_selectedPointIndex >= 0) {
        if (index == m_selectedPointIndex) {
            // The selected point was deleted → deselect
            m_selectedPointIndex = -1;
        } else if (index < m_selectedPointIndex) {
            // A point before the selected one was removed → shift left
            m_selectedPointIndex--;
        }
        // if index > m_selectedPointIndex: no adjustment needed
    }

    m_collection.removePoint(index);

    QString errorMsg;
    if (!saveCsv(&errorMsg)) {
        // Rollback: restore both collection and selection
        m_collection = backupCollection;
        m_selectedPointIndex = backupSelection;
        QMessageBox::warning(nullptr, "Edit Points",
                             QStringLiteral("Failed to save deletion: %1").arg(errorMsg));
        emit selectedPointChanged(m_selectedPointIndex);
        return;
    }

    // Rebuild items (indices changed after removal)
    if (m_scene) {
        m_itemsBuilt = false;
        rebuildItems(m_scene);
        m_itemsBuilt = true;
    }

    // Notify with the adjusted index
    emit selectedPointChanged(m_selectedPointIndex);
}