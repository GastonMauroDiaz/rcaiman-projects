#include "imagelayerpropertieswidget.h"
#include "imagelayer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QSpinBox>

#include <cmath>

namespace {
constexpr int trimSliderMaximum = 1000;
constexpr double trimPercentMaximum = 49.0;

double trimPercentFromSlider(int sliderValue)
{
    const double position = static_cast<double>(sliderValue) / trimSliderMaximum;
    return trimPercentMaximum * position * position;
}

int sliderFromTrimPercent(double trimPercent)
{
    const double normalized = qBound(0.0, trimPercent / trimPercentMaximum, 1.0);
    return qRound(std::sqrt(normalized) * trimSliderMaximum);
}

}

ImageLayerPropertiesWidget::ImageLayerPropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QFormLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(0, 100);
    layout->addRow("Opacity:", m_opacitySlider);

    m_opacitySpin = new QDoubleSpinBox(this);
    m_opacitySpin->setRange(0.0, 1.0);
    m_opacitySpin->setDecimals(2);
    m_opacitySpin->setSingleStep(0.05);
    layout->addRow("Opacity value:", m_opacitySpin);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("Single band", static_cast<int>(RasterRenderMode::SingleBand));
    m_modeCombo->addItem("RGB", static_cast<int>(RasterRenderMode::Rgb));
    layout->addRow("Mode:", m_modeCombo);

    m_rampCombo = new QComboBox(this);
    m_rampCombo->addItem("Gray", static_cast<int>(ColorRamp::Gray));
    m_rampCombo->addItem("Red-Blue", static_cast<int>(ColorRamp::RedBlue));
    m_rampCombo->addItem("Green-Magenta", static_cast<int>(ColorRamp::GreenMagenta));
    m_rampCombo->addItem("Rainbow", static_cast<int>(ColorRamp::Rainbow));
    m_rampCombo->addItem("Categorical (random)", static_cast<int>(ColorRamp::Categorical));
    layout->addRow("Ramp:", m_rampCombo);

    m_stretchCombo = new QComboBox(this);
    m_stretchCombo->addItem("None", static_cast<int>(StretchMode::None));
    m_stretchCombo->addItem("Linear", static_cast<int>(StretchMode::Linear));
    m_stretchCombo->addItem("Gamma", static_cast<int>(StretchMode::Gamma));
    layout->addRow("Stretch:", m_stretchCombo);

    m_gammaSlider = new QSlider(Qt::Horizontal, this);
    m_gammaSlider->setRange(100, 400);
    layout->addRow("Gamma:", m_gammaSlider);

    m_gammaSpin = new QDoubleSpinBox(this);
    m_gammaSpin->setRange(1.0, 4.0);
    m_gammaSpin->setDecimals(2);
    m_gammaSpin->setSingleStep(0.1);
    layout->addRow("Gamma value:", m_gammaSpin);

    m_bandSpin = new QSpinBox(this);
    m_bandSpin->setMinimum(1);
    layout->addRow("Band:", m_bandSpin);

    m_redBandSpin = new QSpinBox(this);
    m_redBandSpin->setMinimum(1);
    layout->addRow("Red:", m_redBandSpin);

    m_greenBandSpin = new QSpinBox(this);
    m_greenBandSpin->setMinimum(1);
    layout->addRow("Green:", m_greenBandSpin);

    m_blueBandSpin = new QSpinBox(this);
    m_blueBandSpin->setMinimum(1);
    layout->addRow("Blue:", m_blueBandSpin);

    m_tailTrimWidget = new QWidget(this);
    auto *tailTrimLayout = new QHBoxLayout(m_tailTrimWidget);
    tailTrimLayout->setContentsMargins(0, 0, 0, 0);
    m_tailTrimCheck = new QCheckBox(m_tailTrimWidget);
    m_trimLeftCheck = new QCheckBox("Left", m_tailTrimWidget);
    m_trimRightCheck = new QCheckBox("Right", m_tailTrimWidget);
    tailTrimLayout->addWidget(m_tailTrimCheck);
    tailTrimLayout->addWidget(m_trimLeftCheck);
    tailTrimLayout->addWidget(m_trimRightCheck);
    tailTrimLayout->addStretch(1);
    layout->addRow("Tail trim:", m_tailTrimWidget);

    m_trimPercentSlider = new QSlider(Qt::Horizontal, this);
    m_trimPercentSlider->setRange(0, trimSliderMaximum);
    layout->addRow("Trim:", m_trimPercentSlider);

    m_trimPercentSpin = new QDoubleSpinBox(this);
    m_trimPercentSpin->setRange(0.0, 49.0);
    m_trimPercentSpin->setDecimals(2);
    m_trimPercentSpin->setSingleStep(0.01);
    m_trimPercentSpin->setSuffix(" %");
    layout->addRow("Trim value:", m_trimPercentSpin);

    m_manualRangeCheck = new QCheckBox(this);
    layout->addRow("Manual range:", m_manualRangeCheck);

    m_manualMinSpin = new QDoubleSpinBox(this);
    m_manualMinSpin->setRange(-1.0e12, 1.0e12);
    m_manualMinSpin->setDecimals(6);
    m_manualMinSpin->setSingleStep(1.0);
    layout->addRow("Min:", m_manualMinSpin);

    m_manualMaxSpin = new QDoubleSpinBox(this);
    m_manualMaxSpin->setRange(-1.0e12, 1.0e12);
    m_manualMaxSpin->setDecimals(6);
    m_manualMaxSpin->setSingleStep(1.0);
    layout->addRow("Max:", m_manualMaxSpin);

    connect(m_opacitySlider, &QSlider::valueChanged,
            this, [this](int value) {
                if (!m_layer || m_updating)
                    return;
                m_layer->setOpacity(static_cast<double>(value) / 100.0);
            });
    connect(m_opacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double value) {
                if (!m_layer || m_updating)
                    return;
                m_layer->setOpacity(value);
            });
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (!m_layer || m_updating || index < 0)
                    return;
                m_layer->setRenderMode(static_cast<RasterRenderMode>(m_modeCombo->currentData().toInt()));
                updateControlVisibility();
            });
    connect(m_rampCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (!m_layer || m_updating || index < 0)
                    return;
                m_layer->setColorRamp(static_cast<ColorRamp>(m_rampCombo->currentData().toInt()));
                updateControlVisibility();
            });
    connect(m_stretchCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (!m_layer || m_updating || index < 0)
                    return;
                m_layer->setStretchMode(static_cast<StretchMode>(m_stretchCombo->currentData().toInt()));
                updateControlVisibility();
            });
    connect(m_gammaSlider, &QSlider::valueChanged,
            this, [this](int value) {
                if (!m_layer || m_updating)
                    return;
                m_layer->setGamma(static_cast<double>(value) / 100.0);
            });
    connect(m_gammaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double value) {
                if (!m_layer || m_updating)
                    return;
                m_layer->setGamma(value);
            });
    connect(m_bandSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                if (!m_layer || m_updating)
                    return;
                m_layer->setRenderBand(value - 1);
            });
    auto rgbBandChanged = [this]() {
        if (!m_layer || m_updating)
            return;
        m_layer->setRgbBands(m_redBandSpin->value() - 1,
                             m_greenBandSpin->value() - 1,
                             m_blueBandSpin->value() - 1);
    };
    connect(m_redBandSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [rgbBandChanged](int) { rgbBandChanged(); });
    connect(m_greenBandSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [rgbBandChanged](int) { rgbBandChanged(); });
    connect(m_blueBandSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [rgbBandChanged](int) { rgbBandChanged(); });
    connect(m_tailTrimCheck, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (!m_layer || m_updating)
                    return;
                m_layer->setTailTrim(checked);
                updateControlVisibility();
            });
    connect(m_trimLeftCheck, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (!m_layer || m_updating)
                    return;
                m_layer->setTrimLeftTail(checked);
                updateControlVisibility();
            });
    connect(m_trimRightCheck, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (!m_layer || m_updating)
                    return;
                m_layer->setTrimRightTail(checked);
                updateControlVisibility();
            });
    connect(m_trimPercentSlider, &QSlider::valueChanged,
            this, [this](int value) {
                if (!m_layer || m_updating)
                    return;
                m_layer->setTrimPercent(trimPercentFromSlider(value));
            });
    connect(m_trimPercentSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double value) {
                if (!m_layer || m_updating)
                    return;
                m_layer->setTrimPercent(value);
            });
    connect(m_manualRangeCheck, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (!m_layer || m_updating)
                    return;

                if (checked) {
                    double minValue = 0.0;
                    double maxValue = 1.0;
                    automaticRenderRange(&minValue, &maxValue);

                    m_updating = true;
                    m_manualMinSpin->setValue(minValue);
                    m_manualMaxSpin->setValue(maxValue);
                    m_updating = false;

                    m_layer->setManualRange(true, minValue, maxValue);
                } else {
                    m_layer->setManualRangeEnabled(false);
                }
                updateControlVisibility();
            });
    auto manualRangeChanged = [this]() {
        if (!m_layer || m_updating)
            return;
        m_layer->setManualRange(
            m_manualMinSpin->value(),
            m_manualMaxSpin->value());
    };
    connect(m_manualMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [manualRangeChanged](double) { manualRangeChanged(); });
    connect(m_manualMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [manualRangeChanged](double) { manualRangeChanged(); });
}

