#ifndef POINTATTRIBUTETABLEWINDOW_H
#define POINTATTRIBUTETABLEWINDOW_H

#include <QPointer>
#include <QWidget>

class ImageViewer;
class QLabel;
class PointAttributeTableModel;
class PointLayer;
class QSortFilterProxyModel;
class QTableView;

class PointAttributeTableWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PointAttributeTableWindow(PointLayer *layer,
                                       ImageViewer *viewer,
                                       QWidget *parent = nullptr);

private:
    void syncSelectionFromLayer();
    void scheduleTableSelectionSync();
    void applyTableSelectionToLayer();
    void updateCounter();
    void zoomToSelection();

    QPointer<PointLayer> m_layer;
    QPointer<ImageViewer> m_viewer;
    PointAttributeTableModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_counter = nullptr;
    bool m_syncingSelection = false;
    bool m_selectionSyncPending = false;
};

#endif // POINTATTRIBUTETABLEWINDOW_H
