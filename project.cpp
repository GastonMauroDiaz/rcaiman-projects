#include "project.h"

#include <QDate>
#include <QDir>
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

static void copyReadmeTemplate(const QString &projectRootPath)
{
    const QString templatePath = QStringLiteral(TEMPLATES_DIR) + "/README.md";
    if (QFileInfo::exists(templatePath)) {
        QFile::copy(templatePath, QDir(projectRootPath).filePath("README.md"));
    }
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

    QDir root;
    if (!root.mkpath(rootPath)) {
        if (errorMessage) {
            *errorMessage = "Could not create the project directory.";
        }
        return {};
    }

    if (!createDirectoryStructure(rootPath, errorMessage)) {
        return {};
    }

    copyReadmeTemplate(rootPath);

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
        "RAW",
        "TIFF",
        "EXIF",
        "Scripts",
        "Outputs",
        "Outputs/SkyMask",
        "Outputs/Points",
        "Outputs/PredictedSky",
        "Outputs/Ratio",
        "Outputs/Results",
    };

    for (const QString &directory : directories) {
        if (!root.mkpath(directory)) {
            if (errorMessage) {
                *errorMessage = QString("Could not create directory: %1").arg(directory);
            }
            return false;
        }
    }

    return true;
}
