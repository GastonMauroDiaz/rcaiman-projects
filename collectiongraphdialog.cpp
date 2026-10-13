// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// Part of the rcaiman suite.

#include "collectiongraphdialog.h"
#include "collectionappearance.h"

#include <QBrush>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace {
constexpr qreal NodeWidth = 178.0;
constexpr qreal NodeHeight = 54.0;
constexpr qreal LevelSpacing = 210.0;
constexpr qreal RoleSpacing = 230.0;
constexpr qreal RowSpacing = 82.0;
constexpr qreal BranchGapRows = 1.0;
constexpr qreal LeftMargin = 36.0;
constexpr qreal TopMargin = 116.0;
constexpr qreal Pi = 3.14159265358979323846;

QGraphicsPolygonItem *addArrowHead(QGraphicsScene *scene,
                                   const QPointF &tip,
                                   qreal angle,
                                   const QColor &color,
                                   qreal zValue)
{
    const qreal arrowSize = 9.0;
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
    return head;
}

QString roleTitle(int role)
{
    switch (role) {
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
    setMinimumSize(760, 500);
    resize(1180, 760);

    auto *layout = new QVBoxLayout(this);

    auto *summaryRow = new QHBoxLayout();
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    summaryRow->addWidget(m_summaryLabel, 1);
    auto *buildModeLabel = new QLabel("Build relations:", this);
    summaryRow->addWidget(buildModeLabel);
    m_buildModeCombo = new QComboBox(this);
    m_buildModeCombo->addItem(
        "Selected", static_cast<int>(BuildRelationMode::Selected));
    m_buildModeCombo->addItem(
        "All", static_cast<int>(BuildRelationMode::All));
    m_buildModeCombo->addItem(
        "Hidden", static_cast<int>(BuildRelationMode::Hidden));
    m_buildModeCombo->setCurrentIndex(
        m_buildModeCombo->findData(
            static_cast<int>(BuildRelationMode::Selected)));
    m_buildModeCombo->setToolTip(
        "Controls the visibility of Built from provenance links.");
    summaryRow->addWidget(m_buildModeCombo);
    layout->addLayout(summaryRow);

    m_issueWidget = new QWidget(this);
    auto *issueLayout = new QHBoxLayout(m_issueWidget);
    issueLayout->setContentsMargins(0, 0, 0, 0);
    auto *issueLabel = new QLabel(
        "Some collection contracts need attention.",
        m_issueWidget);
    issueLabel->setStyleSheet("color: #9b1c1c;");
    issueLayout->addWidget(issueLabel, 1);
    m_checkCollectionsButton = new QPushButton(
        "Check collections...", m_issueWidget);
    issueLayout->addWidget(m_checkCollectionsButton);
    layout->addWidget(m_issueWidget);

    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->viewport()->installEventFilter(this);
    layout->addWidget(m_view, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *refreshButton =
        buttons->addButton("Refresh", QDialogButtonBox::ActionRole);
    connect(refreshButton, &QPushButton::clicked,
            this, &CollectionGraphDialog::refreshGraph);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::close);
    connect(m_checkCollectionsButton, &QPushButton::clicked,
            this, &CollectionGraphDialog::checkCollectionsRequested);
    connect(m_buildModeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshBuildEdges(); });
    connect(m_scene, &QGraphicsScene::selectionChanged,
            this, &CollectionGraphDialog::refreshBuildEdges);
    layout->addWidget(buttons);

    refreshGraph();
}

