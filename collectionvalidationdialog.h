#ifndef COLLECTIONVALIDATIONDIALOG_H
#define COLLECTIONVALIDATIONDIALOG_H

#include "collectionvalidation.h"

#include <QDialog>
#include <QSet>

class QLabel;
class QPushButton;
class QTableWidget;

class CollectionValidationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CollectionValidationDialog(
        const CollectionValidationRun &run,
        QWidget *parent = nullptr);

signals:
    void reviewRequested(const QString &rawPath,
                         const QString &registryPath);

private:
    void done(int result) override;
    void updateReviewButton();
    void reviewSelectedRow();
    void markRowReviewed(int row);

    CollectionValidationRun m_run;
    QTableWidget *m_table = nullptr;
    QPushButton *m_reviewButton = nullptr;
    QSet<int> m_reviewedRows;
    bool m_closeNoticeShown = false;
};

#endif
