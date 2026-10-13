// SPDX-License-Identifier: GPL-3.0-or-later

#include "pointcollection.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>

namespace {

bool writeTextFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(contents) == contents.size();
}

bool nearlyEqual(double left, double right)
{
    return std::abs(left - right) < 1.0e-12;
}

}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid())
        return 1;

    const QString validPath =
        QDir(directory.path()).filePath("valid.csv");
    if (!writeTextFile(
            validPath,
            "\"x\",\"y_down\",\"label\"\n"
            "0.5,1.5,first\n"
            "10.25,20.75,second\n")) {
        return 2;
    }

    QString error;
    const PointCollection loaded = PointCollection::load(validPath, &error);
    if (!loaded.isValid() || !error.isEmpty())
        return 3;
    if (loaded.columnNames()
        != QStringList({"x", "y_down", "label"})) {
        return 4;
    }
    if (loaded.recordCount() != 2
        || !nearlyEqual(loaded.points().at(0).x, 0.5)
        || !nearlyEqual(loaded.points().at(0).yDown, 1.5)
        || loaded.points().at(1).attributes.value(0).toString()
            != QStringLiteral("second")) {
        return 5;
    }

    const QString legacyPath =
        QDir(directory.path()).filePath("legacy.csv");
    if (!writeTextFile(legacyPath, "row,col\n1,2\n"))
        return 6;
    error.clear();
    if (PointCollection::load(legacyPath, &error).isValid()
        || !error.contains("x,y_down")) {
        return 7;
    }

    const QString invalidPath =
        QDir(directory.path()).filePath("invalid.csv");
    if (!writeTextFile(invalidPath, "x,y_down\n1,not-a-number\n"))
        return 8;
    error.clear();
    if (PointCollection::load(invalidPath, &error).isValid()
        || !error.contains("Invalid coordinates")) {
        return 9;
    }

    PointCollection created = PointCollection::createEmpty();
    if (!created.isValid())
        return 10;
    created.addPoint(123.123456789012, 456.987654321098);
    const QString savedPath =
        QDir(directory.path()).filePath("saved.csv");
    error.clear();
    if (!created.save(savedPath, &error))
        return 11;

    const PointCollection roundTrip = PointCollection::load(savedPath, &error);
    if (!roundTrip.isValid() || roundTrip.recordCount() != 1)
        return 12;
    if (roundTrip.columnNames() != QStringList({"x", "y_down"})
        || !nearlyEqual(
            roundTrip.points().first().x, 123.123456789012)
        || !nearlyEqual(
            roundTrip.points().first().yDown, 456.987654321098)) {
        return 13;
    }

    const PointCollection legacyEmpty =
        PointCollection::createEmpty({"row", "col"});
    if (legacyEmpty.isValid())
        return 14;

    return 0;
}
