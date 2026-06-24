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
#include <QtGlobal>

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

PointLayer::~PointLayer()
{
    removeBackup();
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

    const double width = m_style.size;
    const double height = m_style.size;
    const QPointF offset(-width / 2.0, -height / 2.0);

    const QVector<PointRecord> &points = m_collection.points();
    for (int i = 0; i < points.size(); ++i) {
        const PointRecord &rec = points[i];

        auto *pointItem = new PointItem(i);
        pointItem->setMovable(m_editMode);  // only movable in edit mode

        // Always connect clicked
        connect(pointItem, &PointItem::clicked,
                this, &PointLayer::onPointClicked);

        // Only connect edit signals in edit mode
        if (m_editMode) {
            connect(pointItem, &PointItem::positionChanged,
                    this, &PointLayer::onPointMoved, Qt::QueuedConnection);
            connect(pointItem, &PointItem::deleteRequested,
                    this, &PointLayer::onPointDeleted, Qt::QueuedConnection);
        }

        pointItem->setRect(QRectF(offset.x(), offset.y(), width, height));
        pointItem->setPen(QPen(m_style.color));
        pointItem->setBrush(QBrush(m_style.color));
        pointItem->setPos(rec.col, rec.row);
        pointItem->setZValue(zOrder());
        pointItem->setVisible(isVisible());
        scene->addItem(pointItem);
        m_items.append(pointItem);
    }

    applyStyleToItems();
    qDebug() << "PointLayer: built" << m_items.size() << "points (editMode=" << m_editMode << ")";
}

void PointLayer::setStyle(const PointLayerStyle &style)
{
    if (m_style.color == style.color &&
        qFuzzyCompare(m_style.size, style.size) &&
        m_style.symbol == style.symbol &&
        m_style.labelField == style.labelField &&
        qFuzzyCompare(m_style.labelSize, style.labelSize))
        return;

    m_style = style;
    applyStyleToItems();
    emit styleChanged();
}

void PointLayer::setColor(const QColor &color)
{
    PointLayerStyle next = m_style;
    next.color = color;
    setStyle(next);
}

void PointLayer::setSize(double size)
{
    PointLayerStyle next = m_style;
    next.size = qMax(1.0, size);
    setStyle(next);
}

void PointLayer::setSymbol(PointSymbol symbol)
{
    PointLayerStyle next = m_style;
    next.symbol = symbol;
    setStyle(next);
}

void PointLayer::setLabelField(const QString &field)
{
    PointLayerStyle next = m_style;
    next.labelField = field;
    setStyle(next);
}

void PointLayer::setLabelSize(double size)
{
    PointLayerStyle next = m_style;
    next.labelSize = qBound(6.0, size, 48.0);
    setStyle(next);
}

void PointLayer::setActive(bool active)
{
    if (isActive() == active)
        return;

    Layer::setActive(active);
    applyStyleToItems();
}

void PointLayer::applyStyleToItems()
{
    for (int i = 0; i < m_items.size(); ++i)
        applyStyleToPointItem(i);
}

void PointLayer::applyStyleToPointItem(int index)
{
    if (index < 0 || index >= m_items.size())
        return;

    auto *pointItem = dynamic_cast<PointItem*>(m_items[index]);
    if (!pointItem)
        return;

    const bool selected = isActive() && m_selectedPointIndices.contains(index);
    const double size = selected ? m_style.size * 1.35 : m_style.size;
    const QPointF offset(-size / 2.0, -size / 2.0);
    const QColor color = selected ? selectionColor() : m_style.color;

    QPen pen(color);

    pointItem->setSymbol(m_style.symbol);
    pointItem->setRect(QRectF(offset.x(), offset.y(), size, size));
    pointItem->setPen(pen);
    pointItem->setBrush(m_style.symbol == PointSymbol::Circle ? QBrush(color) : QBrush(Qt::NoBrush));
    pointItem->setLabel(labelTextForPoint(index), m_style.labelSize);
}

