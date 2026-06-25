#ifndef METADATARULESDIALOG_H
#define METADATARULESDIALOG_H

#include "metadataregistry.h"

#include <QDialog>

class QLineEdit;
class QTableWidget;

class MetadataRulesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MetadataRulesDialog(
        const QList<MetadataRuleAnalysis> &analysis,
        QWidget *parent = nullptr);

    QString draftId() const;
    QList<MetadataRule> selectedRules() const;

protected:
    void accept() override;

private:
    QList<MetadataRuleAnalysis> m_analysis;
    QLineEdit *m_draftId = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif
