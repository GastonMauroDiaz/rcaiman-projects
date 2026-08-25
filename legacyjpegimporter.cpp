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

#include "legacyjpegimporter.h"

#include "acquisitionmetadata.h"
#include "metadataregistry.h"
#include "rasterdata.h"
#include "tiffwriter.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>

namespace {

constexpr const char *LegacyJpegSidecarProfile =
    "legacy_jpeg_metadata_sidecar_tsv_v1";

QString portablePath(const QString &projectRoot, const QString &path)
{
    const QString relative = QDir(projectRoot).relativeFilePath(path);
    if (!relative.startsWith("../") && relative != "..")
        return QDir::fromNativeSeparators(relative);
    return QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
}

QString tsvField(const QString &value)
{
    if (!value.contains('\t') && !value.contains('\n')
        && !value.contains('\r') && !value.contains('"')) {
        return value;
    }
    QString escaped = value;
    escaped.replace('"', "\"\"");
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString numberField(bool present, double value)
{
    return present ? QString::number(value, 'g', 16) : QString();
}

QString normalizedPath(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath())
        .toCaseFolded();
}

QList<MetadataObservation> observationsFor(
    const QString &path,
    const QList<MetadataObservation> &observations)
{
    QList<MetadataObservation> result;
    const QString target = normalizedPath(path);
    for (const MetadataObservation &observation : observations) {
        if (normalizedPath(observation.sourceFile) == target)
            result.append(observation);
    }
    return result;
}

bool jpegToRgbRaster(const QString &jpegPath,
                     RasterData *raster,
                     QString *errorMessage)
{
    if (!raster)
        return false;

    QImageReader reader(jpegPath);
    reader.setAutoTransform(false);
    QImage image = reader.read();
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not read JPEG %1: %2")
                .arg(jpegPath, reader.errorString());
        }
        return false;
    }

    image = image.convertToFormat(QImage::Format_RGB888);
    raster->allocate(image.width(), image.height(), 3,
                     RasterData::DataType::UInt8);
    raster->setBandName(0, "Red");
    raster->setBandName(1, "Green");
    raster->setBandName(2, "Blue");

    const qsizetype pixels =
        static_cast<qsizetype>(image.width()) * image.height();
    char *base = raster->rawData().data();
    for (int row = 0; row < image.height(); ++row) {
        const uchar *src = image.constScanLine(row);
        for (int col = 0; col < image.width(); ++col) {
            const qsizetype pixelIndex =
                static_cast<qsizetype>(row) * image.width() + col;
            base[pixelIndex] = static_cast<char>(src[col * 3]);
            base[pixels + pixelIndex] =
                static_cast<char>(src[col * 3 + 1]);
            base[pixels * 2 + pixelIndex] =
                static_cast<char>(src[col * 3 + 2]);
        }
    }
    return true;
}

bool writeLegacySidecar(const QString &filePath,
                        const QString &projectRoot,
                        const QString &sourceJpegPath,
                        const QString &tiffPath,
                        const AcquisitionMetadataValues &values,
                        const QString &status,
                        const QString &error,
                        QString *errorMessage)
{
    const QStringList header = {
        "basename", "source_file", "tiff_file", "aperture",
        "exposure_time_seconds", "iso", "exposure_compensation_ev",
        "date_time_original", "gps_latitude", "gps_longitude",
        "gps_altitude", "metadata_status", "metadata_error"
    };
    const QStringList row = {
        QFileInfo(tiffPath).completeBaseName(),
        portablePath(projectRoot, sourceJpegPath),
        portablePath(projectRoot, tiffPath),
        numberField(values.hasAperture, values.aperture),
        numberField(values.hasExposureTime, values.exposureTimeSeconds),
        numberField(values.hasIso, values.iso),
        numberField(values.hasExposureCompensation,
                    values.exposureCompensationEv),
        values.dateTimeOriginal,
        numberField(values.hasGpsLatitude, values.gpsLatitude),
        numberField(values.hasGpsLongitude, values.gpsLongitude),
        numberField(values.hasGpsAltitude, values.gpsAltitude),
        status,
        error
    };

    QStringList encoded;
    encoded.reserve(row.size());
    for (const QString &field : row)
        encoded.append(tsvField(field));

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not write %1")
                .arg(filePath);
        return false;
    }
    file.write(header.join('\t').toUtf8());
    file.write("\n");
    file.write(encoded.join('\t').toUtf8());
    file.write("\n");
    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not commit %1")
                .arg(filePath);
        return false;
    }
    return true;
}

bool writeImportDocumentation(const LegacyJpegImportOptions &options,
                              const LegacyJpegImportReport &report,
                              QString *errorMessage)
{
    const QString filePath =
        QDir(options.destinationCollectionDir)
            .filePath("legacy_jpeg_import.md");
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not write %1")
                .arg(filePath);
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "# Legacy JPEG import\n\n";
    out << "This collection contains TIFF files generated from legacy JPEG "
           "images by rcaiman-projects.\n\n";
    out << "The conversion is provided for old projects where RAW files are "
           "not available. It does not recover RAW sensor data.\n\n";
    out << "## Source and destination\n\n";
    out << "- Source collection: `" << options.sourceCollectionName << "`\n";
    out << "- Destination collection: `"
        << options.destinationCollectionName << "`\n";
    out << "- Imported at: "
        << QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
        << "\n\n";
    out << "## Conversion contract\n\n";
    out << "- Source files are JPEG images.\n";
    out << "- Output files are RGB UInt8 TIFF images with the same basename.\n";
    out << "- No inverse gamma correction is applied.\n";
    out << "- No white-balance correction, radiometric calibration, or RAW "
           "recovery is applied.\n";
    out << "- Each TIFF receives a TSV sidecar generated from the source JPEG "
           "metadata when ExifTool can read it.\n\n";
    out << "## R usage\n\n";
    out << "TSV sidecars can be read from R with `read.delim()`.\n\n";
    out << "## Versions\n\n";
    out << "- Metadata sidecar profile: `" << LegacyJpegSidecarProfile
        << "`\n";
    out << "- ExifTool profile: `" << ExifToolService::Profile << "`\n";
    out << "- ExifTool version: "
        << (report.exifToolVersion.isEmpty()
                ? QStringLiteral("not available")
                : report.exifToolVersion)
        << "\n\n";
    out << "## Import summary\n\n";
    out << "- JPEG files found: " << report.sourceFiles << "\n";
    out << "- TIFF files written: " << report.convertedFiles << "\n";
    out << "- Failed conversions: " << report.failedFiles << "\n";
    out << "- TSV sidecars written: " << report.sidecarsWritten << "\n";
    out << "- Metadata errors: " << report.metadataErrors << "\n";

    if (!report.warnings.isEmpty()) {
        out << "\n## Warnings\n\n";
        for (const QString &warning : report.warnings)
            out << "- " << warning << "\n";
    }

    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not commit %1")
                .arg(filePath);
        return false;
    }
    return true;
}

}

