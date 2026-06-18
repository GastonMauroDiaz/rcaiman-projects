#include "layersdock.h"
#include "layermanager.h"
#include "layer.h"
#include "imagelayer.h"
#include "pointlayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>

LayersDock::LayersDock(QWidget *parent)
    : QDockWidget("Layers", parent)
{
    setObjectName("LayersDock");
    setWindowTitle("Layers");

    auto *container = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    mainLayout->addWidget(m_listWidget);

    auto *btnLayout = new QHBoxLayout();
    auto *addLayerBtn = new QPushButton("Add Image...", this);
    auto *addPointsBtn = new QPushButton("Add Points...", this);
    btnLayout->addWidget(addLayerBtn);
    btnLayout->addWidget(addPointsBtn);
    mainLayout->addLayout(btnLayout);

    auto *orderLayout = new QHBoxLayout();
    auto *removeBtn = new QPushButton("Remove", this);
    auto *upBtn = new QPushButton("Up", this);
    auto *downBtn = new QPushButton("Down", this);
    orderLayout->addWidget(removeBtn);
    orderLayout->addWidget(upBtn);
    orderLayout->addWidget(downBtn);
    mainLayout->addLayout(orderLayout);

    setWidget(container);

    connect(addLayerBtn, &QPushButton::clicked, this, [this]() {
        if (!m_manager) return;
        const QString filePath = QFileDialog::getOpenFileName(
            this, "Open Image",
            m_projectPath,
            "Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp)");
        if (filePath.isEmpty()) return;
        auto *layer = new ImageLayer(filePath);
        if (layer->rasterData().isNull()) {
            delete layer;
            QMessageBox::warning(this, "Add Image", "Could not load image.");
            return;
        }
        m_manager->addLayer(layer);
    });

    connect(addPointsBtn, &QPushButton::clicked, this, [this]() {
        addPointLayer();
    });
    connect(removeBtn, &QPushButton::clicked, this, &LayersDock::removeLayer);
    connect(upBtn, &QPushButton::clicked, this, &LayersDock::moveUp);
    connect(downBtn, &QPushButton::clicked, this, &LayersDock::moveDown);

    connect(m_listWidget, &QListWidget::itemChanged,
            this, &LayersDock::onItemChanged);
}

void LayersDock::setProjectPath(const QString &path)
{
    m_projectPath = path;
}

void LayersDock::setLayerManager(LayerManager *manager)
{
    if (m_manager) {
        disconnect(m_manager, nullptr, this, nullptr);
    }

    m_manager = manager;

    if (m_manager) {
        connect(m_manager, &LayerManager::layersChanged,
                this, &LayersDock::updateList);
    }

    updateList();
}

void LayersDock::updateList()
{
    m_updatingList = true;
    m_listWidget->clear();

    if (!m_manager)
    {
        m_updatingList = false;
        return;
    }

    for (int i = 0; i < m_manager->layerCount(); ++i) {
        Layer *layer = m_manager->layerAt(i);
        auto *item = new QListWidgetItem(layer->name());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(layer->isVisible() ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, i);
        m_listWidget->addItem(item);
    }

    m_updatingList = false;
}

void LayersDock::onItemChanged(QListWidgetItem *item)
{
    if (m_updatingList || !m_manager)
        return;

    const int index = m_listWidget->row(item);
    if (index < 0 || index >= m_manager->layerCount())
        return;

    const bool visible = (item->checkState() == Qt::Checked);
    m_manager->setVisibility(index, visible);
}

void LayersDock::addPointLayer()
{
    if (!m_manager)
        return;

    const QString filePath = QFileDialog::getOpenFileName(
        this, "Select CSV points file",
        m_projectPath,
        "CSV files (*.csv)");
    if (filePath.isEmpty())
        return;

    // Use the CSV's parent directory as the point collection source
    const QString dirPath = QFileInfo(filePath).absolutePath();
    QDir dir(dirPath);
    const QStringList csvFiles = dir.entryList({"*.csv"}, QDir::Files);
    if (csvFiles.isEmpty()) {
        QMessageBox::information(this, "Add Points",
                                 "The selected CSV file's directory does not contain any CSV files.");
        return;
    }

    auto *layer = new PointLayer(filePath);
    m_manager->addLayer(layer);
}

void LayersDock::removeLayer()
{
    if (!m_manager)
        return;

    const int currentRow = m_listWidget->currentRow();
    if (currentRow < 0 || currentRow >= m_manager->layerCount())
        return;

    m_manager->removeLayer(currentRow);
}

void LayersDock::moveUp()
{
    if (!m_manager)
        return;

    const int currentRow = m_listWidget->currentRow();
    if (currentRow > 0 && currentRow < m_manager->layerCount()) {
        m_manager->moveUp(currentRow);
        m_listWidget->setCurrentRow(currentRow - 1);
    }
}

void LayersDock::moveDown()
{
    if (!m_manager)
        return;

    const int currentRow = m_listWidget->currentRow();
    if (currentRow >= 0 && currentRow < m_manager->layerCount() - 1) {
        m_manager->moveDown(currentRow);
        m_listWidget->setCurrentRow(currentRow + 1);
    }
}