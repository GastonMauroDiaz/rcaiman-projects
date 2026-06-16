#ifndef PROJECT_H
#define PROJECT_H

#include <QString>

class Project
{
public:
    Project() = default;

    static Project create(const QString &rootPath, const QString &projectName, QString *errorMessage = nullptr);
    static Project open(const QString &rootPath, QString *errorMessage = nullptr);

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

#endif