void ImageLayerPropertiesWidget::setLayer(ImageLayer *layer)
{
    if (m_layer)
        disconnect(m_layer, nullptr, this, nullptr);

    m_layer = layer;

    if (m_layer) {
        connect(m_layer, &ImageLayer::styleChanged,
                this, &ImageLayerPropertiesWidget::updateFromLayer);
    }

    updateFromLayer();
}

void ImageLayerPropertiesWidget::updateFromLayer()
{
    m_updating = true;
    const bool hasLayer = (m_layer != nullptr);
    setEnabled(hasLayer);

    if (hasLayer) {
        const ImageLayerStyle style = m_layer->style();
        const double opacity = style.opacity;
        m_opacitySlider->setValue(static_cast<int>(opacity * 100.0));
        m_opacitySpin->setValue(opacity);
        m_modeCombo->setCurrentIndex(m_modeCombo->findData(static_cast<int>(style.render.mode)));
        m_rampCombo->setCurrentIndex(m_rampCombo->findData(static_cast<int>(style.render.colorRamp)));
        m_stretchCombo->setCurrentIndex(m_stretchCombo->findData(static_cast<int>(style.render.stretchMode)));
        m_gammaSlider->setValue(static_cast<int>(style.render.gamma * 100.0 + 0.5));
        m_gammaSpin->setValue(style.render.gamma);
        const int bandCount = qMax(1, m_layer->rasterData().bands());
        m_bandSpin->setMaximum(bandCount);
        m_bandSpin->setValue(style.render.band + 1);
        m_redBandSpin->setMaximum(bandCount);
        m_greenBandSpin->setMaximum(bandCount);
        m_blueBandSpin->setMaximum(bandCount);
        m_redBandSpin->setValue(qBound(1, style.render.redBand + 1, bandCount));
        m_greenBandSpin->setValue(qBound(1, style.render.greenBand + 1, bandCount));
        m_blueBandSpin->setValue(qBound(1, style.render.blueBand + 1, bandCount));
        m_tailTrimCheck->setChecked(style.render.tailTrim);
        m_trimLeftCheck->setChecked(style.render.trimLeftTail);
        m_trimRightCheck->setChecked(style.render.trimRightTail);
        m_trimPercentSlider->setValue(sliderFromTrimPercent(style.render.trimPercent));
        m_trimPercentSpin->setValue(style.render.trimPercent);
        m_manualRangeCheck->setChecked(style.render.manualRangeEnabled);
        if (style.render.manualRangeEnabled) {
            m_manualMinSpin->setValue(style.render.manualMin);
            m_manualMaxSpin->setValue(style.render.manualMax);
        } else {
            double minValue = 0.0;
            double maxValue = 1.0;
            automaticRenderRange(&minValue, &maxValue);
            m_manualMinSpin->setValue(minValue);
            m_manualMaxSpin->setValue(maxValue);
        }
    }

    updateControlVisibility();
    m_updating = false;
}

