#include "projectfilesystemmodel.h"
#include "project.h"

#include <QBrush>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {
QString valueAfterColon(const QString &line)
{
    const int separator = line.indexOf(':');
    return separator >= 0 ? line.mid(separator + 1).trimmed() : QString();
}
}

ProjectFileSystemModel::ProjectFileSystemModel(QObject *parent)
    : QFileSystemModel(parent)
{
    auto refreshParent = [this](const QModelIndex &parent) {
        if (parent.isValid())
            emit dataChanged(parent, parent, {Qt::DisplayRole});
    };

    connect(this, &QAbstractItemModel::rowsInserted,
            this, [refreshParent](const QModelIndex &parent, int, int) {
                refreshParent(parent);
            });
    connect(this, &QAbstractItemModel::rowsRemoved,
            this, [refreshParent](const QModelIndex &parent, int, int) {
                refreshParent(parent);
            });
}

QVariant ProjectFileSystemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0)
        return QFileSystemModel::data(index, role);

    const QFileInfo info = fileInfo(index);
    if (!info.isDir())
        return QFileSystemModel::data(index, role);

    const bool directProjectChild = isDirectProjectChild(info);
    const bool reserved = directProjectChild
        && Project::reservedDirectories().contains(
            info.fileName(), Qt::CaseInsensitive);

    if (role == Qt::ForegroundRole && reserved)
        return QBrush(QColor(35, 105, 190));

    if (role == Qt::ToolTipRole
        && reserved
        && info.fileName().compare(
            ".rcaiman", Qt::CaseInsensitive) == 0) {
        return QStringLiteral(
            ".rcaiman stores the application's session state.");
    }

    if (role != Qt::DisplayRole)
        return QFileSystemModel::data(index, role);

    QString className;
    if (directProjectChild)
        className = collectionClass(info.absoluteFilePath());

    const bool countedReserved =
        reserved
        && (info.fileName().compare("raw_files", Qt::CaseInsensitive) == 0
            || info.fileName().compare("sensor_data", Qt::CaseInsensitive) == 0);
    if (className.isEmpty() && !countedReserved)
        return QFileSystemModel::data(index, role);

    const int count = relevantFileCount(info, className);
    return QStringLiteral("%1 (%2)").arg(info.fileName()).arg(count);
}

void ProjectFileSystemModel::setProjectRoot(const QString &path)
{
    m_projectRoot = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

void ProjectFileSystemModel::refreshDirectory(const QString &path)
{
    const QModelIndex directoryIndex = index(path);
    if (directoryIndex.isValid())
        emit dataChanged(directoryIndex, directoryIndex, {Qt::DisplayRole});
}

bool ProjectFileSystemModel::isDirectProjectChild(const QFileInfo &info) const
{
    if (m_projectRoot.isEmpty())
        return false;

    const QString parentPath =
        QDir::cleanPath(info.absoluteDir().absolutePath());
#ifdef Q_OS_WIN
    return parentPath.compare(m_projectRoot, Qt::CaseInsensitive) == 0;
#else
    return parentPath == m_projectRoot;
#endif
}

QString ProjectFileSystemModel::collectionClass(
    const QString &directoryPath) const
{
    QFile file(QDir(directoryPath).filePath("collection.yaml"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.startsWith("class:"))
            return valueAfterColon(line);
    }
    return {};
}

int ProjectFileSystemModel::relevantFileCount(
    const QFileInfo &info,
    const QString &className) const
{
    QStringList filters;
    if (info.fileName().compare("raw_files", Qt::CaseInsensitive) == 0) {
        filters = {
            "*.CR2", "*.CR3", "*.NEF", "*.ARW", "*.DNG", "*.RAF",
            "*.ORF", "*.RW2", "*.PEF", "*.SRW", "*.RAW", "*.DCR", "*.KDC"
        };
    } else if (info.fileName().compare("sensor_data", Qt::CaseInsensitive) == 0
               || className == "preprocessed_image_collection"
               || className == "processed_image_collection"
               || className == "segmentation_collection") {
        filters = {"*.tif", "*.tiff", "*.TIF", "*.TIFF"};
    } else if (className == "point_collection") {
        filters = {"*.csv", "*.CSV"};
    } else {
        return 0;
    }

    return QDir(info.absoluteFilePath())
        .entryList(filters, QDir::Files | QDir::NoSymLinks)
        .size();
}
