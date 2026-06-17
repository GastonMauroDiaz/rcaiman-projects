#include "collectiondialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

CollectionDialog::CollectionDialog(const QString &projectRootPath,
                                   QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Register collection");
    setMinimumWidth(480);

    auto *mainLayout = new QVBoxLayout(this);

    // --- Directory selection ---
    auto *dirGroup = new QGroupBox("Collection folder");
    auto *dirLayout = new QHBoxLayout(dirGroup);
    m_directoryEdit = new QLineEdit(this);
    m_directoryEdit->setReadOnly(true);
    m_directoryEdit->setPlaceholderText("Select a subfolder inside the project...");
    auto *browseBtn = new QPushButton("Browse...", this);
    dirLayout->addWidget(m_directoryEdit);
    dirLayout->addWidget(browseBtn);
    mainLayout->addWidget(dirGroup);

    // --- Form ---
    auto *formGroup = new QGroupBox("Collection metadata");
    auto *formLayout = new QFormLayout(formGroup);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItems({
        "point_collection",
        "preprocessed_image_collection",
        "processed_image_collection",
        "binary_mask_collection",
        "segmentation_collection",
        "other"
    });
    formLayout->addRow("Collection class:", m_typeCombo);

    m_registryEdit = new QLineEdit(this);
    m_registryEdit->setPlaceholderText("e.g. Nikon_Coolpix5700.FCE9.CIEFAP");
    formLayout->addRow("Registry:", m_registryEdit);

    m_scriptEdit = new QLineEdit(this);
    m_scriptEdit->setPlaceholderText("e.g. 00_preprocessing.R");
    formLayout->addRow("Script:", m_scriptEdit);

    mainLayout->addWidget(formGroup);

    // --- Buttons ---
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    mainLayout->addWidget(buttonBox);

    // --- Connections ---
    connect(browseBtn, &QPushButton::clicked, this, [this, projectRootPath]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, "Select a subfolder", projectRootPath);
        if (!dir.isEmpty()) {
            m_directoryEdit->setText(dir);
            // Enable OK only if the selected dir is inside the project root
            bool ok = dir.startsWith(projectRootPath) && dir != projectRootPath;
            findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)
                ->setEnabled(ok);
        }
    });

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString CollectionDialog::selectedDirectory() const
{
    return m_directoryEdit->text();
}

QString CollectionDialog::collectionType() const
{
    return m_typeCombo->currentText();
}

QString CollectionDialog::registry() const
{
    return m_registryEdit->text().trimmed();
}

QString CollectionDialog::script() const
{
    return m_scriptEdit->text().trimmed();
}
