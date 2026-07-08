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

#include "tiffwriter.h"

#include <QFileInfo>

#include <tiffio.h>

bool TiffWriter::write(const QString &fileName,
                       const RasterData &data,
                       QString *errorMessage)
{
    if (data.isNull() || data.width() <= 0 || data.height() <= 0
        || data.bands() != 1) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "Only non-empty single-band rasters can be written as TIFF.");
        return false;
    }

    int bitsPerSample = 0;
    if (data.dataType() == RasterData::DataType::UInt8) {
        bitsPerSample = 8;
    } else if (data.dataType() == RasterData::DataType::UInt16) {
        bitsPerSample = 16;
    } else {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "Only UInt8 and UInt16 rasters can be written as TIFF.");
        return false;
    }

    const QByteArray path = QFileInfo(fileName).absoluteFilePath().toUtf8();
    TIFF *tif = TIFFOpen(path.constData(), "w");
    if (!tif) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not create TIFF file: %1")
                .arg(fileName);
        return false;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,
                 static_cast<uint32_t>(data.width()));
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH,
                 static_cast<uint32_t>(data.height()));
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bitsPerSample);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP,
                 TIFFDefaultStripSize(tif, 0));

    const int bytesPerRow = data.width() * data.sampleSize();
    const char *base = data.rawData().constData();
    for (int row = 0; row < data.height(); ++row) {
        const char *rowData =
            base + static_cast<qsizetype>(row) * bytesPerRow;
        if (TIFFWriteScanline(tif, const_cast<char *>(rowData), row, 0) < 0) {
            TIFFClose(tif);
            if (errorMessage)
                *errorMessage = QStringLiteral("Could not write TIFF row %1.")
                    .arg(row);
            return false;
        }
    }

    TIFFClose(tif);
    return true;
}
