#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "project.h"

#include <QMainWindow>
#include <QMap>
#include <QSettings>
#include <QMetaObject>

class QFileSystemModel;
class ImageViewer;
class LayerManager;
class LayersDock;
class QTableWidget;
class PointLayer;
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
    void saveProject();
    void openImage();
    void importRaw();
    void extractRawData();
    void registerCollection();
    void onProjectTreeDoubleClicked(const QModelIndex &index);
    void onWorkspaceTabCloseRequested(int index);
    void onWorkspaceTabChanged(int index);
    void onLayerSelectionChanged(int index);
    void onEditPointsToggled(bool enabled);
    void onPointSelectionChanged(int index);
    void onProjectTreeItemClicked(const QModelIndex &index);

private:
    void openImageAtPath(const QString &filePath);
    void loadProject(const Project &project);
    void updateProjectExplorer();
    void updateProjectActions();
    ImageViewer *currentImageViewer() const;
    QString selectedProjectPath() const;
    void updatePropertiesTable(const PixelInfo &info);
    void updatePropertiesForPointLayer(PointLayer *layer);
    void clearPropertiesTable();

    Ui::MainWindow *ui;
    QFileSystemModel *m_projectFileSystemModel = nullptr;
    Project m_project;
    QMap<QString, QWidget *> m_openImageTabs;
    LayersDock *m_layersDock = nullptr;
    PreviewDock *m_previewDock = nullptr;
    QStringList m_recentProjects;
    PointLayer *m_currentPointLayer = nullptr;
    QMetaObject::Connection m_pointLayerConnection;
    void addToRecentProjects(const QString &path);
    void updateRecentProjectsMenu();
};

#endif // MAINWINDOW_H