#ifndef PROJECT_H
#define PROJECT_H

#include <QString>
#include <QStringList>
#include <QVector>

struct Collection {
    QString dirPath;   // absolute path to the folder
    QString dirName;   // folder name (basename of dirPath)
    QString type;
    QString parent;
    QString related;
    QString registry;
    QString script;
};

class Project
{
public:
    Project() = default;

    static Project create(const QString &rootPath, const QString &projectName, QString *errorMessage = nullptr);
    static Project open(const QString &rootPath, QString *errorMessage = nullptr);

    // Register a collection: creates collection.yaml inside dirPath.
    static bool registerCollection(const QString &dirPath,
                                   const QString &type,
                                   const QString &parent,
                                   const QString &related,
                                   const QString &registry,
                                   const QString &script,
                                   QString *errorMessage = nullptr);

    // Discover collections: scan first-level subdirectories of rootPath
    // looking for collection.yaml files.
    static QVector<Collection> discoverCollections(const QString &rootPath);

    bool isValid() const;
    bool save(QString *errorMessage = nullptr) const;

    QString rootPath() const;
    QString name() const;
    QString created() const;

private:
    Project(QString rootPath, QString name, QString created);

    static QString metadataFilePath(const QString &rootPath);
    static bool createDirectoryStructure(const QString &rootPath, QString *errorMessage);

    QString m_rootPath;
    QString m_name;
    QString m_created;
};

#endif // PROJECT_H
