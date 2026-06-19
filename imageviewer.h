#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWheelEvent>

class QGraphicsScene;
class LayerManager;
class ImageLayer;

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

    LayerManager *layerManager() const { return m_layerManager; }
    ImageLayer *imageLayer() const { return m_imageLayer; }

signals:
    void pixelInfoChanged(const PixelInfo &info);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void fitImageToView();
    void updateDragMode();

    bool m_panModeEnabled = false;
    bool m_spacePanActive = false;
    bool m_autoFitImage = false;
    QGraphicsScene *m_scene = nullptr;
    LayerManager *m_layerManager = nullptr;
    ImageLayer *m_imageLayer = nullptr;
};

#endif // IMAGEVIEWER_H