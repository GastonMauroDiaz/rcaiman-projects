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

#include "collectionstyledialog.h"
#include "pointcollection.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

CollectionStyleDialog::CollectionStyleDialog(
    const Collection &collection,
    QWidget *parent)
    : QDialog(parent)
    , m_collection(collection)
{
    setWindowTitle(QStringLiteral("Collection Style - %1").arg(collection.dirName));
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);
    if (collection.collectionClass == "point_collection")
        layout->addWidget(createPointEditor());
    else
        layout->addWidget(createImageEditor());

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    auto *resetButton = buttons->addButton(
        "Reset to defaults", QDialogButtonBox::ResetRole);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &CollectionStyleDialog::saveStyle);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    connect(resetButton, &QPushButton::clicked,
            this, &CollectionStyleDialog::resetStyle);
    layout->addWidget(buttons);

    loadInitialStyle();
}

QWidget *CollectionStyleDialog::createPointEditor()
{
    auto *widget = new QWidget(this);
    auto *form = new QFormLayout(widget);

    m_colorButton = new QPushButton(widget);
    form->addRow("Color:", m_colorButton);

    m_pointSize = new QDoubleSpinBox(widget);
    m_pointSize->setRange(1.0, 64.0);
    m_pointSize->setDecimals(1);
    form->addRow("Size:", m_pointSize);

    m_symbol = new QComboBox(widget);
    m_symbol->addItem("Circle", static_cast<int>(PointSymbol::Circle));
    m_symbol->addItem("Cross", static_cast<int>(PointSymbol::Cross));
    form->addRow("Symbol:", m_symbol);

    m_labelField = new QComboBox(widget);
    m_labelField->addItem("None", QString());
    m_labelField->addItem("ID", QStringLiteral("__id__"));
    for (const QString &field : pointLabelFields())
        m_labelField->addItem(field, field);
    form->addRow("Label:", m_labelField);

    m_labelSize = new QDoubleSpinBox(widget);
    m_labelSize->setRange(6.0, 48.0);
    m_labelSize->setDecimals(1);
    form->addRow("Label size:", m_labelSize);

    connect(m_colorButton, &QPushButton::clicked,
            this, &CollectionStyleDialog::choosePointColor);
    connect(m_labelField, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) { m_labelSize->setEnabled(index > 0); });
    return widget;
}

QWidget *CollectionStyleDialog::createImageEditor()
{
    auto *widget = new QWidget(this);
    auto *form = new QFormLayout(widget);

    m_opacity = new QDoubleSpinBox(widget);
    m_opacity->setRange(0.0, 1.0);
    m_opacity->setDecimals(2);
    m_opacity->setSingleStep(0.1);
    form->addRow("Opacity:", m_opacity);

    m_blend = new QComboBox(widget);
    m_blend->addItem("Normal", static_cast<int>(ImageBlendMode::Normal));
    m_blend->addItem("Multiply", static_cast<int>(ImageBlendMode::Multiply));
    m_blend->addItem("Maximum", static_cast<int>(ImageBlendMode::Maximum));
    form->addRow("Blend:", m_blend);

    m_mode = new QComboBox(widget);
    m_mode->addItem("Single band", static_cast<int>(RasterRenderMode::SingleBand));
    m_mode->addItem("RGB", static_cast<int>(RasterRenderMode::Rgb));
    form->addRow("Mode:", m_mode);

    m_ramp = new QComboBox(widget);
    m_ramp->addItem("Gray", static_cast<int>(ColorRamp::Gray));
    m_ramp->addItem("Red-Blue", static_cast<int>(ColorRamp::RedBlue));
    m_ramp->addItem("Green-Magenta", static_cast<int>(ColorRamp::GreenMagenta));
    m_ramp->addItem("Rainbow", static_cast<int>(ColorRamp::Rainbow));
    m_ramp->addItem("Categorical", static_cast<int>(ColorRamp::Categorical));
    form->addRow("Ramp:", m_ramp);

    m_stretch = new QComboBox(widget);
    m_stretch->addItem("None", static_cast<int>(StretchMode::None));
    m_stretch->addItem("Linear", static_cast<int>(StretchMode::Linear));
    m_stretch->addItem("Gamma", static_cast<int>(StretchMode::Gamma));
    form->addRow("Stretch:", m_stretch);

    m_gamma = new QDoubleSpinBox(widget);
    m_gamma->setRange(1.0, 4.0);
    m_gamma->setDecimals(2);
    form->addRow("Gamma:", m_gamma);

    auto makeBandSpin = [widget]() {
        auto *spin = new QSpinBox(widget);
        spin->setRange(1, 999);
        return spin;
    };
    m_band = makeBandSpin();
    m_redBand = makeBandSpin();
    m_greenBand = makeBandSpin();
    m_blueBand = makeBandSpin();
    form->addRow("Band:", m_band);
    form->addRow("Red:", m_redBand);
    form->addRow("Green:", m_greenBand);
    form->addRow("Blue:", m_blueBand);

    auto *tailTrimWidget = new QWidget(widget);
    auto *tailTrimLayout = new QHBoxLayout(tailTrimWidget);
    tailTrimLayout->setContentsMargins(0, 0, 0, 0);
    m_tailTrim = new QCheckBox(tailTrimWidget);
    m_trimLeft = new QCheckBox("Left", tailTrimWidget);
    m_trimRight = new QCheckBox("Right", tailTrimWidget);
    tailTrimLayout->addWidget(m_tailTrim);
    tailTrimLayout->addWidget(m_trimLeft);
    tailTrimLayout->addWidget(m_trimRight);
    tailTrimLayout->addStretch(1);
    form->addRow("Tail trim:", tailTrimWidget);
    m_trim = new QDoubleSpinBox(widget);
    m_trim->setRange(0.0, 49.0);
    m_trim->setDecimals(2);
    m_trim->setSuffix(" %");
    form->addRow("Trim value:", m_trim);

    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CollectionStyleDialog::updateImageVisibility);
    connect(m_stretch, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CollectionStyleDialog::updateImageVisibility);
    connect(m_ramp, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CollectionStyleDialog::updateImageVisibility);
    connect(m_tailTrim, &QCheckBox::toggled,
            this, &CollectionStyleDialog::updateImageVisibility);
    connect(m_trimLeft, &QCheckBox::toggled,
            this, &CollectionStyleDialog::updateImageVisibility);
    connect(m_trimRight, &QCheckBox::toggled,
            this, &CollectionStyleDialog::updateImageVisibility);
    return widget;
}

