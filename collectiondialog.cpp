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
#include "collectionappearance.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

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
    connect(m_previousLevelCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CollectionDialog::updateOkButton);
    connect(m_imageFrameCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
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

QString CollectionDialog::previousLevel() const
{
    if (m_typeCombo
        && m_typeCombo->currentText()
            == "preprocessed_image_collection") {
        return m_previousLevelCombo->currentData(Qt::UserRole).toString();
    }
    return {};
}

QString CollectionDialog::imageFrame() const
{
    if (!m_typeCombo)
        return {};

    const QString collectionClass = m_typeCombo->currentText();
    if (collectionClass == "processed_image_collection"
        || collectionClass == "point_collection"
        || collectionClass == "segmentation_collection") {
        return m_imageFrameCombo->currentData(Qt::UserRole).toString();
    }
    return {};
}

QStringList CollectionDialog::builtFrom() const
{
    QStringList references;
    if (!m_typeCombo || m_typeCombo->currentText() == "other")
        return references;
    for (int row = 0; row < m_builtFromList->count(); ++row) {
        const QListWidgetItem *item = m_builtFromList->item(row);
        if (item->checkState() == Qt::Checked)
            references.append(item->data(Qt::UserRole).toString());
    }
    return references;
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

QString CollectionDialog::alias() const
{
    if (m_aliasEdit)
        return m_aliasEdit->text().trimmed();
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
    m_typeCombo->setIconSize(QSize(14, 14));
    for (const QString &collectionClass : CollectionAppearance::classNames()) {
        m_typeCombo->addItem(
            CollectionAppearance::markerIcon(collectionClass),
            collectionClass);
    }
    formLayout->addRow("Collection class:", m_typeCombo);

    m_aliasEdit = new QLineEdit(this);
    m_aliasEdit->setMaxLength(40);
    m_aliasEdit->setPlaceholderText("Optional display alias");
    m_aliasEdit->setToolTip(
        "Optional display alias. It does not change the folder name, "
        "sorting, scripts, or collection links.");
    formLayout->addRow("Alias:", m_aliasEdit);

    m_previousLevelWidget = new QWidget(this);
    m_previousLevelCombo = new QComboBox(m_previousLevelWidget);
    m_previousLevelCombo->setIconSize(QSize(14, 14));
    auto *previousLevelLayout = new QHBoxLayout(m_previousLevelWidget);
    previousLevelLayout->setContentsMargins(0, 0, 0, 0);
    previousLevelLayout->addWidget(m_previousLevelCombo);
    formLayout->addRow("Previous level:", m_previousLevelWidget);

    m_imageFrameWidget = new QWidget(this);
    m_imageFrameCombo = new QComboBox(m_imageFrameWidget);
    m_imageFrameCombo->setIconSize(QSize(14, 14));
    auto *imageFrameLayout = new QHBoxLayout(m_imageFrameWidget);
    imageFrameLayout->setContentsMargins(0, 0, 0, 0);
    imageFrameLayout->addWidget(m_imageFrameCombo);
    formLayout->addRow("Image frame:", m_imageFrameWidget);

    m_builtFromWidget = new QWidget(this);
    auto *builtFromLayout = new QVBoxLayout(m_builtFromWidget);
    builtFromLayout->setContentsMargins(0, 0, 0, 0);
    m_builtFromList = new QListWidget(m_builtFromWidget);
    m_builtFromList->setIconSize(QSize(14, 14));
    m_builtFromList->setMinimumHeight(92);
    m_builtFromList->setToolTip(
        "Optional source collections used to construct this collection.");
    builtFromLayout->addWidget(m_builtFromList);
    formLayout->addRow("Built from:", m_builtFromWidget);

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
    connect(m_aliasEdit, &QLineEdit::textChanged,
            this, &CollectionDialog::updateOkButton);
    connect(m_builtFromList, &QListWidget::itemChanged,
            this, &CollectionDialog::updateOkButton);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CollectionDialog::onClassChanged(int index)
{
    Q_UNUSED(index);
    updateFieldVisibility();
    populatePreviousLevelCombo();
    populateImageFrameCombo();
    populateBuiltFromList();
    updateContractText();
    updateOkButton();
}

void CollectionDialog::updateFieldVisibility()
{
    const QString collectionClass = m_typeCombo->currentText();
    const bool isPreprocessed =
        collectionClass == "preprocessed_image_collection";
    const bool usesFrame =
        collectionClass == "processed_image_collection"
        || collectionClass == "point_collection"
        || collectionClass == "segmentation_collection";
    const bool isOther = collectionClass == "other";

    m_previousLevelWidget->setVisible(isPreprocessed);
    m_imageFrameWidget->setVisible(usesFrame);
    m_builtFromWidget->setVisible(!isOther);
    m_registryWidget->setVisible(isPreprocessed || isOther);
    m_scriptWidget->setVisible(true);
}

void CollectionDialog::populatePreviousLevelCombo()
{
    m_previousLevelCombo->blockSignals(true);
    m_previousLevelCombo->clear();
    for (const Collection &collection : std::as_const(m_existingCollections)) {
        if (collection.collectionClass == "preprocessed_image_collection") {
            m_previousLevelCombo->addItem(
                CollectionAppearance::markerIcon(collection.collectionClass),
                collection.alias.isEmpty()
                    ? collection.dirName
                    : QStringLiteral("%1 | %2")
                          .arg(collection.dirName, collection.alias),
                collection.dirPath);
        }
    }
    if (m_previousLevelCombo->count() > 0)
        m_previousLevelCombo->insertSeparator(m_previousLevelCombo->count());
    m_previousLevelCombo->addItem("(none - L0)", QString());
    m_previousLevelCombo->setCurrentIndex(
        m_previousLevelCombo->count() - 1);
    m_previousLevelCombo->blockSignals(false);
}

void CollectionDialog::populateImageFrameCombo()
{
    m_imageFrameCombo->blockSignals(true);
    m_imageFrameCombo->clear();
    for (const Collection &collection : std::as_const(m_existingCollections)) {
        if (collection.collectionClass == "preprocessed_image_collection") {
            m_imageFrameCombo->addItem(
                CollectionAppearance::markerIcon(collection.collectionClass),
                collection.alias.isEmpty()
                    ? collection.dirName
                    : QStringLiteral("%1 | %2")
                          .arg(collection.dirName, collection.alias),
                collection.dirPath);
        }
    }
    if (m_imageFrameCombo->count() > 0)
        m_imageFrameCombo->insertSeparator(m_imageFrameCombo->count());
    m_imageFrameCombo->addItem("(none)", QString());
    m_imageFrameCombo->setCurrentIndex(
        m_imageFrameCombo->count() - 1);
    m_imageFrameCombo->blockSignals(false);
}

void CollectionDialog::populateBuiltFromList()
{
    m_builtFromList->blockSignals(true);
    m_builtFromList->clear();
    for (const Collection &collection : std::as_const(m_existingCollections)) {
        if (collection.collectionClass == "other")
            continue;
        auto *item = new QListWidgetItem(
            CollectionAppearance::markerIcon(collection.collectionClass),
            collection.alias.isEmpty()
                ? collection.dirName
                : QStringLiteral("%1 | %2")
                      .arg(collection.dirName, collection.alias),
            m_builtFromList);
        item->setData(Qt::UserRole, collection.dirPath);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    m_builtFromList->blockSignals(false);
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
        text = "TIFF collection that defines an image frame. With no previous "
               "level it is L0; otherwise its level follows the selected "
               "preprocessed collection. Built from records additional provenance.";
    } else if (collectionClass == "processed_image_collection") {
        text = "Processed TIFF collection. Image frame is required and controls "
               "placement and autoload. Built from records the source collections.";
    } else if (collectionClass == "point_collection") {
        text = "Point CSV collection. Image frame is required because it defines "
               "how x and y_down are interpreted. Built from records provenance "
               "such as crop, uncrop, or automatic extraction.";
    } else if (collectionClass == "segmentation_collection") {
        text = "Segmentation TIFF collection. Image frame is required because "
               "the mask refers to a specific image plane. Built from is optional.";
    } else {
        text = "General project collection for notes, documents, results, or auxiliary "
               "files. Other collections are independent of the image-frame graph.";
    }

    const bool requiresFrame =
        collectionClass == "processed_image_collection"
        || collectionClass == "point_collection"
        || collectionClass == "segmentation_collection";
    if (requiresFrame
        && !hasCompatibleCollection({"preprocessed_image_collection"})) {
        text += "\n\nNo image frame is available yet. Register a preprocessed "
                "image collection first.";
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
    const bool aliasOk = Project::collectionAliasIsSupported(
        m_aliasEdit ? m_aliasEdit->text() : QString());

    if (collectionClass == "preprocessed_image_collection") {
        enableOk = hasDirectory;
    } else if (collectionClass == "other") {
        enableOk = hasDirectory;
    } else if (collectionClass == "processed_image_collection"
               || collectionClass == "point_collection"
               || collectionClass == "segmentation_collection") {
        enableOk = hasDirectory
            && !m_imageFrameCombo->currentData(
                    Qt::UserRole).toString().isEmpty();
    }
    enableOk = enableOk && aliasOk;

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
