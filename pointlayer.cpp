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

#include "pointlayer.h"
#include "apppreferences.h"
#include "pointitem.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsEllipseItem>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QProgressDialog>
#include <QStyleOptionGraphicsItem>
#include <QBrush>
#include <QColor>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>

namespace {
constexpr int PointBuildProgressThreshold = 20000;
constexpr int PointBuildProgressStep = 1000;
constexpr int PointAggregateDrawBudget = 50000;
constexpr double PointAggregateBoundsMargin = 1024.0;

QRectF boundsForPoints(const QVector<PointRecord> &points)
{
    if (points.isEmpty())
        return QRectF(0.0, 0.0, 1.0, 1.0);

    double minCol = std::numeric_limits<double>::max();
    double minRow = std::numeric_limits<double>::max();
    double maxCol = std::numeric_limits<double>::lowest();
    double maxRow = std::numeric_limits<double>::lowest();

    for (const PointRecord &point : points) {
        if (!std::isfinite(point.col) || !std::isfinite(point.row))
            continue;
        minCol = qMin(minCol, point.col);
        minRow = qMin(minRow, point.row);
        maxCol = qMax(maxCol, point.col);
        maxRow = qMax(maxRow, point.row);
    }

    if (minCol > maxCol || minRow > maxRow)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    return QRectF(QPointF(minCol, minRow), QPointF(maxCol, maxRow))
        .normalized()
        .adjusted(-PointAggregateBoundsMargin,
                  -PointAggregateBoundsMargin,
                  PointAggregateBoundsMargin,
                  PointAggregateBoundsMargin);
}

class PointAggregateItem : public QGraphicsItem
{
public:
    explicit PointAggregateItem(PointLayer *layer)
        : m_layer(layer)
        , m_bounds(layer ? boundsForPoints(layer->collection().points())
                         : QRectF(0.0, 0.0, 1.0, 1.0))
    {
        setAcceptedMouseButtons(Qt::NoButton);
        setAcceptHoverEvents(false);
        setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    }

    QRectF boundingRect() const override
    {
        return m_bounds;
    }

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *) override
    {
        if (!m_layer || !painter)
            return;

        const QVector<PointRecord> &points =
            m_layer->collection().points();
        if (points.isEmpty())
            return;

        const PointLayerStyle style = m_layer->style();
        QColor color = style.color;
        color.setAlpha(qMax(80, color.alpha()));

        const double lod = qMax(
            1.0e-6,
            QStyleOptionGraphicsItem::levelOfDetailFromTransform(
                painter->worldTransform()));
        const double screenSize = qBound(1.0, style.size, 96.0);
        const double sceneRadius = screenSize / (2.0 * lod);
        QRectF exposed = option ? option->exposedRect : m_bounds;
        exposed.adjust(-sceneRadius, -sceneRadius,
                       sceneRadius, sceneRadius);

        const int total = points.size();
        const int step = qMax(
            1,
            static_cast<int>(
                std::ceil(static_cast<double>(total)
                          / PointAggregateDrawBudget)));

        painter->setRenderHint(QPainter::Antialiasing,
                               step == 1 && screenSize >= 5.0);

        if (screenSize <= 3.0) {
            QPen pointPen(color);
            pointPen.setCosmetic(true);
            painter->setPen(pointPen);
            for (int i = 0; i < total; i += step) {
                const PointRecord &point = points[i];
                const QPointF pos(point.col, point.row);
                if (exposed.contains(pos))
                    painter->drawPoint(pos);
            }
            return;
        }

        if (style.symbol == PointSymbol::Circle) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QBrush(color));
            const double diameter = sceneRadius * 2.0;
            for (int i = 0; i < total; i += step) {
                const PointRecord &point = points[i];
                const QPointF pos(point.col, point.row);
                if (!exposed.contains(pos))
                    continue;
                painter->drawEllipse(
                    QRectF(pos.x() - sceneRadius,
                           pos.y() - sceneRadius,
                           diameter,
                           diameter));
            }
            return;
        }

        QPen crossPen(color);
        crossPen.setCosmetic(true);
        crossPen.setWidthF(qMax(1.0, screenSize * 0.18));
        crossPen.setCapStyle(Qt::RoundCap);
        painter->setPen(crossPen);
        painter->setBrush(Qt::NoBrush);
        const double arm = sceneRadius * 0.82;
        for (int i = 0; i < total; i += step) {
            const PointRecord &point = points[i];
            const QPointF pos(point.col, point.row);
            if (!exposed.contains(pos))
                continue;
            painter->drawLine(QPointF(pos.x() - arm, pos.y()),
                              QPointF(pos.x() + arm, pos.y()));
            painter->drawLine(QPointF(pos.x(), pos.y() - arm),
                              QPointF(pos.x(), pos.y() + arm));
        }
    }

