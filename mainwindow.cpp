#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "imageviewer.h"
#include "layersdock.h"
#include "layermanager.h"
#include "pointlayer.h"
#include "rawreader.h"
#include "collectiondialog.h"

#include <QAction>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_projectFileSystemModel(new QFileSystemModel(this))
{
    ui->setupUi(this);

    m_projectFileSystemModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    ui->projectTree->setModel(m_projectFileSystemModel);
    ui->projectTree->setDragEnabled(true);
    ui->projectTree->setHeaderHidden(true);
    ui->projectTree->hideColumn(1);
    ui->projectTree->hideColumn(2);
    ui->projectTree->hideColumn(3);

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

    connect(ui->actionSaveProject,
            &QAction::triggered,
            this,
            &MainWindow::saveProject);

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

    connect(ui->actionPan,
            &QAction::toggled,
            this,
            [this](bool enabled) {
                ImageViewer *viewer = currentImageViewer();
                if (viewer) {
                    viewer->setPanModeEnabled(enabled);
                }
            });

    connect(ui->projectTree,
            &QTreeView::doubleClicked,
            this,
            &MainWindow::onProjectTreeDoubleClicked);

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

    // Obtain LayersDock from the UI instead of creating it again
    if (QDockWidget *dockLayers = findChild<QDockWidget *>("dockLayers")) {
        m_layersDock = dockLayers->findChild<LayersDock *>("layersDock");
    } else {
        m_layersDock = new LayersDock(this);
        addDockWidget(Qt::RightDockWidgetArea, m_layersDock);
    }
    if (m_project.isValid())
        m_layersDock->setProjectPath(m_project.rootPath());

    // Ensure 3-column layout: Explorer (left), Viewer (center), Layers+Properties (right)
    setDockNestingEnabled(true);
    addDockWidget(Qt::LeftDockWidgetArea, ui->dockProjectExplorer);
    addDockWidget(Qt::RightDockWidgetArea, ui->dockViewer);
    addDockWidget(Qt::RightDockWidgetArea, ui->dockLayers);
    addDockWidget(Qt::RightDockWidgetArea, ui->dockProperties);

    // Split into 3 areas: Explorer | Viewer | (Layers ⊓ Properties)
    splitDockWidget(ui->dockViewer, ui->dockLayers, Qt::Horizontal);
    splitDockWidget(ui->dockLayers, ui->dockProperties, Qt::Vertical);

    // Resize proportions: Viewer takes more space
    resizeDocks({ui->dockProjectExplorer, ui->dockViewer, ui->dockLayers},
                {300, 600, 300}, Qt::Horizontal);

    // Remove old actionLoadPoints (moved to LayersDock)
    // No longer needed; the UI has been cleaned up.
    updateProjectActions();
    updateRecentProjectsMenu();
}

MainWindow::~MainWindow()
{
    delete ui;
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
    // Remove old actions from recent menu
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
        QAction *act = recentMenu->addAction(QDir(path).dirName());
        act->setToolTip(path);
        connect(act, &QAction::triggered, this, [this, path]() {
            // Try to open project
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
        this,
        "Select Project Parent Folder"
        );
    if (parentPath.isEmpty()) {
        return;
    }

    bool accepted = false;
    const QString projectName = QInputDialog::getText(
        this,
        "New Project",
        "Project name:",
        QLineEdit::Normal,
        "",
        &accepted
        ).trimmed();
    if (!accepted || projectName.isEmpty()) {
        return;
    }

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
        this,
        "Open Project Folder"
        );
    if (rootPath.isEmpty()) {
        return;
    }

    QString errorMessage;
    const Project project = Project::open(rootPath, &errorMessage);
    if (!project.isValid()) {
        QMessageBox::warning(this, "Open Project", errorMessage);
        return;
    }

    loadProject(project);
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

void MainWindow::openImage()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(
        this,
        "Open Image",
        m_project.isValid() ? m_project.rootPath() : QString(),
        "Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp)"
        );

    for (const QString &fileName : fileNames) {
        openImageAtPath(fileName);
    }
}

