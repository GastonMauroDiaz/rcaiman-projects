#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMutex>
#include <QMouseEvent>
#include <QPointer>
#include <QRubberBand>
#include <QResizeEvent>
#include <QSet>
#include <QWheelEvent>

class QGraphicsScene;
class Layer;
class LayerManager;
class ImageLayer;
class PointLayer;

enum class ZoomCursorDirection {
    In,
    Out
};

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
    void fitLoadedImage();
    QPointF viewCenter() const;
    QTransform viewTransform() const { return transform(); }
    void restoreView(const QTransform &transform, const QPointF &center);
    void zoomToPointIndices(PointLayer *layer, const QSet<int> &indices);
    void setPanModeEnabled(bool enabled);
    bool panModeEnabled() const;
    void setZoomModeEnabled(bool enabled);
    bool zoomModeEnabled() const;

    void setPointEditEnabled(bool enabled);
    bool pointEditEnabled() const { return m_pointEditEnabled; }
    PointLayer *editingPointLayer() const { return m_editingPointLayer; }

    LayerManager *layerManager() const { return m_layerManager; }
    ImageLayer *imageLayer() const { return m_imageLayer; }
    QString primarySourcePath() const { return m_primarySourcePath; }
    void setPrimarySourcePath(const QString &path) { m_primarySourcePath = path; }

signals:
    void pixelInfoChanged(const PixelInfo &info);
    void closeRequested();
    void activatedByClick();
    void pointLayerFileDropped(const QString &path);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

public:
    static void setSyncEnabled(bool enabled);
    static bool syncEnabled();
    static void setSyncZoomEnabled(bool enabled);
    static bool syncZoomEnabled();

private slots:
    void onActiveLayerChanged(Layer *layer);

private:
    void fitImageToView();
    void updateDragMode();
    void syncToSiblings(int dx, int dy);
    bool panInteractionActive() const;
    bool zoomInteractionActive() const;
    bool handleSpacePanKeyEvent(QEvent *event);
    bool handleTemporaryZoomKeyEvent(QEvent *event);
    void setPointItemsInteractionBlocked(bool blocked);
    void applyDragZoom(const QPoint &pos);
    PointLayer *boxSelectionPointLayer() const;
    void beginPointBoxSelection(const QPoint &pos);
    void updatePointBoxSelection(const QPoint &pos);
    void finishPointBoxSelection(const QPoint &pos);
    void setZoomCursorDirection(ZoomCursorDirection direction);

    bool m_panModeEnabled = false;
    bool m_spacePanActive = false;
    bool m_zoomModeEnabled = false;
    bool m_temporaryZoomActive = false;
    bool m_zoomDragging = false;
    bool m_pointBoxSelecting = false;
    bool m_autoFitImage = false;
    bool m_pointEditEnabled = false;
    PixelInfo m_lastPixelInfo;
    QPoint m_lastMousePos;
    QGraphicsScene *m_scene = nullptr;
    LayerManager *m_layerManager = nullptr;
    ImageLayer *m_imageLayer = nullptr;
    QString m_primarySourcePath;
    QPointer<PointLayer> m_editingPointLayer;
    QPoint m_zoomDragLastPos;
    ZoomCursorDirection m_zoomCursorDirection = ZoomCursorDirection::In;
    QPoint m_pointBoxOrigin;
    QRect m_pointBoxRect;
    Qt::KeyboardModifiers m_pointBoxModifiers;
    QPointer<QRubberBand> m_pointBoxRubberBand;
    static bool ms_syncEnabled;
    static bool ms_syncZoomEnabled;
    bool m_syncingScroll = false;
    QMutex m_syncMutex;
};

#endif // IMAGEVIEWER_H
