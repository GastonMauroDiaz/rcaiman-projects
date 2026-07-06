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

#ifndef ACQUISITIONMETADATA_H
#define ACQUISITIONMETADATA_H

#include "metadataregistry.h"

#include <QString>

struct AcquisitionMetadataValues {
    bool hasAperture = false;
    double aperture = 0.0;
    bool hasExposureTime = false;
    double exposureTimeSeconds = 0.0;
    bool hasIso = false;
    double iso = 0.0;
    bool hasExposureCompensation = false;
    double exposureCompensationEv = 0.0;
    bool hasLibRawBlack = false;
    double libRawBlack = 0.0;
    bool hasLibRawCblack = false;
    double libRawCblack[4] = {0.0, 0.0, 0.0, 0.0};
    QString dateTimeOriginal;
    bool hasGpsLatitude = false;
    double gpsLatitude = 0.0;
    bool hasGpsLongitude = false;
    double gpsLongitude = 0.0;
    bool hasGpsAltitude = false;
    double gpsAltitude = 0.0;
};

struct AcquisitionMetadataSummary {
    int sidecarsWritten = 0;
    int metadataErrors = 0;
    int writeErrors = 0;
    int orphanSidecarsRemoved = 0;
};

namespace AcquisitionMetadata {

AcquisitionMetadataValues valuesFromObservations(
    const QList<MetadataObservation> &observations);

bool writeSidecar(const QString &filePath,
                  const QString &projectRoot,
                  const QString &rawPath,
                  const QString &sensorPath,
                  const AcquisitionMetadataValues &values,
                  const QString &status,
                  const QString &error,
                  QString *errorMessage = nullptr);

bool regenerate(const QString &projectRoot,
                AcquisitionMetadataSummary *summary,
                QString *errorMessage = nullptr);

}

#endif
