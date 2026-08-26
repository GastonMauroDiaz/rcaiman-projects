// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// This file is part of rcaiman-projects.
//
// rcaiman-projects is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// rcaiman-projects is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with rcaiman-projects. If not, see <https://www.gnu.org/licenses/>.
//
// Part of the rcaiman suite.

#include "project.h"
#include "collectionvalidation.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>

#include <algorithm>
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

QString yamlString(const QString &value)
{
    QString escaped = value;
    escaped.replace('\\', "\\\\");
    escaped.replace('"', "\\\"");
    escaped.replace('\n', "\\n");
    escaped.replace('\r', "\\r");
    escaped.replace('\t', "\\t");
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString unescapeYamlQuotedString(const QString &value)
{
    QString result;
    result.reserve(value.size());
    bool escaping = false;

    for (const QChar character : value) {
        if (escaping) {
            if (character == QLatin1Char('n')) {
                result.append(QLatin1Char('\n'));
            } else if (character == QLatin1Char('r')) {
                result.append(QLatin1Char('\r'));
            } else if (character == QLatin1Char('t')) {
                result.append(QLatin1Char('\t'));
            } else {
                result.append(character);
            }
            escaping = false;
        } else if (character == QLatin1Char('\\')) {
            escaping = true;
        } else {
            result.append(character);
        }
    }

    if (escaping)
        result.append(QLatin1Char('\\'));

    return result;
}

QString yamlValueAfterColon(const QString &line)
{
    QString value = valueAfterColon(line);
    if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"')) {
        value = value.mid(1, value.size() - 2);
        value = unescapeYamlQuotedString(value);
    }
    return value;
}

QString normalizedAbsolutePath(const QString &path)
{
    return QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
}

bool pathIsInsideRoot(const QString &path, const QString &rootPath)
{
    const QString normalizedPath = normalizedAbsolutePath(path);
    const QString normalizedRoot = normalizedAbsolutePath(rootPath);
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    return normalizedPath.compare(normalizedRoot, sensitivity) == 0
        || normalizedPath.startsWith(
            normalizedRoot + QLatin1Char('/'), sensitivity);
}

QString cleanRelativePath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

bool relativePathIsExcluded(const QString &relativePath,
                            const QSet<QString> &excludedRelativePaths)
{
    if (excludedRelativePaths.isEmpty())
        return false;

    const QString cleanRelative = cleanRelativePath(relativePath);
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    for (const QString &excludedPath : excludedRelativePaths) {
        if (cleanRelative.compare(excludedPath, sensitivity) == 0
            || cleanRelative.startsWith(
                excludedPath + QLatin1Char('/'), sensitivity)) {
            return true;
        }
    }
    return false;
}

bool copyDirectoryRecursively(const QString &sourcePath,
                              const QString &destinationPath,
                              QString *errorMessage,
                              const QSet<QString> &excludedRelativePaths = {})
{
    QDir source(sourcePath);
    if (!source.exists()) {
        if (errorMessage)
            *errorMessage = "Source project folder does not exist.";
        return false;
    }

    if (!QDir().mkpath(destinationPath)) {
        if (errorMessage)
            *errorMessage = "Could not create destination project folder.";
        return false;
    }

    QDirIterator iterator(
        sourcePath,
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString sourceEntryPath = iterator.next();
        const QFileInfo sourceEntry(sourceEntryPath);
        const QString relativePath =
            cleanRelativePath(source.relativeFilePath(sourceEntryPath));
        if (relativePathIsExcluded(relativePath, excludedRelativePaths))
            continue;

        const QString destinationEntryPath =
            QDir(destinationPath).filePath(relativePath);

        if (sourceEntry.isDir()) {
            if (!QDir().mkpath(destinationEntryPath)) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("Could not create folder: %1").arg(relativePath);
                return false;
            }
        } else if (!QFile::copy(sourceEntryPath, destinationEntryPath)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Could not copy file: %1").arg(relativePath);
            return false;
        }
    }

    return true;
}

QStringList matchingRelativeFiles(const QString &projectRoot,
                                  const QString &directoryPath,
                                  const QStringList &filters,
                                  bool recursive = true)
{
    QStringList files;
    QDir root(projectRoot);
    QDirIterator iterator(
        directoryPath,
        filters,
        QDir::Files | QDir::NoSymLinks | QDir::Hidden | QDir::System,
        recursive ? QDirIterator::Subdirectories
                  : QDirIterator::NoIteratorFlags);
    while (iterator.hasNext())
        files.append(QDir::fromNativeSeparators(
            root.relativeFilePath(iterator.next())));
    return files;
}

QStringList allRelativeFiles(const QString &projectRoot,
                             const QString &directoryPath)
{
    return matchingRelativeFiles(
        projectRoot, directoryPath, {"*"}, true);
}

QString markdownEscaped(const QString &text)
{
    QString result = text;
    result.replace('|', "\\|");
    return result;
}

QString initialCommentHeader(const QString &scriptPath)
{
    QFile file(scriptPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream stream(&file);
    QStringList lines;
    bool started = false;
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (!started && line.trimmed().isEmpty())
            continue;

        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith('#'))
            break;

        started = true;
        line = trimmed.mid(1);
        if (line.startsWith(' '))
            line.remove(0, 1);
        lines.append(line);
    }

    while (!lines.isEmpty() && lines.first().trimmed().isEmpty())
        lines.removeFirst();
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();

    return lines.join('\n').trimmed();
}

