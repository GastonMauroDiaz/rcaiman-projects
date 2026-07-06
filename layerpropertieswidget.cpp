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
