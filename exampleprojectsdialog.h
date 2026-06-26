#ifndef EXAMPLEPROJECTSDIALOG_H
#define EXAMPLEPROJECTSDIALOG_H

#include <QDialog>
#include <QVector>

class QListWidget;
class QTextBrowser;

struct ExampleProjectInfo
{
    QString path;
    QString name;
    QString description;
    QString readme;
};

class ExampleProjectsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExampleProjectsDialog(const QVector<ExampleProjectInfo> &examples,
                                   QWidget *parent = nullptr);

    ExampleProjectInfo selectedExample() const;

private:
    void updateDetails(int row);

    QVector<ExampleProjectInfo> m_examples;
    QListWidget *m_list = nullptr;
    QTextBrowser *m_details = nullptr;
};

#endif // EXAMPLEPROJECTSDIALOG_H