QString scriptReferenceKey(const QString &projectRoot,
                           const QString &scriptReference)
{
    const QString resolved =
        Project::resolveReference(projectRoot, scriptReference);
    if (resolved.isEmpty())
        return {};
    return QDir::fromNativeSeparators(
        QDir(projectRoot).relativeFilePath(resolved));
}

}

int ProjectCleanupPlan::totalSelectedFiles() const
{
    int total = 0;
    for (const ProjectCleanupGroup &group : groups) {
        if (group.selected)
            total += group.relativeFiles.size();
    }
    return total;
}

int ProjectContentCleanPlan::totalSelectedFiles() const
{
    int total = 0;
    for (const ProjectContentCleanItem &item : items) {
        if (item.selected)
            total += item.relativeFiles.size();
    }
    return total;
}

int ProjectContentCleanPlan::totalSelectedFolders() const
{
    int total = 0;
    for (const ProjectContentCleanItem &item : items) {
        if (item.selected && !item.relativeFiles.isEmpty())
            ++total;
    }
    return total;
}

Project::Project(QString rootPath, QString name, QString created,
                 QString description)
    : m_rootPath(std::move(rootPath))
    , m_name(std::move(name))
    , m_created(std::move(created))
    , m_description(std::move(description))
{
}

Project Project::create(const QString &rootPath,
                        const QString &projectName,
                        const QString &description,
                        QString *errorMessage)
{
    const QString cleanName = projectName.trimmed();
    const QString cleanDescription = description.trimmed();
    if (rootPath.trimmed().isEmpty() || cleanName.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "Project path and name are required.";
        }
        return {};
    }
    if (!descriptionIsSupported(cleanDescription, errorMessage))
        return {};

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

    Project project(
        QDir(rootPath).absolutePath(),
        cleanName,
        QDate::currentDate().toString(Qt::ISODate),
        cleanDescription);
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
    QString description;

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.startsWith("project_name:")) {
            name = yamlValueAfterColon(line);
        } else if (line.startsWith("created:")) {
            created = valueAfterColon(line);
        } else if (line.startsWith("description:")) {
            description = yamlValueAfterColon(line);
        }
    }

    if (name.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "project.yaml does not contain project_name.";
        }
        return {};
    }

    return Project(
        QDir(rootPath).absolutePath(), name, created, description);
}

bool Project::descriptionIsSupported(const QString &description,
                                     QString *errorMessage)
{
    for (const QChar character : description) {
        if (character.category() == QChar::Other_Control) {
            if (errorMessage) {
                *errorMessage = "Project description cannot contain line "
                    "breaks or control characters.";
            }
            return false;
        }
    }
    return true;
}

bool Project::collectionAliasIsSupported(const QString &alias,
                                         QString *errorMessage)
{
    const QString trimmedAlias = alias.trimmed();
    if (trimmedAlias.size() > 40) {
        if (errorMessage)
            *errorMessage = "Collection alias cannot be longer than 40 characters.";
        return false;
    }

    for (const QChar character : trimmedAlias) {
        if (character.category() == QChar::Other_Control) {
            if (errorMessage) {
                *errorMessage = "Collection alias cannot contain line breaks "
                    "or control characters.";
            }
            return false;
        }
    }
    return true;
}

bool Project::updateCollectionMetadata(const QString &projectRootPath,
                                       const QString &dirPath,
                                       const QString &alias,
                                       const QString &parent,
                                       const QString &related,
                                       const QString &registry,
                                       const QString &script,
                                       QString *errorMessage)
{
    if (!pathIsInsideRoot(dirPath, projectRootPath)
        || normalizedAbsolutePath(dirPath)
            == normalizedAbsolutePath(projectRootPath)) {
        if (errorMessage)
            *errorMessage = "Collection folder must be inside the project root.";
        return false;
    }

    const QString cleanAlias = alias.trimmed();
    if (!collectionAliasIsSupported(cleanAlias, errorMessage))
        return false;

    const QString filePath = QDir(dirPath).filePath("collection.yaml");
    QFile input(filePath);
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = "Could not read collection.yaml.";
        return false;
    }

    QStringList lines;
    QTextStream inputStream(&input);
    while (!inputStream.atEnd())
        lines.append(inputStream.readLine());
    input.close();

    bool sawClass = false;
    bool wroteMetadata = false;
    QStringList outputLines;
    outputLines.reserve(lines.size() + 5);
    const QStringList editedKeys = {
        "alias:", "parent:", "related:", "registry:", "script:"
    };
    for (const QString &line : std::as_const(lines)) {
        const QString trimmedLine = line.trimmed();
        bool skipLine = false;
        for (const QString &key : editedKeys) {
            if (trimmedLine.startsWith(key)) {
                skipLine = true;
                break;
            }
        }
        if (skipLine)
            continue;

        outputLines.append(line);
        if (!wroteMetadata && trimmedLine.startsWith("class:")) {
            sawClass = true;
            if (!cleanAlias.isEmpty()) {
                outputLines.append(QStringLiteral("alias: %1").arg(
                    yamlString(cleanAlias)));
            }
            if (!parent.isEmpty()) {
                outputLines.append(QStringLiteral("parent: %1").arg(
                    portableReference(projectRootPath, parent)));
            }
            if (!related.isEmpty()) {
                outputLines.append(QStringLiteral("related: %1").arg(
                    portableReference(projectRootPath, related)));
            }
            if (!registry.isEmpty()) {
                outputLines.append(QStringLiteral("registry: %1").arg(
                    portableReference(projectRootPath, registry)));
            }
            if (!script.isEmpty()) {
                outputLines.append(QStringLiteral("script: %1").arg(
                    portableReference(projectRootPath, script)));
            }
            wroteMetadata = true;
        }
    }

    if (!sawClass) {
        if (errorMessage)
            *errorMessage = "collection.yaml does not contain a class field.";
        return false;
    }

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = "Could not write collection.yaml.";
        return false;
    }

    QTextStream outputStream(&output);
    for (const QString &line : std::as_const(outputLines))
        outputStream << line << "\n";
    outputStream.flush();
    if (outputStream.status() != QTextStream::Ok) {
        output.cancelWriting();
        if (errorMessage)
            *errorMessage = "Could not write collection.yaml.";
        return false;
    }
    if (!output.commit()) {
        if (errorMessage)
            *errorMessage = "Could not commit collection.yaml.";
        return false;
    }
    return true;
}

