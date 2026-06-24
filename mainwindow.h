#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "project.h"

#include <QMainWindow>
#include <QMap>
#include <QMetaObject>
#include <QPointer>
#include <QSettings>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHash>

class QCloseEvent;
class QEvent;
class ProjectFileSystemModel;
class ImageViewer;
class LayerManager;
class LayersDock;
class QTableWidget;
class QSplitter;
class QToolButton;
class PointLayer;
class PointAttributeTableWindow;
class PreviewDock;

struct PixelInfo;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void newProject();
    void openProject();
    void closeProject();
    void saveProject();
    void saveProjectAs();
    void saveProjectAsCleanCopy();
    void openImage();
    void importRaw();
    void extractRawData();
    void registerCollection();
    void onProjectTreeDoubleClicked(const QModelIndex &index);
    void onProjectTreeContextMenu(const QPoint &position);
    void onWorkspaceTabCloseRequested(int index);
    void onWorkspaceTabChanged(int index);
    void onLayerSelectionChanged(int index);
    void onEditPointsToggled(bool enabled);
    void onCancelEditTriggered();
    void onPointSelectionChanged();
    void onProjectTreeItemClicked(const QModelIndex &index);
    void splitViewer();
    void onSyncSplitViewsToggled(bool checked);
    void onActiveViewerChanged(ImageViewer *viewer);
    void createPointsCsv();

private:
    void openImageAtPath(const QString &filePath);
    bool loadImageIntoViewer(ImageViewer *viewer, const QString &filePath,
                             bool showWarning = true);
    void loadProject(const Project &project);
    void closeCurrentProject();
    void saveApplicationState();
    void restoreApplicationState();
    void saveProjectSession();
    void restoreProjectSession();
    QString projectRelativePath(const QString &absolutePath) const;
    QString projectAbsolutePath(const QString &relativePath) const;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void updateProjectExplorer();
    void updateProjectActions();
    ImageViewer *currentImageViewer() const;
    QString selectedProjectPath() const;
    void updatePropertiesTable(const PixelInfo &info);
    void updatePropertiesForPointLayer(PointLayer *layer);
    void clearPropertiesTable();
    void configureDockSizing();
    void configureViewer(ImageViewer *viewer);
    void createProjectFolder();
    void openAttributeTable(PointLayer *layer);
    ImageViewer *viewerForLayer(PointLayer *layer) const;
    bool copyDroppedFiles(const QList<QUrl> &urls,
                          const QString &destinationDirectory);

    Ui::MainWindow *ui;
    ProjectFileSystemModel *m_projectFileSystemModel = nullptr;
    Project m_project;
    QMap<QString, QWidget *> m_openImageTabs;
    LayersDock *m_layersDock = nullptr;
    PreviewDock *m_previewDock = nullptr;
    QToolButton *m_newFolderButton = nullptr;
    QStringList m_recentProjects;
    PointLayer *m_currentPointLayer = nullptr;
    QMetaObject::Connection m_pointLayerConnection;
    QMetaObject::Connection m_pointCollectionConnection;
    QPointer<ImageViewer> m_activeViewer;
    QHash<PointLayer*, QPointer<PointAttributeTableWindow>> m_attributeTables;
    void addToRecentProjects(const QString &path);
    void updateRecentProjectsMenu();
};

#endif // MAINWINDOW_H
