#ifndef IMAGELAYERPROPERTIESWIDGET_H
#define IMAGELAYERPROPERTIESWIDGET_H

#include <QWidget>

class ImageLayer;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QSlider;
class QWidget;

class ImageLayerPropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageLayerPropertiesWidget(QWidget *parent = nullptr);

    void setLayer(ImageLayer *layer);

private:
    void updateFromLayer();
    void updateControlVisibility();
    void automaticRenderRange(double *minValue, double *maxValue) const;

    ImageLayer *m_layer = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_rampCombo = nullptr;
    QComboBox *m_stretchCombo = nullptr;
    QSpinBox *m_bandSpin = nullptr;
    QSpinBox *m_redBandSpin = nullptr;
    QSpinBox *m_greenBandSpin = nullptr;
    QSpinBox *m_blueBandSpin = nullptr;
    QWidget *m_tailTrimWidget = nullptr;
    QCheckBox *m_tailTrimCheck = nullptr;
    QCheckBox *m_trimLeftCheck = nullptr;
    QCheckBox *m_trimRightCheck = nullptr;
    QSlider *m_trimPercentSlider = nullptr;
    QDoubleSpinBox *m_trimPercentSpin = nullptr;
    QCheckBox *m_manualRangeCheck = nullptr;
    QDoubleSpinBox *m_manualMinSpin = nullptr;
    QDoubleSpinBox *m_manualMaxSpin = nullptr;
    QSlider *m_gammaSlider = nullptr;
    QDoubleSpinBox *m_gammaSpin = nullptr;
    QSlider *m_opacitySlider = nullptr;
    QDoubleSpinBox *m_opacitySpin = nullptr;
    bool m_updating = false;
};

#endif // IMAGELAYERPROPERTIESWIDGET_H
