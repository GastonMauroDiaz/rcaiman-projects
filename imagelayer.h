#ifndef IMAGELAYER_H
#define IMAGELAYER_H

#include "layer.h"
#include "layerstyle.h"
#include "rasterdata.h"
#include "rasterrenderer.h"

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
    void setStyle(const ImageLayerStyle &style);
    void setOpacity(double opacity);
    void setRenderStyle(const RasterRenderStyle &renderStyle);
    void setColorRamp(ColorRamp ramp);
    void setStretchMode(StretchMode mode);
    void setTailTrim(bool enabled);
    void setTrimPercent(double percent);
    void setGamma(double gamma);
    void setRenderBand(int band);
    void setRenderMode(RasterRenderMode mode);
    void setRgbBands(int redBand, int greenBand, int blueBand);

private:
    void applyStyleToItem();
    void refreshRenderedImage();

    RasterData m_rasterData;
    RasterRenderer m_renderer;
    ImageLayerStyle m_style;
    QGraphicsPixmapItem *m_pixmapItem = nullptr;
};

#endif // IMAGELAYER_H
