#ifndef COLLECTIONDIALOG_H
#define COLLECTIONDIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QVector>

#include "project.h"

class CollectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CollectionDialog(const QString &projectRootPath,
                              const QVector<Collection> &existingCollections,
                              QWidget *parent = nullptr);

    QString selectedDirectory() const;
    QString collectionType() const;
    QString parent() const;
    QString related() const;
    QString registry() const;
    QString script() const;

private slots:
    void onTypeChanged(int index);
    void updateOkButton();
    void browseRegistry();
    void browseScript();

private:
    void setupUI();
    void updateFieldVisibility();
    void populateParentCombo();
    void populateRelatedCombo();
    QStringList compatibleTypes(const QString &targetType, bool asParent) const;

    QString m_projectRootPath;
    QVector<Collection> m_existingCollections;

    QComboBox *m_typeCombo = nullptr;
    QComboBox *m_parentCombo = nullptr;
    QComboBox *m_relatedCombo = nullptr;
    QLineEdit *m_registryEdit = nullptr;
    QLineEdit *m_scriptEdit = nullptr;
    QLineEdit *m_directoryEdit = nullptr;
    QWidget *m_parentWidget = nullptr;
    QWidget *m_relatedWidget = nullptr;
    QWidget *m_registryWidget = nullptr;
    QWidget *m_scriptWidget = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

#endif // COLLECTIONDIALOG_H