#ifndef IMAGELAYER_H
#define IMAGELAYER_H

#include "layer.h"
#include "layerstyle.h"
#include "rasterdata.h"
#include "rasterrenderer.h"

class QTimer;

class ImageLayer : public Layer
{
    Q_OBJECT

public:
    explicit ImageLayer(const QString &filePath, QObject *parent = nullptr);

    const QVector<QGraphicsItem*> &buildItems(QGraphicsScene *scene) override;

    const RasterData &rasterData() const { return m_rasterData; }
    RasterRenderer &renderer() { return m_renderer; }
    QGraphicsPixmapItem *pixmapItem() const { return m_pixmapItem; }
    ImageLayerStyle style() const { return m_style; }
    quint64 dataRevision() const { return m_dataRevision; }
    bool supportsReloadFromDisk() const override;
    bool reloadFromDisk(QString *errorMessage = nullptr) override;
    void setStyle(const ImageLayerStyle &style);
    void setOpacity(double opacity);
    void setRenderStyle(const RasterRenderStyle &renderStyle);
    void setColorRamp(ColorRamp ramp);
    void setStretchMode(StretchMode mode);
    void setTailTrim(bool enabled);
    void setTrimLeftTail(bool enabled);
    void setTrimRightTail(bool enabled);
    void setTrimPercent(double percent);
    void setManualRangeEnabled(bool enabled);
    void setManualRange(double minValue, double maxValue);
    void setManualRange(bool enabled, double minValue, double maxValue);
    void setManualRangeDeferred(double minValue, double maxValue);
    void setGamma(double gamma);
    void setGammaDeferred(double gamma);
    void setRenderBand(int band);
    void setRenderMode(RasterRenderMode mode);
    void setRgbBands(int redBand, int greenBand, int blueBand);
    void refreshRenderedImageNow();

private:
    void setStyleInternal(const ImageLayerStyle &style, bool deferRender);
    void scheduleRenderRefresh();
    void applyStyleToItem();
    void refreshRenderedImage();

    RasterData m_rasterData;
    RasterRenderer m_renderer;
    ImageLayerStyle m_style;
    QGraphicsPixmapItem *m_pixmapItem = nullptr;
    quint64 m_dataRevision = 0;
    QTimer *m_renderRefreshTimer = nullptr;
    bool m_renderRefreshPending = false;
};

#endif // IMAGELAYER_H
