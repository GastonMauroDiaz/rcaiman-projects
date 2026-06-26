#include "acquisitionmetadata.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QSet>

#include <cmath>

namespace {

QString normalizedPath(const QString &path)
{
    QString result = QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
#ifdef Q_OS_WIN
    result = result.toCaseFolded();
#endif
    return result;
}

QString portablePath(const QString &root, const QString &path)
{
    return QDir::fromNativeSeparators(
        QDir(root).relativeFilePath(path));
}

bool numericValue(const QString &text, double *value)
{
    const QString trimmed = text.trimmed();
    const int slash = trimmed.indexOf('/');
    bool ok = false;
    double result = 0.0;
    if (slash > 0 && slash < trimmed.size() - 1) {
        bool numeratorOk = false;
        bool denominatorOk = false;
        const double numerator =
            trimmed.left(slash).toDouble(&numeratorOk);
        const double denominator =
            trimmed.mid(slash + 1).toDouble(&denominatorOk);
        ok = numeratorOk && denominatorOk && denominator != 0.0;
        if (ok)
            result = numerator / denominator;
    } else {
        result = trimmed.toDouble(&ok);
    }
    if (!ok || !std::isfinite(result))
        return false;
    if (value)
        *value = result;
    return true;
}

bool firstPositiveValue(
    const QList<MetadataObservation> &observations,
    const QList<QPair<QString, QString>> &precedence,
    double *value)
{
    for (const auto &key : precedence) {
        for (const MetadataObservation &observation : observations) {
            if (observation.group.compare(
                    key.first, Qt::CaseInsensitive) != 0
                || observation.tag.compare(
                    key.second, Qt::CaseInsensitive) != 0) {
                continue;
            }
            double candidate = 0.0;
            if (numericValue(observation.value, &candidate)
                && candidate > 0.0) {
                if (value)
                    *value = candidate;
                return true;
            }
        }
    }
    return false;
}

bool firstFiniteValue(
    const QList<MetadataObservation> &observations,
    const QList<QPair<QString, QString>> &precedence,
    double *value)
{
    for (const auto &key : precedence) {
        for (const MetadataObservation &observation : observations) {
            if (observation.group.compare(
                    key.first, Qt::CaseInsensitive) != 0
                || observation.tag.compare(
                    key.second, Qt::CaseInsensitive) != 0) {
                continue;
            }
            double candidate = 0.0;
            if (numericValue(observation.value, &candidate)) {
                if (value)
                    *value = candidate;
                return true;
            }
        }
    }
    return false;
}

bool firstPositiveMakerNoteIso(
    const QList<MetadataObservation> &observations,
    double *value)
{
    const QStringList isoTags = {
        "ISO", "ISOSetting", "ISOSelection", "BaseISO"
    };
    for (const MetadataObservation &observation : observations) {
        if (!isoTags.contains(observation.tag, Qt::CaseInsensitive)
            || observation.group.compare(
                   "ExifIFD", Qt::CaseInsensitive) == 0
            || observation.group.compare(
                   "Composite", Qt::CaseInsensitive) == 0) {
            continue;
        }
        double candidate = 0.0;
        if (numericValue(observation.value, &candidate)
            && candidate > 0.0) {
            if (value)
                *value = candidate;
            return true;
        }
    }
    return false;
}

bool hasExifToolError(
    const QList<MetadataObservation> &observations,
    QString *error)
{
    for (const MetadataObservation &observation : observations) {
        if (observation.tag.compare(
                "Error", Qt::CaseInsensitive) == 0) {
            if (error)
                *error = observation.value;
            return true;
        }
    }
    return false;
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

}

AcquisitionMetadataValues AcquisitionMetadata::valuesFromObservations(
    const QList<MetadataObservation> &observations)
{
    AcquisitionMetadataValues values;
    values.hasAperture = firstPositiveValue(
        observations,
        {
            {"ExifIFD", "FNumber"},
            {"Composite", "Aperture"}
        },
        &values.aperture);
    values.hasExposureTime = firstPositiveValue(
        observations,
        {
            {"ExifIFD", "ExposureTime"},
            {"Composite", "ShutterSpeed"}
        },
        &values.exposureTimeSeconds);
    values.hasIso = firstPositiveValue(
        observations,
        {
            {"ExifIFD", "PhotographicSensitivity"},
            {"ExifIFD", "ISO"},
            {"ExifIFD", "RecommendedExposureIndex"},
            {"ExifIFD", "StandardOutputSensitivity"},
            {"Composite", "ISO"}
        },
        &values.iso);
    if (!values.hasIso) {
        values.hasIso =
            firstPositiveMakerNoteIso(observations, &values.iso);
    }
    values.hasExposureCompensation = firstFiniteValue(
        observations,
        {
            {"ExifIFD", "ExposureCompensation"}
        },
        &values.exposureCompensationEv);
    return values;
}

bool AcquisitionMetadata::writeSidecar(
    const QString &filePath,
    const QString &projectRoot,
    const QString &rawPath,
    const QString &sensorPath,
    const AcquisitionMetadataValues &values,
    const QString &status,
    const QString &error,
    QString *errorMessage)
{
    const QStringList header = {
        "basename", "raw_file", "sensor_file", "aperture",
        "exposure_time_seconds", "iso", "exposure_compensation_ev",
        "metadata_status", "metadata_error"
    };
    const QStringList row = {
        QFileInfo(sensorPath).completeBaseName(),
        rawPath.isEmpty() ? QString() : portablePath(projectRoot, rawPath),
        portablePath(projectRoot, sensorPath),
        numberField(values.hasAperture, values.aperture),
        numberField(values.hasExposureTime, values.exposureTimeSeconds),
        numberField(values.hasIso, values.iso),
        numberField(values.hasExposureCompensation,
                    values.exposureCompensationEv),
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
            *errorMessage = QString("Could not write %1").arg(filePath);
        return false;
    }
    file.write(header.join('\t').toUtf8());
    file.write("\n");
    file.write(encoded.join('\t').toUtf8());
    file.write("\n");
    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = QString("Could not commit %1").arg(filePath);
        return false;
    }
    return true;
}

