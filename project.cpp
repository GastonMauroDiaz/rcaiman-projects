#include "project.h"

#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
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

bool copyDirectoryRecursively(const QString &sourcePath,
                              const QString &destinationPath,
                              QString *errorMessage)
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
        const QString relativePath = source.relativeFilePath(sourceEntryPath);
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

bool Project::registerCollection(const QString &projectRootPath,
                                 const QString &dirPath,
                                 const QString &collectionClass,
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
    stream << "class: " << collectionClass << "\n";
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
    stream << "project_name: " << m_name << '\n';
    stream << "created: " << m_created << '\n';
    stream << "rcaiman_projects_version: " << ApplicationVersion << '\n';

    return true;
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
                 QDate::currentDate().toString(Qt::ISODate));
    if (!copy.save(errorMessage)) {
        QDir(destination).removeRecursively();
        return {};
    }

    return copy;
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
        "Sensor images",
        matchingRelativeFiles(
            m_rootPath, QDir(m_rootPath).filePath("sensor_data"), imageFilters));

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

            if (QFileInfo::exists(absolutePath) && !QFile::remove(absolutePath)) {
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

QString Project::metadataFilePath(const QString &rootPath)
{
    return QDir(rootPath).filePath("project.yaml");
}

QStringList Project::reservedDirectories()
{
    return {"raw_files", "sensor_data", "scripts", ".rcaiman"};
}

QStringList Project::projectSubdirectories()
{
    return {"raw_files", "sensor_data", "image_metadata", "scripts"};
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