void CollectionGraphDialog::refreshGraph()
{
    const bool preserveViewState = !m_nodes.isEmpty();
    const QTransform previousTransform = m_view->transform();
    const QPointF previousCenter = m_view->mapToScene(
        m_view->viewport()->rect().center());
    QSet<QString> selectedNodeIds;
    if (preserveViewState) {
        for (QGraphicsItem *item : m_scene->selectedItems()) {
            const QString id = item->data(0).toString();
            if (!id.isEmpty())
                selectedNodeIds.insert(id);
        }
    }

    m_collections = Project::discoverCollections(m_projectRootPath);
    std::sort(m_collections.begin(), m_collections.end(),
              [](const Collection &left, const Collection &right) {
                  return QString::compare(
                      left.dirName, right.dirName,
                      Qt::CaseInsensitive) < 0;
              });

    m_analysis = CollectionContract::analyze(
        m_projectRootPath, m_collections);
    m_nodes.clear();
    m_nodeIndexes.clear();
    m_buildEdgeItems.clear();
    m_scene->clear();
    m_maxPreprocessingLevel = 0;

    for (const Collection &collection : std::as_const(m_collections)) {
        addCollectionNode(collection);
        const CollectionGraphPlacement placement =
            m_analysis.placements.value(nodeIdForCollection(collection));
        if (collection.collectionClass == "preprocessed_image_collection"
            && placement.resolved) {
            m_maxPreprocessingLevel = qMax(
                m_maxPreprocessingLevel,
                placement.preprocessingLevel);
        }
    }

    layoutNodes();
    drawStructuralEdges();
    drawNodes();
    drawLegend();
    if (!selectedNodeIds.isEmpty()) {
        const QSignalBlocker blocker(m_scene);
        for (QGraphicsItem *item : m_scene->items()) {
            if (selectedNodeIds.contains(item->data(0).toString()))
                item->setSelected(true);
        }
    }
    refreshBuildEdges();

    const bool hasPreprocessed = std::any_of(
        m_collections.cbegin(), m_collections.cend(),
        [](const Collection &collection) {
            return collection.collectionClass
                == "preprocessed_image_collection";
        });
    m_summaryLabel->setText(
        hasPreprocessed
            ? QStringLiteral("%1 collection(s). Preprocessing levels: L0-L%2.")
                  .arg(m_collections.size())
                  .arg(m_maxPreprocessingLevel)
            : QStringLiteral("%1 collection(s). No preprocessing levels to display.")
                  .arg(m_collections.size()));
    m_issueWidget->setVisible(m_analysis.hasIssues());

    m_scene->setSceneRect(
        m_scene->itemsBoundingRect().adjusted(-40, -30, 80, 60));
    if (preserveViewState) {
        m_view->setTransform(previousTransform);
        m_view->centerOn(previousCenter);
    } else {
        m_view->resetTransform();
        m_view->horizontalScrollBar()->setValue(0);
        m_view->verticalScrollBar()->setValue(0);
    }
}

void CollectionGraphDialog::addCollectionNode(
    const Collection &collection)
{
    NodeInfo node;
    node.id = nodeIdForCollection(collection);
    node.directory = collection.dirPath;
    node.label = displayLabel(collection);
    node.collectionClass = collection.collectionClass;
    node.issue = m_analysis.hasIssuesForCollection(node.id);
    const CollectionGraphPlacement placement =
        m_analysis.placements.value(node.id);
    node.unresolved = collection.collectionClass != "other"
        && !placement.resolved;

    m_nodeIndexes.insert(node.id, m_nodes.size());
    m_nodes.append(node);
}

QString CollectionGraphDialog::nodeIdForCollection(
    const Collection &collection) const
{
    return CollectionContract::collectionId(collection.dirPath);
}

QString CollectionGraphDialog::nodeIdForReference(
    const QString &reference) const
{
    return CollectionContract::collectionId(
        Project::resolveReference(m_projectRootPath, reference));
}

QString CollectionGraphDialog::displayLabel(
    const Collection &collection) const
{
    if (collection.alias.isEmpty())
        return collection.dirName;
    return QStringLiteral("%1\n%2")
        .arg(collection.dirName, collection.alias);
}

