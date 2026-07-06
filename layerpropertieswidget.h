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
