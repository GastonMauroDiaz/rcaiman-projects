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
