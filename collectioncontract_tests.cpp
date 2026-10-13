// SPDX-License-Identifier: GPL-3.0-or-later

#include "collectioncontract.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

namespace {
Collection makeCollection(const QString &root,
                          const QString &name,
                          const QString &collectionClass,
                          bool *created = nullptr)
{
    Collection collection;
    collection.schemaVersion = CollectionContract::SchemaVersion;
    collection.dirPath = QDir(root).filePath(name);
    collection.dirName = name;
    collection.collectionClass = collectionClass;
    QDir().mkpath(collection.dirPath);
    QFile descriptor(QDir(collection.dirPath).filePath("collection.yaml"));
    const bool descriptorCreated = descriptor.open(QIODevice::WriteOnly);
    if (created)
        *created = descriptorCreated;
    return collection;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid())
        return 1;

    bool descriptorsCreated = true;
    bool descriptorCreated = false;
    Collection l0 = makeCollection(
        directory.path(), "blue", "preprocessed_image_collection",
        &descriptorCreated);
    descriptorsCreated = descriptorsCreated && descriptorCreated;
    Collection l1 = makeCollection(
        directory.path(), "blue_crop", "preprocessed_image_collection",
        &descriptorCreated);
    descriptorsCreated = descriptorsCreated && descriptorCreated;
    l1.previousLevel = "blue";
    Collection points = makeCollection(
        directory.path(), "points_crop", "point_collection",
        &descriptorCreated);
    descriptorsCreated = descriptorsCreated && descriptorCreated;
    if (!descriptorsCreated)
        return 2;
    points.imageFrame = "blue_crop";
    points.builtFrom = {"blue", "blue_crop"};

    QVector<Collection> collections = {l0, l1, points};
    CollectionContractAnalysis analysis =
        CollectionContract::analyze(directory.path(), collections);
    if (analysis.hasIssues())
        return 3;
    const CollectionGraphPlacement l1Placement = analysis.placements.value(
        CollectionContract::collectionId(l1.dirPath));
    const CollectionGraphPlacement pointPlacement = analysis.placements.value(
        CollectionContract::collectionId(points.dirPath));
    if (!l1Placement.resolved || l1Placement.preprocessingLevel != 1)
        return 4;
    if (!pointPlacement.resolved
        || pointPlacement.frameId
            != CollectionContract::collectionId(l1.dirPath)
        || pointPlacement.preprocessingLevel != 1) {
        return 5;
    }

    collections[0].previousLevel = "blue_crop";
    analysis = CollectionContract::analyze(directory.path(), collections);
    if (analysis.errorCount() == 0)
        return 6;

    collections[0].previousLevel.clear();
    collections[2].imageFrame.clear();
    analysis = CollectionContract::analyze(directory.path(), collections);
    if (analysis.errorCount() == 0)
        return 7;

    return 0;
}
