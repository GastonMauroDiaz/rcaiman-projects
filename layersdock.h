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

#ifndef LAYERSDOCK_H
#define LAYERSDOCK_H

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMetaObject>
#include <QVector>
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QIcon;
class QPushButton;
class QDoubleSpinBox;
class LayerManager;
class Layer;
class LayerPropertiesWidget;
class PointLayer;

class LayersDock : public QWidget
{
    Q_OBJECT

public:
    explicit LayersDock(QWidget *parent = nullptr);

    void setLayerManager(LayerManager *manager);
    void setProjectPath(const QString &path);
    LayerManager *layerManager() const { return m_manager; }

signals:
    void activeLayerChanged(int index);
    void pointAttributeTableRequested(PointLayer *layer);

private slots:
    void updateList();
    void onItemChanged(QListWidgetItem *item);
    void updateActiveLayerFromUi(int row);
    void addPointLayer();
    void removeLayer();
    void moveUp();
    void moveDown();
    void onFlickerToggled(bool checked);
    void onFlickerSpeedChanged(double seconds);
    void onCurrentRowChanged(int row);
    void onLayerStyleChanged();
    void onLayerVisibilityChanged(bool visible);
    void onLayerContextMenuRequested(const QPoint &position);
    void onRowsMoved(const QModelIndex &sourceParent,
                     int sourceStart,
                     int sourceEnd,
                     const QModelIndex &destinationParent,
                     int destinationRow);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QIcon pointLayerIcon(const Layer *layer) const;
    QVector<int> selectedLayerRows() const;

    LayerManager *m_manager = nullptr;
    QListWidget *m_listWidget = nullptr;
    QPushButton *m_openTableBtn = nullptr;
    QPushButton *m_flickerBtn = nullptr;
    QDoubleSpinBox *m_flickerSpeed = nullptr;
    LayerPropertiesWidget *m_propertiesWidget = nullptr;
    bool m_updatingList = false;
    QString m_projectPath;
    Layer *m_trackedLayer = nullptr;
    QMetaObject::Connection m_trackedFlickerConnection;
};

#endif // LAYERSDOCK_H
