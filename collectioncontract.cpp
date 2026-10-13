// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// Part of the rcaiman suite.

#include "collectioncontract.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <algorithm>
#include <functional>

namespace {
QString resolveReference(const QString &projectRoot, const QString &reference)
{
    if (reference.trimmed().isEmpty())
        return {};

    const QFileInfo info(reference);
    const QString path = info.isAbsolute()
        ? reference
        : QDir(projectRoot).filePath(reference);
    return CollectionContract::collectionId(path);
}

bool classIsKnown(const QString &collectionClass)
{
    return QStringList{
        "preprocessed_image_collection",
        "processed_image_collection",
        "point_collection",
        "segmentation_collection",
        "other"
    }.contains(collectionClass, Qt::CaseInsensitive);
}

bool requiresImageFrame(const QString &collectionClass)
{
    return collectionClass == "processed_image_collection"
        || collectionClass == "point_collection"
        || collectionClass == "segmentation_collection";
}

bool aliasIsSupported(const QString &alias, QString *errorMessage)
{
    const QString trimmed = alias.trimmed();
    if (trimmed.size() > 40) {
        if (errorMessage)
            *errorMessage = "Collection alias cannot be longer than 40 characters.";
        return false;
    }
    for (const QChar character : trimmed) {
        if (character.category() == QChar::Other_Control) {
            if (errorMessage) {
                *errorMessage = "Collection alias cannot contain line breaks "
                                "or control characters.";
            }
            return false;
        }
    }
    return true;
}

void addIssue(CollectionContractAnalysis *analysis,
              CollectionContractSeverity severity,
              const Collection &collection,
              const QString &message)
{
    if (!analysis)
        return;
    analysis->issues.append({
        severity,
        CollectionContract::collectionId(collection.dirPath),
        CollectionContract::collectionLabel(collection),
        message
    });
}

void checkFileReference(const QString &projectRoot,
                        const Collection &collection,
                        const QString &fieldName,
                        const QString &reference,
                        CollectionContractAnalysis *analysis)
{
    if (reference.trimmed().isEmpty())
        return;

    const QString resolved = QFileInfo(reference).isAbsolute()
        ? reference
        : QDir(projectRoot).filePath(reference);
    const QFileInfo info(resolved);
    if (!info.exists()) {
        addIssue(
            analysis,
            CollectionContractSeverity::Warning,
            collection,
            QStringLiteral("%1 points to missing file \"%2\".")
                .arg(fieldName, QDir::toNativeSeparators(resolved)));
    } else if (!info.isFile()) {
        addIssue(
            analysis,
            CollectionContractSeverity::Warning,
            collection,
            QStringLiteral("%1 points to \"%2\", but it is not a file.")
                .arg(fieldName, QDir::toNativeSeparators(resolved)));
    }
}
}

int CollectionContractAnalysis::errorCount() const
{
    return std::count_if(
        issues.cbegin(), issues.cend(),
        [](const CollectionContractIssue &issue) {
            return issue.severity == CollectionContractSeverity::Error;
        });
}

int CollectionContractAnalysis::warningCount() const
{
    return std::count_if(
        issues.cbegin(), issues.cend(),
        [](const CollectionContractIssue &issue) {
            return issue.severity == CollectionContractSeverity::Warning;
        });
}

bool CollectionContractAnalysis::hasIssues() const
{
    return !issues.isEmpty();
}

bool CollectionContractAnalysis::hasIssuesForCollection(
    const QString &collectionId) const
{
    return std::any_of(
        issues.cbegin(), issues.cend(),
        [&collectionId](const CollectionContractIssue &issue) {
            return issue.collectionId == collectionId;
        });
}

