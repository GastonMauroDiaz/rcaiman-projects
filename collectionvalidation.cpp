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

#include "collectionvalidation.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QSet>

#include <yaml-cpp/yaml.h>

#include <algorithm>

namespace {

QString normalizedPath(const QString &path)
{
    if (path.isEmpty())
        return {};
    QString value = QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
#ifdef Q_OS_WIN
    value = value.toCaseFolded();
#endif
    return value;
}

QString portablePath(const QString &root, const QString &path)
{
    return QDir::fromNativeSeparators(QDir(root).relativeFilePath(path));
}

std::string utf8(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

QString scalar(const YAML::Node &node, const char *field)
{
    const YAML::Node value = node[field];
    return value && value.IsScalar()
        ? QString::fromStdString(value.as<std::string>())
        : QString();
}

QString absoluteProjectPath(const QString &root, const QString &path)
{
    return QFileInfo(QDir(root).filePath(path)).absoluteFilePath();
}

QList<MetadataObservation> observationsFor(
    const QString &path,
    const QList<MetadataObservation> &all)
{
    QList<MetadataObservation> result;
    const QString target = normalizedPath(path);
    for (const MetadataObservation &observation : all) {
        if (normalizedPath(observation.sourceFile) == target)
            result.append(observation);
    }
    return result;
}

bool hasExifToolError(const QList<MetadataObservation> &observations)
{
    for (const MetadataObservation &observation : observations) {
        if (observation.tag.compare(
                "Error", Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

MetadataRuleDecision problemDecision(
    const MetadataRule &rule,
    const QList<MetadataObservation> &observations)
{
    MetadataRuleDecision decision;
    decision.rule = rule;
    QList<MetadataObservation> choices;
    for (const MetadataObservation &observation : observations) {
        if (observation.key() == rule.key())
            choices.append(observation);
    }
    if (choices.isEmpty())
        choices = MetadataRegistry::candidates(rule, observations, 1);
    if (!choices.isEmpty()) {
        decision.observedGroup = choices.first().group;
        decision.observedTag = choices.first().tag;
        decision.observedValue = choices.first().value;
    }
    return decision;
}

struct ReviewMatch {
    QString path;
    QString decision;
};

ReviewMatch applicableReview(const QString &projectRoot,
                             const QString &rawPath,
                             const QString &rawHash,
                             const QString &registryPath,
                             const QString &registryHash)
{
    const QDir reviews(QDir(projectRoot).filePath("registry_reviews"));
    const QFileInfoList files = reviews.entryInfoList(
        {"*.yaml", "*.yml"}, QDir::Files, QDir::Time);
    const QString expectedRaw = normalizedPath(rawPath);
    const QString expectedRegistry = normalizedPath(registryPath);
    for (const QFileInfo &file : files) {
        try {
            const YAML::Node document =
                YAML::LoadFile(utf8(file.absoluteFilePath()));
            if (scalar(document, "artifact") != "registry_review") {
                continue;
            }
            const QString decision = scalar(document, "decision");
            if (decision != "accepted" && decision != "rejected")
                continue;
            const YAML::Node image = document["image"];
            const YAML::Node registry = document["registry"];
            if (scalar(image, "sha256") != rawHash
                || scalar(registry, "sha256") != registryHash) {
                continue;
            }
            if (normalizedPath(absoluteProjectPath(
                    projectRoot, scalar(image, "path"))) != expectedRaw
                || normalizedPath(absoluteProjectPath(
                    projectRoot, scalar(registry, "path")))
                    != expectedRegistry) {
                continue;
            }
            return {file.absoluteFilePath(), decision};
        } catch (const std::exception &) {
        }
    }
    return {};
}

void emitDecision(YAML::Emitter &out,
                  const MetadataRuleDecision &decision)
{
    out << YAML::BeginMap;
    out << YAML::Key << "rule" << YAML::Value << utf8(decision.rule.id);
    out << YAML::Key << "expected_key" << YAML::Value
        << utf8(decision.rule.key());
    out << YAML::Key << "expected_value" << YAML::Value
        << utf8(decision.rule.expected);
    const QString observedKey =
        decision.observedGroup.isEmpty()
            ? QString()
            : decision.observedGroup + ":" + decision.observedTag;
    out << YAML::Key << "observed_key" << YAML::Value << utf8(observedKey);
    out << YAML::Key << "observed_value" << YAML::Value
        << utf8(decision.observedValue);
    out << YAML::EndMap;
}

QString latestManifest(const QString &projectRoot,
                       const Collection &collection)
{
    const QString prefix =
        MetadataRegistry::snakeCase(collection.dirName) + "_";
    const QDir directory(
        QDir(projectRoot).filePath("registry_validations"));
    const QFileInfoList candidates = directory.entryInfoList(
        {prefix + "*.yaml", prefix + "*.yml"},
        QDir::Files, QDir::Name);
    const QString expectedCollection = normalizedPath(collection.dirPath);
    QString selectedPath;
    QString selectedDate;
    for (const QFileInfo &candidate : candidates) {
        try {
            const YAML::Node document =
                YAML::LoadFile(utf8(candidate.absoluteFilePath()));
            const QString savedCollection = absoluteProjectPath(
                projectRoot, scalar(document["collection"], "path"));
            const QString date = scalar(document, "validated_at");
            if (normalizedPath(savedCollection) == expectedCollection
                && (selectedPath.isEmpty() || date > selectedDate)) {
                selectedPath = candidate.absoluteFilePath();
                selectedDate = date;
            }
        } catch (const std::exception &) {
        }
    }
    return selectedPath;
}

}

QString CollectionValidation::statusName(
    CollectionFileValidationStatus status)
{
    switch (status) {
    case CollectionFileValidationStatus::ExactMatch:
        return "exact_match";
    case CollectionFileValidationStatus::AcceptedReview:
        return "accepted_review";
    case CollectionFileValidationStatus::RejectedReview:
        return "rejected_review";
    case CollectionFileValidationStatus::Mismatch:
        return "mismatch";
    case CollectionFileValidationStatus::MissingRaw:
        return "missing_raw";
    case CollectionFileValidationStatus::AmbiguousRaw:
        return "ambiguous_raw";
    case CollectionFileValidationStatus::ReadError:
        return "read_error";
    }
    return "read_error";
}

QStringList CollectionValidation::rawFileFilters()
{
    return {
        "*.CR2", "*.CR3", "*.NEF", "*.ARW", "*.DNG", "*.RAF",
        "*.ORF", "*.RW2", "*.PEF", "*.SRW", "*.RAW", "*.DCR", "*.KDC",
        "*.cr2", "*.cr3", "*.nef", "*.arw", "*.dng", "*.raf",
        "*.orf", "*.rw2", "*.pef", "*.srw", "*.raw", "*.dcr", "*.kdc"
    };
}

QStringList CollectionValidation::preprocessedImageFilters()
{
    return {
        "*.tif", "*.tiff", "*.TIF", "*.TIFF",
        "*.png", "*.PNG", "*.jpg", "*.jpeg", "*.JPG", "*.JPEG",
        "*.bmp", "*.BMP"
    };
}

bool CollectionValidation::isEligible(
    const QString &projectRoot,
    const Collection &collection)
{
    QString registryPath;
    return resolveRegistry(
        projectRoot, collection, &registryPath, nullptr);
}

bool CollectionValidation::resolveRegistry(
    const QString &projectRoot,
    const Collection &collection,
    QString *registryPath,
    QString *errorMessage)
{
    if (registryPath)
        registryPath->clear();
    if (collection.collectionClass
        != "preprocessed_image_collection") {
        if (errorMessage)
            *errorMessage = "The folder is not a preprocessed image collection.";
        return false;
    }

    auto compatibleRegistry = [errorMessage](const QString &path) {
        QList<MetadataRule> rules;
        QString readError;
        if (!QFileInfo(path).isFile()
            || !MetadataRegistry::readRules(
                path, &rules, &readError)) {
            if (errorMessage)
                *errorMessage = readError.isEmpty()
                    ? QString("Registry file does not exist: %1").arg(path)
                    : QString("Registry is incompatible: %1").arg(readError);
            return false;
        }
        return true;
    };

    if (!collection.registry.isEmpty()) {
        const QString explicitPath =
            Project::resolveReference(projectRoot, collection.registry);
        if (!compatibleRegistry(explicitPath))
            return false;
        if (registryPath)
            *registryPath = explicitPath;
        return true;
    }

    if (errorMessage) {
        *errorMessage =
            "No registry is declared in collection.yaml. "
            "The collection is valid, but it is not eligible for registry validation.";
    }
    return false;
}

QList<CollectionFileValidation> CollectionValidation::resolveRawPairs(
    const QString &projectRoot,
    const Collection &collection)
{
    QMap<QString, QStringList> rawByBaseName;
    QDirIterator rawIterator(
        QDir(projectRoot).filePath("raw_files"),
        rawFileFilters(), QDir::Files | QDir::NoSymLinks,
        QDirIterator::Subdirectories);
    while (rawIterator.hasNext()) {
        const QString path = rawIterator.next();
        rawByBaseName[
            QFileInfo(path).completeBaseName().toCaseFolded()].append(path);
    }

    QList<CollectionFileValidation> result;
    const QFileInfoList images = QDir(collection.dirPath).entryInfoList(
        preprocessedImageFilters(), QDir::Files | QDir::NoSymLinks,
        QDir::Name);
    for (const QFileInfo &image : images) {
        CollectionFileValidation file;
        file.preprocessedPath = image.absoluteFilePath();
        file.baseName = image.completeBaseName();
        file.rawCandidates =
            rawByBaseName.value(file.baseName.toCaseFolded());
        if (file.rawCandidates.isEmpty()) {
            file.status = CollectionFileValidationStatus::MissingRaw;
        } else if (file.rawCandidates.size() > 1) {
            file.status = CollectionFileValidationStatus::AmbiguousRaw;
        } else {
            file.rawPath = file.rawCandidates.first();
        }
        result.append(file);
    }
    return result;
}

bool CollectionValidation::validate(
    const QString &projectRoot,
    const Collection &collection,
    CollectionValidationRun *run,
    QString *errorMessage)
{
    QString resolvedRegistry;
    if (!run || !resolveRegistry(
            projectRoot, collection, &resolvedRegistry, errorMessage)) {
        if (errorMessage)
            if (errorMessage->isEmpty())
                *errorMessage =
                    "The selected collection has no valid registry.";
        return false;
    }

    run->collection = collection;
    run->registryPath = resolvedRegistry;
    run->registryHash =
        MetadataRegistry::sha256(run->registryPath, errorMessage);
    run->exifToolVersion = ExifToolService::version(errorMessage);
    run->validatedAt =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    run->files = resolveRawPairs(projectRoot, collection);
    if (run->registryHash.isEmpty() || run->exifToolVersion.isEmpty())
        return false;

    QList<MetadataRule> rules;
    if (!MetadataRegistry::readRules(
            run->registryPath, &rules, errorMessage)) {
        return false;
    }

    QStringList rawPaths;
    for (CollectionFileValidation &file : run->files) {
        file.totalRuleCount = rules.size();
        QString hashError;
        file.preprocessedHash =
            MetadataRegistry::sha256(file.preprocessedPath, &hashError);
        if (file.preprocessedHash.isEmpty()) {
            file.status = CollectionFileValidationStatus::ReadError;
            continue;
        }
        if (!file.rawPath.isEmpty()) {
            file.rawHash =
                MetadataRegistry::sha256(file.rawPath, &hashError);
            if (file.rawHash.isEmpty()) {
                file.status = CollectionFileValidationStatus::ReadError;
                continue;
            }
            rawPaths.append(file.rawPath);
        }
    }

    QList<MetadataObservation> observations;
    QString extractionError;
    if (!rawPaths.isEmpty()
        && !ExifToolService::extract(
            rawPaths, &observations, &extractionError)) {
        observations.clear();
    }

    bool hasReview = false;
    bool hasFailure = run->files.isEmpty();
    for (CollectionFileValidation &file : run->files) {
        if (file.status == CollectionFileValidationStatus::MissingRaw
            || file.status == CollectionFileValidationStatus::AmbiguousRaw
            || (file.status == CollectionFileValidationStatus::ReadError
                && (file.preprocessedHash.isEmpty()
                    || file.rawHash.isEmpty()))) {
            hasFailure = true;
            continue;
        }
        const QList<MetadataObservation> fileObservations =
            observationsFor(file.rawPath, observations);
        if (fileObservations.isEmpty()
            || hasExifToolError(fileObservations)) {
            file.status = CollectionFileValidationStatus::ReadError;
            hasFailure = true;
            continue;
        }

        for (const MetadataRule &rule : rules) {
            bool matched = false;
            for (const MetadataObservation &observation : fileObservations) {
                if (observation.key() == rule.key()
                    && observation.value == rule.expected) {
                    matched = true;
                    break;
                }
            }
            if (matched) {
                ++file.matchedRuleCount;
            } else {
                file.problems.append(
                    problemDecision(rule, fileObservations));
            }
        }

        if (file.problems.isEmpty()) {
            file.status = CollectionFileValidationStatus::ExactMatch;
            continue;
        }
        const ReviewMatch review = applicableReview(
            projectRoot, file.rawPath, file.rawHash,
            run->registryPath, run->registryHash);
        file.reviewPath = review.path;
        if (review.decision == "accepted") {
            file.status = CollectionFileValidationStatus::AcceptedReview;
            hasReview = true;
        } else if (review.decision == "rejected") {
            file.status = CollectionFileValidationStatus::RejectedReview;
            hasFailure = true;
        } else {
            file.status = CollectionFileValidationStatus::Mismatch;
            hasFailure = true;
        }
    }

    run->result = hasFailure
        ? "failed"
        : (hasReview ? "passed_with_reviews" : "passed_exact");
    return true;
}

bool CollectionValidation::writeManifest(
    const QString &filePath,
    const QString &projectRoot,
    const CollectionValidationRun &run,
    QString *errorMessage)
{
    try {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "schema_version" << YAML::Value << 1;
        out << YAML::Key << "artifact" << YAML::Value
            << "registry_collection_validation";
        out << YAML::Key << "validated_at" << YAML::Value
            << utf8(run.validatedAt);
        out << YAML::Key << "collection" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "path" << YAML::Value
            << utf8(portablePath(projectRoot, run.collection.dirPath));
        out << YAML::Key << "class" << YAML::Value
            << utf8(run.collection.collectionClass);
        out << YAML::EndMap;
        out << YAML::Key << "registry" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "path" << YAML::Value
            << utf8(portablePath(projectRoot, run.registryPath));
        out << YAML::Key << "sha256" << YAML::Value
            << utf8(run.registryHash);
        out << YAML::EndMap;
        out << YAML::Key << "metadata_reader" << YAML::Value
            << YAML::BeginMap;
        out << YAML::Key << "name" << YAML::Value << "exiftool";
        out << YAML::Key << "version" << YAML::Value
            << utf8(run.exifToolVersion);
        out << YAML::Key << "profile" << YAML::Value
            << ExifToolService::Profile;
        out << YAML::EndMap;
        out << YAML::Key << "files" << YAML::Value << YAML::BeginSeq;
        for (const CollectionFileValidation &file : run.files) {
            out << YAML::BeginMap;
            out << YAML::Key << "preprocessed" << YAML::Value
                << YAML::BeginMap;
            out << YAML::Key << "path" << YAML::Value
                << utf8(portablePath(projectRoot, file.preprocessedPath));
            out << YAML::Key << "sha256" << YAML::Value
                << utf8(file.preprocessedHash);
            out << YAML::Key << "basename" << YAML::Value
                << utf8(file.baseName);
            out << YAML::EndMap;
            if (!file.rawPath.isEmpty()) {
                out << YAML::Key << "raw" << YAML::Value << YAML::BeginMap;
                out << YAML::Key << "path" << YAML::Value
                    << utf8(portablePath(projectRoot, file.rawPath));
                out << YAML::Key << "sha256" << YAML::Value
                    << utf8(file.rawHash);
                out << YAML::EndMap;
            }
            if (!file.rawCandidates.isEmpty()) {
                out << YAML::Key << "raw_candidates" << YAML::Value
                    << YAML::BeginSeq;
                for (const QString &candidate : file.rawCandidates)
                    out << utf8(portablePath(projectRoot, candidate));
                out << YAML::EndSeq;
            }
            out << YAML::Key << "status" << YAML::Value
                << utf8(statusName(file.status));
            out << YAML::Key << "matched_rules" << YAML::Value
                << file.matchedRuleCount;
            out << YAML::Key << "total_rules" << YAML::Value
                << file.totalRuleCount;
            if (!file.reviewPath.isEmpty()) {
                out << YAML::Key << "review" << YAML::Value
                    << YAML::BeginMap;
                out << YAML::Key << "path" << YAML::Value
                    << utf8(portablePath(projectRoot, file.reviewPath));
                out << YAML::Key << "sha256" << YAML::Value
                    << utf8(MetadataRegistry::sha256(file.reviewPath));
                out << YAML::EndMap;
            }
            if (!file.problems.isEmpty()) {
                out << YAML::Key << "problems" << YAML::Value
                    << YAML::BeginSeq;
                for (const MetadataRuleDecision &problem : file.problems)
                    emitDecision(out, problem);
                out << YAML::EndSeq;
            }
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
        int exact = 0;
        int reviewed = 0;
        int rejected = 0;
        int problems = 0;
        for (const CollectionFileValidation &file : run.files) {
            if (file.status == CollectionFileValidationStatus::ExactMatch)
                ++exact;
            else if (file.status
                     == CollectionFileValidationStatus::AcceptedReview)
                ++reviewed;
            else {
                if (file.status
                    == CollectionFileValidationStatus::RejectedReview)
                    ++rejected;
                ++problems;
            }
        }
        out << YAML::Key << "summary" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "total" << YAML::Value << run.files.size();
        out << YAML::Key << "exact" << YAML::Value << exact;
        out << YAML::Key << "accepted_reviews" << YAML::Value << reviewed;
        out << YAML::Key << "rejected_reviews" << YAML::Value << rejected;
        out << YAML::Key << "problems" << YAML::Value << problems;
        out << YAML::EndMap;
        out << YAML::Key << "result" << YAML::Value << utf8(run.result);
        out << YAML::EndMap;

        QDir().mkpath(QFileInfo(filePath).absolutePath());
        QSaveFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            throw std::runtime_error("Could not open validation manifest.");
        file.write(out.c_str());
        file.write("\n");
        if (!file.commit())
            throw std::runtime_error("Could not commit validation manifest.");
        return true;
    } catch (const std::exception &error) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(error.what());
        return false;
    }
}

CollectionValidationReportStatus CollectionValidation::latestReportStatus(
    const QString &projectRoot,
    const Collection &collection)
{
    CollectionValidationReportStatus status;
    const QString manifestPath = latestManifest(projectRoot, collection);
    if (manifestPath.isEmpty())
        return status;

    status.exists = true;
    try {
        const YAML::Node document = YAML::LoadFile(utf8(manifestPath));
        if (scalar(document, "artifact")
            != "registry_collection_validation") {
            status.stale = true;
            return status;
        }
        status.result = scalar(document, "result");
        status.validatedAt = scalar(document, "validated_at");
        const YAML::Node summary = document["summary"];
        status.exactCount = summary["exact"].as<int>(0);
        status.reviewedCount = summary["accepted_reviews"].as<int>(0);
        status.rejectedCount = summary["rejected_reviews"].as<int>(0);
        status.problemCount = summary["problems"].as<int>(0);

        const QString registryPath = absoluteProjectPath(
            projectRoot, scalar(document["registry"], "path"));
        if (MetadataRegistry::sha256(registryPath)
            != scalar(document["registry"], "sha256")) {
            status.stale = true;
            return status;
        }

        const QList<CollectionFileValidation> current =
            resolveRawPairs(projectRoot, collection);
        const YAML::Node savedFiles = document["files"];
        if (!savedFiles || !savedFiles.IsSequence()
            || static_cast<int>(savedFiles.size()) != current.size()) {
            status.stale = true;
            return status;
        }

        QMap<QString, CollectionFileValidation> currentByImage;
        for (const CollectionFileValidation &file : current)
            currentByImage.insert(normalizedPath(file.preprocessedPath), file);
        for (const YAML::Node &saved : savedFiles) {
            const YAML::Node savedImage = saved["preprocessed"];
            const QString imagePath = absoluteProjectPath(
                projectRoot, scalar(savedImage, "path"));
            const auto currentIt =
                currentByImage.constFind(normalizedPath(imagePath));
            if (currentIt == currentByImage.constEnd()
                || MetadataRegistry::sha256(imagePath)
                    != scalar(savedImage, "sha256")) {
                status.stale = true;
                return status;
            }
            const QString savedRawPath = saved["raw"]
                ? absoluteProjectPath(
                      projectRoot, scalar(saved["raw"], "path"))
                : QString();
            if (normalizedPath(savedRawPath)
                    != normalizedPath(currentIt->rawPath)
                || (!savedRawPath.isEmpty()
                    && MetadataRegistry::sha256(savedRawPath)
                        != scalar(saved["raw"], "sha256"))) {
                status.stale = true;
                return status;
            }
            QStringList savedCandidates;
            const YAML::Node candidates = saved["raw_candidates"];
            if (candidates && candidates.IsSequence()) {
                for (const YAML::Node &candidate : candidates) {
                    savedCandidates.append(normalizedPath(
                        absoluteProjectPath(
                            projectRoot,
                            QString::fromStdString(
                                candidate.as<std::string>()))));
                }
            }
            QStringList currentCandidates;
            for (const QString &candidate : currentIt->rawCandidates)
                currentCandidates.append(normalizedPath(candidate));
            savedCandidates.sort();
            currentCandidates.sort();
            if (savedCandidates != currentCandidates) {
                status.stale = true;
                return status;
            }
            const YAML::Node review = saved["review"];
            if (review) {
                const QString reviewPath = absoluteProjectPath(
                    projectRoot, scalar(review, "path"));
                if (MetadataRegistry::sha256(reviewPath)
                    != scalar(review, "sha256")) {
                    status.stale = true;
                    return status;
                }
            }
        }
    } catch (const std::exception &) {
        status.stale = true;
    }
    return status;
}
