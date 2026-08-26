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

#ifndef COLLECTIONGRAPHDIALOG_H
#define COLLECTIONGRAPHDIALOG_H

#include "project.h"

#include <QDialog>
#include <QHash>

class QGraphicsScene;
class QGraphicsView;
class QLabel;

class CollectionGraphDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CollectionGraphDialog(
        const QString &projectRootPath,
        QWidget *parent = nullptr);

private:
    struct NodeInfo {
        QString id;
        QString label;
        QString collectionClass;
        QRectF rect;
        bool missing = false;
        bool parentIssue = false;
    };

    void rebuildGraph();
    void addCollectionNode(const Collection &collection);
    QString nodeIdForCollection(const Collection &collection) const;
    QString nodeIdForReference(const QString &reference) const;
    QString displayLabel(const Collection &collection) const;
    QColor colorForClass(const QString &collectionClass,
                         bool missing = false) const;
    int columnForClass(const QString &collectionClass,
                       bool missing = false) const;
    void layoutNodes();
    void drawEdges();
    void drawNodes();
    void drawLegend();
    void drawParentArrow(const QRectF &sourceRect,
                         const QRectF &targetRect,
                         const QColor &color,
                         bool missing);

    QString m_projectRootPath;
    QVector<Collection> m_collections;
    QVector<NodeInfo> m_nodes;
    QHash<QString, int> m_nodeIndexes;
    QGraphicsScene *m_scene = nullptr;
    QGraphicsView *m_view = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_relatedLabel = nullptr;
    QLabel *m_brokenParentsLabel = nullptr;
};

#endif // COLLECTIONGRAPHDIALOG_H
