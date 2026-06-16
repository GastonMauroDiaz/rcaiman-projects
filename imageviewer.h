#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include "rasterdata.h"
#include "rasterrenderer.h"

#include <QGraphicsView>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QWheelEvent>

class QGraphicsPixmapItem;
class QGraphicsScene;

class ImageViewer : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ImageViewer(QWidget *parent = nullptr);

    void setImage(const QString &fileName);
    void setPanModeEnabled(bool enabled);
    bool panModeEnabled() const;

    const RasterData &rasterData() const { return m_rasterData; }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateDisplay();
    void fitImageToView();
    void updateDragMode();

    bool m_panModeEnabled = false;
    bool m_spacePanActive = false;
    bool m_autoFitImage = false;
    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_imageItem = nullptr;
    RasterData m_rasterData;
    RasterRenderer m_renderer;
};

#endif // IMAGEVIEWER_H