void MainWindow::importRaw()
{
    if (!m_project.isValid()) {
        QMessageBox::information(this, "Import raw",
                                 "No project is open. Please create or open a project first.");
        return;
    }

    const QStringList fileNames = QFileDialog::getOpenFileNames(
        this,
        "Select RAW files to import",
        QString(),
        "RAW files (*.CR2 *.CR3 *.NEF *.ARW *.DNG *.RAF *.ORF *.RW2 *.PEF *.SRW *.RAW *.DCR *.KDC)"
        );

    if (fileNames.isEmpty())
        return;

    // Ensure raw_files/ exists
    const QString rawDir = QDir(m_project.rootPath()).filePath("raw_files");
    QDir().mkpath(rawDir);

    int copiedCount = 0;
    for (const QString &srcPath : fileNames) {
        const QFileInfo fi(srcPath);
        const QString destPath = QDir(rawDir).filePath(fi.fileName());
        if (QFile::exists(destPath))
            continue; // skip duplicates
        if (QFile::copy(srcPath, destPath))
            ++copiedCount;
    }

    if (copiedCount > 0) {
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

    // Collect raw files recursively
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
            continue; // skip already extracted

        statusBar()->showMessage(
            QStringLiteral("Extracting: %1").arg(fi.fileName()), 0);

        QString errorMsg;
        if (RawReader::extractSensorData(rawPath, tiffPath, &errorMsg)) {
            ++successCount;
        } else {
            ++failCount;
            qWarning() << "Failed to extract" << rawPath << ":" << errorMsg;
        }
    }

    statusBar()->showMessage(
        QStringLiteral("Extraction complete: %1 succeeded, %2 failed")
            .arg(successCount).arg(failCount), 8000);

    updateProjectActions();
}