void CollectionStyleDialog::loadInitialStyle()
{
    m_initialData = CollectionStyleFile::load(m_collection.dirPath);

    if (m_collection.collectionClass == "point_collection") {
        PointLayerStyle style;
        style.symbol = PointSymbol::Cross;
        style.color = Qt::magenta;
        style.size = 10.0;
        if (m_initialData.kind == CollectionStyleData::Kind::Point)
            style = m_initialData.pointStyle;

        m_colorButton->setProperty("color", style.color);
        m_pointSize->setValue(style.size);
        m_symbol->setCurrentIndex(m_symbol->findData(static_cast<int>(style.symbol)));
        int labelIndex = m_labelField->findData(style.labelField);
        if (labelIndex < 0 && !style.labelField.isEmpty()) {
            m_labelField->addItem(style.labelField, style.labelField);
            labelIndex = m_labelField->count() - 1;
        }
        m_labelField->setCurrentIndex(qMax(0, labelIndex));
        m_labelSize->setValue(style.labelSize);
        m_labelSize->setEnabled(m_labelField->currentIndex() > 0);
        updateColorButton();
        return;
    }

    ImageLayerStyle style;
    style.render.stretchMode = StretchMode::None;
    style.render.tailTrim = false;
    if (m_collection.collectionClass == "preprocessed_image_collection") {
        style.render.mode = RasterRenderMode::Rgb;
    } else if (m_collection.collectionClass == "processed_image_collection") {
        style.render.colorRamp = ColorRamp::GreenMagenta;
    } else if (m_collection.collectionClass == "segmentation_collection") {
        style.render.colorRamp = ColorRamp::Categorical;
    }
    if (m_initialData.kind == CollectionStyleData::Kind::Image)
        style = m_initialData.imageStyle;

    m_opacity->setValue(style.opacity);
    const int blendIndex =
        m_blend->findData(static_cast<int>(style.blendMode));
    m_blend->setCurrentIndex(blendIndex >= 0 ? blendIndex : 0);
    m_mode->setCurrentIndex(m_mode->findData(static_cast<int>(style.render.mode)));
    m_ramp->setCurrentIndex(m_ramp->findData(static_cast<int>(style.render.colorRamp)));
    m_stretch->setCurrentIndex(m_stretch->findData(static_cast<int>(style.render.stretchMode)));
    m_gamma->setValue(style.render.gamma);
    m_band->setValue(style.render.band + 1);
    m_redBand->setValue(style.render.redBand + 1);
    m_greenBand->setValue(style.render.greenBand + 1);
    m_blueBand->setValue(style.render.blueBand + 1);
    m_tailTrim->setChecked(style.render.tailTrim);
    m_trimLeft->setChecked(style.render.trimLeftTail);
    m_trimRight->setChecked(style.render.trimRightTail);
    m_trim->setValue(style.render.trimPercent);
    updateImageVisibility();
}

