#include "projectcleanupdialog.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

ProjectCleanupDialog::ProjectCleanupDialog(
    const ProjectCleanupPlan &plan,
    const QString &currentDescription,
    QWidget *parent)
    : QDialog(parent)
    , m_plan(plan)
    , m_currentDescription(currentDescription)
{
    setWindowTitle("Create Clean Project Copy");
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(
        "Select the generated data to remove from the new copy. "
        "The current project will not be modified.",
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *profileLayout = new QFormLayout();
    m_profileCombo = new QComboBox(this);
    m_profileCombo->addItem("Structure only");
    m_profileCombo->addItem("Keep RAW");
    m_profileCombo->addItem("Keep RAW and Other collections");
    m_profileCombo->addItem("Custom");
    profileLayout->addRow("Profile:", m_profileCombo);
    layout->addLayout(profileLayout);

    for (const ProjectCleanupGroup &group : std::as_const(m_plan.groups)) {
        auto *checkBox = new QCheckBox(
            QStringLiteral("%1 (%2)")
                .arg(group.label)
                .arg(group.relativeFiles.size()),
            this);
        checkBox->setChecked(group.selected);
        checkBox->setEnabled(!group.relativeFiles.isEmpty());
        connect(checkBox, &QCheckBox::toggled, this, [this]() {
            setCustomProfile();
            updateSummary();
        });
        m_checkBoxes.append(checkBox);
        layout->addWidget(checkBox);
    }

    m_summaryLabel = new QLabel(this);
    layout->addWidget(m_summaryLabel);

    m_keepDescription = new QCheckBox(
        "Keep current project description", this);
    m_keepDescription->setChecked(false);
    m_keepDescription->setEnabled(!m_currentDescription.isEmpty());
    layout->addWidget(m_keepDescription);

    auto *descriptionLayout = new QFormLayout();
    m_newDescription = new QLineEdit(this);
    m_newDescription->setPlaceholderText(
        "Leave empty for no description");
    descriptionLayout->addRow("New description:", m_newDescription);
    m_descriptionWarning = new QLabel(this);
    m_descriptionWarning->setWordWrap(true);
    m_descriptionWarning->setStyleSheet("color: #8a1f11;");
    m_descriptionWarning->hide();
    descriptionLayout->addRow(QString(), m_descriptionWarning);
    layout->addLayout(descriptionLayout);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *createButton = m_buttons->addButton(
        "Create clean copy", QDialogButtonBox::AcceptRole);
    connect(createButton, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    layout->addWidget(m_buttons);

    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProjectCleanupDialog::applyProfile);
    connect(m_keepDescription, &QCheckBox::toggled,
            m_newDescription, &QWidget::setDisabled);
    connect(m_keepDescription, &QCheckBox::toggled,
            this, &ProjectCleanupDialog::updateDescriptionValidation);
    connect(m_newDescription, &QLineEdit::textChanged,
            this, &ProjectCleanupDialog::updateDescriptionValidation);
    applyProfile(0);
    updateSummary();
    updateDescriptionValidation();
}

QString ProjectCleanupDialog::selectedDescription() const
{
    return m_keepDescription->isChecked()
        ? m_currentDescription
        : m_newDescription->text().trimmed();
}

ProjectCleanupPlan ProjectCleanupDialog::selectedPlan() const
{
    ProjectCleanupPlan selected = m_plan;
    const int count = qMin(selected.groups.size(), m_checkBoxes.size());
    for (int i = 0; i < count; ++i)
        selected.groups[i].selected = m_checkBoxes[i]->isChecked();
    return selected;
}

void ProjectCleanupDialog::applyProfile(int profileIndex)
{
    if (profileIndex < 0 || profileIndex == 3)
        return;

    m_updatingProfile = true;
    const int count = qMin(m_plan.groups.size(), m_checkBoxes.size());
    for (int i = 0; i < count; ++i) {
        const ProjectCleanupCategory category = m_plan.groups[i].category;
        bool remove = true;
        if (profileIndex == 1) {
            remove = category != ProjectCleanupCategory::RawFiles;
        } else if (profileIndex == 2) {
            remove = category != ProjectCleanupCategory::RawFiles
                && category != ProjectCleanupCategory::OtherCollections;
        }
        m_checkBoxes[i]->setChecked(remove);
    }
    m_updatingProfile = false;
    updateSummary();
}

void ProjectCleanupDialog::setCustomProfile()
{
    if (m_updatingProfile || !m_profileCombo)
        return;

    m_updatingProfile = true;
    m_profileCombo->setCurrentIndex(3);
    m_updatingProfile = false;
}

void ProjectCleanupDialog::updateSummary()
{
    int selectedFiles = 0;
    const int count = qMin(m_plan.groups.size(), m_checkBoxes.size());
    for (int i = 0; i < count; ++i) {
        if (m_checkBoxes[i]->isChecked())
            selectedFiles += m_plan.groups[i].relativeFiles.size();
    }
    m_summaryLabel->setText(
        QStringLiteral("%1 file(s) will be removed from the new copy.")
            .arg(selectedFiles));
}

void ProjectCleanupDialog::updateDescriptionValidation()
{
    QString descriptionError;
    const bool descriptionOk = m_keepDescription->isChecked()
        || Project::descriptionIsSupported(
            m_newDescription->text().trimmed(), &descriptionError);
    m_descriptionWarning->setVisible(!descriptionOk);
    m_descriptionWarning->setText(descriptionError);

    if (!m_buttons)
        return;

    const QList<QAbstractButton*> buttons = m_buttons->buttons();
    for (QAbstractButton *button : buttons) {
        if (m_buttons->buttonRole(button) == QDialogButtonBox::AcceptRole)
            button->setEnabled(descriptionOk);
    }
}
