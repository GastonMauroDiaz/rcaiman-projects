// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// Part of the rcaiman suite.

#ifndef COLLECTIONGRAPHDIALOG_H
#define COLLECTIONGRAPHDIALOG_H

#include "collectioncontract.h"
#include "project.h"

#include <QColor>
#include <QDialog>
#include <QHash>
#include <QRectF>
#include <QVector>

class QComboBox;
class QGraphicsItem;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QPushButton;
class QWidget;

class CollectionGraphDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CollectionGraphDialog(
        const QString &projectRootPath,
        QWidget *parent = nullptr);

public slots:
    void refreshGraph();

signals:
    void checkCollectionsRequested();
    void editCollectionRequested(const QString &collectionDirectory);
    void unregisterCollectionRequested(const QString &collectionDirectory);

private:
    enum class BuildRelationMode {
        Selected,
        All,
        Hidden
    };

    struct NodeInfo {
        QString id;
        QString directory;
        QString label;
        QString collectionClass;
        QRectF rect;
        bool issue = false;
        bool unresolved = false;
    };

    void addCollectionNode(const Collection &collection);
    QString nodeIdForCollection(const Collection &collection) const;
    QString nodeIdForReference(const QString &reference) const;
    QString displayLabel(const Collection &collection) const;
    int roleColumn(const QString &collectionClass) const;
    qreal xForRoleColumn(int roleColumn) const;
    void layoutNodes();
    void drawStructuralEdges();
    void refreshBuildEdges();
    void drawNodes();
    void drawLegend();
    QVector<QGraphicsItem*> drawRelationArrow(
        const QRectF &sourceRect,
        const QRectF &targetRect,
        const QColor &color,
        Qt::PenStyle style,
        qreal width,
        qreal zValue);
    BuildRelationMode buildRelationMode() const;
    int nodeIndexAt(const QPoint &viewportPosition,
                    QGraphicsItem **nodeItem = nullptr) const;
    bool eventFilter(QObject *watched, QEvent *event) override;

    QString m_projectRootPath;
    QVector<Collection> m_collections;
    QVector<NodeInfo> m_nodes;
    QHash<QString, int> m_nodeIndexes;
    CollectionContractAnalysis m_analysis;
    QVector<QGraphicsItem*> m_buildEdgeItems;
    int m_maxPreprocessingLevel = 0;
    QGraphicsScene *m_scene = nullptr;
    QGraphicsView *m_view = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QWidget *m_issueWidget = nullptr;
    QComboBox *m_buildModeCombo = nullptr;
    QPushButton *m_checkCollectionsButton = nullptr;
};

#endif // COLLECTIONGRAPHDIALOG_H
