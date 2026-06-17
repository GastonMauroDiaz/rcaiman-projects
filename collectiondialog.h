#ifndef COLLECTIONDIALOG_H
#define COLLECTIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>

class CollectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CollectionDialog(const QString &projectRootPath,
                              QWidget *parent = nullptr);

    QString selectedDirectory() const;
    QString collectionType() const;
    QString registry() const;
    QString script() const;

private:
    QComboBox *m_typeCombo;
    QLineEdit *m_registryEdit;
    QLineEdit *m_scriptEdit;
    QLineEdit *m_directoryEdit;
};

#endif // COLLECTIONDIALOG_H
