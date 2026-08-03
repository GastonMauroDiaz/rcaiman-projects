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

#ifndef POINTATTRIBUTETABLEWINDOW_H
#define POINTATTRIBUTETABLEWINDOW_H

#include <QPointer>
#include <QWidget>

class ImageViewer;
class QLabel;
class PointAttributeTableModel;
class PointAttributeSortProxyModel;
class PointLayer;
class QTableView;

class PointAttributeTableWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PointAttributeTableWindow(PointLayer *layer,
                                       ImageViewer *viewer,
                                       QWidget *parent = nullptr);

private:
    void syncSelectionFromLayer();
    void scheduleTableSelectionSync();
    void applyTableSelectionToLayer();
    void updateCounter();
    void zoomToSelection();
    void deleteSelectedPoints();

    QPointer<PointLayer> m_layer;
    QPointer<ImageViewer> m_viewer;
    PointAttributeTableModel *m_model = nullptr;
    PointAttributeSortProxyModel *m_proxy = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_counter = nullptr;
    bool m_syncingSelection = false;
    bool m_selectionSyncPending = false;
};

#endif // POINTATTRIBUTETABLEWINDOW_H
