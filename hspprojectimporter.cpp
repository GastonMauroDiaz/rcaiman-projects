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

#include "hspprojectimporter.h"
#include "collectionstylefile.h"
#include "pgmreader.h"
#include "tiffwriter.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringConverter>
#include <QTextStream>

#include <algorithm>

namespace {
const QStringList RawFilters = {
    "*.CR2", "*.CR3", "*.NEF", "*.ARW", "*.DNG", "*.RAF",
    "*.ORF", "*.RW2", "*.PEF", "*.SRW", "*.RAW", "*.DCR", "*.KDC",
    "*.cr2", "*.cr3", "*.nef", "*.arw", "*.dng", "*.raf",
    "*.orf", "*.rw2", "*.pef", "*.srw", "*.raw", "*.dcr", "*.kdc"
};

struct HspImageEntry
{
    QString baseName;
    QString groupName;
    QString groupToken;
    QString pgmPath;
};

QString propertyValue(const QString &filePath, const QString &key)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        const int separator = line.indexOf('=');
        if (separator <= 0)
            continue;
        if (line.left(separator).trimmed() != key)
            continue;

        QString value = line.mid(separator + 1).trimmed();
        value.replace("\\:", ":");
        value.replace("\\ ", " ");
        value.replace("\\\\", "\\");
        return value;
    }
    return {};
}

QString firstLegacyDateLine(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith('#'))
            line.remove(0, 1);
        return line.trimmed();
    }
    return {};
}

QString safeToken(QString text)
{
    text = text.trimmed();
    if (text.isEmpty())
        text = "ungrouped";
    text.replace(QRegularExpression("[^A-Za-z0-9_-]+"), "_");
    text.replace(QRegularExpression("_+"), "_");
    text = text.trimmed();
    while (text.startsWith('_'))
        text.remove(0, 1);
    while (text.endsWith('_'))
        text.chop(1);
    return text.isEmpty() ? QStringLiteral("ungrouped") : text;
}

bool writeTextFile(const QString &path,
                   const QString &text,
                   QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not write file: %1").arg(path);
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << text;
    return true;
}

QString numberText(double value)
{
    return QString::number(value, 'g', 15);
}

QString tsvField(QString value)
{
    if (!value.contains('\t')
        && !value.contains('\n')
        && !value.contains('\r')
        && !value.contains('"')) {
        return value;
    }

    value.replace("\"", "\"\"");
    return QStringLiteral("\"%1\"").arg(value);
}

QString decimalText(QString value)
{
    value = value.trimmed();
    value.replace(',', '.');
    return value;
}

bool writePointCsv(const QString &path,
                   const QVector<QVector<double>> &rows,
                   const QStringList &columns,
                   QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not write CSV: %1").arg(path);
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << columns.join(',') << '\n';
    for (const QVector<double> &row : rows) {
        QStringList fields;
        fields.reserve(row.size());
        for (double value : row)
            fields.append(numberText(value));
        stream << fields.join(',') << '\n';
    }
    return true;
}

QVector<QVector<double>> parseSkyPoints(const QString &path,
                                        QStringList *warnings)
{
    QVector<QVector<double>> rows;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (warnings)
            warnings->append(QStringLiteral("Could not read sky points: %1").arg(path));
        return rows;
    }

    QTextStream stream(&file);
    const QString firstLine = stream.readLine().trimmed();
    bool ok = false;
    const int expectedCount = firstLine.toInt(&ok);
    if (!ok || expectedCount < 0) {
        if (warnings)
            warnings->append(QStringLiteral("Invalid sky point count in %1").arg(path));
        return rows;
    }

    const QString content = stream.readAll();
    const QStringList tokens =
        content.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        const QStringList parts = token.split('.');
        if (parts.size() != 3) {
            if (warnings)
                warnings->append(QStringLiteral("Invalid sky point token '%1' in %2")
                                     .arg(token, path));
            continue;
        }
        bool colOk = false;
        bool rowOk = false;
        bool typeOk = false;
        const double col = parts.at(0).toDouble(&colOk);
        const double row = parts.at(1).toDouble(&rowOk);
        const double type = parts.at(2).toDouble(&typeOk);
        if (!colOk || !rowOk || !typeOk) {
            if (warnings)
                warnings->append(QStringLiteral("Invalid sky point token '%1' in %2")
                                     .arg(token, path));
            continue;
        }
        rows.append({row, col, type});
    }

    if (rows.size() != expectedCount && warnings) {
        warnings->append(QStringLiteral(
            "Sky point count mismatch in %1: expected %2, imported %3.")
                         .arg(path)
                         .arg(expectedCount)
                         .arg(rows.size()));
    }
    return rows;
}

