#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "project.h"

#include <QMainWindow>
#include <QMap>

class QFileSystemModel;
class ImageViewer;
class LayerManager;
class LayersDock;

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

private:
    void openImageAtPath(const QString &filePath);
    void loadProject(const Project &project);
    void updateProjectExplorer();
    void updateProjectActions();
    ImageViewer *currentImageViewer() const;

    Ui::MainWindow *ui;
    QFileSystemModel *m_projectFileSystemModel = nullptr;
    Project m_project;
    QMap<QString, QWidget *> m_openImageTabs;
    LayersDock *m_layersDock = nullptr;
};
#endif // MAINWINDOW_H