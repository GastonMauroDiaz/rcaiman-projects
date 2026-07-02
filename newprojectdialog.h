#ifndef NEWPROJECTDIALOG_H
#define NEWPROJECTDIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget *parent = nullptr);

    QString parentDirectory() const;
    QString folderName() const;
    QString description() const;

private:
    void browseParentDirectory();
    void updateCreateButton();

    QLineEdit *m_parentDirectory = nullptr;
    QLineEdit *m_folderName = nullptr;
    QLineEdit *m_description = nullptr;
    QLabel *m_descriptionWarning = nullptr;
    QPushButton *m_createButton = nullptr;
};

#endif // NEWPROJECTDIALOG_H
