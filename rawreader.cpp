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

#include "rawreader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <libraw/libraw.h>
#include <tiffio.h>

#include <vector>

namespace {

bool openRawFile(const QString &rawFilePath,
                 LibRaw *processor,
                 QString *errorMessage)
{
    const QByteArray rawPath =
        QFileInfo(rawFilePath).absoluteFilePath().toUtf8();
    const int ret = processor->open_file(rawPath.constData());
    if (ret != LIBRAW_SUCCESS) {
        if (errorMessage)
            *errorMessage =
                QStringLiteral("Cannot open RAW file: %1\nLibRaw error: %2")
                    .arg(rawFilePath,
                         QString::fromLatin1(libraw_strerror(ret)));
        return false;
    }
    return true;
}

}

bool RawReader::extractSensorData(const QString &rawFilePath,
                                   const QString &tiffOutputPath,
                                   QString *errorMessage)
{
    // Open RAW file via LibRaw
    LibRaw processor;
    if (!openRawFile(rawFilePath, &processor, errorMessage))
        return false;

    // Unpack raw data (no processing, just raw Bayer data)
    int ret = processor.unpack();
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

    processor.recycle();
    return true;
}

bool RawReader::readBlackLevels(const QString &rawFilePath,
                                RawBlackLevels *blackLevels,
                                QString *errorMessage)
{
    if (!blackLevels) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "Black level output is required.");
        return false;
    }

    LibRaw processor;
    if (!openRawFile(rawFilePath, &processor, errorMessage))
        return false;

    RawBlackLevels values;
    const libraw_colordata_t &color = processor.imgdata.color;
    values.hasBlack = true;
    values.black = static_cast<double>(color.black);

    values.hasCblack = true;
    for (int i = 0; i < 4; ++i) {
        values.cblack[static_cast<size_t>(i)] =
            static_cast<double>(color.cblack[i]);
    }

    *blackLevels = values;
    processor.recycle();
    return true;
}
