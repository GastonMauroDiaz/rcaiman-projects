#include "layerpropertieswidget.h"
#include "imagelayer.h"
#include "imagelayerpropertieswidget.h"
#include "pointlayer.h"
#include "pointlayerpropertieswidget.h"

#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

LayerPropertiesWidget::LayerPropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack);

    m_emptyWidget = new QLabel("No layer selected", this);
    m_pointWidget = new PointLayerPropertiesWidget(this);
    m_imageWidget = new ImageLayerPropertiesWidget(this);

    m_stack->addWidget(m_emptyWidget);
    m_stack->addWidget(m_pointWidget);
    m_stack->addWidget(m_imageWidget);
}

void LayerPropertiesWidget::setLayer(Layer *layer)
{
    m_pointWidget->setLayer(nullptr);
    m_imageWidget->setLayer(nullptr);

    if (auto *pointLayer = qobject_cast<PointLayer*>(layer)) {
        m_pointWidget->setLayer(pointLayer);
        m_stack->setCurrentWidget(m_pointWidget);
    } else if (auto *imageLayer = qobject_cast<ImageLayer*>(layer)) {
        m_imageWidget->setLayer(imageLayer);
        m_stack->setCurrentWidget(m_imageWidget);
    } else {
        m_stack->setCurrentWidget(m_emptyWidget);
    }
}
