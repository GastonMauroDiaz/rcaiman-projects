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

#ifndef POINTLAYERPROPERTIESWIDGET_H
#define POINTLAYERPROPERTIESWIDGET_H

#include <QPointer>
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

    QPointer<PointLayer> m_layer;
    QPushButton *m_colorButton = nullptr;
    QDoubleSpinBox *m_sizeSpin = nullptr;
    QComboBox *m_symbolCombo = nullptr;
    QComboBox *m_labelCombo = nullptr;
    QDoubleSpinBox *m_labelSizeSpin = nullptr;
    bool m_updating = false;
};

#endif // POINTLAYERPROPERTIESWIDGET_H