int CollectionGraphDialog::roleColumn(
    const QString &collectionClass) const
{
    if (collectionClass == "point_collection")
        return 1;
    if (collectionClass == "segmentation_collection")
        return 2;
    if (collectionClass == "processed_image_collection")
        return 3;
    if (collectionClass == "other")
        return 4;
    return 0;
}

qreal CollectionGraphDialog::xForRoleColumn(int role) const
{
    return LeftMargin
        + (m_maxPreprocessingLevel + 1) * LevelSpacing
        + 24.0
        + (role - 1) * RoleSpacing;
}

void CollectionGraphDialog::layoutNodes()
{
    QHash<QString, QVector<int>> attachedNodes;
    QHash<QString, QVector<int>> branchFrames;
    QVector<int> otherNodes;
    QVector<int> unresolvedNodes;

    for (int nodeIndex = 0; nodeIndex < m_nodes.size(); ++nodeIndex) {
        NodeInfo &node = m_nodes[nodeIndex];
        const CollectionGraphPlacement placement =
            m_analysis.placements.value(node.id);
        if (node.collectionClass == "other") {
            otherNodes.append(nodeIndex);
        } else if (!placement.resolved) {
            node.unresolved = true;
            unresolvedNodes.append(nodeIndex);
        } else if (node.collectionClass
                   == "preprocessed_image_collection") {
            branchFrames[placement.branchRootId].append(nodeIndex);
        } else {
            attachedNodes[placement.frameId].append(nodeIndex);
        }
    }

    QStringList branchRoots = branchFrames.keys();
    std::sort(branchRoots.begin(), branchRoots.end(),
              [this](const QString &left, const QString &right) {
                  const int leftIndex = m_nodeIndexes.value(left, -1);
                  const int rightIndex = m_nodeIndexes.value(right, -1);
                  const QString leftLabel = leftIndex >= 0
                      ? m_nodes.at(leftIndex).label : left;
                  const QString rightLabel = rightIndex >= 0
                      ? m_nodes.at(rightIndex).label : right;
                  return QString::compare(
                      leftLabel, rightLabel,
                      Qt::CaseInsensitive) < 0;
              });

    qreal rowCursor = 0.0;
    for (const QString &rootId : std::as_const(branchRoots)) {
        QVector<int> frames = branchFrames.value(rootId);
        std::sort(frames.begin(), frames.end(),
                  [this](int left, int right) {
                      const CollectionGraphPlacement leftPlacement =
                          m_analysis.placements.value(m_nodes.at(left).id);
                      const CollectionGraphPlacement rightPlacement =
                          m_analysis.placements.value(m_nodes.at(right).id);
                      if (leftPlacement.preprocessingLevel
                          != rightPlacement.preprocessingLevel) {
                          return leftPlacement.preprocessingLevel
                              < rightPlacement.preprocessingLevel;
                      }
                      return QString::compare(
                          m_nodes.at(left).label,
                          m_nodes.at(right).label,
                          Qt::CaseInsensitive) < 0;
                  });

        for (const int frameNodeIndex : std::as_const(frames)) {
            const QString frameId = m_nodes.at(frameNodeIndex).id;
            QHash<int, QVector<int>> nodesByRole;
            for (const int attachedIndex : attachedNodes.value(frameId)) {
                nodesByRole[roleColumn(
                    m_nodes.at(attachedIndex).collectionClass)]
                    .append(attachedIndex);
            }
            int frameHeight = 1;
            for (int role = 1; role <= 3; ++role) {
                QVector<int> indexes = nodesByRole.value(role);
                std::sort(indexes.begin(), indexes.end(),
                          [this](int left, int right) {
                              return QString::compare(
                                  m_nodes.at(left).label,
                                  m_nodes.at(right).label,
                                  Qt::CaseInsensitive) < 0;
                          });
                nodesByRole[role] = indexes;
                frameHeight = qMax(frameHeight, indexes.size());
            }

            const CollectionGraphPlacement framePlacement =
                m_analysis.placements.value(frameId);
            NodeInfo &frameNode = m_nodes[frameNodeIndex];
            frameNode.rect = QRectF(
                LeftMargin
                    + framePlacement.preprocessingLevel * LevelSpacing,
                TopMargin
                    + (rowCursor + (frameHeight - 1) / 2.0) * RowSpacing,
                NodeWidth,
                NodeHeight);

            for (int role = 1; role <= 3; ++role) {
                const QVector<int> indexes = nodesByRole.value(role);
                for (int row = 0; row < indexes.size(); ++row) {
                    NodeInfo &node = m_nodes[indexes.at(row)];
                    node.rect = QRectF(
                        xForRoleColumn(role),
                        TopMargin + (rowCursor + row) * RowSpacing,
                        NodeWidth,
                        NodeHeight);
                }
            }
            rowCursor += frameHeight;
        }
        rowCursor += BranchGapRows;
    }

    if (!unresolvedNodes.isEmpty()) {
        auto *title = m_scene->addText("Unresolved");
        QFont font = title->font();
        font.setBold(true);
        title->setFont(font);
        title->setDefaultTextColor(QColor("#9b1c1c"));
        title->setPos(LeftMargin,
                      TopMargin + rowCursor * RowSpacing - 26.0);

        QHash<int, QVector<int>> nodesByRole;
        for (const int nodeIndex : std::as_const(unresolvedNodes))
            nodesByRole[roleColumn(m_nodes.at(nodeIndex).collectionClass)]
                .append(nodeIndex);
        int unresolvedHeight = 1;
        for (auto it = nodesByRole.begin(); it != nodesByRole.end(); ++it) {
            std::sort(it.value().begin(), it.value().end(),
                      [this](int left, int right) {
                          return QString::compare(
                              m_nodes.at(left).label,
                              m_nodes.at(right).label,
                              Qt::CaseInsensitive) < 0;
                      });
            unresolvedHeight = qMax(unresolvedHeight, it.value().size());
            for (int row = 0; row < it.value().size(); ++row) {
                NodeInfo &node = m_nodes[it.value().at(row)];
                const qreal x = it.key() == 0
                    ? LeftMargin : xForRoleColumn(it.key());
                node.rect = QRectF(
                    x,
                    TopMargin + (rowCursor + row) * RowSpacing,
                    NodeWidth,
                    NodeHeight);
            }
        }
        rowCursor += unresolvedHeight + BranchGapRows;
    }

    std::sort(otherNodes.begin(), otherNodes.end(),
              [this](int left, int right) {
                  return QString::compare(
                      m_nodes.at(left).label,
                      m_nodes.at(right).label,
                      Qt::CaseInsensitive) < 0;
              });
    for (int row = 0; row < otherNodes.size(); ++row) {
        NodeInfo &node = m_nodes[otherNodes.at(row)];
        node.rect = QRectF(
            xForRoleColumn(4),
            TopMargin + row * RowSpacing,
            NodeWidth,
            NodeHeight);
    }
}

