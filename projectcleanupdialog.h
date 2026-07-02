#ifndef PROJECTCLEANUPDIALOG_H
#define PROJECTCLEANUPDIALOG_H

#include "project.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;

class ProjectCleanupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProjectCleanupDialog(const ProjectCleanupPlan &plan,
                                  const QString &currentDescription,
                                  QWidget *parent = nullptr);

    ProjectCleanupPlan selectedPlan() const;
    QString selectedDescription() const;

private:
    void applyProfile(int profileIndex);
    void setCustomProfile();
    void updateSummary();
    void updateDescriptionValidation();

    ProjectCleanupPlan m_plan;
    QVector<QCheckBox*> m_checkBoxes;
    QComboBox *m_profileCombo = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QCheckBox *m_keepDescription = nullptr;
    QLineEdit *m_newDescription = nullptr;
    QLabel *m_descriptionWarning = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    QString m_currentDescription;
    bool m_updatingProfile = false;
};

#endif // PROJECTCLEANUPDIALOG_H
