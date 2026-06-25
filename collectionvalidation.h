#ifndef COLLECTIONVALIDATION_H
#define COLLECTIONVALIDATION_H

#include "metadataregistry.h"
#include "project.h"

#include <QList>
#include <QString>

enum class CollectionFileValidationStatus {
    ExactMatch,
    AcceptedReview,
    RejectedReview,
    Mismatch,
    MissingRaw,
    AmbiguousRaw,
    ReadError
};

struct CollectionFileValidation {
    QString preprocessedPath;
    QString preprocessedHash;
    QString baseName;
    QString rawPath;
    QString rawHash;
    QStringList rawCandidates;
    CollectionFileValidationStatus status =
        CollectionFileValidationStatus::ReadError;
    int matchedRuleCount = 0;
    int totalRuleCount = 0;
    QList<MetadataRuleDecision> problems;
    QString reviewPath;
};

struct CollectionValidationRun {
    Collection collection;
    QString registryPath;
    QString registryHash;
    QString exifToolVersion;
    QString validatedAt;
    QString result;
    QList<CollectionFileValidation> files;
};

struct CollectionValidationReportStatus {
    bool exists = false;
    bool stale = false;
    QString result;
    QString validatedAt;
    int exactCount = 0;
    int reviewedCount = 0;
    int rejectedCount = 0;
    int problemCount = 0;
};

namespace CollectionValidation {

QString statusName(CollectionFileValidationStatus status);
QStringList rawFileFilters();
QStringList preprocessedImageFilters();

bool isEligible(const QString &projectRoot, const Collection &collection);

bool resolveRegistry(const QString &projectRoot,
                     const Collection &collection,
                     QString *registryPath,
                     QString *errorMessage = nullptr);

QList<CollectionFileValidation> resolveRawPairs(
    const QString &projectRoot,
    const Collection &collection);

bool validate(const QString &projectRoot,
              const Collection &collection,
              CollectionValidationRun *run,
              QString *errorMessage = nullptr);

bool writeManifest(const QString &filePath,
                   const QString &projectRoot,
                   const CollectionValidationRun &run,
                   QString *errorMessage = nullptr);

CollectionValidationReportStatus latestReportStatus(
    const QString &projectRoot,
    const Collection &collection);

}

#endif
