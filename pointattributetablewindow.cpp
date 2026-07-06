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

#include "pointattributetablewindow.h"
#include "imageviewer.h"
#include "pointattributetablemodel.h"
#include "pointlayer.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

class PointAttributeSortProxyModel : public QSortFilterProxyModel
{
public:
    explicit PointAttributeSortProxyModel(PointLayer *layer,
                                          QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
        , m_layer(layer)
    {
    }

    void setSelectedFirst(bool enabled)
    {
        if (m_selectedFirst == enabled)
            return;
        m_selectedFirst = enabled;
        invalidate();
    }

    bool selectedFirst() const { return m_selectedFirst; }

    void refreshSelectionOrder()
    {
        if (m_selectedFirst)
            invalidate();
    }

protected:
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override
    {
        m_sortOrder = order;
        QSortFilterProxyModel::sort(column, order);
    }

    bool lessThan(const QModelIndex &left,
                  const QModelIndex &right) const override
    {
        if (m_selectedFirst && m_layer) {
            const bool leftSelected =
                m_layer->selectedPointIndices().contains(left.row());
            const bool rightSelected =
                m_layer->selectedPointIndices().contains(right.row());
            if (leftSelected != rightSelected)
                return m_sortOrder == Qt::AscendingOrder
                    ? leftSelected : !leftSelected;
        }
        return QSortFilterProxyModel::lessThan(left, right);
    }

private:
    QPointer<PointLayer> m_layer;
    bool m_selectedFirst = false;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};

PointAttributeTableWindow::PointAttributeTableWindow(
    PointLayer *layer,
    ImageViewer *viewer,
    QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_layer(layer)
    , m_viewer(viewer)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(layer
        ? QStringLiteral("Attribute Table - %1").arg(layer->name())
        : QStringLiteral("Attribute Table"));
    resize(760, 420);

    auto *layout = new QVBoxLayout(this);
    auto *toolbar = new QHBoxLayout();
    auto *selectedFirstButton = new QPushButton("Selected first", this);
    selectedFirstButton->setCheckable(true);
    auto *zoomButton = new QPushButton("Zoom", this);
    auto *clearButton = new QPushButton("Clear selection", this);
    m_counter = new QLabel(this);
    toolbar->addStretch(1);
    toolbar->addWidget(selectedFirstButton);
    toolbar->addWidget(zoomButton);
    toolbar->addWidget(clearButton);
    toolbar->addWidget(m_counter);
    layout->addLayout(toolbar);

    m_model = new PointAttributeTableModel(layer, this);
    m_proxy = new PointAttributeSortProxyModel(layer, this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(Qt::UserRole);
    m_proxy->setDynamicSortFilter(true);

    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->verticalHeader()->hide();
    m_table->resizeColumnsToContents();
    layout->addWidget(m_table);

    connect(zoomButton, &QPushButton::clicked,
            this, &PointAttributeTableWindow::zoomToSelection);
    connect(selectedFirstButton, &QPushButton::toggled,
            this, [this](bool checked) {
                if (m_proxy)
                    m_proxy->setSelectedFirst(checked);
                const int sortColumn =
                    m_table->horizontalHeader()->sortIndicatorSection() >= 0
                        ? m_table->horizontalHeader()->sortIndicatorSection()
                        : 0;
                m_table->sortByColumn(
                    sortColumn,
                    m_table->horizontalHeader()->sortIndicatorOrder());
            });
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        if (m_layer)
            m_layer->clearSelection();
    });
    connect(m_table->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this, [this]() { scheduleTableSelectionSync(); });

    if (m_layer) {
        connect(m_layer, &PointLayer::selectionChanged,
                this, [this]() {
                    if (m_proxy)
                        m_proxy->refreshSelectionOrder();
                    if (m_proxy && m_proxy->selectedFirst()) {
                        const int sortColumn =
                            m_table->horizontalHeader()
                                ->sortIndicatorSection() >= 0
                                ? m_table->horizontalHeader()
                                      ->sortIndicatorSection()
                                : 0;
                        m_table->sortByColumn(
                            sortColumn,
                            m_table->horizontalHeader()
                                ->sortIndicatorOrder());
                    }
                    syncSelectionFromLayer();
                });
        connect(m_layer, &PointLayer::collectionChanged, this, [this]() {
            m_syncingSelection = true;
            m_model->refresh();
            m_table->resizeColumnsToContents();
            m_syncingSelection = false;
            syncSelectionFromLayer();
        });
        connect(m_layer, &QObject::destroyed,
                this, &QWidget::close);
    }
    if (m_viewer)
        connect(m_viewer, &QObject::destroyed, this, &QWidget::close);

    syncSelectionFromLayer();
}

void PointAttributeTableWindow::syncSelectionFromLayer()
{
    if (m_syncingSelection || !m_layer || !m_table->selectionModel())
        return;

    m_syncingSelection = true;
    QItemSelection selection;
    for (int sourceRow : m_layer->selectedPointIndices()) {
        const QModelIndex sourceIndex = m_model->index(sourceRow, 0);
        const QModelIndex proxyIndex = m_proxy->mapFromSource(sourceIndex);
        if (proxyIndex.isValid()) {
            selection.select(
                m_proxy->index(proxyIndex.row(), 0),
                m_proxy->index(proxyIndex.row(), m_proxy->columnCount() - 1));
        }
    }
    m_table->selectionModel()->select(
        selection,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    const int primary = m_layer->primarySelectedPointIndex();
    const QModelIndex primaryProxy =
        m_proxy->mapFromSource(m_model->index(primary, 0));
    if (primaryProxy.isValid())
        m_table->selectionModel()->setCurrentIndex(
            primaryProxy, QItemSelectionModel::NoUpdate);
    m_syncingSelection = false;
    updateCounter();
}

void PointAttributeTableWindow::scheduleTableSelectionSync()
{
    if (m_syncingSelection || m_selectionSyncPending)
        return;

    m_selectionSyncPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_selectionSyncPending = false;
        applyTableSelectionToLayer();
    });
}

void PointAttributeTableWindow::applyTableSelectionToLayer()
{
    if (m_syncingSelection || !m_layer)
        return;

    QSet<int> indices;
    for (int sourceRow : m_layer->selectedPointIndices()) {
        const QModelIndex proxyIndex =
            m_proxy->mapFromSource(m_model->index(sourceRow, 0));
        if (!proxyIndex.isValid())
            indices.insert(sourceRow);
    }

    const QModelIndexList rows =
        m_table->selectionModel()->selectedRows(0);
    for (const QModelIndex &proxyIndex : rows) {
        const QModelIndex sourceIndex =
            m_proxy->mapToSource(proxyIndex);
        if (sourceIndex.isValid())
            indices.insert(sourceIndex.row());
    }

    int primary = -1;
    const QModelIndex current = m_table->currentIndex();
    if (current.isValid())
        primary = m_proxy->mapToSource(current).row();

    m_syncingSelection = true;
    m_layer->setSelectedPointIndices(indices, primary);
    m_syncingSelection = false;
    updateCounter();
}

void PointAttributeTableWindow::updateCounter()
{
    const int selected = m_layer
        ? m_layer->selectedPointIndices().size() : 0;
    const int total = m_model ? m_model->rowCount() : 0;
    m_counter->setText(
        QStringLiteral("%1 selected / %2").arg(selected).arg(total));
}

void PointAttributeTableWindow::zoomToSelection()
{
    if (m_viewer && m_layer)
        m_viewer->zoomToPointIndices(
            m_layer, m_layer->selectedPointIndices());
}
