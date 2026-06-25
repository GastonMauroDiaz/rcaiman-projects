#include "metadataregistry.h"
#include "collectionvalidation.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QString error;
    if (!ExifToolService::isAvailable(&error))
        return 1;
    if (ExifToolService::version(&error) != "13.59")
        return 2;

    const QString executable = ExifToolService::executablePath();
    QList<MetadataObservation> observations;
    if (!ExifToolService::extract({executable}, &observations, &error)
        || observations.isEmpty()) {
        return 3;
    }

    QTemporaryDir directory;
    if (!directory.isValid())
        return 4;
    const MetadataRule rule = {
        "camera_model", "IFD0", "Model", "Camera Model Name", "E5700"
    };
    const MetadataRule escapedRule = {
        "quoted_value", "EXIF", "Comment", "Quoted \"label\"",
        "line 1\nline 2\\path"
    };
    const QString draftPath = QDir(directory.path()).filePath("draft.yaml");
    if (!MetadataRegistry::writeDraft(
            draftPath, directory.path(), {executable}, {rule, escapedRule},
            "13.59", &error)) {
        return 5;
    }
    QList<MetadataRule> loadedRules;
    if (!MetadataRegistry::readRules(draftPath, &loadedRules, &error)
        || loadedRules.size() != 2
        || loadedRules.first().key() != rule.key()) {
        return 6;
    }
    QFile snippet(QDir(directory.path()).filePath("draft_snippet.R"));
    if (!snippet.open(QIODevice::ReadOnly)) {
        return 10;
    }
    const QByteArray snippetText = snippet.readAll();
    snippet.close();
    if (!snippetText.contains("metadata_rules <- list(")
        || !snippetText.contains("Quoted \\\"label\\\"")
        || !snippetText.contains("line 1\\nline 2\\\\path")) {
        return 11;
    }
    QString rscript = QStandardPaths::findExecutable("Rscript");
#ifdef Q_OS_WIN
    if (rscript.isEmpty()) {
        const QString installed =
            "C:/Program Files/R/R-4.5.1/bin/Rscript.exe";
        if (QFileInfo::exists(installed))
            rscript = installed;
    }