QString PointLayer::labelTextForPoint(int index) const
{
    if (m_style.labelField.isEmpty()
        || index < 0
        || index >= m_collection.points().size()) {
        return {};
    }

    if (m_style.labelField == QStringLiteral("__id__"))
        return QString::number(index);

    const QStringList columns = m_collection.columnNames();
    const int csvColumn = columns.indexOf(m_style.labelField);
    const int attributeIndex = csvColumn - 2;
    if (attributeIndex < 0)
        return {};

    const QVector<QVariant> &attributes =
        m_collection.points().at(index).attributes;
    if (attributeIndex >= attributes.size()
        || !attributes.at(attributeIndex).isValid()) {
        return {};
    }

    return attributes.at(attributeIndex).toString();
}

QColor PointLayer::selectionColor() const
{
    const QColor base = m_style.color.toHsv();
    if (!base.isValid())
        return m_editMode ? QColor(0, 120, 255) : QColor(Qt::cyan);

    const int hue = base.hsvHue();
    if (hue < 0)
        return m_editMode ? QColor(0, 120, 255) : QColor(Qt::cyan);

    const int complementHue = (hue + 180 + (m_editMode ? 30 : 0)) % 360;
    QColor color;
    color.setHsv(complementHue,
                 qMax(160, base.hsvSaturation()),
                 qMax(210, base.value()));

    return color;
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
    QSet<int> validSelection;
    for (int index : std::as_const(m_selectedPointIndices)) {
        if (index >= 0 && index < m_collection.recordCount())
            validSelection.insert(index);
    }
    m_selectedPointIndices = validSelection;
    if (!m_selectedPointIndices.contains(m_primarySelectedPointIndex)) {
        m_primarySelectedPointIndex = m_selectedPointIndices.isEmpty()
            ? -1 : *m_selectedPointIndices.constBegin();
    }

    // Rebuild items
    if (m_scene) {
        m_itemsBuilt = false;
        rebuildItems(m_scene);
        m_itemsBuilt = true;
    }
    emit collectionChanged();
    emit selectionChanged();
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
    if (m_primarySelectedPointIndex < 0
        || m_primarySelectedPointIndex >= m_collection.points().size())
        return nullptr;
    return &m_collection.points()[m_primarySelectedPointIndex];
}

void PointLayer::selectPoint(int index)
{
    if (index < 0) {
        clearSelection();
        return;
    }
    setSelectedPointIndices(QSet<int>{index}, index);
}

void PointLayer::togglePointSelection(int index)
{
    if (index < 0 || index >= m_collection.recordCount())
        return;

    QSet<int> selection = m_selectedPointIndices;
    int primary = m_primarySelectedPointIndex;
    if (selection.contains(index)) {
        selection.remove(index);
        if (primary == index)
            primary = selection.isEmpty() ? -1 : *selection.constBegin();
    } else {
        selection.insert(index);
        primary = index;
    }
    setSelectedPointIndices(selection, primary);
}

void PointLayer::setSelectedPointIndices(const QSet<int> &indices,
                                         int primaryIndex)
{
    QSet<int> valid;
    for (int index : indices) {
        if (index >= 0 && index < m_collection.recordCount())
            valid.insert(index);
    }
    if (!valid.contains(primaryIndex))
        primaryIndex = valid.isEmpty() ? -1 : *valid.constBegin();

    if (valid == m_selectedPointIndices
        && primaryIndex == m_primarySelectedPointIndex)
        return;

    const QSet<int> changed = valid | m_selectedPointIndices;
    m_selectedPointIndices = valid;
    m_primarySelectedPointIndex = primaryIndex;
    for (int index : changed)
        applyStyleToPointItem(index);
    emit selectionChanged();
}

void PointLayer::clearSelection()
{
    setSelectedPointIndices({}, -1);
}

// --- Slots ---

