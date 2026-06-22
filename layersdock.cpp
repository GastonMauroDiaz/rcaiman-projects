#include "layersdock.h"
#include "layermanager.h"
#include "layer.h"
#include "imagelayer.h"
#include "pointlayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QMimeData>
#include <QUrl>

LayersDock::LayersDock(QWidget *parent)
    : QDockWidget("Layers", parent)
{
    setAcceptDrops(true);
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

    // Flicker controls
    m_flickerBtn = new QPushButton("Flicker", this);
    m_flickerBtn->setCheckable(true);
    m_flickerBtn->setEnabled(false);

    m_flickerSpeed = new QDoubleSpinBox(this);
    m_flickerSpeed->setRange(0.1, 10.0);
    m_flickerSpeed->setSingleStep(0.1);
    m_flickerSpeed->setValue(0.5);
    m_flickerSpeed->setSuffix(" s");
    m_flickerSpeed->setEnabled(false);

    orderLayout->addWidget(removeBtn);
    orderLayout->addWidget(upBtn);
    orderLayout->addWidget(downBtn);
    orderLayout->addWidget(m_flickerBtn);
    orderLayout->addWidget(m_flickerSpeed);
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

    connect(m_listWidget, &QListWidget::currentRowChanged,
            this, &LayersDock::updateActiveLayerFromUi);
    connect(m_listWidget, &QListWidget::currentRowChanged,
            this, &LayersDock::onCurrentRowChanged);
    connect(m_listWidget, &QListWidget::itemChanged,
            this, &LayersDock::onItemChanged);

    connect(m_flickerBtn, &QPushButton::toggled,
            this, &LayersDock::onFlickerToggled);
    connect(m_flickerSpeed, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LayersDock::onFlickerSpeedChanged);
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

    // Disconnect tracked layer
    if (m_trackedLayer) {
        disconnect(m_trackedLayer, nullptr, this, nullptr);
        m_trackedLayer = nullptr;
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
    m_listWidget->blockSignals(true);

    if (!m_manager)
    {
        m_updatingList = false;
        m_listWidget->blockSignals(false);
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

    if (m_manager->activeLayer()) {
        const int idx = m_manager->indexOf(m_manager->activeLayer());
        if (idx >= 0)
            m_listWidget->setCurrentRow(idx);
    }

    m_updatingList = false;
    m_listWidget->blockSignals(false);

    // Update flicker controls for the active layer
    onCurrentRowChanged(m_listWidget->currentRow());
}

void LayersDock::updateActiveLayerFromUi(int row)
{
    if (!m_manager)
        return;

    const int count = m_manager->layerCount();
    if (row < 0 || row >= count) {
        emit activeLayerChanged(-1);
        return;
    }

    emit activeLayerChanged(row);
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

    // Disable flicker on the layer being removed
    Layer *layer = m_manager->layerAt(currentRow);
    if (layer && layer->flickerEnabled())
        layer->setFlickerEnabled(false);

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

// --- Flicker ---

void LayersDock::onCurrentRowChanged(int row)
{
    // Disconnect previous tracked layer
    if (m_trackedLayer) {
        disconnect(m_trackedLayer, nullptr, this, nullptr);
        m_trackedLayer = nullptr;
    }

    if (!m_manager || row < 0 || row >= m_manager->layerCount()) {
        m_flickerBtn->setEnabled(false);
        m_flickerSpeed->setEnabled(false);
        m_flickerBtn->setChecked(false);
        return;
    }

    Layer *layer = m_manager->layerAt(row);
    m_trackedLayer = layer;

    // Connect to flickerChanged to sync button state
    connect(layer, &Layer::flickerChanged, this, [this](bool enabled) {
        m_flickerBtn->blockSignals(true);
        m_flickerBtn->setChecked(enabled);
        m_flickerBtn->blockSignals(false);
    });

    // Update controls to match current layer state
    m_flickerBtn->blockSignals(true);
    m_flickerBtn->setChecked(layer->flickerEnabled());
    m_flickerBtn->setEnabled(true);
    m_flickerBtn->blockSignals(false);

    m_flickerSpeed->blockSignals(true);
    m_flickerSpeed->setEnabled(true);
    // Read current interval from layer (stored in seconds by default 0.5)
    // We don't have a getter for the interval value, but defaults are fine
    m_flickerSpeed->blockSignals(false);
}

void LayersDock::onFlickerToggled(bool checked)
{
    if (!m_trackedLayer)
        return;

    m_trackedLayer->setFlickerEnabled(checked);
}

void LayersDock::onFlickerSpeedChanged(double seconds)
{
    if (!m_trackedLayer)
        return;

    m_trackedLayer->setFlickerInterval(seconds);
}

// --- Drag & Drop ---

static bool fileNameIsAcceptable(const QString &suffix)
{
    const QStringList imgExts = {"tif", "tiff", "png", "jpg", "jpeg", "bmp"};
    const QStringList csvExts = {"csv"};
    return imgExts.contains(suffix) || csvExts.contains(suffix);
}

void LayersDock::dragEnterEvent(QDragEnterEvent *event)
{
    if (!m_manager || !event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    for (const QUrl &url : event->mimeData()->urls()) {
        const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
        if (fileNameIsAcceptable(suffix)) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void LayersDock::dropEvent(QDropEvent *event)
{
    if (!m_manager || !event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    for (const QUrl &url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (path.isEmpty())
            continue;

        const QString suffix = QFileInfo(path).suffix().toLower();

        if (fileNameIsAcceptable(suffix)) {
            if (suffix == "csv") {
                auto *layer = new PointLayer(path);
                m_manager->addLayer(layer);
            } else {
                auto *imgLayer = new ImageLayer(path);
                if (imgLayer->rasterData().isNull()) {
                    delete imgLayer;
                    continue;
                }
                m_manager->addLayer(imgLayer);
            }
        }
    }
    event->acceptProposedAction();
}
