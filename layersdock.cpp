#include "layersdock.h"
#include "layermanager.h"
#include "layer.h"
#include "layerpropertieswidget.h"
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
#include <QGroupBox>
#include <QIcon>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QUrl>

LayersDock::LayersDock(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setObjectName("LayersDock");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setIconSize(QSize(18, 18));
    mainLayout->addWidget(m_listWidget);

    auto *btnLayout = new QHBoxLayout();
    auto *addLayerBtn = new QPushButton("Add Image...", this);
    auto *addPointsBtn = new QPushButton("Add Points...", this);
    auto *removeBtn = new QPushButton("Remove", this);
    removeBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #b94a48;"
        "  color: white;"
        "  border: 1px solid #983b39;"
        "  padding: 3px 8px;"
        "}"
        "QPushButton:hover { background-color: #c95754; }"
        "QPushButton:pressed { background-color: #983b39; }"
        "QPushButton:disabled {"
        "  background-color: #d8b2b1;"
        "  color: #f4e8e8;"
        "  border-color: #c99a99;"
        "}");
    btnLayout->addWidget(addLayerBtn);
    btnLayout->addWidget(addPointsBtn);
    btnLayout->addWidget(removeBtn);
    mainLayout->addLayout(btnLayout);

    auto *orderLayout = new QHBoxLayout();
    auto *upBtn = new QPushButton(this);
    upBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    upBtn->setToolTip("Move layer up");
    auto *downBtn = new QPushButton(this);
    downBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    downBtn->setToolTip("Move layer down");
    upBtn->setFixedWidth(30);
    downBtn->setFixedWidth(30);

    m_openTableBtn = new QPushButton("Open table", this);
    m_openTableBtn->setEnabled(false);
    m_openTableBtn->setToolTip(
        "Open the attribute table for the selected point layer.\n"
        "You can also double-click the layer.");

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

    orderLayout->addWidget(upBtn);
    orderLayout->addWidget(downBtn);
    orderLayout->addWidget(m_openTableBtn);
    orderLayout->addWidget(m_flickerBtn);
    orderLayout->addWidget(m_flickerSpeed);
    mainLayout->addLayout(orderLayout);

    auto *propertiesGroup = new QGroupBox("Layer style", this);
    auto *propertiesLayout = new QVBoxLayout(propertiesGroup);
    propertiesLayout->setContentsMargins(4, 4, 4, 4);
    m_propertiesWidget = new LayerPropertiesWidget(propertiesGroup);
    propertiesLayout->addWidget(m_propertiesWidget);
    mainLayout->addWidget(propertiesGroup);

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
    connect(m_openTableBtn, &QPushButton::clicked, this, [this]() {
        if (!m_manager)
            return;
        auto *pointLayer = qobject_cast<PointLayer*>(
            m_manager->layerAt(m_listWidget->currentRow()));
        if (pointLayer)
            emit pointAttributeTableRequested(pointLayer);
    });

    connect(m_listWidget, &QListWidget::currentRowChanged,
            this, &LayersDock::updateActiveLayerFromUi);
    connect(m_listWidget, &QListWidget::currentRowChanged,
            this, &LayersDock::onCurrentRowChanged);
    connect(m_listWidget, &QListWidget::itemChanged,
            this, &LayersDock::onItemChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *item) {
                if (!m_manager || !item)
                    return;
                const int row = m_listWidget->row(item);
                if (auto *pointLayer =
                        qobject_cast<PointLayer*>(m_manager->layerAt(row))) {
                    emit pointAttributeTableRequested(pointLayer);
                }
            });

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
    if (m_propertiesWidget)
        m_propertiesWidget->setLayer(nullptr);

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
        m_openTableBtn->setEnabled(false);
        if (m_propertiesWidget)
            m_propertiesWidget->setLayer(nullptr);
        return;
    }

    for (int i = 0; i < m_manager->layerCount(); ++i) {
        Layer *layer = m_manager->layerAt(i);
        auto *item = new QListWidgetItem(layer->name());
        item->setIcon(pointLayerIcon(layer));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(layer->isVisible() ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, i);
        m_listWidget->addItem(item);

        if (qobject_cast<PointLayer*>(layer)) {
            connect(layer, &Layer::styleChanged,
                    this, &LayersDock::onLayerStyleChanged,
                    Qt::UniqueConnection);
        }
    }

    if (m_manager->activeLayer()) {
        const int idx = m_manager->indexOf(m_manager->activeLayer());
        if (idx >= 0)
            m_listWidget->setCurrentRow(idx);
    }

    m_updatingList = false;
    m_listWidget->blockSignals(false);

    // Update flicker controls for the active layer
    const int currentRow = m_listWidget->currentRow();
    onCurrentRowChanged(currentRow);
    if (currentRow >= 0)
        emit activeLayerChanged(currentRow);
}

void LayersDock::onLayerStyleChanged()
{
    if (!m_manager)
        return;

    auto *layer = qobject_cast<Layer*>(sender());
    const int index = m_manager->indexOf(layer);
    if (index < 0)
        return;

    if (QListWidgetItem *item = m_listWidget->item(index))
        item->setIcon(pointLayerIcon(layer));
}

QIcon LayersDock::pointLayerIcon(const Layer *layer) const
{
    const auto *pointLayer = qobject_cast<const PointLayer*>(layer);
    if (!pointLayer)
        return {};

    constexpr int iconSize = 18;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const PointLayerStyle style = pointLayer->style();
    const QPointF center(iconSize / 2.0, iconSize / 2.0);
    if (style.symbol == PointSymbol::Circle) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(style.color);
        painter.drawEllipse(center, 5.0, 5.0);
    } else {
        QPen pen(style.color, 3.0, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(QPointF(3.5, center.y()),
                         QPointF(iconSize - 3.5, center.y()));
        painter.drawLine(QPointF(center.x(), 3.5),
                         QPointF(center.x(), iconSize - 3.5));
    }

    return QIcon(pixmap);
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

    auto *layer = new PointLayer(filePath, m_manager->scene());
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
        m_openTableBtn->setEnabled(false);
        m_flickerBtn->setEnabled(false);
        m_flickerSpeed->setEnabled(false);
        m_flickerBtn->setChecked(false);
        if (m_propertiesWidget)
            m_propertiesWidget->setLayer(nullptr);
        return;
    }

    Layer *layer = m_manager->layerAt(row);
    m_openTableBtn->setEnabled(qobject_cast<PointLayer*>(layer) != nullptr);
    m_trackedLayer = layer;
    if (m_propertiesWidget)
        m_propertiesWidget->setLayer(layer);

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
                auto *layer = new PointLayer(path, m_manager->scene());
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
