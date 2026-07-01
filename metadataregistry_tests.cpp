#include "metadataregistry.h"
#include "collectionvalidation.h"
#include "acquisitionmetadata.h"

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

    collection.registry.clear();
    QString resolvedRegistry;
    QString registryError;
    if (CollectionValidation::resolveRegistry(
            projectRoot, collection, &resolvedRegistry, &registryError)
        || !resolvedRegistry.isEmpty()
        || !registryError.contains("No registry is declared")) {
        return 42;
    }
    collection.registry = "registry_drafts/exact.yaml";

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
    const QString scriptsDirectory =
        QDir(projectRoot).filePath("scripts");
    QDir().mkpath(scriptsDirectory);
    QFile scriptFile(QDir(scriptsDirectory).filePath(
        "01_preprocessing_foo.R"));
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)
        || scriptFile.write(
            "# Preprocess calibration images.\n"
            "# Produces the preprocessed image collection.\n"
            "\n"
            "message('ok')\n") < 0) {
        return 58;
    }
    scriptFile.close();
    if (!collectionFile.open(QIODevice::WriteOnly | QIODevice::Text)
        || collectionFile.write(
            "schema_version: 1\n"
            "class: preprocessed_image_collection\n"
            "registry: registry_drafts/reviewed.yaml\n"
            "script: scripts/01_preprocessing_foo.R\n") < 0) {
        return 32;
    }
    collectionFile.close();
    const Project project = Project::open(projectRoot, &error);
    if (!project.isValid() || !project.writeReport(&error))
        return 33;
    QFile reportFile(project.reportFilePath());
    if (!reportFile.open(QIODevice::ReadOnly)) {
        return 34;
    }
    const QByteArray reportText = reportFile.readAll();
    if (!reportText.contains("Passed with reviews")) {
        return 34;
    }
    if (!reportText.contains("## Processing Story")
        || !reportText.contains("Preprocess calibration images.")
        || !reportText.contains("`preprocessed_images`")
        || reportText.contains("## Collection Details")) {
        return 59;
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

    const QList<MetadataObservation> acquisitionObservations = {
        {"ExifIFD", "FNumber", "3.2", rawPath},
        {"ExifIFD", "ExposureTime", "1/200", rawPath},
        {"ExifIFD", "ExposureCompensation", "-1/3", rawPath},
        {"Nikon", "ISO", "0", rawPath}
    };
    const AcquisitionMetadataValues parsedValues =
        AcquisitionMetadata::valuesFromObservations(
            acquisitionObservations);
    if (!parsedValues.hasAperture
        || qAbs(parsedValues.aperture - 3.2) > 0.000001
        || !parsedValues.hasExposureTime
        || qAbs(parsedValues.exposureTimeSeconds - 0.005) > 0.000001
        || !parsedValues.hasExposureCompensation
        || qAbs(parsedValues.exposureCompensationEv + (1.0 / 3.0)) > 0.000001
        || parsedValues.hasIso) {
        return 46;
    }

    const QString sidecarProject =
        QDir(directory.path()).filePath("sidecar_project");
    const QString sidecarSensorDirectory =
        QDir(sidecarProject).filePath("sensor_data");
    const QString sidecarRawDirectory =
        QDir(sidecarProject).filePath("raw_files");
    QDir().mkpath(sidecarSensorDirectory);
    QDir().mkpath(sidecarRawDirectory);
    const QString directSensor =
        QDir(sidecarSensorDirectory).filePath("with spaces.tif");
    QFile directSensorFile(directSensor);
    if (!directSensorFile.open(QIODevice::WriteOnly)
        || directSensorFile.write("image") < 0) {
        return 47;
    }
    directSensorFile.close();
    const QString directRaw =
        QDir(sidecarRawDirectory).filePath("with spaces.NEF");
    if (!QFile::copy(executable, directRaw))
        return 48;
    AcquisitionMetadataValues directValues;
    directValues.hasAperture = true;
    directValues.aperture = 3.2;
    directValues.hasExposureTime = true;
    directValues.exposureTimeSeconds = 0.005;
    directValues.hasExposureCompensation = true;
    directValues.exposureCompensationEv = -0.3333333333333333;
    directValues.hasLibRawBlack = true;
    directValues.libRawBlack = 512;
    directValues.hasLibRawCblack = true;
    directValues.libRawCblack[0] = 2;
    directValues.libRawCblack[1] = 0;
    directValues.libRawCblack[2] = 1;
    directValues.libRawCblack[3] = 0;
    directValues.dateTimeOriginal = "2026:06:29 10:15:30";
    directValues.hasGpsLatitude = true;
    directValues.gpsLatitude = -42.75;
    directValues.hasGpsLongitude = true;
    directValues.gpsLongitude = -71.25;
    const QString directSidecar =
        QDir(sidecarSensorDirectory).filePath("with spaces.tsv");
    if (!AcquisitionMetadata::writeSidecar(
            directSidecar, sidecarProject, directRaw, directSensor,
            directValues, "error",
            "tab\tquote \"value\"\nsecond line", &error)) {
        return 49;
    }
    QFile directSidecarFile(directSidecar);
    if (!directSidecarFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 50;
    }
    const QByteArray directText = directSidecarFile.readAll();
    directSidecarFile.close();
    if (!directText.startsWith(
            "basename\traw_file\tsensor_file\taperture\t"
            "exposure_time_seconds\tiso\texposure_compensation_ev\t"
            "libraw_black\tlibraw_cblack_0\tlibraw_cblack_1\t"
            "libraw_cblack_2\tlibraw_cblack_3\tdate_time_original\t"
            "gps_latitude\tgps_longitude\tgps_altitude\tmetadata_status\t"
            "metadata_error\n")
        || !directText.contains("raw_files/with spaces.NEF")
        || !directText.contains(
            "\t3.2\t0.005\t\t-0.3333333333333333\t512\t2\t0\t1\t0\t"
            "2026:06:29 10:15:30\t-42.75\t-71.25\t\terror\t")
        || !directText.contains("\"tab\tquote \"\"value\"\"\n"
                                "second line\"")) {
        return 51;
    }
    if (!rscript.isEmpty()) {
        const QString expression =
            "p<-commandArgs(trailingOnly=TRUE)[1];"
            "a<-read.delim(p,na.strings='',check.names=FALSE);"
            "b<-read.table(p,header=TRUE,sep='\\t',na.strings='',"
            "check.names=FALSE);"
            "stopifnot(nrow(a)==1,nrow(b)==1,"
            "is.numeric(a$aperture),"
            "abs(a$exposure_time_seconds-0.005)<1e-12,"
            "abs(a$exposure_compensation_ev+1/3)<1e-12,"
            "abs(a$libraw_black-512)<1e-12,"
            "abs(a$libraw_cblack_0-2)<1e-12,"
            "abs(a$libraw_cblack_1-0)<1e-12,"
            "abs(a$libraw_cblack_2-1)<1e-12,"
            "abs(a$libraw_cblack_3-0)<1e-12,"
            "abs(a$gps_latitude+42.75)<1e-12,"
            "abs(a$gps_longitude+71.25)<1e-12,"
            "is.na(a$iso))";
        QProcess parser;
        parser.start(rscript, {"-e", expression, directSidecar});
        if (!parser.waitForFinished(30000)
            || parser.exitStatus() != QProcess::NormalExit
            || parser.exitCode() != 0) {
            return 52;
        }
    }

    const QString exactSensor =
        QDir(sidecarSensorDirectory).filePath("exact.tif");
    const QString missingSensor =
        QDir(sidecarSensorDirectory).filePath("missing.tif");
    const QString ambiguousSensor =
        QDir(sidecarSensorDirectory).filePath("ambiguous.tif");
    for (const QString &path :
         {exactSensor, missingSensor, ambiguousSensor}) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)
            || file.write("image") < 0) {
            return 53;
        }
    }
    if (!QFile::copy(
            executable,
            QDir(sidecarRawDirectory).filePath("EXACT.NEF"))
        || !QFile::copy(
            executable,
            QDir(sidecarRawDirectory).filePath("ambiguous.NEF"))
        || !QFile::copy(
            executable,
            QDir(sidecarRawDirectory).filePath("AMBIGUOUS.DNG"))) {
        return 54;
    }
    const QString orphanSidecar =
        QDir(sidecarSensorDirectory).filePath("orphan.tsv");
    QFile orphanSidecarFile(orphanSidecar);
    if (!orphanSidecarFile.open(QIODevice::WriteOnly)
        || orphanSidecarFile.write("orphan") < 0) {
        return 55;
    }
    orphanSidecarFile.close();

    AcquisitionMetadataSummary acquisitionSummary;
    if (!AcquisitionMetadata::regenerate(
            sidecarProject, &acquisitionSummary, &error)
        || acquisitionSummary.sidecarsWritten != 4
        || acquisitionSummary.metadataErrors != 2
        || acquisitionSummary.orphanSidecarsRemoved != 1
        || QFileInfo::exists(orphanSidecar)) {
        return 56;
    }
    QFile exactSidecar(
        QDir(sidecarSensorDirectory).filePath("exact.tsv"));
    QFile missingSidecar(
        QDir(sidecarSensorDirectory).filePath("missing.tsv"));
    QFile ambiguousSidecar(
        QDir(sidecarSensorDirectory).filePath("ambiguous.tsv"));
    if (!exactSidecar.open(QIODevice::ReadOnly | QIODevice::Text)
        || !exactSidecar.readAll().contains("\tok\t\n")
        || !missingSidecar.open(QIODevice::ReadOnly | QIODevice::Text)
        || !missingSidecar.readAll().contains(
            "\terror\tNo RAW with basename")
        || !ambiguousSidecar.open(
            QIODevice::ReadOnly | QIODevice::Text)
        || !ambiguousSidecar.readAll().contains(
            "\terror\tMultiple RAW files with basename")) {
        return 57;
    }
    return 0;
}