bool Project::unregisterCollection(const QString &projectRootPath,
                                   const QString &dirPath,
                                   QString *errorMessage)
{
    if (!pathIsInsideRoot(dirPath, projectRootPath)
        || normalizedAbsolutePath(dirPath)
            == normalizedAbsolutePath(projectRootPath)) {
        if (errorMessage)
            *errorMessage = "Collection folder must be inside the project root.";
        return false;
    }

    const QDir dir(dirPath);
    if (!dir.exists()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Directory does not exist: %1").arg(dirPath);
        return false;
    }

    const QString collectionPath = dir.filePath("collection.yaml");
    if (!QFileInfo::exists(collectionPath)) {
        if (errorMessage)
            *errorMessage = "This folder is not a registered collection.";
        return false;
    }

    const QString stylePath = dir.filePath("style.yaml");
    if (QFileInfo::exists(stylePath) && !QFile::remove(stylePath)) {
        if (errorMessage)
            *errorMessage = "Could not remove style.yaml.";
        return false;
    }

    if (!QFile::remove(collectionPath)) {
        if (errorMessage)
            *errorMessage = "Could not remove collection.yaml.";
        return false;
    }

    return true;
}

bool Project::registerCollection(const QString &projectRootPath,
                                 const QString &dirPath,
                                 const QString &collectionClass,
                                 const QString &parent,
                                 const QString &related,
                                 const QString &registry,
                                 const QString &script,
                                 const QString &alias,
                                 QString *errorMessage)
{
    if (!pathIsInsideRoot(dirPath, projectRootPath)
        || normalizedAbsolutePath(dirPath)
            == normalizedAbsolutePath(projectRootPath)) {
        if (errorMessage)
            *errorMessage = "Collection folder must be inside the project root.";
        return false;
    }

    const QDir dir(dirPath);
    if (!dir.exists()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Directory does not exist: %1").arg(dirPath);
        return false;
    }

    const QString cleanAlias = alias.trimmed();
    if (!collectionAliasIsSupported(cleanAlias, errorMessage))
        return false;

    const QString filePath = dir.filePath("collection.yaml");
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not write collection.yaml: %1").arg(dirPath);
        return false;
    }

    QTextStream stream(&file);
    stream << "schema_version: 1\n";
    stream << "class: " << collectionClass << "\n";
    if (!cleanAlias.isEmpty())
        stream << "alias: " << yamlString(cleanAlias) << "\n";
    if (!parent.isEmpty())
        stream << "parent: "
               << portableReference(projectRootPath, parent) << "\n";
    if (!related.isEmpty())
        stream << "related: "
               << portableReference(projectRootPath, related) << "\n";
    if (!registry.isEmpty())
        stream << "registry: "
               << portableReference(projectRootPath, registry) << "\n";
    if (!script.isEmpty())
        stream << "script: "
               << portableReference(projectRootPath, script) << "\n";

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

        // Parse minimal YAML to extract class, registry, script
        QFile file(yamlPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        Collection col;
        col.dirPath = QFileInfo(yamlPath).absolutePath();
        col.dirName = entry;

        QTextStream stream(&file);
        while (!stream.atEnd()) {
            const QString line = stream.readLine();
            if (line.startsWith("class:"))
                col.collectionClass = valueAfterColon(line);
            else if (line.startsWith("alias:"))
                col.alias = yamlValueAfterColon(line);
            else if (line.startsWith("parent:"))
                col.parent = valueAfterColon(line);
            else if (line.startsWith("related:"))
                col.related = valueAfterColon(line);
            else if (line.startsWith("registry:"))
                col.registry = valueAfterColon(line);
            else if (line.startsWith("script:"))
                col.script = valueAfterColon(line);
        }

        result.append(col);
    }

    return result;
}

QString Project::resolveReference(const QString &projectRootPath,
                                  const QString &reference)
{
    if (reference.trimmed().isEmpty())
        return {};

    const QFileInfo referenceInfo(reference);
    if (referenceInfo.isAbsolute())
        return normalizedAbsolutePath(reference);

    return normalizedAbsolutePath(
        QDir(projectRootPath).filePath(reference));
}

QString Project::portableReference(const QString &projectRootPath,
                                   const QString &reference)
{
    if (reference.trimmed().isEmpty())
        return {};

    const QFileInfo referenceInfo(reference);
    if (referenceInfo.isRelative())
        return QDir::cleanPath(QDir::fromNativeSeparators(reference));

    const QString normalizedReference = normalizedAbsolutePath(reference);
    if (!pathIsInsideRoot(normalizedReference, projectRootPath))
        return normalizedReference;

    return QDir::fromNativeSeparators(
        QDir(projectRootPath).relativeFilePath(normalizedReference));
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
    stream << "project_name: " << yamlString(m_name) << '\n';
    if (!m_description.isEmpty())
        stream << "description: " << yamlString(m_description) << '\n';
    stream << "created: " << m_created << '\n';
    stream << "rcaiman_projects_version: " << ApplicationVersion << '\n';

    return true;
}