void CollectionGraphDialog::drawStructuralEdges()
{
    for (const Collection &collection : std::as_const(m_collections)) {
        const QString collectionId = nodeIdForCollection(collection);
        if (!m_nodeIndexes.contains(collectionId))
            continue;
        const NodeInfo &node = m_nodes.at(m_nodeIndexes.value(collectionId));
        const CollectionGraphPlacement placement =
            m_analysis.placements.value(collectionId);

        if (collection.collectionClass == "preprocessed_image_collection"
            && placement.resolved
            && !collection.previousLevel.trimmed().isEmpty()) {
            const QString previousId =
                nodeIdForReference(collection.previousLevel);
            if (m_nodeIndexes.contains(previousId)) {
                const NodeInfo &previousNode =
                    m_nodes.at(m_nodeIndexes.value(previousId));
                const CollectionGraphPlacement previousPlacement =
                    m_analysis.placements.value(previousId);
                if (previousNode.collectionClass
                        != "preprocessed_image_collection"
                    || !previousPlacement.resolved
                    || placement.branchRootId
                        != previousPlacement.branchRootId
                    || placement.preprocessingLevel
                        != previousPlacement.preprocessingLevel + 1) {
                    continue;
                }
                drawRelationArrow(
                    node.rect,
                    previousNode.rect,
                    QColor("#00aa00"), Qt::SolidLine, 2.2, 0.2);
            }
        }
        if (roleColumn(collection.collectionClass) >= 1
            && roleColumn(collection.collectionClass) <= 3
            && placement.resolved
            && !collection.imageFrame.trimmed().isEmpty()) {
            const QString frameId = nodeIdForReference(collection.imageFrame);
            if (m_nodeIndexes.contains(frameId)
                && placement.frameId == frameId) {
                const NodeInfo &frameNode =
                    m_nodes.at(m_nodeIndexes.value(frameId));
                if (frameNode.collectionClass
                    != "preprocessed_image_collection") {
                    continue;
                }
                drawRelationArrow(
                    node.rect,
                    frameNode.rect,
                    QColor("#000000"), Qt::SolidLine, 1.8, 0.1);
            }
        }
    }
}

