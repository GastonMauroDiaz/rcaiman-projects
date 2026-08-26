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

#include "tiffreader.h"

#include <QFileInfo>
#include <QDebug>
#include <QXmlStreamReader>

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

TiffImageInfo TiffReader::imageInfo(const QString &fileName,
                                    QString *errorMessage)
{
    TiffImageInfo result;
    const QByteArray path = QFileInfo(fileName).absoluteFilePath().toUtf8();

    TIFF *tif = TIFFOpen(path.constData(), "r");
    if (!tif) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("Cannot open TIFF file: %1").arg(fileName);
        }
        return result;
    }

    uint32_t w = 0;
    uint32_t h = 0;
    uint16_t samplesPerPixel = 1;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFClose(tif);

    if (w == 0 || h == 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid TIFF dimensions.");
        return {};
    }

    result.width = static_cast<int>(w);
    result.height = static_cast<int>(h);
    result.bands = static_cast<int>(samplesPerPixel);
    return result;
}

static void readGdalBandDescriptions(TIFF *tif, RasterData *data)
{
    if (!tif || !data)
        return;

    QByteArray metadataBytes;
    char *metadata = nullptr;
    const TIFFField *field =
        TIFFFindField(tif, TIFFTAG_GDAL_METADATA, TIFF_ANY);
    if (field && TIFFFieldPassCount(field)) {
        uint32_t metadataSize = 0;
        if (!TIFFGetField(
                tif, TIFFTAG_GDAL_METADATA, &metadataSize, &metadata)
            || !metadata || metadataSize == 0) {
            return;
        }
        if (metadata[metadataSize - 1] == '\0')
            --metadataSize;
        metadataBytes = QByteArray(metadata, static_cast<qsizetype>(metadataSize));
    } else {
        if (!TIFFGetField(tif, TIFFTAG_GDAL_METADATA, &metadata)
            || !metadata) {
            return;
        }
        metadataBytes = QByteArray(metadata);
    }

    if (metadataBytes.isEmpty())
        return;

    QXmlStreamReader xml(QString::fromUtf8(metadataBytes));
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QStringLiteral("Item"))
            continue;

        const QXmlStreamAttributes attributes = xml.attributes();
        if (attributes.value(QStringLiteral("name"))
                != QStringLiteral("DESCRIPTION")) {
            continue;
        }

        bool ok = false;
        const int sample =
            attributes.value(QStringLiteral("sample")).toInt(&ok);
        const QString description =
            xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
        if (ok && sample >= 0 && sample < data->bands()
            && !description.isEmpty()) {
            data->setBandName(sample, description);
        }
    }

    if (xml.hasError()) {
        qDebug() << "TiffReader: could not parse GDAL metadata:"
                 << xml.errorString();
    }
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

    readGdalBandDescriptions(tif, &result);

    qDebug() << "TiffReader: success, bands=" << result.bands();

    TIFFClose(tif);
    return result;
}