bool Project::setDescription(const QString &description,
                             QString *errorMessage)
{
    const QString cleanDescription = description.trimmed();
    if (!descriptionIsSupported(cleanDescription, errorMessage))
        return false;
    m_description = cleanDescription;
    return save(errorMessage);
}

Project Project::saveAs(const QString &newRootPath,
                        const QString &newProjectName,
                        QString *errorMessage) const
{
    if (!isValid() || newProjectName.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = "A valid source project and new name are required.";
        return {};
    }

    const QString destination =
        QDir::cleanPath(QFileInfo(newRootPath).absoluteFilePath());
    if (QFileInfo::exists(destination)) {
        if (errorMessage)
            *errorMessage = "The destination project folder already exists.";
        return {};
    }

    if (pathIsInsideRoot(destination, m_rootPath)) {
        if (errorMessage)
            *errorMessage = "The new project cannot be created inside the current project.";
        return {};
    }

    if (!copyDirectoryRecursively(m_rootPath, destination, errorMessage)) {
        QDir(destination).removeRecursively();
        return {};
    }

    Project copy(destination,
                 newProjectName.trimmed(),
                 QDate::currentDate().toString(Qt::ISODate),
                 m_description);
    QFile::remove(copy.reportFilePath());
    if (!copy.save(errorMessage)) {
        QDir(destination).removeRecursively();
        return {};
    }

    return copy;
}

Project Project::saveAsCleanCopy(const QString &newRootPath,
                                 const QString &newProjectName,
                                 const ProjectCleanupPlan &cleanupPlan,
                                 const QString &description,
                                 QString *errorMessage) const
{
    const QString cleanName = newProjectName.trimmed();
    const QString cleanDescription = description.trimmed();
    if (!isValid() || cleanName.isEmpty()) {
        if (errorMessage)
            *errorMessage = "A valid source project and new name are required.";
        return {};
    }
    if (!descriptionIsSupported(cleanDescription, errorMessage))
        return {};

    const QString destination =
        QDir::cleanPath(QFileInfo(newRootPath).absoluteFilePath());
    if (QFileInfo::exists(destination)) {
        if (errorMessage)
            *errorMessage = "The destination project folder already exists.";
        return {};
    }

    if (pathIsInsideRoot(destination, m_rootPath)) {
        if (errorMessage)
            *errorMessage = "The new project cannot be created inside the current project.";
        return {};
    }

    const QString normalizedRoot = normalizedAbsolutePath(m_rootPath);
    QSet<QString> excludedRelativePaths;
    for (const ProjectCleanupGroup &group : cleanupPlan.groups) {
        if (!group.selected)
            continue;

        for (const QString &relativePath : group.relativeFiles) {
            const QString cleanRelative = cleanRelativePath(relativePath);
            const QString absolutePath = normalizedAbsolutePath(
                QDir(m_rootPath).filePath(cleanRelative));
            if (cleanRelative.isEmpty()
                || cleanRelative == "."
                || !pathIsInsideRoot(absolutePath, normalizedRoot)
                || absolutePath == normalizedRoot) {
                if (errorMessage)
                    *errorMessage = "Cleanup plan contains an invalid path.";
                return {};
            }
            excludedRelativePaths.insert(cleanRelative);
        }
    }
    excludedRelativePaths.insert(QStringLiteral("project-report.md"));

    if (!copyDirectoryRecursively(
            m_rootPath, destination, errorMessage, excludedRelativePaths)) {
        QDir(destination).removeRecursively();
        return {};
    }

    Project copy(destination,
                 cleanName,
                 QDate::currentDate().toString(Qt::ISODate),
                 cleanDescription);
    QFile::remove(copy.reportFilePath());
    if (!copy.save(errorMessage)) {
        QDir(destination).removeRecursively();
        return {};
    }

    return copy;
}

Project Project::copyExistingProject(const QString &sourceRootPath,
                                     const QString &destinationRootPath,
                                     const QString &newProjectName,
                                     QString *errorMessage)
{
    Project source = Project::open(sourceRootPath, errorMessage);
    if (!source.isValid())
        return {};

    if (newProjectName.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = "A new project name is required.";
        return {};
    }

    const QString destination =
        QDir::cleanPath(QFileInfo(destinationRootPath).absoluteFilePath());
    if (QFileInfo::exists(destination)) {
        if (errorMessage)
            *errorMessage = "The destination project folder already exists.";
        return {};
    }

    if (pathIsInsideRoot(destination, source.rootPath())) {
        if (errorMessage)
            *errorMessage = "The new project cannot be created inside the example project.";
        return {};
    }

    if (!copyDirectoryRecursively(source.rootPath(), destination, errorMessage)) {
        QDir(destination).removeRecursively();
        return {};
    }

    Project copy(destination,
                 newProjectName.trimmed(),
                 QDate::currentDate().toString(Qt::ISODate),
                 source.description());
    QFile::remove(copy.reportFilePath());
    if (!copy.save(errorMessage)) {
        QDir(destination).removeRecursively();
        return {};
    }

    return copy;
}

