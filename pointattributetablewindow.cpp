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
    auto *zoomButton = new QPushButton("Zoom", this);
    auto *clearButton = new QPushButton("Clear selection", this);
    m_counter = new QLabel(this);
    toolbar->addStretch(1);
    toolbar->addWidget(zoomButton);
    toolbar->addWidget(clearButton);
    toolbar->addWidget(m_counter);
    layout->addLayout(toolbar);

    m_model = new PointAttributeTableModel(layer, this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(Qt::UserRole);
    m_proxy->setDynamicSortFilter(true);

    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    layout->addWidget(m_table);

    connect(zoomButton, &QPushButton::clicked,
            this, &PointAttributeTableWindow::zoomToSelection);
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        if (m_layer)
            m_layer->clearSelection();
    });
    connect(m_table->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this, [this]() { scheduleTableSelectionSync(); });

    if (m_layer) {
        connect(m_layer, &PointLayer::selectionChanged,
                this, &PointAttributeTableWindow::syncSelectionFromLayer);
        connect(m_layer, &PointLayer::collectionChanged, this, [this]() {
            m_syncingSelection = true;
            m_model->refresh();
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
        m_table->setCurrentIndex(primaryProxy);
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