QStringList LegacyJpegImporter::jpegFiles(const QString &sourceDirectory)
{
    QStringList files;
    const QFileInfoList entries = QDir(sourceDirectory).entryInfoList(
        {"*.jpg", "*.jpeg", "*.JPG", "*.JPEG"},
        QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo &entry : entries)
        files.append(entry.absoluteFilePath());
    return files;
}

QStringList LegacyJpegImporter::conflictingOutputs(
    const LegacyJpegImportOptions &options)
{
    QStringList conflicts;
    for (const QString &jpegPath : jpegFiles(options.sourceCollectionDir)) {
        const QString baseName = QFileInfo(jpegPath).completeBaseName();
        const QString tiffPath =
            QDir(options.destinationCollectionDir)
                .filePath(baseName + ".tif");
        const QString sidecarPath =
            QDir(options.destinationCollectionDir)
                .filePath(baseName + ".tsv");
        if (QFileInfo::exists(tiffPath))
            conflicts.append(tiffPath);
        if (QFileInfo::exists(sidecarPath))
            conflicts.append(sidecarPath);
    }
    return conflicts;
}

bool LegacyJpegImporter::importProject(
    const LegacyJpegImportOptions &options,
    LegacyJpegImportReport *report,
    QString *errorMessage)
{
    if (report)
        *report = {};
    LegacyJpegImportReport localReport;

    const QStringList files = jpegFiles(options.sourceCollectionDir);
    localReport.sourceFiles = files.size();
    if (files.isEmpty()) {
        if (errorMessage)
            *errorMessage = "The selected source collection contains no JPEG files.";
        return false;
    }

    if (!QDir().mkpath(options.destinationCollectionDir)) {
        if (errorMessage)
            *errorMessage = "Could not create the destination collection folder.";
        return false;
    }

    QList<MetadataObservation> observations;
    QString metadataError;
    bool metadataAvailable = false;
    if (ExifToolService::isAvailable(&metadataError)) {
        localReport.exifToolVersion =
            ExifToolService::version(&metadataError);
        metadataAvailable =
            ExifToolService::extract(files, &observations, &metadataError);
    }
    if (!metadataAvailable && !metadataError.isEmpty()) {
        localReport.warnings.append(metadataError);
    }

    for (const QString &jpegPath : files) {
        const QFileInfo jpegInfo(jpegPath);
        const QString baseName = jpegInfo.completeBaseName();
        const QString tiffPath =
            QDir(options.destinationCollectionDir)
                .filePath(baseName + ".tif");
        const QString sidecarPath =
            QDir(options.destinationCollectionDir)
                .filePath(baseName + ".tsv");

        if (!options.replaceExisting
            && (QFileInfo::exists(tiffPath)
                || QFileInfo::exists(sidecarPath))) {
            ++localReport.failedFiles;
            localReport.warnings.append(
                QStringLiteral("Skipped existing output for %1.")
                    .arg(jpegInfo.fileName()));
            continue;
        }

        RasterData raster;
        QString conversionError;
        if (!jpegToRgbRaster(jpegPath, &raster, &conversionError)
            || !TiffWriter::write(tiffPath, raster, &conversionError)) {
            ++localReport.failedFiles;
            localReport.warnings.append(conversionError);
            continue;
        }
        ++localReport.convertedFiles;

        QString status = "ok";
        QString sidecarErrorText;
        AcquisitionMetadataValues values;
        if (!metadataAvailable) {
            status = "error";
            sidecarErrorText = metadataError.isEmpty()
                ? QStringLiteral("ExifTool metadata extraction failed.")
                : metadataError;
            ++localReport.metadataErrors;
        } else {
            const QList<MetadataObservation> fileObservations =
                observationsFor(jpegPath, observations);
            if (fileObservations.isEmpty()) {
                status = "error";
                sidecarErrorText =
                    "ExifTool returned no metadata for this JPEG file.";
                ++localReport.metadataErrors;
            } else {
                values =
                    AcquisitionMetadata::valuesFromObservations(
                        fileObservations);
            }
        }

        QString writeError;
        if (!writeLegacySidecar(sidecarPath, options.projectRoot,
                                jpegPath, tiffPath, values, status,
                                sidecarErrorText, &writeError)) {
            localReport.warnings.append(writeError);
        } else {
            ++localReport.sidecarsWritten;
        }
    }

    QString docError;
    if (!writeImportDocumentation(options, localReport, &docError))
        localReport.warnings.append(docError);

    if (report)
        *report = localReport;
    return localReport.convertedFiles > 0;
}