bool AcquisitionMetadata::regenerate(
    const QString &projectRoot,
    AcquisitionMetadataSummary *summary,
    QString *errorMessage)
{
    AcquisitionMetadataSummary localSummary;
    const QString sensorDirectory =
        QDir(projectRoot).filePath("sensor_data");
    const QString rawDirectory =
        QDir(projectRoot).filePath("raw_files");

    const QFileInfoList sensorFiles = QDir(sensorDirectory).entryInfoList(
        {"*.tif", "*.tiff", "*.TIF", "*.TIFF"},
        QDir::Files | QDir::NoSymLinks, QDir::Name);

    QMap<QString, QStringList> rawByBaseName;
    const QStringList rawFilters = {
        "*.CR2", "*.CR3", "*.NEF", "*.ARW", "*.DNG", "*.RAF",
        "*.ORF", "*.RW2", "*.PEF", "*.SRW", "*.RAW", "*.DCR", "*.KDC",
        "*.cr2", "*.cr3", "*.nef", "*.arw", "*.dng", "*.raf",
        "*.orf", "*.rw2", "*.pef", "*.srw", "*.raw", "*.dcr", "*.kdc"
    };
    QDirIterator rawIterator(
        rawDirectory, rawFilters,
        QDir::Files | QDir::NoSymLinks,
        QDirIterator::Subdirectories);
    while (rawIterator.hasNext()) {
        const QString path = rawIterator.next();
        rawByBaseName[
            QFileInfo(path).completeBaseName().toCaseFolded()].append(path);
    }

    QStringList extractableRawPaths;
    for (const QFileInfo &sensor : sensorFiles) {
        const QStringList matches =
            rawByBaseName.value(
                sensor.completeBaseName().toCaseFolded());
        if (matches.size() == 1)
            extractableRawPaths.append(matches.first());
    }
    extractableRawPaths.removeDuplicates();

    QList<MetadataObservation> allObservations;
    QString extractionError;
    bool extractionSucceeded = true;
    if (!extractableRawPaths.isEmpty()) {
        extractionSucceeded = ExifToolService::extract(
            extractableRawPaths, &allObservations, &extractionError);
    }

    QSet<QString> expectedSidecars;
    for (const QFileInfo &sensor : sensorFiles) {
        const QString sidecarPath =
            sensor.absolutePath() + "/" + sensor.completeBaseName() + ".tsv";
        expectedSidecars.insert(normalizedPath(sidecarPath));

        const QStringList rawMatches =
            rawByBaseName.value(
                sensor.completeBaseName().toCaseFolded());
        QString rawPath;
        QString status = "ok";
        QString metadataError;
        AcquisitionMetadataValues values;

        if (rawMatches.isEmpty()) {
            status = "error";
            metadataError = QString(
                "No RAW with basename '%1' was found in raw_files.")
                                .arg(sensor.completeBaseName());
        } else if (rawMatches.size() > 1) {
            status = "error";
            QStringList portableMatches;
            for (const QString &match : rawMatches)
                portableMatches.append(portablePath(projectRoot, match));
            metadataError = QString(
                "Multiple RAW files with basename '%1' were found: %2")
                                .arg(sensor.completeBaseName(),
                                     portableMatches.join(", "));
        } else {
            rawPath = rawMatches.first();
            if (!extractionSucceeded) {
                status = "error";
                metadataError = extractionError;
            } else {
                const QList<MetadataObservation> observations =
                    observationsFor(rawPath, allObservations);
                if (observations.isEmpty()) {
                    status = "error";
                    metadataError =
                        "ExifTool returned no metadata for this RAW file.";
                } else if (hasExifToolError(
                               observations, &metadataError)) {
                    status = "error";
                } else {
                    values = valuesFromObservations(observations);
                }
            }
        }

        QString writeError;
        if (writeSidecar(
                sidecarPath, projectRoot, rawPath,
                sensor.absoluteFilePath(), values,
                status, metadataError, &writeError)) {
            ++localSummary.sidecarsWritten;
            if (status == "error")
                ++localSummary.metadataErrors;
        } else {
            ++localSummary.writeErrors;
            if (errorMessage && errorMessage->isEmpty())
                *errorMessage = writeError;
        }
    }

    const QFileInfoList sidecars = QDir(sensorDirectory).entryInfoList(
        {"*.tsv", "*.TSV"}, QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &sidecar : sidecars) {
        if (!expectedSidecars.contains(
                normalizedPath(sidecar.absoluteFilePath()))
            && QFile::remove(sidecar.absoluteFilePath())) {
            ++localSummary.orphanSidecarsRemoved;
        }
    }

    if (summary)
        *summary = localSummary;
    return localSummary.writeErrors == 0;
}
