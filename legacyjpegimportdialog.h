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

#ifndef LEGACYJPEGIMPORTDIALOG_H
#define LEGACYJPEGIMPORTDIALOG_H

#include "project.h"

#include <QDialog>
#include <QVector>

class QComboBox;

class LegacyJpegImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LegacyJpegImportDialog(
        const QVector<Collection> &sourceCollections,
        const QVector<Collection> &destinationCollections,
        QWidget *parent = nullptr);

    Collection sourceCollection() const;
    Collection destinationCollection() const;

private:
    QVector<Collection> m_sourceCollections;
    QVector<Collection> m_destinationCollections;
    QComboBox *m_sourceCombo = nullptr;
    QComboBox *m_destinationCombo = nullptr;
};

#endif // LEGACYJPEGIMPORTDIALOG_H