private:
    PointLayer *m_layer = nullptr;
    QRectF m_bounds;
};

void deletePointItems(QVector<QGraphicsItem*> &items)
{
    for (QGraphicsItem *item : std::as_const(items)) {
        if (item->scene())
            item->scene()->removeItem(item);
        delete item;
    }
    items.clear();
}
}

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

    if (rebuildItems(scene, true))
        m_itemsBuilt = true;
    return m_items;
}

bool PointLayer::lastBuildCanceled() const
{
    return m_lastBuildCanceled;
}

bool PointLayer::usesAggregateRendering() const
{
    return m_aggregateRendering;
}

bool PointLayer::supportsReloadFromDisk() const
{
    const QFileInfo fi(sourcePath());
    return !m_editMode
        && fi.isFile()
        && fi.suffix().compare(QStringLiteral("csv"), Qt::CaseInsensitive) == 0;
}

bool PointLayer::reloadFromDisk(QString *errorMessage)
{
    if (m_editMode) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Point layer is currently being edited.");
        return false;
    }

    const QFileInfo fi(sourcePath());
    if (!fi.isFile()
        || fi.suffix().compare(QStringLiteral("csv"), Qt::CaseInsensitive) != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Source path is not a CSV file: %1")
                .arg(sourcePath());
        return false;
    }

    QString loadError;
    PointCollection reloaded = PointCollection::load(sourcePath(), &loadError);
    if (!reloaded.isValid()) {
        if (errorMessage)
            *errorMessage = loadError.isEmpty()
                ? QStringLiteral("Could not reload points file.")
                : loadError;
        return false;
    }

    const PointCollection previousCollection = m_collection;
    const QSet<int> previousSelection = m_selectedPointIndices;
    const int previousPrimarySelection = m_primarySelectedPointIndex;

    m_collection = reloaded;
    m_selectedPointIndices.clear();
    m_primarySelectedPointIndex = -1;

    if (m_scene) {
        m_itemsBuilt = false;
        if (!rebuildItems(m_scene, true)) {
            m_collection = previousCollection;
            m_selectedPointIndices = previousSelection;
            m_primarySelectedPointIndex = previousPrimarySelection;
            m_itemsBuilt = !m_items.isEmpty();
            if (errorMessage)
                *errorMessage = QStringLiteral("Point rendering was stopped.");
            return false;
        }
        m_itemsBuilt = true;
    }

    emit collectionChanged();
    emit selectionChanged();
    emit dataChanged();
    return true;
}

bool PointLayer::rebuildItems(QGraphicsScene *scene, bool allowCancel)
{
    m_lastBuildCanceled = false;

    if (shouldUseAggregateRendering()) {
        const int fastPreviewLimit = AppPreferences::pointFastPreviewLimit();
        QVector<QGraphicsItem*> newItems;
        auto *aggregateItem = new PointAggregateItem(this);
        aggregateItem->setZValue(zOrder());
        aggregateItem->setVisible(isVisible());
        if (scene)
            scene->addItem(aggregateItem);
        newItems.append(aggregateItem);

        clearPointItems();
        m_items = newItems;
        m_aggregateRendering = true;
        m_fastPreviewLimit = fastPreviewLimit;
        qDebug() << "PointLayer: using aggregate renderer for"
                 << m_collection.recordCount() << "points, limit"
                 << m_fastPreviewLimit;
        return true;
    }

    const double width = m_style.size;
    const double height = m_style.size;
    const QPointF offset(-width / 2.0, -height / 2.0);

    const QVector<PointRecord> &points = m_collection.points();
    QVector<QGraphicsItem*> newItems;
    newItems.reserve(points.size());

    std::unique_ptr<QProgressDialog> progress;
    if (allowCancel && points.size() >= PointBuildProgressThreshold) {
        progress = std::make_unique<QProgressDialog>(
            QStringLiteral("Rendering %1 point%2...")
                .arg(points.size())
                .arg(points.size() == 1 ? "" : "s"),
            QStringLiteral("Stop"),
            0,
            points.size(),
            QApplication::activeWindow());
        progress->setWindowTitle("Render Points");
        progress->setWindowModality(Qt::ApplicationModal);
        progress->setMinimumDuration(0);
        progress->setValue(0);
    }

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
        pointItem->setVisible(false);
        if (scene)
            scene->addItem(pointItem);
        newItems.append(pointItem);

        if (progress
            && ((i + 1) % PointBuildProgressStep == 0
                || i + 1 == points.size())) {
            progress->setValue(i + 1);
            QApplication::processEvents();
            if (progress->wasCanceled()) {
                m_lastBuildCanceled = true;
                deletePointItems(newItems);
                return false;
            }
        }
    }

    if (progress)
        progress->setValue(points.size());

    clearPointItems();
    m_items = newItems;
    m_aggregateRendering = false;
    m_fastPreviewLimit = AppPreferences::pointFastPreviewLimit();
    applyStyleToItems();
    for (QGraphicsItem *item : std::as_const(m_items)) {
        item->setZValue(zOrder());
        item->setVisible(isVisible());
    }
    qDebug() << "PointLayer: built" << m_items.size() << "points (editMode=" << m_editMode << ")";
    return true;
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
    for (int i = 0; i < m_items.size(); ++i) {
        if (dynamic_cast<PointItem*>(m_items[i]))
            applyStyleToPointItem(i);
        else
            m_items[i]->update();
    }
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
        return QString::number(index + 1);

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

