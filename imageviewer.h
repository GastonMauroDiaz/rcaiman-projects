#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMutex>
#include <QMouseEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QWheelEvent>

class QGraphicsScene;
class Layer;
class LayerManager;
class ImageLayer;
class PointLayer;

struct PixelInfo {
    int row = -1;
    int col = -1;
    int bandCount = 0;
    QVector<QVariant> bandValues;
};

class ImageViewer : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ImageViewer(QWidget *parent = nullptr);

    void setImage(const QString &fileName);
    void setPanModeEnabled(bool enabled);
    bool panModeEnabled() const;

    void setPointEditEnabled(bool enabled);
    bool pointEditEnabled() const { return m_pointEditEnabled; }
    PointLayer *editingPointLayer() const { return m_editingPointLayer; }

    LayerManager *layerManager() const { return m_layerManager; }
    ImageLayer *imageLayer() const { return m_imageLayer; }

signals:
    void pixelInfoChanged(const PixelInfo &info);
    void closeRequested();
    void activatedByClick();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

public:
    static void setSyncEnabled(bool enabled);
    static bool syncEnabled();

private slots:
    void onActiveLayerChanged(Layer *layer);

private:
    void fitImageToView();
    void updateDragMode();
    void syncToSiblings(int dx, int dy);
    bool panInteractionActive() const;
    bool handleSpacePanKeyEvent(QEvent *event);
    void setPointItemsInteractionBlocked(bool blocked);

    bool m_panModeEnabled = false;
    bool m_spacePanActive = false;
    bool m_autoFitImage = false;
    bool m_pointEditEnabled = false;
    PixelInfo m_lastPixelInfo;
    QPoint m_lastMousePos;
    QGraphicsScene *m_scene = nullptr;
    LayerManager *m_layerManager = nullptr;
    ImageLayer *m_imageLayer = nullptr;
    QPointer<PointLayer> m_editingPointLayer;
    static bool ms_syncEnabled;
    bool m_syncingScroll = false;
    QMutex m_syncMutex;
};

#endif // IMAGEVIEWER_H
