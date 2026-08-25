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

#ifndef LEGACYJPEGIMPORTER_H
#define LEGACYJPEGIMPORTER_H

#include <QString>
#include <QStringList>

struct LegacyJpegImportOptions {
    QString projectRoot;
    QString sourceCollectionDir;
    QString sourceCollectionName;
    QString destinationCollectionDir;
    QString destinationCollectionName;
    bool replaceExisting = false;
};

struct LegacyJpegImportReport {
    int sourceFiles = 0;
    int convertedFiles = 0;
    int failedFiles = 0;
    int sidecarsWritten = 0;
    int metadataErrors = 0;
    QString exifToolVersion;
    QStringList warnings;
};

namespace LegacyJpegImporter {

QStringList jpegFiles(const QString &sourceDirectory);

QStringList conflictingOutputs(const LegacyJpegImportOptions &options);

bool importProject(const LegacyJpegImportOptions &options,
                   LegacyJpegImportReport *report,
                   QString *errorMessage = nullptr);

}

#endif // LEGACYJPEGIMPORTER_H
