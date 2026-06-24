#ifndef POINTLAYERPROPERTIESWIDGET_H
#define POINTLAYERPROPERTIESWIDGET_H

#include <QWidget>

class PointLayer;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;

class PointLayerPropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PointLayerPropertiesWidget(QWidget *parent = nullptr);

    void setLayer(PointLayer *layer);

private:
    void updateFromLayer();
    void chooseColor();
    void updateColorButton();
    void updateLabelFields();

    PointLayer *m_layer = nullptr;
    QPushButton *m_colorButton = nullptr;
    QDoubleSpinBox *m_sizeSpin = nullptr;
    QComboBox *m_symbolCombo = nullptr;
    QComboBox *m_labelCombo = nullptr;
    QDoubleSpinBox *m_labelSizeSpin = nullptr;
    bool m_updating = false;
};

#endif // POINTLAYERPROPERTIESWIDGET_H
