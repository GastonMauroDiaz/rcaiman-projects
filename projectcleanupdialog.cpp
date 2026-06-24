#include "projectcleanupdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

ProjectCleanupDialog::ProjectCleanupDialog(
    const ProjectCleanupPlan &plan,
    QWidget *parent)
    : QDialog(parent)
    , m_plan(plan)
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

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *createButton = buttons->addButton(
        "Create clean copy", QDialogButtonBox::AcceptRole);
    connect(createButton, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProjectCleanupDialog::applyProfile);
    applyProfile(0);
    updateSummary();
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
