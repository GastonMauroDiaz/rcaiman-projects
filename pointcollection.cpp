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

#include "pointcollection.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QDebug>

#include <cmath>

static QVariant convertValue(const QString &raw, QMetaType::Type type)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty() || trimmed == "NA")
        return QVariant();

    switch (type) {
    case QMetaType::Int:
    {
        bool ok = false;
        int v = trimmed.toInt(&ok);
        return ok ? QVariant(v) : QVariant(trimmed);
    }
    case QMetaType::Double:
    {
        bool ok = false;
        double v = trimmed.toDouble(&ok);
        return ok ? QVariant(v) : QVariant(trimmed);
    }
    case QMetaType::Bool:
        return QVariant(trimmed.toUpper() == "TRUE");
    default:
        return QVariant(trimmed);
    }
}

static QMetaType::Type inferColumnType(const QStringList &rawValues)
{
    bool allInt = true;
    bool allDouble = true;
    bool allBool = true;

    for (const QString &raw : rawValues) {
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty() || trimmed == "NA")
            continue;

        if (allInt) {
            bool ok = false;
            trimmed.toInt(&ok);
            if (!ok) allInt = false;
        }

        if (allDouble) {
            bool ok = false;
            trimmed.toDouble(&ok);
            if (!ok) allDouble = false;
        }

        if (allBool) {
            const QString upper = trimmed.toUpper();
            if (upper != "TRUE" && upper != "FALSE")
                allBool = false;
        }

        // Early exit if all failed
        if (!allInt && !allDouble && !allBool)
            break;
    }

    if (allInt)       return QMetaType::Int;
    if (allDouble)    return QMetaType::Double;
    if (allBool)      return QMetaType::Bool;
    return QMetaType::QString;
}

static QStringList parseCsvLine(const QString &line)
{
    QStringList result;
    bool inQuotes = false;
    QString current;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            result.append(current.trimmed());
            current.clear();
        } else {
            current += c;
        }
    }
    result.append(current.trimmed());
    return result;
}

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

    // Parse header
    const QStringList colNames = parseCsvLine(header);
    if (colNames.size() < 2
        || colNames[0] != QStringLiteral("x")
        || colNames[1] != QStringLiteral("y_down")) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "Invalid header. Expected: x,y_down");
        return collection;
    }

    // Build ColumnMeta for all columns
    for (int i = 0; i < colNames.size(); ++i) {
        ColumnMeta meta;
        meta.name = colNames[i].trimmed();
        if (meta.name.startsWith('"') && meta.name.endsWith('"') && meta.name.size() > 1)
            meta.name = meta.name.mid(1, meta.name.size() - 2);
        meta.type = QMetaType::Double; // default, inferred later for extras
        collection.m_columnMeta.append(meta);
    }

    // Read all raw data
    struct RawRow {
        QStringList values;
    };
    QVector<RawRow> rawRows;

    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty())
            continue;

        const QStringList parts = parseCsvLine(line);
        if (parts.size() < 2) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "Invalid point row. Expected x,y_down values: %1")
                        .arg(line);
            }
            collection.m_columnMeta.clear();
            return collection;
        }

        RawRow row;
        row.values = parts;
        rawRows.append(row);
    }

    // If no data rows, return collection with headers only (valid for empty CSVs)
    if (rawRows.isEmpty()) {
        // Headers are already loaded in m_columnMeta, collection is valid
        return collection;
    }

    // Infer types for extra columns (index >= 2)
    const int extraCount = colNames.size() - 2;
    for (int col = 0; col < extraCount; ++col) {
        const int csvCol = col + 2;
        QStringList rawValues;
        rawValues.reserve(rawRows.size());
        for (const RawRow &row : rawRows) {
            if (csvCol < row.values.size())
                rawValues.append(row.values[csvCol]);
            else
                rawValues.append(QString());
        }
        QMetaType::Type inferred = inferColumnType(rawValues);
        collection.m_columnMeta[csvCol].type = inferred;
    }

    // Convert rows to PointRecords
    for (int r = 0; r < rawRows.size(); ++r) {
        const QStringList &vals = rawRows[r].values;
        bool xOk = false;
        bool yDownOk = false;
        const double x = vals[0].toDouble(&xOk);
        const double yDown = vals[1].toDouble(&yDownOk);
        if (!xOk || !yDownOk
            || !std::isfinite(x) || !std::isfinite(yDown)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Invalid coordinates at row %1: %2")
                    .arg(r + 2).arg(vals.join(","));
            collection.m_points.clear();
            collection.m_columnMeta.clear();
            return collection;
        }

        PointRecord rec;
        rec.x = x;
        rec.yDown = yDown;

        for (int c = 2; c < vals.size() && (c - 2) < extraCount; ++c) {
            const QMetaType::Type t = collection.m_columnMeta[c].type;
            rec.attributes.append(convertValue(vals[c], t));
        }
        // Pad missing columns with null QVariant
        while (rec.attributes.size() < extraCount)
            rec.attributes.append(QVariant());

        collection.m_points.append(rec);
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
    for (const QString &csvFile : csvFiles) {
        QString err;
        const PointCollection col = PointCollection::load(dir.filePath(csvFile), &err);
        if (!col.isValid()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Failed to load %1: %2").arg(csvFile, err);
            return PointCollection();
        }
        // Merge: use first file's columnMeta, copy points
        if (result.m_columnMeta.isEmpty())
            result.m_columnMeta = col.m_columnMeta;
        result.m_points.append(col.points());
    }

    return result;
}

