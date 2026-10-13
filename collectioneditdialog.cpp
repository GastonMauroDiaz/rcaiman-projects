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

#include "collectioneditdialog.h"
#include "collectionappearance.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString normalizedPath(const QString &path)
{
    if (path.trimmed().isEmpty())
        return {};

    QString normalized = QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

bool referencesSamePath(const QString &projectRootPath,
                        const QString &reference,
                        const QString &path)
{
    if (reference.trimmed().isEmpty() || path.trimmed().isEmpty())
        return false;

    return normalizedPath(Project::resolveReference(projectRootPath, reference))
        == normalizedPath(path);
}
}

CollectionEditDialog::CollectionEditDialog(
    const QString &projectRootPath,
    const Collection &collection,
    const QVector<Collection> &collections,
    QWidget *parent)
    : QDialog(parent)
    , m_projectRootPath(projectRootPath)
    , m_collection(collection)
    , m_collections(collections)
{
    setWindowTitle("Edit collection");
    setMinimumWidth(520);

    setupUI();
    updateFieldVisibility();
    populatePreviousLevelCombo();
    populateImageFrameCombo();
    populateBuiltFromList();
    setComboToReference(
        m_previousLevelCombo, m_collection.previousLevel);
    setComboToReference(m_imageFrameCombo, m_collection.imageFrame);
    updateOkButton();
}

QString CollectionEditDialog::alias() const
{
    return m_aliasEdit ? m_aliasEdit->text().trimmed() : QString();
}

QString CollectionEditDialog::previousLevelReference() const
{
    if (hasPreviousLevelField() && m_previousLevelCombo)
        return m_previousLevelCombo->currentData(Qt::UserRole).toString();
    return {};
}

QString CollectionEditDialog::imageFrameReference() const
{
    if (hasImageFrameField() && m_imageFrameCombo)
        return m_imageFrameCombo->currentData(Qt::UserRole).toString();
    return {};
}

QStringList CollectionEditDialog::builtFromReferences() const
{
    QStringList references;
    if (!hasBuiltFromField() || !m_builtFromList)
        return references;
    for (int row = 0; row < m_builtFromList->count(); ++row) {
        const QListWidgetItem *item = m_builtFromList->item(row);
        if (item->checkState() == Qt::Checked)
            references.append(item->data(Qt::UserRole).toString());
    }
    return references;
}

QString CollectionEditDialog::registry() const
{
    if (hasRegistryField() && m_registryEdit)
        return m_registryEdit->text().trimmed();
    return {};
}

QString CollectionEditDialog::script() const
{
    return m_scriptEdit ? m_scriptEdit->text().trimmed() : QString();
}

void CollectionEditDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    auto *identityGroup = new QGroupBox("Collection identity", this);
    auto *identityLayout = new QFormLayout(identityGroup);
    auto *folderLabel = new QLabel(m_collection.dirName, identityGroup);
    folderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    identityLayout->addRow("Folder:", folderLabel);
    auto *classWidget = new QWidget(identityGroup);
    auto *classLayout = new QHBoxLayout(classWidget);
    classLayout->setContentsMargins(0, 0, 0, 0);
    classLayout->setSpacing(4);
    auto *classMarker = new QLabel(classWidget);
    classMarker->setPixmap(
        CollectionAppearance::markerIcon(m_collection.collectionClass)
            .pixmap(14, 14));
    classMarker->setFixedSize(14, 14);
    auto *classLabel = new QLabel(m_collection.collectionClass, classWidget);
    classLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    classLayout->addWidget(classMarker);
    classLayout->addWidget(classLabel);
    classLayout->addStretch(1);
    identityLayout->addRow("Class:", classWidget);
    mainLayout->addWidget(identityGroup);

    auto *metadataGroup = new QGroupBox("Collection metadata", this);
    auto *formLayout = new QFormLayout(metadataGroup);

    m_aliasEdit = new QLineEdit(metadataGroup);
    m_aliasEdit->setMaxLength(40);
    m_aliasEdit->setText(m_collection.alias);
    m_aliasEdit->setPlaceholderText("Optional display alias");
    m_aliasEdit->setToolTip(
        "Optional display alias. It does not change the folder name, "
        "sorting, scripts, or collection links.");
    formLayout->addRow("Alias:", m_aliasEdit);

    m_previousLevelWidget = new QWidget(metadataGroup);
    auto *previousLevelLayout = new QHBoxLayout(m_previousLevelWidget);
    previousLevelLayout->setContentsMargins(0, 0, 0, 0);
    m_previousLevelCombo = new QComboBox(m_previousLevelWidget);
    m_previousLevelCombo->setIconSize(QSize(14, 14));
    previousLevelLayout->addWidget(m_previousLevelCombo);
    formLayout->addRow("Previous level:", m_previousLevelWidget);

    m_imageFrameWidget = new QWidget(metadataGroup);
    auto *imageFrameLayout = new QHBoxLayout(m_imageFrameWidget);
    imageFrameLayout->setContentsMargins(0, 0, 0, 0);
    m_imageFrameCombo = new QComboBox(m_imageFrameWidget);
    m_imageFrameCombo->setIconSize(QSize(14, 14));
    imageFrameLayout->addWidget(m_imageFrameCombo);
    formLayout->addRow("Image frame:", m_imageFrameWidget);

    m_builtFromWidget = new QWidget(metadataGroup);
    auto *builtFromLayout = new QVBoxLayout(m_builtFromWidget);
    builtFromLayout->setContentsMargins(0, 0, 0, 0);
    m_builtFromList = new QListWidget(m_builtFromWidget);
    m_builtFromList->setIconSize(QSize(14, 14));
    m_builtFromList->setMinimumHeight(92);
    m_builtFromList->setToolTip(
        "Optional source collections used to construct this collection.");
    builtFromLayout->addWidget(m_builtFromList);
    formLayout->addRow("Built from:", m_builtFromWidget);

    m_registryWidget = new QWidget(metadataGroup);
    auto *registryLayout = new QHBoxLayout(m_registryWidget);
    registryLayout->setContentsMargins(0, 0, 0, 0);
    m_registryEdit = new QLineEdit(m_registryWidget);
    m_registryEdit->setText(m_collection.registry);
    auto *registryButton = new QPushButton("Browse...", m_registryWidget);
    registryLayout->addWidget(m_registryEdit);
    registryLayout->addWidget(registryButton);
    formLayout->addRow("Registry:", m_registryWidget);

    m_scriptWidget = new QWidget(metadataGroup);
    auto *scriptLayout = new QHBoxLayout(m_scriptWidget);
    scriptLayout->setContentsMargins(0, 0, 0, 0);
    m_scriptEdit = new QLineEdit(m_scriptWidget);
    m_scriptEdit->setText(m_collection.script);
    auto *scriptButton = new QPushButton("Browse...", m_scriptWidget);
    scriptLayout->addWidget(m_scriptEdit);
    scriptLayout->addWidget(scriptButton);
    formLayout->addRow("Script:", m_scriptWidget);

    mainLayout->addWidget(metadataGroup);

    auto *contractGroup = new QGroupBox("Collection contract", this);
    auto *contractLayout = new QVBoxLayout(contractGroup);
    m_contractLabel = new QLabel(contractGroup);
    m_contractLabel->setWordWrap(true);
    m_contractLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    contractLayout->addWidget(m_contractLabel);
    mainLayout->addWidget(contractGroup);

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    connect(m_aliasEdit, &QLineEdit::textChanged,
            this, &CollectionEditDialog::updateOkButton);
    connect(m_previousLevelCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CollectionEditDialog::updateOkButton);
    connect(m_imageFrameCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CollectionEditDialog::updateOkButton);
    connect(m_builtFromList, &QListWidget::itemChanged,
            this, &CollectionEditDialog::updateOkButton);
    connect(m_registryEdit, &QLineEdit::textChanged,
            this, &CollectionEditDialog::updateOkButton);
    connect(m_scriptEdit, &QLineEdit::textChanged,
            this, &CollectionEditDialog::updateOkButton);
    connect(registryButton, &QPushButton::clicked,
            this, &CollectionEditDialog::browseRegistry);
    connect(scriptButton, &QPushButton::clicked,
            this, &CollectionEditDialog::browseScript);
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

