#include "tiffreader.h"

#include <QFileInfo>
#include <QDebug>

#include <tiffio.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

static RasterData::DataType mapDataType(int bitsPerSample, int sampleFormat)
{
    switch (sampleFormat) {
    case SAMPLEFORMAT_UINT:
        if (bitsPerSample == 8)  return RasterData::DataType::UInt8;
        if (bitsPerSample == 16) return RasterData::DataType::UInt16;
        if (bitsPerSample == 32) return RasterData::DataType::UInt32;
        break;
    case SAMPLEFORMAT_INT:
        if (bitsPerSample == 16) return RasterData::DataType::Int16;
        if (bitsPerSample == 32) return RasterData::DataType::Int32;
        break;
    case SAMPLEFORMAT_IEEEFP:
        if (bitsPerSample == 32) return RasterData::DataType::Float32;
        if (bitsPerSample == 64) return RasterData::DataType::Float64;
        break;
    default:
        break;
    }
    return RasterData::DataType::Invalid;
}

RasterData TiffReader::load(const QString &fileName, QString *errorMessage)
{
    RasterData result;

    const QByteArray path = QFileInfo(fileName).absoluteFilePath().toUtf8();

    qDebug() << "TiffReader: opening" << path;

    TIFF *tif = TIFFOpen(path.constData(), "r");
    if (!tif) {
        const QString msg = QStringLiteral("Cannot open TIFF file: %1").arg(fileName);
        qDebug() << "TiffReader:" << msg;
        if (errorMessage) *errorMessage = msg;
        return result;
    }

    // Read tags
    uint32_t w = 0, h = 0;
    uint16_t bitsPerSample = 1, samplesPerPixel = 1, sampleFormat = SAMPLEFORMAT_UINT;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);

    qDebug() << "TiffReader: w=" << w << "h=" << h
             << "bitsPerSample=" << bitsPerSample
             << "samplesPerPixel=" << samplesPerPixel
             << "sampleFormat=" << sampleFormat;

    if (w == 0 || h == 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid TIFF dimensions.");
        TIFFClose(tif);
        return result;
    }

    const RasterData::DataType dt = mapDataType(bitsPerSample, sampleFormat);
    if (dt == RasterData::DataType::Invalid) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unsupported format: bitsPerSample=%1, sampleFormat=%2")
                                .arg(bitsPerSample).arg(sampleFormat);
        TIFFClose(tif);
        return result;
    }

    // Allocate planar RasterData.
    result.allocate(static_cast<int>(w), static_cast<int>(h),
                    static_cast<int>(samplesPerPixel), dt);
    qDebug() << "TiffReader: allocated" << w << "x" << h << "bands=" << samplesPerPixel;

    // Read using TIFFReadScanline (handles all compression automatically).
    const int bytesPerSample = (bitsPerSample + 7) / 8;
    const tmsize_t scanlineSize = TIFFScanlineSize(tif);

    // Allocate a scanline buffer large enough for one row.
    std::vector<uint8_t> scanlineBuf(static_cast<size_t>(scanlineSize));

    // We assume contiguous (interleaved) data, which is the common case.
    // De-interleave row by row into planar bands.
    for (uint32_t row = 0; row < h; ++row) {
        if (TIFFReadScanline(tif, scanlineBuf.data(), row) < 0) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Failed to read scanline %1").arg(row);
            TIFFClose(tif);
            return RasterData();
        }

        // Copy each pixel's samples into their respective planar band buffers.
        for (uint32_t col = 0; col < w; ++col) {
            for (uint16_t b = 0; b < samplesPerPixel; ++b) {
                const uint8_t *src = scanlineBuf.data()
                                     + (col * samplesPerPixel + b) * bytesPerSample;
                uint8_t *dst = reinterpret_cast<uint8_t *>(result.rawData().data())
                               + static_cast<qsizetype>(b) * w * h * bytesPerSample
                               + static_cast<qsizetype>(row) * w * bytesPerSample
                               + static_cast<qsizetype>(col) * bytesPerSample;
                std::memcpy(dst, src, static_cast<size_t>(bytesPerSample));
            }
        }
    }

    // Set band names
    for (int b = 0; b < result.bands(); ++b)
        result.setBandName(b, QStringLiteral("Band %1").arg(b));

    qDebug() << "TiffReader: success, bands=" << result.bands();

    TIFFClose(tif);
    return result;
}