bool Project::writeReport(QString *errorMessage) const
{
    if (!isValid()) {
        if (errorMessage)
            *errorMessage = "There is no valid project to report.";
        return false;
    }

    QFile file(reportFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text
                   | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = "Could not write project-report.md.";
        return false;
    }

    const QStringList rawFilters = {
        "*.CR2", "*.CR3", "*.NEF", "*.ARW", "*.DNG", "*.RAF",
        "*.ORF", "*.RW2", "*.PEF", "*.SRW", "*.RAW", "*.DCR", "*.KDC",
        "*.cr2", "*.cr3", "*.nef", "*.arw", "*.dng", "*.raf",
        "*.orf", "*.rw2", "*.pef", "*.srw", "*.raw", "*.dcr", "*.kdc"
    };
    const QStringList imageFilters = {
        "*.tif", "*.tiff", "*.TIF", "*.TIFF",
        "*.png", "*.PNG", "*.jpg", "*.JPG", "*.jpeg", "*.JPEG",
        "*.bmp", "*.BMP"
    };

    const QVector<Collection> collections =
        discoverCollections(m_rootPath);
    const int rawCount = matchingRelativeFiles(
        m_rootPath, QDir(m_rootPath).filePath("raw_files"),
        rawFilters).size();
    const int sensorCount = matchingRelativeFiles(
        m_rootPath, QDir(m_rootPath).filePath("sensor_data"),
        imageFilters).size();
    const int scriptCount = matchingRelativeFiles(
        m_rootPath, QDir(m_rootPath).filePath("scripts"),
        {"*.R", "*.r", "*.Rmd", "*.rmd", "*.qmd"}).size();

    QTextStream stream(&file);
    stream << "# "
           << (m_description.isEmpty() ? m_name : m_description)
           << "\n\n";
    stream << "- **Project folder:** `" << m_name << "`\n";
    if (!m_description.isEmpty())
        stream << "- **Description:** " << m_description << "\n";
    stream << "- **Created:** " << m_created << "\n";
    stream << "- **Report generated:** "
           << QDateTime::currentDateTime().toString(Qt::ISODate)
           << "\n\n";

    stream << "## Overview\n\n";
    stream << "- RAW files: " << rawCount << "\n";
    stream << "- Sensor images: " << sensorCount << "\n";
    stream << "- Registered collections: " << collections.size() << "\n";
    stream << "- Scripts: " << scriptCount << "\n\n";

    stream << "## Registry Validation\n\n";
    bool hasEligibleCollection = false;
    stream << "| Collection | Registry | Status | Validated | Exact | Accepted | Rejected | Problems |\n";
    stream << "| --- | --- | --- | --- | ---: | ---: | ---: | ---: |\n";
    for (const Collection &collection : collections) {
        if (!CollectionValidation::isEligible(m_rootPath, collection))
            continue;
        hasEligibleCollection = true;
        QString resolvedRegistry;
        CollectionValidation::resolveRegistry(
            m_rootPath, collection, &resolvedRegistry);
        const CollectionValidationReportStatus validation =
            CollectionValidation::latestReportStatus(
                m_rootPath, collection);
        QString status = "Not validated";
        if (validation.exists) {
            if (validation.stale) {
                status = "Stale";
            } else if (validation.result == "passed_exact") {
                status = "Passed exact";
            } else if (validation.result == "passed_with_reviews") {
                status = "Passed with reviews";
            } else {
                status = "Failed";
            }
        }
        stream << "| `" << collection.dirName << "` | `"
               << portableReference(m_rootPath, resolvedRegistry)
               << "` | " << status << " | "
               << (validation.validatedAt.isEmpty()
                       ? "-" : validation.validatedAt)
               << " | " << validation.exactCount
               << " | " << validation.reviewedCount
               << " | " << validation.rejectedCount
               << " | " << validation.problemCount << " |\n";
    }
    if (!hasEligibleCollection)
        stream << "| - | - | No eligible collections | - | 0 | 0 | 0 | 0 |\n";
    stream << "\n";

    stream << "## Project Structure\n\n";
    const QDir projectRoot(m_rootPath);
    const QStringList directories = projectRoot.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
        QDir::Name);
    if (directories.isEmpty()) {
        stream << "No project folders.\n\n";
    } else {
        for (const QString &directory : directories) {
            const int fileCount = allRelativeFiles(
                m_rootPath, projectRoot.filePath(directory)).size();
            QString role;
            if (reservedDirectories().contains(
                    directory, Qt::CaseInsensitive)) {
                role = "reserved";
            } else {
                for (const Collection &collection : collections) {
                    if (collection.dirName.compare(
                            directory, Qt::CaseInsensitive) == 0) {
                        role = collection.collectionClass;
                        break;
                    }
                }
            }

            stream << "- `" << directory << "/` - "
                   << fileCount << " file(s)";
            if (!role.isEmpty())
                stream << " - " << role;
            stream << "\n";
        }
        stream << "\n";
    }

    stream << "## Collections\n\n";
    if (collections.isEmpty()) {
        stream << "No registered collections.\n";
    } else {
        stream << "| Folder | Class | Relevant files | Parent | Related |\n";
        stream << "| --- | --- | ---: | --- | --- |\n";
        for (const Collection &collection : collections) {
            QStringList filters = {"*"};
            if (collection.collectionClass == "point_collection")
                filters = {"*.csv", "*.CSV"};
            else if (collection.collectionClass != "other")
                filters = imageFilters;
            const int fileCount = matchingRelativeFiles(
                m_rootPath, collection.dirPath, filters, false).size();
            const QString collectionName = collection.alias.isEmpty()
                ? QStringLiteral("`%1`").arg(collection.dirName)
                : QStringLiteral("`%1`<br>%2")
                      .arg(collection.dirName,
                           markdownEscaped(collection.alias));
            stream << "| " << collectionName << " | "
                   << collection.collectionClass << " | "
                   << fileCount << " | "
                   << (collection.parent.isEmpty() ? "-" : collection.parent)
                   << " | "
                   << (collection.related.isEmpty() ? "-" : collection.related)
                   << " |\n";
        }
    }

    stream << "\n## Processing Story\n\n";
    QMap<QString, QVector<Collection>> collectionsByScript;
    for (const Collection &collection : collections) {
        if (collection.script.trimmed().isEmpty())
            continue;

        const QString scriptKey =
            scriptReferenceKey(m_rootPath, collection.script);
        if (!scriptKey.isEmpty())
            collectionsByScript[scriptKey].append(collection);
    }

    if (collectionsByScript.isEmpty()) {
        stream << "No collection declares a script.\n";
    } else {
        for (auto it = collectionsByScript.cbegin();
             it != collectionsByScript.cend();
             ++it) {
            const QString scriptRelativePath = it.key();
            const QString scriptPath =
                QDir(m_rootPath).filePath(scriptRelativePath);
            stream << "### `" << scriptRelativePath << "`\n\n";

            const QString header = initialCommentHeader(scriptPath);
            if (!header.isEmpty()) {
                const QStringList lines = header.split('\n');
                for (const QString &line : lines)
                    stream << markdownEscaped(line) << "\n";
                stream << "\n";
            } else if (!QFileInfo::exists(scriptPath)) {
                stream << "_Script file not found._\n\n";
            } else {
                stream << "_No leading comment header._\n\n";
            }

            stream << "| Collection | Class | Parent | Related | Registry |\n";
            stream << "| --- | --- | --- | --- | --- |\n";
            QVector<Collection> scriptCollections = it.value();
            std::sort(
                scriptCollections.begin(), scriptCollections.end(),
                [](const Collection &left, const Collection &right) {
                    return left.dirName.compare(
                        right.dirName, Qt::CaseInsensitive) < 0;
                });
            for (const Collection &collection : scriptCollections) {
                stream << "| `" << collection.dirName << "` | "
                       << collection.collectionClass << " | "
                       << (collection.parent.isEmpty()
                               ? "-" : markdownEscaped(collection.parent))
                       << " | "
                       << (collection.related.isEmpty()
                               ? "-" : markdownEscaped(collection.related))
                       << " | "
                       << (collection.registry.isEmpty()
                               ? "-" : QStringLiteral("`%1`").arg(
                                     markdownEscaped(collection.registry)))
                       << " |\n";
            }
            stream << "\n";
        }
    }

    if (stream.status() != QTextStream::Ok) {
        if (errorMessage)
            *errorMessage = "Could not complete project-report.md.";
        return false;
    }
    return true;
}