QVector<QVector<double>> parseSunMark(const QString &path,
                                      QStringList *warnings)
{
    QVector<QVector<double>> rows;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (warnings)
            warnings->append(QStringLiteral("Could not read sun mark: %1").arg(path));
        return rows;
    }

    QTextStream stream(&file);
    const QString token = stream.readAll().trimmed();
    if (token.isEmpty())
        return rows;

    const QStringList parts = token.split('.');
    if (parts.size() != 2) {
        if (warnings)
            warnings->append(QStringLiteral("Invalid sun mark token '%1' in %2")
                                 .arg(token, path));
        return rows;
    }

    bool colOk = false;
    bool rowOk = false;
    const double col = parts.at(0).toDouble(&colOk);
    const double row = parts.at(1).toDouble(&rowOk);
    if (!colOk || !rowOk) {
        if (warnings)
            warnings->append(QStringLiteral("Invalid sun mark token '%1' in %2")
                                 .arg(token, path));
        return rows;
    }
    rows.append({row, col});
    return rows;
}

bool parseOptimizedSkyCoefficients(const QString &path,
                                   QHash<QString, double> *coefficients,
                                   QString *errorMessage)
{
    coefficients->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("could not read source file");
        return false;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        const int separator = line.indexOf('=');
        if (separator <= 0)
            continue;

        const QString key = line.left(separator).trimmed();
        const QString rawValue = decimalText(line.mid(separator + 1));
        bool ok = false;
        const double value = rawValue.toDouble(&ok);
        if (!ok) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("invalid coefficient %1=%2")
                                    .arg(key, rawValue);
            }
            return false;
        }
        coefficients->insert(key, value);
    }

    for (const QString &key : QStringList{"a", "b", "c", "d", "e"}) {
        if (!coefficients->contains(key)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("missing coefficient %1").arg(key);
            return false;
        }
    }
    return true;
}

bool writeOptimizedSkyCoefReadme(const QString &collectionDir,
                                 const QString &groupToken,
                                 QString *errorMessage)
{
    QString text;
    QTextStream stream(&text);
    stream << "# Optimized sky coefficients\n\n";
    stream << "This collection was generated from legacy HSP "
              "`*_opt-parameters.conf` files for group `"
           << groupToken << "`.\n\n";
    stream << "`opt_sky_coef.tsv` contains one row per imported image. "
              "The coefficients are ordered as `a`, `b`, `c`, `d`, `e` "
              "and decimal commas from HSP were converted to decimal points.\n\n";
    stream << "## R usage\n\n";
    stream << "Use `read.delim()` to read the TSV file.\n";
    return writeTextFile(QDir(collectionDir).filePath("README.md"),
                         text, errorMessage);
}

