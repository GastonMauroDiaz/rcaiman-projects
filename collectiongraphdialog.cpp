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

#include "collectiongraphdialog.h"

#include <QBrush>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

namespace {
constexpr qreal NodeWidth = 178.0;
constexpr qreal NodeHeight = 54.0;
constexpr qreal ColumnSpacing = 230.0;
constexpr qreal RowSpacing = 82.0;
constexpr qreal BranchGapRows = 1.0;
constexpr qreal LeftMargin = 36.0;
constexpr qreal TopMargin = 88.0;
constexpr qreal Pi = 3.14159265358979323846;

QString normalizedPath(const QString &path)
{
    if (path.trimmed().isEmpty())
        return {};
    QString normalized =
        QDir::cleanPath(QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return QDir::fromNativeSeparators(normalized);
}

QString columnTitle(int column)
{
    switch (column) {
    case 0:
        return "Preprocessed";
    case 1:
        return "Points";
    case 2:
        return "Segmentation";
    case 3:
        return "Processed";
    case 4:
        return "Other";
    default:
        return {};
    }
}

bool parentIsRequired(const QString &collectionClass)
{
    return collectionClass == "point_collection"
        || collectionClass == "segmentation_collection";
}

QStringList compatibleParentClasses(const QString &collectionClass)
{
    if (collectionClass == "processed_image_collection"
        || collectionClass == "point_collection"
        || collectionClass == "segmentation_collection") {
        return {"preprocessed_image_collection",
                "processed_image_collection"};
    }
    return {};
}

void addArrowHead(QGraphicsScene *scene,
                  const QPointF &tip,
                  qreal angle,
                  const QColor &color,
                  qreal zValue)
{
    const qreal arrowSize = 10.0;
    const QPointF arrowP1 = tip - QPointF(
        std::cos(angle - Pi / 7.0) * arrowSize,
        std::sin(angle - Pi / 7.0) * arrowSize);
    const QPointF arrowP2 = tip - QPointF(
        std::cos(angle + Pi / 7.0) * arrowSize,
        std::sin(angle + Pi / 7.0) * arrowSize);

    QPolygonF arrowHead;
    arrowHead << tip << arrowP1 << arrowP2;
    auto *head = scene->addPolygon(
        arrowHead, QPen(color), QBrush(color));
    head->setZValue(zValue);
}

void drawLegendArrow(QGraphicsScene *scene,
                     const QPointF &start,
                     const QPointF &end,
                     const QColor &color,
                     Qt::PenStyle style)
{
    auto *line = scene->addLine(QLineF(start, end),
                                QPen(color, 2.0, style));
    line->setZValue(4);
    addArrowHead(scene, end, 0.0, color, 5);
}
}

CollectionGraphDialog::CollectionGraphDialog(
    const QString &projectRootPath,
    QWidget *parent)
    : QDialog(parent)
    , m_projectRootPath(projectRootPath)
{
    setWindowTitle("Collection Graph");
    setWindowFlags(windowFlags()
                   | Qt::Window
                   | Qt::WindowMinMaxButtonsHint
                   | Qt::WindowCloseButtonHint);
    setSizeGripEnabled(true);
    setMinimumSize(720, 480);
    resize(1120, 720);

    auto *layout = new QVBoxLayout(this);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    m_relatedLabel = new QLabel(this);
    m_relatedLabel->setWordWrap(true);
    m_relatedLabel->setTextFormat(Qt::PlainText);
    m_relatedLabel->setStyleSheet("color: #4b5563;");
    layout->addWidget(m_relatedLabel);

    m_brokenParentsLabel = new QLabel(this);
    m_brokenParentsLabel->setWordWrap(true);
    m_brokenParentsLabel->setTextFormat(Qt::PlainText);
    m_brokenParentsLabel->setStyleSheet("color: #b3261e;");
    layout->addWidget(m_brokenParentsLabel);

    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    layout->addWidget(m_view, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *refreshButton =
        buttons->addButton("Refresh", QDialogButtonBox::ActionRole);
    connect(refreshButton, &QPushButton::clicked,
            this, &CollectionGraphDialog::rebuildGraph);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::close);
    layout->addWidget(buttons);

    rebuildGraph();
}

void CollectionGraphDialog::rebuildGraph()
{
    m_collections = Project::discoverCollections(m_projectRootPath);
    m_nodes.clear();
    m_nodeIndexes.clear();
    m_scene->clear();

    std::sort(m_collections.begin(), m_collections.end(),
              [](const Collection &left, const Collection &right) {
                  return QString::compare(left.dirName, right.dirName,
                                          Qt::CaseInsensitive) < 0;
              });

    for (const Collection &collection : std::as_const(m_collections))
        addCollectionNode(collection);

    QHash<QString, int> collectionIndexesById;
    QHash<QString, QString> collectionNamesById;
    for (int i = 0; i < m_collections.size(); ++i) {
        const QString id = nodeIdForCollection(m_collections.at(i));
        collectionIndexesById.insert(id, i);
        collectionNamesById.insert(id, m_collections.at(i).dirName);
    }

    int parentEdges = 0;
    QStringList parentIssueLines;
    for (const Collection &collection : std::as_const(m_collections)) {
        const QString collectionId = nodeIdForCollection(collection);
        const QStringList allowedParents =
            compatibleParentClasses(collection.collectionClass);
        if (collection.parent.trimmed().isEmpty()) {
            if (parentIsRequired(collection.collectionClass)) {
                parentIssueLines.append(
                    QStringLiteral("- %1: Parent is required but empty.")
                        .arg(displayLabel(collection).replace('\n', " | ")));
                if (m_nodeIndexes.contains(collectionId))
                    m_nodes[m_nodeIndexes.value(collectionId)].parentIssue = true;
            }
            continue;
        }

        ++parentEdges;
        if (allowedParents.isEmpty()) {
            parentIssueLines.append(
                QStringLiteral(
                    "- %1: Parent is set, but this collection class does not use it.")
                    .arg(displayLabel(collection).replace('\n', " | ")));
            if (m_nodeIndexes.contains(collectionId))
                m_nodes[m_nodeIndexes.value(collectionId)].parentIssue = true;
            continue;
        }

        const QString parentId = nodeIdForReference(collection.parent);
        const int parentIndex = collectionIndexesById.value(parentId, -1);
        if (parentIndex < 0) {
            const QString targetName = QFileInfo(
                Project::resolveReference(m_projectRootPath,
                                          collection.parent)).fileName();
            parentIssueLines.append(
                QStringLiteral("- %1 -> %2: parent target is missing or not registered.")
                    .arg(displayLabel(collection).replace('\n', " | "),
                         targetName.isEmpty() ? collection.parent : targetName));
            if (m_nodeIndexes.contains(collectionId))
                m_nodes[m_nodeIndexes.value(collectionId)].parentIssue = true;
            continue;
        }

        const Collection &parentCollection =
            m_collections.at(parentIndex);
        if (parentId == collectionId) {
            parentIssueLines.append(
                QStringLiteral("- %1: Parent points to itself.")
                    .arg(displayLabel(collection).replace('\n', " | ")));
            if (m_nodeIndexes.contains(collectionId))
                m_nodes[m_nodeIndexes.value(collectionId)].parentIssue = true;
            continue;
        }

        if (!allowedParents.contains(parentCollection.collectionClass,
                                     Qt::CaseInsensitive)) {
            parentIssueLines.append(
                QStringLiteral("- %1 -> %2: expected parent class %3.")
                    .arg(displayLabel(collection).replace('\n', " | "),
                         displayLabel(parentCollection).replace('\n', " | "),
                         allowedParents.join(", ")));
            if (m_nodeIndexes.contains(collectionId))
                m_nodes[m_nodeIndexes.value(collectionId)].parentIssue = true;
        }
    }

    layoutNodes();
    drawEdges();
    drawNodes();
    drawLegend();

    m_summaryLabel->setText(
        QStringLiteral(
            "%1 collection(s). Declared parent links: %2. Parent issues: %3.")
            .arg(m_collections.size())
            .arg(parentEdges)
            .arg(parentIssueLines.size()));

    QStringList relatedLines;
    for (const Collection &collection : std::as_const(m_collections)) {
        if (collection.related.trimmed().isEmpty())
            continue;

        const QString relatedId = nodeIdForReference(collection.related);
        QString targetName = collectionNamesById.value(relatedId);
        if (targetName.isEmpty()) {
            targetName = QFileInfo(
                Project::resolveReference(m_projectRootPath,
                                          collection.related)).fileName();
        }
        if (targetName.isEmpty())
            targetName = collection.related;

        relatedLines.append(
            QStringLiteral("- %1 -> %2")
                .arg(collection.dirName, targetName));
    }
    relatedLines.sort(Qt::CaseInsensitive);
    m_relatedLabel->setVisible(!relatedLines.isEmpty());
    if (!relatedLines.isEmpty()) {
        m_relatedLabel->setText(
            QStringLiteral("Related links (derivation):\n%1")
                .arg(relatedLines.join('\n')));
    } else {
        m_relatedLabel->clear();
    }

    parentIssueLines.sort(Qt::CaseInsensitive);
    m_brokenParentsLabel->setVisible(!parentIssueLines.isEmpty());
    if (!parentIssueLines.isEmpty()) {
        m_brokenParentsLabel->setText(
            QStringLiteral("Parent issues:\n%1")
                .arg(parentIssueLines.join('\n')));
    } else {
        m_brokenParentsLabel->clear();
    }

    m_scene->setSceneRect(
        m_scene->itemsBoundingRect().adjusted(-40, -30, 80, 60));
    m_view->resetTransform();
    m_view->horizontalScrollBar()->setValue(0);
    m_view->verticalScrollBar()->setValue(0);
}

void CollectionGraphDialog::addCollectionNode(const Collection &collection)
{
    NodeInfo node;
    node.id = nodeIdForCollection(collection);
    node.label = displayLabel(collection);
    node.collectionClass = collection.collectionClass;

    m_nodeIndexes.insert(node.id, m_nodes.size());
    m_nodes.append(node);
}

QString CollectionGraphDialog::nodeIdForCollection(
    const Collection &collection) const
{
    return normalizedPath(collection.dirPath);
}

QString CollectionGraphDialog::nodeIdForReference(
    const QString &reference) const
{
    return normalizedPath(
        Project::resolveReference(m_projectRootPath, reference));
}

QString CollectionGraphDialog::displayLabel(
    const Collection &collection) const
{
    if (collection.alias.isEmpty())
        return collection.dirName;
    return QStringLiteral("%1\n%2").arg(collection.dirName, collection.alias);
}

QColor CollectionGraphDialog::colorForClass(
    const QString &collectionClass,
    bool missing) const
{
    if (missing)
        return QColor("#f4b6b6");
    if (collectionClass == "preprocessed_image_collection")
        return QColor("#d8ead3");
    if (collectionClass == "processed_image_collection")
        return QColor("#d8e9f8");
    if (collectionClass == "segmentation_collection")
        return QColor("#efe0f8");
    if (collectionClass == "point_collection")
        return QColor("#fff0c2");
    if (collectionClass == "other")
        return QColor("#f4c7c3");
    return QColor("#e6e6e6");
}

int CollectionGraphDialog::columnForClass(
    const QString &collectionClass,
    bool missing) const
{
    if (missing)
        return -1;
    if (collectionClass == "preprocessed_image_collection")
        return 0;
    if (collectionClass == "point_collection")
        return 1;
    if (collectionClass == "segmentation_collection")
        return 2;
    if (collectionClass == "processed_image_collection")
        return 3;
    if (collectionClass == "other")
        return 4;
    return -1;
}

void CollectionGraphDialog::layoutNodes()
{
    QHash<QString, int> collectionIndexes;
    for (int i = 0; i < m_collections.size(); ++i)
        collectionIndexes.insert(nodeIdForCollection(m_collections.at(i)), i);

    std::function<QString(const Collection&, QSet<QString>*)> anchorFor;
    anchorFor = [&](const Collection &collection,
                    QSet<QString> *seen) -> QString {
        const QString id = nodeIdForCollection(collection);
        if (collection.collectionClass == "preprocessed_image_collection")
            return id;
        if (collection.parent.trimmed().isEmpty() || !seen)
            return {};
        if (seen->contains(id))
            return {};
        seen->insert(id);

        const QString parentId = nodeIdForReference(collection.parent);
        if (!collectionIndexes.contains(parentId))
            return {};
        const Collection &parent = m_collections.at(
            collectionIndexes.value(parentId));
        if (parent.collectionClass == "preprocessed_image_collection")
            return parentId;
        return anchorFor(parent, seen);
    };

    QHash<QString, QHash<int, QVector<int>>> branchColumns;
    QStringList branchOrder;
    QVector<int> otherNodes;

    for (const Collection &collection : std::as_const(m_collections)) {
        if (collection.collectionClass != "preprocessed_image_collection")
            continue;
        const QString id = nodeIdForCollection(collection);
        branchOrder.append(id);
    }

    for (int nodeIndex = 0; nodeIndex < m_nodes.size(); ++nodeIndex) {
        const NodeInfo &node = m_nodes.at(nodeIndex);
        if (node.collectionClass == "other") {
            otherNodes.append(nodeIndex);
            continue;
        }

        QString branchKey = node.id;
        if (collectionIndexes.contains(node.id)) {
            QSet<QString> seen;
            const QString anchor =
                anchorFor(m_collections.at(collectionIndexes.value(node.id)),
                          &seen);
            if (!anchor.isEmpty())
                branchKey = anchor;
        }
        if (!branchOrder.contains(branchKey))
            branchOrder.append(branchKey);
        branchColumns[branchKey][columnForClass(
            node.collectionClass, node.missing)].append(nodeIndex);
    }

    std::sort(branchOrder.begin(), branchOrder.end(),
              [this](const QString &left, const QString &right) {
                  const int leftIndex = m_nodeIndexes.value(left, -1);
                  const int rightIndex = m_nodeIndexes.value(right, -1);
                  const QString leftLabel =
                      leftIndex >= 0 ? m_nodes.at(leftIndex).label : left;
                  const QString rightLabel =
                      rightIndex >= 0 ? m_nodes.at(rightIndex).label : right;
                  return QString::compare(
                      leftLabel, rightLabel, Qt::CaseInsensitive) < 0;
              });

    qreal rowCursor = 0.0;
    for (const QString &branchKey : std::as_const(branchOrder)) {
        QHash<int, QVector<int>> columns = branchColumns.value(branchKey);
        int branchHeight = 1;
        for (int column = 0; column <= 3; ++column) {
            QVector<int> indexes = columns.value(column);
            std::sort(indexes.begin(), indexes.end(),
                      [this](int left, int right) {
                          return QString::compare(
                              m_nodes.at(left).label,
                              m_nodes.at(right).label,
                              Qt::CaseInsensitive) < 0;
                      });
            columns[column] = indexes;
        }

        QHash<int, qreal> nodeRows;
        qreal branchRow = 0.0;
        const QVector<int> preprocessed = columns.value(0);
        for (int column : {1, 2, 3}) {
            const QVector<int> indexes = columns.value(column);
            for (const int nodeIndex : indexes) {
                nodeRows.insert(nodeIndex, branchRow);
                branchRow += 1.0;
            }
        }
        branchHeight = qMax(1, qCeil(branchRow));

        if (!preprocessed.isEmpty()) {
            const qreal parentRow = branchRow > 0.0
                ? (branchRow - 1.0) / 2.0
                : 0.0;
            for (const int nodeIndex : preprocessed)
                nodeRows.insert(nodeIndex, parentRow);
        }

        for (auto it = nodeRows.cbegin(); it != nodeRows.cend(); ++it) {
            NodeInfo &node = m_nodes[it.key()];
            const int column = columnForClass(
                node.collectionClass, node.missing);
            node.rect = QRectF(
                LeftMargin + column * ColumnSpacing,
                TopMargin + (rowCursor + it.value()) * RowSpacing,
                NodeWidth,
                NodeHeight);
        }
        rowCursor += branchHeight + BranchGapRows;
    }

    std::sort(otherNodes.begin(), otherNodes.end(),
              [this](int left, int right) {
                  return QString::compare(m_nodes.at(left).label,
                                          m_nodes.at(right).label,
                                          Qt::CaseInsensitive) < 0;
              });
    for (int row = 0; row < otherNodes.size(); ++row) {
        NodeInfo &node = m_nodes[otherNodes.at(row)];
        node.rect = QRectF(
            LeftMargin + columnForClass(node.collectionClass) * ColumnSpacing,
            TopMargin + row * RowSpacing,
            NodeWidth,
            NodeHeight);
    }

}

void CollectionGraphDialog::drawEdges()
{
    for (const Collection &collection : std::as_const(m_collections)) {
        const QString collectionId = nodeIdForCollection(collection);
        if (!m_nodeIndexes.contains(collectionId))
            continue;

        const NodeInfo &child = m_nodes.at(m_nodeIndexes.value(collectionId));
        if (!collection.parent.trimmed().isEmpty()) {
            const QString parentId = nodeIdForReference(collection.parent);
            if (m_nodeIndexes.contains(parentId)) {
                const NodeInfo &parentNode =
                    m_nodes.at(m_nodeIndexes.value(parentId));
                drawParentArrow(child.rect, parentNode.rect,
                                parentNode.missing ? QColor("#b3261e")
                                                   : QColor("#4b5563"),
                                parentNode.missing);
            }
        }
    }
}

void CollectionGraphDialog::drawNodes()
{
    for (const NodeInfo &node : std::as_const(m_nodes)) {
        QPainterPath path;
        path.addRoundedRect(node.rect, 8, 8);
        const bool hasIssue = node.missing || node.parentIssue;
        auto *rect = m_scene->addPath(
            path,
            QPen(hasIssue ? QColor("#b3261e") : QColor("#6b7280"),
                 hasIssue ? 2.0 : 1.2),
            QBrush(colorForClass(node.collectionClass, node.missing)));
        rect->setZValue(2);

        auto *text = m_scene->addText(QString());
        text->setDefaultTextColor(
            hasIssue ? QColor("#7f1d1d") : QColor("#111827"));
        const QStringList labelLines = node.label.split('\n');
        QString html = QStringLiteral(
            "<div style=\"font-size:9pt; font-weight:600;\">%1</div>")
            .arg(labelLines.value(0).toHtmlEscaped());
        if (labelLines.size() > 1) {
            html += QStringLiteral(
                "<div style=\"font-size:7pt; color:#4b5563;\">%1</div>")
                .arg(labelLines.mid(1).join(" ").toHtmlEscaped());
        }
        text->setHtml(html);
        text->setTextWidth(node.rect.width() - 18);
        text->setPos(node.rect.left() + 9, node.rect.top() + 7);
        text->setZValue(3);
        text->setToolTip(node.label);
    }
}

void CollectionGraphDialog::drawLegend()
{
    QFont headerFont;
    headerFont.setPointSize(11);
    headerFont.setBold(true);
    for (int column = 0; column <= 4; ++column) {
        auto *title = m_scene->addText(columnTitle(column));
        title->setFont(headerFont);
        title->setDefaultTextColor(QColor("#374151"));
        title->setPos(LeftMargin + column * ColumnSpacing,
                      TopMargin - 42);
    }

    qreal legendY = TopMargin + 40.0;
    for (const NodeInfo &node : std::as_const(m_nodes))
        legendY = qMax(legendY, node.rect.bottom() + 56.0);

    drawLegendArrow(
        m_scene,
        QPointF(LeftMargin, legendY),
        QPointF(LeftMargin + 64, legendY),
        QColor("#4b5563"),
        Qt::SolidLine);
    auto *parentText = m_scene->addText("child -> parent");
    parentText->setDefaultTextColor(QColor("#374151"));
    parentText->setPos(LeftMargin + 76, legendY - 13);

    auto *note = m_scene->addText(
        "Some collection classes can declare a parent. "
        "Collections do not declare their children.");
    QFont noteFont = note->font();
    noteFont.setPointSize(8);
    note->setFont(noteFont);
    note->setDefaultTextColor(QColor("#6b7280"));
    note->setTextWidth(520);
    note->setPos(LeftMargin, legendY + 20);
}

void CollectionGraphDialog::drawParentArrow(const QRectF &sourceRect,
                                            const QRectF &targetRect,
                                            const QColor &color,
                                            bool missing)
{
    const QPointF sourceCenter = sourceRect.center();
    const QPointF targetCenter = targetRect.center();
    if (QLineF(sourceCenter, targetCenter).length() < 1.0)
        return;

    const bool leftToRight = targetCenter.x() >= sourceCenter.x();
    QPointF source = leftToRight
        ? QPointF(sourceRect.right(), sourceCenter.y())
        : QPointF(sourceRect.left(), sourceCenter.y());
    QPointF target = leftToRight
        ? QPointF(targetRect.left(), targetCenter.y())
        : QPointF(targetRect.right(), targetCenter.y());

    QPainterPath path;
    path.moveTo(source);
    qreal arrowAngle = leftToRight ? 0.0 : Pi;
    const qreal direction = leftToRight ? 1.0 : -1.0;
    const qreal trunkX = source.x() + direction * 22.0;
    path.lineTo(QPointF(trunkX, source.y()));
    path.lineTo(QPointF(trunkX, target.y()));
    path.lineTo(target);

    auto *line = m_scene->addPath(
        path,
        QPen(color, missing ? 2.4 : 1.8, Qt::SolidLine));
    line->setZValue(0);

    addArrowHead(m_scene, target, arrowAngle, color, 1);
}
