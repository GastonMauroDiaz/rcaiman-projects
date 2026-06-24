#include "newprojectdialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

NewProjectDialog::NewProjectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("New Project");
    setMinimumWidth(480);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    auto *parentRow = new QWidget(this);
    auto *parentLayout = new QHBoxLayout(parentRow);
    parentLayout->setContentsMargins(0, 0, 0, 0);
    m_parentDirectory = new QLineEdit(parentRow);
    auto *browseButton = new QPushButton("Browse...", parentRow);
    parentLayout->addWidget(m_parentDirectory, 1);
    parentLayout->addWidget(browseButton);
    form->addRow("Parent folder:", parentRow);

    m_folderName = new QLineEdit(this);
    form->addRow("Folder name:", m_folderName);

    m_description = new QLineEdit(this);
    m_description->setPlaceholderText("Short human-readable project title");
    form->addRow("Description:", m_description);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *createButton = buttons->addButton(
        "Create", QDialogButtonBox::AcceptRole);
    m_createButton = createButton;
    layout->addWidget(buttons);

    connect(browseButton, &QPushButton::clicked,
            this, &NewProjectDialog::browseParentDirectory);
    connect(m_parentDirectory, &QLineEdit::textChanged,
            this, &NewProjectDialog::updateCreateButton);
    connect(m_folderName, &QLineEdit::textChanged,
            this, &NewProjectDialog::updateCreateButton);
    connect(createButton, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    updateCreateButton();
}

QString NewProjectDialog::parentDirectory() const
{
    return m_parentDirectory->text().trimmed();
}

QString NewProjectDialog::folderName() const
{
    return m_folderName->text().trimmed();
}

QString NewProjectDialog::description() const
{
    return m_description->text().trimmed();
}

void NewProjectDialog::browseParentDirectory()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, "Select Project Parent Folder",
        parentDirectory());
    if (!path.isEmpty())
        m_parentDirectory->setText(path);
}

void NewProjectDialog::updateCreateButton()
{
    const QString name = folderName();
    m_createButton->setEnabled(
        !parentDirectory().isEmpty()
        && !name.isEmpty()
        && name != "."
        && name != ".."
        && !name.contains('/')
        && !name.contains('\\'));
}
