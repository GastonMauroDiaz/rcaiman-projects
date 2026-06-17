#include "project.h"

#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <utility>

namespace {
constexpr int ProjectVersion = 1;
constexpr auto ApplicationVersion = "0.1";

QString valueAfterColon(const QString &line)
{
    const int separatorIndex = line.indexOf(':');
    if (separatorIndex < 0) {
        return {};
    }
    return line.mid(separatorIndex + 1).trimmed();
}
}

Project::Project(QString rootPath, QString name, QString created)
    : m_rootPath(std::move(rootPath))
    , m_name(std::move(name))
    , m_created(std::move(created))
{
}

Project Project::create(const QString &rootPath, const QString &projectName, QString *errorMessage)
{
    const QString cleanName = projectName.trimmed();
    if (rootPath.trimmed().isEmpty() || cleanName.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "Project path and name are required.";
        }
        return {};
    }

    // Check if folder already exists
    QDir root;
    if (root.exists(rootPath)) {
        if (errorMessage) {
            *errorMessage = "The project folder already exists.";
        }
        return {};
    }

    if (!root.mkpath(rootPath)) {
        if (errorMessage) {
            *errorMessage = "Could not create the project directory.";
        }
        return {};
    }

    if (!createDirectoryStructure(rootPath, errorMessage)) {
        return {};
    }

    Project project(QDir(rootPath).absolutePath(), cleanName, QDate::currentDate().toString(Qt::ISODate));
    if (!project.save(errorMessage)) {
        return {};
    }

    return project;
}

Project Project::open(const QString &rootPath, QString *errorMessage)
{
    const QFileInfo metadataInfo(metadataFilePath(rootPath));
    if (!metadataInfo.exists()) {
        if (errorMessage) {
            *errorMessage = "The selected folder does not contain project.yaml.";
        }
        return {};
    }

    QFile file(metadataInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = "Could not read project.yaml.";
        }
        return {};
    }

    QString name;
    QString created;

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.startsWith("project_name:")) {
            name = valueAfterColon(line);
        } else if (line.startsWith("created:")) {
            created = valueAfterColon(line);
        }
    }

    if (name.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "project.yaml does not contain project_name.";
        }
        return {};
    }

    return Project(QDir(rootPath).absolutePath(), name, created);
}

bool Project::registerCollection(const QString &dirPath,
                                 const QString &type,
                                 const QString &registry,
                                 const QString &script,
                                 QString *errorMessage)
{
    const QDir dir(dirPath);
    if (!dir.exists()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Directory does not exist: %1").arg(dirPath);
        return false;
    }

    const QString filePath = dir.filePath("collection.yaml");
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not write collection.yaml: %1").arg(dirPath);
        return false;
    }

    QTextStream stream(&file);
    stream << "schema_version: 1\n";
    stream << "type: " << type << "\n";
    if (!registry.isEmpty())
        stream << "registry: " << registry << "\n";
    if (!script.isEmpty())
        stream << "script: " << script << "\n";

    return true;
}

QVector<Collection> Project::discoverCollections(const QString &rootPath)
{
    QVector<Collection> result;
    QDir root(rootPath);
    if (!root.exists())
        return result;

    const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &entry : entries) {
        const QString yamlPath = root.filePath(entry + "/collection.yaml");
        QFileInfo fi(yamlPath);
        if (!fi.exists())
            continue;

        // Parse minimal YAML to extract type, registry, script
        QFile file(yamlPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        Collection col;
        col.dirPath = QFileInfo(yamlPath).absolutePath();
        col.dirName = entry;

        QTextStream stream(&file);
        while (!stream.atEnd()) {
            const QString line = stream.readLine();
            if (line.startsWith("type:"))
                col.type = valueAfterColon(line);
            else if (line.startsWith("registry:"))
                col.registry = valueAfterColon(line);
            else if (line.startsWith("script:"))
                col.script = valueAfterColon(line);
        }

        result.append(col);
    }

    return result;
}

bool Project::isValid() const
{
    return !m_rootPath.isEmpty() && !m_name.isEmpty();
}

bool Project::save(QString *errorMessage) const
{
    if (!isValid()) {
        if (errorMessage) {
            *errorMessage = "There is no valid project to save.";
        }
        return false;
    }

    QFile file(metadataFilePath(m_rootPath));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = "Could not write project.yaml.";
        }
        return false;
    }

    QTextStream stream(&file);
    stream << "version: " << ProjectVersion << '\n';
    stream << "project_name: " << m_name << '\n';
    stream << "created: " << m_created << '\n';
    stream << "rcaiman_projects_version: " << ApplicationVersion << '\n';

    return true;
}

QString Project::rootPath() const
{
    return m_rootPath;
}

QString Project::name() const
{
    return m_name;
}

QString Project::created() const
{
    return m_created;
}

QString Project::metadataFilePath(const QString &rootPath)
{
    return QDir(rootPath).filePath("project.yaml");
}

bool Project::createDirectoryStructure(const QString &rootPath, QString *errorMessage)
{
    QDir root(rootPath);
    const QStringList directories = {
        "raw_files",
        "sensor_data",
    };

    for (const QString &directory : directories) {
        if (!root.mkpath(directory)) {
            if (errorMessage) {
                *errorMessage = QString("Could not create directory: %1").arg(directory);
            }
            return false;
        }
    }

    // Copy README.md template if available
    const QString readmeSrc = QStringLiteral(TEMPLATES_DIR) + "/README.md";
    const QString readmeDst = root.filePath("README.md");
    if (QFile::exists(readmeSrc) && !QFile::exists(readmeDst)) {
        QFile::copy(readmeSrc, readmeDst);
    }

    return true;
}