ProjectCleanupPlan Project::buildCleanupPlan() const
{
    ProjectCleanupPlan plan;
    if (!isValid())
        return plan;

    auto addGroup = [&plan](ProjectCleanupCategory category,
                            const QString &label,
                            const QStringList &files) {
        ProjectCleanupGroup group;
        group.category = category;
        group.label = label;
        group.relativeFiles = files;
        plan.groups.append(group);
    };

    const QStringList rawFilters = {
        "*.CR2", "*.CR3", "*.NEF", "*.ARW", "*.DNG", "*.RAF",
        "*.ORF", "*.RW2", "*.PEF", "*.SRW", "*.RAW", "*.DCR", "*.KDC",
        "*.cr2", "*.cr3", "*.nef", "*.arw", "*.dng", "*.raf",
        "*.orf", "*.rw2", "*.pef", "*.srw", "*.raw", "*.dcr", "*.kdc"
    };
    const QStringList imageFilters = {
        "*.tif", "*.tiff", "*.TIF", "*.TIFF",
        "*.png", "*.PNG", "*.jpg", "*.JPG", "*.jpeg", "*.JPEG",
        "*.bmp", "*.BMP"
    };

    addGroup(
        ProjectCleanupCategory::RawFiles,
        "RAW files",
        matchingRelativeFiles(
            m_rootPath, QDir(m_rootPath).filePath("raw_files"), rawFilters));
    addGroup(
        ProjectCleanupCategory::SensorImages,
        "Sensor images and metadata",
        matchingRelativeFiles(
            m_rootPath, QDir(m_rootPath).filePath("sensor_data"),
            imageFilters + QStringList{"*.tsv", "*.TSV"}));

    QStringList collectionImages;
    QStringList segmentations;
    QStringList pointFiles;
    QStringList otherCollectionFiles;
    const QVector<Collection> collections = discoverCollections(m_rootPath);

    QSet<QString> protectedPaths;
    for (const Collection &collection : collections) {
        for (const QString &reference : {collection.script, collection.registry}) {
            if (reference.isEmpty())
                continue;
            const QString resolved = resolveReference(m_rootPath, reference);
            if (pathIsInsideRoot(resolved, m_rootPath))
                protectedPaths.insert(normalizedAbsolutePath(resolved));
        }
    }

    for (const Collection &collection : collections) {
        if (collection.collectionClass == "segmentation_collection") {
            segmentations.append(matchingRelativeFiles(
                m_rootPath, collection.dirPath, imageFilters, false));
        } else if (collection.collectionClass == "point_collection") {
            pointFiles.append(matchingRelativeFiles(
                m_rootPath, collection.dirPath, {"*.csv", "*.CSV"}, false));
        } else if (collection.collectionClass == "preprocessed_image_collection"
                   || collection.collectionClass == "processed_image_collection") {
            collectionImages.append(matchingRelativeFiles(
                m_rootPath, collection.dirPath, imageFilters, false));
        } else if (collection.collectionClass == "other") {
            const QStringList candidates =
                allRelativeFiles(m_rootPath, collection.dirPath);
            for (const QString &relativePath : candidates) {
                const QString absolutePath = normalizedAbsolutePath(
                    QDir(m_rootPath).filePath(relativePath));
                if (QFileInfo(relativePath).fileName().compare(
                        "collection.yaml", Qt::CaseInsensitive) == 0
                    || QFileInfo(relativePath).fileName().compare(
                        "style.yaml", Qt::CaseInsensitive) == 0
                    || protectedPaths.contains(absolutePath)
                    || relativePath.endsWith(
                        ".bak", Qt::CaseInsensitive)) {
                    continue;
                }
                otherCollectionFiles.append(relativePath);
            }
        }
    }

    addGroup(ProjectCleanupCategory::CollectionImages,
             "Collection images", collectionImages);
    addGroup(ProjectCleanupCategory::Segmentations,
             "Segmentations", segmentations);
    addGroup(ProjectCleanupCategory::PointFiles,
             "Point CSV files", pointFiles);
    addGroup(ProjectCleanupCategory::OtherCollections,
             "Other collections", otherCollectionFiles);
    addGroup(
        ProjectCleanupCategory::RegistryArtifacts,
        "Registry drafts, reviews and validations",
        [&]() {
            QStringList registryFiles;
            const QDir root(m_rootPath);
            for (const QString &directoryName : {
                     QStringLiteral("registry_drafts"),
                     QStringLiteral("registry_reviews"),
                     QStringLiteral("registry_validations")}) {
                registryFiles.append(
                    allRelativeFiles(m_rootPath, root.filePath(directoryName)));
            }
            registryFiles.removeDuplicates();
            registryFiles.sort(Qt::CaseInsensitive);
            return registryFiles;
        }());
    addGroup(
        ProjectCleanupCategory::RootFiles,
        "Loose files in project root",
        [&]() {
            QStringList rootFiles;
            const QDir root(m_rootPath);
            const QFileInfoList entries = root.entryInfoList(
                QDir::Files | QDir::NoSymLinks | QDir::Hidden | QDir::System,
                QDir::Name);
            for (const QFileInfo &entry : entries) {
                if (entry.fileName().compare(
                        "project.yaml", Qt::CaseInsensitive) == 0
                    || entry.fileName().compare(
                        "project-report.md", Qt::CaseInsensitive) == 0
                    || entry.fileName().compare(
                        "README.md", Qt::CaseInsensitive) == 0) {
                    continue;
                }
                rootFiles.append(QDir::fromNativeSeparators(entry.fileName()));
            }
            return rootFiles;
        }());
    addGroup(
        ProjectCleanupCategory::TemporaryBackups,
        "Temporary backups",
        [&]() {
            QStringList temporaryFiles = matchingRelativeFiles(
                m_rootPath, m_rootPath, {"*.bak", "*.BAK"});
            const QString sessionPath =
                QDir(m_rootPath).filePath(".rcaiman/session.json");
            if (QFileInfo::exists(sessionPath)) {
                temporaryFiles.append(
                    QDir::fromNativeSeparators(
                        QDir(m_rootPath).relativeFilePath(sessionPath)));
            }
            const QString marksPath =
                QDir(m_rootPath).filePath(".rcaiman/file_marks.json");
            if (QFileInfo::exists(marksPath)) {
                temporaryFiles.append(
                    QDir::fromNativeSeparators(
                        QDir(m_rootPath).relativeFilePath(marksPath)));
            }
            const QString projectExplorerStatePath =
                QDir(m_rootPath).filePath(
                    ".rcaiman/project_explorer_state.json");
            if (QFileInfo::exists(projectExplorerStatePath)) {
                temporaryFiles.append(
                    QDir::fromNativeSeparators(
                        QDir(m_rootPath).relativeFilePath(
                            projectExplorerStatePath)));
            }
            const QString rstudioStatePath =
                QDir(m_rootPath).filePath(".Rproj.user");
            if (QFileInfo(rstudioStatePath).isDir()) {
                temporaryFiles.append(
                    QDir::fromNativeSeparators(
                        QDir(m_rootPath).relativeFilePath(rstudioStatePath)));
            }
            return temporaryFiles;
        }());

    return plan;
}

