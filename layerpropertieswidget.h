#ifndef LAYERPROPERTIESWIDGET_H
#define LAYERPROPERTIESWIDGET_H

#include <QWidget>

class ImageLayerPropertiesWidget;
class Layer;
class PointLayerPropertiesWidget;
class QStackedWidget;

class LayerPropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LayerPropertiesWidget(QWidget *parent = nullptr);

    void setLayer(Layer *layer);

private:
    QWidget *m_emptyWidget = nullptr;
    PointLayerPropertiesWidget *m_pointWidget = nullptr;
    ImageLayerPropertiesWidget *m_imageWidget = nullptr;
    QStackedWidget *m_stack = nullptr;
};

#endif // LAYERPROPERTIESWIDGET_H
