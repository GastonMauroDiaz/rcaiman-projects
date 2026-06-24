#ifndef PROJECTCLEANUPDIALOG_H
#define PROJECTCLEANUPDIALOG_H

#include "project.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;

class ProjectCleanupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProjectCleanupDialog(const ProjectCleanupPlan &plan,
                                  QWidget *parent = nullptr);

    ProjectCleanupPlan selectedPlan() const;

private:
    void applyProfile(int profileIndex);
    void setCustomProfile();
    void updateSummary();

    ProjectCleanupPlan m_plan;
    QVector<QCheckBox*> m_checkBoxes;
    QComboBox *m_profileCombo = nullptr;
    QLabel *m_summaryLabel = nullptr;
    bool m_updatingProfile = false;
};

#endif // PROJECTCLEANUPDIALOG_H
