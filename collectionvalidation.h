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
