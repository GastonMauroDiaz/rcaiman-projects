#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "imageviewer.h"

#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
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

    updateProjectActions();
}

MainWindow::~MainWindow()
{
    delete ui;
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
    if (QDir(rootPath).exists()) {
        const auto answer = QMessageBox::question(
            this,
            "Project Folder Exists",
            "The selected project folder already exists. Continue and initialize it as a project?"
            );
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

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

void MainWindow::onProjectTreeDoubleClicked(const QModelIndex &index)
{
    const QString filePath = m_projectFileSystemModel->filePath(index);
    const QFileInfo fi(filePath);

    const QStringList imageExtensions = {"tif", "tiff", "png", "jpg", "jpeg", "bmp"};
    if (imageExtensions.contains(fi.suffix().toLower())) {
        openImageAtPath(filePath);
    }
}

void MainWindow::onWorkspaceTabCloseRequested(int index)
{
    QWidget *tabWidget = ui->workspaceTabs->widget(index);

    // Remove from the open-tabs map
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
        return;
    }

    ImageViewer *viewer = currentImageViewer();
    if (viewer) {
        ui->actionPan->setChecked(viewer->panModeEnabled());
        ui->actionPan->setEnabled(true);
    }
}

void MainWindow::openImageAtPath(const QString &filePath)
{
    const QFileInfo fi(filePath);
    const QString absolutePath = fi.absoluteFilePath();

    // Check if already open
    auto it = m_openImageTabs.constFind(absolutePath);
    if (it != m_openImageTabs.constEnd()) {
        // Switch to the existing tab
        const int tabIndex = ui->workspaceTabs->indexOf(it.value());
        if (tabIndex >= 0) {
            ui->workspaceTabs->setCurrentIndex(tabIndex);
        }
        return;
    }

    // Create new ImageViewer in a tab
    auto *viewer = new ImageViewer(this);

    const QString tabTitle = fi.fileName();
    const int newIndex = ui->workspaceTabs->addTab(viewer, tabTitle);
    ui->workspaceTabs->setCurrentIndex(newIndex);
    viewer->setImage(absolutePath);

    m_openImageTabs.insert(absolutePath, viewer);

    updateProjectActions();
}

void MainWindow::loadProject(const Project &project)
{
    // Close all open image tabs
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
    ui->actionSaveProject->setEnabled(m_project.isValid());
    ui->actionPan->setEnabled(currentImageViewer() != nullptr);
}

ImageViewer *MainWindow::currentImageViewer() const
{
    const int index = ui->workspaceTabs->currentIndex();
    if (index < 0) {
        return nullptr;
    }
    return qobject_cast<ImageViewer *>(ui->workspaceTabs->widget(index));
}
