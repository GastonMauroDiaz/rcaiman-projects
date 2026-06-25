#ifndef RAWREADER_H
#define RAWREADER_H

#include <QString>

class RawReader
{
public:
    // Extract raw sensor data from a RAW file and write it as a single-band
    // UInt16 TIFF (no demosaicing, no white balance, no corrections).
    // Equivalent to dcraw -D -4 -T.
    static bool extractSensorData(const QString &rawFilePath,
                                  const QString &tiffOutputPath,
                                  QString *errorMessage = nullptr);
};

#endif // RAWREADER_H