#endif
    if (!rscript.isEmpty()) {
        QProcess parser;
        parser.start(
            rscript,
            {
                "-e",
                "parse(file=commandArgs(trailingOnly=TRUE)[1])",
                snippet.fileName()
            });
        if (!parser.waitForFinished(30000)
            || parser.exitStatus() != QProcess::NormalExit
            || parser.exitCode() != 0) {
            return 40;
        }
    }

    const QList<MetadataObservation> duplicateValues = {
        {"EXIF", "CameraModelName", "E5700", executable},
        {"EXIF", "CameraModelName", "E5700 alias", executable}
    };
    if (MetadataRegistry::candidates(
            rule, duplicateValues, 5).size() != 2) {
        return 7;
    }

    const MetadataRuleDecision decision = {
        rule, "IFD0", "Model", "E5700", false, false
    };
    const QString reviewPath = QDir(directory.path()).filePath("review.yaml");
    if (!MetadataRegistry::writeReview(
            reviewPath, directory.path(), executable, draftPath,
            "13.59", {decision}, true, &error)) {
        return 8;
    }
    QFile review(reviewPath);
    if (!review.open(QIODevice::ReadOnly)
        || !review.readAll().contains("decision: accepted")) {
        return 9;
    }

    const QString projectRoot = QDir(directory.path()).filePath("project");
    const QString rawDirectory = QDir(projectRoot).filePath("raw_files");
    const QString collectionDirectory =
        QDir(projectRoot).filePath("preprocessed_images");
    QDir().mkpath(rawDirectory);
    QDir().mkpath(collectionDirectory);
    const QString rawPath = QDir(rawDirectory).filePath("DSCN6280.NEF");
    const QString imagePath =
        QDir(collectionDirectory).filePath("dscn6280.tif");
    if (!QFile::copy(executable, rawPath)) {
        return 12;
    }
    QFile image(imagePath);
    if (!image.open(QIODevice::WriteOnly) || image.write("image") < 0) {
        return 13;
    }
    image.close();

    Collection collection;
    collection.dirPath = collectionDirectory;
    collection.dirName = "preprocessed_images";
    collection.collectionClass = "preprocessed_image_collection";
    collection.registry = QDir::fromNativeSeparators(
        QDir(projectRoot).relativeFilePath(draftPath));
    const QList<CollectionFileValidation> pairs =
        CollectionValidation::resolveRawPairs(projectRoot, collection);
    if (pairs.size() != 1
        || QFileInfo(pairs.first().rawPath).fileName() != "DSCN6280.NEF") {
        return 14;
    }

    QFile duplicate(QDir(rawDirectory).filePath("dscn6280.DNG"));
    if (!duplicate.open(QIODevice::WriteOnly)
        || duplicate.write("duplicate") < 0) {
        return 15;
    }
    duplicate.close();
    if (CollectionValidation::resolveRawPairs(
            projectRoot, collection).first().status
        != CollectionFileValidationStatus::AmbiguousRaw) {
        return 16;
    }
    QFile::remove(duplicate.fileName());

    QFile missing(QDir(collectionDirectory).filePath("missing.tif"));
    if (!missing.open(QIODevice::WriteOnly)
        || missing.write("image") < 0) {
        return 17;
    }
    missing.close();
    const QList<CollectionFileValidation> mixedPairs =
        CollectionValidation::resolveRawPairs(projectRoot, collection);
    if (mixedPairs.size() != 2
        || mixedPairs.last().status
            != CollectionFileValidationStatus::MissingRaw) {
        return 18;
    }
    QFile::remove(missing.fileName());

    QList<MetadataObservation> rawObservations;
    if (!ExifToolService::extract(
            {rawPath}, &rawObservations, &error)
        || rawObservations.isEmpty()) {
        return 19;
    }
    MetadataObservation observed;
    for (const MetadataObservation &candidate : rawObservations) {
        if (!candidate.group.isEmpty() && !candidate.tag.isEmpty()
            && !candidate.value.isEmpty()) {
            observed = candidate;
            break;
        }
    }
    if (observed.key() == ":")
        return 20;

    const QString registryDirectory =
        QDir(projectRoot).filePath("registry_drafts");
    QDir().mkpath(registryDirectory);
    const QString exactRegistry =
        QDir(registryDirectory).filePath("exact.yaml");
    const MetadataRule exactRule = {
        "observed_rule", observed.group, observed.tag,
        "Observed Rule", observed.value
    };
    if (!MetadataRegistry::writeDraft(
            exactRegistry, projectRoot, {rawPath}, {exactRule},
            "13.59", &error)) {
        return 21;
    }
    collection.registry = "registry_drafts/exact.yaml";
    QDir().mkpath(QDir(projectRoot).filePath("registry_validations"));
    CollectionValidationRun exactRun;
    if (!CollectionValidation::validate(
            projectRoot, collection, &exactRun, &error)
        || exactRun.result != "passed_exact"
        || exactRun.files.first().status
            != CollectionFileValidationStatus::ExactMatch) {
        return 22;
    }
    const QString manifest = QDir(projectRoot).filePath(
        "registry_validations/preprocessed_images_20260625_120000.yaml");
    if (!CollectionValidation::writeManifest(
            manifest, projectRoot, exactRun, &error)) {
        return 23;
    }
    CollectionValidationReportStatus report =
        CollectionValidation::latestReportStatus(projectRoot, collection);
    if (!report.exists || report.stale
        || report.result != "passed_exact") {
        return 24;
    }
    if (!image.open(QIODevice::Append) || image.write("changed") < 0)
        return 25;
    image.close();
    report = CollectionValidation::latestReportStatus(
        projectRoot, collection);
    if (!report.stale)
        return 26;

    const QString discoveredRegistry =
        QDir(collectionDirectory).filePath("camera_registry.yaml");
    if (!QFile::copy(exactRegistry, discoveredRegistry))
        return 41;
    collection.registry.clear();
    QString resolvedRegistry;
    QString registryError;
    if (!CollectionValidation::resolveRegistry(
            projectRoot, collection, &resolvedRegistry, &registryError)
        || QFileInfo(resolvedRegistry).absoluteFilePath()
            != QFileInfo(discoveredRegistry).absoluteFilePath()) {
        return 42;
    }
    const QString secondRegistry =
        QDir(collectionDirectory).filePath("second_registry.yaml");
    if (!QFile::copy(exactRegistry, secondRegistry)
        || CollectionValidation::resolveRegistry(
            projectRoot, collection, &resolvedRegistry, &registryError)
        || !registryError.contains("Multiple compatible registries")) {
        return 43;
    }
    QFile::remove(secondRegistry);
    QFile::remove(discoveredRegistry);

    const QString reviewedRegistry =
        QDir(registryDirectory).filePath("reviewed.yaml");
    MetadataRule reviewedRule = exactRule;
    reviewedRule.expected = observed.value + "_different";
    if (!MetadataRegistry::writeDraft(
            reviewedRegistry, projectRoot, {rawPath}, {reviewedRule},
            "13.59", &error)) {
        return 27;
    }
    QDir().mkpath(QDir(projectRoot).filePath("registry_reviews"));
    const MetadataRuleDecision acceptedDecision = {
        reviewedRule, observed.group, observed.tag, observed.value,
        false, true
    };
    const QString acceptedReview =
        QDir(projectRoot).filePath("registry_reviews/accepted.yaml");
    if (!MetadataRegistry::writeReview(
            acceptedReview, projectRoot, rawPath, reviewedRegistry,
            "13.59", {acceptedDecision}, true, &error)) {
        return 28;
    }
    collection.registry = "registry_drafts/reviewed.yaml";
    CollectionValidationRun reviewedRun;
    if (!CollectionValidation::validate(
            projectRoot, collection, &reviewedRun, &error)
        || reviewedRun.result != "passed_with_reviews"
        || reviewedRun.files.first().status
            != CollectionFileValidationStatus::AcceptedReview) {
        return 29;
    }
    reviewedRun.validatedAt = "2099-06-25T13:00:00Z";
    const QString reviewedManifest = QDir(projectRoot).filePath(
        "registry_validations/preprocessed_images_20260625_130000.yaml");
    if (!CollectionValidation::writeManifest(
            reviewedManifest, projectRoot, reviewedRun, &error)) {
        return 30;
    }
    QFile projectFile(QDir(projectRoot).filePath("project.yaml"));
    if (!projectFile.open(QIODevice::WriteOnly | QIODevice::Text)
        || projectFile.write(
            "version: 1\nproject_name: test\ncreated: 2026-06-25\n")
            < 0) {
        return 31;
    }
    projectFile.close();
    QFile collectionFile(
        QDir(collectionDirectory).filePath("collection.yaml"));
    if (!collectionFile.open(QIODevice::WriteOnly | QIODevice::Text)
        || collectionFile.write(
            "schema_version: 1\n"
            "class: preprocessed_image_collection\n"
            "registry: registry_drafts/reviewed.yaml\n") < 0) {
        return 32;
    }
    collectionFile.close();
    const Project project = Project::open(projectRoot, &error);
    if (!project.isValid() || !project.writeReport(&error))
        return 33;
    QFile reportFile(project.reportFilePath());
    if (!reportFile.open(QIODevice::ReadOnly)
        || !reportFile.readAll().contains("Passed with reviews")) {
        return 34;
    }
    reportFile.close();
    QFile orphan(QDir(rawDirectory).filePath("orphan.NEF"));
    if (!orphan.open(QIODevice::WriteOnly)
        || orphan.write("orphan") < 0) {
        return 35;
    }
    orphan.close();
    if (CollectionValidation::latestReportStatus(
            projectRoot, collection).stale) {
        return 36;
    }
    QFile added(QDir(collectionDirectory).filePath("added.tif"));
    if (!added.open(QIODevice::WriteOnly)
        || added.write("image") < 0) {
        return 37;
    }
    added.close();
    if (!CollectionValidation::latestReportStatus(
            projectRoot, collection).stale) {
        return 38;
    }
    QFile::remove(added.fileName());
    if (!QFile::remove(acceptedReview)
        || !CollectionValidation::latestReportStatus(
                projectRoot, collection).stale) {
        return 39;
    }
    const QString rejectedReview =
        QDir(projectRoot).filePath("registry_reviews/rejected.yaml");
    if (!MetadataRegistry::writeReview(
            rejectedReview, projectRoot, rawPath, reviewedRegistry,
            "13.59", {acceptedDecision}, false, &error)) {
        return 44;
    }
    CollectionValidationRun rejectedRun;
    if (!CollectionValidation::validate(
            projectRoot, collection, &rejectedRun, &error)
        || rejectedRun.result != "failed"
        || rejectedRun.files.first().status
            != CollectionFileValidationStatus::RejectedReview
        || rejectedRun.files.first().reviewPath != rejectedReview) {
        return 45;
    }
    return 0;
}
