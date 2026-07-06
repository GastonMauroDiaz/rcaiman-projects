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

#ifndef COLLECTIONSTYLEDIALOG_H
#define COLLECTIONSTYLEDIALOG_H

#include "collectionstylefile.h"
#include "project.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;
class QStackedWidget;

class CollectionStyleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CollectionStyleDialog(const Collection &collection,
                                   QWidget *parent = nullptr);

private:
    QWidget *createPointEditor();
    QWidget *createImageEditor();
    CollectionStyleData pointData() const;
    CollectionStyleData imageData() const;
    void loadInitialStyle();
    void saveStyle();
    void resetStyle();
    void choosePointColor();
    void updateColorButton();
    void updateImageVisibility();
    QStringList pointLabelFields() const;

    Collection m_collection;
    CollectionStyleData m_initialData;

    QPushButton *m_colorButton = nullptr;
    QDoubleSpinBox *m_pointSize = nullptr;
    QComboBox *m_symbol = nullptr;
    QComboBox *m_labelField = nullptr;
    QDoubleSpinBox *m_labelSize = nullptr;

    QDoubleSpinBox *m_opacity = nullptr;
    QComboBox *m_mode = nullptr;
    QComboBox *m_ramp = nullptr;
    QComboBox *m_stretch = nullptr;
    QDoubleSpinBox *m_gamma = nullptr;
    QSpinBox *m_band = nullptr;
    QSpinBox *m_redBand = nullptr;
    QSpinBox *m_greenBand = nullptr;
    QSpinBox *m_blueBand = nullptr;
    QCheckBox *m_tailTrim = nullptr;
    QDoubleSpinBox *m_trim = nullptr;
};

#endif // COLLECTIONSTYLEDIALOG_H
