#include "rawreader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <libraw/libraw.h>
#include <tiffio.h>

#include <vector>

bool RawReader::extractSensorData(const QString &rawFilePath,
                                   const QString &tiffOutputPath,
                                   QString *errorMessage)
{
    const QByteArray rawPath = QFileInfo(rawFilePath).absoluteFilePath().toUtf8();

    // Open RAW file via LibRaw
    LibRaw processor;
    int ret = processor.open_file(rawPath.constData());
    if (ret != LIBRAW_SUCCESS) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot open RAW file: %1\nLibRaw error: %2")
                                .arg(rawFilePath, QString::fromLatin1(libraw_strerror(ret)));
        return false;
    }

    // Unpack raw data (no processing, just raw Bayer data)
    ret = processor.unpack();
    if (ret != LIBRAW_SUCCESS) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot unpack RAW data: %1\nLibRaw error: %2")
                                .arg(rawFilePath, QString::fromLatin1(libraw_strerror(ret)));
        processor.recycle();
        return false;
    }

    // Access raw sensor data
    // libraw.imgdata.rawdata.raw_image is a ushort* of size width * height
    // containing unprocessed Bayer CFA data (equivalent to dcraw -D)
    const libraw_rawdata_t &raw = processor.imgdata.rawdata;
    if (!raw.raw_image) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No raw image data available.");
        processor.recycle();
        return false;
    }

    const unsigned int w = processor.imgdata.sizes.raw_width;
    const unsigned int h = processor.imgdata.sizes.raw_height;

    if (w == 0 || h == 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid raw image dimensions.");
        processor.recycle();
        return false;
    }

    // Write TIFF using libtiff
    const QByteArray tiffPath = QFileInfo(tiffOutputPath).absoluteFilePath().toUtf8();
    QDir().mkpath(QFileInfo(tiffOutputPath).absolutePath());

    TIFF *tif = TIFFOpen(tiffPath.constData(), "w");
    if (!tif) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot create output TIFF: %1").arg(tiffOutputPath);
        processor.recycle();
        return false;
    }

    // Set TIFF fields: 1 band, UInt16, uncompressed, MINISBLACK
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, w);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, h);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, h);  // single strip (small images)

    // Write row by row.
    std::vector<uint16_t> rowBuf(static_cast<size_t>(w));

    for (unsigned int row = 0; row < h; ++row) {
        for (unsigned int col = 0; col < w; ++col) {
            rowBuf[col] = raw.raw_image[static_cast<size_t>(row) * w + col];
        }
        if (TIFFWriteScanline(tif, rowBuf.data(), static_cast<uint32_t>(row)) < 0) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Failed to write scanline %1").arg(row);
            TIFFClose(tif);
            processor.recycle();
            return false;
        }
    }

    TIFFClose(tif);

    // Write LibRaw version info (once per sensor_data folder)
    const QString versionFilePath = QFileInfo(tiffOutputPath).absolutePath() + "/libraw_version.yaml";
    if (!QFile::exists(versionFilePath)) {
        QFile vFile(versionFilePath);
        if (vFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&vFile);
            stream << "libraw_version: " << QString::fromLatin1(libraw_version()) << "\n";
        }
    }

    processor.recycle();
    return true;
}
