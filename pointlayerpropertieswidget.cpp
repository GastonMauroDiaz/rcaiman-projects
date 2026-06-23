#include "pointlayerpropertieswidget.h"
#include "pointlayer.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>

PointLayerPropertiesWidget::PointLayerPropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QFormLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_colorButton = new QPushButton(this);
    m_colorButton->setMinimumWidth(64);
    layout->addRow("Color:", m_colorButton);

    m_sizeSpin = new QDoubleSpinBox(this);
    m_sizeSpin->setRange(1.0, 64.0);
    m_sizeSpin->setDecimals(1);
    m_sizeSpin->setSingleStep(1.0);
    layout->addRow("Size:", m_sizeSpin);

    m_symbolCombo = new QComboBox(this);
    m_symbolCombo->addItem("Circle", static_cast<int>(PointSymbol::Circle));
    m_symbolCombo->addItem("Cross", static_cast<int>(PointSymbol::Cross));
    layout->addRow("Symbol:", m_symbolCombo);

    connect(m_colorButton, &QPushButton::clicked,
            this, &PointLayerPropertiesWidget::chooseColor);
    connect(m_sizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double value) {
                if (!m_layer || m_updating)
                    return;
                m_layer->setSize(value);
            });
    connect(m_symbolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (!m_layer || m_updating || index < 0)
                    return;

                const PointSymbol symbol =
                    static_cast<PointSymbol>(m_symbolCombo->itemData(index).toInt());
                m_layer->setSymbol(symbol);
            });
}

void PointLayerPropertiesWidget::setLayer(PointLayer *layer)
{
    if (m_layer)
        disconnect(m_layer, nullptr, this, nullptr);

    m_layer = layer;

    if (m_layer) {
        connect(m_layer, &PointLayer::styleChanged,
                this, &PointLayerPropertiesWidget::updateFromLayer);
    }

    updateFromLayer();
}

void PointLayerPropertiesWidget::updateFromLayer()
{
    m_updating = true;
    const bool hasLayer = (m_layer != nullptr);
    setEnabled(hasLayer);

    if (hasLayer) {
        const PointLayerStyle style = m_layer->style();
        m_sizeSpin->setValue(style.size);
        const int symbolIndex = m_symbolCombo->findData(static_cast<int>(style.symbol));
        m_symbolCombo->setCurrentIndex(symbolIndex >= 0 ? symbolIndex : 0);
    }

    updateColorButton();
    m_updating = false;
}

void PointLayerPropertiesWidget::chooseColor()
{
    if (!m_layer)
        return;

    const QColor color = QColorDialog::getColor(m_layer->style().color, this, "Point color");
    if (color.isValid())
        m_layer->setColor(color);
}

void PointLayerPropertiesWidget::updateColorButton()
{
    const QColor color = m_layer ? m_layer->style().color : QColor(Qt::transparent);
    m_colorButton->setText(color.isValid() ? color.name(QColor::HexRgb) : QString());
    m_colorButton->setStyleSheet(color.isValid()
        ? QStringLiteral("background-color: %1;").arg(color.name(QColor::HexRgb))
        : QString());
}