CollectionStyleData CollectionStyleDialog::pointData() const
{
    CollectionStyleData data;
    data.kind = CollectionStyleData::Kind::Point;
    data.pointStyle.color = m_colorButton->property("color").value<QColor>();
    data.pointStyle.size = m_pointSize->value();
    data.pointStyle.symbol =
        static_cast<PointSymbol>(m_symbol->currentData().toInt());
    data.pointStyle.labelField = m_labelField->currentData().toString();
    data.pointStyle.labelSize = m_labelSize->value();
    return data;
}

CollectionStyleData CollectionStyleDialog::imageData() const
{
    CollectionStyleData data;
    data.kind = CollectionStyleData::Kind::Image;
    data.imageStyle.opacity = m_opacity->value();
    data.imageStyle.blendMode =
        static_cast<ImageBlendMode>(m_blend->currentData().toInt());
    RasterRenderStyle &render = data.imageStyle.render;
    render.mode = static_cast<RasterRenderMode>(m_mode->currentData().toInt());
    render.colorRamp = static_cast<ColorRamp>(m_ramp->currentData().toInt());
    render.stretchMode = static_cast<StretchMode>(m_stretch->currentData().toInt());
    render.gamma = m_gamma->value();
    render.band = m_band->value() - 1;
    render.redBand = m_redBand->value() - 1;
    render.greenBand = m_greenBand->value() - 1;
    render.blueBand = m_blueBand->value() - 1;
    render.tailTrim = m_tailTrim->isChecked();
    render.trimLeftTail = m_trimLeft->isChecked();
    render.trimRightTail = m_trimRight->isChecked();
    render.trimPercent = m_trim->value();
    return data;
}

void CollectionStyleDialog::saveStyle()
{
    QString errorMessage;
    const CollectionStyleData data =
        m_collection.collectionClass == "point_collection"
        ? pointData() : imageData();
    if (!CollectionStyleFile::save(
            m_collection.dirPath, data, &errorMessage)) {
        QMessageBox::warning(this, "Collection Style", errorMessage);
        return;
    }
    accept();
}

void CollectionStyleDialog::resetStyle()
{
    QString errorMessage;
    if (!CollectionStyleFile::remove(
            m_collection.dirPath, &errorMessage)) {
        QMessageBox::warning(this, "Collection Style", errorMessage);
        return;
    }
    accept();
}

void CollectionStyleDialog::choosePointColor()
{
    const QColor current =
        m_colorButton->property("color").value<QColor>();
    const QColor color = QColorDialog::getColor(
        current, this, "Point color");
    if (!color.isValid())
        return;
    m_colorButton->setProperty("color", color);
    updateColorButton();
}

void CollectionStyleDialog::updateColorButton()
{
    const QColor color =
        m_colorButton->property("color").value<QColor>();
    m_colorButton->setText(color.name(QColor::HexRgb));
    m_colorButton->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(color.name(QColor::HexRgb)));
}

void CollectionStyleDialog::updateImageVisibility()
{
    if (!m_mode)
        return;
    const bool rgb = m_mode->currentData().toInt()
        == static_cast<int>(RasterRenderMode::Rgb);
    const bool categorical = !rgb && m_ramp->currentData().toInt()
        == static_cast<int>(ColorRamp::Categorical);
    const bool gamma = m_stretch->currentData().toInt()
        == static_cast<int>(StretchMode::Gamma);

    m_ramp->setEnabled(!rgb);
    m_band->setEnabled(!rgb);
    m_redBand->setEnabled(rgb);
    m_greenBand->setEnabled(rgb);
    m_blueBand->setEnabled(rgb);
    m_stretch->setEnabled(!categorical);
    m_gamma->setEnabled(!categorical && gamma);
    m_tailTrim->setEnabled(!categorical);
    const bool trimEnabled = !categorical && m_tailTrim->isChecked();
    m_trimLeft->setEnabled(trimEnabled);
    m_trimRight->setEnabled(trimEnabled);
    m_trim->setEnabled(trimEnabled
                       && (m_trimLeft->isChecked() || m_trimRight->isChecked()));
}

QStringList CollectionStyleDialog::pointLabelFields() const
{
    QDir directory(m_collection.dirPath);
    const QStringList csvFiles = directory.entryList(
        {"*.csv", "*.CSV"}, QDir::Files, QDir::Name);
    for (const QString &csvFile : csvFiles) {
        const PointCollection collection =
            PointCollection::load(directory.filePath(csvFile));
        const QStringList columns = collection.columnNames();
        if (columns.size() > 2)
            return columns.mid(2);
    }
    return {};
}
