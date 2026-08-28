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

#include "pointlayerpropertieswidget.h"
#include "pointlayer.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

PointLayerPropertiesWidget::PointLayerPropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QFormLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_fastPreviewLabel = new QLabel(this);
    m_fastPreviewLabel->setWordWrap(true);
    m_fastPreviewLabel->setStyleSheet("color: #6b7280;");
    layout->addRow(m_fastPreviewLabel);

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

    m_updating = true;
    m_layer = layer;
    updateLabelFields();
    m_updating = false;

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

    if (m_fastPreviewLabel) {
        const bool fastPreview =
            hasLayer && m_layer->usesAggregateRendering();
        m_fastPreviewLabel->setVisible(fastPreview);
        if (fastPreview) {
            m_fastPreviewLabel->setText(
                QStringLiteral(
                    "Fast preview: this CSV has %1 points. The file is "
                    "complete, but the viewer draws a regular subset to stay "
                    "responsive. Editing, box selection, labels, and the "
                    "attribute table are disabled. Limit used: %2 points.")
                    .arg(m_layer->collection().recordCount())
                    .arg(m_layer->fastPreviewLimit()));
        }
    }

    if (hasLayer) {
        const PointLayerStyle style = m_layer->style();
        m_sizeSpin->setValue(style.size);
        const int symbolIndex = m_symbolCombo->findData(static_cast<int>(style.symbol));
        m_symbolCombo->setCurrentIndex(symbolIndex >= 0 ? symbolIndex : 0);
        const int labelIndex = m_labelCombo->findData(style.labelField);
        m_labelCombo->setCurrentIndex(labelIndex >= 0 ? labelIndex : 0);
        m_labelSizeSpin->setValue(style.labelSize);
        const bool fastPreview = m_layer->usesAggregateRendering();
        m_labelCombo->setEnabled(!fastPreview);
        m_labelSizeSpin->setEnabled(
            !fastPreview && m_labelCombo->currentIndex() > 0);
        m_labelCombo->setToolTip(
            fastPreview
                ? QStringLiteral("Labels are disabled in fast preview mode.")
                : QString());
        m_labelSizeSpin->setToolTip(
            fastPreview
                ? QStringLiteral("Labels are disabled in fast preview mode.")
                : QString());
    }

    updateColorButton();
    m_updating = false;
}

void PointLayerPropertiesWidget::updateLabelFields()
{
    const QSignalBlocker blocker(m_labelCombo);
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