void CollectionEditDialog::updateFieldVisibility()
{
    const QString collectionClass = m_collection.collectionClass;
    m_previousLevelWidget->setVisible(hasPreviousLevelField());
    m_imageFrameWidget->setVisible(hasImageFrameField());
    m_builtFromWidget->setVisible(hasBuiltFromField());
    m_registryWidget->setVisible(hasRegistryField());
    m_scriptWidget->setVisible(true);

    QString text;
    if (collectionClass == "preprocessed_image_collection") {
        text = "TIFF collection that defines an image frame. With no previous "
               "level it is L0; otherwise its level follows the selected "
               "preprocessed collection. Built from records additional provenance.";
    } else if (collectionClass == "processed_image_collection") {
        text = "Processed TIFF collection. Image frame is required and controls "
               "placement and autoload. Built from records source collections.";
    } else if (collectionClass == "point_collection") {
        text = "Point CSV collection. Image frame is required because it defines "
               "how x and y_down are interpreted. Built from records provenance.";
    } else if (collectionClass == "segmentation_collection") {
        text = "Segmentation TIFF collection. Image frame is required because "
               "the mask refers to a specific image plane. Built from is optional.";
    } else {
        text = "General project collection. Other collections are independent "
               "of the image-frame graph.";
    }

    if (requiresImageFrame()
        && !hasCompatibleCollection({"preprocessed_image_collection"})) {
        text += "\n\nNo image frame is available yet. Register a preprocessed "
                "image collection first.";
    }

    m_contractLabel->setText(text);
}

void CollectionEditDialog::populatePreviousLevelCombo()
{
    m_previousLevelCombo->blockSignals(true);
    m_previousLevelCombo->clear();

    bool hasValid = false;
    const QString ownPath = normalizedPath(m_collection.dirPath);
    for (const Collection &collection : std::as_const(m_collections)) {
        if (normalizedPath(collection.dirPath) == ownPath)
            continue;
        if (collection.collectionClass == "preprocessed_image_collection") {
            m_previousLevelCombo->addItem(
                CollectionAppearance::markerIcon(
                    collection.collectionClass),
                collectionLabel(collection), collection.dirPath);
            hasValid = true;
        }
    }
    if (hasValid)
        m_previousLevelCombo->insertSeparator(m_previousLevelCombo->count());
    m_previousLevelCombo->addItem("(none - L0)", QString());
    m_previousLevelCombo->blockSignals(false);
}

void CollectionEditDialog::populateImageFrameCombo()
{
    m_imageFrameCombo->blockSignals(true);
    m_imageFrameCombo->clear();

    bool hasValid = false;
    const QString ownPath = normalizedPath(m_collection.dirPath);
    for (const Collection &collection : std::as_const(m_collections)) {
        if (normalizedPath(collection.dirPath) == ownPath)
            continue;
        if (collection.collectionClass == "preprocessed_image_collection") {
            m_imageFrameCombo->addItem(
                CollectionAppearance::markerIcon(
                    collection.collectionClass),
                collectionLabel(collection), collection.dirPath);
            hasValid = true;
        }
    }
    if (hasValid)
        m_imageFrameCombo->insertSeparator(m_imageFrameCombo->count());
    m_imageFrameCombo->addItem("(none)", QString());
    m_imageFrameCombo->blockSignals(false);
}