void CollectionGraphDialog::refreshBuildEdges()
{
    for (QGraphicsItem *item : std::as_const(m_buildEdgeItems)) {
        if (item && item->scene() == m_scene)
            m_scene->removeItem(item);
        delete item;
    }
    m_buildEdgeItems.clear();

    const BuildRelationMode mode = buildRelationMode();
    if (mode == BuildRelationMode::Hidden)
        return;

    QSet<QString> selectedIds;
    if (mode == BuildRelationMode::Selected) {
        for (QGraphicsItem *item : m_scene->selectedItems()) {
            const QString id = item->data(0).toString();
            if (!id.isEmpty())
                selectedIds.insert(id);
        }
        if (selectedIds.isEmpty())
            return;
    }

    for (const Collection &collection : std::as_const(m_collections)) {
        const QString resultId = nodeIdForCollection(collection);
        if (!m_nodeIndexes.contains(resultId)
            || collection.collectionClass == "other") {
            continue;
        }
        QSet<QString> drawnSources;
        for (const QString &reference : collection.builtFrom) {
            const QString sourceId = nodeIdForReference(reference);
            if (!m_nodeIndexes.contains(sourceId)
                || sourceId == resultId
                || drawnSources.contains(sourceId)
                || m_nodes.at(m_nodeIndexes.value(sourceId)).collectionClass
                    == "other") {
                continue;
            }
            drawnSources.insert(sourceId);
            if (mode == BuildRelationMode::Selected
                && !selectedIds.contains(resultId)
                && !selectedIds.contains(sourceId)) {
                continue;
            }

            const QVector<QGraphicsItem*> items = drawRelationArrow(
                m_nodes.at(m_nodeIndexes.value(resultId)).rect,
                m_nodes.at(m_nodeIndexes.value(sourceId)).rect,
                QColor("#ff5500"), Qt::DashLine, 1.8, 0.7);
            m_buildEdgeItems.append(items);
        }
    }

    m_scene->setSceneRect(
        m_scene->itemsBoundingRect().adjusted(-40, -30, 80, 60));
}