bool Project::applyCleanupPlan(const ProjectCleanupPlan &plan,
                               QString *errorMessage) const
{
    if (!isValid()) {
        if (errorMessage)
            *errorMessage = "There is no valid project to clean.";
        return false;
    }

    const QString normalizedRoot = normalizedAbsolutePath(m_rootPath);
    for (const ProjectCleanupGroup &group : plan.groups) {
        if (!group.selected)
            continue;

        for (const QString &relativePath : group.relativeFiles) {
            const QString absolutePath = normalizedAbsolutePath(
                QDir(m_rootPath).filePath(relativePath));
            if (!pathIsInsideRoot(absolutePath, normalizedRoot)
                || absolutePath == normalizedRoot) {
                if (errorMessage)
                    *errorMessage = "Cleanup plan contains an invalid path.";
                return false;
            }

            const QFileInfo cleanupTarget(absolutePath);
            if (cleanupTarget.isDir()) {
                if (!QDir(absolutePath).removeRecursively()) {
                    if (errorMessage) {
                        *errorMessage = QStringLiteral(
                            "Could not remove folder: %1").arg(relativePath);
                    }
                    return false;
                }
            } else if (cleanupTarget.exists() && !QFile::remove(absolutePath)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral(
                        "Could not remove file: %1").arg(relativePath);
                }
                return false;
            }
        }
    }

    return true;
}

