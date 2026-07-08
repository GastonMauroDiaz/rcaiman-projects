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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "imageviewer.h"
#include "layersdock.h"
#include "layermanager.h"
#include "layer.h"
#include "imagelayer.h"
#include "pointlayer.h"
#include "pointcollection.h"
#include "pointattributetablewindow.h"
#include "histogramdock.h"
#include "previewdock.h"
#include "projectcleanupdialog.h"
#include "projectfilesystemmodel.h"
#include "rawreader.h"
#include "collectiondialog.h"
#include "collectionautoload.h"
#include "collectionstyledialog.h"
#include "newprojectdialog.h"
#include "metadataregistry.h"
#include "metadatarulesdialog.h"
#include "registryvalidationdialog.h"
#include "collectionvalidation.h"
#include "collectionvalidationdialog.h"
#include "acquisitionmetadata.h"
#include "tiffreader.h"
#include "rasterrenderer.h"
#include "exampleprojectsdialog.h"
#include "hspimportdialog.h"
#include "hspprojectimporter.h"
#include "shortcuttext.h"

#include <QAction>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QDir>
#include <QDirIterator>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QFrame>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProcess>
#include <QSizePolicy>
#include <QSettings>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QSet>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QDateTime>

namespace {
enum class ProjectExplorerSortMode {
    Name = 0,
    Type = 1
};

class ProjectExplorerSortProxyModel : public QSortFilterProxyModel
{
public:
    explicit ProjectExplorerSortProxyModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setDynamicSortFilter(true);
        setSortCaseSensitivity(Qt::CaseInsensitive);
    }

    void setSortMode(ProjectExplorerSortMode mode)
    {
        if (m_sortMode == mode)
            return;
        m_sortMode = mode;
        invalidate();
    }

protected:
    bool lessThan(const QModelIndex &left,
                  const QModelIndex &right) const override
    {
        const auto *model =
            qobject_cast<const ProjectFileSystemModel*>(sourceModel());
        if (!model)
            return QSortFilterProxyModel::lessThan(left, right);

        const QFileInfo leftInfo = model->fileInfo(left);
        const QFileInfo rightInfo = model->fileInfo(right);
        if (leftInfo.isDir() != rightInfo.isDir())
            return leftInfo.isDir();

        if (!leftInfo.isDir()
            && m_sortMode == ProjectExplorerSortMode::Type) {
            const int extensionCompare = QString::compare(
                leftInfo.suffix(), rightInfo.suffix(), Qt::CaseInsensitive);
            if (extensionCompare != 0)
                return extensionCompare < 0;
        }

        return QString::compare(
            leftInfo.fileName(), rightInfo.fileName(), Qt::CaseInsensitive) < 0;
    }

private:
    ProjectExplorerSortMode m_sortMode = ProjectExplorerSortMode::Name;
};

QString normalizedArtifactPath(const QString &path)
{
    if (path.isEmpty())
        return {};

    QString normalized = QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

QString pointCsvContractMessage(const QString &errorMessage)
{
    return QStringLiteral(
        "This file is not a valid point CSV for rcaiman-projects.\n\n"
        "Point CSV contract:\n"
        "- The file must be a .csv file.\n"
        "- The first two columns must be row,col.\n"
        "- Quoted headers such as \"row\",\"col\" are accepted.\n"
        "- row and col values must be numeric.\n"
        "- Extra columns are allowed and are loaded as attributes.\n"
        "- Empty CSV files are allowed if they contain a valid header.\n\n"
        "Parser error:\n%1").arg(errorMessage);
}

bool sameProjectRoot(const QString &left, const QString &right)
{
    return !left.isEmpty()
        && !right.isEmpty()
        && normalizedArtifactPath(left) == normalizedArtifactPath(right);
}

QJsonObject imageStyleToJson(const ImageLayerStyle &style)
{
    const RasterRenderStyle &render = style.render;
    QJsonObject object;
    object["opacity"] = style.opacity;
    object["blend_mode"] = static_cast<int>(style.blendMode);
    object["mode"] = static_cast<int>(render.mode);
    object["ramp"] = static_cast<int>(render.colorRamp);
    object["stretch"] = static_cast<int>(render.stretchMode);
    object["tail_trim"] = render.tailTrim;
    object["trim_left_tail"] = render.trimLeftTail;
    object["trim_right_tail"] = render.trimRightTail;
    object["trim_percent"] = render.trimPercent;
    object["manual_range_enabled"] = render.manualRangeEnabled;
    object["manual_min"] = render.manualMin;
    object["manual_max"] = render.manualMax;
    object["gamma"] = render.gamma;
    object["band"] = render.band;
    object["red_band"] = render.redBand;
    object["green_band"] = render.greenBand;
    object["blue_band"] = render.blueBand;
    return object;
}

ImageLayerStyle imageStyleFromJson(const QJsonObject &object,
                                   const ImageLayerStyle &fallback)
{
    ImageLayerStyle style = fallback;
    style.opacity = object.value("opacity").toDouble(style.opacity);
    style.blendMode = static_cast<ImageBlendMode>(
        object.value("blend_mode").toInt(static_cast<int>(style.blendMode)));
    RasterRenderStyle &render = style.render;
    render.mode = static_cast<RasterRenderMode>(
        object.value("mode").toInt(static_cast<int>(render.mode)));
    render.colorRamp = static_cast<ColorRamp>(
        object.value("ramp").toInt(static_cast<int>(render.colorRamp)));
    render.stretchMode = static_cast<StretchMode>(
        object.value("stretch").toInt(static_cast<int>(render.stretchMode)));
    render.tailTrim = object.value("tail_trim").toBool(render.tailTrim);
    render.trimLeftTail =
        object.value("trim_left_tail").toBool(render.trimLeftTail);
    render.trimRightTail =
        object.value("trim_right_tail").toBool(render.trimRightTail);
    render.trimPercent =
        object.value("trim_percent").toDouble(render.trimPercent);
    render.manualRangeEnabled =
        object.value("manual_range_enabled").toBool(render.manualRangeEnabled);
    render.manualMin =
        object.value("manual_min").toDouble(render.manualMin);
    render.manualMax =
        object.value("manual_max").toDouble(render.manualMax);
    render.gamma = object.value("gamma").toDouble(render.gamma);
    render.band = object.value("band").toInt(render.band);
    render.redBand = object.value("red_band").toInt(render.redBand);
    render.greenBand = object.value("green_band").toInt(render.greenBand);
    render.blueBand = object.value("blue_band").toInt(render.blueBand);
    return style;
}

QJsonObject pointStyleToJson(const PointLayerStyle &style)
{
    QJsonObject object;
    object["color"] = style.color.name(QColor::HexArgb);
    object["size"] = style.size;
    object["symbol"] = static_cast<int>(style.symbol);
    object["label_field"] = style.labelField;
    object["label_size"] = style.labelSize;
    return object;
}

PointLayerStyle pointStyleFromJson(const QJsonObject &object,
                                   const PointLayerStyle &fallback)
{
    PointLayerStyle style = fallback;
    const QColor color(object.value("color").toString());
    if (color.isValid())
        style.color = color;
    style.size = object.value("size").toDouble(style.size);
    style.symbol = static_cast<PointSymbol>(
        object.value("symbol").toInt(static_cast<int>(style.symbol)));
    style.labelField =
        object.value("label_field").toString(style.labelField);
    style.labelSize =
        object.value("label_size").toDouble(style.labelSize);
    return style;
}

bool pathBelongsToProject(const QString &projectRoot, const QString &path)
{
    const QString relative =
        QDir::fromNativeSeparators(
            QDir(projectRoot).relativeFilePath(
                QFileInfo(path).absoluteFilePath()));
    return relative != ".." && !relative.startsWith("../");
}

bool samePath(const QString &left, const QString &right)
{
    const QString leftPath =
        QDir::cleanPath(QFileInfo(left).absoluteFilePath());
    const QString rightPath =
        QDir::cleanPath(QFileInfo(right).absoluteFilePath());
#ifdef Q_OS_WIN
    return leftPath.compare(rightPath, Qt::CaseInsensitive) == 0;
#else
    return leftPath == rightPath;
#endif
}

QStringList exampleProjectRoots()
{
    QStringList roots;
    roots << QDir(QCoreApplication::applicationDirPath()).filePath("examples");
#ifdef EXAMPLES_DIR
    roots << QString::fromUtf8(EXAMPLES_DIR);
#endif
    roots.removeDuplicates();
    return roots;
}

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QVector<ExampleProjectInfo> discoverExampleProjects()
{
    QVector<ExampleProjectInfo> examples;
    for (const QString &rootPath : exampleProjectRoots()) {
        QDir root(rootPath);
        if (!root.exists())
            continue;

        const QFileInfoList entries =
            root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                               QDir::Name);
        for (const QFileInfo &entry : entries) {
            QString errorMessage;
            const Project project = Project::open(entry.absoluteFilePath(),
                                                  &errorMessage);
            if (!project.isValid())
                continue;

            ExampleProjectInfo info;
            info.path = project.rootPath();
            info.name = project.name();
            info.description = project.description();
            info.readme = readTextFile(
                QDir(project.rootPath()).filePath("README.md"));
            examples.append(info);
        }
    }
    return examples;
}

QPixmap circularThumbnail(const QString &resourcePath, int size)
{
    const QPixmap original(resourcePath);
    if (original.isNull())
        return {};

    QPixmap scaled =
        original.scaled(size, size,
                        Qt::KeepAspectRatioByExpanding,
                        Qt::SmoothTransformation);
    const QRect cropRect(
        (scaled.width() - size) / 2,
        (scaled.height() - size) / 2,
        size,
        size);
    scaled = scaled.copy(cropRect);

    QPixmap thumbnail(size, size);
    thumbnail.fill(Qt::transparent);

    QPainter painter(&thumbnail);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clipPath;
    clipPath.addEllipse(QRectF(0.5, 0.5, size - 1.0, size - 1.0));
    painter.setClipPath(clipPath);
    painter.drawPixmap(0, 0, scaled);
    painter.setClipping(false);
    painter.setPen(QPen(QColor(220, 220, 220), 1.0));
    painter.drawEllipse(QRectF(0.5, 0.5, size - 1.0, size - 1.0));

    return thumbnail;
}

QString collectionStyleKind(const Collection &collection)
{
    if (collection.collectionClass == "point_collection")
        return QStringLiteral("point");
    if (collection.collectionClass == "preprocessed_image_collection"
        || collection.collectionClass == "processed_image_collection"
        || collection.collectionClass == "segmentation_collection") {
        return QStringLiteral("image");
    }
    return {};
}

