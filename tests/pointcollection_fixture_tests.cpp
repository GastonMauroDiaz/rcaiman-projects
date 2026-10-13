// SPDX-License-Identifier: GPL-3.0-or-later

#include "pointcollection.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>
#include <limits>

namespace {
const QStringList ExpectedColumns = {
    "x", "y_down", "col", "row", "Red", "Green", "Blue",
    "mask 10", "ratio with sky_cie"
};
const QVector<QMetaType::Type> ExpectedTypes = {
    QMetaType::Double, QMetaType::Double,
    QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::Int,
    QMetaType::Bool, QMetaType::Double
};

bool matches(const PointCollection &collection,
             const QVector<PointRecord> &expected)
{
    if (!collection.isValid() || collection.columnNames() != ExpectedColumns
        || collection.recordCount() != expected.size()) {
        qCritical() << "Fixture schema or record count changed";
        return false;
    }
    const auto &metadata = collection.columnMeta();
    for (int column = 0; column < metadata.size(); ++column) {
        if (metadata.at(column).type != ExpectedTypes.at(column)) {
            qCritical() << "Fixture column type changed:" << ExpectedColumns.at(column);
            return false;
        }
    }
    for (int index = 0; index < expected.size(); ++index) {
        const auto &actual = collection.points().at(index);
        const auto &reference = expected.at(index);
        if (actual.x != reference.x || actual.yDown != reference.yDown
            || actual.attributes != reference.attributes) {
            qCritical() << "Fixture data changed at record" << index + 1;
            return false;
        }
    }
    return true;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    if (argc > 2) {
        qCritical() << "Usage: pointcollection_fixture_tests [output-directory]";
        return 1;
    }

    QFile source(QStringLiteral(POINT_FIXTURE_PATH));
    if (!source.open(QIODevice::ReadOnly)) {
        qCritical() << "Cannot read the reference CSV:" << source.errorString();
        return 1;
    }
    const QByteArray sourceBytes = source.readAll();
    source.close();
    const auto lines = QString::fromUtf8(sourceBytes).split('\n', Qt::SkipEmptyParts);
    if (lines.size() != 101) {
        qCritical() << "The reference CSV must contain 100 records and its header";
        return 1;
    }

    // This known fixture is an independent oracle, not a general CSV parser.
    QVector<PointRecord> expected;
    int missingCount = 0;
    int infiniteCount = 0;
    int finiteCount = 0;
    for (int row = 1; row < lines.size(); ++row) {
        const auto fields = lines.at(row).trimmed().split(',');
        if (fields.size() != ExpectedColumns.size())
            return 1;
        PointRecord record;
        for (int column = 0; column < fields.size(); ++column) {
            bool ok = false;
            if (column < 2) {
                const double value = fields.at(column).toDouble(&ok);
                if (column == 0)
                    record.x = value;
                else
                    record.yDown = value;
            } else if (column < 7) {
                const int value = fields.at(column).toInt(&ok);
                record.attributes.append(value);
            } else if (column == 7) {
                const QString value = fields.at(column);
                ok = value == "TRUE" || value == "FALSE";
                record.attributes.append(value == "TRUE");
            } else if (fields.at(column) == "NA") {
                record.attributes.append(QVariant());
                ++missingCount;
                ok = true;
            } else if (fields.at(column) == "Inf") {
                record.attributes.append(std::numeric_limits<double>::infinity());
                ++infiniteCount;
                ok = true;
            } else {
                const double value = fields.at(column).toDouble(&ok);
                ok = ok && std::isfinite(value);
                record.attributes.append(value);
                ++finiteCount;
            }
            if (!ok) {
                qCritical() << "Invalid numeric reference at row/column" << row << column;
                return 1;
            }
        }
        const int col = record.attributes.at(0).toInt();
        const int rowIndex = record.attributes.at(1).toInt();
        if (record.x != col - 0.5 || record.yDown != rowIndex - 0.5
            || std::floor(record.x) + 1 != col
            || std::floor(record.yDown) + 1 != rowIndex) {
            qCritical() << "Reference coordinate/index contract failed at record" << row;
            return 1;
        }
        expected.append(record);
    }
    if (missingCount != 87 || infiniteCount != 2 || finiteCount != 11) {
        qCritical() << "Reference must contain 87 NA, 2 Inf and 11 finite ratios";
        return 1;
    }

    QString error;
    const auto original = PointCollection::load(source.fileName(), &error);
    if (!error.isEmpty() || !matches(original, expected)) {
        qCritical() << "Reading the reference CSV failed:" << error;
        return 1;
    }

    QTemporaryDir temporary;
    if (!temporary.isValid())
        return 1;
    const QDir output(argc == 2 ? application.arguments().at(1) : temporary.path());
    if (!output.exists()) {
        qCritical() << "Output directory does not exist:" << output.path();
        return 1;
    }
    const QString unchangedPath = output.filePath("unchanged.csv");
    const QString editedPath = output.filePath("edited.csv");
    if (QFile::exists(unchangedPath) || QFile::exists(editedPath)) {
        qCritical() << "Refusing to overwrite existing test output";
        return 1;
    }
    if (!original.save(unchangedPath, &error)
        || !matches(PointCollection::load(unchangedPath, &error), expected)) {
        qCritical() << "Unchanged round trip failed:" << error;
        return 1;
    }

    PointCollection edited = original;
    expected[0].x += 1.25;
    expected[0].yDown -= 2.25;
    edited.movePoint(0, expected[0].x, expected[0].yDown);
    // All additional columns are script-provided attributes, not fields to recompute.
    if (!matches(edited, expected) || !edited.save(editedPath, &error)
        || !matches(PointCollection::load(editedPath, &error), expected)) {
        qCritical() << "Edited round trip failed:" << error;
        return 1;
    }
    if (!source.open(QIODevice::ReadOnly) || source.readAll() != sourceBytes) {
        qCritical() << "Reference CSV was modified";
        return 1;
    }
    qInfo() << "V3 fixture: 100 records, 87 NA, 2 Inf, logical values and column names"
               " preserved; only the edited coordinates changed.";
    return 0;
}