PointCollection PointCollection::createEmpty(const QStringList &columnNames)
{
    PointCollection collection;
    if (columnNames.size() < 2
        || columnNames.at(0) != QStringLiteral("x")
        || columnNames.at(1) != QStringLiteral("y_down")) {
        return collection;
    }

    for (const QString &name : columnNames) {
        ColumnMeta meta;
        meta.name = name;
        if (name == QStringLiteral("x")
            || name == QStringLiteral("y_down")) {
            meta.type = QMetaType::Double;
        }
        collection.m_columnMeta.append(meta);
    }
    return collection;
}

bool PointCollection::isValid() const
{
    return m_columnMeta.size() >= 2
        && m_columnMeta.at(0).name == QStringLiteral("x")
        && m_columnMeta.at(1).name == QStringLiteral("y_down");
}

const QVector<PointRecord> &PointCollection::points() const
{
    return m_points;
}

QStringList PointCollection::columnNames() const
{
    QStringList names;
    names.reserve(m_columnMeta.size());
    for (const ColumnMeta &meta : m_columnMeta)
        names.append(meta.name);
    return names;
}

void PointCollection::addPoint(double x, double yDown)
{
    PointRecord rec;
    rec.x = x;
    rec.yDown = yDown;
    // Pad attributes with null QVariants for extra columns
    const int extraCount = qMax(0, m_columnMeta.size() - 2);
    rec.attributes.resize(extraCount);
    m_points.append(rec);
}

void PointCollection::removePoint(int index)
{
    if (index < 0 || index >= m_points.size())
        return;
    m_points.removeAt(index);
}

void PointCollection::movePoint(int index, double x, double yDown)
{
    if (index < 0 || index >= m_points.size())
        return;
    m_points[index].x = x;
    m_points[index].yDown = yDown;
}

bool PointCollection::save(const QString &filePath, QString *errorMessage) const
{
    if (!isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "Invalid point collection. Expected columns: x,y_down");
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot write points file: %1").arg(filePath);
        return false;
    }

    QTextStream stream(&file);
    stream.setRealNumberNotation(QTextStream::SmartNotation);
    stream.setRealNumberPrecision(15);

    // Write header with all column names
    QStringList headerParts;
    for (const ColumnMeta &meta : m_columnMeta)
        headerParts.append(meta.name);
    stream << headerParts.join(",") << "\n";

    // Write data rows
    const int extraCount = qMax(0, m_columnMeta.size() - 2);
    for (const PointRecord &rec : m_points) {
        stream << rec.x << ',' << rec.yDown;
        for (int i = 0; i < extraCount; ++i) {
            stream << ',';
            if (i < rec.attributes.size() && rec.attributes[i].isValid()) {
                const QVariant &val = rec.attributes[i];
                const QMetaType::Type t = (i + 2 < m_columnMeta.size())
                    ? m_columnMeta[i + 2].type : QMetaType::Double;

                switch (t) {
                case QMetaType::Int:
                    stream << val.toInt();
                    break;
                case QMetaType::Double:
                    stream << val.toDouble();
                    break;
                case QMetaType::Bool:
                    stream << (val.toBool() ? "TRUE" : "FALSE");
                    break;
                default:
                    // Quote strings that contain commas or quotes
                    {
                        const QString s = val.toString();
                        if (s.contains(',') || s.contains('"'))
                            stream << '"' << s << '"';
                        else
                            stream << s;
                    }
                    break;
                }
            }
        }
        stream << "\n";
    }
    stream.flush();

    if (stream.status() != QTextStream::Ok) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Error writing points file: %1").arg(filePath);
        return false;
    }

    return true;
}
