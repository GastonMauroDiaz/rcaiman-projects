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

#include "metadataregistry.h"
#include "collectionvalidation.h"
#include "acquisitionmetadata.h"
#include "collectionstylefile.h"
#include "hspprojectimporter.h"
#include "project.h"
#include "pgmreader.h"
#include "tiffreader.h"
#include "tiffwriter.h"

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

    AcquisitionMetadataValues zeroBlackValues;
    zeroBlackValues.hasLibRawBlack = true;
    zeroBlackValues.libRawBlack = 0.0;
    zeroBlackValues.hasLibRawCblack = true;
    const QString zeroBlackSidecar =
        QDir(directory.path()).filePath("zero_black.tsv");
    if (!AcquisitionMetadata::writeSidecar(
            zeroBlackSidecar,
            directory.path(),
            QDir(directory.path()).filePath("raw_files/raw.NEF"),
            QDir(directory.path()).filePath("sensor_data/raw.tif"),
            zeroBlackValues,
            "ok",
            {},
            &error)) {
        return 82;
    }
    QFile zeroBlackFile(zeroBlackSidecar);
    if (!zeroBlackFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return 83;
    const QList<QByteArray> zeroBlackLines =
        zeroBlackFile.readAll().split('\n');
    if (zeroBlackLines.size() < 2)
        return 84;
    const QList<QByteArray> zeroBlackFields =
        zeroBlackLines.at(1).split('\t');
    if (zeroBlackFields.size() < 12
        || zeroBlackFields.at(7) != "0"
        || zeroBlackFields.at(8) != "0"
        || zeroBlackFields.at(9) != "0"
        || zeroBlackFields.at(10) != "0"
        || zeroBlackFields.at(11) != "0") {
        return 85;
    }

    const QString pgm8Path = QDir(directory.path()).filePath("pgm8.pgm");
    QFile pgm8(pgm8Path);
    if (!pgm8.open(QIODevice::WriteOnly)
        || pgm8.write("P5\n2 2\n255\n") < 0
        || pgm8.write(QByteArray::fromRawData("\x00\x7f\x80\xff", 4)) < 0) {
        return 60;
    }
    pgm8.close();
    RasterData pgm8Data = PgmReader::load(pgm8Path, &error);
    if (pgm8Data.isNull()
        || pgm8Data.width() != 2
        || pgm8Data.height() != 2
        || pgm8Data.dataType() != RasterData::DataType::UInt8
        || pgm8Data.sample(0, 1, 1).toInt() != 255) {
        return 61;
    }

    const QString pgm16Path = QDir(directory.path()).filePath("pgm16.pgm");
    QFile pgm16(pgm16Path);
    const char pgm16Pixels[] = {
        '\x00', '\x01', '\x01', '\x02',
        '\x03', '\xff', '\x04', '\x00'
    };
    if (!pgm16.open(QIODevice::WriteOnly)
        || pgm16.write("P5\n2 2\n1024\n") < 0
        || pgm16.write(pgm16Pixels, sizeof(pgm16Pixels)) < 0) {
        return 62;
    }
    pgm16.close();
    RasterData pgm16Data = PgmReader::load(pgm16Path, &error);
    if (pgm16Data.isNull()
        || pgm16Data.dataType() != RasterData::DataType::UInt16
        || pgm16Data.sample(0, 0, 1).toInt() != 258
        || pgm16Data.sample(0, 1, 1).toInt() != 1024) {
        return 63;
    }

    RasterData rgbData;
    rgbData.allocate(2, 1, 3, RasterData::DataType::UInt8);
    rgbData.rawData()[0] = static_cast<char>(10);
    rgbData.rawData()[1] = static_cast<char>(20);
    rgbData.rawData()[2] = static_cast<char>(30);
    rgbData.rawData()[3] = static_cast<char>(40);
    rgbData.rawData()[4] = static_cast<char>(50);
    rgbData.rawData()[5] = static_cast<char>(60);
    const QString rgbTiffPath =
        QDir(directory.path()).filePath("rgb.tif");
    if (!TiffWriter::write(rgbTiffPath, rgbData, &error))
        return 86;
    const RasterData rgbRead = TiffReader::load(rgbTiffPath, &error);
    if (rgbRead.isNull()
        || rgbRead.width() != 2
        || rgbRead.height() != 1
        || rgbRead.bands() != 3
        || rgbRead.dataType() != RasterData::DataType::UInt8
        || rgbRead.sample(0, 0, 1).toUInt() != 20
        || rgbRead.sample(1, 0, 1).toUInt() != 40
        || rgbRead.sample(2, 0, 1).toUInt() != 60) {
        return 87;
    }

    const QString hspSource = QDir(directory.path()).filePath("legacy_hsp");
    QDir().mkpath(QDir(hspSource).filePath("raw"));
    QDir().mkpath(QDir(hspSource).filePath("manipulate"));
    QDir().mkpath(QDir(hspSource).filePath("info"));
    QFile hspProject(QDir(hspSource).filePath("project.conf"));
    if (!hspProject.open(QIODevice::WriteOnly | QIODevice::Text)
        || hspProject.write(
            "#Thu Apr 30 11:44:41 EEST 2020\n"
            "name=legacy_hsp\n"
            "comment = Field note from HSP\n") < 0) {
        return 64;
    }
    hspProject.close();
    if (!QFile::copy(executable, QDir(hspSource).filePath("raw/test.NEF")))
        return 65;
    if (!QFile::copy(pgm16Path, QDir(hspSource).filePath("manipulate/test.pgm")))
        return 66;
    QFile hspInfo(QDir(hspSource).filePath("info/test.conf"));
    if (!hspInfo.open(QIODevice::WriteOnly | QIODevice::Text)
        || hspInfo.write("group_name=Group A\n") < 0) {
        return 67;
    }
    hspInfo.close();
    QFile hspPoints(QDir(hspSource).filePath("manipulate/test_points.conf"));
    if (!hspPoints.open(QIODevice::WriteOnly | QIODevice::Text)
        || hspPoints.write("2\n10.20.3 30.40.3\n") < 0) {
        return 68;
    }
    hspPoints.close();
    QFile hspSun(QDir(hspSource).filePath("manipulate/test_sun.conf"));
    if (!hspSun.open(QIODevice::WriteOnly | QIODevice::Text)
        || hspSun.write("50.60\n") < 0) {
        return 69;
    }
    hspSun.close();
    QFile hspOpt(QDir(hspSource).filePath("manipulate/test_opt-parameters.conf"));
    if (!hspOpt.open(QIODevice::WriteOnly | QIODevice::Text)
        || hspOpt.write(
            "#Tue Feb 18 11:14:19 ART 2025\n"
            "b=-0,5\n"
            "a=3,25\n"
            "e=4,5\n"
            "d=-1\n"
            "c=0,125\n") < 0) {
        return 75;
    }
    hspOpt.close();
    QFile hspMasks(QDir(hspSource).filePath("manipulate/test_masks.conf"));
    if (!hspMasks.open(QIODevice::WriteOnly | QIODevice::Text)
        || hspMasks.write("1\n4 0.0 2.0 2.2 0.2\n") < 0) {
        return 86;
    }
    hspMasks.close();
    QFile hspPolygons(QDir(hspSource).filePath("manipulate/test_polygons.conf"));
    if (!hspPolygons.open(QIODevice::WriteOnly | QIODevice::Text)
        || hspPolygons.write("1\n4 0.0 1.0 1.1 0.1\n") < 0) {
        return 87;
    }
    hspPolygons.close();
    QFile hspEmptyMasks(QDir(hspSource).filePath("manipulate/empty_masks.conf"));
    if (!hspEmptyMasks.open(QIODevice::WriteOnly | QIODevice::Text)
        || hspEmptyMasks.write("0\n") < 0) {
        return 91;
    }
    hspEmptyMasks.close();
    if (!QFile::copy(pgm16Path, QDir(hspSource).filePath("manipulate/empty.pgm")))
        return 92;
    QFile hspEmptyInfo(QDir(hspSource).filePath("info/empty.conf"));
    if (!hspEmptyInfo.open(QIODevice::WriteOnly | QIODevice::Text)
        || hspEmptyInfo.write("group_name=Group A\n") < 0) {
        return 93;
    }
    hspEmptyInfo.close();

    HspImportOptions hspOptions;
    hspOptions.sourceProjectPath = hspSource;
    hspOptions.destinationProjectPath =
        QDir(directory.path()).filePath("imported_hsp");
    hspOptions.projectName = "imported_hsp";
    hspOptions.description = "Imported from test HSP";
    HspImportReport hspReport;
    const Project importedHsp =
        HspProjectImporter::importProject(hspOptions, &hspReport, &error);
    if (!importedHsp.isValid()
        || hspReport.rawFilesCopied != 1
        || hspReport.pgmFilesConverted != 2
        || hspReport.skyPointFilesWritten != 1
        || hspReport.sunMarkFilesWritten != 1
        || hspReport.maskFilesWritten != 1
        || hspReport.markedAreaFilesWritten != 1
        || hspReport.optSkyCoefTablesWritten != 1) {
        return 70;
    }
    RasterData importedTiff = TiffReader::load(
        QDir(importedHsp.rootPath()).filePath("preprocessed_Group_A/test.tif"),
        &error);
    if (importedTiff.isNull()
        || importedTiff.bands() != 1
        || importedTiff.sample(0, 1, 1).toInt() != 1024) {
        return 71;
    }
    QFile importedPoints(
        QDir(importedHsp.rootPath()).filePath("sky_points_Group_A/test.csv"));
    if (!importedPoints.open(QIODevice::ReadOnly | QIODevice::Text)
        || !importedPoints.readAll().contains("20,10,3")) {
        return 72;
    }
    QFile importedSun(
        QDir(importedHsp.rootPath()).filePath("sun_marks_Group_A/test.csv"));
    if (!importedSun.open(QIODevice::ReadOnly | QIODevice::Text)
        || !importedSun.readAll().contains("60,50")) {
        return 73;
    }
    QFile importedNote(
        QDir(importedHsp.rootPath()).filePath("field_notes/comment.md"));
    if (!importedNote.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 74;
    }
    const QByteArray importedNoteText = importedNote.readAll();
    if (!importedNoteText.contains("Thu Apr 30 11:44:41 EEST 2020")
        || !importedNoteText.contains("Field note from HSP")) {
        return 74;
    }
    const CollectionStyleData importedStyle = CollectionStyleFile::load(
        QDir(importedHsp.rootPath()).filePath("preprocessed_Group_A"));
    if (importedStyle.kind != CollectionStyleData::Kind::Image
        || importedStyle.imageStyle.render.stretchMode != StretchMode::None
        || importedStyle.imageStyle.render.tailTrim) {
        return 76;
    }
    QFile importedOptSky(
        QDir(importedHsp.rootPath()).filePath("opt_sky_coef_Group_A/opt_sky_coef.tsv"));
    if (!importedOptSky.open(QIODevice::ReadOnly | QIODevice::Text)
        || !importedOptSky.readAll().contains(
            "test\t3.25\t-0.5\t0.125\t-1\t4.5\tmanipulate/test_opt-parameters.conf\tok\t")) {
        return 77;
    }
    QFile importedOptSkyReadme(
        QDir(importedHsp.rootPath()).filePath("opt_sky_coef_Group_A/README.md"));
    if (!importedOptSkyReadme.open(QIODevice::ReadOnly | QIODevice::Text)
        || !importedOptSkyReadme.readAll().contains("read.delim()")) {
        return 78;
    }
    RasterData importedMask = TiffReader::load(
        QDir(importedHsp.rootPath()).filePath("hsp_masks_Group_A/test.tif"),
        &error);
    if (importedMask.isNull()
        || importedMask.dataType() != RasterData::DataType::UInt8
        || importedMask.sample(0, 0, 0).toInt() != 255
        || importedMask.sample(0, 1, 1).toInt() != 255) {
        return 88;
    }
    RasterData importedMarkedArea = TiffReader::load(
        QDir(importedHsp.rootPath()).filePath("hsp_marked_areas_Group_A/test.tif"),
        &error);
    if (importedMarkedArea.isNull()
        || importedMarkedArea.dataType() != RasterData::DataType::UInt8
        || importedMarkedArea.sample(0, 0, 0).toInt() != 255
        || importedMarkedArea.sample(0, 1, 1).toInt() != 0) {
        return 89;
    }
    const CollectionStyleData importedMaskStyle = CollectionStyleFile::load(
        QDir(importedHsp.rootPath()).filePath("hsp_masks_Group_A"));
    if (importedMaskStyle.kind != CollectionStyleData::Kind::Image
        || importedMaskStyle.imageStyle.opacity != 0.5
        || importedMaskStyle.imageStyle.blendMode != ImageBlendMode::Multiply
        || importedMaskStyle.imageStyle.render.colorRamp != ColorRamp::RedBlue
        || importedMaskStyle.imageStyle.render.tailTrim) {
        return 90;
    }
    if (QFileInfo::exists(
            QDir(importedHsp.rootPath()).filePath("hsp_masks_Group_A/empty.tif"))) {
        return 94;
    }

    const QString emptyCommentHsp = QDir(directory.path()).filePath("empty_comment_hsp");
    QDir().mkpath(QDir(emptyCommentHsp).filePath("raw"));
    QDir().mkpath(QDir(emptyCommentHsp).filePath("manipulate"));
    QFile emptyCommentProject(QDir(emptyCommentHsp).filePath("project.conf"));
    if (!emptyCommentProject.open(QIODevice::WriteOnly | QIODevice::Text)
        || emptyCommentProject.write(
            "#Wed Dec 17 09:02:39 ART 2025\n"
            "name=empty_comment_hsp\n"
            "comments=\n") < 0) {
        return 79;
    }
    emptyCommentProject.close();
    HspImportOptions emptyCommentOptions;
    emptyCommentOptions.sourceProjectPath = emptyCommentHsp;
    emptyCommentOptions.destinationProjectPath =
        QDir(directory.path()).filePath("imported_empty_comment_hsp");
    emptyCommentOptions.projectName = "imported_empty_comment_hsp";
    emptyCommentOptions.description = "Imported empty comment HSP";
    const Project importedEmptyComment =
        HspProjectImporter::importProject(emptyCommentOptions, nullptr, &error);
    if (!importedEmptyComment.isValid())
        return 80;
    QFile emptyCommentNote(
        QDir(importedEmptyComment.rootPath()).filePath("field_notes/comment.md"));
    if (!emptyCommentNote.open(QIODevice::ReadOnly | QIODevice::Text)
        || !emptyCommentNote.readAll().contains("Wed Dec 17 09:02:39 ART 2025")) {
        return 81;
    }

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
    QFile extractionDoc(
        QDir(sidecarSensorDirectory).filePath("README.md"));
    if (!extractionDoc.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 58;
    }
    const QByteArray extractionDocText = extractionDoc.readAll();
    if (!extractionDocText.contains("Sensor data")
        || !extractionDocText.contains("sensor_metadata_sidecar_tsv_v1")
        || !extractionDocText.contains("read.delim")
        || !extractionDocText.contains("LibRaw version")) {
        return 59;
    }
    if (QFileInfo::exists(
            QDir(sidecarSensorDirectory).filePath("metadata_extraction.md"))
        || QFileInfo::exists(
            QDir(sidecarSensorDirectory).filePath("libraw_version.yaml"))) {
        return 60;
    }
    const QString quotedProjectRoot =
        QDir(directory.path()).filePath("quoted_project");
    const QString quotedDescription =
        "Calibration = Nikon \"A\": C:\\raw # checked";
    const Project quotedProject = Project::create(
        quotedProjectRoot, "quoted_project", quotedDescription, &error);
    if (!quotedProject.isValid()) {
        return 61;
    }
    const Project reopenedQuotedProject =
        Project::open(quotedProjectRoot, &error);
    if (!reopenedQuotedProject.isValid()
        || reopenedQuotedProject.name() != "quoted_project"
        || reopenedQuotedProject.description() != quotedDescription) {
        return 62;
    }
    if (Project::descriptionIsSupported("bad\nproject", &error)) {
        return 63;
    }
    const Project invalidDescriptionProject = Project::create(
        QDir(directory.path()).filePath("invalid_description_project"),
        "invalid_description_project", "bad\nproject", &error);
    if (invalidDescriptionProject.isValid()) {
        return 64;
    }
    const QString cleanupProjectRoot =
        QDir(directory.path()).filePath("cleanup_project");
    const Project cleanupProject = Project::create(
        cleanupProjectRoot, "cleanup_project", "Cleanup test", &error);
    if (!cleanupProject.isValid())
        return 95;
    QDir cleanupRoot(cleanupProjectRoot);
    if (!cleanupRoot.mkpath(".Rproj.user/session"))
        return 96;
    QFile rstudioState(cleanupRoot.filePath(".Rproj.user/session/state"));
    if (!rstudioState.open(QIODevice::WriteOnly)
        || rstudioState.write("local state") < 0) {
        return 97;
    }
    rstudioState.close();
    QFile looseRootFile(cleanupRoot.filePath("analysis.Rproj"));
    if (!looseRootFile.open(QIODevice::WriteOnly)
        || looseRootFile.write("Version: 1.0") < 0) {
        return 98;
    }
    looseRootFile.close();
    QFile readmeFile(cleanupRoot.filePath("README.md"));
    if (!readmeFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)
        || readmeFile.write("# Keep me\n") < 0) {
        return 99;
    }
    readmeFile.close();
    const ProjectCleanupPlan cleanupPlan = cleanupProject.buildCleanupPlan();
    if (!cleanupProject.applyCleanupPlan(cleanupPlan, &error))
        return 100;
    if (QFileInfo::exists(cleanupRoot.filePath(".Rproj.user"))
        || QFileInfo::exists(cleanupRoot.filePath("analysis.Rproj"))
        || !QFileInfo::exists(cleanupRoot.filePath("README.md"))) {
        return 101;
    }
    return 0;
}
