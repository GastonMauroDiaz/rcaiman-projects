#ifndef LAYERSDOCK_H
#define LAYERSDOCK_H

#include <QDockWidget>

class QListWidget;
class QListWidgetItem;
class LayerManager;

class LayersDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit LayersDock(QWidget *parent = nullptr);

    void setLayerManager(LayerManager *manager);
    LayerManager *layerManager() const { return m_manager; }

private slots:
    void updateList();
    void onItemChanged(QListWidgetItem *item);
    void addPointLayer();
    void removeLayer();
    void moveUp();
    void moveDown();

private:
    LayerManager *m_manager = nullptr;
    QListWidget *m_listWidget = nullptr;
    bool m_updatingList = false;
};

#endif // LAYERSDOCK_H