void CollectionEditDialog::populateBuiltFromList()
{
    m_builtFromList->blockSignals(true);
    m_builtFromList->clear();
    const QString ownPath = normalizedPath(m_collection.dirPath);
    for (const Collection &collection : std::as_const(m_collections)) {
        if (normalizedPath(collection.dirPath) == ownPath
            || collection.collectionClass == "other") {
            continue;
        }
        auto *item = new QListWidgetItem(
            CollectionAppearance::markerIcon(collection.collectionClass),
            collectionLabel(collection), m_builtFromList);
        item->setData(Qt::UserRole, collection.dirPath);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        bool checked = false;
        for (const QString &reference : m_collection.builtFrom) {
            if (referencesSamePath(
                    m_projectRootPath, reference, collection.dirPath)) {
                checked = true;
                break;
            }
        }
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
    m_builtFromList->blockSignals(false);
}

void CollectionEditDialog::setComboToReference(QComboBox *combo,
                                               const QString &reference)
{
    if (!combo)
        return;

    if (!reference.trimmed().isEmpty()) {
        for (int i = 0; i < combo->count(); ++i) {
            if (referencesSamePath(
                    m_projectRootPath,
                    reference,
                    combo->itemData(i, Qt::UserRole).toString())) {
                combo->setCurrentIndex(i);
                return;
            }
        }
    }
    combo->setCurrentIndex(qMax(0, combo->count() - 1));
}

bool CollectionEditDialog::hasPreviousLevelField() const
{
    return m_collection.collectionClass == "preprocessed_image_collection";
}

bool CollectionEditDialog::hasImageFrameField() const
{
    const QString collectionClass = m_collection.collectionClass;
    return collectionClass == "processed_image_collection"
        || collectionClass == "point_collection"
        || collectionClass == "segmentation_collection";
}

bool CollectionEditDialog::hasBuiltFromField() const
{
    return m_collection.collectionClass != "other";
}

bool CollectionEditDialog::hasRegistryField() const
{
    const QString collectionClass = m_collection.collectionClass;
    return collectionClass == "preprocessed_image_collection"
        || collectionClass == "other";
}

bool CollectionEditDialog::requiresImageFrame() const
{
    const QString collectionClass = m_collection.collectionClass;
    return collectionClass == "processed_image_collection"
        || collectionClass == "point_collection"
        || collectionClass == "segmentation_collection";
}

QString CollectionEditDialog::collectionLabel(
    const Collection &collection) const
{
    if (collection.alias.isEmpty())
        return collection.dirName;
    return QStringLiteral("%1 | %2").arg(collection.dirName, collection.alias);
}

bool CollectionEditDialog::hasCompatibleCollection(
    const QStringList &classes) const
{
    const QString ownPath = normalizedPath(m_collection.dirPath);
    for (const Collection &collection : std::as_const(m_collections)) {
        if (normalizedPath(collection.dirPath) == ownPath)
            continue;
        if (classes.contains(collection.collectionClass, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

void CollectionEditDialog::updateOkButton()
{
    if (!m_buttonBox)
        return;

    const QString cleanAlias = alias();
    bool enableOk = cleanAlias.isEmpty()
        || Project::collectionAliasIsSupported(cleanAlias);
    if (requiresImageFrame())
        enableOk = enableOk && !imageFrameReference().isEmpty();

    if (auto *okButton = m_buttonBox->button(QDialogButtonBox::Ok))
        okButton->setEnabled(enableOk);
}

void CollectionEditDialog::browseRegistry()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, "Select registry file", m_projectRootPath);
    if (!filePath.isEmpty())
        m_registryEdit->setText(filePath);
}

void CollectionEditDialog::browseScript()
{
    const QString scriptDirectory = QDir(m_projectRootPath).filePath("scripts");
    const QString initialDirectory = QDir(scriptDirectory).exists()
        ? scriptDirectory
        : m_projectRootPath;

    const QString filePath = QFileDialog::getOpenFileName(
        this, "Select script file", initialDirectory);
    if (!filePath.isEmpty())
        m_scriptEdit->setText(filePath);
}
