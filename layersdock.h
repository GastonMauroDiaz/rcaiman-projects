#ifndef LAYERSDOCK_H
#define LAYERSDOCK_H

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QIcon;
class QPushButton;
class QDoubleSpinBox;
class LayerManager;
class Layer;
class LayerPropertiesWidget;
class PointLayer;

class LayersDock : public QWidget
{
    Q_OBJECT

public:
    explicit LayersDock(QWidget *parent = nullptr);

    void setLayerManager(LayerManager *manager);
    void setProjectPath(const QString &path);
    LayerManager *layerManager() const { return m_manager; }

signals:
    void activeLayerChanged(int index);
    void pointAttributeTableRequested(PointLayer *layer);

private slots:
    void updateList();
    void onItemChanged(QListWidgetItem *item);
    void updateActiveLayerFromUi(int row);
    void addPointLayer();
    void removeLayer();
    void moveUp();
    void moveDown();
    void onFlickerToggled(bool checked);
    void onFlickerSpeedChanged(double seconds);
    void onCurrentRowChanged(int row);
    void onLayerStyleChanged();
    void onRowsMoved(const QModelIndex &sourceParent,
                     int sourceStart,
                     int sourceEnd,
                     const QModelIndex &destinationParent,
                     int destinationRow);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QIcon pointLayerIcon(const Layer *layer) const;

    LayerManager *m_manager = nullptr;
    QListWidget *m_listWidget = nullptr;
    QPushButton *m_openTableBtn = nullptr;
    QPushButton *m_flickerBtn = nullptr;
    QDoubleSpinBox *m_flickerSpeed = nullptr;
    LayerPropertiesWidget *m_propertiesWidget = nullptr;
    bool m_updatingList = false;
    QString m_projectPath;
    Layer *m_trackedLayer = nullptr;
};

#endif // LAYERSDOCK_H