ProjectContentCleanPlan Project::buildContentCleanPlan() const
{
    ProjectContentCleanPlan plan;
    if (!isValid())
        return plan;

    auto addItem = [&plan, this](const QString &label,
                                 const QString &kind,
                                 const QString &folderPath,
                                 const QStringList &relativeFiles) {
        ProjectContentCleanItem item;
        item.label = label;
        item.kind = kind;
        item.rootRelativePath = QDir::fromNativeSeparators(
            QDir(m_rootPath).relativeFilePath(folderPath));
        item.relativeFiles = relativeFiles;
        item.selected = false;
        plan.items.append(item);
    };

    const QStringList rawFilters = {
        "*.CR2", "*.CR3", "*.NEF", "*.ARW", "*.DNG", "*.RAF",
        "*.ORF", "*.RW2", "*.PEF", "*.SRW", "*.RAW", "*.DCR", "*.KDC",
        "*.cr2", "*.cr3", "*.nef", "*.arw", "*.dng", "*.raf",
        "*.orf", "*.rw2", "*.pef", "*.srw", "*.raw", "*.dcr", "*.kdc"
    };
    const QStringList imageFilters = {
        "*.tif", "*.tiff", "*.TIF", "*.TIFF",
        "*.png", "*.PNG", "*.jpg", "*.JPG", "*.jpeg", "*.JPEG",
        "*.bmp", "*.BMP"
    };

    const QDir root(m_rootPath);
    const QString rawPath = root.filePath("raw_files");
    addItem(
        "raw_files",
        "RAW files",
        rawPath,
        matchingRelativeFiles(m_rootPath, rawPath, rawFilters));

    const QString sensorPath = root.filePath("sensor_data");
    addItem(
        "sensor_data",
        "Sensor data",
        sensorPath,
        allRelativeFiles(m_rootPath, sensorPath));

    QVector<Collection> collections = discoverCollections(m_rootPath);
    std::sort(
        collections.begin(), collections.end(),
        [](const Collection &left, const Collection &right) {
            return left.dirName.compare(
                right.dirName, Qt::CaseInsensitive) < 0;
        });
    for (const Collection &collection : std::as_const(collections)) {
        QStringList files;
        if (collection.collectionClass == "point_collection") {
            files = matchingRelativeFiles(
                m_rootPath, collection.dirPath, {"*.csv", "*.CSV"}, false);
        } else if (collection.collectionClass == "preprocessed_image_collection"
                   || collection.collectionClass == "processed_image_collection"
                   || collection.collectionClass == "segmentation_collection") {
            files = matchingRelativeFiles(
                m_rootPath, collection.dirPath, imageFilters, false);
        } else {
            continue;
        }
        addItem(collection.dirName,
                collection.collectionClass,
                collection.dirPath,
                files);
    }

    return plan;
}

bool Project::applyContentCleanPlan(const ProjectContentCleanPlan &plan,
                                    QString *errorMessage) const
{
    if (!isValid()) {
        if (errorMessage)
            *errorMessage = "There is no valid project to clean.";
        return false;
    }

    const QString normalizedRoot = normalizedAbsolutePath(m_rootPath);
    for (const ProjectContentCleanItem &item : plan.items) {
        if (!item.selected)
            continue;

        for (const QString &relativePath : item.relativeFiles) {
            const QString absolutePath = normalizedAbsolutePath(
                QDir(m_rootPath).filePath(relativePath));
            if (!pathIsInsideRoot(absolutePath, normalizedRoot)
                || absolutePath == normalizedRoot) {
                if (errorMessage)
                    *errorMessage = "Clean plan contains an invalid path.";
                return false;
            }

            const QFileInfo cleanTarget(absolutePath);
            if (cleanTarget.isDir()) {
                if (errorMessage)
                    *errorMessage = "Clean removes files, not folders.";
                return false;
            }
            if (cleanTarget.exists() && !QFile::remove(absolutePath)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral(
                        "Could not remove file: %1").arg(relativePath);
                }
                return false;
            }
        }
    }

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

QString Project::description() const
{
    return m_description;
}

QString Project::reportFilePath() const
{
    return QDir(m_rootPath).filePath("project-report.md");
}

QString Project::metadataFilePath(const QString &rootPath)
{
    return QDir(rootPath).filePath("project.yaml");
}

QStringList Project::reservedDirectories()
{
    return {
        "raw_files", "sensor_data", "scripts",
        "registry_drafts", "registry_reviews",
        "registry_validations", ".rcaiman"
    };
}

QStringList Project::projectSubdirectories()
{
    return {
        "raw_files", "sensor_data", "scripts",
        "registry_drafts", "registry_reviews", "registry_validations"
    };
}

bool Project::createDirectoryStructure(const QString &rootPath, QString *errorMessage)
{
    QDir root(rootPath);
    const QStringList directories = Project::projectSubdirectories();

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
