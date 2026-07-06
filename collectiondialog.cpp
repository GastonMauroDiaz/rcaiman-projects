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

#include "collectiondialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

CollectionDialog::CollectionDialog(const QString &projectRootPath,
                                   const QVector<Collection> &existingCollections,
                                   const QString &preselectedDirectory,
                                   QWidget *parent)
    : QDialog(parent)
    , m_projectRootPath(projectRootPath)
    , m_existingCollections(existingCollections)
{
    setWindowTitle("Register collection");
    setMinimumWidth(480);

    setupUI();

    if (!preselectedDirectory.isEmpty()) {
        m_directoryEdit->setText(preselectedDirectory);
        const bool ok = preselectedDirectory.startsWith(m_projectRootPath) && preselectedDirectory != m_projectRootPath;
        if (m_buttonBox) {
            if (auto *okBtn = m_buttonBox->button(QDialogButtonBox::Ok))
                okBtn->setEnabled(ok);
        }
    }

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CollectionDialog::onClassChanged);

    connect(m_registryEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        Q_UNUSED(this);
        onClassChanged(m_typeCombo->currentIndex());
    });
    connect(m_parentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CollectionDialog::updateOkButton);

    updateFieldVisibility();
    onClassChanged(m_typeCombo->currentIndex());
}

QString CollectionDialog::selectedDirectory() const
{
    return m_directoryEdit->text();
}

QString CollectionDialog::collectionClass() const
{
    return m_typeCombo->currentText();
}

QString CollectionDialog::parent() const
{
    if (m_parentCombo)
        return m_parentCombo->currentData(Qt::UserRole).toString();
    return {};
}

QString CollectionDialog::related() const
{
    if (m_relatedCombo)
        return m_relatedCombo->currentData(Qt::UserRole).toString();
    return {};
}

QString CollectionDialog::registry() const
{
    if (!m_registryEdit || !m_typeCombo)
        return {};

    const QString collectionClass = m_typeCombo->currentText();
    if (collectionClass == "preprocessed_image_collection"
        || collectionClass == "other") {
        return m_registryEdit->text().trimmed();
    }
    return {};
}

QString CollectionDialog::script() const
{
    if (m_scriptEdit)
        return m_scriptEdit->text();
    return {};
}

void CollectionDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    // Directory selection
    auto *dirGroup = new QGroupBox("Collection folder");
    auto *dirLayout = new QHBoxLayout(dirGroup);
    m_directoryEdit = new QLineEdit(this);
    m_directoryEdit->setReadOnly(true);
    m_directoryEdit->setPlaceholderText("Select a subfolder inside the project...");
    auto *browseBtn = new QPushButton("Browse...", this);
    dirLayout->addWidget(m_directoryEdit);
    dirLayout->addWidget(browseBtn);
    mainLayout->addWidget(dirGroup);

    // Form
    auto *formGroup = new QGroupBox("Collection metadata");
    auto *formLayout = new QFormLayout(formGroup);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItems({
        "preprocessed_image_collection",
        "processed_image_collection",
        "point_collection",
        "segmentation_collection",
        "other"
    });
    formLayout->addRow("Collection class:", m_typeCombo);

    m_parentWidget = new QWidget(this);
    m_parentCombo = new QComboBox(m_parentWidget);
    auto *parentLayout = new QHBoxLayout(m_parentWidget);
    parentLayout->setContentsMargins(0, 0, 0, 0);
    parentLayout->addWidget(m_parentCombo);
    m_parentWidget->setLayout(parentLayout);
    formLayout->addRow("Parent:", m_parentWidget);

    m_relatedWidget = new QWidget(this);
    m_relatedCombo = new QComboBox(m_relatedWidget);
    auto *relatedLayout = new QHBoxLayout(m_relatedWidget);
    relatedLayout->setContentsMargins(0, 0, 0, 0);
    relatedLayout->addWidget(m_relatedCombo);
    m_relatedWidget->setLayout(relatedLayout);
    formLayout->addRow("Related:", m_relatedWidget);

    m_registryWidget = new QWidget(this);
    auto *regLayout = new QHBoxLayout(m_registryWidget);
    regLayout->setContentsMargins(0, 0, 0, 0);
    m_registryEdit = new QLineEdit(m_registryWidget);
    m_registryEdit->setReadOnly(true);
    auto *regBtn = new QPushButton("Browse...", m_registryWidget);
    regLayout->addWidget(m_registryEdit);
    regLayout->addWidget(regBtn);
    m_registryWidget->setLayout(regLayout);
    formLayout->addRow("Registry:", m_registryWidget);

    m_scriptWidget = new QWidget(this);
    auto *scrLayout = new QHBoxLayout(m_scriptWidget);
    scrLayout->setContentsMargins(0, 0, 0, 0);
    m_scriptEdit = new QLineEdit(m_scriptWidget);
    m_scriptEdit->setReadOnly(true);
    auto *scrBtn = new QPushButton("Browse...", m_scriptWidget);
    scrLayout->addWidget(m_scriptEdit);
    scrLayout->addWidget(scrBtn);
    m_scriptWidget->setLayout(scrLayout);
    formLayout->addRow("Script:", m_scriptWidget);

    mainLayout->addWidget(formGroup);

    auto *contractGroup = new QGroupBox("Collection contract");
    auto *contractLayout = new QVBoxLayout(contractGroup);
    m_contractLabel = new QLabel(contractGroup);
    m_contractLabel->setWordWrap(true);
    m_contractLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_contractLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    contractLayout->addWidget(m_contractLabel);
    mainLayout->addWidget(contractGroup);

    // Buttons
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    mainLayout->addWidget(m_buttonBox);

    // Connections
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, "Select a subfolder", m_projectRootPath);
        if (!dir.isEmpty()) {
            m_directoryEdit->setText(dir);
            updateOkButton();
        }
    });

    connect(regBtn, &QPushButton::clicked, this, &CollectionDialog::browseRegistry);
    connect(scrBtn, &QPushButton::clicked, this, &CollectionDialog::browseScript);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CollectionDialog::onClassChanged(int index)
{
    Q_UNUSED(index);
    updateFieldVisibility();
    populateParentCombo();
    populateRelatedCombo();
    updateContractText();
    updateOkButton();
}

