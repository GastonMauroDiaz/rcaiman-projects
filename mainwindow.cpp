#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "imageviewer.h"
#include "layersdock.h"
#include "layermanager.h"
#include "layer.h"
#include "imagelayer.h"
#include "pointlayer.h"
#include "pointattributetablewindow.h"
#include "previewdock.h"
#include "projectcleanupdialog.h"
#include "projectfilesystemmodel.h"
#include "rawreader.h"
#include "collectiondialog.h"
#include "collectionautoload.h"
#include "collectionstyledialog.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QMimeData>
#include <QPixmap>
#include <QProcess>
#include <QSizePolicy>
#include <QSettings>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QJsonObject imageStyleToJson(const ImageLayerStyle &style)
{
    const RasterRenderStyle &render = style.render;
    QJsonObject object;
    object["opacity"] = style.opacity;
    object["mode"] = static_cast<int>(render.mode);
    object["ramp"] = static_cast<int>(render.colorRamp);
    object["stretch"] = static_cast<int>(render.stretchMode);
    object["tail_trim"] = render.tailTrim;
    object["trim_percent"] = render.trimPercent;
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
    RasterRenderStyle &render = style.render;
    render.mode = static_cast<RasterRenderMode>(
        object.value("mode").toInt(static_cast<int>(render.mode)));
    render.colorRamp = static_cast<ColorRamp>(
        object.value("ramp").toInt(static_cast<int>(render.colorRamp)));
    render.stretchMode = static_cast<StretchMode>(
        object.value("stretch").toInt(static_cast<int>(render.stretchMode)));
    render.tailTrim = object.value("tail_trim").toBool(render.tailTrim);
    render.trimPercent =
        object.value("trim_percent").toDouble(render.trimPercent);
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

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_projectFileSystemModel(new ProjectFileSystemModel(this))
{
    ui->setupUi(this);

    m_projectFileSystemModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    ui->projectTree->setModel(m_projectFileSystemModel);
    const QModelIndex fileSystemRoot =
        m_projectFileSystemModel->setRootPath(QString());
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
    ui->dockProjectExplorerLayout->insertWidget(0, m_newFolderButton);
    connect(m_newFolderButton, &QToolButton::clicked,
            this, &MainWindow::createProjectFolder);

    connect(m_projectFileSystemModel, &QAbstractItemModel::rowsInserted,
            this, [this]() { updateProjectActions(); });
    connect(m_projectFileSystemModel, &QAbstractItemModel::rowsRemoved,
            this, [this]() { updateProjectActions(); });
    connect(m_projectFileSystemModel, &QFileSystemModel::directoryLoaded,
            this, [this]() { updateProjectActions(); });

    // Load recent projects
    QSettings settings;
    m_recentProjects = settings.value("recentProjects").toStringList();

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

    connect(ui->actionCreatePoints,
            &QAction::triggered,
            this,
            &MainWindow::createPointsCsv);

    connect(ui->actionPan,
            &QAction::toggled,
            this,
            [this](bool enabled) {
                ImageViewer *viewer = currentImageViewer();
                if (viewer)
                    viewer->setPanModeEnabled(enabled);
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

    connect(ui->projectTree,
            &QTreeView::doubleClicked,
            this,
            &MainWindow::onProjectTreeDoubleClicked);

    connect(ui->projectTree,
            &QTreeView::clicked,
            this,
            &MainWindow::onProjectTreeItemClicked);

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

    configureDockSizing();

    // Initial dock proportions.
    resizeDocks({ui->dockProjectExplorer, ui->dockViewer, ui->dockLayers},
                {420, 600, 260}, Qt::Horizontal);

    updateProjectActions();
    updateRecentProjectsMenu();
    restoreApplicationState();
}

MainWindow::~MainWindow()
{
    delete ui;
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
    const QString parentPath = QFileDialog::getExistingDirectory(
        this, "Select Project Parent Folder");
    if (parentPath.isEmpty())
        return;

    bool accepted = false;
    const QString projectName = QInputDialog::getText(
        this, "New Project", "Project name:",
        QLineEdit::Normal, "", &accepted).trimmed();
    if (!accepted || projectName.isEmpty())
        return;

    const QString rootPath = QDir(parentPath).filePath(projectName);
    QString errorMessage;
    const Project project = Project::create(rootPath, projectName, &errorMessage);
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
        m_project.buildCleanupPlan(), this);
    if (cleanupDialog.exec() != QDialog::Accepted)
        return;

    const QString destination = QDir(parentPath).filePath(projectName);
    QString errorMessage;
    const Project copy =
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

    loadProject(copy);
    statusBar()->showMessage(
        QStringLiteral("Clean project copy created: %1")
            .arg(destination),
        5000);
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

    if (rawFiles.isEmpty()) {
        QMessageBox::information(this, "Extract raw data",
                                 "No raw files found in raw_files folder.");
        return;
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
        if (RawReader::extractSensorData(rawPath, tiffPath, m_project.rootPath(), &errorMsg))
            ++successCount;
        else
            ++failCount;
    }

    statusBar()->showMessage(
        QStringLiteral("Extraction complete: %1 succeeded, %2 failed")
            .arg(successCount).arg(failCount), 8000);
    m_projectFileSystemModel->refreshDirectory(sensorDir);

    updateProjectActions();
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
    const QString filePath = m_projectFileSystemModel->filePath(index);
    const QFileInfo fi(filePath);

    const QStringList imageExtensions = {"tif", "tiff", "png", "jpg", "jpeg", "bmp"};
    if (imageExtensions.contains(fi.suffix().toLower())) {
        openImageAtPath(filePath);
        return;
    }

    if (!fi.isDir())
        return;

    const QString yamlPath = QDir(filePath).filePath("collection.yaml");
    const QFileInfo yamlFi(yamlPath);
    if (!yamlFi.exists())
        return;

    QFile file(yamlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QString type;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.startsWith("class:")) {
            const int sep = line.indexOf(':');
            if (sep >= 0)
                type = line.mid(sep + 1).trimmed();
            break;
        }
    }

    if (type == "point_collection") {
        ImageViewer *viewer = currentImageViewer();
        if (!viewer) {
            QMessageBox::information(this, "Add layer",
                                     "No image viewer is active. Open an image first.");
            return;
        }
                auto *layer = new PointLayer(filePath, viewer->scene());
        viewer->layerManager()->addLayer(layer);
    }
}

void MainWindow::onProjectTreeContextMenu(const QPoint &position)
{
    if (!m_project.isValid())
        return;

    const QModelIndex index = ui->projectTree->indexAt(position);
    if (!index.isValid())
        return;

    const QFileInfo info(m_projectFileSystemModel->filePath(index));
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
    QAction *styleAction = nullptr;
    if (info.isDir()) {
        menu.addSeparator();
        registerAction = menu.addAction("Register collection...");
        styleAction = menu.addAction("Edit collection style...");
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
    if (styleAction)
        styleAction->setEnabled(supportsStyle);

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

        QFileInfo destination(
            m_projectFileSystemModel->filePath(index));
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

    const QString filePath = m_projectFileSystemModel->filePath(index);
    const QFileInfo fi(filePath);

    if (!fi.isFile()) {
        m_previewDock->clear();
        return;
    }

    const QString suffix = fi.suffix().toLower();
    const QStringList textExts = {"yaml", "yml", "json", "csv", "txt", "log", "md"};
    const QStringList imgExts = {"tif", "tiff", "png", "jpg", "jpeg", "bmp"};

    if (textExts.contains(suffix)) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            m_previewDock->showText(QString::fromUtf8(file.readAll()));
        else
            m_previewDock->clear();
    } else if (imgExts.contains(suffix)) {
        QPixmap pix(filePath);
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
}

void MainWindow::onLayerSelectionChanged(int index)
{
    ImageViewer *viewer = currentImageViewer();
    if (!viewer)
        return;

    LayerManager *mgr = viewer->layerManager();
    if (!mgr)
        return;

    if (index < 0 || index >= mgr->layerCount())
        return;

    Layer *layer = mgr->layerAt(index);
    mgr->setActiveLayer(layer);

    // Disconnect previous point layer connection
    if (m_currentPointLayer) {
        disconnect(m_pointLayerConnection);
        disconnect(m_pointCollectionConnection);
        m_currentPointLayer = nullptr;
    }

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

void MainWindow::onPointSelectionChanged()
{
    if (!m_currentPointLayer || !ui->propertiesTable)
        return;

    const PointCollection &col = m_currentPointLayer->collection();
    const int selectionCount =
        m_currentPointLayer->selectedPointIndices().size();
    const int index = m_currentPointLayer->primarySelectedPointIndex();
    const int sectionARows = 4;
    const int sectionBRows = selectionCount == 1 && index >= 0
        ? 3 + col.points()[index].attributes.size()
        : (selectionCount > 1 ? 1 : 0);

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
        ui->propertiesTable->setItem(
            sectionARows, 0, new QTableWidgetItem("Selected"));
        ui->propertiesTable->setItem(
            sectionARows, 1,
            new QTableWidgetItem(QString::number(selectionCount)));
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
    viewer->setPointEditEnabled(enabled);

    // Update Cancel edit button state
    PointLayer *pointLayer = viewer->editingPointLayer();
    ui->actionCancelEdit->setEnabled(pointLayer && pointLayer->editMode());
    ui->actionEditPoints->setChecked(viewer->pointEditEnabled());
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
        ui->actionPan->setChecked(false);
        ui->actionPan->setEnabled(false);
        ui->actionSyncSplitViews->setEnabled(false);
        ui->actionEditPoints->setChecked(false);
        ui->actionEditPoints->setEnabled(false);
        ui->actionCancelEdit->setEnabled(false);
        if (m_layersDock)
            m_layersDock->setLayerManager(nullptr);
        clearPropertiesTable();
        return;
    }

    ui->actionPan->setChecked(viewer->panModeEnabled());
    ui->actionPan->setEnabled(true);

    // Enable sync only if the viewer is inside a QSplitter with siblings
    auto *splitter = qobject_cast<QSplitter*>(viewer->parentWidget());
    ui->actionSyncSplitViews->setEnabled(splitter && splitter->count() > 1);

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
    connect(viewer, &ImageViewer::activatedByClick, this, [this, viewer]() {
        if (m_activeViewer == viewer)
            return;

        m_activeViewer = viewer;
        onActiveViewerChanged(viewer);
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

        if (m_activeViewer == v) {
            auto *remainingViewer = qobject_cast<ImageViewer*>(splitter->widget(0));
            m_activeViewer = remainingViewer;
            onActiveViewerChanged(remainingViewer);
            if (remainingViewer)
                remainingViewer->setFocus();
        }
    });
}

void MainWindow::loadProject(const Project &project)
{
    closeCurrentProject();

    m_project = project;
    updateProjectExplorer();
    updateProjectActions();
    setWindowTitle(QString("%1 - rcaiman_projects").arg(m_project.name()));
    if (m_layersDock)
        m_layersDock->setProjectPath(m_project.rootPath());
    if (m_previewDock)
        m_previewDock->show();
    addToRecentProjects(m_project.rootPath());
    restoreProjectSession();
}

void MainWindow::closeCurrentProject()
{
    saveProjectSession();

    if (m_currentPointLayer) {
        disconnect(m_pointLayerConnection);
        disconnect(m_pointCollectionConnection);
        m_currentPointLayer = nullptr;
    }

    {
        const QSignalBlocker blocker(ui->workspaceTabs);
        while (ui->workspaceTabs->count() > 0) {
            QWidget *tab = ui->workspaceTabs->widget(0);
            ui->workspaceTabs->removeTab(0);
            delete tab;
        }
    }

    m_openImageTabs.clear();
    m_activeViewer.clear();
    onActiveViewerChanged(nullptr);

    if (m_layersDock) {
        m_layersDock->setLayerManager(nullptr);
        m_layersDock->setProjectPath(QString());
    }
    if (m_previewDock) {
        m_previewDock->clear();
        m_previewDock->hide();
    }

    m_project = Project();
    m_projectFileSystemModel->setProjectRoot(QString());
    ui->projectTree->setRootIndex(
        m_projectFileSystemModel->setRootPath(QString()));
    clearPropertiesTable();
    setWindowTitle("rcaiman_projects");
    updateProjectActions();
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

    if (ui->workspaceTabs->count() == 0)
        return;

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
    updateProjectActions();
}

void MainWindow::updateProjectExplorer()
{
    if (!m_project.isValid()) {
        ui->projectTree->setRootIndex(QModelIndex());
        return;
    }
    m_projectFileSystemModel->setProjectRoot(m_project.rootPath());
    const QModelIndex rootIndex = m_projectFileSystemModel->setRootPath(m_project.rootPath());
    ui->projectTree->setRootIndex(rootIndex);
    ui->projectTree->setColumnWidth(0, 220);
}

void MainWindow::updateProjectActions()
{
    const bool hasProject = m_project.isValid();
    ui->actionSaveProject->setEnabled(hasProject);
    ui->actionSaveProjectAs->setEnabled(hasProject);
    ui->actionSaveProjectAsCleanCopy->setEnabled(hasProject);
    ui->actionCloseProject->setEnabled(hasProject);
    ui->actionPan->setEnabled(currentImageViewer() != nullptr);
    ui->actionImportRaw->setEnabled(hasProject);
    ui->actionRegisterCollection->setEnabled(hasProject);
    ui->actionCreatePoints->setEnabled(hasProject);
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
            QMessageBox::information(this, "Add Points",
                "Open an image first before adding a point layer.");
            event->acceptProposedAction();
            return;
        }
        v->layerManager()->addLayer(new PointLayer(path, v->scene()));
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

    const QFileInfo fi(m_projectFileSystemModel->filePath(index));
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
    QStringList pointCollectionDirs;
    for (const Collection &col : collections) {
        if (col.collectionClass == "point_collection")
            pointCollectionDirs.append(col.dirPath);
    }

    if (pointCollectionDirs.isEmpty()) {
        QMessageBox::information(this, "Create points",
                                 "No point collection is registered in this project.\n"
                                 "Register a point collection first using Project > Register collection...");
        return;
    }

    QString selectedDir;
    if (pointCollectionDirs.size() == 1) {
        selectedDir = pointCollectionDirs.first();
    } else {
        bool ok = false;
        const QString choice = QInputDialog::getItem(this, "Select point collection",
                                                     "Point collection:", pointCollectionDirs, 0, false, &ok);
        if (!ok || choice.isEmpty())
            return;
        selectedDir = choice;
    }

    const QString defaultName = QDir(selectedDir).dirName() + ".csv";
    const QString savePath = QFileDialog::getSaveFileName(this, "Create points CSV",
                                                          QDir(selectedDir).filePath(defaultName),
                                                          "CSV files (*.csv)");
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
        if (reply == QMessageBox::Yes)
            viewer->layerManager()->addLayer(new PointLayer(savePath, viewer->scene()));
    }
}