void CollectionGraphDialog::drawNodes()
{
    for (const NodeInfo &node : std::as_const(m_nodes)) {
        QPainterPath path;
        path.addRoundedRect(node.rect, 8, 8);
        auto *rect = m_scene->addPath(
            path,
            QPen(node.issue ? QColor("#b3261e") : QColor("#6b7280"),
                 node.issue ? 2.0 : 1.2),
            QBrush(CollectionAppearance::surfaceColor(node.collectionClass)));
        rect->setZValue(2);
        rect->setFlag(QGraphicsItem::ItemIsSelectable, true);
        rect->setData(0, node.id);
        rect->setToolTip(node.label);

        auto *text = m_scene->addText(QString());
        text->setAcceptedMouseButtons(Qt::NoButton);
        text->setDefaultTextColor(
            node.issue ? QColor("#7f1d1d") : QColor("#111827"));
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

    const qreal preprocessedWidth =
        m_maxPreprocessingLevel * LevelSpacing + NodeWidth;
    auto *preprocessedTitle = m_scene->addText("Preprocessed");
    preprocessedTitle->setFont(headerFont);
    preprocessedTitle->setDefaultTextColor(QColor("#374151"));
    preprocessedTitle->setPos(
        LeftMargin + (preprocessedWidth
                      - preprocessedTitle->boundingRect().width()) / 2.0,
        TopMargin - 78.0);

    QFont levelFont;
    levelFont.setPointSize(9);
    levelFont.setBold(true);
    for (int level = 0; level <= m_maxPreprocessingLevel; ++level) {
        auto *title = m_scene->addText(
            QStringLiteral("L%1").arg(level));
        title->setFont(levelFont);
        title->setDefaultTextColor(QColor("#4b5563"));
        title->setPos(
            LeftMargin + level * LevelSpacing
                + (NodeWidth - title->boundingRect().width()) / 2.0,
            TopMargin - 46.0);
    }

    for (int role = 1; role <= 4; ++role) {
        auto *title = m_scene->addText(roleTitle(role));
        title->setFont(headerFont);
        title->setDefaultTextColor(QColor("#374151"));
        title->setPos(xForRoleColumn(role), TopMargin - 62.0);
    }

    qreal legendY = TopMargin + 40.0;
    for (const NodeInfo &node : std::as_const(m_nodes))
        legendY = qMax(legendY, node.rect.bottom() + 56.0);

    auto addLegend = [&](qreal x, const QString &text,
                         const QColor &color, Qt::PenStyle style) {
        const QVector<QGraphicsItem*> items = drawRelationArrow(
            QRectF(x, legendY - 14, 1, 28),
            QRectF(x + 64, legendY - 14, 1, 28),
            color, style, 2.0, 4.0);
        Q_UNUSED(items);
        auto *label = m_scene->addText(text);
        label->setDefaultTextColor(QColor("#374151"));
        label->setPos(x + 76, legendY - 13);
    };
    addLegend(LeftMargin, "uses image frame",
              QColor("#000000"), Qt::SolidLine);
    addLegend(LeftMargin + 250, "previous level",
              QColor("#00aa00"), Qt::SolidLine);
    addLegend(LeftMargin + 480, "built from",
              QColor("#ff5500"), Qt::DashLine);

    auto *note = m_scene->addText(
        "Arrows point to the declared reference. Build relations do not "
        "control placement or autoload.");
    QFont noteFont = note->font();
    noteFont.setPointSize(8);
    note->setFont(noteFont);
    note->setDefaultTextColor(QColor("#6b7280"));
    note->setTextWidth(620);
    note->setPos(LeftMargin, legendY + 22.0);
}

QVector<QGraphicsItem*> CollectionGraphDialog::drawRelationArrow(
    const QRectF &sourceRect,
    const QRectF &targetRect,
    const QColor &color,
    Qt::PenStyle style,
    qreal width,
    qreal zValue)
{
    QVector<QGraphicsItem*> items;
    const QPointF sourceCenter = sourceRect.center();
    const QPointF targetCenter = targetRect.center();
    if (QLineF(sourceCenter, targetCenter).length() < 1.0)
        return items;

    QPointF source;
    QPointF target;
    qreal arrowAngle = 0.0;
    QPainterPath path;

    if (std::abs(sourceCenter.x() - targetCenter.x()) < 4.0) {
        const bool targetBelow = targetCenter.y() > sourceCenter.y();
        source = targetBelow
            ? QPointF(sourceCenter.x(), sourceRect.bottom())
            : QPointF(sourceCenter.x(), sourceRect.top());
        target = targetBelow
            ? QPointF(targetCenter.x(), targetRect.top())
            : QPointF(targetCenter.x(), targetRect.bottom());
        arrowAngle = targetBelow ? Pi / 2.0 : -Pi / 2.0;
        path.moveTo(source);
        path.lineTo(target);
    } else {
        const bool targetRight = targetCenter.x() > sourceCenter.x();
        source = targetRight
            ? QPointF(sourceRect.right(), sourceCenter.y())
            : QPointF(sourceRect.left(), sourceCenter.y());
        target = targetRight
            ? QPointF(targetRect.left(), targetCenter.y())
            : QPointF(targetRect.right(), targetCenter.y());
        const qreal middleX = (source.x() + target.x()) / 2.0;
        path.moveTo(source);
        path.lineTo(QPointF(middleX, source.y()));
        path.lineTo(QPointF(middleX, target.y()));
        path.lineTo(target);
        arrowAngle = targetRight ? 0.0 : Pi;
    }

    auto *line = m_scene->addPath(path, QPen(color, width, style));
    line->setZValue(zValue);
    items.append(line);
    items.append(addArrowHead(
        m_scene, target, arrowAngle, color, zValue + 0.1));
    return items;
}

CollectionGraphDialog::BuildRelationMode
CollectionGraphDialog::buildRelationMode() const
{
    if (!m_buildModeCombo)
        return BuildRelationMode::Selected;
    return static_cast<BuildRelationMode>(
        m_buildModeCombo->currentData(Qt::UserRole).toInt());
}

int CollectionGraphDialog::nodeIndexAt(
    const QPoint &viewportPosition,
    QGraphicsItem **nodeItem) const
{
    if (nodeItem)
        *nodeItem = nullptr;

    const QPointF scenePosition = m_view->mapToScene(viewportPosition);
    for (QGraphicsItem *item : m_scene->items(scenePosition)) {
        const QString nodeId = item->data(0).toString();
        if (nodeId.isEmpty())
            continue;

        const int nodeIndex = m_nodeIndexes.value(nodeId, -1);
        if (nodeIndex < 0)
            continue;
        if (nodeItem)
            *nodeItem = item;
        return nodeIndex;
    }
    return -1;
}

bool CollectionGraphDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_view->viewport())
        return QDialog::eventFilter(watched, event);

    if (event->type() == QEvent::ContextMenu) {
        auto *contextEvent = static_cast<QContextMenuEvent*>(event);
        QGraphicsItem *nodeItem = nullptr;
        const int nodeIndex = nodeIndexAt(
            contextEvent->pos(), &nodeItem);
        if (nodeIndex < 0 || !nodeItem)
            return QDialog::eventFilter(watched, event);

        m_scene->clearSelection();
        nodeItem->setSelected(true);

        QMenu menu(this);
        QAction *editAction = menu.addAction("Edit collection...");
        QAction *unregisterAction =
            menu.addAction("Unregister collection...");
        QAction *selectedAction = menu.exec(contextEvent->globalPos());
        if (selectedAction == editAction) {
            emit editCollectionRequested(
                m_nodes.at(nodeIndex).directory);
        } else if (selectedAction == unregisterAction) {
            emit unregisterCollectionRequested(
                m_nodes.at(nodeIndex).directory);
        }
        return true;
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QGraphicsItem *nodeItem = nullptr;
            const int nodeIndex = nodeIndexAt(
                mouseEvent->position().toPoint(), &nodeItem);
            if (nodeIndex >= 0 && nodeItem) {
                m_scene->clearSelection();
                nodeItem->setSelected(true);
                emit editCollectionRequested(
                    m_nodes.at(nodeIndex).directory);
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}