void CollectionDialog::updateFieldVisibility()
{
    const QString collectionClass = m_typeCombo->currentText();

    if (collectionClass == "preprocessed_image_collection") {
        m_parentWidget->setVisible(false);
        m_relatedWidget->setVisible(false);
        m_registryWidget->setVisible(true);
        m_scriptWidget->setVisible(true);
    } else if (collectionClass == "processed_image_collection") {
        m_parentWidget->setVisible(true);
        m_relatedWidget->setVisible(false);
        m_registryWidget->setVisible(false);
        m_scriptWidget->setVisible(true);
    } else if (collectionClass == "point_collection") {
        m_parentWidget->setVisible(true);
        m_relatedWidget->setVisible(true);
        m_registryWidget->setVisible(false);
        m_scriptWidget->setVisible(true);
    } else if (collectionClass == "segmentation_collection") {
        m_parentWidget->setVisible(true);
        m_relatedWidget->setVisible(false);
        m_registryWidget->setVisible(false);
        m_scriptWidget->setVisible(true);
    } else {
        m_parentWidget->setVisible(false);
        m_relatedWidget->setVisible(false);
        m_registryWidget->setVisible(true);
        m_scriptWidget->setVisible(true);
    }
}

void CollectionDialog::populateParentCombo()
{
    m_parentCombo->blockSignals(true);
    m_parentCombo->clear();
    const QString collectionClass = m_typeCombo->currentText();
    const QStringList allowed = compatibleClasses(collectionClass, true);

    bool hasValid = false;
    for (const Collection &col : m_existingCollections) {
        if (allowed.contains(col.collectionClass, Qt::CaseInsensitive)) {
            m_parentCombo->addItem(col.dirName, col.dirPath);
            hasValid = true;
        }
    }
    if (hasValid) {
        m_parentCombo->insertSeparator(m_parentCombo->count());
    }
    m_parentCombo->addItem("(none)", QString());
    m_parentCombo->blockSignals(false);
}

void CollectionDialog::populateRelatedCombo()
{
    m_relatedCombo->blockSignals(true);
    m_relatedCombo->clear();
    const QString collectionClass = m_typeCombo->currentText();
    const QStringList allowed = compatibleClasses(collectionClass, false);

    bool hasValid = false;
    for (const Collection &col : m_existingCollections) {
        if (allowed.contains(col.collectionClass, Qt::CaseInsensitive)) {
            m_relatedCombo->addItem(col.dirName, col.dirPath);
            hasValid = true;
        }
    }
    if (hasValid) {
        m_relatedCombo->insertSeparator(m_relatedCombo->count());
    }
    m_relatedCombo->addItem("(none)", QString());
    m_relatedCombo->blockSignals(false);
}