bool writeOptimizedSkyCoefTable(const QString &collectionDir,
                                const QString &sourcePath,
                                const QVector<HspImageEntry> &entries,
                                QStringList *warnings,
                                QString *errorMessage)
{
    QFile file(QDir(collectionDir).filePath("opt_sky_coef.tsv"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not write optimized sky coefficients: %1")
                                .arg(file.fileName());
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "basename\ta\tb\tc\td\te\tsource_file\tstatus\terror\n";
    const QDir source(sourcePath);
    for (const HspImageEntry &entry : entries) {
        const QString relativeSource =
            QStringLiteral("manipulate/%1_opt-parameters.conf").arg(entry.baseName);
        const QString absoluteSource = source.filePath(relativeSource);

        QHash<QString, double> coefficients;
        QString parseError;
        const bool ok = QFileInfo::exists(absoluteSource)
            && parseOptimizedSkyCoefficients(absoluteSource, &coefficients, &parseError);
        if (!QFileInfo::exists(absoluteSource))
            parseError = "missing source file";

        QStringList fields;
        fields << entry.baseName;
        for (const QString &key : QStringList{"a", "b", "c", "d", "e"}) {
            fields << (ok ? numberText(coefficients.value(key)) : QString());
        }
        fields << QDir::fromNativeSeparators(relativeSource);
        fields << (ok ? "ok" : "error");
        fields << (ok ? QString() : parseError);

        for (QString &field : fields)
            field = tsvField(field);
        stream << fields.join('\t') << '\n';

        if (!ok && warnings) {
            warnings->append(QStringLiteral("Could not import optimized sky coefficients for %1: %2")
                                 .arg(entry.baseName, parseError));
        }
    }
    return true;
}

QString markdownList(const QStringList &items, const QString &emptyText)
{
    if (items.isEmpty())
        return QStringLiteral("- %1\n").arg(emptyText);

    QString text;
    for (const QString &item : items)
        text += QStringLiteral("- %1\n").arg(item);
    return text;
}

void collectOmitted(const QString &sourcePath, HspImportReport *report)
{
    if (!report)
        return;

    const QDir source(sourcePath);
    const QStringList patterns = {
        "manipulate/*_masks.conf",
        "manipulate/*_polygons.conf",
        "manipulate/*_model.png",
        "manipulate/*_parameters.conf",
        "manipulate/*_settings.conf",
        "manipulate/*_statistics.conf",
        "results/*_transparency.pgm"
    };

    for (const QString &pattern : patterns) {
        const QString folder = QFileInfo(pattern).path();
        const QString filePattern = QFileInfo(pattern).fileName();
        QDir dir(source.filePath(folder));
        const QFileInfoList files =
            dir.entryInfoList({filePattern}, QDir::Files, QDir::Name);
        for (const QFileInfo &file : files) {
            report->omitted.append(
                QDir::fromNativeSeparators(source.relativeFilePath(file.absoluteFilePath())));
        }
    }
}

bool writeImportReport(const QString &projectPath,
                       const QString &sourcePath,
                       const HspImportReport &report,
                       QString *errorMessage)
{
    QString text;
    QTextStream stream(&text);
    stream << "# Legacy HSP import\n\n";
    stream << "- Source: `" << QDir::fromNativeSeparators(sourcePath) << "`\n";
    stream << "- Imported at: "
           << QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
           << " UTC\n";
    stream << "- RAW files copied: " << report.rawFilesCopied << "\n";
    stream << "- PGM images converted: " << report.pgmFilesConverted << "\n";
    stream << "- Sky point CSV files: " << report.skyPointFilesWritten << "\n";
    stream << "- Sun mark CSV files: " << report.sunMarkFilesWritten << "\n";
    stream << "- Optimized sky coefficient TSV files: "
           << report.optSkyCoefTablesWritten << "\n\n";

    stream << "## Groups\n\n";
    stream << markdownList(report.groups, "No HSP groups were imported.") << "\n";

    stream << "## Warnings\n\n";
    stream << markdownList(report.warnings, "No warnings.") << "\n";

    stream << "## Omitted legacy artifacts\n\n";
    stream << markdownList(report.omitted, "No legacy artifacts were omitted.") << "\n";

    return writeTextFile(QDir(projectPath).filePath("legacy_hsp_import.md"),
                         text, errorMessage);
}
}

Project HspProjectImporter::importProject(const HspImportOptions &options,
                                          HspImportReport *report,
                                          QString *errorMessage)
{
    HspImportReport localReport;
    HspImportReport &out = report ? *report : localReport;
    out = HspImportReport{};

    const QString sourcePath =
        QFileInfo(options.sourceProjectPath).absoluteFilePath();
    const QString destinationPath =
        QFileInfo(options.destinationProjectPath).absoluteFilePath();
    const QDir source(sourcePath);

    if (!QFileInfo::exists(source.filePath("project.conf"))
        || !QFileInfo(source.filePath("raw")).isDir()
        || !QFileInfo(source.filePath("manipulate")).isDir()) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "The selected folder is not a supported legacy HSP project.");
        return {};
    }

    if (QFileInfo::exists(destinationPath)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("The destination project folder already exists.");
        return {};
    }

    Project project = Project::create(
        destinationPath, options.projectName, options.description, errorMessage);
    if (!project.isValid())
        return {};

    auto failAndRemove = [&](const QString &message) {
        QDir(destinationPath).removeRecursively();
        if (errorMessage)
            *errorMessage = message;
        return Project{};
    };

    QDir destination(destinationPath);
    QDir rawDestination(destination.filePath("raw_files"));
    const QFileInfoList rawFiles =
        QDir(source.filePath("raw")).entryInfoList(
            RawFilters, QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo &rawFile : rawFiles) {
        const QString target = rawDestination.filePath(rawFile.fileName());
        if (QFile::copy(rawFile.absoluteFilePath(), target)) {
            ++out.rawFilesCopied;
        } else {
            out.warnings.append(QStringLiteral("Could not copy RAW file: %1")
                                    .arg(rawFile.fileName()));
        }
    }

    QVector<HspImageEntry> images;
    QSet<QString> groupTokens;
    const QFileInfoList pgmFiles =
        QDir(source.filePath("manipulate")).entryInfoList(
            {"*.pgm", "*.PGM"}, QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo &pgmFile : pgmFiles) {
        const QString baseName = pgmFile.completeBaseName();
        QString groupName = propertyValue(
            source.filePath(QStringLiteral("info/%1.conf").arg(baseName)),
            "group_name");
        if (groupName.isEmpty())
            groupName = "ungrouped";
        const QString groupToken = safeToken(groupName);
        groupTokens.insert(groupToken);
        images.append({baseName, groupName, groupToken, pgmFile.absoluteFilePath()});
    }

    QStringList sortedGroupTokens = groupTokens.values();
    sortedGroupTokens.sort(Qt::CaseInsensitive);
    QHash<QString, QString> preprocessedDirs;
    QHash<QString, QString> skyPointDirs;
    QHash<QString, QString> sunMarkDirs;
    QHash<QString, QString> optSkyCoefDirs;
    for (const QString &groupToken : sortedGroupTokens) {
        const QString preprocessedDir =
            destination.filePath(QStringLiteral("preprocessed_%1").arg(groupToken));
        const QString skyDir =
            destination.filePath(QStringLiteral("sky_points_%1").arg(groupToken));
        const QString sunDir =
            destination.filePath(QStringLiteral("sun_marks_%1").arg(groupToken));
        const QString optSkyCoefDir =
            destination.filePath(QStringLiteral("opt_sky_coef_%1").arg(groupToken));
        if (!destination.mkpath(QFileInfo(preprocessedDir).fileName())
            || !destination.mkpath(QFileInfo(skyDir).fileName())
            || !destination.mkpath(QFileInfo(sunDir).fileName())
            || !destination.mkpath(QFileInfo(optSkyCoefDir).fileName())) {
            return failAndRemove(QStringLiteral("Could not create HSP import collections."));
        }

        QString registrationError;
        if (!Project::registerCollection(
                project.rootPath(), preprocessedDir,
                "preprocessed_image_collection", {}, {}, {}, {},
                &registrationError)) {
            return failAndRemove(registrationError);
        }
        CollectionStyleData imageStyle;
        imageStyle.kind = CollectionStyleData::Kind::Image;
        imageStyle.imageStyle.render.mode = RasterRenderMode::SingleBand;
        imageStyle.imageStyle.render.colorRamp = ColorRamp::Gray;
        imageStyle.imageStyle.render.stretchMode = StretchMode::None;
        imageStyle.imageStyle.render.tailTrim = false;
        imageStyle.imageStyle.render.manualRangeEnabled = false;
        imageStyle.imageStyle.render.band = 0;
        QString styleError;
        if (!CollectionStyleFile::save(preprocessedDir, imageStyle, &styleError))
            out.warnings.append(styleError);

        if (!Project::registerCollection(
                project.rootPath(), skyDir,
                "point_collection", preprocessedDir, {}, {}, {},
                &registrationError)) {
            return failAndRemove(registrationError);
        }
        if (!Project::registerCollection(
                project.rootPath(), sunDir,
                "point_collection", preprocessedDir, {}, {}, {},
                &registrationError)) {
            return failAndRemove(registrationError);
        }
        if (!Project::registerCollection(
                project.rootPath(), optSkyCoefDir,
                "other", {}, {}, {}, {},
                &registrationError)) {
            return failAndRemove(registrationError);
        }
        QString readmeError;
        if (!writeOptimizedSkyCoefReadme(optSkyCoefDir, groupToken, &readmeError))
            out.warnings.append(readmeError);

        preprocessedDirs.insert(groupToken, preprocessedDir);
        skyPointDirs.insert(groupToken, skyDir);
        sunMarkDirs.insert(groupToken, sunDir);
        optSkyCoefDirs.insert(groupToken, optSkyCoefDir);
        out.groups.append(groupToken);
    }

    for (const HspImageEntry &entry : std::as_const(images)) {
        QString readError;
        RasterData data = PgmReader::load(entry.pgmPath, &readError);
        if (data.isNull()) {
            out.warnings.append(QStringLiteral("Could not read PGM %1: %2")
                                    .arg(QFileInfo(entry.pgmPath).fileName(),
                                         readError));
            continue;
        }

        const QString tifPath =
            QDir(preprocessedDirs.value(entry.groupToken))
                .filePath(entry.baseName + ".tif");
        QString writeError;
        if (TiffWriter::write(tifPath, data, &writeError)) {
            ++out.pgmFilesConverted;
        } else {
            out.warnings.append(QStringLiteral("Could not write TIFF %1: %2")
                                    .arg(QFileInfo(tifPath).fileName(),
                                         writeError));
            continue;
        }

        const QString pointsPath =
            source.filePath(QStringLiteral("manipulate/%1_points.conf")
                                .arg(entry.baseName));
        if (QFileInfo::exists(pointsPath)) {
            const QVector<QVector<double>> rows =
                parseSkyPoints(pointsPath, &out.warnings);
            QString csvError;
            const QString csvPath =
                QDir(skyPointDirs.value(entry.groupToken))
                    .filePath(entry.baseName + ".csv");
            if (writePointCsv(csvPath, rows, {"row", "col", "type"}, &csvError)) {
                ++out.skyPointFilesWritten;
            } else {
                out.warnings.append(csvError);
            }
        }

        const QString sunPath =
            source.filePath(QStringLiteral("manipulate/%1_sun.conf")
                                .arg(entry.baseName));
        if (QFileInfo::exists(sunPath)) {
            const QVector<QVector<double>> rows =
                parseSunMark(sunPath, &out.warnings);
            QString csvError;
            const QString csvPath =
                QDir(sunMarkDirs.value(entry.groupToken))
                    .filePath(entry.baseName + ".csv");
            if (writePointCsv(csvPath, rows, {"row", "col"}, &csvError)) {
                ++out.sunMarkFilesWritten;
            } else {
                out.warnings.append(csvError);
            }
        }
    }

    for (const QString &groupToken : std::as_const(sortedGroupTokens)) {
        QVector<HspImageEntry> groupEntries;
        for (const HspImageEntry &entry : std::as_const(images)) {
            if (entry.groupToken == groupToken)
                groupEntries.append(entry);
        }

        QString tsvError;
        if (writeOptimizedSkyCoefTable(optSkyCoefDirs.value(groupToken),
                                       sourcePath,
                                       groupEntries,
                                       &out.warnings,
                                       &tsvError)) {
            ++out.optSkyCoefTablesWritten;
        } else {
            out.warnings.append(tsvError);
        }
    }

    const QString fieldNotesDir = destination.filePath("field_notes");
    if (!destination.mkpath("field_notes"))
        return failAndRemove(QStringLiteral("Could not create field_notes collection."));

    QString registrationError;
    if (!Project::registerCollection(
            project.rootPath(), fieldNotesDir, "other", {}, {}, {}, {},
            &registrationError)) {
        return failAndRemove(registrationError);
    }

    QString comment = propertyValue(source.filePath("project.conf"), "comments");
    if (comment.isEmpty())
        comment = propertyValue(source.filePath("project.conf"), "comment");
    const QString legacyDate = firstLegacyDateLine(source.filePath("project.conf"));
    QString noteText;
    QTextStream noteStream(&noteText);
    noteStream << "# Legacy HSP comment\n\n";
    if (legacyDate.isEmpty()) {
        noteStream << "- Original HSP date: not recorded\n";
    } else {
        noteStream << "- Original HSP date: " << legacyDate << "\n";
    }
    noteStream << "- Imported from: `"
               << QDir::fromNativeSeparators(sourcePath) << "`\n\n";
    if (comment.isEmpty()) {
        noteStream << "No project comment was recorded in `project.conf`.\n";
    } else {
        noteStream << comment << '\n';
    }
    QString noteError;
    if (!writeTextFile(QDir(fieldNotesDir).filePath("comment.md"),
                       noteText, &noteError)) {
        out.warnings.append(noteError);
    }

    collectOmitted(sourcePath, &out);
    QString reportError;
    if (!writeImportReport(project.rootPath(), sourcePath, out, &reportError))
        out.warnings.append(reportError);

    return project;
}
