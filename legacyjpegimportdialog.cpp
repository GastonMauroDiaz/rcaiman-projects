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

#include "legacyjpegimportdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfoList>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

int jpegCount(const QString &directory)
{
    return QDir(directory).entryInfoList(
        {"*.jpg", "*.jpeg", "*.JPG", "*.JPEG"},
        QDir::Files | QDir::NoSymLinks).size();
}

QString collectionLabel(const Collection &collection)
{
    return QStringLiteral("%1 (%2 JPEG%3)")
        .arg(collection.dirName)
        .arg(jpegCount(collection.dirPath))
        .arg(jpegCount(collection.dirPath) == 1 ? "" : "s");
}

}

LegacyJpegImportDialog::LegacyJpegImportDialog(
    const QVector<Collection> &sourceCollections,
    const QVector<Collection> &destinationCollections,
    QWidget *parent)
    : QDialog(parent)
    , m_sourceCollections(sourceCollections)
    , m_destinationCollections(destinationCollections)
{
    setWindowTitle("Import legacy JPEGs as TIFF");
    setMinimumWidth(520);

    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(
        "Convert legacy JPEG files from an `other` collection into RGB TIFF "
        "files in a preprocessed image collection. No gamma correction or "
        "RAW recovery is applied.",
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *form = new QFormLayout;
    m_sourceCombo = new QComboBox(this);
    for (qsizetype i = 0; i < m_sourceCollections.size(); ++i)
        m_sourceCombo->addItem(collectionLabel(m_sourceCollections.at(i)),
                               static_cast<int>(i));
    form->addRow("JPEG collection:", m_sourceCombo);

    m_destinationCombo = new QComboBox(this);
    for (qsizetype i = 0; i < m_destinationCollections.size(); ++i)
        m_destinationCombo->addItem(
            m_destinationCollections.at(i).dirName,
            static_cast<int>(i));
    form->addRow("TIFF collection:", m_destinationCombo);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Cancel, this);
    auto *importButton =
        buttons->addButton("Import", QDialogButtonBox::AcceptRole);
    importButton->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    connect(importButton, &QPushButton::clicked,
            this, &QDialog::accept);
}

Collection LegacyJpegImportDialog::sourceCollection() const
{
    if (!m_sourceCombo)
        return {};
    const int index = m_sourceCombo->currentData().toInt();
    if (index < 0 || index >= m_sourceCollections.size())
        return {};
    return m_sourceCollections.at(index);
}

Collection LegacyJpegImportDialog::destinationCollection() const
{
    if (!m_destinationCombo)
        return {};
    const int index = m_destinationCombo->currentData().toInt();
    if (index < 0 || index >= m_destinationCollections.size())
        return {};
    return m_destinationCollections.at(index);
}