void PointLayer::onPointClicked(int index, Qt::KeyboardModifiers modifiers)
{
    if (!isActive())
        return;

    if (modifiers.testFlag(Qt::ControlModifier)) {
        togglePointSelection(index);
    } else if (m_editMode && m_selectedPointIndices.contains(index)) {
        setSelectedPointIndices(m_selectedPointIndices, index);
    } else {
        selectPoint(index);
    }
}

void PointLayer::addPoint(double row, double col)
{
    // Backup the collection and selection for potential revert
    const PointCollection backupCollection = m_collection;
    const QSet<int> backupSelection = m_selectedPointIndices;
    const int backupPrimary = m_primarySelectedPointIndex;

    m_collection.addPoint(row, col);

    QString errorMsg;
    if (!saveCsv(&errorMsg)) {
        m_collection = backupCollection;
        m_selectedPointIndices = backupSelection;
        m_primarySelectedPointIndex = backupPrimary;
        QMessageBox::warning(nullptr, "Edit Points",
                             QStringLiteral("Failed to save point: %1").arg(errorMsg));
        return;
    }

    const int newIndex = m_collection.points().size() - 1;
    m_selectedPointIndices = QSet<int>{newIndex};
    m_primarySelectedPointIndex = newIndex;

    // Rebuild items
    if (m_scene) {
        m_itemsBuilt = false;
        rebuildItems(m_scene);
        m_itemsBuilt = true;
    }
    emit collectionChanged();
    emit selectionChanged();
}

void PointLayer::onPointMoved(int index, QPointF pos)
{
    if (index < 0 || index >= m_collection.points().size())
        return;

    const PointCollection backupCollection = m_collection;
    const QSet<int> backupSelection = m_selectedPointIndices;
    const int backupPrimary = m_primarySelectedPointIndex;

    m_collection.movePoint(index, pos.y(), pos.x());  // row=y, col=x

    QString errorMsg;
    if (!saveCsv(&errorMsg)) {
        m_collection = backupCollection;
        m_selectedPointIndices = backupSelection;
        m_primarySelectedPointIndex = backupPrimary;
        QMessageBox::warning(nullptr, "Edit Points",
                             QStringLiteral("Failed to save moved point: %1").arg(errorMsg));
    }

    // Rebuild items (indices may be fine, but position in collection changed)
    if (m_scene) {
        m_itemsBuilt = false;
        rebuildItems(m_scene);
        m_itemsBuilt = true;
    }
    emit collectionChanged();
}

void PointLayer::onPointDeleted(int index)
{
    if (index < 0 || index >= m_collection.points().size())
        return;

    // Backup the full state before modifying
    const PointCollection backupCollection = m_collection;
    const QSet<int> backupSelection = m_selectedPointIndices;
    const int backupPrimary = m_primarySelectedPointIndex;

    // Adjust selection index BEFORE removing
    QSet<int> adjustedSelection;
    for (int selectedIndex : std::as_const(m_selectedPointIndices)) {
        if (selectedIndex == index)
            continue;
        adjustedSelection.insert(
            selectedIndex > index ? selectedIndex - 1 : selectedIndex);
    }
    m_selectedPointIndices = adjustedSelection;
    if (m_primarySelectedPointIndex == index) {
        m_primarySelectedPointIndex = adjustedSelection.isEmpty()
            ? -1 : *adjustedSelection.constBegin();
    } else if (m_primarySelectedPointIndex > index) {
        --m_primarySelectedPointIndex;
    }

    m_collection.removePoint(index);

    QString errorMsg;
    if (!saveCsv(&errorMsg)) {
        // Rollback: restore both collection and selection
        m_collection = backupCollection;
        m_selectedPointIndices = backupSelection;
        m_primarySelectedPointIndex = backupPrimary;
        QMessageBox::warning(nullptr, "Edit Points",
                             QStringLiteral("Failed to save deletion: %1").arg(errorMsg));
        emit selectionChanged();
        return;
    }

    // Rebuild items (indices changed after removal)
    if (m_scene) {
        m_itemsBuilt = false;
        rebuildItems(m_scene);
        m_itemsBuilt = true;
    }

    emit collectionChanged();
    emit selectionChanged();
}