QStringList CollectionDialog::compatibleClasses(const QString &targetClass, bool asParent) const
{
    if (targetClass == "point_collection") {
        if (asParent)
            return {"preprocessed_image_collection", "processed_image_collection"};
        else
            return {"segmentation_collection"};
    }
    if (targetClass == "segmentation_collection") {
        if (asParent)
            return {"preprocessed_image_collection", "processed_image_collection"};
    }
    if (targetClass == "processed_image_collection") {
        if (asParent)
            return {"preprocessed_image_collection", "processed_image_collection"};
    }
    if (targetClass == "preprocessed_image_collection") {
        Q_UNUSED(asParent);
    }
    return {};
}

bool CollectionDialog::hasCompatibleCollection(const QStringList &classes) const
{
    for (const Collection &col : m_existingCollections) {
        if (classes.contains(col.collectionClass, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

void CollectionDialog::updateContractText()
{
    if (!m_contractLabel || !m_typeCombo)
        return;

    const QString collectionClass = m_typeCombo->currentText();
    QString text;

    if (collectionClass == "preprocessed_image_collection") {
        text = "Primary TIFF collection for visualization and geometry. "
               "No parent required. Registry is optional; if provided, this "
               "collection can be validated against RAW files. Script is optional.";
    } else if (collectionClass == "processed_image_collection") {
        text = "Derived TIFF collection. Parent is optional. Set a parent "
               "preprocessed or processed image collection if this collection "
               "should autoload with that image branch. Script is optional.";
    } else if (collectionClass == "point_collection") {
        text = "Point CSV collection. Parent is required and must be a "
               "preprocessed_image_collection or processed_image_collection "
               "because points need an image geometry. Related segmentation "
               "is optional.";
    } else if (collectionClass == "segmentation_collection") {
        text = "Segmentation TIFF collection. Parent is required and must be a "
               "preprocessed_image_collection or processed_image_collection "
               "because it refers to the segmentation of an image plane.";
    } else {
        text = "General project collection for notes, documents, or auxiliary "
               "files. No parent required.";
    }

    const QStringList parentClasses =
        compatibleClasses(collectionClass, true);
    if ((collectionClass == "point_collection"
         || collectionClass == "segmentation_collection")
        && !hasCompatibleCollection(parentClasses)) {
        text += "\n\nNo compatible parent collection is registered yet. "
                "Register a preprocessed or processed image collection first.";
    }

    m_contractLabel->setText(text);
}

void CollectionDialog::updateOkButton()
{
    if (!m_buttonBox)
        return;

    const QString collectionClass = m_typeCombo->currentText();
    const QString projectRoot =
        QDir::cleanPath(QDir::fromNativeSeparators(m_projectRootPath));
    const QString selectedDirectory =
        QDir::cleanPath(QDir::fromNativeSeparators(m_directoryEdit->text()));
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseSensitive;
#endif
    const bool hasDirectory =
        !selectedDirectory.isEmpty()
        && selectedDirectory.compare(projectRoot, pathCaseSensitivity) != 0
        && selectedDirectory.startsWith(
            projectRoot + QLatin1Char('/'), pathCaseSensitivity);
    bool enableOk = false;

    if (collectionClass == "preprocessed_image_collection") {
        enableOk = hasDirectory;
    } else if (collectionClass == "other") {
        enableOk = hasDirectory;
    } else if (collectionClass == "processed_image_collection") {
        enableOk = hasDirectory;
    } else if (collectionClass == "point_collection" || collectionClass == "segmentation_collection") {
        enableOk = hasDirectory
            && !m_parentCombo->currentData(Qt::UserRole).toString().isEmpty();
    }

    if (auto *okBtn = m_buttonBox->button(QDialogButtonBox::Ok))
        okBtn->setEnabled(enableOk);
}

void CollectionDialog::browseRegistry()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, "Select registry file", m_projectRootPath);
    if (!filePath.isEmpty()) {
        m_registryEdit->setText(filePath);
    }
}

void CollectionDialog::browseScript()
{
    const QString scriptDirectory = QDir(m_projectRootPath).filePath("scripts");
    const QString initialDirectory = QDir(scriptDirectory).exists()
        ? scriptDirectory
        : m_projectRootPath;

    const QString filePath = QFileDialog::getOpenFileName(
        this, "Select script file", initialDirectory);
    if (!filePath.isEmpty()) {
        m_scriptEdit->setText(filePath);
    }
}