QString CollectionContract::collectionId(const QString &path)
{
    if (path.trimmed().isEmpty())
        return {};

    QString normalized = QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

QString CollectionContract::collectionLabel(const Collection &collection)
{
    QString label = collection.dirName;
    if (!collection.alias.isEmpty())
        label += QStringLiteral(" | %1").arg(collection.alias);
    if (!collection.collectionClass.isEmpty())
        label += QStringLiteral(" (%1)").arg(collection.collectionClass);
    return label;
}

CollectionContractAnalysis CollectionContract::analyze(
    const QString &projectRoot,
    const QVector<Collection> &collections)
{
    CollectionContractAnalysis analysis;
    analysis.checkedCollections = collections.size();

    QHash<QString, int> indexes;
    for (int i = 0; i < collections.size(); ++i)
        indexes.insert(collectionId(collections.at(i).dirPath), i);

    QVector<int> previousTargets(collections.size(), -1);
    QVector<int> frameTargets(collections.size(), -1);
    QVector<QVector<int>> builtFromTargets(collections.size());

    for (int i = 0; i < collections.size(); ++i) {
        const Collection &collection = collections.at(i);
        const QFileInfo collectionDir(collection.dirPath);
        const QFileInfo yamlFile(
            QDir(collection.dirPath).filePath("collection.yaml"));

        if (!collectionDir.exists() || !collectionDir.isDir()) {
            addIssue(&analysis, CollectionContractSeverity::Error,
                     collection, "Collection folder does not exist.");
            continue;
        }
        if (!yamlFile.exists() || !yamlFile.isFile()) {
            addIssue(&analysis, CollectionContractSeverity::Error,
                     collection, "collection.yaml is missing.");
        }
        if (!collection.parseError.isEmpty()) {
            addIssue(&analysis, CollectionContractSeverity::Error,
                     collection,
                     QStringLiteral("collection.yaml could not be parsed: %1")
                         .arg(collection.parseError));
            continue;
        }
        if (collection.schemaVersion != SchemaVersion) {
            addIssue(
                &analysis,
                CollectionContractSeverity::Error,
                collection,
                QStringLiteral("Unsupported collection schema version %1; expected %2.")
                    .arg(collection.schemaVersion)
                    .arg(SchemaVersion));
        }
        for (const QString &field : collection.unsupportedFields) {
            addIssue(
                &analysis,
                CollectionContractSeverity::Error,
                collection,
                QStringLiteral("Field %1 is not part of schema version %2.")
                    .arg(field)
                    .arg(SchemaVersion));
        }
        if (collection.collectionClass.trimmed().isEmpty()) {
            addIssue(&analysis, CollectionContractSeverity::Error,
                     collection, "collection.yaml does not declare a class.");
            continue;
        }
        if (!classIsKnown(collection.collectionClass)) {
            addIssue(
                &analysis,
                CollectionContractSeverity::Error,
                collection,
                QStringLiteral("Unknown collection class \"%1\".")
                    .arg(collection.collectionClass));
            continue;
        }

        QString aliasError;
        if (!aliasIsSupported(collection.alias, &aliasError)) {
            addIssue(
                &analysis,
                CollectionContractSeverity::Warning,
                collection,
                QStringLiteral("Alias is not supported: %1").arg(aliasError));
        }

        const bool isPreprocessed =
            collection.collectionClass == "preprocessed_image_collection";
        const bool isOther = collection.collectionClass == "other";

        if (!collection.previousLevel.trimmed().isEmpty()) {
            if (!isPreprocessed) {
                addIssue(
                    &analysis,
                    CollectionContractSeverity::Warning,
                    collection,
                    "Previous level is set, but this collection class does not use it.");
            } else {
                const QString targetId = resolveReference(
                    projectRoot, collection.previousLevel);
                const int targetIndex = indexes.value(targetId, -1);
                if (targetIndex < 0) {
                    addIssue(
                        &analysis,
                        CollectionContractSeverity::Error,
                        collection,
                        "Previous level points to a missing or unregistered collection.");
                } else if (targetIndex == i) {
                    addIssue(
                        &analysis,
                        CollectionContractSeverity::Error,
                        collection,
                        "Previous level points to the collection itself.");
                } else if (collections.at(targetIndex).collectionClass
                           != "preprocessed_image_collection") {
                    addIssue(
                        &analysis,
                        CollectionContractSeverity::Error,
                        collection,
                        "Previous level must reference a preprocessed image collection.");
                } else {
                    previousTargets[i] = targetIndex;
                }
            }
        }

        if (collection.imageFrame.trimmed().isEmpty()) {
            if (requiresImageFrame(collection.collectionClass)) {
                addIssue(&analysis, CollectionContractSeverity::Error,
                         collection, "Image frame is required but empty.");
            }
        } else if (!requiresImageFrame(collection.collectionClass)) {
            addIssue(
                &analysis,
                CollectionContractSeverity::Warning,
                collection,
                "Image frame is set, but this collection class does not use it.");
        } else {
            const QString targetId = resolveReference(
                projectRoot, collection.imageFrame);
            const int targetIndex = indexes.value(targetId, -1);
            if (targetIndex < 0) {
                addIssue(
                    &analysis,
                    CollectionContractSeverity::Error,
                    collection,
                    "Image frame points to a missing or unregistered collection.");
            } else if (targetIndex == i) {
                addIssue(
                    &analysis,
                    CollectionContractSeverity::Error,
                    collection,
                    "Image frame points to the collection itself.");
            } else if (collections.at(targetIndex).collectionClass
                       != "preprocessed_image_collection") {
                addIssue(
                    &analysis,
                    CollectionContractSeverity::Error,
                    collection,
                    "Image frame must reference a preprocessed image collection.");
            } else {
                frameTargets[i] = targetIndex;
            }
        }

        if (isOther && !collection.builtFrom.isEmpty()) {
            addIssue(
                &analysis,
                CollectionContractSeverity::Warning,
                collection,
                "Built from is set, but Other collections are independent.");
        } else if (!isOther) {
            QSet<QString> seenSources;
            for (const QString &source : collection.builtFrom) {
                const QString sourceId = resolveReference(projectRoot, source);
                if (sourceId.isEmpty())
                    continue;
                if (seenSources.contains(sourceId)) {
                    addIssue(
                        &analysis,
                        CollectionContractSeverity::Warning,
                        collection,
                        "Built from contains a duplicate collection.");
                    continue;
                }
                seenSources.insert(sourceId);
                const int sourceIndex = indexes.value(sourceId, -1);
                if (sourceIndex < 0) {
                    addIssue(
                        &analysis,
                        CollectionContractSeverity::Warning,
                        collection,
                        "Built from points to a missing or unregistered collection.");
                } else if (sourceIndex == i) {
                    addIssue(
                        &analysis,
                        CollectionContractSeverity::Warning,
                        collection,
                        "Built from contains the collection itself.");
                } else if (collections.at(sourceIndex).collectionClass
                           == "other") {
                    addIssue(
                        &analysis,
                        CollectionContractSeverity::Warning,
                        collection,
                        "Built from cannot reference an independent Other collection.");
                } else {
                    builtFromTargets[i].append(sourceIndex);
                }
            }
        }

        if (isPreprocessed || isOther) {
            checkFileReference(projectRoot, collection, "Registry",
                               collection.registry, &analysis);
        } else if (!collection.registry.trimmed().isEmpty()) {
            addIssue(
                &analysis,
                CollectionContractSeverity::Warning,
                collection,
                "Registry is set, but this collection class does not use it.");
        }
        checkFileReference(projectRoot, collection, "Script",
                           collection.script, &analysis);
    }

    QVector<int> previousState(collections.size(), 0);
    QSet<QString> previousCycleIssues;
    std::function<bool(int)> resolvePreprocessed;
    resolvePreprocessed = [&](int index) -> bool {
        const Collection &collection = collections.at(index);
        if (collection.collectionClass != "preprocessed_image_collection")
            return false;
        if (previousState[index] == 2) {
            return analysis.placements.value(
                collectionId(collection.dirPath)).resolved;
        }
        if (previousState[index] == 1) {
            const QString id = collectionId(collection.dirPath);
            if (!previousCycleIssues.contains(id)) {
                previousCycleIssues.insert(id);
                addIssue(&analysis, CollectionContractSeverity::Error,
                         collection,
                         "Previous level relations contain a cycle.");
            }
            return false;
        }

        previousState[index] = 1;
        CollectionGraphPlacement placement;
        placement.frameId = collectionId(collection.dirPath);
        if (collection.previousLevel.trimmed().isEmpty()) {
            placement.branchRootId = placement.frameId;
            placement.preprocessingLevel = 0;
            placement.resolved = true;
        } else {
            const int targetIndex = previousTargets.at(index);
            if (targetIndex >= 0 && resolvePreprocessed(targetIndex)) {
                const CollectionGraphPlacement previous =
                    analysis.placements.value(
                        collectionId(collections.at(targetIndex).dirPath));
                placement.branchRootId = previous.branchRootId;
                placement.preprocessingLevel =
                    previous.preprocessingLevel + 1;
                placement.resolved = true;
            }
        }
        analysis.placements.insert(placement.frameId, placement);
        previousState[index] = 2;
        return placement.resolved;
    };

    for (int i = 0; i < collections.size(); ++i) {
        if (collections.at(i).collectionClass
            == "preprocessed_image_collection") {
            resolvePreprocessed(i);
        }
    }

    for (int i = 0; i < collections.size(); ++i) {
        const Collection &collection = collections.at(i);
        const QString id = collectionId(collection.dirPath);
        if (collection.collectionClass == "preprocessed_image_collection")
            continue;

        CollectionGraphPlacement placement;
        if (frameTargets.at(i) >= 0) {
            const Collection &frame = collections.at(frameTargets.at(i));
            const CollectionGraphPlacement framePlacement =
                analysis.placements.value(collectionId(frame.dirPath));
            if (framePlacement.resolved) {
                placement = framePlacement;
                placement.frameId = collectionId(frame.dirPath);
            }
        }
        if (collection.collectionClass == "other")
            placement.resolved = true;
        analysis.placements.insert(id, placement);
    }

    QVector<int> builtState(collections.size(), 0);
    QSet<QString> builtCycleIssues;
    std::function<void(int)> visitBuiltFrom;
    visitBuiltFrom = [&](int index) {
        if (builtState[index] == 2)
            return;
        if (builtState[index] == 1) {
            const Collection &collection = collections.at(index);
            const QString id = collectionId(collection.dirPath);
            if (!builtCycleIssues.contains(id)) {
                builtCycleIssues.insert(id);
                addIssue(&analysis, CollectionContractSeverity::Warning,
                         collection,
                         "Built from relations contain a cycle.");
            }
            return;
        }
        builtState[index] = 1;
        for (const int sourceIndex : builtFromTargets.at(index))
            visitBuiltFrom(sourceIndex);
        builtState[index] = 2;
    };
    for (int i = 0; i < collections.size(); ++i)
        visitBuiltFrom(i);

    std::sort(
        analysis.issues.begin(), analysis.issues.end(),
        [](const CollectionContractIssue &left,
           const CollectionContractIssue &right) {
            if (left.severity != right.severity) {
                return left.severity == CollectionContractSeverity::Error;
            }
            const int collectionCompare = QString::compare(
                left.collectionLabel, right.collectionLabel,
                Qt::CaseInsensitive);
            if (collectionCompare != 0)
                return collectionCompare < 0;
            return QString::compare(
                left.message, right.message, Qt::CaseInsensitive) < 0;
        });
    return analysis;
}