bool PointLayer::shouldUseAggregateRendering() const
{
    return !m_editMode
        && m_collection.recordCount()
            >= AppPreferences::pointFastPreviewLimit();
}

void PointLayer::setEditMode(bool enabled)
{
    if (m_editMode == enabled)
        return;

    if (enabled && usesAggregateRendering()) {
        QMessageBox::information(
            nullptr,
            "Edit Points",
            QStringLiteral(
                "This point layer has %1 points and is displayed in a "
                "fast preview mode.\n\n"
                "Interactive point editing is disabled for dense point "
                "layers to keep pan and zoom responsive.\n\n"
                "Current fast preview limit: %2 points.")
                .arg(m_collection.recordCount())
                .arg(m_fastPreviewLimit > 0
                         ? m_fastPreviewLimit
                         : AppPreferences::pointFastPreviewLimit()));
        return;
    }

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

void PointLayer::invertSelection()
{
    QSet<int> inverted;
    const int count = m_collection.recordCount();
    for (int index = 0; index < count; ++index) {
        if (!m_selectedPointIndices.contains(index))
            inverted.insert(index);
    }
    setSelectedPointIndices(inverted, inverted.isEmpty() ? -1 : *inverted.constBegin());
}

void PointLayer::deleteSelectedPoints()
{
    if (m_selectedPointIndices.isEmpty())
        return;

    const int primary = m_primarySelectedPointIndex >= 0
        ? m_primarySelectedPointIndex
        : *m_selectedPointIndices.constBegin();
    onPointDeleted(primary);
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

    QVector<int> indicesToRemove;
    if (m_selectedPointIndices.contains(index)) {
        indicesToRemove.reserve(m_selectedPointIndices.size());
        for (int selectedIndex : std::as_const(m_selectedPointIndices)) {
            if (selectedIndex >= 0
                && selectedIndex < m_collection.recordCount())
                indicesToRemove.append(selectedIndex);
        }
    } else {
        indicesToRemove.append(index);
    }

    std::sort(indicesToRemove.begin(), indicesToRemove.end());
    indicesToRemove.erase(
        std::unique(indicesToRemove.begin(), indicesToRemove.end()),
        indicesToRemove.end());

    QSet<int> removedSet;
    for (int removeIndex : std::as_const(indicesToRemove))
        removedSet.insert(removeIndex);

    QSet<int> adjustedSelection;
    for (int selectedIndex : std::as_const(m_selectedPointIndices)) {
        if (removedSet.contains(selectedIndex))
            continue;

        int shift = 0;
        for (int removeIndex : std::as_const(indicesToRemove)) {
            if (removeIndex < selectedIndex)
                ++shift;
        }
        adjustedSelection.insert(selectedIndex - shift);
    }
    m_selectedPointIndices = adjustedSelection;
    if (removedSet.contains(m_primarySelectedPointIndex)) {
        m_primarySelectedPointIndex = adjustedSelection.isEmpty()
            ? -1 : *adjustedSelection.constBegin();
    } else {
        int shift = 0;
        for (int removeIndex : std::as_const(indicesToRemove)) {
            if (removeIndex < m_primarySelectedPointIndex)
                ++shift;
        }
        m_primarySelectedPointIndex =
            m_primarySelectedPointIndex >= 0
                ? m_primarySelectedPointIndex - shift : -1;
    }

    std::sort(indicesToRemove.begin(), indicesToRemove.end(),
              std::greater<int>());
    for (int removeIndex : std::as_const(indicesToRemove))
        m_collection.removePoint(removeIndex);


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