void MainWindow::registerCollection()
{
    if (!m_project.isValid()) {
        QMessageBox::information(this, "Register collection",
                                 "No project is open.");
        return;
    }

    const QVector<Collection> existingCollections = Project::discoverCollections(m_project.rootPath());
    CollectionDialog dialog(m_project.rootPath(), existingCollections, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString dirPath = dialog.selectedDirectory();
    if (dirPath.isEmpty())
        return;

    QString errorMsg;
    if (Project::registerCollection(dirPath,
                                     dialog.collectionType(),
                                     dialog.parent(),
                                     dialog.related(),
                                     dialog.registry(),
                                     dialog.script(),
                                     &errorMsg))
    {
        statusBar()->showMessage(
            QStringLiteral("Collection registered: %1").arg(
                QDir(dirPath).dirName()), 5000);
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

    // Check for collection.yaml
    const QString yamlPath = QDir(filePath).filePath("collection.yaml");
    const QFileInfo yamlFi(yamlPath);
    if (!yamlFi.exists())
        return;

    // Parse minimal YAML to extract class
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
        // Add as a PointLayer to the current viewer
        ImageViewer *viewer = currentImageViewer();
        if (!viewer) {
            QMessageBox::information(this, "Add layer",
                                     "No image viewer is active. Open an image first.");
            return;
        }

        auto *layer = new PointLayer(filePath);
        viewer->layerManager()->addLayer(layer);
    }
}

void MainWindow::onWorkspaceTabCloseRequested(int index)
{
    QWidget *tabWidget = ui->workspaceTabs->widget(index);

    const QString key = m_openImageTabs.key(tabWidget);
    if (!key.isEmpty()) {
        m_openImageTabs.remove(key);
    }

    ui->workspaceTabs->removeTab(index);
    delete tabWidget;

    updateProjectActions();
}

void MainWindow::onWorkspaceTabChanged(int index)
{
    if (index < 0) {
        ui->actionPan->setChecked(false);
        ui->actionPan->setEnabled(false);
        if (m_layersDock)
            m_layersDock->setLayerManager(nullptr);
        return;
    }

    ImageViewer *viewer = currentImageViewer();
    if (viewer) {
        ui->actionPan->setChecked(viewer->panModeEnabled());
        ui->actionPan->setEnabled(true);
        if (m_layersDock)
            m_layersDock->setLayerManager(viewer->layerManager());
    }
}

void MainWindow::openImageAtPath(const QString &filePath)
{
    const QFileInfo fi(filePath);
    const QString absolutePath = fi.absoluteFilePath();

    auto it = m_openImageTabs.find(absolutePath);
    if (it != m_openImageTabs.end()) {
        const int tabIndex = ui->workspaceTabs->indexOf(it.value());
        if (tabIndex >= 0) {
            ui->workspaceTabs->setCurrentIndex(tabIndex);
        }
        return;
    }

    auto *viewer = new ImageViewer(this);

    const QString tabTitle = fi.fileName();
    const int newIndex = ui->workspaceTabs->addTab(viewer, tabTitle);
    ui->workspaceTabs->setCurrentIndex(newIndex);
    viewer->setImage(absolutePath);

    // Autoload PointCollection si aplica
    if (viewer->layerManager()) {
        const QString baseName = fi.completeBaseName();
        const QString imgDir = fi.absolutePath();

        const QVector<Collection> collections = Project::discoverCollections(m_project.rootPath());

        QStringList matchingParents;
        for (const Collection &col : collections) {
            if (col.dirPath == imgDir) {
                matchingParents.append(col.dirPath);
            }
        }

        if (matchingParents.size() == 1) {
            QStringList pointCollections;
            for (const Collection &col : collections) {
                if (col.type == "point_collection" && col.parent == matchingParents.first()) {
                    pointCollections.append(col.dirPath);
                }
            }

            if (pointCollections.size() == 1) {
                const QString csvPath = QDir(pointCollections.first()).filePath(baseName + ".csv");
                if (QFile::exists(csvPath)) {
                    auto *layer = new PointLayer(csvPath);
                    viewer->layerManager()->addLayer(layer);
                }
            } else if (pointCollections.size() > 1) {
                qWarning() << "Multiple point_collections found for" << imgDir << "; autoload skipped.";
            }
        }
    }

    m_openImageTabs.insert(absolutePath, viewer);

    // Update layers dock
    if (m_layersDock)
        m_layersDock->setLayerManager(viewer->layerManager());

    updateProjectActions();
}

void MainWindow::loadProject(const Project &project)
{
    while (ui->workspaceTabs->count() > 0) {
        QWidget *w = ui->workspaceTabs->widget(0);
        ui->workspaceTabs->removeTab(0);
        delete w;
    }
    m_openImageTabs.clear();

    m_project = project;
    updateProjectExplorer();
    updateProjectActions();
    setWindowTitle(QString("%1 - rcaiman_projects").arg(m_project.name()));
    if (m_layersDock)
        m_layersDock->setProjectPath(m_project.rootPath());
    addToRecentProjects(m_project.rootPath());
}

void MainWindow::updateProjectExplorer()
{
    if (!m_project.isValid()) {
        ui->projectTree->setRootIndex(QModelIndex());
        return;
    }

    const QModelIndex rootIndex = m_projectFileSystemModel->setRootPath(m_project.rootPath());
    ui->projectTree->setRootIndex(rootIndex);
    ui->projectTree->setColumnWidth(0, 220);
}

void MainWindow::updateProjectActions()
{
    const bool hasProject = m_project.isValid();
    ui->actionSaveProject->setEnabled(hasProject);
    ui->actionPan->setEnabled(currentImageViewer() != nullptr);
    ui->actionImportRaw->setEnabled(hasProject);

    // Enable "Extract raw data" if there are raw files in the RAW folder
    bool hasRawFiles = false;
    if (hasProject) {
        const QString rawDir = QDir(m_project.rootPath()).filePath("raw_files");
        QDirIterator it(rawDir, QDir::Files);
        hasRawFiles = it.hasNext();
    }
    ui->actionExtractRawData->setEnabled(hasRawFiles);
}

ImageViewer *MainWindow::currentImageViewer() const
{
    const int index = ui->workspaceTabs->currentIndex();
    if (index < 0) {
        return nullptr;
    }
    return qobject_cast<ImageViewer *>(ui->workspaceTabs->widget(index));
}