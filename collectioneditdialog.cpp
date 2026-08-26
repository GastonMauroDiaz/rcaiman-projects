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
    populateParentCombo();
    populateRelatedCombo();
    setComboToReference(m_parentCombo, m_collection.parent);
    setComboToReference(m_relatedCombo, m_collection.related);
    updateOkButton();
}

QString CollectionEditDialog::alias() const
{
    return m_aliasEdit ? m_aliasEdit->text().trimmed() : QString();
}

QString CollectionEditDialog::parentReference() const
{
    if (hasParentField() && m_parentCombo)
        return m_parentCombo->currentData(Qt::UserRole).toString();
    return {};
}

QString CollectionEditDialog::relatedReference() const
{
    if (hasRelatedField() && m_relatedCombo)
        return m_relatedCombo->currentData(Qt::UserRole).toString();
    return {};
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
    auto *classLabel = new QLabel(m_collection.collectionClass, identityGroup);
    classLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    identityLayout->addRow("Class:", classLabel);
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

    m_parentWidget = new QWidget(metadataGroup);
    auto *parentLayout = new QHBoxLayout(m_parentWidget);
    parentLayout->setContentsMargins(0, 0, 0, 0);
    m_parentCombo = new QComboBox(m_parentWidget);
    parentLayout->addWidget(m_parentCombo);
    formLayout->addRow("Parent:", m_parentWidget);

    m_relatedWidget = new QWidget(metadataGroup);
    auto *relatedLayout = new QHBoxLayout(m_relatedWidget);
    relatedLayout->setContentsMargins(0, 0, 0, 0);
    m_relatedCombo = new QComboBox(m_relatedWidget);
    relatedLayout->addWidget(m_relatedCombo);
    formLayout->addRow("Related:", m_relatedWidget);

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
    connect(m_parentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CollectionEditDialog::updateOkButton);
    connect(m_relatedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
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
    m_parentWidget->setVisible(hasParentField());
    m_relatedWidget->setVisible(hasRelatedField());
    m_registryWidget->setVisible(hasRegistryField());
    m_scriptWidget->setVisible(true);

    QString text;
    if (collectionClass == "preprocessed_image_collection") {
        text = "Primary TIFF collection for visualization and geometry. "
               "Registry and script are optional.";
    } else if (collectionClass == "processed_image_collection") {
        text = "Derived TIFF collection. Parent is optional; set it when this "
               "collection belongs to a specific image branch.";
    } else if (collectionClass == "point_collection") {
        text = "Point CSV collection. Parent is required because points need "
               "an image geometry. Related segmentation is optional.";
    } else if (collectionClass == "segmentation_collection") {
        text = "Segmentation TIFF collection. Parent is required because masks "
               "refer to an image geometry.";
    } else {
        text = "General project collection. Registry and script are optional.";
    }

    const QStringList parentClasses = compatibleClasses(true);
    if ((collectionClass == "point_collection"
         || collectionClass == "segmentation_collection")
        && !hasCompatibleCollection(parentClasses)) {
        text += "\n\nNo compatible parent collection is registered yet. "
                "Register a preprocessed or processed image collection first.";
    }

    m_contractLabel->setText(text);
}

void CollectionEditDialog::populateParentCombo()
{
    m_parentCombo->blockSignals(true);
    m_parentCombo->clear();
    const QStringList allowed = compatibleClasses(true);

    bool hasValid = false;
    const QString ownPath = normalizedPath(m_collection.dirPath);
    for (const Collection &collection : std::as_const(m_collections)) {
        if (normalizedPath(collection.dirPath) == ownPath)
            continue;
        if (allowed.contains(collection.collectionClass, Qt::CaseInsensitive)) {
            m_parentCombo->addItem(collectionLabel(collection),
                                   collection.dirPath);
            hasValid = true;
        }
    }
    if (hasValid)
        m_parentCombo->insertSeparator(m_parentCombo->count());
    m_parentCombo->addItem("(none)", QString());
    m_parentCombo->blockSignals(false);
}

void CollectionEditDialog::populateRelatedCombo()
{
    m_relatedCombo->blockSignals(true);
    m_relatedCombo->clear();
    const QStringList allowed = compatibleClasses(false);

    bool hasValid = false;
    const QString ownPath = normalizedPath(m_collection.dirPath);
    for (const Collection &collection : std::as_const(m_collections)) {
        if (normalizedPath(collection.dirPath) == ownPath)
            continue;
        if (allowed.contains(collection.collectionClass, Qt::CaseInsensitive)) {
            m_relatedCombo->addItem(collectionLabel(collection),
                                    collection.dirPath);
            hasValid = true;
        }
    }
    if (hasValid)
        m_relatedCombo->insertSeparator(m_relatedCombo->count());
    m_relatedCombo->addItem("(none)", QString());
    m_relatedCombo->blockSignals(false);
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

bool CollectionEditDialog::hasParentField() const
{
    const QString collectionClass = m_collection.collectionClass;
    return collectionClass == "processed_image_collection"
        || collectionClass == "point_collection"
        || collectionClass == "segmentation_collection";
}

bool CollectionEditDialog::hasRelatedField() const
{
    return m_collection.collectionClass == "point_collection";
}

bool CollectionEditDialog::hasRegistryField() const
{
    const QString collectionClass = m_collection.collectionClass;
    return collectionClass == "preprocessed_image_collection"
        || collectionClass == "other";
}

bool CollectionEditDialog::requiresParent() const
{
    const QString collectionClass = m_collection.collectionClass;
    return collectionClass == "point_collection"
        || collectionClass == "segmentation_collection";
}

QStringList CollectionEditDialog::compatibleClasses(bool asParent) const
{
    const QString targetClass = m_collection.collectionClass;
    if (targetClass == "point_collection") {
        if (asParent)
            return {"preprocessed_image_collection", "processed_image_collection"};
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
    return {};
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
    if (requiresParent())
        enableOk = enableOk && !parentReference().isEmpty();

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
