// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// This file is part of rcaiman-projects.
//
// rcaiman-projects is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// rcaiman-projects is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with rcaiman-projects. If not, see <https://www.gnu.org/licenses/>.
//
// Part of the rcaiman suite.

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
    void updateBandSpinTexts();
    void updateBandSpinText(QSpinBox *spin, int band) const;

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
