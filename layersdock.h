#ifndef LAYERSDOCK_H
#define LAYERSDOCK_H

#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QDoubleSpinBox;
class LayerManager;
class Layer;

class LayersDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit LayersDock(QWidget *parent = nullptr);

    void setLayerManager(LayerManager *manager);
    void setProjectPath(const QString &path);
    LayerManager *layerManager() const { return m_manager; }

signals:
    void activeLayerChanged(int index);

private slots:
    void updateList();
    void onItemChanged(QListWidgetItem *item);
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void updateActiveLayerFromUi(int row);
    void addPointLayer();
    void removeLayer();
    void moveUp();
    void moveDown();
    void onFlickerToggled(bool checked);
    void onFlickerSpeedChanged(double seconds);
    void onCurrentRowChanged(int row);

private:
    LayerManager *m_manager = nullptr;
    QListWidget *m_listWidget = nullptr;
    QPushButton *m_flickerBtn = nullptr;
    QDoubleSpinBox *m_flickerSpeed = nullptr;
    bool m_updatingList = false;
    QString m_projectPath;
    Layer *m_trackedLayer = nullptr;
};

#endif // LAYERSDOCK_H