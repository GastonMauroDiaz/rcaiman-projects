#include "pointcollection.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStringList>

PointCollection PointCollection::load(const QString &filePath, QString *errorMessage)
{
    PointCollection collection;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot open points file: %1").arg(filePath);
        return collection;
    }

    QTextStream stream(&file);
    QString header = stream.readLine();
    if (header.isNull()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Empty points file: %1").arg(filePath);
        return collection;
    }

    QStringList cols = header.split(',', Qt::SkipEmptyParts);
    for (QString &col : cols) {
        col = col.trimmed();
        if (col.startsWith('"') && col.endsWith('"') && col.size() > 1)
            col = col.mid(1, col.size() - 2);
    }
    if (cols.size() < 2 || cols[0].compare("row", Qt::CaseInsensitive) != 0 || cols[1].compare("col", Qt::CaseInsensitive) != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid header. Expected: row,col");
        return collection;
    }

    int lineNum = 1;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty())
            continue;

        const QStringList parts = line.split(',', Qt::SkipEmptyParts);
        if (parts.size() < 2)
            continue;

        bool okR = false, okC = false;
        const double row = parts[0].toDouble(&okR);
        const double col = parts[1].toDouble(&okC);
        if (!okR || !okC) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Invalid coordinates at line %1: %2").arg(lineNum).arg(line);
            collection.m_points.clear();
            return collection;
        }

        PointRecord rec;
        rec.row = row;
        rec.col = col;
        collection.m_points.append(rec);
        ++lineNum;
    }

    return collection;
}

PointCollection PointCollection::loadFromDirectory(const QString &dirPath, QString *errorMessage)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Directory does not exist: %1").arg(dirPath);
        return PointCollection();
    }

    const QStringList csvFiles = dir.entryList(QStringList() << "*.csv", QDir::Files);
    if (csvFiles.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No CSV files found in: %1").arg(dirPath);
        return PointCollection();
    }

    PointCollection result;
    int totalPoints = 0;
    for (const QString &csvFile : csvFiles) {
        QString err;
        const PointCollection col = PointCollection::load(dir.filePath(csvFile), &err);
        if (!col.isValid()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Failed to load %1: %2").arg(csvFile, err);
            return PointCollection();
        }
        result.m_points.append(col.points());
        totalPoints += col.points().size();
    }

    if (!result.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No valid points found in directory: %1").arg(dirPath);
        return PointCollection();
    }

    return result;
}

bool PointCollection::isValid() const
{
    return !m_points.isEmpty();
}

const QVector<PointRecord> &PointCollection::points() const
{
    return m_points;
}