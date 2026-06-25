#ifndef REGISTRYVALIDATIONDIALOG_H
#define REGISTRYVALIDATIONDIALOG_H

#include "metadataregistry.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

class RegistryValidationDialog : public QDialog
{
    Q_OBJECT

public:
    RegistryValidationDialog(
        const QList<MetadataRule> &rules,
        const QList<MetadataObservation> &observations,
        QWidget *parent = nullptr);

    QList<MetadataRuleDecision> decisions() const;
    bool acceptedDecision() const;

private:
    struct Row {
        MetadataRule rule;
        QList<MetadataObservation> choices;
        QComboBox *observed = nullptr;
        QCheckBox *itemEquivalence = nullptr;
        QCheckBox *valueEquivalence = nullptr;
        QLabel *status = nullptr;
    };

    void updateRow(int row);
    void updateAcceptEnabled();
    bool rowResolved(const Row &row) const;

    QList<Row> m_rows;
    QTableWidget *m_table = nullptr;
    QPushButton *m_acceptButton = nullptr;
    bool m_acceptedDecision = false;
};

#endif
