#include "rawreader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <libraw/libraw.h>
#include <tiffio.h>

#include <vector>

static void writeMetadataYaml(const LibRaw &processor,
                               const QString &yamlPath)
{
    QDir().mkpath(QFileInfo(yamlPath).absolutePath());

    QFile file(yamlPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return;

    QTextStream out(&file);

    const libraw_image_sizes_t &sizes  = processor.imgdata.sizes;
    const libraw_iparams_t     &params = processor.imgdata.idata;
    const libraw_colordata_t   &color  = processor.imgdata.color;
    const libraw_imgother_t    &other  = processor.imgdata.other;
    const libraw_lensinfo_t    &lens   = processor.imgdata.lens;

    // Camera / image basics
    out << "camera_make: "  << params.make << "\n";
    out << "camera_model: " << params.model << "\n";
    out << "raw_width: "    << sizes.raw_width << "\n";
    out << "raw_height: "   << sizes.raw_height << "\n";
    out << "colors: "       << params.colors << "\n";
    out << "filters: "      << "0x" << QString::number(params.filters, 16) << "\n";
    out << "raw_count: "    << params.raw_count << "\n";

    // Timestamp (from imgother)
    out << "timestamp: " << other.timestamp << "\n";

    // Shooting info
    out << "iso_speed: "    << other.iso_speed << "\n";
    out << "shutter: "      << other.shutter << "\n";
    out << "aperture: "     << other.aperture << "\n";
    out << "focal_length: " << other.focal_len << "\n";

    // Description (user comment from other.desc)
    QString desc = QString::fromUtf8(other.desc).trimmed();
    if (desc.isEmpty())
        out << "description: \"\"\n";
    else
        out << "description: \"" << desc.replace("\"", "\\\"") << "\"\n";

    // Black level and white maximum (from colordata)
    out << "black: "          << color.black << "\n";
    out << "data_maximum: "   << color.data_maximum << "\n";
    out << "white_maximum: "  << color.maximum << "\n";

    // White balance multipliers (daylight)
    for (int i = 0; i < 4; ++i) {
        if (color.cam_mul[i] > 0.0f)
            out << "daylight_mult_" << i << ": " << color.cam_mul[i] << "\n";
    }

    // Camera multipliers
    for (int i = 0; i < 4; ++i) {
        if (color.pre_mul[i] > 0.0f)
            out << "camera_mult_" << i << ": " << color.pre_mul[i] << "\n";
    }

    // Color matrix (cmatrix: 3x4, output first 3 columns)
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out << "color_matrix_" << (i * 3 + j) << ": "
                << color.cmatrix[i][j] << "\n";
        }
    }

    // Lens info
    if (lens.makernotes.LensID > 0) {
        out << "lens_id: 0x" << QString::number(
            static_cast<quint64>(lens.makernotes.LensID), 16) << "\n";
    }
    if (lens.LensMake[0])
        out << "lens_make: \"" << lens.LensMake << "\"\n";
    if (lens.Lens[0])
        out << "lens_model: \"" << lens.Lens << "\"\n";
    if (lens.MinFocal > 0.0f)
        out << "lens_focal_min: " << lens.MinFocal << "\n";
    if (lens.MaxFocal > 0.0f)
        out << "lens_focal_max: " << lens.MaxFocal << "\n";
    if (lens.MaxAp4MinFocal > 0.0f)
        out << "lens_aperture_min_focal: " << lens.MaxAp4MinFocal << "\n";
    if (lens.MaxAp4MaxFocal > 0.0f)
        out << "lens_aperture_max_focal: " << lens.MaxAp4MaxFocal << "\n";
    if (lens.EXIF_MaxAp > 0.0f)
        out << "lens_aperture_exif: " << lens.EXIF_MaxAp << "\n";

    // LibRaw version
    out << "libraw_version: " << QString::fromLatin1(libraw_version()) << "\n";

    out.flush();
}

bool RawReader::extractSensorData(const QString &rawFilePath,
                                   const QString &tiffOutputPath,
                                   const QString &projectRootPath,
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

    // Write metadata YAML
    const QFileInfo fi(rawFilePath);
    const QString baseName = fi.completeBaseName();
    const QString yamlPath = QDir(QDir(projectRootPath).filePath("image_metadata"))
                                 .filePath(baseName + ".yaml");
    writeMetadataYaml(processor, yamlPath);

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