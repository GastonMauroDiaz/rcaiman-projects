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

    m_labelCombo = new QComboBox(this);
    layout->addRow("Label:", m_labelCombo);

    m_labelSizeSpin = new QDoubleSpinBox(this);
    m_labelSizeSpin->setRange(6.0, 48.0);
    m_labelSizeSpin->setDecimals(1);
    m_labelSizeSpin->setSingleStep(1.0);
    layout->addRow("Label size:", m_labelSizeSpin);

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
    connect(m_labelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (!m_layer || m_updating || index < 0)
                    return;
                m_layer->setLabelField(m_labelCombo->itemData(index).toString());
                m_labelSizeSpin->setEnabled(index > 0);
            });
    connect(m_labelSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double value) {
                if (!m_layer || m_updating)
                    return;
                m_layer->setLabelSize(value);
            });
}

void PointLayerPropertiesWidget::setLayer(PointLayer *layer)
{
    if (m_layer)
        disconnect(m_layer, nullptr, this, nullptr);

    m_layer = layer;
    updateLabelFields();

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
        const int labelIndex = m_labelCombo->findData(style.labelField);
        m_labelCombo->setCurrentIndex(labelIndex >= 0 ? labelIndex : 0);
        m_labelSizeSpin->setValue(style.labelSize);
        m_labelSizeSpin->setEnabled(m_labelCombo->currentIndex() > 0);
    }

    updateColorButton();
    m_updating = false;
}

void PointLayerPropertiesWidget::updateLabelFields()
{
    m_labelCombo->clear();
    m_labelCombo->addItem("None", QString());
    m_labelCombo->addItem("ID", QStringLiteral("__id__"));

    if (!m_layer)
        return;

    const QStringList columns = m_layer->collection().columnNames();
    for (int i = 2; i < columns.size(); ++i)
        m_labelCombo->addItem(columns.at(i), columns.at(i));
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
