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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "collectionstylefile.h"
#include "project.h"

#include <QMainWindow>
#include <QMap>
#include <QMetaObject>
#include <QPointer>
#include <QSettings>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHash>
#include <QList>
#include <QSet>
#include <QByteArray>

class QCloseEvent;
class QEvent;
class QFileSystemWatcher;
class ProjectFileSystemModel;
class ImageLayer;
class ImageViewer;
class HistogramDock;
class Layer;
class LayerManager;
class LayersDock;
class QTableWidget;
class QSplitter;
class QToolButton;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QLabel;
class QModelIndex;
class QSortFilterProxyModel;
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
    void createProjectReport();
    void importLegacyHspProject();
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
    void resetWindowLayout();
    void onSyncSplitViewsToggled(bool checked);
    void onSyncSplitZoomToggled(bool checked);
    void onActiveViewerChanged(ImageViewer *viewer);
    void createPointsCsv();
    void buildMetadataRules();
    void validateImageMetadata();
    void validateCollectionMetadata();
    void showAboutDialog();
    void showInteractionShortcutsDialog();
    void createProjectFromExample();

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
    void closeAttributeTables();
    ImageViewer *viewerForLayer(PointLayer *layer) const;
    bool copyDroppedFiles(const QList<QUrl> &urls,
                          const QString &destinationDirectory);
    bool validateRawImage(const QString &rawPath,
                          const QString &registryPath = {});
    Collection collectionForPath(const QString &path) const;
    ImageLayer *createStyledImageLayer(const QString &path);
    PointLayer *createStyledPointLayer(const QString &path,
                                       ImageViewer *viewer);
    void refreshReloadableArtifactWatchers();
    void scheduleArtifactReload(const QString &path, int delayMs = 500);
    void reloadArtifactFromDisk(const QString &path);
    void reloadViewerLayers(ImageViewer *viewer);
    QList<Layer*> reloadableLayersForPath(const QString &path) const;
    void setHistogramLayer(Layer *layer);
    void createViewerEmptyState();
    void updateViewerEmptyState();
    QModelIndex projectSourceIndex(const QModelIndex &index) const;
    QModelIndex projectProxyIndex(const QModelIndex &index) const;
    QString projectPathFromIndex(const QModelIndex &index) const;

    Ui::MainWindow *ui;
    ProjectFileSystemModel *m_projectFileSystemModel = nullptr;
    QSortFilterProxyModel *m_projectSortProxy = nullptr;
    QFileSystemWatcher *m_artifactWatcher = nullptr;
    Project m_project;
    QMap<QString, QWidget *> m_openImageTabs;
    LayersDock *m_layersDock = nullptr;
    HistogramDock *m_histogramDock = nullptr;
    PreviewDock *m_previewDock = nullptr;
    QToolButton *m_newFolderButton = nullptr;
    QComboBox *m_projectSortCombo = nullptr;
    QDoubleSpinBox *m_zoomPercentSpin = nullptr;
    QWidget *m_viewerEmptyState = nullptr;
    QLabel *m_viewerEmptyTitle = nullptr;
    QLabel *m_viewerEmptyBody = nullptr;
    QFrame *m_viewerQuickRule = nullptr;
    QLabel *m_viewerQuickTitle = nullptr;
    QLabel *m_viewerQuickBody = nullptr;
    QStringList m_recentProjects;
    QPointer<PointLayer> m_currentPointLayer;
    QMetaObject::Connection m_pointLayerConnection;
    QMetaObject::Connection m_pointCollectionConnection;
    QPointer<ImageViewer> m_activeViewer;
    QHash<PointLayer*, QPointer<PointAttributeTableWindow>> m_attributeTables;
    QMetaObject::Connection m_histogramStyleConnection;
    QMetaObject::Connection m_histogramDataConnection;
    QByteArray m_defaultWindowState;
    bool m_resetLayoutPending = false;
    QHash<QString, int> m_pendingReloadAttempts;
    QSet<QString> m_scheduledArtifactReloads;
    CollectionStyleData m_copiedCollectionStyle;
    QString m_copiedCollectionStyleKind;
    QString m_copiedCollectionStyleSource;
    void addToRecentProjects(const QString &path);
    void updateRecentProjectsMenu();
};

#endif // MAINWINDOW_H