void ImageLayerPropertiesWidget::updateControlVisibility()
{
    const bool rgbMode = m_layer && m_layer->style().render.mode == RasterRenderMode::Rgb;
    const bool categoricalRamp = !rgbMode && m_layer
        && m_layer->style().render.colorRamp == ColorRamp::Categorical;
    const bool gammaMode = m_layer && m_layer->style().render.stretchMode == StretchMode::Gamma;
    const bool manualRangeEnabled =
        m_layer && m_layer->style().render.manualRangeEnabled;
    const bool trimControlsEnabled =
        m_layer && m_layer->style().render.tailTrim && !manualRangeEnabled;
    const bool trimRangeEnabled =
        trimControlsEnabled
        && (m_layer->style().render.trimLeftTail
            || m_layer->style().render.trimRightTail);

    auto setFieldVisible = [this](QWidget *field, bool visible) {
        if (!field)
            return;
        field->setVisible(visible);
        if (auto *layout = qobject_cast<QFormLayout*>(this->layout())) {
            if (QWidget *label = layout->labelForField(field))
                label->setVisible(visible);
        }
    };

    setFieldVisible(m_rampCombo, !rgbMode);
    setFieldVisible(m_bandSpin, !rgbMode);
    setFieldVisible(m_redBandSpin, rgbMode);
    setFieldVisible(m_greenBandSpin, rgbMode);
    setFieldVisible(m_blueBandSpin, rgbMode);
    setFieldVisible(m_stretchCombo, !categoricalRamp);
    setFieldVisible(m_gammaSlider, !categoricalRamp && gammaMode);
    setFieldVisible(m_gammaSpin, !categoricalRamp && gammaMode);
    setFieldVisible(m_tailTrimWidget, !categoricalRamp);
    setFieldVisible(m_trimPercentSlider, !categoricalRamp);
    setFieldVisible(m_trimPercentSpin, !categoricalRamp);
    setFieldVisible(m_manualRangeCheck, !categoricalRamp);
    setFieldVisible(m_manualMinSpin, !categoricalRamp);
    setFieldVisible(m_manualMaxSpin, !categoricalRamp);

    m_tailTrimCheck->setEnabled(!manualRangeEnabled);
    m_trimLeftCheck->setEnabled(trimControlsEnabled);
    m_trimRightCheck->setEnabled(trimControlsEnabled);
    m_trimPercentSlider->setEnabled(trimRangeEnabled);
    m_trimPercentSpin->setEnabled(trimRangeEnabled);
    m_manualMinSpin->setEnabled(manualRangeEnabled);
    m_manualMaxSpin->setEnabled(manualRangeEnabled);
}

void ImageLayerPropertiesWidget::automaticRenderRange(
    double *minValue,
    double *maxValue) const
{
    if (!minValue || !maxValue)
        return;

    *minValue = 0.0;
    *maxValue = 1.0;
    if (!m_layer || m_layer->rasterData().isNull())
        return;

    RasterRenderStyle autoStyle = m_layer->style().render;
    autoStyle.manualRangeEnabled = false;

    RasterRenderer &renderer = m_layer->renderer();
    if (autoStyle.mode == RasterRenderMode::Rgb) {
        renderer.renderRgbRange(
            m_layer->rasterData(),
            autoStyle.redBand,
            autoStyle.greenBand,
            autoStyle.blueBand,
            autoStyle,
            *minValue,
            *maxValue);
    } else {
        renderer.renderRange(
            m_layer->rasterData(),
            autoStyle.band,
            autoStyle,
            *minValue,
            *maxValue);
    }
}