bool collectionStyleMatchesKind(const CollectionStyleData &style,
                                const QString &kind)
{
    if (kind == "point")
        return style.kind == CollectionStyleData::Kind::Point;
    if (kind == "image")
        return style.kind == CollectionStyleData::Kind::Image;
    return false;
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_projectFileSystemModel(new ProjectFileSystemModel(this))
    , m_projectSortProxy(new ProjectExplorerSortProxyModel(this))
    , m_artifactWatcher(new QFileSystemWatcher(this))
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/branding/rcaiman-icon.png"));
    qApp->setWindowIcon(windowIcon());
    ui->actionPan->setToolTip(ShortcutText::panTooltip());
    ui->actionZoom->setToolTip(ShortcutText::zoomTooltip());
    ui->actionEditPoints->setToolTip(ShortcutText::editPointsTooltip());

    connect(m_artifactWatcher, &QFileSystemWatcher::fileChanged,
            this, [this](const QString &path) {
                scheduleArtifactReload(path);
            });

    m_projectFileSystemModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    m_projectSortProxy->setSourceModel(m_projectFileSystemModel);
    ui->projectTree->setModel(m_projectSortProxy);
    const QModelIndex fileSystemRoot = projectProxyIndex(
        m_projectFileSystemModel->setRootPath(QString()));
    ui->projectTree->setRootIndex(fileSystemRoot);
    ui->projectTree->setDragEnabled(true);
    ui->projectTree->setAcceptDrops(true);
    ui->projectTree->viewport()->setAcceptDrops(true);
    ui->projectTree->viewport()->installEventFilter(this);
    ui->projectTree->setDropIndicatorShown(true);
    setAcceptDrops(true);
    ui->projectTree->setHeaderHidden(true);
    ui->projectTree->hideColumn(1);
    ui->projectTree->hideColumn(2);
    ui->projectTree->hideColumn(3);
    ui->projectTree->setContextMenuPolicy(Qt::CustomContextMenu);

    m_newFolderButton = new QToolButton(this);
    m_newFolderButton->setText("New folder");
    m_newFolderButton->setIcon(
        style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    m_newFolderButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_newFolderButton->setAutoRaise(true);
    m_newFolderButton->setEnabled(false);
    m_projectSortCombo = new QComboBox(this);
    m_projectSortCombo->addItem("Order files by name", static_cast<int>(ProjectExplorerSortMode::Name));
    m_projectSortCombo->addItem("Order files by type", static_cast<int>(ProjectExplorerSortMode::Type));
    m_projectSortCombo->setToolTip("Sort Project Explorer");
    m_projectSortCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_projectSortCombo->setMinimumWidth(
        m_projectSortCombo->fontMetrics().horizontalAdvance(
            QStringLiteral("Order files by name")) + 22);
    QSettings explorerSettings;
    const int savedSortMode =
        explorerSettings.value("projectExplorer/sortMode",
                               static_cast<int>(ProjectExplorerSortMode::Name)).toInt();
    const int savedSortIndex = m_projectSortCombo->findData(savedSortMode);
    if (savedSortIndex >= 0)
        m_projectSortCombo->setCurrentIndex(savedSortIndex);
    auto *projectSortProxy =
        static_cast<ProjectExplorerSortProxyModel*>(m_projectSortProxy);
    projectSortProxy->setSortMode(static_cast<ProjectExplorerSortMode>(
        m_projectSortCombo->currentData().toInt()));
    m_projectSortProxy->sort(0, Qt::AscendingOrder);

    auto *projectExplorerToolbar = new QWidget(this);
    auto *projectExplorerToolbarLayout =
        new QHBoxLayout(projectExplorerToolbar);
    projectExplorerToolbarLayout->setContentsMargins(0, 0, 0, 0);
    projectExplorerToolbarLayout->setSpacing(4);
    projectExplorerToolbarLayout->addWidget(m_newFolderButton);
    projectExplorerToolbarLayout->addStretch(1);
    projectExplorerToolbarLayout->addWidget(m_projectSortCombo);
    ui->dockProjectExplorerLayout->insertWidget(0, projectExplorerToolbar);
    connect(m_newFolderButton, &QToolButton::clicked,
            this, &MainWindow::createProjectFolder);
    connect(m_projectSortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                auto *proxy = static_cast<ProjectExplorerSortProxyModel*>(
                    m_projectSortProxy);
                proxy->setSortMode(static_cast<ProjectExplorerSortMode>(
                    m_projectSortCombo->currentData().toInt()));
                m_projectSortProxy->sort(0, Qt::AscendingOrder);
                QSettings settings;
                settings.setValue("projectExplorer/sortMode",
                                  m_projectSortCombo->currentData().toInt());
            });

    connect(m_projectFileSystemModel, &QAbstractItemModel::rowsInserted,
            this, [this]() { updateProjectActions(); });
    connect(m_projectFileSystemModel, &QAbstractItemModel::rowsRemoved,
            this, [this]() { updateProjectActions(); });
    connect(m_projectFileSystemModel, &QFileSystemModel::directoryLoaded,
            this, [this]() { updateProjectActions(); });

    // Load recent projects
    QSettings recentSettings;
    m_recentProjects = recentSettings.value("recentProjects").toStringList();

    connect(ui->actionNewProject,
            &QAction::triggered,
            this,
            &MainWindow::newProject);

    connect(ui->actionOpenProject,
            &QAction::triggered,
            this,
            &MainWindow::openProject);

    connect(ui->actionCloseProject,
            &QAction::triggered,
            this,
            &MainWindow::closeProject);

    connect(ui->actionSaveProject,
            &QAction::triggered,
            this,
            &MainWindow::saveProject);

    connect(ui->actionSaveProjectAs,
            &QAction::triggered,
            this,
            &MainWindow::saveProjectAs);

    connect(ui->actionSaveProjectAsCleanCopy,
            &QAction::triggered,
            this,
            &MainWindow::saveProjectAsCleanCopy);

    connect(ui->actionProjectReport,
            &QAction::triggered,
            this,
            &MainWindow::createProjectReport);

    connect(ui->actionImportLegacyHspProject,
            &QAction::triggered,
            this,
            &MainWindow::importLegacyHspProject);

    connect(ui->actionOpen_Image,
            &QAction::triggered,
            this,
            &MainWindow::openImage);

    connect(ui->actionImportRaw,
            &QAction::triggered,
            this,
            &MainWindow::importRaw);

    connect(ui->actionExtractRawData,
            &QAction::triggered,
            this,
            &MainWindow::extractRawData);

    connect(ui->actionRegisterCollection,
            &QAction::triggered,
            this,
            &MainWindow::registerCollection);

    connect(ui->actionBuildMetadataRules,
            &QAction::triggered,
            this,
            &MainWindow::buildMetadataRules);

    connect(ui->actionValidateImageMetadata,
            &QAction::triggered,
            this,
            &MainWindow::validateImageMetadata);

    connect(ui->actionValidateCollectionMetadata,
            &QAction::triggered,
            this,
            &MainWindow::validateCollectionMetadata);

    connect(ui->actionCreatePoints,
            &QAction::triggered,
            this,
            &MainWindow::createPointsCsv);

    connect(ui->actionPan,
            &QAction::toggled,
            this,
            [this](bool enabled) {
                if (enabled && ui->actionZoom->isChecked())
                    ui->actionZoom->setChecked(false);
                ImageViewer *viewer = currentImageViewer();
                if (viewer)
                    viewer->setPanModeEnabled(enabled);
            });

    connect(ui->actionZoom,
            &QAction::toggled,
            this,
            [this](bool enabled) {
                if (enabled && ui->actionPan->isChecked())
                    ui->actionPan->setChecked(false);
                ImageViewer *viewer = currentImageViewer();
                if (viewer)
                    viewer->setZoomModeEnabled(enabled);
            });

    m_zoomPercentSpin = new QDoubleSpinBox(this);
    m_zoomPercentSpin->setRange(1.0, 6400.0);
    m_zoomPercentSpin->setDecimals(0);
    m_zoomPercentSpin->setSingleStep(10.0);
    m_zoomPercentSpin->setSuffix("%");
    m_zoomPercentSpin->setToolTip(
        "Viewer zoom level. Enter 100 for actual size; press 0 in the viewer for 100%.");
    m_zoomPercentSpin->setEnabled(false);
    m_zoomPercentSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_zoomPercentSpin->setMinimumWidth(
        m_zoomPercentSpin->fontMetrics().horizontalAdvance(
            QStringLiteral("6400%")) + 18);
    ui->toolBar->insertWidget(ui->actionSplitViewer, m_zoomPercentSpin);
    connect(m_zoomPercentSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            [this](double value) {
                ImageViewer *viewer = currentImageViewer();
                if (!viewer)
                    return;
                if (!qFuzzyCompare(viewer->zoomPercent() + 1.0, value + 1.0))
                    viewer->setZoomPercent(value);
            });

    connect(ui->actionEditPoints,
            &QAction::toggled,
            this,
            &MainWindow::onEditPointsToggled);

    connect(ui->actionCancelEdit,
            &QAction::triggered,
            this,
            &MainWindow::onCancelEditTriggered);

    connect(ui->actionSplitViewer,
            &QAction::triggered,
            this,
            &MainWindow::splitViewer);

    connect(ui->actionSyncSplitViews,
            &QAction::toggled,
            this,
            &MainWindow::onSyncSplitViewsToggled);

    connect(ui->actionSyncSplitZoom,
            &QAction::toggled,
            this,
            &MainWindow::onSyncSplitZoomToggled);

    connect(ui->actionResetWindowLayout,
            &QAction::triggered,
            this,
            &MainWindow::resetWindowLayout);
    connect(ui->actionAboutRcaimanProjects,
            &QAction::triggered,
            this,
            &MainWindow::showAboutDialog);
    connect(ui->actionInteractionShortcuts,
            &QAction::triggered,
            this,
            &MainWindow::showInteractionShortcutsDialog);
    connect(ui->actionExampleProjects,
            &QAction::triggered,
            this,
            &MainWindow::createProjectFromExample);

    connect(ui->projectTree,
            &QTreeView::doubleClicked,
            this,
            &MainWindow::onProjectTreeDoubleClicked);

    connect(ui->projectTree,
            &QTreeView::clicked,
            this,
            &MainWindow::onProjectTreeItemClicked);
    connect(ui->projectTree->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            [this](const QModelIndex &current, const QModelIndex &) {
                onProjectTreeItemClicked(current);
            });

    connect(ui->projectTree,
            &QWidget::customContextMenuRequested,
            this,
            &MainWindow::onProjectTreeContextMenu);

    connect(ui->workspaceTabs,
            &QTabWidget::tabCloseRequested,
            this,
            &MainWindow::onWorkspaceTabCloseRequested);

    connect(ui->workspaceTabs,
            &QTabWidget::currentChanged,
            this,
            &MainWindow::onWorkspaceTabChanged);

    ui->workspaceTabs->setTabsClosable(true);
    ui->workspaceTabs->setElideMode(Qt::ElideMiddle);
    ui->workspaceTabs->setMovable(true);
    ui->workspaceTabs->setDocumentMode(true);
    createViewerEmptyState();

    // Obtain LayersDock from the UI instead of creating it again
    if (QDockWidget *dockLayers = findChild<QDockWidget *>("dockLayers")) {
        m_layersDock = dockLayers->findChild<LayersDock *>("layersDock");
    } else {
        m_layersDock = new LayersDock(this);
        auto *fallbackDockLayers = new QDockWidget("Layers", this);
        fallbackDockLayers->setObjectName("dockLayers");
        fallbackDockLayers->setWidget(m_layersDock);
        addDockWidget(Qt::RightDockWidgetArea, fallbackDockLayers);
    }
    if (m_layersDock && m_project.isValid())
        m_layersDock->setProjectPath(m_project.rootPath());

    if (m_layersDock) {
        connect(m_layersDock, &LayersDock::activeLayerChanged,
                this, &MainWindow::onLayerSelectionChanged);
        connect(m_layersDock, &LayersDock::pointAttributeTableRequested,
                this, &MainWindow::openAttributeTable);
    }

    // Ensure 3-column layout: Explorer (left), Viewer (center), Layers+Properties (right)
    setDockNestingEnabled(true);
    addDockWidget(Qt::LeftDockWidgetArea, ui->dockProjectExplorer);
    addDockWidget(Qt::RightDockWidgetArea, ui->dockViewer);
    addDockWidget(Qt::RightDockWidgetArea, ui->dockLayers);
    addDockWidget(Qt::RightDockWidgetArea, ui->dockProperties);

    // Split into 3 areas: Explorer | Viewer | (Layers ⊓ Properties)
    splitDockWidget(ui->dockViewer, ui->dockLayers, Qt::Horizontal);
    splitDockWidget(ui->dockLayers, ui->dockProperties, Qt::Vertical);

    // Preview Dock: below Project Explorer
    m_previewDock = new PreviewDock(this);
    m_previewDock->setObjectName("dockPreview");
    addDockWidget(Qt::LeftDockWidgetArea, m_previewDock);
    splitDockWidget(ui->dockProjectExplorer, m_previewDock, Qt::Vertical);
    m_previewDock->hide(); // hidden until a project is loaded

    m_histogramDock = new HistogramDock(this);
    addDockWidget(Qt::RightDockWidgetArea, m_histogramDock);
    tabifyDockWidget(ui->dockProperties, m_histogramDock);
    m_histogramDock->raise();

    configureDockSizing();

    // Initial dock proportions.
    resizeDocks({ui->dockProjectExplorer, ui->dockViewer, ui->dockLayers},
                {420, 600, 260}, Qt::Horizontal);

    connect(ui->dockProperties, &QDockWidget::topLevelChanged,
            this, [this](bool floating) {
                if (floating) {
                    ui->dockProperties->setMaximumWidth(QWIDGETSIZE_MAX);
                    if (QWidget *contents = ui->dockProperties->widget())
                        contents->setMaximumWidth(QWIDGETSIZE_MAX);
                } else {
                    ui->dockProperties->setMinimumWidth(220);
                    ui->dockProperties->setMaximumWidth(320);
                    if (QWidget *contents = ui->dockProperties->widget()) {
                        contents->setMinimumWidth(220);
                        contents->setMaximumWidth(320);
                    }
                }
            });
    connect(m_previewDock, &QDockWidget::topLevelChanged,
            this, [this](bool floating) {
                if (floating) {
                    m_previewDock->setMaximumWidth(QWIDGETSIZE_MAX);
                    if (QWidget *contents = m_previewDock->widget())
                        contents->setMaximumWidth(QWIDGETSIZE_MAX);
                } else {
                    m_previewDock->setMinimumWidth(260);
                    m_previewDock->setMaximumWidth(460);
                    if (QWidget *contents = m_previewDock->widget()) {
                        contents->setMinimumWidth(260);
                        contents->setMaximumWidth(460);
                    }
                }
            });
    connect(m_histogramDock, &QDockWidget::topLevelChanged,
            this, [this](bool floating) {
                if (floating) {
                    m_histogramDock->setMaximumWidth(QWIDGETSIZE_MAX);
                    if (QWidget *contents = m_histogramDock->widget())
                        contents->setMaximumWidth(QWIDGETSIZE_MAX);
                } else {
                    m_histogramDock->setMinimumWidth(220);
                    m_histogramDock->setMaximumWidth(320);
                    if (QWidget *contents = m_histogramDock->widget()) {
                        contents->setMinimumWidth(220);
                        contents->setMaximumWidth(320);
                    }
                }
            });

    m_defaultWindowState = saveState(1);
    updateProjectActions();
    updateViewerEmptyState();
    updateRecentProjectsMenu();
    restoreApplicationState();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::createViewerEmptyState()
{
    if (m_viewerEmptyState)
        return;

    m_viewerEmptyState = new QWidget(ui->dockViewerContents);
    m_viewerEmptyState->setAcceptDrops(true);
    m_viewerEmptyState->installEventFilter(this);
    m_viewerEmptyState->setSizePolicy(
        QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *outerLayout = new QVBoxLayout(m_viewerEmptyState);
    outerLayout->setContentsMargins(48, 48, 48, 48);
    outerLayout->addStretch(1);

    auto *contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(10);
    contentLayout->setAlignment(Qt::AlignCenter);

    m_viewerEmptyTitle = new QLabel(m_viewerEmptyState);
    QFont titleFont = m_viewerEmptyTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    m_viewerEmptyTitle->setFont(titleFont);
    m_viewerEmptyTitle->setAlignment(Qt::AlignCenter);
    m_viewerEmptyTitle->setStyleSheet("color: #6f7782;");
    m_viewerEmptyTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(m_viewerEmptyTitle);

    auto *rule = new QFrame(m_viewerEmptyState);
    rule->setFrameShape(QFrame::HLine);
    rule->setFrameShadow(QFrame::Plain);
    rule->setFixedWidth(360);
    rule->setStyleSheet("color: #9aa0a6;");
    rule->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(rule, 0, Qt::AlignCenter);

    m_viewerEmptyBody = new QLabel(m_viewerEmptyState);
    m_viewerEmptyBody->setWordWrap(true);
    m_viewerEmptyBody->setTextFormat(Qt::RichText);
    m_viewerEmptyBody->setAlignment(Qt::AlignLeft);
    m_viewerEmptyBody->setMaximumWidth(520);
    m_viewerEmptyBody->setStyleSheet("color: #7b8490;");
    m_viewerEmptyBody->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(m_viewerEmptyBody);

    contentLayout->addSpacing(22);

    m_viewerQuickTitle = new QLabel(m_viewerEmptyState);
    m_viewerQuickTitle->setFont(titleFont);
    m_viewerQuickTitle->setAlignment(Qt::AlignCenter);
    m_viewerQuickTitle->setStyleSheet("color: #6f7782;");
    m_viewerQuickTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(m_viewerQuickTitle);

    m_viewerQuickRule = new QFrame(m_viewerEmptyState);
    m_viewerQuickRule->setFrameShape(QFrame::HLine);
    m_viewerQuickRule->setFrameShadow(QFrame::Plain);
    m_viewerQuickRule->setFixedWidth(360);
    m_viewerQuickRule->setStyleSheet("color: #9aa0a6;");
    m_viewerQuickRule->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(m_viewerQuickRule, 0, Qt::AlignCenter);

    m_viewerQuickBody = new QLabel(m_viewerEmptyState);
    m_viewerQuickBody->setWordWrap(true);
    m_viewerQuickBody->setTextFormat(Qt::RichText);
    m_viewerQuickBody->setAlignment(Qt::AlignLeft);
    m_viewerQuickBody->setMaximumWidth(520);
    m_viewerQuickBody->setStyleSheet("color: #7b8490;");
    m_viewerQuickBody->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(m_viewerQuickBody);

    outerLayout->addLayout(contentLayout);
    outerLayout->addStretch(2);

    ui->dockViewerLayout->addWidget(m_viewerEmptyState);
}

void MainWindow::updateViewerEmptyState()
{
    if (!m_viewerEmptyState || !ui->workspaceTabs)
        return;

    const bool isEmpty = ui->workspaceTabs->count() == 0;
    ui->workspaceTabs->setVisible(!isEmpty);
    m_viewerEmptyState->setVisible(isEmpty);
    if (!isEmpty)
        return;

    if (m_project.isValid()) {
        m_viewerEmptyTitle->setText("Open a TIFF");
        m_viewerEmptyBody->setText(
            "<ul>"
            "<li>Double-click a TIFF in Project Explorer.</li>"
            "<li>Drag and drop a TIFF here.</li>"
            "<li>Use <b>Image &gt; Open...</b>.</li>"
            "</ul>"
            "<p>Point CSV layers can be added after an image viewer is open.</p>");
        m_viewerQuickTitle->hide();
        m_viewerQuickRule->hide();
        m_viewerQuickBody->hide();
    } else {
        m_viewerEmptyTitle->setText("Open or create a project");
        m_viewerEmptyBody->setText(
            "<ul>"
            "<li><b>Project &gt; New...</b></li>"
            "<li><b>Project &gt; Open...</b></li>"
            "<li><b>Help &gt; Example projects...</b></li>"
            "</ul>");
        m_viewerQuickTitle->setText("Quick view without creating a project");
        m_viewerQuickBody->setText(
            "<ul>"
            "<li>Drag and drop a TIFF here to inspect it.</li>"
            "<li>Drag and drop a compatible point CSV after a TIFF is open.</li>"
            "</ul>");
        m_viewerQuickTitle->show();
        m_viewerQuickRule->show();
        m_viewerQuickBody->show();
    }
}

void MainWindow::showAboutDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("About rcaiman-projects");
    dialog.setWindowIcon(QIcon(":/branding/rcaiman-icon.png"));

    auto *layout = new QVBoxLayout(&dialog);

    auto *brandSceneLayout = new QHBoxLayout();
    brandSceneLayout->setSpacing(14);
    brandSceneLayout->addStretch();
    const QStringList hemisphericImages = {
        ":/branding/hemispheric-1.jpg",
        ":/branding/hemispheric-2.jpg",
        ":/branding/hemispheric-3.jpg"
    };
    for (const QString &imagePath : hemisphericImages) {
        const QPixmap thumbnail = circularThumbnail(imagePath, 56);
        if (thumbnail.isNull())
            continue;
        auto *imageLabel = new QLabel(&dialog);
        imageLabel->setFixedSize(56, 56);
        imageLabel->setPixmap(thumbnail);
        brandSceneLayout->addWidget(imageLabel);
    }

    QPixmap iconPixmap(":/branding/rcaiman-icon.png");
    if (!iconPixmap.isNull()) {
        auto *iconLabel = new QLabel(&dialog);
        iconLabel->setFixedSize(88, 88);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setPixmap(
            iconPixmap.scaled(88, 88,
                              Qt::KeepAspectRatio,
                              Qt::SmoothTransformation));
        brandSceneLayout->addWidget(iconLabel);
    }
    brandSceneLayout->addStretch();
    layout->addLayout(brandSceneLayout);

    QPixmap logoPixmap(":/branding/rcaiman-projects-logo.png");
    if (!logoPixmap.isNull()) {
        auto *logoLabel = new QLabel(&dialog);
        logoLabel->setAlignment(Qt::AlignCenter);
        logoLabel->setPixmap(
            logoPixmap.scaledToWidth(420, Qt::SmoothTransformation));
        layout->addWidget(logoLabel);
    }

    QString aboutText;
    QFile aboutFile(":/branding/about.md");
    if (aboutFile.open(QIODevice::ReadOnly | QIODevice::Text))
        aboutText = QString::fromUtf8(aboutFile.readAll());
    if (aboutText.trimmed().isEmpty())
        aboutText = "rcaiman-projects";

    auto *textBrowser = new QTextBrowser(&dialog);
    textBrowser->setOpenExternalLinks(true);
    textBrowser->setMarkdown(aboutText);
    textBrowser->setMinimumSize(460, 180);
    layout->addWidget(textBrowser);

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    connect(buttons, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::showInteractionShortcutsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Interaction shortcuts");
    dialog.setWindowIcon(windowIcon());

    auto *layout = new QVBoxLayout(&dialog);

    auto *textBrowser = new QTextBrowser(&dialog);
    textBrowser->setMarkdown(ShortcutText::interactionShortcutsMarkdown());
    textBrowser->setMinimumSize(520, 420);
    layout->addWidget(textBrowser);

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    connect(buttons, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::createProjectFromExample()
{
    const QVector<ExampleProjectInfo> examples = discoverExampleProjects();
    ExampleProjectsDialog dialog(examples, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const ExampleProjectInfo example = dialog.selectedExample();
    if (example.path.isEmpty())
        return;

    const QString parentPath = QFileDialog::getExistingDirectory(
        this, "Select Destination Parent Folder");
    if (parentPath.isEmpty())
        return;

    bool accepted = false;
    const QString projectName = QInputDialog::getText(
        this,
        "Create Example Project",
        "New project name:",
        QLineEdit::Normal,
        example.name,
        &accepted).trimmed();
    if (!accepted || projectName.isEmpty())
        return;

    const QString destination = QDir(parentPath).filePath(projectName);
    QString errorMessage;
    const Project project = Project::copyExistingProject(
        example.path, destination, projectName, &errorMessage);
    if (!project.isValid()) {
        QMessageBox::warning(this, "Example Project", errorMessage);
        return;
    }

    loadProject(project);
    statusBar()->showMessage(
        QStringLiteral("Example project created: %1").arg(destination),
        5000);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveProjectSession();
    saveApplicationState();
    QMainWindow::closeEvent(event);
}

void MainWindow::addToRecentProjects(const QString &path)
{
    m_recentProjects.removeAll(path);
    m_recentProjects.push_front(path);
    if (m_recentProjects.size() > 5)
        m_recentProjects.erase(m_recentProjects.end() - 1, m_recentProjects.end());
    QSettings settings;
    settings.setValue("recentProjects", m_recentProjects);
    updateRecentProjectsMenu();
}

void MainWindow::updateRecentProjectsMenu()
{
    QMenu *recentMenu = findChild<QMenu *>("menuRecentProjects");
    if (!recentMenu)
        return;
    qDeleteAll(recentMenu->actions());
    recentMenu->clear();

    bool hasRecent = false;
    for (const QString &path : std::as_const(m_recentProjects)) {
        if (path.isEmpty())
            continue;
        hasRecent = true;
        const QFileInfo projectInfo(path);
        QAction *act = recentMenu->addAction(
            QStringLiteral("%1 - %2")
                .arg(projectInfo.fileName(),
                     projectInfo.dir().dirName()));
        act->setToolTip(path);
        connect(act, &QAction::triggered, this, [this, path]() {
            QString errorMessage;
            const Project project = Project::open(path, &errorMessage);
            if (!project.isValid()) {
                QMessageBox::warning(this, "Open Recent Project", errorMessage);
                return;
            }
            loadProject(project);
        });
    }
    if (!hasRecent) {
        auto *emptyAct = recentMenu->addAction("(empty)");
        emptyAct->setEnabled(false);
    }
}

void MainWindow::newProject()
{
    NewProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString rootPath =
        QDir(dialog.parentDirectory()).filePath(dialog.folderName());
    QString errorMessage;
    const Project project = Project::create(
        rootPath, dialog.folderName(), dialog.description(), &errorMessage);
    if (!project.isValid()) {
        QMessageBox::warning(this, "New Project", errorMessage);
        return;
    }

    loadProject(project);
}

void MainWindow::openProject()
{
    const QString rootPath = QFileDialog::getExistingDirectory(
        this, "Open Project Folder");
    if (rootPath.isEmpty())
        return;

    QString errorMessage;
    const Project project = Project::open(rootPath, &errorMessage);
    if (!project.isValid()) {
        QMessageBox::warning(this, "Open Project", errorMessage);
        return;
    }

    loadProject(project);
}

void MainWindow::importLegacyHspProject()
{
    HspImportDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    HspImportOptions options;
    options.sourceProjectPath = dialog.sourceDirectory();
    options.destinationProjectPath =
        QDir(dialog.parentDirectory()).filePath(dialog.folderName());
    options.projectName = dialog.folderName();
    options.description = dialog.description();

    HspImportReport report;
    QString errorMessage;
    const Project project =
        HspProjectImporter::importProject(options, &report, &errorMessage);
    if (!project.isValid()) {
        QMessageBox::warning(this, "Import legacy HSP project", errorMessage);
        return;
    }

    loadProject(project);

    QString message = QStringLiteral(
        "Imported legacy HSP project.\n\n"
        "RAW files copied: %1\n"
        "PGM images converted: %2\n"
        "Sky point CSV files: %3\n"
        "Sun mark CSV files: %4\n"
        "Optimized sky coefficient TSV files: %5")
        .arg(report.rawFilesCopied)
        .arg(report.pgmFilesConverted)
        .arg(report.skyPointFilesWritten)
        .arg(report.sunMarkFilesWritten)
        .arg(report.optSkyCoefTablesWritten);
    if (!report.warnings.isEmpty()) {
        message += QStringLiteral(
            "\n\nWarnings were written to legacy_hsp_import.md.");
    }
    QMessageBox::information(this, "Import legacy HSP project", message);
    statusBar()->showMessage(
        QStringLiteral("Imported legacy HSP project: %1")
            .arg(project.rootPath()),
        5000);
}

void MainWindow::closeProject()
{
    if (!m_project.isValid())
        return;

    closeCurrentProject();
    statusBar()->showMessage("Project closed", 3000);
}

void MainWindow::saveProject()
{
    QString errorMessage;
    if (!m_project.save(&errorMessage)) {
        QMessageBox::warning(this, "Save Project", errorMessage);
        return;
    }
    statusBar()->showMessage("Project saved", 3000);
}

void MainWindow::saveProjectAs()
{
    if (!m_project.isValid())
        return;

    saveProjectSession();

    const QString parentPath = QFileDialog::getExistingDirectory(
        this,
        "Select Destination Parent Folder",
        QFileInfo(m_project.rootPath()).absolutePath());
    if (parentPath.isEmpty())
        return;

    bool accepted = false;
    const QString projectName = QInputDialog::getText(
        this,
        "Save Project As",
        "New project name:",
        QLineEdit::Normal,
        m_project.name(),
        &accepted).trimmed();
    if (!accepted || projectName.isEmpty())
        return;

    const QString destination = QDir(parentPath).filePath(projectName);
    QString errorMessage;
    const Project copy =
        m_project.saveAs(destination, projectName, &errorMessage);
    if (!copy.isValid()) {
        QMessageBox::warning(this, "Save Project As", errorMessage);
        return;
    }

    loadProject(copy);
    statusBar()->showMessage(
        QStringLiteral("Project saved as: %1").arg(destination), 5000);
}

void MainWindow::saveProjectAsCleanCopy()
{
    if (!m_project.isValid())
        return;

    saveProjectSession();

    const QString parentPath = QFileDialog::getExistingDirectory(
        this,
        "Select Destination Parent Folder",
        QFileInfo(m_project.rootPath()).absolutePath());
    if (parentPath.isEmpty())
        return;

    bool accepted = false;
    const QString projectName = QInputDialog::getText(
        this,
        "Save Project As Clean Copy",
        "New project name:",
        QLineEdit::Normal,
        m_project.name(),
        &accepted).trimmed();
    if (!accepted || projectName.isEmpty())
        return;

    ProjectCleanupDialog cleanupDialog(
        m_project.buildCleanupPlan(), m_project.description(), this);
    if (cleanupDialog.exec() != QDialog::Accepted)
        return;

    const QString destination = QDir(parentPath).filePath(projectName);
    QString errorMessage;
    Project copy =
        m_project.saveAs(destination, projectName, &errorMessage);
    if (!copy.isValid()) {
        QMessageBox::warning(
            this, "Save Project As Clean Copy", errorMessage);
        return;
    }

    const ProjectCleanupPlan selectedPlan = cleanupDialog.selectedPlan();
    if (!copy.applyCleanupPlan(selectedPlan, &errorMessage)) {
        QDir(copy.rootPath()).removeRecursively();
        QMessageBox::warning(
            this, "Save Project As Clean Copy",
            QStringLiteral(
                "The clean copy could not be completed and was removed.\n%1")
                .arg(errorMessage));
        return;
    }

    QFile::remove(copy.reportFilePath());
    if (!copy.setDescription(
            cleanupDialog.selectedDescription(), &errorMessage)) {
        QDir(copy.rootPath()).removeRecursively();
        QMessageBox::warning(
            this, "Save Project As Clean Copy",
            QStringLiteral(
                "The clean copy metadata could not be completed and "
                "the copy was removed.\n%1").arg(errorMessage));
        return;
    }

    loadProject(copy);
    statusBar()->showMessage(
        QStringLiteral("Clean project copy created: %1")
            .arg(destination),
        5000);
}

void MainWindow::createProjectReport()
{
    if (!m_project.isValid())
        return;

    const QString reportPath = m_project.reportFilePath();
    if (QFileInfo::exists(reportPath)) {
        const auto answer = QMessageBox::question(
            this, "Project Report",
            "Replace the existing project report?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    QString errorMessage;
    if (!m_project.writeReport(&errorMessage)) {
        QMessageBox::warning(
            this, "Project Report", errorMessage);
        return;
    }

    m_projectFileSystemModel->refreshDirectory(m_project.rootPath());
    const auto openAnswer = QMessageBox::question(
        this, "Project Report",
        "The report was created as project-report.md.\n"
        "Open it with the default application?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (openAnswer == QMessageBox::Yes) {
        if (!QDesktopServices::openUrl(
                QUrl::fromLocalFile(reportPath))) {
            QMessageBox::warning(
                this, "Project Report",
                "The report was created but could not be opened.");
        }
    }
}

void MainWindow::openImage()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(
        this, "Open Image",
        m_project.isValid() ? m_project.rootPath() : QString(),
        "Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp)");

    for (const QString &fileName : fileNames)
        openImageAtPath(fileName);
}

void MainWindow::importRaw()
{
    if (!m_project.isValid()) {
        QMessageBox::information(this, "Import raw",
                                 "No project is open. Please create or open a project first.");
        return;
    }

    const QStringList fileNames = QFileDialog::getOpenFileNames(
        this, "Select RAW files to import", QString(),
        "RAW files (*.CR2 *.CR3 *.NEF *.ARW *.DNG *.RAF *.ORF *.RW2 *.PEF *.SRW *.RAW *.DCR *.KDC)");
    if (fileNames.isEmpty())
        return;

    const QString rawDir = QDir(m_project.rootPath()).filePath("raw_files");
    QDir().mkpath(rawDir);

    int copiedCount = 0;
    for (const QString &srcPath : fileNames) {
        const QFileInfo fi(srcPath);
        const QString destPath = QDir(rawDir).filePath(fi.fileName());
        if (QFile::exists(destPath))
            continue;
        if (QFile::copy(srcPath, destPath))
            ++copiedCount;
    }

    if (copiedCount > 0) {
        m_projectFileSystemModel->refreshDirectory(rawDir);
        statusBar()->showMessage(
            QStringLiteral("Imported %1 RAW file(s)").arg(copiedCount), 5000);
        updateProjectActions();
    } else {
        statusBar()->showMessage("No new RAW files were imported.", 3000);
    }
}

void MainWindow::extractRawData()
{
    if (!m_project.isValid())
        return;

    const QString rawDir = QDir(m_project.rootPath()).filePath("raw_files");
    const QString sensorDir = QDir(m_project.rootPath()).filePath("sensor_data");
    QDir().mkpath(sensorDir);

    QStringList rawFiles;
    QDirIterator it(rawDir, QDir::Files, QDirIterator::Subdirectories);
    const QStringList rawExtensions = {"cr2", "cr3", "nef", "arw", "dng", "raf",
                                       "orf", "rw2", "pef", "srw", "raw", "dcr", "kdc"};
    while (it.hasNext()) {
        const QString path = it.next();
        const QString ext = QFileInfo(path).suffix().toLower();
        if (rawExtensions.contains(ext))
            rawFiles.append(path);
    }

    int successCount = 0;
    int failCount = 0;

    for (const QString &rawPath : rawFiles) {
        const QFileInfo fi(rawPath);
        const QString baseName = fi.completeBaseName();
        const QString tiffPath = QDir(sensorDir).filePath(baseName + ".tif");
        if (QFile::exists(tiffPath))
            continue;

        statusBar()->showMessage(
            QStringLiteral("Extracting: %1").arg(fi.fileName()), 0);

        QString errorMsg;
        if (RawReader::extractSensorData(rawPath, tiffPath, &errorMsg))
            ++successCount;
        else
            ++failCount;
    }

    statusBar()->showMessage("Writing acquisition metadata sidecars...", 0);
    AcquisitionMetadataSummary metadataSummary;
    QString metadataError;
    const bool metadataWritten = AcquisitionMetadata::regenerate(
        m_project.rootPath(), &metadataSummary, &metadataError);

    statusBar()->showMessage(
        QStringLiteral(
            "Extraction complete: %1 TIFF(s) created, %2 failed; "
            "%3 metadata sidecar(s) written")
            .arg(successCount)
            .arg(failCount)
            .arg(metadataSummary.sidecarsWritten),
        8000);
    m_projectFileSystemModel->refreshDirectory(sensorDir);

    if (!metadataWritten || metadataSummary.metadataErrors > 0) {
        QStringList details;
        if (metadataSummary.metadataErrors > 0) {
            details.append(
                QStringLiteral(
                    "%1 sidecar(s) record a metadata error.")
                    .arg(metadataSummary.metadataErrors));
        }
        if (metadataSummary.writeErrors > 0) {
            details.append(
                QStringLiteral("%1 sidecar(s) could not be written.")
                    .arg(metadataSummary.writeErrors));
        }
        if (!metadataError.isEmpty())
            details.append(metadataError);
        details.append(
            "Inspect the TSV files in sensor_data for per-image details.");
        QMessageBox::warning(
            this, "Extract raw data and metadata", details.join("\n"));
    }

    updateProjectActions();
    refreshReloadableArtifactWatchers();
}

void MainWindow::registerCollection()
{
    if (!m_project.isValid()) {
        QMessageBox::information(this, "Register collection", "No project is open.");
        return;
    }

    const QVector<Collection> existingCollections = Project::discoverCollections(m_project.rootPath());
    CollectionDialog dialog(m_project.rootPath(), existingCollections, selectedProjectPath(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString dirPath = dialog.selectedDirectory();
    if (dirPath.isEmpty())
        return;

    const QString selectedDirName = QDir(dirPath).dirName();
    if (Project::reservedDirectories().contains(selectedDirName, Qt::CaseInsensitive)) {
        QMessageBox::warning(this, "Register collection",
                             QStringLiteral("Cannot register collection: \"%1\" is a reserved directory.").arg(selectedDirName));
        return;
    }

    const QString yamlPath = QDir(dirPath).filePath("collection.yaml");
    if (QFileInfo::exists(yamlPath)) {
        const auto reply = QMessageBox::question(
            this, "Register collection",
            "Collection configuration for this folder already exists. Overwrite it?");
        if (reply != QMessageBox::Yes)
            return;
    }

    QString errorMsg;
    if (Project::registerCollection(m_project.rootPath(),
                                     dirPath,
                                     dialog.collectionClass(),
                                     dialog.parent(),
                                     dialog.related(),
                                     dialog.registry(),
                                     dialog.script(),
                                     &errorMsg))
    {
        m_projectFileSystemModel->refreshDirectory(dirPath);
        statusBar()->showMessage(
            QStringLiteral("Collection registered: %1").arg(QDir(dirPath).dirName()), 5000);
    } else {
        QMessageBox::warning(this, "Register collection", errorMsg);
    }
}

void MainWindow::onProjectTreeDoubleClicked(const QModelIndex &index)
{
    const QString filePath = projectPathFromIndex(index);
    const QFileInfo fi(filePath);

    const QStringList imageExtensions = {"tif", "tiff", "png", "jpg", "jpeg", "bmp"};
    if (imageExtensions.contains(fi.suffix().toLower())) {
        openImageAtPath(filePath);
        return;
    }
}

void MainWindow::onProjectTreeContextMenu(const QPoint &position)
{
    if (!m_project.isValid())
        return;

    const QModelIndex index = ui->projectTree->indexAt(position);
    if (!index.isValid())
        return;

    const QFileInfo info(projectPathFromIndex(index));
    ui->projectTree->setCurrentIndex(index);

    QMenu menu(this);
    QAction *openDefaultAction =
        menu.addAction("Open with default app");
    openDefaultAction->setEnabled(info.isFile());
    QAction *showInFolderAction =
        menu.addAction("Show in folder");
    menu.addSeparator();
    QAction *deleteAction = menu.addAction("Delete");

    QAction *registerAction = nullptr;
    QAction *createPointsAction = nullptr;
    QAction *styleAction = nullptr;
    QAction *copyStyleAction = nullptr;
    QAction *pasteStyleAction = nullptr;
    QAction *validateCollectionAction = nullptr;
    if (info.isDir()) {
        menu.addSeparator();
        registerAction = menu.addAction("Register collection...");
        createPointsAction =
            menu.addAction("Create empty point layer...");
        styleAction = menu.addAction("Edit collection style...");
        copyStyleAction = menu.addAction("Copy collection style");
        pasteStyleAction = menu.addAction("Paste collection style");
        validateCollectionAction =
            menu.addAction("Validate collection...");
    }

    const bool isReserved =
        Project::reservedDirectories().contains(
            info.fileName(), Qt::CaseInsensitive);
    if (registerAction)
        registerAction->setEnabled(!isReserved);

    Collection selectedCollection;
    bool hasRegisteredCollection = false;
    const QVector<Collection> collections =
        Project::discoverCollections(m_project.rootPath());
    for (const Collection &collection : collections) {
        if (QDir::cleanPath(collection.dirPath)
            == QDir::cleanPath(info.absoluteFilePath())) {
            selectedCollection = collection;
            hasRegisteredCollection = true;
            break;
        }
    }
    const bool supportsStyle = info.isDir() && hasRegisteredCollection
        && selectedCollection.collectionClass != "other";
    const QString selectedStyleKind =
        hasRegisteredCollection ? collectionStyleKind(selectedCollection) : QString();
    const CollectionStyleData selectedStyle = supportsStyle
        ? CollectionStyleFile::load(selectedCollection.dirPath)
        : CollectionStyleData();
    const bool hasValidPersistedStyle = supportsStyle
        && collectionStyleMatchesKind(selectedStyle, selectedStyleKind);
    const bool canPasteStyle = supportsStyle
        && !m_copiedCollectionStyleKind.isEmpty()
        && m_copiedCollectionStyleKind == selectedStyleKind
        && collectionStyleMatchesKind(
            m_copiedCollectionStyle, m_copiedCollectionStyleKind);
    const bool supportsCreatePoints = info.isDir() && hasRegisteredCollection
        && selectedCollection.collectionClass == "point_collection";
    const bool supportsCollectionValidation =
        info.isDir() && hasRegisteredCollection
        && CollectionValidation::isEligible(
            m_project.rootPath(), selectedCollection);
    if (createPointsAction)
        createPointsAction->setEnabled(supportsCreatePoints);
    if (styleAction)
        styleAction->setEnabled(supportsStyle);
    if (copyStyleAction)
        copyStyleAction->setEnabled(hasValidPersistedStyle);
    if (pasteStyleAction)
        pasteStyleAction->setEnabled(canPasteStyle);
    if (validateCollectionAction)
        validateCollectionAction->setEnabled(
            supportsCollectionValidation);

    QAction *validateImageAction = nullptr;
    if (info.isFile()) {
        const QString extension = info.suffix().toLower();
        const bool rawFile =
            QStringList{
                "cr2", "cr3", "nef", "arw", "dng", "raf", "orf",
                "rw2", "pef", "srw", "raw", "dcr", "kdc"
            }.contains(extension);
        const Collection fileCollection =
            collectionForPath(info.absoluteFilePath());
        const bool preprocessedImage =
            CollectionValidation::isEligible(
                m_project.rootPath(), fileCollection)
            && CollectionValidation::preprocessedImageFilters().contains(
                "*." + info.suffix(), Qt::CaseInsensitive);
        if (rawFile || preprocessedImage) {
            menu.addSeparator();
            validateImageAction =
                menu.addAction("Validate image...");
        }
    }

    connect(openDefaultAction, &QAction::triggered, this, [this, info]() {
        if (!QDesktopServices::openUrl(
                QUrl::fromLocalFile(info.absoluteFilePath()))) {
            QMessageBox::warning(
                this, "Open with default app",
                "The selected file could not be opened.");
        }
    });
    connect(showInFolderAction, &QAction::triggered, this, [this, info]() {
#ifdef Q_OS_WIN
        const QString path = QDir::toNativeSeparators(
            info.absoluteFilePath());
        const QStringList arguments = info.isDir()
            ? QStringList{path}
            : QStringList{QStringLiteral("/select,%1").arg(path)};
        if (!QProcess::startDetached(
                QStringLiteral("explorer.exe"), arguments)) {
            QMessageBox::warning(
                this, "Show in folder",
                "The selected location could not be opened.");
        }
#else
        const QString folderPath = info.isDir()
            ? info.absoluteFilePath()
            : info.absoluteDir().absolutePath();
        if (!QDesktopServices::openUrl(
                QUrl::fromLocalFile(folderPath))) {
            QMessageBox::warning(
                this, "Show in folder",
                "The selected location could not be opened.");
        }
#endif
    });
    connect(deleteAction, &QAction::triggered, this, [this, info]() {
        const QString kind = info.isDir() ? "folder" : "file";
        const auto answer = QMessageBox::warning(
            this, "Delete",
            QStringLiteral("Delete the %1 \"%2\"%3?")
                .arg(kind, info.fileName(),
                     info.isDir() ? " and all its contents" : ""),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;

        const QString parentPath = info.absoluteDir().absolutePath();
        const bool removed = info.isDir()
            ? QDir(info.absoluteFilePath()).removeRecursively()
            : QFile::remove(info.absoluteFilePath());
        if (!removed) {
            QMessageBox::warning(
                this, "Delete", "The selected item could not be deleted.");
            return;
        }
        if (m_previewDock)
            m_previewDock->clear();
        m_projectFileSystemModel->refreshDirectory(parentPath);
        updateProjectActions();
    });

    if (registerAction) {
        connect(registerAction, &QAction::triggered,
                this, &MainWindow::registerCollection);
    }
    if (createPointsAction) {
        connect(createPointsAction, &QAction::triggered,
                this, &MainWindow::createPointsCsv);
    }
    if (styleAction) {
        connect(styleAction, &QAction::triggered,
                this, [this, selectedCollection, supportsStyle]() {
                    if (!supportsStyle)
                        return;
                    CollectionStyleDialog dialog(selectedCollection, this);
                    if (dialog.exec() == QDialog::Accepted)
                        m_projectFileSystemModel->refreshDirectory(
                            selectedCollection.dirPath);
                });
    }
    if (copyStyleAction) {
        connect(copyStyleAction, &QAction::triggered,
                this, [this, selectedCollection, selectedStyle,
                       selectedStyleKind, hasValidPersistedStyle]() {
                    if (!hasValidPersistedStyle)
                        return;
                    m_copiedCollectionStyle = selectedStyle;
                    m_copiedCollectionStyleKind = selectedStyleKind;
                    m_copiedCollectionStyleSource =
                        selectedCollection.dirName;
                    statusBar()->showMessage(
                        QStringLiteral("Copied collection style from %1")
                            .arg(selectedCollection.dirName),
                        3000);
                });
    }
    if (pasteStyleAction) {
        connect(pasteStyleAction, &QAction::triggered,
                this, [this, selectedCollection, selectedStyleKind,
                       canPasteStyle]() {
                    if (!canPasteStyle)
                        return;
                    QString errorMessage;
                    if (!CollectionStyleFile::save(
                            selectedCollection.dirPath,
                            m_copiedCollectionStyle,
                            &errorMessage)) {
                        QMessageBox::warning(
                            this, "Paste Collection Style",
                            errorMessage);
                        return;
                    }
                    m_projectFileSystemModel->refreshDirectory(
                        selectedCollection.dirPath);
                    statusBar()->showMessage(
                        QStringLiteral("Pasted %1 collection style to %2")
                            .arg(selectedStyleKind,
                                 selectedCollection.dirName),
                        3000);
                });
    }
    if (validateCollectionAction) {
        connect(validateCollectionAction, &QAction::triggered,
                this, &MainWindow::validateCollectionMetadata);
    }
    if (validateImageAction) {
        connect(validateImageAction, &QAction::triggered,
                this, [this, info]() {
                    const Collection collection =
                        collectionForPath(info.absoluteFilePath());
                    if (CollectionValidation::isEligible(
                            m_project.rootPath(), collection)) {
                        const QList<CollectionFileValidation> pairs =
                            CollectionValidation::resolveRawPairs(
                                m_project.rootPath(), collection);
                        for (const CollectionFileValidation &pair : pairs) {
                            if (!samePath(
                                    pair.preprocessedPath,
                                    info.absoluteFilePath())) {
                                continue;
                            }
                            if (pair.status
                                == CollectionFileValidationStatus::MissingRaw) {
                                QMessageBox::warning(
                                    this, "Validate Image",
                                    QString(
                                        "No RAW with basename \"%1\" was "
                                        "found in raw_files.")
                                        .arg(pair.baseName));
                                return;
                            }
                            if (pair.status
                                == CollectionFileValidationStatus::AmbiguousRaw) {
                                QMessageBox::warning(
                                    this, "Validate Image",
                                    QString(
                                        "More than one RAW with basename "
                                        "\"%1\" was found:\n%2")
                                        .arg(pair.baseName,
                                             pair.rawCandidates.join("\n")));
                                return;
                            }
                            validateRawImage(
                                pair.rawPath,
                                [&]() {
                                    QString registryPath;
                                    CollectionValidation::resolveRegistry(
                                        m_project.rootPath(),
                                        collection, &registryPath);
                                    return registryPath;
                                }());
                            return;
                        }
                    }
                    validateRawImage(info.absoluteFilePath());
                });
    }

    menu.exec(ui->projectTree->viewport()->mapToGlobal(position));
}

void MainWindow::createProjectFolder()
{
    if (!m_project.isValid())
        return;

    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, "New folder", "Folder name:",
        QLineEdit::Normal, QString(), &accepted).trimmed();
    if (!accepted || name.isEmpty())
        return;
    if (name == "." || name == ".."
        || name.contains('/') || name.contains('\\')) {
        QMessageBox::warning(
            this, "New folder", "Enter a simple folder name.");
        return;
    }

    QDir root(m_project.rootPath());
    if (QFileInfo::exists(root.filePath(name))) {
        QMessageBox::warning(
            this, "New folder",
            "A file or folder with that name already exists.");
        return;
    }
    if (!root.mkdir(name)) {
        QMessageBox::warning(
            this, "New folder", "The folder could not be created.");
        return;
    }

    m_projectFileSystemModel->refreshDirectory(m_project.rootPath());
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_viewerEmptyState) {
        if (event->type() == QEvent::DragEnter) {
            dragEnterEvent(static_cast<QDragEnterEvent*>(event));
            return true;
        }
        if (event->type() == QEvent::DragMove) {
            auto *dragEvent = static_cast<QDragMoveEvent*>(event);
            if (dragEvent->mimeData()->hasUrls())
                dragEvent->acceptProposedAction();
            else
                dragEvent->ignore();
            return true;
        }
        if (event->type() == QEvent::Drop) {
            dropEvent(static_cast<QDropEvent*>(event));
            updateViewerEmptyState();
            return true;
        }
    }

    if (watched != ui->projectTree->viewport()
        || !m_project.isValid()) {
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() == QEvent::DragEnter
        || event->type() == QEvent::DragMove) {
        auto *dragEvent = static_cast<QDragMoveEvent*>(event);
        if (dragEvent->mimeData()->hasUrls()) {
            bool hasLocalFile = false;
            for (const QUrl &url : dragEvent->mimeData()->urls()) {
                const QFileInfo info(url.toLocalFile());
                if (info.isFile()) {
                    hasLocalFile = true;
                    break;
                }
            }
            if (hasLocalFile) {
                dragEvent->setDropAction(Qt::CopyAction);
                dragEvent->accept();
                return true;
            }
        }
    } else if (event->type() == QEvent::Drop) {
        auto *dropEvent = static_cast<QDropEvent*>(event);
        const QModelIndex index =
            ui->projectTree->indexAt(dropEvent->position().toPoint());
        if (!index.isValid()) {
            dropEvent->ignore();
            return true;
        }

        QFileInfo destination(projectPathFromIndex(index));
        const QString destinationDirectory = destination.isDir()
            ? destination.absoluteFilePath()
            : destination.absoluteDir().absolutePath();
        if (copyDroppedFiles(
                dropEvent->mimeData()->urls(), destinationDirectory)) {
            dropEvent->setDropAction(Qt::CopyAction);
            dropEvent->accept();
        } else {
            dropEvent->ignore();
        }
        return true;
    }

    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::copyDroppedFiles(
    const QList<QUrl> &urls,
    const QString &destinationDirectory)
{
    QDir destination(destinationDirectory);
    if (!destination.exists())
        return false;

    int copied = 0;
    for (const QUrl &url : urls) {
        const QFileInfo source(url.toLocalFile());
        if (!source.isFile())
            continue;

        const QString targetPath =
            destination.filePath(source.fileName());
        if (QFileInfo(targetPath).absoluteFilePath()
            == source.absoluteFilePath()) {
            continue;
        }

        if (QFileInfo::exists(targetPath)) {
            const auto answer = QMessageBox::question(
                this, "Replace file",
                QStringLiteral("\"%1\" already exists. Replace it?")
                    .arg(source.fileName()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (answer != QMessageBox::Yes)
                continue;
        }

        QFile input(source.absoluteFilePath());
        QSaveFile output(targetPath);
        if (!input.open(QIODevice::ReadOnly)
            || !output.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(
                this, "Add files",
                QStringLiteral("Could not copy \"%1\".")
                    .arg(source.fileName()));
            continue;
        }

        bool ok = true;
        while (!input.atEnd()) {
            const QByteArray chunk = input.read(1024 * 1024);
            if (chunk.isEmpty() && input.error() != QFile::NoError) {
                ok = false;
                break;
            }
            if (output.write(chunk) != chunk.size()) {
                ok = false;
                break;
            }
        }
        if (!ok || !output.commit()) {
            output.cancelWriting();
            QMessageBox::warning(
                this, "Add files",
                QStringLiteral("Could not copy \"%1\".")
                    .arg(source.fileName()));
            continue;
        }
        ++copied;
    }

    if (copied > 0) {
        m_projectFileSystemModel->refreshDirectory(destinationDirectory);
        updateProjectActions();
        statusBar()->showMessage(
            QStringLiteral("%1 file(s) added").arg(copied), 4000);
    }
    return copied > 0;
}

void MainWindow::onProjectTreeItemClicked(const QModelIndex &index)
{
    if (!m_previewDock || !m_project.isValid())
        return;

    const QString filePath = projectPathFromIndex(index);
    const QFileInfo fi(filePath);

    if (!fi.isFile()) {
        m_previewDock->clear();
        return;
    }

    const QString suffix = fi.suffix().toLower();
    const QStringList textExts = {
        "yaml", "yml", "json", "csv", "tsv", "txt", "log", "md"
    };
    const QStringList imgExts = {"tif", "tiff", "png", "jpg", "jpeg", "bmp"};

    if (textExts.contains(suffix)) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString text = QString::fromUtf8(file.readAll());
            if (suffix == "md")
                m_previewDock->showMarkdown(text);
            else
                m_previewDock->showText(text);
        } else {
            m_previewDock->clear();
        }
    } else if (imgExts.contains(suffix)) {
        QPixmap pix;
        if (suffix == "tif" || suffix == "tiff") {
            QString errorMessage;
            const RasterData raster = TiffReader::load(filePath, &errorMessage);
            if (!raster.isNull()) {
                const QImage image = RasterRenderer().renderAuto(raster);
                if (!image.isNull())
                    pix = QPixmap::fromImage(image);
            }
        } else {
            pix.load(filePath);
        }
        if (!pix.isNull())
            m_previewDock->showImage(pix);
        else
            m_previewDock->clear();
    } else {
        m_previewDock->clear();
    }
}

void MainWindow::onWorkspaceTabCloseRequested(int index)
{
    QWidget *tabWidget = ui->workspaceTabs->widget(index);
    if (!tabWidget)
        return;

    if (m_activeViewer
        && (tabWidget == m_activeViewer || tabWidget->isAncestorOf(m_activeViewer))) {
        m_activeViewer.clear();
        onActiveViewerChanged(nullptr);
    }

    const QString key = m_openImageTabs.key(tabWidget);
    if (!key.isEmpty())
        m_openImageTabs.remove(key);

    ui->workspaceTabs->removeTab(index);
    delete tabWidget;

    // Re-resolve the active viewer from the surviving current tab. This also
    // makes closing a non-current tab harmless.
    m_activeViewer.clear();
    ImageViewer *viewer = currentImageViewer();
    m_activeViewer = viewer;
    onActiveViewerChanged(viewer);

    updateProjectActions();
    updateViewerEmptyState();
    refreshReloadableArtifactWatchers();
}

void MainWindow::onLayerSelectionChanged(int index)
{
    if (m_currentPointLayer) {
        disconnect(m_pointLayerConnection);
        disconnect(m_pointCollectionConnection);
        m_currentPointLayer = nullptr;
    }

    ImageViewer *viewer = currentImageViewer();
    if (!viewer) {
        setHistogramLayer(nullptr);
        ui->actionEditPoints->setChecked(false);
        ui->actionEditPoints->setEnabled(false);
        ui->actionCancelEdit->setEnabled(false);
        return;
    }

    LayerManager *mgr = viewer->layerManager();
    if (!mgr) {
        setHistogramLayer(nullptr);
        ui->actionEditPoints->setChecked(false);
        ui->actionEditPoints->setEnabled(false);
        ui->actionCancelEdit->setEnabled(false);
        return;
    }

    if (index < 0 || index >= mgr->layerCount()) {
        mgr->setActiveLayer(nullptr);
        setHistogramLayer(nullptr);
        ui->actionEditPoints->setChecked(viewer->pointEditEnabled());
        ui->actionEditPoints->setEnabled(viewer->pointEditEnabled());
        PointLayer *editingLayer = viewer->editingPointLayer();
        ui->actionCancelEdit->setEnabled(editingLayer && editingLayer->editMode());
        return;
    }

    Layer *layer = mgr->layerAt(index);
    mgr->setActiveLayer(layer);
    setHistogramLayer(layer);

    if (auto *pointLayer = qobject_cast<PointLayer*>(layer)) {
        m_currentPointLayer = pointLayer;
        m_pointLayerConnection = connect(pointLayer, &PointLayer::selectionChanged,
                                         this, &MainWindow::onPointSelectionChanged);
        m_pointCollectionConnection = connect(
            pointLayer, &PointLayer::collectionChanged,
            this, &MainWindow::onPointSelectionChanged);
        updatePropertiesForPointLayer(pointLayer);
        ui->actionEditPoints->setEnabled(true);
    } else {
        m_currentPointLayer = nullptr;
        ui->actionEditPoints->setEnabled(viewer->pointEditEnabled());
    }

    PointLayer *editingLayer = viewer->editingPointLayer();
    ui->actionEditPoints->setChecked(viewer->pointEditEnabled());
    ui->actionCancelEdit->setEnabled(editingLayer && editingLayer->editMode());
}

void MainWindow::setHistogramLayer(Layer *layer)
{
    disconnect(m_histogramStyleConnection);
    disconnect(m_histogramDataConnection);

    auto *imageLayer = qobject_cast<ImageLayer*>(layer);
    if (m_histogramDock)
        m_histogramDock->setLayer(imageLayer);

    if (!imageLayer || !m_histogramDock)
        return;

    m_histogramStyleConnection =
        connect(imageLayer, &Layer::styleChanged, this,
                [this, imageLayer]() {
                    if (m_histogramDock)
                        m_histogramDock->setLayer(imageLayer);
                });
    m_histogramDataConnection =
        connect(imageLayer, &Layer::dataChanged, this,
                [this, imageLayer]() {
                    if (m_histogramDock)
                        m_histogramDock->setLayer(imageLayer);
                });
}

void MainWindow::onPointSelectionChanged()
{
    if (!m_currentPointLayer || !ui->propertiesTable)
        return;

    const PointCollection &col = m_currentPointLayer->collection();
    const int selectionCount =
        m_currentPointLayer->selectedPointIndices().size();
    const int index = m_currentPointLayer->primarySelectedPointIndex();
    const int sectionARows = 4;
    QVector<QPair<QString, QString>> selectionSummary;
    if (selectionCount > 1) {
        const QStringList colNames = col.columnNames();
        const QSet<int> selectedIndices =
            m_currentPointLayer->selectedPointIndices();
        const int attributeCount = qMax(0, colNames.size() - 2);
        for (int attr = 0; attr < attributeCount; ++attr) {
            int validCount = 0;
            bool allNumeric = true;
            double sum = 0.0;
            double minValue = 0.0;
            double maxValue = 0.0;
            QSet<QString> distinctValues;

            for (int selectedIndex : selectedIndices) {
                if (selectedIndex < 0 || selectedIndex >= col.points().size())
                    continue;

                const QVector<QVariant> &attributes =
                    col.points().at(selectedIndex).attributes;
                if (attr >= attributes.size()
                    || !attributes.at(attr).isValid()) {
                    continue;
                }

                const QVariant value = attributes.at(attr);
                ++validCount;
                bool numericOk = false;
                const double numericValue = value.toDouble(&numericOk);
                if (numericOk) {
                    sum += numericValue;
                    if (validCount == 1) {
                        minValue = numericValue;
                        maxValue = numericValue;
                    } else {
                        minValue = qMin(minValue, numericValue);
                        maxValue = qMax(maxValue, numericValue);
                    }
                } else {
                    allNumeric = false;
                }
                distinctValues.insert(value.toString());
            }

            const QString attrName = (attr + 2 < colNames.size())
                ? colNames.at(attr + 2)
                : QStringLiteral("Attr %1").arg(attr);
            QString summary;
            if (validCount == 0) {
                summary = "all NA";
            } else if (allNumeric) {
                summary = QStringLiteral("mean %1; range %2 - %3")
                              .arg(sum / validCount, 0, 'g', 6)
                              .arg(minValue, 0, 'g', 6)
                              .arg(maxValue, 0, 'g', 6);
            } else {
                summary = QStringLiteral("%1 distinct")
                              .arg(distinctValues.size());
                if (distinctValues.size() <= 3) {
                    QStringList values = distinctValues.values();
                    values.sort(Qt::CaseInsensitive);
                    summary += QStringLiteral(": %1").arg(values.join(", "));
                }
            }
            selectionSummary.append({attrName, summary});
        }
    }
    const int sectionBRows = selectionCount == 1 && index >= 0
        ? 3 + col.points()[index].attributes.size()
        : (selectionCount > 1 ? 1 + selectionSummary.size() : 0);

    ui->propertiesTable->setRowCount(sectionARows + sectionBRows);

    // Section A — File info
    ui->propertiesTable->setItem(0, 0, new QTableWidgetItem("Type"));
    ui->propertiesTable->setItem(0, 1, new QTableWidgetItem("Point Layer"));
    ui->propertiesTable->setItem(1, 0, new QTableWidgetItem("Source"));
    ui->propertiesTable->setItem(1, 1, new QTableWidgetItem(m_currentPointLayer->sourcePath()));
    ui->propertiesTable->setItem(2, 0, new QTableWidgetItem("Records"));
    ui->propertiesTable->setItem(2, 1, new QTableWidgetItem(QString::number(col.recordCount())));
    ui->propertiesTable->setItem(3, 0, new QTableWidgetItem("Columns"));
    ui->propertiesTable->setItem(3, 1, new QTableWidgetItem(col.columnNames().join(", ")));

    // Section B — Selected point detail
    if (selectionCount > 1) {
        int row = sectionARows;
        ui->propertiesTable->setItem(
            row, 0, new QTableWidgetItem("Selected"));
        ui->propertiesTable->setItem(
            row, 1,
            new QTableWidgetItem(QString::number(selectionCount)));
        ++row;
        for (const auto &summary : std::as_const(selectionSummary)) {
            ui->propertiesTable->setItem(
                row, 0, new QTableWidgetItem(summary.first));
            ui->propertiesTable->setItem(
                row, 1, new QTableWidgetItem(summary.second));
            ++row;
        }
    } else if (selectionCount == 1
               && index >= 0 && index < col.points().size()) {
        const PointRecord &rec = col.points()[index];
        int row = sectionARows;

        ui->propertiesTable->setItem(row, 0, new QTableWidgetItem("ID"));
        ui->propertiesTable->setItem(row, 1, new QTableWidgetItem(QString::number(index + 1)));
        ++row;

        ui->propertiesTable->setItem(row, 0, new QTableWidgetItem("Row"));
        ui->propertiesTable->setItem(row, 1, new QTableWidgetItem(QString::number(rec.row)));
        ++row;

        ui->propertiesTable->setItem(row, 0, new QTableWidgetItem("Col"));
        ui->propertiesTable->setItem(row, 1, new QTableWidgetItem(QString::number(rec.col)));
        ++row;

        const QStringList colNames = col.columnNames();
        for (int i = 0; i < rec.attributes.size(); ++i) {
            const int csvColIndex = i + 2;
            const QString attrName = (csvColIndex < colNames.size())
                ? colNames[csvColIndex] : QStringLiteral("Attr %1").arg(i);
            const QString valueStr = rec.attributes[i].isValid()
                ? rec.attributes[i].toString() : "NA";
            ui->propertiesTable->setItem(row, 0, new QTableWidgetItem(attrName));
            ui->propertiesTable->setItem(row, 1, new QTableWidgetItem(valueStr));
            ++row;
        }
    }
}

void MainWindow::onEditPointsToggled(bool enabled)
{
    ImageViewer *viewer = currentImageViewer();
    if (!viewer)
        return;

    if (enabled) {
        if (ui->actionPan->isChecked())
            ui->actionPan->setChecked(false);
        if (ui->actionZoom->isChecked())
            ui->actionZoom->setChecked(false);
    }

    viewer->setPointEditEnabled(enabled);

    // Update Cancel edit button state
    PointLayer *pointLayer = viewer->editingPointLayer();
    ui->actionCancelEdit->setEnabled(pointLayer && pointLayer->editMode());
    ui->actionEditPoints->setChecked(viewer->pointEditEnabled());
    refreshReloadableArtifactWatchers();
}

void MainWindow::onCancelEditTriggered()
{
    ImageViewer *viewer = currentImageViewer();
    if (!viewer)
        return;

    PointLayer *pointLayer = viewer->editingPointLayer();
    if (!pointLayer || !pointLayer->editMode())
        return;

    pointLayer->cancelEdit();
    viewer->setPointEditEnabled(false);
    ui->actionEditPoints->setChecked(false);
    ui->actionCancelEdit->setEnabled(false);
    refreshReloadableArtifactWatchers();
}

void MainWindow::onWorkspaceTabChanged(int index)
{
    // Clean up previous point layer connection
    if (m_currentPointLayer) {
        disconnect(m_pointLayerConnection);
        disconnect(m_pointCollectionConnection);
        m_currentPointLayer = nullptr;
    }

    if (index < 0) {
        m_activeViewer = nullptr;
        onActiveViewerChanged(nullptr);
        return;
    }

    // Find the first viewer in the splitter for this tab
    auto *splitter = qobject_cast<QSplitter*>(ui->workspaceTabs->widget(index));
    if (!splitter || splitter->count() == 0) {
        onActiveViewerChanged(nullptr);
        return;
    }

    auto *viewer = qobject_cast<ImageViewer*>(splitter->widget(0));
    m_activeViewer = viewer;
    onActiveViewerChanged(viewer);
}

void MainWindow::onActiveViewerChanged(ImageViewer *viewer)
{
    if (!viewer) {
        {
            const QSignalBlocker panBlocker(ui->actionPan);
            const QSignalBlocker zoomBlocker(ui->actionZoom);
            ui->actionPan->setChecked(false);
            ui->actionZoom->setChecked(false);
        }
        ui->actionPan->setEnabled(false);
        ui->actionZoom->setEnabled(false);
        if (m_zoomPercentSpin) {
            const QSignalBlocker zoomSpinBlocker(m_zoomPercentSpin);
            m_zoomPercentSpin->setEnabled(false);
            m_zoomPercentSpin->setValue(100.0);
        }
        ui->actionSyncSplitViews->setEnabled(false);
        ui->actionSyncSplitZoom->setEnabled(false);
        ui->actionEditPoints->setChecked(false);
        ui->actionEditPoints->setEnabled(false);
        ui->actionCancelEdit->setEnabled(false);
        if (m_layersDock)
            m_layersDock->setLayerManager(nullptr);
        setHistogramLayer(nullptr);
        clearPropertiesTable();
        return;
    }

    {
        const QSignalBlocker panBlocker(ui->actionPan);
        const QSignalBlocker zoomBlocker(ui->actionZoom);
        ui->actionPan->setChecked(viewer->panModeEnabled());
        ui->actionZoom->setChecked(viewer->zoomModeEnabled());
    }
    ui->actionPan->setEnabled(true);
    ui->actionZoom->setEnabled(true);
    if (m_zoomPercentSpin) {
        const QSignalBlocker zoomSpinBlocker(m_zoomPercentSpin);
        m_zoomPercentSpin->setEnabled(true);
        m_zoomPercentSpin->setValue(viewer->zoomPercent());
    }

    // Enable sync only if the viewer is inside a QSplitter with siblings
    auto *splitter = qobject_cast<QSplitter*>(viewer->parentWidget());
    const bool hasSplitSiblings = splitter && splitter->count() > 1;
    ui->actionSyncSplitViews->setEnabled(hasSplitSiblings);
    ui->actionSyncSplitZoom->setEnabled(hasSplitSiblings);

    LayerManager *mgr = viewer->layerManager();
    Layer *active = mgr ? mgr->activeLayer() : nullptr;
    auto *pointLayer = qobject_cast<PointLayer*>(active);
    const bool isPointLayer = (pointLayer != nullptr);
    PointLayer *editingLayer = viewer->editingPointLayer();
    ui->actionEditPoints->setEnabled(isPointLayer || viewer->pointEditEnabled());
    ui->actionEditPoints->setChecked(viewer->pointEditEnabled());
    ui->actionCancelEdit->setEnabled(editingLayer && editingLayer->editMode());

    if (m_layersDock)
        m_layersDock->setLayerManager(mgr);
    setHistogramLayer(active);
}

ImageViewer *MainWindow::viewerForLayer(PointLayer *layer) const
{
    if (!layer)
        return nullptr;

    for (int tabIndex = 0;
         tabIndex < ui->workspaceTabs->count();
         ++tabIndex) {
        auto *splitter = qobject_cast<QSplitter*>(
            ui->workspaceTabs->widget(tabIndex));
        if (!splitter)
            continue;
        for (int panelIndex = 0;
             panelIndex < splitter->count();
             ++panelIndex) {
            auto *viewer = qobject_cast<ImageViewer*>(
                splitter->widget(panelIndex));
            if (viewer && viewer->layerManager()
                && viewer->layerManager()->indexOf(layer) >= 0)
                return viewer;
        }
    }
    return nullptr;
}

void MainWindow::openAttributeTable(PointLayer *layer)
{
    if (!layer)
        return;

    if (auto existing = m_attributeTables.value(layer)) {
        existing->showNormal();
        existing->raise();
        existing->activateWindow();
        return;
    }

    ImageViewer *viewer = viewerForLayer(layer);
    if (!viewer)
        return;

    auto *window = new PointAttributeTableWindow(layer, viewer, this);
    m_attributeTables.insert(layer, window);
    connect(window, &QObject::destroyed, this, [this, layer]() {
        m_attributeTables.remove(layer);
    });
    window->show();
    window->raise();
    window->activateWindow();
}

void MainWindow::closeAttributeTables()
{
    const auto windows = m_attributeTables.values();
    m_attributeTables.clear();

    for (const QPointer<PointAttributeTableWindow> &window : windows) {
        if (!window)
            continue;
        auto *rawWindow = window.data();
        rawWindow->setAttribute(Qt::WA_DeleteOnClose, false);
        rawWindow->close();
        delete rawWindow;
    }
}

void MainWindow::splitViewer()
{
    ImageViewer *viewer = currentImageViewer();
    if (!viewer)
        return;

    auto *splitter = qobject_cast<QSplitter*>(viewer->parentWidget());
    if (!splitter)
        return;

    auto *newViewer = new ImageViewer(this);
    newViewer->setFocusPolicy(Qt::StrongFocus);
    configureViewer(newViewer);

    int idx = splitter->indexOf(viewer);
    splitter->insertWidget(idx + 1, newViewer);
    newViewer->restoreView(viewer->viewTransform(), viewer->viewCenter());

    const int totalWidth = splitter->width();
    if (totalWidth > 0) {
        QList<int> sizes;
        sizes.reserve(splitter->count());
        const int baseSize = totalWidth / splitter->count();
        for (int i = 0; i < splitter->count(); ++i)
            sizes.append(baseSize);
        splitter->setSizes(sizes);
    }

    m_activeViewer = newViewer;
    onActiveViewerChanged(newViewer);
    updateProjectActions();
    newViewer->setFocus();
}

void MainWindow::openImageAtPath(const QString &filePath)
{
    const QFileInfo fi(filePath);
    const QString absolutePath = fi.absoluteFilePath();

    // Check if this image is already open in any viewer across any tab
    for (int t = 0; t < ui->workspaceTabs->count(); ++t) {
        auto *splitter = qobject_cast<QSplitter*>(ui->workspaceTabs->widget(t));
        if (!splitter) continue;
        for (int i = 0; i < splitter->count(); ++i) {
            auto *v = qobject_cast<ImageViewer*>(splitter->widget(i));
            if (v && v->primarySourcePath() == absolutePath) {
                ui->workspaceTabs->setCurrentIndex(t);
                v->setFocus();
                return;
            }
        }
    }

    auto *viewer = new ImageViewer(this);
    if (!loadImageIntoViewer(viewer, absolutePath)) {
        delete viewer;
        return;
    }

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(viewer);

    const QString tabTitle = fi.fileName();
    const int newIndex = ui->workspaceTabs->addTab(splitter, tabTitle);
    ui->workspaceTabs->setCurrentIndex(newIndex);

    m_openImageTabs.insert(absolutePath, splitter);

    if (m_layersDock)
        m_layersDock->setLayerManager(viewer->layerManager());

    configureViewer(viewer);
    m_activeViewer = viewer;
    updateProjectActions();
    updateViewerEmptyState();
}

bool MainWindow::loadImageIntoViewer(ImageViewer *viewer,
                                     const QString &filePath,
                                     bool showWarning)
{
    if (!viewer)
        return false;

    const QFileInfo fi(filePath);
    const QString absolutePath = fi.absoluteFilePath();
    const QVector<Collection> collections = m_project.isValid()
        ? Project::discoverCollections(m_project.rootPath())
        : QVector<Collection>{};
    CollectionAutoloadResolver autoloadResolver;
    QVector<AutoloadCandidate> candidates =
        autoloadResolver.resolve(
            m_project.isValid() ? m_project.rootPath() : fi.absolutePath(),
            absolutePath,
            collections);

    bool openedFileIncluded = false;
    for (const AutoloadCandidate &candidate : std::as_const(candidates)) {
        if (candidate.openedFile) {
            openedFileIncluded = true;
            break;
        }
    }

    if (!openedFileIncluded) {
        AutoloadCandidate fallback;
        fallback.collection.dirPath = fi.absolutePath();
        fallback.collection.dirName = fi.dir().dirName();
        fallback.filePath = absolutePath;
        fallback.layerKind = AutoloadLayerKind::Image;
        fallback.priority = 100;
        fallback.openedFile = true;
        candidates.prepend(fallback);
    }

    CollectionStyleResolver styleResolver;
    int loadedImageCount = 0;

    for (const AutoloadCandidate &candidate : std::as_const(candidates)) {
        Layer *layer = nullptr;
        if (candidate.layerKind == AutoloadLayerKind::Image) {
            auto *imageLayer = new ImageLayer(candidate.filePath);
            if (imageLayer->rasterData().isNull()) {
                qWarning() << "Autoload: could not load image" << candidate.filePath;
                delete imageLayer;
                continue;
            }
            imageLayer->setStyle(styleResolver.imageStyle(
                candidate.collection, imageLayer->rasterData()));
            layer = imageLayer;
            ++loadedImageCount;
        } else {
            auto *pointLayer = new PointLayer(candidate.filePath, viewer->scene());
            if (!pointLayer->collection().isValid()) {
                qWarning() << "Autoload: could not load points" << candidate.filePath;
                delete pointLayer;
                continue;
            }
            pointLayer->setStyle(styleResolver.pointStyle(
                candidate.collection, pointLayer->collection().recordCount()));
            layer = pointLayer;
        }

        if (!candidate.collection.dirName.isEmpty()) {
            layer->rename(QStringLiteral("%1/%2")
                              .arg(candidate.collection.dirName,
                                   QFileInfo(candidate.filePath).fileName()));
        }
        viewer->layerManager()->addLayer(layer);
    }

    if (loadedImageCount == 0) {
        if (showWarning) {
            QMessageBox::warning(
                this, "Open Image",
                QStringLiteral("Could not load image:\n%1").arg(absolutePath));
        }
        return false;
    }

    viewer->fitLoadedImage();
    viewer->setPrimarySourcePath(absolutePath);
    qInfo() << "Autoload loaded" << viewer->layerManager()->layerCount()
            << "layer(s) for" << fi.completeBaseName();
    return true;
}

void MainWindow::configureViewer(ImageViewer *viewer)
{
    if (!viewer)
        return;

    connect(viewer, &ImageViewer::pixelInfoChanged,
            this, &MainWindow::updatePropertiesTable);
    connect(viewer, &ImageViewer::zoomPercentChanged,
            this, [this, viewer](double percent) {
                if (viewer != currentImageViewer() || !m_zoomPercentSpin)
                    return;
                const QSignalBlocker blocker(m_zoomPercentSpin);
                m_zoomPercentSpin->setValue(percent);
            });
    connect(viewer->layerManager(), &LayerManager::layersChanged,
            this, &MainWindow::refreshReloadableArtifactWatchers);
    connect(viewer, &ImageViewer::activatedByClick, this, [this, viewer]() {
        if (m_activeViewer == viewer)
            return;

        m_activeViewer = viewer;
        onActiveViewerChanged(viewer);
    });
    connect(viewer, &ImageViewer::pointLayerFileDropped,
            this, [this, viewer](const QString &path) {
                if (!viewer || !viewer->layerManager())
                    return;
                if (PointLayer *layer = createStyledPointLayer(path, viewer))
                    viewer->layerManager()->addLayer(layer);
            });
    connect(viewer, &ImageViewer::imageLayerFileDropped,
            this, [this, viewer](const QString &path) {
                if (!viewer || !viewer->layerManager())
                    return;
                auto *layer = createStyledImageLayer(path);
                if (!layer)
                    return;
                viewer->layerManager()->addLayer(layer);
                if (viewer->primarySourcePath().isEmpty())
                    viewer->setPrimarySourcePath(
                        QFileInfo(path).absoluteFilePath());
            });
    connect(viewer, &ImageViewer::closeRequested, this, [this, viewer]() {
        auto *v = qobject_cast<ImageViewer*>(sender());
        if (!v || v != viewer)
            return;

        auto *splitter = qobject_cast<QSplitter*>(v->parentWidget());
        if (!splitter)
            return;

        if (splitter->count() <= 1) {
            QPointer<QWidget> tabWidget = splitter;
            for (int t = 0; t < ui->workspaceTabs->count(); ++t) {
                if (ui->workspaceTabs->widget(t) == splitter) {
                    // The request originates inside ImageViewer::contextMenuEvent().
                    // Closing synchronously would destroy that viewer before its
                    // event handler has returned.
                    QTimer::singleShot(0, this, [this, tabWidget]() {
                        if (!tabWidget)
                            return;

                        const int tabIndex = ui->workspaceTabs->indexOf(tabWidget);
                        if (tabIndex >= 0)
                            onWorkspaceTabCloseRequested(tabIndex);
                    });
                    return;
                }
            }
            return;
        }

        v->hide();
        v->setParent(nullptr);
        v->deleteLater();
        refreshReloadableArtifactWatchers();

        if (m_activeViewer == v) {
            auto *remainingViewer = qobject_cast<ImageViewer*>(splitter->widget(0));
            m_activeViewer = remainingViewer;
            onActiveViewerChanged(remainingViewer);
            if (remainingViewer)
                remainingViewer->setFocus();
        }
    });
    connect(viewer, &ImageViewer::reloadRequested,
            this, [this, viewer]() {
                auto *v = qobject_cast<ImageViewer*>(sender());
                if (!v || v != viewer)
                    return;
                reloadViewerLayers(v);
            });
    refreshReloadableArtifactWatchers();
}

QModelIndex MainWindow::projectSourceIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    if (index.model() == m_projectFileSystemModel)
        return index;
    if (index.model() == m_projectSortProxy)
        return m_projectSortProxy->mapToSource(index);
    return {};
}

QModelIndex MainWindow::projectProxyIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    if (index.model() == m_projectSortProxy)
        return index;
    if (index.model() == m_projectFileSystemModel)
        return m_projectSortProxy->mapFromSource(index);
    return {};
}

QString MainWindow::projectPathFromIndex(const QModelIndex &index) const
{
    const QModelIndex sourceIndex = projectSourceIndex(index);
    return sourceIndex.isValid()
        ? m_projectFileSystemModel->filePath(sourceIndex)
        : QString();
}

QList<Layer*> MainWindow::reloadableLayersForPath(const QString &path) const
{
    const QString target = normalizedArtifactPath(path);
    QList<Layer*> result;
    if (target.isEmpty())
        return result;

    for (int tabIndex = 0; tabIndex < ui->workspaceTabs->count(); ++tabIndex) {
        auto *splitter =
            qobject_cast<QSplitter*>(ui->workspaceTabs->widget(tabIndex));
        if (!splitter)
            continue;

        for (int panelIndex = 0; panelIndex < splitter->count(); ++panelIndex) {
            auto *viewer =
                qobject_cast<ImageViewer*>(splitter->widget(panelIndex));
            if (!viewer || !viewer->layerManager())
                continue;

            LayerManager *manager = viewer->layerManager();
            for (int layerIndex = 0; layerIndex < manager->layerCount(); ++layerIndex) {
                Layer *layer = manager->layerAt(layerIndex);
                if (!layer || !layer->supportsReloadFromDisk())
                    continue;
                if (normalizedArtifactPath(layer->sourcePath()) == target)
                    result.append(layer);
            }
        }
    }

    return result;
}

void MainWindow::reloadViewerLayers(ImageViewer *viewer)
{
    if (!viewer || !viewer->layerManager())
        return;

    LayerManager *manager = viewer->layerManager();
    int attempted = 0;
    int reloaded = 0;
    QStringList failures;

    for (int layerIndex = 0; layerIndex < manager->layerCount(); ++layerIndex) {
        Layer *layer = manager->layerAt(layerIndex);
        if (!layer || !layer->supportsReloadFromDisk())
            continue;

        ++attempted;
        QString errorMessage;
        if (layer->reloadFromDisk(&errorMessage)) {
            ++reloaded;
        } else {
            failures.append(QStringLiteral("%1: %2")
                                .arg(QFileInfo(layer->sourcePath()).fileName(),
                                     errorMessage));
        }
    }

    if (attempted == 0) {
        statusBar()->showMessage("No reloadable layers in this panel", 3000);
        return;
    }

    if (QGraphicsScene *scene = viewer->scene()) {
        const QRectF bounds = scene->itemsBoundingRect();
        scene->setSceneRect(bounds);
    }

    refreshReloadableArtifactWatchers();
    if (m_activeViewer == viewer)
        onActiveViewerChanged(viewer);

    if (failures.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("Reloaded %1 layer(s)").arg(reloaded), 3000);
    } else {
        statusBar()->showMessage(
            QStringLiteral("Reloaded %1 of %2 layer(s); %3 failed")
                .arg(reloaded)
                .arg(attempted)
                .arg(failures.size()),
            5000);
        qWarning() << "Manual panel reload failures:" << failures;
    }
}

void MainWindow::refreshReloadableArtifactWatchers()
{
    if (!m_artifactWatcher)
        return;

    QHash<QString, QString> desiredPaths;
    for (int tabIndex = 0; tabIndex < ui->workspaceTabs->count(); ++tabIndex) {
        auto *splitter =
            qobject_cast<QSplitter*>(ui->workspaceTabs->widget(tabIndex));
        if (!splitter)
            continue;

        for (int panelIndex = 0; panelIndex < splitter->count(); ++panelIndex) {
            auto *viewer =
                qobject_cast<ImageViewer*>(splitter->widget(panelIndex));
            if (!viewer || !viewer->layerManager())
                continue;

            LayerManager *manager = viewer->layerManager();
            for (int layerIndex = 0; layerIndex < manager->layerCount(); ++layerIndex) {
                Layer *layer = manager->layerAt(layerIndex);
                if (!layer || !layer->supportsReloadFromDisk())
                    continue;

                const QString path = QFileInfo(layer->sourcePath()).absoluteFilePath();
                if (QFileInfo::exists(path))
                    desiredPaths.insert(normalizedArtifactPath(path), path);
            }
        }
    }

    const QStringList watched = m_artifactWatcher->files();
    for (const QString &path : watched) {
        if (!desiredPaths.contains(normalizedArtifactPath(path)))
            m_artifactWatcher->removePath(path);
    }

    QSet<QString> watchedKeys;
    for (const QString &path : m_artifactWatcher->files())
        watchedKeys.insert(normalizedArtifactPath(path));

    for (auto it = desiredPaths.cbegin(); it != desiredPaths.cend(); ++it) {
        if (!watchedKeys.contains(it.key()))
            m_artifactWatcher->addPath(it.value());
    }
}

void MainWindow::scheduleArtifactReload(const QString &path, int delayMs)
{
    const QString key = normalizedArtifactPath(path);
    if (key.isEmpty())
        return;

    if (!m_pendingReloadAttempts.contains(key))
        m_pendingReloadAttempts.insert(key, 0);

    if (m_scheduledArtifactReloads.contains(key))
        return;
    m_scheduledArtifactReloads.insert(key);

    QTimer::singleShot(delayMs, this, [this, key]() {
        m_scheduledArtifactReloads.remove(key);
        reloadArtifactFromDisk(key);
    });
}

void MainWindow::reloadArtifactFromDisk(const QString &path)
{
    const QString key = normalizedArtifactPath(path);
    if (key.isEmpty())
        return;

    QList<Layer*> layers = reloadableLayersForPath(key);
    if (layers.isEmpty()) {
        m_pendingReloadAttempts.remove(key);
        refreshReloadableArtifactWatchers();
        return;
    }

    const QString sourcePath = layers.first()->sourcePath();
    const QFileInfo info(sourcePath);
    const bool pointsArtifact = qobject_cast<PointLayer*>(layers.first()) != nullptr;
    const QString artifactKind = pointsArtifact
        ? QStringLiteral("points")
        : QStringLiteral("image");
    QString errorMessage;
    bool ok = info.exists() && info.isFile();
    if (ok) {
        for (Layer *layer : std::as_const(layers)) {
            if (!layer->reloadFromDisk(&errorMessage)) {
                ok = false;
                break;
            }
        }
    } else {
        errorMessage = QStringLiteral("File is not available yet.");
    }

    if (!ok) {
        const int attempts = m_pendingReloadAttempts.value(key, 0) + 1;
        m_pendingReloadAttempts.insert(key, attempts);
        if (attempts <= 8) {
            statusBar()->showMessage(
                QStringLiteral("Waiting for stable %1: %2")
                    .arg(artifactKind, info.fileName()),
                1500);
            scheduleArtifactReload(sourcePath, 500);
            refreshReloadableArtifactWatchers();
            return;
        }

        m_pendingReloadAttempts.remove(key);
        statusBar()->showMessage(
            QStringLiteral("Could not reload %1: %2")
                .arg(artifactKind, info.fileName()),
            5000);
        qWarning() << "Hot reload failed for" << sourcePath << errorMessage;
        refreshReloadableArtifactWatchers();
        return;
    }

    m_pendingReloadAttempts.remove(key);
    refreshReloadableArtifactWatchers();
    statusBar()->showMessage(
        QStringLiteral("Reloaded %1: %2").arg(artifactKind, info.fileName()),
        3000);

    if (m_activeViewer)
        onActiveViewerChanged(m_activeViewer);
}

void MainWindow::loadProject(const Project &project)
{
    // Only the folder containing project.yaml identifies a project. Its parent
    // folder may contain many sibling projects and must not be treated as open.
    if (m_project.isValid()
        && sameProjectRoot(m_project.rootPath(), project.rootPath())) {
        m_project = project;
        updateProjectExplorer();
        updateProjectActions();
        setWindowTitle(QString("%1 - rcaiman-projects").arg(m_project.name()));
        if (m_layersDock)
            m_layersDock->setProjectPath(m_project.rootPath());
        if (m_previewDock)
            m_previewDock->show();
        addToRecentProjects(m_project.rootPath());
        updateViewerEmptyState();
        statusBar()->showMessage("Project is already open", 3000);
        return;
    }

    closeCurrentProject();

    m_project = project;
    updateProjectExplorer();
    updateProjectActions();
    setWindowTitle(QString("%1 - rcaiman-projects").arg(m_project.name()));
    if (m_layersDock)
        m_layersDock->setProjectPath(m_project.rootPath());
    if (m_previewDock)
        m_previewDock->show();
    addToRecentProjects(m_project.rootPath());
    restoreProjectSession();
    updateViewerEmptyState();
}

void MainWindow::closeCurrentProject()
{
    saveProjectSession();

    closeAttributeTables();

    m_pendingReloadAttempts.clear();
    m_scheduledArtifactReloads.clear();
    if (m_artifactWatcher)
        m_artifactWatcher->removePaths(m_artifactWatcher->files());

    if (m_currentPointLayer) {
        disconnect(m_pointLayerConnection);
        disconnect(m_pointCollectionConnection);
        m_currentPointLayer = nullptr;
    }

    setHistogramLayer(nullptr);
    if (m_layersDock) {
        m_layersDock->setLayerManager(nullptr);
        m_layersDock->setProjectPath(QString());
    }
    if (m_previewDock) {
        m_previewDock->clear();
        m_previewDock->hide();
    }
    clearPropertiesTable();

    {
        const QSignalBlocker panBlocker(ui->actionPan);
        const QSignalBlocker zoomBlocker(ui->actionZoom);
        const QSignalBlocker editBlocker(ui->actionEditPoints);
        ui->actionPan->setChecked(false);
        ui->actionZoom->setChecked(false);
        ui->actionEditPoints->setChecked(false);
    }
    ui->actionPan->setEnabled(false);
    ui->actionZoom->setEnabled(false);
    if (m_zoomPercentSpin) {
        const QSignalBlocker zoomSpinBlocker(m_zoomPercentSpin);
        m_zoomPercentSpin->setEnabled(false);
        m_zoomPercentSpin->setValue(100.0);
    }
    ui->actionSyncSplitViews->setEnabled(false);
    ui->actionSyncSplitZoom->setEnabled(false);
    ui->actionEditPoints->setEnabled(false);
    ui->actionCancelEdit->setEnabled(false);

    m_activeViewer.clear();

    {
        const QSignalBlocker blocker(ui->workspaceTabs);
        while (ui->workspaceTabs->count() > 0) {
            QWidget *tab = ui->workspaceTabs->widget(0);
            ui->workspaceTabs->removeTab(0);
            delete tab;
        }
    }

    m_openImageTabs.clear();

    m_project = Project();
    m_projectFileSystemModel->setProjectRoot(QString());
    ui->projectTree->setRootIndex(projectProxyIndex(
        m_projectFileSystemModel->setRootPath(QString())));
    setWindowTitle("rcaiman-projects");
    updateProjectActions();
    updateViewerEmptyState();
}

void MainWindow::saveApplicationState()
{
    QSettings settings;
    settings.setValue("mainWindow/geometry", saveGeometry());
    settings.setValue("mainWindow/state", saveState(1));
}

void MainWindow::restoreApplicationState()
{
    QSettings settings;
    const QByteArray geometry =
        settings.value("mainWindow/geometry").toByteArray();
    const QByteArray state =
        settings.value("mainWindow/state").toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    if (!state.isEmpty())
        restoreState(state, 1);
}

void MainWindow::resetWindowLayout()
{
    if (m_resetLayoutPending)
        return;

    m_resetLayoutPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_resetLayoutPending = false;

        if (m_defaultWindowState.isEmpty()
            || !restoreState(m_defaultWindowState, 1)) {
            statusBar()->showMessage(
                "Window layout could not be reset", 4000);
            return;
        }

        if (m_previewDock)
            m_previewDock->setVisible(m_project.isValid());

        QSettings settings;
        settings.setValue("mainWindow/state", m_defaultWindowState);
        statusBar()->showMessage("Window layout reset", 3000);
    });
}

QString MainWindow::projectRelativePath(const QString &absolutePath) const
{
    if (!m_project.isValid() || absolutePath.isEmpty())
        return {};
    return QDir::fromNativeSeparators(
        QDir(m_project.rootPath()).relativeFilePath(absolutePath));
}

QString MainWindow::projectAbsolutePath(const QString &relativePath) const
{
    if (!m_project.isValid() || relativePath.isEmpty())
        return {};
    return QFileInfo(
        QDir(m_project.rootPath()).filePath(relativePath)).absoluteFilePath();
}

void MainWindow::saveProjectSession()
{
    if (!m_project.isValid())
        return;

    QJsonArray tabs;
    for (int tabIndex = 0; tabIndex < ui->workspaceTabs->count(); ++tabIndex) {
        auto *splitter =
            qobject_cast<QSplitter*>(ui->workspaceTabs->widget(tabIndex));
        if (!splitter)
            continue;

        QJsonArray panels;
        int activePanel = 0;
        for (int panelIndex = 0; panelIndex < splitter->count(); ++panelIndex) {
            auto *viewer =
                qobject_cast<ImageViewer*>(splitter->widget(panelIndex));
            if (!viewer || viewer->primarySourcePath().isEmpty())
                continue;

            const QString source = projectRelativePath(
                viewer->primarySourcePath());
            if (source.isEmpty())
                continue;

            const QTransform transform = viewer->viewTransform();
            QJsonArray transformValues{
                transform.m11(), transform.m12(), transform.m13(),
                transform.m21(), transform.m22(), transform.m23(),
                transform.m31(), transform.m32(), transform.m33()
            };
            const QPointF center = viewer->viewCenter();

            QJsonObject panel;
            panel["source"] = source;
            panel["transform"] = transformValues;
            panel["center"] = QJsonArray{center.x(), center.y()};

            QJsonArray layers;
            LayerManager *manager = viewer->layerManager();
            for (int layerIndex = 0;
                 manager && layerIndex < manager->layerCount();
                 ++layerIndex) {
                Layer *layer = manager->layerAt(layerIndex);
                if (!layer)
                    continue;

                const QString layerSource =
                    projectRelativePath(layer->sourcePath());
                if (layerSource.isEmpty())
                    continue;

                QJsonObject layerObject;
                layerObject["source"] = layerSource;
                layerObject["name"] = layer->name();
                layerObject["visible"] = layer->isVisible();
                if (auto *imageLayer = qobject_cast<ImageLayer*>(layer)) {
                    layerObject["type"] = "image";
                    layerObject["image_style"] =
                        imageStyleToJson(imageLayer->style());
                } else if (auto *pointLayer =
                               qobject_cast<PointLayer*>(layer)) {
                    layerObject["type"] = "points";
                    layerObject["point_style"] =
                        pointStyleToJson(pointLayer->style());
                }
                layers.append(layerObject);
            }
            panel["layers"] = layers;
            panels.append(panel);

            if (viewer == m_activeViewer)
                activePanel = panels.size() - 1;
        }

        if (panels.isEmpty())
            continue;

        QJsonArray sizes;
        for (int size : splitter->sizes())
            sizes.append(size);

        QJsonObject tab;
        tab["title"] = ui->workspaceTabs->tabText(tabIndex);
        tab["panels"] = panels;
        tab["split_sizes"] = sizes;
        tab["active_panel"] = activePanel;
        tabs.append(tab);
    }

    QJsonObject session;
    session["schema_version"] = 2;
    session["current_tab"] = ui->workspaceTabs->currentIndex();
    session["sync_split_views"] = ImageViewer::syncEnabled();
    session["sync_split_zoom"] = ImageViewer::syncZoomEnabled();
    session["tabs"] = tabs;

    const QString sessionDirectory =
        QDir(m_project.rootPath()).filePath(".rcaiman");
    if (!QDir().mkpath(sessionDirectory))
        return;

    QSaveFile file(QDir(sessionDirectory).filePath("session.json"));
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(session).toJson(QJsonDocument::Indented));
    file.commit();
}

void MainWindow::restoreProjectSession()
{
    if (!m_project.isValid())
        return;

    QFile file(QDir(m_project.rootPath()).filePath(".rcaiman/session.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        qWarning() << "Session restore skipped:" << parseError.errorString();
        return;
    }

    const QJsonObject session = document.object();
    const int schemaVersion =
        session.value("schema_version").toInt();
    if (schemaVersion < 1 || schemaVersion > 2)
        return;

    const QJsonArray tabs = session.value("tabs").toArray();
    int restoredCurrentTab = session.value("current_tab").toInt(0);
    ImageViewer *requestedActiveViewer = nullptr;

    for (const QJsonValue &tabValue : tabs) {
        const QJsonObject tabObject = tabValue.toObject();
        const QJsonArray panels = tabObject.value("panels").toArray();
        auto *splitter = new QSplitter(Qt::Horizontal);
        int restoredPanels = 0;
        const int activePanel =
            tabObject.value("active_panel").toInt(0);

        for (int panelIndex = 0; panelIndex < panels.size(); ++panelIndex) {
            const QJsonObject panelObject = panels.at(panelIndex).toObject();
            const QString absolutePath = projectAbsolutePath(
                panelObject.value("source").toString());
            if (!QFileInfo::exists(absolutePath))
                continue;

            auto *viewer = new ImageViewer(this);
            const QJsonArray savedLayers =
                panelObject.value("layers").toArray();
            LayerManager *manager = viewer->layerManager();
            int loadedImageCount = 0;

            // LayerManager inserts at the top. Restoring in reverse preserves
            // the exact top-to-bottom order recorded in the session.
            for (int layerIndex = savedLayers.size() - 1;
                 layerIndex >= 0;
                 --layerIndex) {
                const QJsonObject layerObject =
                    savedLayers.at(layerIndex).toObject();
                const QString sourcePath = projectAbsolutePath(
                    layerObject.value("source").toString());
                if (!QFileInfo::exists(sourcePath))
                    continue;

                QString type = layerObject.value("type").toString();
                if (type.isEmpty()) {
                    type = QFileInfo(sourcePath).suffix().compare(
                               "csv", Qt::CaseInsensitive) == 0
                        ? QStringLiteral("points")
                        : QStringLiteral("image");
                }

                Layer *layer = nullptr;
                if (type == QStringLiteral("points")) {
                    auto *pointLayer =
                        new PointLayer(sourcePath, viewer->scene());
                    if (!pointLayer->collection().isValid()) {
                        delete pointLayer;
                        continue;
                    }
                    const QJsonObject styleObject =
                        layerObject.value("point_style").toObject();
                    if (!styleObject.isEmpty()) {
                        pointLayer->setStyle(pointStyleFromJson(
                            styleObject, pointLayer->style()));
                    }
                    layer = pointLayer;
                } else {
                    auto *imageLayer = new ImageLayer(sourcePath);
                    if (imageLayer->rasterData().isNull()) {
                        delete imageLayer;
                        continue;
                    }
                    const QJsonObject styleObject =
                        layerObject.value("image_style").toObject();
                    if (!styleObject.isEmpty()) {
                        imageLayer->setStyle(imageStyleFromJson(
                            styleObject, imageLayer->style()));
                    }
                    layer = imageLayer;
                    ++loadedImageCount;
                }

                const QString savedName =
                    layerObject.value("name").toString();
                if (!savedName.isEmpty())
                    layer->rename(savedName);
                layer->setVisible(
                    layerObject.value("visible").toBool(layer->isVisible()));
                manager->addLayer(layer);
            }

            // Very early session files may not contain a layer inventory.
            // Keep their previous behavior without involving autoload for
            // sessions that do contain explicit layers.
            if (savedLayers.isEmpty()
                && loadImageIntoViewer(viewer, absolutePath, false)) {
                loadedImageCount = 1;
            }

            if (loadedImageCount == 0) {
                delete viewer;
                continue;
            }

            viewer->fitLoadedImage();
            viewer->setPrimarySourcePath(absolutePath);
            configureViewer(viewer);
            splitter->addWidget(viewer);

            const QJsonArray transformValues =
                panelObject.value("transform").toArray();
            const QJsonArray centerValues =
                panelObject.value("center").toArray();
            if (transformValues.size() == 9 && centerValues.size() == 2) {
                const QTransform transform(
                    transformValues[0].toDouble(),
                    transformValues[1].toDouble(),
                    transformValues[2].toDouble(),
                    transformValues[3].toDouble(),
                    transformValues[4].toDouble(),
                    transformValues[5].toDouble(),
                    transformValues[6].toDouble(),
                    transformValues[7].toDouble(),
                    transformValues[8].toDouble());
                const QPointF center(
                    centerValues[0].toDouble(),
                    centerValues[1].toDouble());
                QPointer<ImageViewer> guardedViewer = viewer;
                QTimer::singleShot(0, this, [guardedViewer, transform, center]() {
                    if (guardedViewer)
                        guardedViewer->restoreView(transform, center);
                });
            }

            if (panelIndex == activePanel)
                requestedActiveViewer = viewer;
            ++restoredPanels;
        }

        if (restoredPanels == 0) {
            delete splitter;
            continue;
        }

        const QJsonArray savedSizes =
            tabObject.value("split_sizes").toArray();
        QList<int> sizes;
        for (const QJsonValue &size : savedSizes)
            sizes.append(size.toInt());
        if (sizes.size() == splitter->count())
            splitter->setSizes(sizes);

        QString title = tabObject.value("title").toString();
        if (title.isEmpty()) {
            auto *viewer =
                qobject_cast<ImageViewer*>(splitter->widget(0));
            title = viewer && !viewer->primarySourcePath().isEmpty()
                ? QFileInfo(viewer->primarySourcePath()).fileName()
                : QStringLiteral("Viewer");
        }
        ui->workspaceTabs->addTab(splitter, title);

        auto *firstViewer =
            qobject_cast<ImageViewer*>(splitter->widget(0));
        if (firstViewer && !firstViewer->primarySourcePath().isEmpty()) {
            m_openImageTabs.insert(
                firstViewer->primarySourcePath(), splitter);
        }
    }

    if (ui->workspaceTabs->count() == 0) {
        updateViewerEmptyState();
        return;
    }

    restoredCurrentTab = qBound(
        0, restoredCurrentTab, ui->workspaceTabs->count() - 1);
    ui->workspaceTabs->setCurrentIndex(restoredCurrentTab);

    if (!requestedActiveViewer
        || ui->workspaceTabs->indexOf(
            requestedActiveViewer->parentWidget()) != restoredCurrentTab) {
        auto *splitter = qobject_cast<QSplitter*>(
            ui->workspaceTabs->widget(restoredCurrentTab));
        requestedActiveViewer = splitter
            ? qobject_cast<ImageViewer*>(splitter->widget(0))
            : nullptr;
    }

    m_activeViewer = requestedActiveViewer;
    onActiveViewerChanged(requestedActiveViewer);
    if (requestedActiveViewer)
        requestedActiveViewer->setFocus();

    const bool sync =
        session.value("sync_split_views").toBool(false);
    ui->actionSyncSplitViews->setChecked(sync);
    ImageViewer::setSyncEnabled(sync);
    const bool syncZoom =
        session.value("sync_split_zoom").toBool(true);
    ui->actionSyncSplitZoom->setChecked(syncZoom);
    ImageViewer::setSyncZoomEnabled(syncZoom);
    updateProjectActions();
    updateViewerEmptyState();
    refreshReloadableArtifactWatchers();
}

void MainWindow::updateProjectExplorer()
{
    if (!m_project.isValid()) {
        ui->projectTree->setRootIndex(QModelIndex());
        return;
    }
    m_projectFileSystemModel->setProjectRoot(m_project.rootPath());
    const QModelIndex rootIndex = projectProxyIndex(
        m_projectFileSystemModel->setRootPath(m_project.rootPath()));
    ui->projectTree->setRootIndex(rootIndex);
    ui->projectTree->setColumnWidth(0, 220);
}

void MainWindow::updateProjectActions()
{
    const bool hasProject = m_project.isValid();
    ui->actionSaveProject->setEnabled(hasProject);
    ui->actionSaveProjectAs->setEnabled(hasProject);
    ui->actionSaveProjectAsCleanCopy->setEnabled(hasProject);
    ui->actionProjectReport->setEnabled(hasProject);
    ui->actionCloseProject->setEnabled(hasProject);
    ImageViewer *viewer = currentImageViewer();
    ui->actionPan->setEnabled(viewer != nullptr);
    ui->actionZoom->setEnabled(viewer != nullptr);
    auto *splitter = viewer
        ? qobject_cast<QSplitter*>(viewer->parentWidget())
        : nullptr;
    const bool hasSplitSiblings = splitter && splitter->count() > 1;
    ui->actionSyncSplitViews->setEnabled(hasSplitSiblings);
    ui->actionSyncSplitZoom->setEnabled(hasSplitSiblings);
    ui->actionImportRaw->setEnabled(hasProject);
    ui->actionRegisterCollection->setEnabled(hasProject);
    ui->actionCreatePoints->setEnabled(hasProject);
    ui->actionBuildMetadataRules->setEnabled(hasProject);
    ui->actionValidateImageMetadata->setEnabled(hasProject);
    ui->actionValidateCollectionMetadata->setEnabled(hasProject);
    if (m_newFolderButton)
        m_newFolderButton->setEnabled(hasProject);

    bool hasRawFiles = false;
    if (hasProject) {
        const QString rawDir = QDir(m_project.rootPath()).filePath("raw_files");
        const QStringList rawExtensions = {
            "cr2", "cr3", "nef", "arw", "dng", "raf", "orf",
            "rw2", "pef", "srw", "raw", "dcr", "kdc"
        };
        QDirIterator it(rawDir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (rawExtensions.contains(
                    QFileInfo(it.next()).suffix().toLower())) {
                hasRawFiles = true;
                break;
            }
        }
    }
    ui->actionExtractRawData->setEnabled(hasRawFiles);
}

void MainWindow::buildMetadataRules()
{
    if (!m_project.isValid())
        return;

    QString errorMessage;
    if (!ExifToolService::isAvailable(&errorMessage)) {
        QMessageBox::critical(this, "ExifTool", errorMessage);
        return;
    }

    const QString startDirectory =
        QDir(m_project.rootPath()).filePath("raw_files");
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        "Select Calibration Images",
        QFileInfo::exists(startDirectory)
            ? startDirectory : m_project.rootPath(),
        "Images (*.nef *.NEF *.cr2 *.CR2 *.cr3 *.CR3 *.arw *.ARW "
        "*.dng *.DNG *.raf *.RAF *.orf *.ORF *.rw2 *.RW2 "
        "*.tif *.tiff *.TIF *.TIFF *.jpg *.jpeg *.JPG *.JPEG);;"
        "All files (*)");
    if (files.isEmpty())
        return;
    for (const QString &file : files) {
        if (!pathBelongsToProject(m_project.rootPath(), file)) {
            QMessageBox::warning(
                this, "Portable Metadata Draft",
                "Calibration images must be inside the current project so "
                "the draft remains portable.");
            return;
        }
    }

    if (files.size() == 1
        && QMessageBox::question(
               this, "Single Calibration Image",
               "Using several calibration images makes constant metadata "
               "easier to recognize. Continue with one image?",
               QMessageBox::Yes | QMessageBox::No,
               QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QList<MetadataObservation> observations;
    const bool extracted =
        ExifToolService::extract(files, &observations, &errorMessage);
    const QString version = extracted
        ? ExifToolService::version(&errorMessage) : QString();
    QApplication::restoreOverrideCursor();
    if (!extracted || version.isEmpty()) {
        QMessageBox::critical(this, "Metadata Extraction", errorMessage);
        return;
    }

    MetadataRulesDialog dialog(
        MetadataRegistry::analyze(files, observations), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString draftPath =
        QDir(m_project.rootPath()).filePath(
            "registry_drafts/" + dialog.draftId() + ".yaml");
    const QString snippetPath =
        QFileInfo(draftPath).absolutePath() + "/"
        + QFileInfo(draftPath).completeBaseName() + "_snippet.R";
    if ((QFileInfo::exists(draftPath) || QFileInfo::exists(snippetPath))
        && QMessageBox::question(
            this, "Replace Draft",
            "A draft or R snippet with this ID already exists. "
            "Replace both artifacts?",
               QMessageBox::Yes | QMessageBox::No,
               QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    if (!MetadataRegistry::writeDraft(
            draftPath, m_project.rootPath(), files,
            dialog.selectedRules(), version, &errorMessage)) {
        QMessageBox::critical(this, "Save Metadata Rules", errorMessage);
        return;
    }
    updateProjectExplorer();
    QMessageBox::information(
        this, "Metadata Rules",
        QString("Draft and R snippet saved to:\n%1\n%2")
            .arg(QDir::toNativeSeparators(draftPath),
                 QDir::toNativeSeparators(snippetPath)));
}

void MainWindow::validateImageMetadata()
{
    if (!m_project.isValid())
        return;

    QString errorMessage;
    if (!ExifToolService::isAvailable(&errorMessage)) {
        QMessageBox::critical(this, "ExifTool", errorMessage);
        return;
    }

    QString initialPath = selectedProjectPath();
    if (!QFileInfo(initialPath).exists())
        initialPath = m_project.rootPath();
    const QString imagePath = QFileDialog::getOpenFileName(
        this, "Select Image", initialPath,
        "Images (*.nef *.NEF *.cr2 *.CR2 *.cr3 *.CR3 *.arw *.ARW "
        "*.dng *.DNG *.raf *.RAF *.orf *.ORF *.rw2 *.RW2 "
        "*.tif *.tiff *.TIF *.TIFF *.jpg *.jpeg *.JPG *.JPEG);;"
        "All files (*)");
    if (imagePath.isEmpty())
        return;
    if (!pathBelongsToProject(m_project.rootPath(), imagePath)) {
        QMessageBox::warning(
            this, "Portable Registry Review",
            "The image must be inside the current project.");
        return;
    }

    const Collection collection = collectionForPath(imagePath);
    if (CollectionValidation::isEligible(
            m_project.rootPath(), collection)) {
        const QList<CollectionFileValidation> pairs =
            CollectionValidation::resolveRawPairs(
                m_project.rootPath(), collection);
        for (const CollectionFileValidation &pair : pairs) {
            if (!samePath(pair.preprocessedPath, imagePath)) {
                continue;
            }
            if (pair.status
                == CollectionFileValidationStatus::MissingRaw) {
                QMessageBox::warning(
                    this, "Validate Image",
                    QString(
                        "No RAW with basename \"%1\" was found in raw_files.")
                        .arg(pair.baseName));
                return;
            }
            if (pair.status
                == CollectionFileValidationStatus::AmbiguousRaw) {
                QMessageBox::warning(
                    this, "Validate Image",
                    QString(
                        "More than one RAW with basename \"%1\" was found:\n%2")
                        .arg(pair.baseName,
                             pair.rawCandidates.join("\n")));
                return;
            }
            validateRawImage(
                pair.rawPath,
                [&]() {
                    QString registryPath;
                    CollectionValidation::resolveRegistry(
                        m_project.rootPath(),
                        collection, &registryPath);
                    return registryPath;
                }());
            return;
        }
    }
    validateRawImage(imagePath);
}

bool MainWindow::validateRawImage(
    const QString &rawPath,
    const QString &providedRegistryPath)
{
    QString errorMessage;
    if (!ExifToolService::isAvailable(&errorMessage)) {
        QMessageBox::critical(this, "ExifTool", errorMessage);
        return false;
    }
    QString registryPath = providedRegistryPath;
    if (registryPath.isEmpty()) {
        const QString rulesStart =
            QDir(m_project.rootPath()).filePath("registry_drafts");
        registryPath = QFileDialog::getOpenFileName(
            this, "Select Registry Snapshot or Rules Draft", rulesStart,
            "YAML files (*.yaml *.yml)");
    }
    if (registryPath.isEmpty())
        return false;
    if (!pathBelongsToProject(m_project.rootPath(), registryPath)) {
        QMessageBox::warning(
            this, "Portable Registry Review",
            "The snapshot or draft must be inside the current project.");
        return false;
    }

    QList<MetadataRule> rules;
    if (!MetadataRegistry::readRules(
            registryPath, &rules, &errorMessage)) {
        QMessageBox::critical(this, "Registry Rules", errorMessage);
        return false;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QList<MetadataObservation> observations;
    const bool extracted = ExifToolService::extract(
        {rawPath}, &observations, &errorMessage);
    const QString version = extracted
        ? ExifToolService::version(&errorMessage) : QString();
    QApplication::restoreOverrideCursor();
    if (!extracted || version.isEmpty()) {
        QMessageBox::critical(this, "Metadata Extraction", errorMessage);
        return false;
    }

    RegistryValidationDialog dialog(rules, observations, this);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    const QString stem =
        MetadataRegistry::snakeCase(QFileInfo(rawPath).completeBaseName());
    const QString timestamp =
        QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss");
    const QString reviewPath =
        QDir(m_project.rootPath()).filePath(
            QString("registry_reviews/%1_%2.yaml").arg(stem, timestamp));
    if (!MetadataRegistry::writeReview(
            reviewPath, m_project.rootPath(), rawPath, registryPath,
            version, dialog.decisions(), dialog.acceptedDecision(),
            &errorMessage)) {
        QMessageBox::critical(this, "Save Registry Review", errorMessage);
        return false;
    }
    updateProjectExplorer();
    QMessageBox::information(
        this, "Registry Review",
        QString("%1 review saved to:\n%2")
            .arg(dialog.acceptedDecision() ? "Accepted" : "Rejected",
                 QDir::toNativeSeparators(reviewPath)));
    return true;
}

void MainWindow::validateCollectionMetadata()
{
    if (!m_project.isValid())
        return;

    const QVector<Collection> collections =
        Project::discoverCollections(m_project.rootPath());
    QVector<Collection> eligible;
    for (const Collection &collection : collections) {
        if (CollectionValidation::isEligible(
                m_project.rootPath(), collection)) {
            eligible.append(collection);
        }
    }
    if (eligible.isEmpty()) {
        QStringList diagnostics;
        for (const Collection &collection : collections) {
            if (collection.collectionClass
                != "preprocessed_image_collection") {
                continue;
            }
            QString registryPath;
            QString reason;
            CollectionValidation::resolveRegistry(
                m_project.rootPath(), collection,
                &registryPath, &reason);
            diagnostics.append(
                QString("%1: %2").arg(collection.dirName, reason));
        }
        QMessageBox::information(
            this, "Validate Collection",
            diagnostics.isEmpty()
                ? QString(
                    "No preprocessed image collection was found.")
                : QString(
                    "No preprocessed image collection has a usable registry.\n\n%1")
                    .arg(diagnostics.join("\n")));
        return;
    }

    Collection selected = collectionForPath(selectedProjectPath());
    if (!CollectionValidation::isEligible(
            m_project.rootPath(), selected)) {
        QStringList names;
        for (const Collection &collection : eligible)
            names.append(collection.dirName);
        bool accepted = false;
        const QString name = QInputDialog::getItem(
            this, "Validate Collection", "Collection:",
            names, 0, false, &accepted);
        if (!accepted)
            return;
        for (const Collection &collection : eligible) {
            if (collection.dirName == name) {
                selected = collection;
                break;
            }
        }
    }

    QString errorMessage;
    if (!ExifToolService::isAvailable(&errorMessage)) {
        QMessageBox::critical(this, "ExifTool", errorMessage);
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    CollectionValidationRun run;
    const bool validated = CollectionValidation::validate(
        m_project.rootPath(), selected, &run, &errorMessage);
    QApplication::restoreOverrideCursor();
    if (!validated) {
        QMessageBox::critical(
            this, "Validate Collection", errorMessage);
        return;
    }

    const QString manifestPath =
        QDir(m_project.rootPath()).filePath(
            QString("registry_validations/%1_%2.yaml")
                .arg(MetadataRegistry::snakeCase(selected.dirName),
                     QDateTime::currentDateTimeUtc().toString(
                         "yyyyMMdd_HHmmss")));
    if (!CollectionValidation::writeManifest(
            manifestPath, m_project.rootPath(), run, &errorMessage)) {
        QMessageBox::critical(
            this, "Validate Collection", errorMessage);
        return;
    }
    m_projectFileSystemModel->refreshDirectory(
        QDir(m_project.rootPath()).filePath("registry_validations"));

    CollectionValidationDialog dialog(run, this);
    connect(&dialog, &CollectionValidationDialog::reviewRequested,
            this, [this](const QString &rawPath,
                         const QString &registryPath) {
                validateRawImage(rawPath, registryPath);
            });
    dialog.exec();
}

Collection MainWindow::collectionForPath(const QString &path) const
{
    if (!m_project.isValid() || path.isEmpty())
        return {};
    const QString absolute = QFileInfo(path).isDir()
        ? QFileInfo(path).absoluteFilePath()
        : QFileInfo(path).absolutePath();
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    const QVector<Collection> collections =
        Project::discoverCollections(m_project.rootPath());
    for (const Collection &collection : collections) {
        if (QDir::cleanPath(collection.dirPath).compare(
                QDir::cleanPath(absolute), sensitivity) == 0) {
            return collection;
        }
    }
    return {};
}

ImageLayer *MainWindow::createStyledImageLayer(const QString &path)
{
    auto *layer = new ImageLayer(path);
    if (layer->rasterData().isNull()) {
        delete layer;
        QMessageBox::warning(this, "Add TIFF", "Could not load TIFF.");
        return nullptr;
    }

    const Collection collection = collectionForPath(path);
    if (!collection.collectionClass.isEmpty()) {
        CollectionStyleResolver styleResolver;
        layer->setStyle(styleResolver.imageStyle(
            collection, layer->rasterData()));
    }
    return layer;
}

PointLayer *MainWindow::createStyledPointLayer(const QString &path,
                                               ImageViewer *viewer)
{
    if (!viewer)
        return nullptr;

    QString errorMessage;
    const PointCollection pointCollection =
        PointCollection::load(path, &errorMessage);
    if (!pointCollection.isValid()) {
        QMessageBox::warning(this, "Add CSV",
                             pointCsvContractMessage(errorMessage));
        return nullptr;
    }

    auto *layer = new PointLayer(path, viewer->scene());
    const Collection collection = collectionForPath(path);
    if (collection.collectionClass == "point_collection"
        && layer->collection().isValid()) {
        CollectionStyleResolver styleResolver;
        layer->setStyle(styleResolver.pointStyle(
            collection, layer->collection().recordCount()));
    }
    return layer;
}

ImageViewer *MainWindow::currentImageViewer() const
{
    if (m_activeViewer)
        return m_activeViewer;

    const int index = ui->workspaceTabs->currentIndex();
    if (index < 0)
        return nullptr;

    auto *splitter = qobject_cast<QSplitter*>(ui->workspaceTabs->widget(index));
    if (!splitter || splitter->count() == 0)
        return nullptr;

    return qobject_cast<ImageViewer*>(splitter->widget(0));
}

void MainWindow::updatePropertiesTable(const PixelInfo &info)
{
    if (!ui->propertiesTable)
        return;

    ui->propertiesTable->setRowCount(0);
    if (info.row < 0 || info.bandCount <= 0)
        return;

    ui->propertiesTable->setRowCount(2 + info.bandCount);

    ui->propertiesTable->setItem(0, 0, new QTableWidgetItem("Row"));
    ui->propertiesTable->setItem(0, 1, new QTableWidgetItem(QString::number(info.row)));
    ui->propertiesTable->setItem(1, 0, new QTableWidgetItem("Column"));
    ui->propertiesTable->setItem(1, 1, new QTableWidgetItem(QString::number(info.col)));

    for (int b = 0; b < info.bandCount; ++b) {
        ui->propertiesTable->setItem(2 + b, 0,
            new QTableWidgetItem(QStringLiteral("Band %1").arg(b + 1)));
        ui->propertiesTable->setItem(2 + b, 1,
            new QTableWidgetItem(info.bandValues.value(b).toString()));
    }
}

void MainWindow::updatePropertiesForPointLayer(PointLayer *layer)
{
    Q_UNUSED(layer);
    onPointSelectionChanged();
}

void MainWindow::onSyncSplitViewsToggled(bool checked)
{
    ImageViewer::setSyncEnabled(checked);
}

void MainWindow::onSyncSplitZoomToggled(bool checked)
{
    ImageViewer::setSyncZoomEnabled(checked);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const QString suffix = QFileInfo(event->mimeData()->urls().first().toLocalFile()).suffix().toLower();
    const QStringList ok = {"tif", "tiff", "png", "jpg", "jpeg", "bmp", "csv"};
    if (ok.contains(suffix))
        event->acceptProposedAction();
    else
        event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QUrl url = event->mimeData()->urls().first();
    const QString path = url.toLocalFile();
    if (path.isEmpty()) {
        event->ignore();
        return;
    }

    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "csv") {
        ImageViewer *v = currentImageViewer();
        if (!v) {
            QMessageBox::information(this, "Add CSV",
                "Open an image first before adding a point CSV layer.");
              event->acceptProposedAction();
              return;
          }
        if (PointLayer *layer = createStyledPointLayer(path, v))
            v->layerManager()->addLayer(layer);
    } else {
        openImageAtPath(path);
    }
    event->acceptProposedAction();
}

void MainWindow::clearPropertiesTable()
{
    if (ui->propertiesTable)
        ui->propertiesTable->setRowCount(0);
}

void MainWindow::configureDockSizing()
{
    if (ui->centralwidget) {
        ui->centralwidget->setMinimumSize(0, 0);
        ui->centralwidget->setMaximumSize(0, 0);
        ui->centralwidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    auto constrainWidth = [](QDockWidget *dock, int minimumWidth, int maximumWidth) {
        if (!dock)
            return;

        dock->setMinimumWidth(minimumWidth);
        dock->setMaximumWidth(maximumWidth);

        if (QWidget *contents = dock->widget()) {
            contents->setMinimumWidth(minimumWidth);
            contents->setMaximumWidth(maximumWidth);
        }
    };

    constrainWidth(ui->dockProjectExplorer, 260, 460);
    constrainWidth(m_previewDock, 260, 460);
    constrainWidth(ui->dockLayers, 220, 320);
    constrainWidth(ui->dockProperties, 220, 320);
    constrainWidth(m_histogramDock, 220, 320);

    if (ui->dockViewer) {
        ui->dockViewer->setMinimumWidth(320);
        if (QWidget *contents = ui->dockViewer->widget())
            contents->setMinimumWidth(320);
    }
}

QString MainWindow::selectedProjectPath() const
{
    const QModelIndex index = ui->projectTree->currentIndex();
    if (!index.isValid())
        return {};

    const QFileInfo fi(projectPathFromIndex(index));
    if (!fi.isDir())
        return {};

    return fi.absoluteFilePath();
}

void MainWindow::createPointsCsv()
{
    if (!m_project.isValid()) {
        QMessageBox::information(this, "Create points",
                                 "No project is open. Please create or open a project first.");
        return;
    }

    const QVector<Collection> collections = Project::discoverCollections(m_project.rootPath());
    QVector<Collection> pointCollections;
    for (const Collection &col : collections) {
        if (col.collectionClass == "point_collection")
            pointCollections.append(col);
    }

    if (pointCollections.isEmpty()) {
        QMessageBox::information(this, "Create points",
                                 "No point collection is registered in this project.\n"
                                 "Register a point collection first using Project > Register collection...");
        return;
    }

    Collection selectedCollection = collectionForPath(selectedProjectPath());
    if (selectedCollection.collectionClass != "point_collection") {
        selectedCollection = {};
    }

    if (selectedCollection.collectionClass != "point_collection"
        && pointCollections.size() == 1) {
        selectedCollection = pointCollections.first();
    } else if (selectedCollection.collectionClass != "point_collection") {
        QStringList collectionNames;
        for (const Collection &collection : pointCollections)
            collectionNames.append(collection.dirName);
        bool ok = false;
        const QString choice = QInputDialog::getItem(this, "Select point collection",
                                                     "Point collection:", collectionNames, 0, false, &ok);
        if (!ok || choice.isEmpty())
            return;
        for (const Collection &collection : pointCollections) {
            if (collection.dirName == choice) {
                selectedCollection = collection;
                break;
            }
        }
    }

    const QString selectedDir = selectedCollection.dirPath;
    QSet<QString> usedBaseNames;
    const QFileInfoList existingPointFiles = QDir(selectedDir).entryInfoList(
        {"*.csv", "*.CSV"}, QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &file : existingPointFiles)
        usedBaseNames.insert(file.completeBaseName().toCaseFolded());

    QStringList availableBaseNames;
    const QString parentPath = Project::resolveReference(
        m_project.rootPath(), selectedCollection.parent);
    if (QDir(parentPath).exists()) {
        const QFileInfoList parentImages = QDir(parentPath).entryInfoList(
            {"*.tif", "*.tiff", "*.TIF", "*.TIFF",
             "*.png", "*.PNG", "*.jpg", "*.jpeg", "*.JPG", "*.JPEG",
             "*.bmp", "*.BMP"},
            QDir::Files | QDir::NoSymLinks, QDir::Name);
        for (const QFileInfo &image : parentImages) {
            const QString baseName = image.completeBaseName();
            if (!usedBaseNames.contains(baseName.toCaseFolded())
                && !availableBaseNames.contains(
                    baseName, Qt::CaseInsensitive)) {
                availableBaseNames.append(baseName);
            }
        }
    }

    QString savePath;
    if (!availableBaseNames.isEmpty()) {
        QStringList choices = availableBaseNames;
        choices.append("Custom name...");
        bool ok = false;
        const QString choice = QInputDialog::getItem(
            this, "Create points CSV", "Image or filename:",
            choices, 0, false, &ok);
        if (!ok || choice.isEmpty())
            return;
        if (choice != "Custom name...") {
            savePath = QDir(selectedDir).filePath(choice + ".csv");
        }
    }
    if (savePath.isEmpty()) {
        const QString defaultName =
            QDir(selectedDir).dirName() + ".csv";
        savePath = QFileDialog::getSaveFileName(
            this, "Create points CSV",
            QDir(selectedDir).filePath(defaultName),
            "CSV files (*.csv)");
    }
    if (savePath.isEmpty())
        return;

    const PointCollection collection = PointCollection::createEmpty({"row", "col"});
    QString errorMsg;
    if (!collection.save(savePath, &errorMsg)) {
        QMessageBox::warning(this, "Create points", errorMsg);
        return;
    }

    statusBar()->showMessage(QStringLiteral("Created points file: %1").arg(QFileInfo(savePath).fileName()), 5000);
    m_projectFileSystemModel->refreshDirectory(QFileInfo(savePath).absolutePath());

    // If an image viewer is active, offer to add the new layer
    ImageViewer *viewer = currentImageViewer();
      if (viewer && viewer->layerManager()) {
          const auto reply = QMessageBox::question(this, "Add layer",
                                                   "Add the new points file as a layer to the current viewer?");
          if (reply == QMessageBox::Yes) {
              if (PointLayer *layer = createStyledPointLayer(savePath, viewer))
                  viewer->layerManager()->addLayer(layer);
          }
      }
  }
