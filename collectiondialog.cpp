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
                                   QWidget *parent)
    : QDialog(parent)
    , m_projectRootPath(projectRootPath)
    , m_existingCollections(existingCollections)
{
    setWindowTitle("Register collection");
    setMinimumWidth(480);

    setupUI();

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CollectionDialog::onTypeChanged);

    connect(m_registryEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        Q_UNUSED(this);
        onTypeChanged(m_typeCombo->currentIndex());
    });
    connect(m_parentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CollectionDialog::updateOkButton);

    updateFieldVisibility();
    onTypeChanged(m_typeCombo->currentIndex());
}

QString CollectionDialog::selectedDirectory() const
{
    return m_directoryEdit->text();
}

QString CollectionDialog::collectionType() const
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
    if (m_registryEdit)
        return m_registryEdit->text();
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
            bool ok = dir.startsWith(m_projectRootPath) && dir != m_projectRootPath;
            if (m_buttonBox) {
                if (auto *okBtn = m_buttonBox->button(QDialogButtonBox::Ok))
                    okBtn->setEnabled(ok);
            }
        }
    });

    connect(regBtn, &QPushButton::clicked, this, &CollectionDialog::browseRegistry);
    connect(scrBtn, &QPushButton::clicked, this, &CollectionDialog::browseScript);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CollectionDialog::onTypeChanged(int index)
{
    Q_UNUSED(index);
    updateFieldVisibility();
    populateParentCombo();
    populateRelatedCombo();
    updateOkButton();
}

void CollectionDialog::updateFieldVisibility()
{
    const QString type = m_typeCombo->currentText();

    if (type == "preprocessed_image_collection" || type == "processed_image_collection") {
        m_parentWidget->setVisible(false);
        m_relatedWidget->setVisible(false);
        m_registryWidget->setVisible(true);
        m_scriptWidget->setVisible(true);
    } else if (type == "point_collection") {
        m_parentWidget->setVisible(true);
        m_relatedWidget->setVisible(true);
        m_registryWidget->setVisible(false);
        m_scriptWidget->setVisible(true);
    } else if (type == "segmentation_collection") {
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
    const QString type = m_typeCombo->currentText();
    const QStringList allowed = compatibleTypes(type, true);

    bool hasValid = false;
    for (const Collection &col : m_existingCollections) {
        if (allowed.contains(col.type, Qt::CaseInsensitive)) {
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
    const QString type = m_typeCombo->currentText();
    const QStringList allowed = compatibleTypes(type, false);

    bool hasValid = false;
    for (const Collection &col : m_existingCollections) {
        if (allowed.contains(col.type, Qt::CaseInsensitive)) {
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

QStringList CollectionDialog::compatibleTypes(const QString &targetType, bool asParent) const
{
    if (targetType == "point_collection") {
        if (asParent)
            return {"preprocessed_image_collection", "processed_image_collection"};
        else
            return {"segmentation_collection"};
    }
    if (targetType == "segmentation_collection") {
        if (asParent)
            return {"preprocessed_image_collection", "processed_image_collection"};
    }
    if (targetType == "preprocessed_image_collection" || targetType == "processed_image_collection") {
        Q_UNUSED(asParent);
    }
    return {};
}

void CollectionDialog::updateOkButton()
{
    if (!m_buttonBox)
        return;

    const QString type = m_typeCombo->currentText();
    bool enableOk = false;

    if (type == "preprocessed_image_collection" || type == "processed_image_collection" || type == "other") {
        enableOk = !m_registryEdit->text().isEmpty();
    } else if (type == "point_collection" || type == "segmentation_collection") {
        enableOk = !m_parentCombo->currentData(Qt::UserRole).toString().isEmpty();
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
    const QString filePath = QFileDialog::getOpenFileName(
        this, "Select script file", m_projectRootPath);
    if (!filePath.isEmpty()) {
        m_scriptEdit->setText(filePath);
    }
}