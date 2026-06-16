#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QGraphicsView>
#include <QKeyEvent>
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

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateDragMode();

    bool m_panModeEnabled = false;
    bool m_spacePanActive = false;
    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_imageItem = nullptr;
};

#endif
