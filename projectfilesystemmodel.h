#ifndef PROJECTFILESYSTEMMODEL_H
#define PROJECTFILESYSTEMMODEL_H

#include <QFileSystemModel>

class QFileInfo;

class ProjectFileSystemModel : public QFileSystemModel
{
    Q_OBJECT

public:
    explicit ProjectFileSystemModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    void setProjectRoot(const QString &path);
    void refreshDirectory(const QString &path);

private:
    bool isDirectProjectChild(const QFileInfo &info) const;
    QString collectionClass(const QString &directoryPath) const;
    int relevantFileCount(const QFileInfo &info,
                          const QString &collectionClass) const;

    QString m_projectRoot;
};

#endif // PROJECTFILESYSTEMMODEL_H
